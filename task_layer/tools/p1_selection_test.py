# -*- coding: utf-8 -*-
"""P1 执行选择（selection: full/stages/resume）验收测试。

用法：
  python p1_selection_test.py --exe <task_layer.exe> --cases <dev/cases> --work <工作目录> [--ref <参考 DATA 目录>]

覆盖文档 §6.1 验收 ①-⑨ 的 P1 部分（resume 指纹/上游变化为 P3，本脚本不测）。
"""
import argparse, json, os, shutil, subprocess, sys, time
import queue, threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rpc_smoke import Rpc   # 复用 serve 模式 JSON-RPC 客户端


def copy_case(cases, sub, dst):
    src = os.path.join(cases, sub)
    if os.path.exists(dst):
        shutil.rmtree(dst)
    shutil.copytree(src, dst)
    return dst


def stage_map(state):
    return {s["id"]: s for s in state.get("stages", [])}


def read_state(root, tid):
    with open(os.path.join(root, "Result", "tasks", tid, "state.json"),
              encoding="utf-8") as f:
        return json.load(f)


def _collect(rpc, msg, events):
    m = msg.get("method")
    if m == "task.started":
        events["task.started"] = msg.get("params")
    elif m == "task.stage.finished":
        p = msg.get("params")
        events["stages"][p["stageId"]] = p
    elif m == "task.finished":
        events["finished"] = msg.get("params")
        return True
    return False


def run_task(rpc, root, pipeline, selection=None, timeout=600):
    """task.run：task.started 通知先于响应到达，需先读响应再收集（响应等待中丢弃的通知不可恢复，
    故 task.run 不走 rpc.call，改为手工读取；task.started 在响应之前发出，可能已被消费——
    因此改为：发送请求后逐行读取，响应与通知统一收集）。"""
    params = {"pipeline": pipeline, "dataRoot": root}
    if selection is not None:
        params["selection"] = selection
    req_id = rpc.send("task.run", params)
    events = {"task.started": None, "finished": None, "stages": {}}
    deadline = time.time() + 60
    result = None
    while time.time() < deadline and result is None:
        line = rpc.proc.stdout.readline()
        if not line:
            raise RuntimeError("task_layer 提前退出（响应前）")
        line = line.strip()
        if not line:
            continue
        msg = json.loads(line)
        if msg.get("id") == req_id:
            if "error" in msg:
                raise RuntimeError("RPC error: %s" % msg["error"])
            result = msg.get("result")
        else:
            _collect(rpc, msg, events)
    if result is None:
        raise TimeoutError("task.run 响应超时")
    tid = result["taskId"]
    deadline = time.time() + timeout
    while time.time() < deadline:
        line = rpc.proc.stdout.readline()
        if not line:
            raise RuntimeError("task_layer 提前退出")
        line = line.strip()
        if not line:
            continue
        if _collect(rpc, json.loads(line), events):
            break
    if not events["finished"]:
        raise TimeoutError("task.finished 超时: " + tid)
    return tid, events


_readers = {}


def make_reader(proc):
    """stdout 读取线程（每进程单例）：行入队列，主线程带超时取行。
    task.finished 可能先于 task.run 响应到达（全假定任务同步收尾）；
    且同一 proc 只能有一个消费者线程，否则旧 run_task 的线程会吞掉新任务的行。"""
    key = id(proc)
    if key not in _readers:
        q = queue.Queue()

        def _run():
            for line in proc.stdout:
                q.put(line)
            q.put(None)   # EOF 哨兵

        threading.Thread(target=_run, daemon=True).start()
        _readers[key] = q
    q = _readers[key]

    def readline(timeout=60):
        try:
            return q.get(timeout=timeout)
        except queue.Empty:
            return None

    return readline


def rpc_call(rpc, method, params=None, timeout=60):
    """队列式 RPC 调用（与读取线程兼容）。"""
    req_id = rpc.send(method, params)
    readline = make_reader(rpc.proc)
    deadline = time.time() + timeout
    while time.time() < deadline:
        line = readline(timeout=min(10.0, max(0.5, deadline - time.time())))
        if line is None:
            continue
        if line.rstrip() == "":
            continue
        msg = json.loads(line)
        if msg.get("id") == req_id:
            if "error" in msg:
                raise RuntimeError("RPC error: %s" % msg["error"])
            return msg.get("result")
    raise TimeoutError(method + " 响应超时")


def expect_error(rpc, method, params=None, timeout=60):
    """队列式 RPC 调用，预期返回错误（返回错误描述；若意外成功则 raise）。"""
    req_id = rpc.send(method, params)
    readline = make_reader(rpc.proc)
    deadline = time.time() + timeout
    while time.time() < deadline:
        line = readline(timeout=min(10.0, max(0.5, deadline - time.time())))
        if line is None:
            continue
        if line.rstrip() == "":
            continue
        msg = json.loads(line)
        if msg.get("id") == req_id:
            if "error" in msg:
                return "RPC error: %s" % msg["error"]
            raise RuntimeError("预期错误但收到成功响应: %s" % line[:200])
    raise TimeoutError(method + " 响应超时")


def _collect(msg, events):
    m = msg.get("method")
    if m == "task.started":
        events["task.started"] = msg.get("params")
    elif m == "task.stage.finished":
        p = msg.get("params")
        events["stages"][p["stageId"]] = p
    elif m == "task.finished":
        events["finished"] = msg.get("params")
    elif m == "task.stage.started" or m == "task.stage.log":
        pass
    return events["finished"] is not None


def run_task(rpc, root, pipeline, selection=None, timeout=600):
    """task.run：task.started/assumed stage.finished/（全假定时）task.finished
    都可能先于 task.run 响应到达 → 单循环统一收集，行级超时读取。"""
    params = {"pipeline": pipeline, "dataRoot": root}
    if selection is not None:
        params["selection"] = selection
    req_id = rpc.send("task.run", params)
    readline = make_reader(rpc.proc)
    events = {"task.started": None, "finished": None, "stages": {}}
    result = None
    deadline = time.time() + timeout
    # 注意：全假定任务同步收尾 → task.finished 先于 task.run 响应到达；
    # 必须等"响应 + 收尾"两者齐备才退出（只收响应/只收尾都可能丢一半）。
    while time.time() < deadline:
        line = readline(timeout=max(1.0, min(30.0, deadline - time.time())))
        if line is None:
            continue      # 空读窗口：继续等，不是终止条件
        if line.rstrip() == "":
            continue
        msg = json.loads(line)
        if msg.get("id") == req_id:
            if "error" in msg:
                raise RuntimeError("RPC error: %s" % msg["error"])
            result = msg.get("result")
        else:
            _collect(msg, events)
        if result is not None and events["finished"] is not None:
            break
    if result is None:
        raise TimeoutError("task.run 响应超时")
    if not events["finished"]:
        deadline = time.time() + timeout
        while time.time() < deadline and not events["finished"]:
            line = readline(timeout=max(1.0, min(30.0, deadline - time.time())))
            if line is None or line.rstrip() == "":
                continue
            _collect(json.loads(line), events)
    if not events["finished"]:
        raise TimeoutError("task.finished 超时: " + result["taskId"])
    return result["taskId"], events


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--cases", required=True)
    ap.add_argument("--work", required=True)
    ap.add_argument("--ref", default=None, help="参考 DATA 目录（基线比对）")
    args = ap.parse_args()

    exe = os.path.abspath(args.exe)
    work = os.path.abspath(args.work)
    std = os.path.join(work, "std")
    fresh = os.path.join(work, "fresh")
    copy_case(args.cases, os.path.join("compact_em", "standard"), std)
    copy_case(args.cases, os.path.join("compact_em", "standard"), fresh)

    rpc = Rpc(exe, cwd=os.path.dirname(exe))
    results = []

    def check(name, cond, detail=""):
        results.append((name, bool(cond), detail))
        print("%s %s %s" % ("PASS" if cond else "FAIL", name, detail))

    try:
        # ── T1 全链（缺省，无 selection）──
        tid1, ev1 = run_task(rpc, std, "peec_basic")
        st1 = read_state(std, tid1)
        sm1 = stage_map(st1)
        check("T1 全链 done", ev1["finished"]["status"] == "done",
              "summary=" + ev1["finished"]["summary"])
        check("T1 state.json 无 selection 字段", "selection" not in st1)
        check("T1 无 assumed 阶段",
              all(not s.get("assumed") for s in sm1.values()))
        check("T1 started 通知无 selection",
              ev1["task.started"] is not None and "selection" not in ev1["task.started"])

        # ── T2 子集 [post]（上游已在盘）──
        tid2, ev2 = run_task(rpc, std, "peec_basic",
                             {"mode": "stages", "stages": ["post"]})
        st2 = read_state(std, tid2)
        sm2 = stage_map(st2)
        check("T2 子集 [post] done", ev2["finished"]["status"] == "done",
              "summary=" + ev2["finished"]["summary"])
        check("T2 selection 留痕",
              st2.get("selection") == {"mode": "stages", "stages": ["post"]},
              json.dumps(st2.get("selection"), ensure_ascii=False))
        check("T2 extract/solve 假定完成",
              sm2["extract"].get("assumed") is True and
              sm2["solve"].get("assumed") is True and
              sm2["post"].get("assumed") is not True)
        check("T2 assumed 记录 errorCode",
              sm2["extract"].get("errorCode") == "QS-TL-ASSUMED-DONE" and
              sm2["solve"].get("errorCode") == "QS-TL-ASSUMED-DONE")
        check("T2 assumed 通知带 assumed 标志",
              ev2["stages"].get("extract", {}).get("assumed") is True and
              ev2["stages"].get("solve", {}).get("assumed") is True)
        check("T2 started 通知带 selection",
              ev2["task.started"].get("selection", {}).get("mode") == "stages")

        # ── T3 子集入口依赖缺失 → 拒绝 ──
        err = expect_error(rpc, "task.run",
                              {"pipeline": "peec_basic", "dataRoot": fresh,
                               "selection": {"mode": "stages", "stages": ["post"]}})
        check("T3 缺上游拒绝 QS-TL-SEL-DEP-MISSING",
              err is not None and "QS-TL-SEL-DEP-MISSING" in err, str(err)[:160])
        check("T3 未创建任务", not os.path.exists(
            os.path.join(fresh, "Result", "tasks")),
            "fresh/Result 应不存在")

        # ── T4 未知 stage → 拒绝 ──
        err = expect_error(rpc, "task.run",
                              {"pipeline": "peec_basic", "dataRoot": std,
                               "selection": {"mode": "stages", "stages": ["bogus"]}})
        check("T4 未知 stage 拒绝 QS-TL-SEL-UNKNOWNSTAGE",
              err is not None and "QS-TL-SEL-UNKNOWNSTAGE" in err, str(err)[:160])

        # ── T5 resume 已完成任务 → 全部假定完成 ──
        tid5, ev5 = run_task(rpc, std, "peec_basic",
                             {"mode": "resume", "resumeTaskId": tid1})
        st5 = read_state(std, tid5)
        sm5 = stage_map(st5)
        check("T5 resume 全假定 done",
              ev5["finished"]["status"] == "done" and
              all(s.get("assumed") for s in sm5.values()),
              "summary=" + ev5["finished"]["summary"])
        check("T5 selection 留痕（原始 resume 形态）",
              st5.get("selection") == {"mode": "resume", "resumeTaskId": tid1})

        # ── T6 子集 [extract] → resume 该任务 → solve+post 真实执行 ──
        tid6a, ev6a = run_task(rpc, std, "peec_basic",
                               {"mode": "stages", "stages": ["extract"]})
        tid6b, ev6b = run_task(rpc, std, "peec_basic",
                               {"mode": "resume", "resumeTaskId": tid6a})
        st6b = read_state(std, tid6b)
        sm6b = stage_map(st6b)
        check("T6 resume 续跑 done",
              ev6b["finished"]["status"] == "done",
              "summary=" + ev6b["finished"]["summary"])
        check("T6 extract 假定 / solve+post 真实",
              sm6b["extract"].get("assumed") is True and
              sm6b["solve"].get("assumed") is not True and
              sm6b["post"].get("assumed") is not True and
              sm6b["solve"].get("startedAt", -1) >= 0 and
              sm6b["solve"].get("finishedAt", 0) > sm6b["solve"].get("startedAt", 0),
              "solve start/finish=%s/%s" % (sm6b["solve"].get("startedAt"),
                                             sm6b["solve"].get("finishedAt")))

        # ── T7 resume pipeline 不一致 → 拒绝 ──
        err = expect_error(rpc, "task.run",
                              {"pipeline": "thermal_basic", "dataRoot": std,
                               "selection": {"mode": "resume", "resumeTaskId": tid1}})
        check("T7 resume pipeline 不匹配拒绝",
              err is not None and "pipeline mismatch" in err, str(err)[:160])

        # ── T8 resume 时上游产出缺失 → 拒绝 ──
        peec = os.path.join(std, "Result", "compact_em", "extract", "model.peec")
        bak = peec + ".bak"
        shutil.move(peec, bak)
        try:
            err = expect_error(rpc, "task.run",
                                {"pipeline": "peec_basic", "dataRoot": std,
                                 "selection": {"mode": "resume", "resumeTaskId": tid6a}})
        finally:
            shutil.move(bak, peec)
        check("T8 resume 产出缺失拒绝",
              err is not None and "QS-TL-SEL-DEP-MISSING" in err, str(err)[:160])

        # ── T9/T10 inputs.validate 带 selection ──
        r9 = rpc_call(rpc, "inputs.validate",
                      {"dataRoot": std, "pipeline": "peec_basic",
                       "selection": {"mode": "stages", "stages": ["post"]}})
        check("T9 validate 子集 [post]（工件在盘）valid",
              r9.get("valid") is True, json.dumps(r9.get("issues", []),
                                                 ensure_ascii=False)[:160])
        r10 = rpc_call(rpc, "inputs.validate",
                       {"dataRoot": fresh, "pipeline": "peec_basic",
                        "selection": {"mode": "stages", "stages": ["post"]}})
        check("T10 validate 子集 [post]（无工件）invalid 且指向缺失文件",
              r10.get("valid") is False and
              any("model.peec" in i or "S_parameter" in i for i in
                  [str(x) for x in r10.get("issues", [])]),
              json.dumps(r10.get("issues", []), ensure_ascii=False)[:200])

        # ── T11 数值基线比对（可选）──
        if args.ref:
            ref_out = None
            ref_state_dir = os.path.join(args.ref, "Result", "tasks")
            if os.path.isdir(ref_state_dir):
                last = max(os.listdir(ref_state_dir))
                ref_state = read_state(args.ref, last)
                for s in ref_state.get("stages", []):
                    if s["id"] == "post" or (s.get("outputs") and
                                             any("S_parameter" in o for o in s["outputs"])):
                        ref_out = s.get("outputs")
                        break
                if not ref_out:
                    for s in ref_state.get("stages", []):
                        for o in s.get("outputs", []):
                            if "S_parameter" in o:
                                ref_out = [o]
            # 新任务的 solve 产出
            new_out = None
            for s in st1.get("stages", []):
                for o in s.get("outputs", []):
                    if "S_parameter" in o:
                        new_out = o
            if ref_out and new_out:
                a = open(new_out, "rb").read()
                b = open(ref_out[0], "rb").read()
                check("T11 S_parameter.csv 与参考逐字节一致",
                      a == b, "%d bytes vs %d bytes" % (len(a), len(b)))
            else:
                print("SKIP T11: 未找到参考/新产出的 S_parameter 路径")
    finally:
        rpc.close()

    fails = [r for r in results if not r[1]]
    print("==== %d/%d PASS ====" % (len(results) - len(fails), len(results)))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
