# -*- coding: utf-8 -*-
"""P3 部署级覆盖 pipelines.local.json + module.run 路由修复（H2）验收测试。

用法：
  python p3_local_test.py --exe <task_layer.exe> --cfg-src <config 目录> --cases <dev/cases> --work <工作目录>

验收（计划 §6 P3 ①-④；⑤ 基线另跑回归套件）：
  ① local 文件加载 + 同名 pipeline 覆盖（local 赢）+ source 标记 + local 阶段库条目
  ② local 文件非法 -> 整文件拒载（serve ErrConfig，退出码 2，错误带 pipelines.local.json 前缀）
  ③ module.run thermal.extract 单独跑：user/thermal.mesh 按库级 artifacts 路由
     （[launch] --mesh 指向 Result/gds2geo/thermal/ 而非 manifest 默认 Model/ 位置，H2 回归）
  ④ 同模块多接线 -> 报错列候选；stageRef "pipelineId/stageId" 消歧后放行
"""
import argparse, json, os, shutil, subprocess, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


def make_cfg(work, name, local_json=None):
    d = os.path.join(work, "cfg_" + name)
    if os.path.exists(d):
        shutil.rmtree(d)
    os.makedirs(d)
    return d


class Srv:
    """serve 客户端：stdout 队列化，stderr 单独收集（启动日志留痕断言）。"""
    def __init__(self, exe, cfg, log=None):
        import queue, threading
        self.proc = subprocess.Popen(
            [exe, "serve", "--config", cfg],
            cwd=os.path.dirname(exe),
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, bufsize=1, text=True,
            encoding="utf-8", errors="replace")
        self.q = queue.Queue()
        self.nid = 0
        self.stderr_lines = []
        t = threading.Thread(target=self._run, daemon=True)
        if log is not None:
            t.start()
            e = threading.Thread(target=self._err, daemon=True); e.start()
        else:
            t.start()

    def _run(self):
        for line in self.proc.stdout:
            self.q.put(line)

    def _err(self):
        for line in self.proc.stderr:
            self.stderr_lines.append(line.rstrip())

    def send(self, m, p=None):
        self.nid += 1
        self.proc.stdin.write(json.dumps({"jsonrpc": "2.0", "id": self.nid,
                                          "method": m, "params": p or {}}) + chr(10))
        self.proc.stdin.flush()
        return self.nid

    def call(self, m, p=None, timeout=60):
        import queue
        rid = self.send(m, p)
        dl = time.time() + timeout
        while time.time() < dl:
            try:
                line = self.q.get(timeout=min(20, dl - time.time()))
            except queue.Empty:
                continue
            if not line.strip():
                continue
            mm = json.loads(line)
            if mm.get("id") == rid:
                if "error" in mm:
                    raise RuntimeError(json.dumps(mm["error"], ensure_ascii=False))
                return mm["result"]
        raise TimeoutError(m)

    def run_task(self, p, timeout=900, method="task.run"):
        """发请求后收通知到 task.finished；返回 (响应, 通知列表)。"""
        import queue
        rid = self.send(method, p)
        resp = None
        notifs = []
        dl = time.time() + timeout
        while time.time() < dl:
            try:
                line = self.q.get(timeout=min(30, dl - time.time()))
            except queue.Empty:
                continue
            if not line.strip():
                continue
            mm = json.loads(line)
            if mm.get("id") == rid:
                resp = mm
                continue
            notifs.append(mm)
            if mm.get("method") == "task.finished":
                # 同步启动校验失败时 task.finished 通知可能先于响应到达：
                # 继续排空队列直到本请求的响应（或短超时）
                dl2 = time.time() + 15
                while time.time() < dl2 and resp is None:
                    try:
                        line2 = self.q.get(timeout=min(5, dl2 - time.time()))
                    except queue.Empty:
                        continue
                    if not line2.strip():
                        continue
                    mm2 = json.loads(line2)
                    if mm2.get("id") == rid:
                        resp = mm2
                return resp, notifs
        return resp, notifs

    def close(self):
        try:
            self.proc.stdin.close()
        except Exception:
            pass
        try:
            self.proc.wait(timeout=15)
        except Exception:
            self.proc.kill()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--cfg-src", required=True)
    ap.add_argument("--cases", required=True)
    ap.add_argument("--work", required=True)
    args = ap.parse_args()

    exe = os.path.abspath(args.exe)
    work = os.path.abspath(args.work)
    if os.path.exists(work):
        shutil.rmtree(work)
    os.makedirs(work)
    results = []

    def check(name, cond, detail=""):
        results.append((name, bool(cond), detail))
        print("%s %s %s" % ("PASS" if cond else "FAIL", name,
                            str(detail)[:220]))

    main_cfg_json = open(os.path.join(args.cfg_src, "pipelines.json"),
                         encoding="utf-8").read()

    def seed_cfg(name, local_json=None):
        d = make_cfg(work, name)
        shutil.copy2(os.path.join(args.cfg_src, "modules.json"),
                     os.path.join(d, "modules.json"))
        open(os.path.join(d, "pipelines.json"), "w", encoding="utf-8").write(main_cfg_json)
        if local_json is not None:
            open(os.path.join(d, "pipelines.local.json"), "w",
                 encoding="utf-8").write(local_json)
        return d

    # ── ① local 覆盖 + source 标记 + local 阶段库 ──
    local1 = json.dumps({
        "schemaVersion": 2,
        "stages": {
            "local.gated_post": {
                "module": "peec.post", "name": "local 闸门后处理",
                "userGate": {"confirm": True},
                "requires": ["Result/compact_em/extract/model.peec"],
            },
        },
        "pipelines": {
            "peec_basic": {
                "displayName": "local 覆盖版 peec", "maxParallel": 1,
                "stages": [
                    {"id": "extract", "ref": "em.model_extract", "dependsOn": []},
                ],
            },
            "local_chain": {
                "displayName": "local 自定义链", "maxParallel": 1,
                "stages": [
                    {"id": "s1", "ref": "em.solver", "dependsOn": []},
                    {"id": "s2", "ref": "local.gated_post", "dependsOn": ["s1"]},
                ],
            },
        },
    }, ensure_ascii=False)
    c1 = seed_cfg("local", local1)
    srv = Srv(exe, c1, log=True)
    try:
        pl = srv.call("pipelines.list")["pipelines"]
        byid = {p["id"]: p for p in pl}
        check("① 内置未覆盖 pipeline 标 builtin",
              byid["gds_full"]["source"] == "builtin"
              and byid["thermal_basic"]["source"] == "builtin",
              [p.get("source") for p in pl])
        check("① 同名 pipeline local 赢（覆盖内容 + source=local）",
              byid["peec_basic"]["source"] == "local"
              and byid["peec_basic"]["displayName"] == "local 覆盖版 peec"
              and len(byid["peec_basic"]["stages"]) == 1,
              json.dumps(byid["peec_basic"], ensure_ascii=False)[:150])
        lc = byid["local_chain"]
        s1, s2 = lc["stages"][0], lc["stages"][1]
        check("① local pipeline 物化：主库 ref 接线生效（requires 库默认）",
              lc["source"] == "local"
              and s1["module"] == "peec.solver"
              and s1["requires"] == ["Result/compact_em/extract/model.peec"],
              json.dumps(s1, ensure_ascii=False)[:150])
        check("① local 阶段库条目可用（userGate 物化）",
              s2["module"] == "peec.post"
              and s2["userGate"] == {"confirm": True},
              json.dumps(s2, ensure_ascii=False)[:150])
        w = "\n".join(srv.stderr_lines)
        check("① 覆盖留痕（启动日志 stderr 含 overrides builtin）",
              "peec_basic" in w and "overrides builtin" in w, w[:200])
        srv.close()
    finally:
        if srv.proc.poll() is None:
            srv.proc.kill()

    # ── ② local 非法 -> 整文件拒载 ──
    bad1 = local1.replace('"ref": "local.gated_post"', '"ref": "nope.bogus"')
    c2 = seed_cfg("badref", bad1)
    p = subprocess.Popen([exe, "serve", "--config", c2], cwd=os.path.dirname(exe),
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, bufsize=1, text=True,
                         encoding="utf-8", errors="replace")
    line = p.stdout.readline().strip()
    rc = p.wait(timeout=15) if p.poll() is None else p.poll()
    try:
        rc = p.wait(timeout=15)
    except Exception:
        p.kill()
        rc = -1
    check("② local 未知 ref 整文件拒载（ErrConfig + 前缀）",
          "unknown ref" in line and "pipelines.local.json" in line and rc == 2,
          line[:200] + " rc=" + str(rc))

    bad2 = '{"schemaVersion": 3, "pipelines": {}}'
    c3 = seed_cfg("badschema", bad2)
    p = subprocess.Popen([exe, "serve", "--config", c3], cwd=os.path.dirname(exe),
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, bufsize=1, text=True,
                         encoding="utf-8", errors="replace")
    line = p.stdout.readline().strip()
    try:
        rc = p.wait(timeout=15)
    except Exception:
        p.kill()
        rc = -1
    check("② local schemaVersion>=3 拒载",
          "unsupported schemaVersion" in line and rc == 2, line[:200])

    # ── ③ module.run thermal.extract H2 路由 ──
    # 造 gds_full 型根：热网格只在 Result/gds2geo/thermal/（manifest 默认 Model/ 位不存在）
    th = os.path.join(work, "th")
    if os.path.exists(th):
        shutil.rmtree(th)
    shutil.copytree(os.path.join(args.cases, "thermal", "demo1_4.1"), th)
    os.makedirs(os.path.join(th, "Result", "gds2geo", "thermal"), exist_ok=True)
    shutil.copy2(os.path.join(th, "Model", "model_thermal.msh"),
                 os.path.join(th, "Result", "gds2geo", "thermal", "model_thermal.msh"))
    os.remove(os.path.join(th, "Model", "model_thermal.msh"))   # 默认位撤除
    sj = json.load(open(os.path.join(th, "Model", "source_thermal.json"), encoding="utf-8"))
    sj["meshFile"] = "Result/gds2geo/thermal/model_thermal.msh"
    json.dump(sj, open(os.path.join(th, "Model", "source_thermal.json"), "w",
                       encoding="utf-8"), ensure_ascii=False, indent=2)
    # 清掉算例自带结果（避免旧件干扰"新生成"断言）
    for d in ["post", "extract"]:
        dp = os.path.join(th, "Result", "thermal", d)
        if os.path.exists(dp):
            shutil.rmtree(dp)

    c0 = seed_cfg("base")   # 纯内置配置
    srv = Srv(exe, c0)
    try:
        resp, notifs = srv.run_task({"moduleId": "thermal.extract", "dataRoot": th},
                             method="module.run")
        fin = [n for n in notifs if n.get("method") == "task.finished"]
        ok_done = resp and "error" not in resp and fin             and fin[-1]["params"].get("status") == "done"
        check("③ module.run thermal.extract 任务完成", bool(ok_done),
              json.dumps(resp, ensure_ascii=False)[:150] if resp else "no resp")
        taskdir = None
        if resp and "error" not in resp:
            tid = resp["result"]["taskId"]
            taskdir = os.path.join(th, "Result", "tasks", tid)
        logpath = os.path.join(taskdir, "task.log") if taskdir else ""
        launch = ""
        if os.path.exists(logpath):
            for ln in open(logpath, encoding="utf-8", errors="replace"):
                if "[launch]" in ln:
                    launch = ln
                    break
        import re as _re
        mm = _re.search(r"--mesh\s+(\S+)", launch)
        mesharg = mm.group(1) if mm else ""
        tail = mesharg.replace("/", os.sep).lstrip(os.sep)
        check("③ [launch] --mesh 指向库级路由位（非 manifest 默认 Model/）",
              tail.endswith(os.path.join("Result", "gds2geo", "thermal", "model_thermal.msh"))
              and not tail.startswith("Model"),
              launch.strip()[:250])
        sp = os.path.join(th, "Result", "thermal", "extract", "ThermalNetList.sp")
        check("③ 单跑产出 ThermalNetList.sp（新件）", os.path.exists(sp), sp)
        srv.close()
    finally:
        if srv.proc.poll() is None:
            srv.proc.kill()

    # ── ④ 多候选 -> 报错列候选 + stageRef 消歧 ──
    local4 = json.dumps({
        "schemaVersion": 2,
        "pipelines": {
            "dup_chain": {
                "displayName": "dup", "maxParallel": 1,
                "stages": [
                    {"id": "x", "ref": "em.model_extract", "dependsOn": []},
                ],
            },
        },
    }, ensure_ascii=False)
    c4 = seed_cfg("dup", local4)
    empty = os.path.join(work, "emptyroot")
    os.makedirs(empty, exist_ok=True)
    srv = Srv(exe, c4)
    try:
        try:
            srv.call("module.run", {"moduleId": "peec.model_extract",
                                    "dataRoot": empty})
            check("④ 多候选报错", False, "no error")
        except RuntimeError as e:
            msg = str(e)
            check("④ 多候选报错列候选",
                  "peec_basic/extract" in msg and "dup_chain/x" in msg
                  and "stageRef" in msg, msg[:250])
        resp4, notifs4 = srv.run_task({"moduleId": "peec.model_extract",
                                      "dataRoot": empty,
                                      "stageRef": "dup_chain/x"},
                                      method="module.run")
        fin4 = [n for n in notifs4 if n.get("method") == "task.finished"]
        stage_err = ""
        for n in notifs4:
            if n.get("method") == "task.stage.finished":
                stage_err = n["params"].get("errorCode", "")
        check("④ stageRef 消歧放行（任务创建，启动期输入校验失败收尾 failed，非候选错误）",
              resp4 is not None and "error" not in resp4
              and fin4 and fin4[-1]["params"].get("status") == "failed"
              and "INPUT" in stage_err,
              "resp=" + json.dumps(resp4, ensure_ascii=False)[:100]
              + " fin=" + (json.dumps(fin4[-1]["params"], ensure_ascii=False)[:100] if fin4 else "-")
              + " stageErr=" + stage_err)
        srv.close()
    finally:
        if srv.proc.poll() is None:
            srv.proc.kill()

    print()
    fails = [n for n, ok, _ in results if not ok]
    print("RESULT: %s" % ("PASS" if not fails else "FAIL %s" % fails))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
