# -*- coding: utf-8 -*-
"""task_layer 三条内建流水线 P1 基线回归（数值/字节红线）。
A. thermal_basic @ dev/cases/thermal/demo1_4.1: MaximumTemperature=344.1037 K,
   NodeTemperatureAtOperatingPoint.csv 9564 行, networkNodes=12750
B. gds_full @ dev/cases/heater_strip: Result/gds2geo/** 与 final3 参考逐字节一致
"""
import json, os, shutil, subprocess, sys, time, queue, threading
EXE = r"E:\code\PEEC_Studio\task_layer\build\task_layer.exe"
CASES = r"E:\code\PEEC_Studio\dev\cases"
WORK = r"E:\code\_integ\p1baseline"
GDS_REF = r"E:\code\_integ\final3\case\DATA\Result\gds2geo"
fails = []
def check(name, cond, detail=""):
    print(("PASS " if cond else "FAIL ") + name + (" " + str(detail) if detail else ""))
    if not cond: fails.append(name)
class Serve:
    def __init__(self):
        self.proc = subprocess.Popen([EXE, "serve"], cwd=os.path.dirname(EXE),
                                    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    bufsize=1, text=True, encoding="utf-8", errors="replace")
        self.q = queue.Queue()
        threading.Thread(target=self._run, daemon=True).start()
        self.nid = 0
    def _run(self):
        for line in self.proc.stdout: self.q.put(line)
    def send(self, m, p=None):
        self.nid += 1
        msg = {"jsonrpc": "2.0", "id": self.nid, "method": m, "params": p or {}}
        self.proc.stdin.write(json.dumps(msg) + chr(10)); self.proc.stdin.flush()
        return self.nid
    def call(self, m, p=None, timeout=120):
        rid = self.send(m, p)
        dl = time.time() + timeout
        while time.time() < dl:
            line = self.q.get(timeout=min(20, dl - time.time())) if (dl - time.time()) > 0 else None
            if line is None or line.strip() == "": continue
            mm = json.loads(line)
            if mm.get("id") == rid:
                if "error" in mm: raise RuntimeError(mm["error"])
                return mm["result"]
        raise TimeoutError(m)
    def run_task(self, root, pipeline, timeout=1800):
        rid = self.send("task.run", {"pipeline": pipeline, "dataRoot": root})
        events = {"finished": None, "stages": {}}
        dl = time.time() + timeout
        while time.time() < dl:
            try: line = self.q.get(timeout=30)
            except queue.Empty: continue
            if line is None or line.strip() == "": continue
            mm = json.loads(line)
            if mm.get("id") == rid: continue
            meth = mm.get("method")
            if meth == "task.stage.finished":
                p2 = mm["params"]; events["stages"][p2["stageId"]] = p2
            elif meth == "task.finished":
                events["finished"] = mm["params"]
                break
        return events
    def close(self):
        try: self.send("shutdown")
        except Exception: pass
        time.sleep(1)
        if self.proc.poll() is None: self.proc.terminate()
if os.path.exists(WORK): shutil.rmtree(WORK)
os.makedirs(WORK)
# ── A. thermal_basic ──
# 设计（v2.0 既有）：thermal_basic 的热网格经 stage artifacts 路由到
# Result/gds2geo/thermal/model_thermal.msh（gds2geo 热链产物位）。
# 独立算例 demo1_4.1 自带 Model/model_thermal.msh → 按 v2.0 流程先落位该 mesh。
th = os.path.join(WORK, "th")
shutil.copytree(os.path.join(CASES, "thermal", "demo1_4.1"), th)
# 备份算例自带（v2.0 时期）的数值结果作为字节红线参考
refdir = os.path.join(th, "_ref")
os.makedirs(os.path.join(refdir, "post"))
os.makedirs(os.path.join(refdir, "extract"))
for f in ["MaximumTemperature.txt", "NodeTemperatureAtOperatingPoint.csv"]:
    shutil.copy2(os.path.join(th, "Result", "thermal", "post", f), os.path.join(refdir, "post", f))
for f in ["ThermalNetList.sp", "ThermalTemperatureMap.csv"]:
    shutil.copy2(os.path.join(th, "Result", "thermal", "extract", f), os.path.join(refdir, "extract", f))
os.makedirs(os.path.join(th, "Result", "gds2geo", "thermal"), exist_ok=True)
shutil.copy2(os.path.join(th, "Model", "model_thermal.msh"),
             os.path.join(th, "Result", "gds2geo", "thermal", "model_thermal.msh"))
# 网格换到 gds 产物位后，UI 侧需同步 source_thermal.json 的 meshFile 绑定
#（maintainedBy: ui；网格字节不变 → meshSha256 不变）
_src = os.path.join(th, "Model", "source_thermal.json")
_sj = json.load(open(_src, encoding="utf-8"))
_sj["meshFile"] = "Result/gds2geo/thermal/model_thermal.msh"
json.dump(_sj, open(_src, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
print("[A] thermal_basic @", th)
sv = Serve()
ev = sv.run_task(th, "thermal_basic")
sv.close()
fin = ev["finished"] or {}
check("A thermal done", fin.get("status") == "done", str(fin.get("status")) + " " + str(fin.get("summary", ""))[:120])
for sid, p2 in sorted(ev["stages"].items()):
    print("    stage", sid, p2.get("status"), p2.get("errorCode", ""))
def same_bytes(a, b):
    if not (os.path.exists(a) and os.path.exists(b)): return False
    return open(a, "rb").read() == open(b, "rb").read()
check("A MaximumTemperature 字节一致", same_bytes(os.path.join(refdir, "post", "MaximumTemperature.txt"),
      os.path.join(th, "Result", "thermal", "post", "MaximumTemperature.txt")))
mttxt = open(os.path.join(th, "Result", "thermal", "post", "MaximumTemperature.txt"), encoding="utf-8").read().strip()
check("A MaximumTemperature=344.1037", "344.1037" in mttxt, mttxt[:60])
check("A NodeTemperature 字节一致", same_bytes(os.path.join(refdir, "post", "NodeTemperatureAtOperatingPoint.csv"),
      os.path.join(th, "Result", "thermal", "post", "NodeTemperatureAtOperatingPoint.csv")))
data_rows = len(open(os.path.join(th, "Result", "thermal", "post", "NodeTemperatureAtOperatingPoint.csv"), encoding="utf-8").read().splitlines()) - 1
check("A NodeTemperature 9564 数据行", data_rows == 9564, data_rows)
check("A ThermalNetList.sp 字节一致", same_bytes(os.path.join(refdir, "extract", "ThermalNetList.sp"),
      os.path.join(th, "Result", "thermal", "extract", "ThermalNetList.sp")))
check("A ThermalTemperatureMap 字节一致", same_bytes(os.path.join(refdir, "extract", "ThermalTemperatureMap.csv"),
      os.path.join(th, "Result", "thermal", "extract", "ThermalTemperatureMap.csv")))
rep = os.path.join(th, "Result", "thermal", "extract", "extraction_report.json")
nodes = None
if os.path.exists(rep):
    rj = json.load(open(rep, encoding="utf-8"))
    def find(o, key="networkNodes"):
        if isinstance(o, dict):
            for k, v in o.items():
                if k == key: return v
                r = find(v, key)
                if r is not None: return r
        elif isinstance(o, list):
            for v in o:
                r = find(v, key)
                if r is not None: return r
        return None
    nodes = find(rj)
check("A networkNodes=12750", nodes == 12750, nodes)

# ── B. gds_full ──
gs = os.path.join(WORK, "gs")
shutil.copytree(os.path.join(CASES, "heater_strip"), gs)
print("[B] gds_full @", gs)
sv = Serve()
ev = sv.run_task(gs, "gds_full")
sv.close()
fin = ev["finished"] or {}
check("B gds done", fin.get("status") == "done", str(fin.get("status")) + " " + str(fin.get("summary", ""))[:120])
for sid, p2 in sorted(ev["stages"].items()):
    print("    stage", sid, p2.get("status"), p2.get("errorCode", ""))
same, diff, missing = 0, [], []
for dirpath, dirnames, filenames in os.walk(GDS_REF):
    for fn in filenames:
        rp = os.path.join(dirpath, fn)
        rel = os.path.relpath(rp, GDS_REF)
        np_ = os.path.join(gs, "Result", "gds2geo", rel)
        if not os.path.exists(np_): missing.append(rel); continue
        a = open(rp, "rb").read(); b = open(np_, "rb").read()
        if a == b: same += 1
        else: diff.append("%s (%d vs %d)" % (rel, len(b), len(a)))
check("B gds2geo 产物与参考字节一致", not diff and not missing, "same=%d diff=%s missing=%s" % (same, diff[:4], missing[:4]))
print("\nRESULT:", "PASS" if not fails else "FAIL " + str(fails))
sys.exit(0 if not fails else 1)