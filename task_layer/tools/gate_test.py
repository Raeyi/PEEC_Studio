# -*- coding: utf-8 -*-
"""P1.5 用户闸门（WaitingInput + task.provideInput）验收测试。

用法：
  python gate_test.py --exe <task_layer.exe> --ref <std100 DATA 参考目录> --work <工作目录> --cfg-src <task_layer/config 目录>

测试用流水线 gate_demo（单阶段 userGate，独立配置目录，不入内置三流水线）：
  extract: peec.model_extract, userGate={
    waitForFiles:["Model/gate_probe.json"], confirm:true,
    inputs:[{name:"export", type:"string", required:true}]}

验收（计划 §6 P1.5 ①-⑥）：
  ① 被 gate 阶段暂停并发 awaiting（缺文件/未确认/缺值三种缺法各有断言）
  ② provideInput 非法值被白名单拒绝
  ③ 等待中 cancel → 干净收尾
  ④ 等待中 UI 断连 → cancel
  ⑤ 回归：等待不触发 QS-TL-CFG-DEADLOCK（§4.2.4 陷阱）
  ⑥ values 确实拼进该阶段 CLI（job.args 可见）
  ⑦ 全链基线不变 → 另跑 rpc_smoke.py / baseline_e2e.py
"""
import argparse, json, os, shutil, subprocess, sys, time
import queue, threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rpc_smoke import Rpc

GATE_DEMO = {
    "displayName": "Gate 演示（P1.5 验收专用）",
    "description": "单阶段 userGate 验收流水线（不入交付内置流水线）",
    "maxParallel": 1,
    "stages": [{
        "id": "extract", "module": "peec.model_extract",
        "name": "PEEC 模型提取（闸门）", "dependsOn": [],
        "userGate": {
            "waitForFiles": ["Model/gate_probe.json"],
            "confirm": True,
            "inputs": [{"name": "export", "type": "string",
                        "required": True, "description": "导出模式"}],
        },
    }],
}
EXPECTED_GATE = {
    "waitForFiles": ["Model/gate_probe.json"],
    "confirm": True,
    "inputs": [{"name": "export", "type": "string",
                "required": True, "description": "导出模式"}],
}


class GateRpc(Rpc):
    """serve --config <dir> 的 RPC 客户端（单读取线程 + 双向暂存）。

    通知可能先于 RPC 响应到达（awaiting 在 provideInput 响应之前发出），
    各等待函数取到非目标行时存入对侧队列，不得丢弃。"""
    def __init__(self, exe, cfg):
        self.proc = subprocess.Popen(
            [exe, "serve", "--config", cfg],
            cwd=os.path.dirname(exe),
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, bufsize=1, text=True,
            encoding="utf-8", errors="replace")
        self.next_id = 0
        self._q = queue.Queue()
        self._resp_q = queue.Queue()    # 暂存响应
        self._notif_q = queue.Queue()   # 暂存通知
        threading.Thread(target=self._reader, daemon=True).start()

    def _reader(self):
        for line in self.proc.stdout:
            self._q.put(line)
        self._q.put(None)

    def _next(self, timeout=10):
        try:
            return self._q.get(timeout=timeout)
        except queue.Empty:
            return None

    def send(self, method, params=None):
        self.next_id += 1
        msg = {"jsonrpc": "2.0", "id": self.next_id, "method": method,
               "params": params or {}}
        self.proc.stdin.write(json.dumps(msg, ensure_ascii=False) + "\n")
        self.proc.stdin.flush()
        return self.next_id

    def _route(self, msg, want_method=None):
        """非目标行入对侧暂存队列。返回 (hit, value)。"""
        if want_method is not None and msg.get("method") == want_method:
            return True, msg.get("params")
        if msg.get("id") is not None:
            self._resp_q.put(msg)
        elif "method" in msg:
            self._notif_q.put(msg)
        return False, None

    def rpc_call(self, method, params=None, timeout=60):
        req_id = self.send(method, params)
        deadline = time.time() + timeout
        while time.time() < deadline:
            line = self._next(timeout=min(10.0, max(0.5, deadline - time.time())))
            if line is None or line.strip() == "":
                continue
            msg = json.loads(line)
            if msg.get("id") == req_id:
                if "error" in msg:
                    raise RuntimeError("RPC error: %s" % msg["error"])
                return msg.get("result")
            self._route(msg)
        raise TimeoutError(method + " 响应超时")

    def expect_error(self, method, params=None, timeout=60):
        req_id = self.send(method, params)
        deadline = time.time() + timeout
        while time.time() < deadline:
            line = self._next(timeout=min(10.0, max(0.5, deadline - time.time())))
            if line is None or line.strip() == "":
                continue
            msg = json.loads(line)
            if msg.get("id") == req_id:
                if "error" in msg:
                    return str(msg["error"])
                raise RuntimeError("预期错误但成功: %s" % line[:200])
            self._route(msg)
        raise TimeoutError(method + " 响应超时")

    def wait_awaiting(self, timeout=60):
        while not self._notif_q.empty():
            msg = self._notif_q.get()
            if msg.get("method") == "task.stage.awaiting":
                return msg.get("params")
        deadline = time.time() + timeout
        while time.time() < deadline:
            line = self._next(timeout=min(10.0, max(0.5, deadline - time.time())))
            if line is None or line.strip() == "":
                continue
            hit, val = self._route(json.loads(line), "task.stage.awaiting")
            if hit:
                return val
        raise TimeoutError("task.stage.awaiting 超时")

    def wait_finished(self, timeout=600):
        while not self._notif_q.empty():
            msg = self._notif_q.get()
            if msg.get("method") == "task.finished":
                return msg.get("params")
        deadline = time.time() + timeout
        while time.time() < deadline:
            line = self._next(timeout=min(10.0, max(0.5, deadline - time.time())))
            if line is None or line.strip() == "":
                continue
            hit, val = self._route(json.loads(line), "task.finished")
            if hit:
                return val
        raise TimeoutError("task.finished 超时")

    def run_to_awaiting(self, root, pipeline, timeout=120):
        """task.run -> 等到响应 + task.started + 首条 task.stage.awaiting。"""
        req_id = self.send("task.run", {"pipeline": pipeline, "dataRoot": root})
        deadline = time.time() + timeout
        tid, started, awaiting = None, None, None
        while time.time() < deadline and (tid is None or awaiting is None):
            line = self._next(timeout=10)
            if line is None or line.strip() == "":
                continue
            msg = json.loads(line)
            if msg.get("id") == req_id:
                if "error" in msg:
                    raise RuntimeError("task.run 被拒: %s" % msg["error"])
                tid = msg["result"]["taskId"]
            elif msg.get("method") == "task.started":
                started = msg["params"]
            elif msg.get("method") == "task.stage.awaiting":
                awaiting = msg["params"]
            else:
                self._route(msg)
        if tid is None:
            raise TimeoutError("task.run 响应超时")
        if awaiting is None:
            raise TimeoutError("task.stage.awaiting 超时（阶段未进入等待？）")
        return tid, started, awaiting


def read_state(root, tid):
    with open(os.path.join(root, "Result", "tasks", tid, "state.json"),
              encoding="utf-8") as f:
        return json.load(f)


def stage_of(state, sid):
    for s in state.get("stages", []):
        if s.get("id") == sid:
            return s
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--ref", required=True, help="std100 DATA 参考目录")
    ap.add_argument("--work", required=True)
    ap.add_argument("--cfg-src", required=True, help="task_layer/config 目录")
    args = ap.parse_args()

    exe = os.path.abspath(args.exe)
    work = os.path.abspath(args.work)
    cfg = os.path.join(work, "config")
    root = os.path.join(work, "gate")
    for d in (cfg, root):
        if os.path.exists(d):
            shutil.rmtree(d)
        os.makedirs(d)
    shutil.copy2(os.path.join(args.cfg_src, "modules.json"), os.path.join(cfg, "modules.json"))
    pj = json.load(open(os.path.join(args.cfg_src, "pipelines.json"), encoding="utf-8"))
    pj["pipelines"]["gate_demo"] = GATE_DEMO
    json.dump(pj, open(os.path.join(cfg, "pipelines.json"), "w", encoding="utf-8"),
              ensure_ascii=False, indent=2)
    if os.path.exists(root):
        shutil.rmtree(root)
    shutil.copytree(args.ref, root)

    results = []

    def check(name, cond, detail=""):
        results.append((name, bool(cond), detail))
        print("%s %s %s" % ("PASS" if cond else "FAIL", name, detail))

    rpc = GateRpc(exe, cfg)
    try:
        # ── T1 闸门全生命周期（① 三种缺法 / ⑤ 无死锁 / ⑥ CLI 注入 / 审计留痕）──
        print("[T1] gate_demo 全生命周期")
        tid, started, aw1 = rpc.run_to_awaiting(root, "gate_demo")
        check("T1 awaiting#1 gate 原样", aw1.get("gate") == EXPECTED_GATE,
              json.dumps(aw1.get("gate"), ensure_ascii=False)[:120])
        miss1 = aw1.get("missing", {})
        check("T1 ① 三缺齐（文件/确认/值）",
              miss1.get("files") == ["Model/gate_probe.json"]
              and miss1.get("confirmed") is True
              and miss1.get("inputs") == ["export"],
              json.dumps(miss1, ensure_ascii=False))
        st = read_state(root, tid)
        check("T1 state.json 阶段 waiting",
              stage_of(st, "extract").get("status") == "waiting")
        # ⑤ 防悬挂兜底回归：等待 8s（远超 pump 循环节拍）不得被判死锁/收尾
        time.sleep(8)
        st = read_state(root, tid)
        check("T1 ⑤ 等待 8s 未触发死锁",
              st.get("status") == "running"
              and stage_of(st, "extract").get("status") == "waiting"
              and "CFG-DEADLOCK" not in json.dumps(st),
              st.get("status"))
        # 先给值（文件尚缺）：增量提供 + 类型校验。
        # 注意：对 confirm 闸门的任意 provideInput 调用即显式确认（F3 语义），
        # 故中间态缺法断言为"仅缺文件"（confirmed:false = 确认已满足）。
        err = rpc.expect_error("task.provideInput",
                               {"taskId": tid, "stageId": "extract",
                                "values": {"export": 123}})
        check("T1 类型校验拒绝", "type" in err, err)
        rpc.rpc_call("task.provideInput",
                     {"taskId": tid, "stageId": "extract",
                      "values": {"export": "post,report"}})
        aw2 = rpc.wait_awaiting()
        miss2 = aw2.get("missing", {})
        check("T1 ① 值+确认满足后仅缺文件",
              miss2.get("files") == ["Model/gate_probe.json"]
              and miss2.get("confirmed") is False
              and miss2.get("inputs") == [],
              json.dumps(miss2, ensure_ascii=False))
        # 文件出现 + 空 values 唤醒事件（§4.2.3(3)：平台只在收到调用时复检文件）
        with open(os.path.join(root, "Model", "gate_probe.json"), "w", encoding="utf-8") as f:
            json.dump({"probe": True}, f)
        rpc.rpc_call("task.provideInput",
                     {"taskId": tid, "stageId": "extract", "values": {}})
        fin = rpc.wait_finished(timeout=600)
        check("T1 闸门解除后 done", fin["status"] == "done", fin["summary"])
        # exe 模块的 CLI 留痕 = task.log 的 [launch] 行（DllWorker 模块另有 job.json）
        with open(os.path.join(root, "Result", "tasks", tid, "task.log"),
                  encoding="utf-8", errors="replace") as f:
            log = f.read()
        launch = [l for l in log.splitlines()
                  if "[launch]" in l and "Model_Extractor" in l]
        check("T1 ⑥ gate 值拼进 CLI（launch 行）",
              bool(launch) and launch[-1].rstrip().endswith("--export post,report"),
              (launch[-1][-110:] if launch else "无 [launch] 行"))
        check("T1 ⑥ 无重复 --key（注入覆盖模板默认）",
              bool(launch) and launch[-1].count("--export") == 1,
              ("--export 次数: %s" % launch[-1].count("--export")) if launch else "")
        st = read_state(root, tid)
        pi = st.get("providedInputs", {})
        pe = pi.get("extract", {})
        check("T1 审计 providedInputs 留痕",
              pe.get("values", {}).get("export") == "post,report"
              and pe.get("confirmed") is True and pe.get("ts", 0) > 0,
              json.dumps(pe, ensure_ascii=False)[:160])

        # ── T2 白名单拒绝（②）──
        print("[T2] 白名单拒绝")
        tid2, _, _ = rpc.run_to_awaiting(root, "gate_demo")
        err = rpc.expect_error("task.provideInput",
                               {"taskId": tid2, "stageId": "extract",
                                "values": {"bogus": 1}})
        check("T2 ② 非法键白名单拒绝", "not in allowedArgs" in err, err)
        st = read_state(root, tid2)
        check("T2 拒绝后仍在等待",
              stage_of(st, "extract").get("status") == "waiting")
        rpc.rpc_call("task.cancel", {"taskId": tid2})
        fin = rpc.wait_finished()
        check("T2 cancel 干净收尾", fin["status"] == "cancelled", fin["summary"])

        # ── T3 等待中 cancel（③）──
        print("[T3] 等待中 cancel")
        tid3, _, _ = rpc.run_to_awaiting(root, "gate_demo")
        rpc.rpc_call("task.cancel", {"taskId": tid3})
        fin = rpc.wait_finished()
        check("T3 ③ 等待中 cancel -> cancelled", fin["status"] == "cancelled",
              fin["summary"])
        st = read_state(root, tid3)
        se = stage_of(st, "extract")
        check("T3 等待阶段 Skipped/QS-TL-CANCEL",
              se.get("status") == "skipped" and se.get("errorCode") == "QS-TL-CANCEL")

        # ── T4 运行中 provideInput → NOTWAITING ──
        print("[T4] NOTWAITING")
        tid4, _, aw4a = rpc.run_to_awaiting(root, "gate_demo")
        # 文件已在盘（T1 遗留）：awaiting#1 缺法 = 确认 + 值
        m4a = aw4a.get("missing", {})
        check("T4 awaiting#1 缺确认+值",
              m4a.get("files") == [] and m4a.get("confirmed") is True
              and m4a.get("inputs") == ["export"],
              json.dumps(m4a, ensure_ascii=False))
        # 先确认（空 values 唤醒事件），再给值 → 值给齐后阶段才启动
        rpc.rpc_call("task.provideInput",
                     {"taskId": tid4, "stageId": "extract", "values": {}})
        aw4b = rpc.wait_awaiting()
        m4b = aw4b.get("missing", {})
        check("T4 awaiting#2 仅缺值",
              m4b.get("files") == [] and m4b.get("confirmed") is False
              and m4b.get("inputs") == ["export"],
              json.dumps(m4b, ensure_ascii=False))
        rpc.rpc_call("task.provideInput",
                     {"taskId": tid4, "stageId": "extract",
                      "values": {"export": "post,report"}})
        time.sleep(1)        # 阶段进入 running（extract 实际耗时 1min+，余量充足）
        st4 = read_state(root, tid4)
        se4 = stage_of(st4, "extract")
        check("T4 阶段确已 running", se4.get("status") == "running",
              se4.get("status"))
        err = rpc.expect_error("task.provideInput",
                               {"taskId": tid4, "stageId": "extract", "values": {}})
        check("T4 运行中 provideInput 拒绝", "QS-TL-INPUT-NOTWAITING" in err, err)
        fin = rpc.wait_finished(timeout=600)
        check("T4 阶段照常完成", fin["status"] == "done", fin["summary"])

        # ── T5 UI 断连（④）──
        print("[T5] 等待中 UI 断连")
        tid5, _, _ = rpc.run_to_awaiting(root, "gate_demo")
        check("T5 断连前任务 running",
              read_state(root, tid5).get("status") == "running")
        rpc.proc.stdin.close()
        try:
            rc = rpc.proc.wait(timeout=30)
        except subprocess.TimeoutExpired:
            rpc.proc.kill()
            rc = -1
        check("T5 ④ 断连 -> 进程退出 0", rc == 0, rc)
        st = read_state(root, tid5)
        se = stage_of(st, "extract")
        check("T5 断连后状态 cancelled/skipped",
              st.get("status") == "cancelled" and se.get("status") == "skipped"
              and se.get("errorCode") == "QS-TL-CANCEL",
              st.get("status"))
    finally:
        if rpc.proc.poll() is None:
            rpc.proc.kill()

    print()
    fails = [n for n, ok, _ in results if not ok]
    print("RESULT: %s" % ("PASS" if not fails else "FAIL %s" % fails))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
