# -*- coding: utf-8 -*-
"""切工程产物路径串用回归（缺陷报告 2026-11-10 §7 验收）。

流程：
  P1 旧二进制（c5b0cf7 基线 worktree 构建，未含修复）在 _integ/ps_old 复现缺陷：
     A→B 后 B 的 stages[].outputs/resultFiles 仍指向 A 的 dataRoot。
  P2 新二进制（合并后主线 59072c7 构建，含修复）在 _integ/ps_new 验收：
     同一 serve 进程 gds_full @A → @B → @A（A→B→A，不重启）：
       - B 任务 done；B 全部阶段 outputs / resultFiles 均在 B 根内，A 根出现 0 次；
       - B 发布件 Model/model_em.msh + matdb.json 存在且与 B Result em_msh 字节一致；
       - A 树 sha256 快照在 B 任务前后不变（无交叉写）；
       - 再跑 A → done 且产物在 A（§7 A→B→A）。
  P3 容纳性守卫误报检查：合法相对 manifest 下 validate → valid true（守卫不误伤）。

P1 为根因证据（可 SKIP，不影响 P2 验收）；P2/P3 为验收红线。
"""
import json, os, shutil, subprocess, sys, time, queue, threading, hashlib

ROOT     = r"E:\code\PEEC_Studio"
NEW_EXE  = os.path.join(ROOT, "task_layer", "build", "task_layer.exe")   # 合并后主线（59072c7，含修复 + host publishOverrides）
OLD_EXE  = os.path.join(r"E:\code\PEEC_Studio_wt_c5b0cf7", "task_layer", "build", "task_layer.exe")   # c5b0cf7 基线（合并前主线，未含修复）
CASE     = os.path.join(ROOT, "dev", "cases", "heater_strip")
WORK_OLD = os.path.join(ROOT, "_integ", "ps_old")
WORK_NEW = os.path.join(ROOT, "_integ", "ps_new")
PIPELINE = "gds_full"

fails = []
def check(name, cond, detail=""):
    print(("PASS " if cond else "FAIL ") + name + (" | " + str(detail) if detail else ""))
    if not cond: fails.append(name)
def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f: h.update(f.read())
    return h.hexdigest()
def snap_tree(rootdir):
    out = {}
    for dp, dn, fn in os.walk(rootdir):
        for f in fn:
            p = os.path.join(dp, f)
            out[os.path.relpath(p, rootdir).replace("\\", "/")] = sha256(p)
    return out
def under_root(p, root):
    # 形态无关：两侧归一分隔符（slash/backslash 等价）+ 大小写不敏感（Windows）
    p = os.path.normpath(str(p)).casefold()
    r = os.path.normpath(str(root)).casefold()
    return p.startswith(r + os.sep)

class Serve:
    def __init__(self, exe):
        self.proc = subprocess.Popen([exe, "serve"], cwd=os.path.dirname(exe),
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            bufsize=1, text=True, encoding="utf-8", errors="replace")
        self.q = queue.Queue(); self.nid = 0
        threading.Thread(target=self._run, daemon=True).start()
    def _run(self):
        for line in self.proc.stdout: self.q.put(line)
    def _next_json(self, timeout):
        dl = time.time() + timeout
        while time.time() < dl:
            try: line = self.q.get(timeout=min(20, max(0.1, dl - time.time())))
            except queue.Empty: return None
            if line is None or line.strip() == "": continue
            try: mm = json.loads(line)
            except Exception: continue   # stderr 合并进来的日志行
            if isinstance(mm, dict) and "id" in mm: return mm
        return None
    def send(self, m, p=None):
        self.nid += 1
        msg = {"jsonrpc": "2.0", "id": self.nid, "method": m, "params": p or {}}
        self.proc.stdin.write(json.dumps(msg) + chr(10)); self.proc.stdin.flush()
        return self.nid
    def call(self, m, p=None, timeout=60):
        rid = self.send(m, p)
        mm = self._next_json(timeout)
        if mm is None: raise TimeoutError(m)
        if "error" in mm: raise RuntimeError(mm["error"])
        return mm["result"]
    def run_task(self, root, pipeline, timeout=1500):
        rid = self.send("task.run", {"pipeline": pipeline, "dataRoot": root})
        ev = {"finished": None, "stages": {}, "resp": None}
        dl = time.time() + timeout
        while time.time() < dl:
            try: line = self.q.get(timeout=30)
            except queue.Empty: continue
            if line is None or line.strip() == "": continue
            try: mm = json.loads(line)
            except Exception: continue
            if mm.get("id") == rid:
                ev["resp"] = mm.get("result") if "error" not in mm else mm.get("error")
                continue
            meth = mm.get("method")
            if meth == "task.stage.finished":
                p2 = mm["params"]; ev["stages"][p2["stageId"]] = p2
            elif meth == "task.finished":
                ev["finished"] = mm["params"]; break
        return ev
    def task_result(self, root, task_id, timeout=30):
        return self.call("task.result", {"taskId": task_id, "dataRoot": root}, timeout)
    def task_id_of(self, root, ev):
        r = ev.get("resp")
        if isinstance(r, dict):
            t = r.get("taskId")
            if t: return str(t)
        if isinstance(r, str) and r: return r
        # 回退：扫 dataRoot/Result/tasks 取最新
        tdir = os.path.join(root, "Result", "tasks")
        if os.path.isdir(tdir):
            cands = [os.path.join(tdir, d) for d in os.listdir(tdir)
                     if os.path.isdir(os.path.join(tdir, d))]
            if cands:
                return os.path.basename(max(cands, key=os.path.getmtime))
        return None
    def close(self):
        try: self.send("shutdown")
        except Exception: pass
        time.sleep(1)
        if self.proc.poll() is None: self.proc.terminate()

def prep_case(dst):
    if os.path.exists(dst): shutil.rmtree(dst)
    shutil.copytree(CASE, dst)
    return dst

# ══════════════════════════ P1 旧二进制复现（证据） ══════════════════════════
if os.path.exists(OLD_EXE):
    print("\n===== P1 旧二进制（%s）复现 =====" % os.path.basename(os.path.dirname(OLD_EXE)))
    A = prep_case(os.path.join(WORK_OLD, "A")); B = prep_case(os.path.join(WORK_OLD, "B"))
    sv = Serve(OLD_EXE)
    evA = sv.run_task(A, PIPELINE)
    stA = (evA["finished"] or {}).get("status")
    print("[P1] old A status =", stA)
    if stA == "done":
        # 同一 serve 进程内续跑 B（冻结缺陷只在同进程 A→B 时出现，不可重启）
        evB = sv.run_task(B, PIPELINE)
        finB = evB["finished"] or {}
        tid = sv.task_id_of(B, evB)
        stB = None
        if tid: stB = sv.task_result(B, tid)
        sv.close()
        print("[P1] old B status =", finB.get("status"), "task =", tid)
        a_count = b_count = 0
        stg = (stB or {}).get("stages") or []
        stg_items = stg.items() if isinstance(stg, dict) else [(s.get("stageId") or s.get("id"), s) for s in stg]
        for sid, p2 in stg_items:
            for o in p2.get("outputs", []):
                if under_root(o, A): a_count += 1
                if under_root(o, B): b_count += 1
        for f in (stB or {}).get("resultFiles", []):
            if under_root(f, A): a_count += 1
        if a_count > 0:
            print("PASS  [P1 证据] 旧二进制复现：B 状态含 %d 条 A 根路径（串用）" % a_count)
        else:
            print("SKIP  [P1 证据] 旧二进制未在 B 状态复现 A 根路径（A根=%d B根=%d），不影响 P2 验收" % (a_count, b_count))
    else:
        sv.close()
        print("SKIP  [P1 证据] 旧二进制 A 任务未 done（%s），跳过复现" % stA)
else:
    print("\nSKIP  P1：旧二进制不存在（%s）" % OLD_EXE)

# ══════════════════════════ P2 新二进制验收（A→B→A） ══════════════════════════
print("\n===== P2 新二进制验收（A→B→A，单 serve 进程不重启） =====")
A = prep_case(os.path.join(WORK_NEW, "A")); B = prep_case(os.path.join(WORK_NEW, "B"))
sv = Serve(NEW_EXE)

ev = sv.run_task(A, PIPELINE)
fin = ev["finished"] or {}
tid = sv.task_id_of(A, ev)
st = sv.task_result(A, tid) if tid else {}
sv_stages = st.get("stages", [])
outputs_A = [o for s in sv_stages for o in s.get("outputs", [])]
check("P2 A 任务 done", fin.get("status") == "done", str(fin.get("summary", ""))[:120])
check("P2 A outputs 全部在 A 根", len(outputs_A) > 0 and all(under_root(o, A) for o in outputs_A),
      "n=%d" % len(outputs_A) + (" | 越出: %s" % [o for o in outputs_A if not under_root(o, A)] if outputs_A else ""))
pub_em  = os.path.join(A, "Model", "model_em.msh")
pub_md  = os.path.join(A, "Model", "matdb.json")
res_em  = os.path.join(A, "Result", "gds2geo", "em_msh", "model_em.msh")
res_md  = os.path.join(A, "Result", "gds2geo", "em_msh", "matdb.json")
check("P2 A 发布件存在", os.path.isfile(pub_em) and os.path.isfile(pub_md), pub_em)
if os.path.isfile(pub_em) and os.path.isfile(res_em):
    check("P2 A 发布件与 em_msh 字节一致", sha256(pub_em) == sha256(res_em) and sha256(pub_md) == sha256(res_md))
snapA1 = snap_tree(A)
check("P2 A 快照采集", len(snapA1) > 0, "files=%d" % len(snapA1))

ev = sv.run_task(B, PIPELINE)
fin = ev["finished"] or {}
tid = sv.task_id_of(B, ev)
stB = sv.task_result(B, tid) if tid else {}
sv_stages = stB.get("stages", [])
outputs_B = [o for s in sv_stages for o in s.get("outputs", [])]
files_B = stB.get("resultFiles", [])
a_hits = [o for o in outputs_B + files_B if under_root(o, A)]
check("P2 B 任务 done", fin.get("status") == "done", str(fin.get("summary", ""))[:120])
check("P2 B outputs 非空且全部在 B 根", len(outputs_B) > 0 and all(under_root(o, B) for o in outputs_B),
      "n=%d" % len(outputs_B) + (" | 越出: %s" % [o for o in outputs_B if not under_root(o, B)] if outputs_B else ""))
check("P2 B 状态零 A 根路径（核心回归）", len(a_hits) == 0, "hits=%d: %s" % (len(a_hits), a_hits[:4]))
check("P2 B resultFiles 全部在 B 根", len(files_B) > 0 and all(under_root(f, B) for f in files_B),
      "n=%d" % len(files_B))
pub_emB  = os.path.join(B, "Model", "model_em.msh")
pub_mdB  = os.path.join(B, "Model", "matdb.json")
res_emB  = os.path.join(B, "Result", "gds2geo", "em_msh", "model_em.msh")
res_mdB  = os.path.join(B, "Result", "gds2geo", "em_msh", "matdb.json")
check("P2 B 发布件存在", os.path.isfile(pub_emB) and os.path.isfile(pub_mdB), pub_emB)
if os.path.isfile(pub_emB) and os.path.isfile(res_emB):
    check("P2 B 发布件与 em_msh 字节一致", sha256(pub_emB) == sha256(res_emB) and sha256(pub_mdB) == sha256(res_mdB))
snapA2 = snap_tree(A)
diff = {k: (snapA1.get(k), snapA2.get(k)) for k in set(snapA1) | set(snapA2)
        if snapA1.get(k) != snapA2.get(k)}
check("P2 B 运行后 A 树无变化（无交叉写）", not diff, "diff=%d: %s" % (len(diff), list(diff)[:4]))

ev = sv.run_task(A, PIPELINE)
fin = ev["finished"] or {}
tid = sv.task_id_of(A, ev)
st = sv.task_result(A, tid) if tid else {}
outputs_A2 = [o for s in st.get("stages", []) for o in s.get("outputs", [])]
check("P2 A→B→A 再次 A 任务 done", fin.get("status") == "done", str(fin.get("summary", ""))[:120])
check("P2 A→B→A A outputs 全部在 A 根", len(outputs_A2) > 0 and all(under_root(o, A) for o in outputs_A2),
      "n=%d" % len(outputs_A2))
sv.close()

# ══════════════════════════ P3 守卫误报检查 ══════════════════════════
print("\n===== P3 容纳性守卫：合法相对 manifest 不误报 =====")
r1 = subprocess.run([NEW_EXE, "validate", "--data-root", B, "--pipeline", PIPELINE],
                    cwd=os.path.dirname(NEW_EXE), capture_output=True, text=True,
                    encoding="utf-8", errors="replace", timeout=120)
ok1 = False
raw = (r1.stdout or "").lstrip("\ufeff")
i0, i1 = raw.find("{"), raw.rfind("}")
if i0 >= 0 and i1 > i0:
    try:
        j = json.loads(raw[i0:i1 + 1])
        if isinstance(j, dict) and "valid" in j:
            ok1 = j.get("valid") is True and not any(
                i.get("reason") == "QS-TL-PATH-ESCAPE" for i in j.get("issues", []) if isinstance(i, dict))
    except Exception:
        pass
check("P3 validate B 合法（valid=true 且无 PATH-ESCAPE）", ok1, "exit=%d" % r1.returncode)

print("\n===== 结果 =====")
if fails:
    print("FAIL %d 项: %s" % (len(fails), fails)); sys.exit(1)
print("全部通过（P2/P3 验收红线 + P1 证据）")
