#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""切换工程后产物路径串用 — 验收脚本（v2.1 布局）

背景：同一 serve 进程先跑工程 A 再跑工程 B 时，旧实现（原地展开契约路径 +
isAbsolute() 判"已展开"）使 B 的 outputChecks/publishes 冻结在 A 的 dataRoot，
B 的 stages[].outputs / resultFiles 指向 A 的 Result/，B 的 Model/ 发布件缺失。

验收（对应缺陷报告 §7 红线；v2.1 布局：source/prepared/em/thermal，pipeline=gds_full）：
  P1  旧版基线二进制（task_layer\build_prefix\task_layer.exe）同进程 A→B 复现冻结
      （无该二进制则 SKIP）
  P2  修复版（task_layer\build\task_layer.exe）同一 serve 进程 A→B→A：
      - A done，outputs 全部在 A 根，发布件（Model/model_em.msh、Model/matdb.json）
        存在且与 Result/gds2geo/em/ 对应件 SHA-256 相等
      - B done，outputs 全部在 B 根且 A 根 0 命中，resultFiles 全部在 B 根，
        发布件与 Result 副本字节一致
      - B 跑完后 A 树快照不变（无跨工程写）
      - 删除 A 的旧产物（Model 发布件 + Result/gds2geo/em/*）后重跑 A：
        done 且产物重新生成（不静默复用旧件）
  P3  inputs.validate 对 A/B 均 valid:true 且无 QS-TL-PATH-ESCAPE 误报
      （守卫只拦越界路径，合法 manifest 不触发）

用法：python project_switch_test.py   （全部通过退出码 0，任一失败退出码 1）
"""
import hashlib
import json
import os
import queue
import shutil
import subprocess
import sys
import threading
import time

ROOT = r"E:\code\PEEC_Studio"
NEW_EXE = os.path.join(ROOT, "task_layer", "build", "task_layer.exe")
OLD_EXE = os.path.join(ROOT, "task_layer", "build_prefix", "task_layer.exe")
CASE = os.path.join(ROOT, "dev", "cases", "heater_strip")
PIPELINE = "gds_full"
WORK = os.path.join(ROOT, "switch_scratch")
A_ROOT = os.path.join(WORK, "测试工程 A（电磁）", "DATA")
B_ROOT = os.path.join(WORK, "测试工程 B（热）", "DATA")
P1A = os.path.join(WORK, "旧版复现 A", "DATA")
P1B = os.path.join(WORK, "旧版复现 B", "DATA")
GMSH = os.path.join(ROOT, "tools", "gmsh", "gmsh.exe")
TASK_TIMEOUT = 900

EM_MESH = "Result/gds2geo/em/model_em.msh"
EM_MATDB = "Result/gds2geo/em/matdb.json"
PUB_MESH = "Model/model_em.msh"
PUB_MATDB = "Model/matdb.json"

FAILS = []


def check(name, cond, detail=""):
    tag = "PASS" if cond else "FAIL"
    if not cond:
        FAILS.append(name)
    line = f"[{tag}] {name}"
    if detail:
        line += f" :: {detail}"
    print(line)
    sys.stdout.flush()


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def under_root(p, root):
    """p 是否严格落在 root 之内（大小写不敏感）。"""
    if not p:
        return False
    a = os.path.normpath(str(p)).casefold()
    b = os.path.normpath(root).casefold() + os.sep
    return a.startswith(b)


def stage_outputs_of(result):
    """stages 可能是 list 或 map，统一取出各阶段 outputs 的并集。"""
    out = []
    stages = (result or {}).get("stages") or {}
    items = stages if isinstance(stages, list) else (stages.values() if isinstance(stages, dict) else [])
    for s in items:
        if isinstance(s, dict):
            o = s.get("outputs")
            if isinstance(o, list):
                out.extend(o)
    return out


def result_files_of(result):
    rf = (result or {}).get("resultFiles")
    if isinstance(rf, list) and rf:
        return rf
    return stage_outputs_of(result)


def is_done(ev):
    st = (ev.get("result") or {}).get("status")
    if st is None:
        st = (ev.get("finished") or {}).get("status")
    return st == "done"


class Serve:
    """serve 模式 RPC 客户端（stdout JSON-RPC，逐行）。"""

    def __init__(self, exe):
        env = dict(os.environ)
        env["GDS2GEO_GMSH_EXECUTABLE"] = GMSH
        self.proc = subprocess.Popen(
            [exe, "serve"],
            cwd=os.path.dirname(exe),
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, encoding="utf-8", errors="replace", env=env,
        )
        self.q = queue.Queue()
        self._id = 0
        self._lock = threading.Lock()
        self._errbuf = []
        threading.Thread(target=self._read, daemon=True).start()
        threading.Thread(target=self._readerr, daemon=True).start()

    def _read(self):
        for line in self.proc.stdout:
            line = line.strip()
            if not line:
                continue
            try:
                self.q.put(("msg", json.loads(line)))
            except Exception:
                self.q.put(("raw", line))

    def _readerr(self):
        for line in self.proc.stderr:
            self._errbuf.append(line)
            if len(self._errbuf) > 5000:
                self._errbuf.pop(0)

    def call(self, method, params=None, timeout=120):
        with self._lock:
            self._id += 1
            rid = self._id
        req = {"jsonrpc": "2.0", "id": rid, "method": method}
        if params is not None:
            req["params"] = params
        self.proc.stdin.write(json.dumps(req, ensure_ascii=False) + "\n")
        self.proc.stdin.flush()
        t0 = time.time()
        while time.time() - t0 < timeout:
            try:
                kind, payload = self.q.get(timeout=1)
            except queue.Empty:
                if self.proc.poll() is not None:
                    raise RuntimeError(f"serve 进程提前退出; stderr 尾部: {self._errbuf[-20:]}")
                continue
            if kind != "msg":
                continue
            if payload.get("id") == rid:
                return payload
        raise TimeoutError(f"{method} (id={rid}) 未在 {timeout}s 内响应")

    def task_id_of(self, root, wait_sec=30):
        tdir = os.path.join(root, "Result", "tasks")
        t0 = time.time()
        while time.time() - t0 < wait_sec:
            if os.path.isdir(tdir):
                cands = []
                for d in os.listdir(tdir):
                    full = os.path.join(tdir, d)
                    if os.path.isdir(full):
                        cands.append((os.path.getmtime(full), d))
                if cands:
                    cands.sort()
                    return cands[-1][1]
            time.sleep(0.5)
        return None

    def run_task(self, root, pipeline, timeout=TASK_TIMEOUT):
        resp = self.call("task.run", {"pipeline": pipeline, "dataRoot": root}, timeout=180)
        if "error" in resp:
            return {"resp": resp, "result": None, "finished": None, "taskId": None}
        respResult = resp.get("result") or {}
        tid = respResult.get("taskId")
        # 若响应已是终态（同步模式），直接采用其状态
        terminal = respResult.get("status") in ("done", "failed", "cancelled")
        finished = None
        if not terminal:
            t0 = time.time()
            while time.time() - t0 < timeout:
                try:
                    kind, payload = self.q.get(timeout=1)
                except queue.Empty:
                    if self.proc.poll() is not None:
                        raise RuntimeError(f"serve 进程提前退出; stderr 尾部: {self._errbuf[-20:]}")
                    continue
                if kind != "msg":
                    continue
                if payload.get("method") == "task.finished":
                    p = payload.get("params") or {}
                    if tid and p.get("taskId") and p.get("taskId") != tid:
                        continue
                    finished = p
                    break
            if not tid:
                tid = (finished or {}).get("taskId") or self.task_id_of(root)
        else:
            tid = tid or self.task_id_of(root, wait_sec=5)
        # 取完整状态（stages/resultFiles）
        result = respResult if (respResult.get("stages") or respResult.get("resultFiles")) else {}
        if not result:
            res = self.call("task.result", {"taskId": tid, "dataRoot": root}, timeout=120)
            if "result" in res:
                result = res.get("result") or {}
        return {"resp": resp, "result": result, "finished": finished, "taskId": tid}

    def close(self):
        try:
            self.call("shutdown", timeout=10)
        except Exception:
            pass
        try:
            self.proc.wait(timeout=10)
        except Exception:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=5)
            except Exception:
                self.proc.kill()


def prep_case(dst):
    if os.path.isdir(dst):
        shutil.rmtree(dst, ignore_errors=True)
    shutil.copytree(CASE, dst)
    ok = all(os.path.isfile(os.path.join(dst, f)) for f in ("Model/model.gds", "Model/model_info.csv"))
    check(f"算例就位 {dst}", ok)
    if not ok:
        print("RESULT: FAIL（算例不完整，终止）")
        sys.exit(1)


def snap_tree(root):
    snap = {}
    for dp, dn, fn in os.walk(root):
        for f in fn:
            full = os.path.join(dp, f)
            rel = os.path.relpath(full, root)
            try:
                snap[rel] = sha256(full)
            except Exception as e:
                snap[rel] = f"ERR:{e}"
    return snap


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    print(f"NEW_EXE: {NEW_EXE} exists={os.path.isfile(NEW_EXE)}")
    if not os.path.isfile(NEW_EXE):
        print("RESULT: FAIL（修复版二进制不存在，请先构建 task_layer\build）")
        return 1

    # ---------------- P1：旧版基线复现（可选） ----------------
    p1 = "SKIP"
    if os.path.isfile(OLD_EXE):
        print("== P1 旧版基线二进制复现 ==")
        prep_case(P1A)
        prep_case(P1B)
        srv = Serve(OLD_EXE)
        try:
            evA = srv.run_task(P1A, PIPELINE)
            check("P1 A done", is_done(evA), str((evA["resp"].get("error"))))
            evB = srv.run_task(P1B, PIPELINE)
            check("P1 B done（旧版误报 done）", is_done(evB), str((evB["resp"].get("error"))))
            outs = stage_outputs_of(evB["result"] or {})
            hitsA = [o for o in outs if under_root(o, P1A)]
            if outs and hitsA:
                p1 = "REPRO"
                check("P1 B 的 outputs 指向 A 根（冻结复现）", True,
                      f"{len(hitsA)}/{len(outs)} 条命中 P1A，例: {hitsA[0]}")
            else:
                p1 = "NO-REPRO"
                check("P1 冻结复现", True, "旧版未复现（outputs 已在 B 根）— 记 SKIP，不影响修复结论")
        finally:
            srv.close()
    else:
        print("== P1 SKIP（build_prefix 不存在） ==")

    # ---------------- P2：修复版 A→B→A（同一 serve 进程） ----------------
    print("== P2 修复版 A→B→A（同一 serve 进程） ==")
    prep_case(A_ROOT)
    prep_case(B_ROOT)
    srv = Serve(NEW_EXE)
    try:
        # --- A ---
        evA = srv.run_task(A_ROOT, PIPELINE)
        resA = evA["result"] or {}
        check("P2-A done", is_done(evA), str((evA["resp"].get("error"))))
        outsA = stage_outputs_of(resA)
        check("P2-A outputs 非空", len(outsA) > 0, f"共 {len(outsA)} 条")
        check("P2-A outputs 全部在 A 根", bool(outsA) and all(under_root(o, A_ROOT) for o in outsA),
              str(outsA[:3]))
        for label, rel, rrel in (("mesh", PUB_MESH, EM_MESH), ("matdb", PUB_MATDB, EM_MATDB)):
            ap, rp = os.path.join(A_ROOT, rel), os.path.join(A_ROOT, rrel)
            ok = os.path.isfile(ap) and os.path.isfile(rp) and sha256(ap) == sha256(rp)
            check(f"P2-A 发布件 {rel} 存在且与 Result 副本一致", ok)
        snapA = snap_tree(A_ROOT)

        # --- B（关键：同进程切工程） ---
        evB = srv.run_task(B_ROOT, PIPELINE)
        resB = evB["result"] or {}
        check("P2-B done", is_done(evB), str((evB["resp"].get("error"))))
        outsB = stage_outputs_of(resB)
        check("P2-B outputs 非空", len(outsB) > 0, f"共 {len(outsB)} 条")
        hitsA = [o for o in outsB if under_root(o, A_ROOT)]
        check("P2-B outputs 全部在 B 根且 A 根 0 命中",
              bool(outsB) and all(under_root(o, B_ROOT) for o in outsB) and not hitsA,
              f"共 {len(outsB)} 条, A 命中 {len(hitsA)}")
        rfs = result_files_of(resB)
        check("P2-B resultFiles 全部在 B 根", len(rfs) > 0 and all(under_root(r, B_ROOT) for r in rfs),
              f"共 {len(rfs)} 条")
        for label, rel, rrel in (("mesh", PUB_MESH, EM_MESH), ("matdb", PUB_MATDB, EM_MATDB)):
            bp, br = os.path.join(B_ROOT, rel), os.path.join(B_ROOT, rrel)
            ok = os.path.isfile(bp) and os.path.isfile(br) and sha256(bp) == sha256(br)
            check(f"P2-B 发布件 {rel} 字节一致（发布不丢）", ok)
        snapA2 = snap_tree(A_ROOT)
        diff = [k for k in set(snapA) | set(snapA2) if snapA.get(k) != snapA2.get(k)]
        check("P2-B 跑完 A 树无变化（无跨工程写）", not diff, str(diff[:3]))

        # --- A2：删旧产物重跑（不得静默复用） ---
        for rel in (PUB_MESH, PUB_MATDB):
            p = os.path.join(A_ROOT, rel)
            if os.path.isfile(p):
                os.remove(p)
        emdir = os.path.join(A_ROOT, "Result", "gds2geo", "em")
        if os.path.isdir(emdir):
            shutil.rmtree(emdir, ignore_errors=True)
        check("P2-A2 旧产物已删除",
              not os.path.isfile(os.path.join(A_ROOT, PUB_MESH)) and not os.path.isfile(os.path.join(A_ROOT, EM_MESH)))
        evA2 = srv.run_task(A_ROOT, PIPELINE)
        resA2 = evA2["result"] or {}
        check("P2-A2 重跑 done（不复用旧件）", is_done(evA2), str((evA2["resp"].get("error"))))
        for label, rel, rrel in (("mesh", PUB_MESH, EM_MESH), ("matdb", PUB_MATDB, EM_MATDB)):
            ok = os.path.isfile(os.path.join(A_ROOT, rel)) and os.path.isfile(os.path.join(A_ROOT, rrel))
            check(f"P2-A2 {rel} 重新生成", ok)
        outsA2 = stage_outputs_of(resA2)
        check("P2-A2 outputs 全部在 A 根", bool(outsA2) and all(under_root(o, A_ROOT) for o in outsA2))

        # ---------------- P3：守卫误报检查 ----------------
        print("== P3 守卫误报检查（inputs.validate） ==")
        for tag, root in (("A", A_ROOT), ("B", B_ROOT)):
            v = srv.call("inputs.validate", {"dataRoot": root, "pipeline": PIPELINE})
            resv = v.get("result") or {}
            issues = resv.get("issues") or []
            esc = [i for i in issues if "PATH-ESCAPE" in str(i.get("reason", ""))]
            check(f"P3 validate {tag} valid:true", resv.get("valid") is True, str(issues[:2]))
            check(f"P3 validate {tag} 无 PATH-ESCAPE 误报", not esc, str(esc))
    finally:
        srv.close()

    print()
    if FAILS:
        print(f"RESULT: FAIL（{len(FAILS)} 项失败, P1={p1}）")
        for f in FAILS:
            print("  -", f)
        return 1
    print(f"RESULT: ALL PASS (P1={p1})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
