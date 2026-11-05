# -*- coding: utf-8 -*-
"""P2 阶段库 + 引用组合（schemaVersion 2）验收测试。

用法：
  python p2_stage_lib_test.py --exe <task_layer.exe> --cfg-src <task_layer/config 目录> --work <工作目录> --git <repo 根> --v1-commit <P1 提交>

验收（计划 §6 P2 ①-④；⑤ 基线另跑回归套件）：
  ① v1/v2 双格式解析：内置三 pipeline 物化结果逐字段等价（pipelines.list 载荷对照）
  ② 未知 ref -> 加载期报错（serve 启动即 ErrConfig，进程退出 2）
  ③ 本地覆盖合并规则：params 键级合并 / requires 本地赢 / optional 本地赢 /
     artifacts 键级合并 / userGate extends（库级键不可被本地取消）
  ④ 同一 ref 被两条链引用 + 各自 params 覆盖生效（物化互不串扰）
  附加：params 白名单加载期 fail-fast（manifest allowedArgs 之外的键拒载）
"""
import argparse, json, os, shutil, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rpc_smoke import Rpc


def make_cfg(work, name, pipelines_json, modules_src):
    d = os.path.join(work, "cfg_" + name)
    if os.path.exists(d):
        shutil.rmtree(d)
    os.makedirs(d)
    shutil.copy2(os.path.join(modules_src, "modules.json"), os.path.join(d, "modules.json"))
    with open(os.path.join(d, "pipelines.json"), "w", encoding="utf-8") as f:
        f.write(pipelines_json)
    return d


def raw_payloads(rpc):
    """pipelines.list 原样载荷（含 ref 溯源等全部字段）。"""
    res = rpc.call("pipelines.list")
    return {p["id"]: p for p in res["pipelines"]}


def list_payloads(rpc):
    """归一化载荷（比较用）：空字段缺省；ref 溯源字段不入比较集。"""
    res = rpc.call("pipelines.list")
    out = {}
    for p in res["pipelines"]:
        out[p["id"]] = {
            "displayName": p.get("displayName"),
            "maxParallel": p.get("maxParallel"),
            "stages": [{
                "id": s["id"], "module": s.get("module"), "name": s.get("name"),
                "optional": s.get("optional", False),
                "dependsOn": s.get("dependsOn", []),
                "requires": s.get("requires", []),
                "params": s.get("params", {}),
                "artifacts": s.get("artifacts", {}),
                "userGate": s.get("userGate", {}),
            } for s in p.get("stages", [])],
        }
    return out


def norm(payloads):
    """等价比较归一化：空字段缺省 + 键排序。"""
    return json.dumps(payloads, ensure_ascii=False, sort_keys=True)


def serve_err(exe, cfg):
    """配置非法时 serve 的首行错误 + 退出码。"""
    p = subprocess.Popen([exe, "serve", "--config", cfg],
                         cwd=os.path.dirname(exe),
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, bufsize=1, text=True,
                         encoding="utf-8", errors="replace")
    line = p.stdout.readline()
    try:
        rc = p.wait(timeout=15)
    except subprocess.TimeoutExpired:
        p.kill()
        rc = -1
    return line.strip(), rc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--cfg-src", required=True)
    ap.add_argument("--work", required=True)
    ap.add_argument("--git", required=True)
    ap.add_argument("--v1-commit", required=True)
    args = ap.parse_args()

    exe = os.path.abspath(args.exe)
    work = os.path.abspath(args.work)
    if os.path.exists(work):
        shutil.rmtree(work)
    os.makedirs(work)

    # v1 参考配置（P1 提交的 pipelines.json，全内联）
    v1_text = subprocess.run(
        ["git", "-C", args.git, "show", args.v1_commit + ":task_layer/config/pipelines.json"],
        capture_output=True, text=True, encoding="utf-8").stdout
    v1cfg = make_cfg(work, "v1", v1_text, args.cfg_src)
    # v2 现行配置（阶段库 + ref）
    v2_text = open(os.path.join(args.cfg_src, "pipelines.json"), encoding="utf-8").read()
    v2cfg = make_cfg(work, "v2", v2_text, args.cfg_src)

    results = []

    def check(name, cond, detail=""):
        results.append((name, bool(cond), detail))
        print("%s %s %s" % ("PASS" if cond else "FAIL", name, detail))

    servers = []   # 已启动的 serve 进程（统一收尾）

    class Srv(Rpc):
        def __init__(self, exe, cfg):
            self.proc = subprocess.Popen(
                [exe, "serve", "--config", cfg],
                cwd=os.path.dirname(exe),
                stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, bufsize=1, text=True,
                encoding="utf-8", errors="replace")
            self.next_id = 0

    s1 = Srv(exe, v1cfg)
    servers.append(s1)
    try:
        p1r = raw_payloads(s1)
        p1 = list_payloads(s1)
        s2 = Srv(exe, v2cfg)
        servers.append(s2)
        try:
            p2r = raw_payloads(s2)
            p2 = list_payloads(s2)
            # ① 物化等价（list_payloads 已归一化：v2 的 ref 溯源字段不入比较集）
            check("① 三内置 pipeline id 集一致",
                  set(p1) == set(p2) and len(p1) == 3,
                  str(sorted(p1)) + " vs " + str(sorted(p2)))
            eq_detail = []
            for pid in sorted(p1):
                a, b = json.dumps(p1[pid], ensure_ascii=False, sort_keys=True),                        json.dumps(p2.get(pid, {}), ensure_ascii=False, sort_keys=True)
                ok = a == b
                if not ok:
                    eq_detail.append(pid + ": v1=" + a[:150] + " v2=" + b[:150])
                check("① 物化等价 " + pid, ok, "; ".join(eq_detail)[:300])
            # v2 载荷应带 ref 溯源（原样载荷统计；v1 侧为 0）
            nref = sum(1 for p in p2r.values() for s in p.get("stages", [])
                       if s.get("ref"))
            nref1 = sum(1 for p in p1r.values() for s in p.get("stages", [])
                        if s.get("ref"))
            check("① v2 载荷带 ref 溯源（9 阶段全 ref，v1 无）",
                  nref == 9 and nref1 == 0, "v2=%s v1=%s" % (nref, nref1))
            s2.proc.stdin.close()
            s2.proc.wait(timeout=10)
        finally:
            pass
        s1.proc.stdin.close()
        s1.proc.wait(timeout=10)

        # ② 未知 ref -> 加载期报错
        bad = json.loads(v2_text)
        bad["pipelines"]["peec_basic"]["stages"][0]["ref"] = "nope.bogus"
        badcfg = make_cfg(work, "badref", json.dumps(bad, ensure_ascii=False), args.cfg_src)
        line, rc = serve_err(exe, badcfg)
        check("② 未知 ref 加载期报错", "unknown ref" in line and "nope.bogus" in line, line[:150])
        check("② serve 配置错误退出码 2", rc == 2, str(rc))

        # ③ 合并规则（lib + 本地覆盖 + userGate extends + 白名单附加）
        merge = json.loads(v2_text)
        merge["stages"]["lib.merge"] = {
            "module": "peec.solver",
            "name": "libname",
            "params": {"export": "post,report", "s-columns": 100},
            "artifacts": {"r1": "A.txt", "r2": "B.txt"},
            "requires": ["libreq.txt"],
            "optional": False,
            "userGate": {"confirm": True},
        }
        merge["stages"]["lib.plain"] = {"module": "peec.model_extract", "name": "plainlib"}
        merge["pipelines"]["merge_test"] = {
            "displayName": "merge", "maxParallel": 1,
            "stages": [
                {"id": "full", "ref": "lib.plain", "dependsOn": []},
                {"id": "over", "ref": "lib.merge", "dependsOn": [],
                 "name": "localname", "optional": True,
                 "requires": ["localreq.txt"],
                 "params": {"s-columns": 41, "export": "report"},
                 "artifacts": {"r2": "B2.txt"},
                 "userGate": {"waitForFiles": ["x.json"], "confirm": False}},
                {"id": "clean", "ref": "lib.merge", "dependsOn": []},
            ],
        }
        mcfg = make_cfg(work, "merge", json.dumps(merge, ensure_ascii=False), args.cfg_src)
        s3 = Srv(exe, mcfg)
        try:
            pm = list_payloads(s3)
            st = {s["id"]: s for s in pm["merge_test"]["stages"]}
            over = st["over"]
            check("③ params 键级合并本地赢",
                  over["params"] == {"export": "report", "s-columns": 41},
                  json.dumps(over["params"]))
            check("③ requires 本地赢", over["requires"] == ["localreq.txt"],
                  json.dumps(over["requires"]))
            check("③ optional 本地赢", over["optional"] is True)
            check("③ name 本地赢", over["name"] == "localname")
            check("③ artifacts 键级合并",
                  over["artifacts"] == {"r1": "A.txt", "r2": "B2.txt"},
                  json.dumps(over["artifacts"]))
            check("③ userGate extends（新增键并入，库级 confirm 不可被本地取消）",
                  over["userGate"] == {"confirm": True, "waitForFiles": ["x.json"]},
                  json.dumps(over["userGate"]))
            clean = st["clean"]
            check("③ 无本地覆盖 -> 库默认原样",
                  clean["name"] == "libname" and clean["optional"] is False
                  and clean["requires"] == ["libreq.txt"]
                  and clean["params"] == {"export": "post,report", "s-columns": 100}
                  and clean["artifacts"] == {"r1": "A.txt", "r2": "B.txt"},
                  json.dumps(clean))
            plain = st["full"]
            check("③ 无参库 ref 正常物化",
                  plain["module"] == "peec.model_extract" and plain["name"] == "plainlib",
                  json.dumps(plain))
            s3.proc.stdin.close()
            s3.proc.wait(timeout=10)
        except Exception as e:
            check("③ merge serve 异常", False, repr(e))
            s3.proc.kill()

        # 附加：params 白名单 fail-fast（allowedArgs 之外的键）
        badp = json.loads(v2_text)
        badp["stages"]["lib.badparam"] = {
            "module": "peec.model_extract",
            "name": "badparam",
            "params": {"notallowed": 1},
        }
        badp["pipelines"]["badparam_test"] = {
            "displayName": "bp", "maxParallel": 1,
            "stages": [{"id": "s", "ref": "lib.badparam", "dependsOn": []}],
        }
        bcfg = make_cfg(work, "badparam", json.dumps(badp, ensure_ascii=False), args.cfg_src)
        line, rc = serve_err(exe, bcfg)
        check("params 白名单拒载", "not in allowedArgs" in line and "notallowed" in line,
              line[:150])

        # ④ 同一 ref 两条链 + 各自 params 覆盖
        chains = json.loads(v2_text)
        chains["stages"]["lib.solver2"] = {
            "module": "peec.solver", "name": "共享求解",
            "params": {"s-columns": 5},
            "requires": ["Result/compact_em/extract/model.peec"],
        }
        chains["pipelines"]["chainA"] = {
            "displayName": "A", "maxParallel": 1,
            "stages": [{"id": "solveA", "ref": "lib.solver2", "dependsOn": [],
                        "params": {"s-columns": 10}}],
        }
        chains["pipelines"]["chainB"] = {
            "displayName": "B", "maxParallel": 1,
            "stages": [{"id": "solveB", "ref": "lib.solver2", "dependsOn": [],
                        "params": {"s-columns": 20}}],
        }
        ccfg = make_cfg(work, "chains", json.dumps(chains, ensure_ascii=False), args.cfg_src)
        s4 = Srv(exe, ccfg)
        try:
            pc = list_payloads(s4)
            sa = pc["chainA"]["stages"][0]
            sb = pc["chainB"]["stages"][0]
            check("④ 同 ref 两链各自 params 生效",
                  sa["params"] == {"s-columns": 10}
                  and sb["params"] == {"s-columns": 20},
                  json.dumps(sa["params"]) + " / " + json.dumps(sb["params"]))
            check("④ 库级接线两链共享（requires）",
                  sa["requires"] == sb["requires"] == ["Result/compact_em/extract/model.peec"],
                  json.dumps(sa["requires"]))
            s4.proc.stdin.close()
            s4.proc.wait(timeout=10)
        except Exception as e:
            check("④ chains serve 异常", False, repr(e))
            s4.proc.kill()
    finally:
        for srv in servers:
            if srv.proc.poll() is None:
                srv.proc.kill()

    print()
    fails = [n for n, ok, _ in results if not ok]
    print("RESULT: %s" % ("PASS" if not fails else "FAIL %s" % fails))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
