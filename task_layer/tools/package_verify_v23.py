# -*- coding: utf-8 -*-
"""v2.3 打包验证门：zip 37 条目白名单 + 负面零命中 + 逐件 SHA256（zip vs staging vs v2.2 zip）。"""
import hashlib, io, os, sys, zipfile

STAGE = r"E:/code/PEEC_Studio_20261110"
ZIP23 = r"E:/code/PEEC_Studio_20261110_v2.3.zip"
ZIP22 = r"E:/code/PEEC_Studio_20261109_v2.2.zip"

def sha(path_or_zip, name=None):
    if name is None:
        h = hashlib.sha256()
        with open(path_or_zip, "rb") as f:
            while True:
                b = f.read(1 << 20)
                if not b: break
                h.update(b)
        return h.hexdigest()
    with zipfile.ZipFile(path_or_zip) as z:
        return hashlib.sha256(z.read(name)).hexdigest()

z23 = zipfile.ZipFile(ZIP23)
z22 = zipfile.ZipFile(ZIP22)
e23 = [i for i in z23.infolist() if not i.is_dir()]
e22 = {i.filename for i in z22.infolist() if not i.is_dir()}
names = sorted(i.filename for i in e23)

ok = True
def check(tag, cond, detail=""):
    global ok
    mark = "PASS" if cond else "FAIL"
    if not cond: ok = False
    print(f"[{mark}] {tag} {detail}")

check("条目数 = 37", len(e23) == 37, f"n={len(e23)}")
check("无目录条目", all(not i.is_dir() for i in z23.infolist()))
check("白名单 = v2.2 同 37 名", set(names) == e22,
      f"only23={sorted(set(names) - e22)} only22={sorted(e22 - set(names))}")
bad = [n for n in names if any(k in n.lower() for k in (".bak", ".git", "_integ", "validation", "scratch"))]
check("负面清单零命中", not bad, f"hits={bad}")
check("gds2geo 无 gmsh", not any(n.startswith("gds2geo/gmsh") for n in names))

# 逐件：zip 内容 SHA == staging 文件 SHA（37/37）
mismatch = []
for n in names:
    rel = n.replace("/", os.sep)
    sp = os.path.join(STAGE, rel)
    if not os.path.isfile(sp) or sha(sp) != sha(ZIP23, n):
        mismatch.append(n)
check("zip 逐件 == staging（37/37）", not mismatch, f"mismatch={mismatch}")

# 29 件与 v2.2 逐字节一致
changed = {
    "task_layer/dist/task_layer.exe",
    "task_layer/dist/bin/worker_host.exe",
    "task_layer/dist/config/modules.json",
    "task_layer/dist/config/pipelines.json",
    "task_layer/dist/README.md",
    "task_layer/dist/UI_API.md",
    "task_layer/dist/ERROR_CODES.md",
    os.path.basename("给UI负责人的交付说明.md").encode("utf-8").decode("utf-8"),
}
diff22 = [n for n in names if n not in changed and sha(ZIP23, n) != sha(ZIP22, n)]
same22 = [n for n in names if n not in changed and sha(ZIP23, n) == sha(ZIP22, n)]
check("29 件与 v2.2 逐字节一致", len(diff22) == 0 and len(same22) == 29,
      f"same={len(same22)} diff={diff22}")

# 8 件变更确认（与预期源 SHA 不同 + 尺寸）
sizes = {i.filename: i.file_size for i in e23}
check("task_layer.exe 尺寸 1056854", sizes["task_layer/dist/task_layer.exe"] == 1056854,
      f"got={sizes.get('task_layer/dist/task_layer.exe')}")
check("worker_host.exe 尺寸 260796", sizes["task_layer/dist/bin/worker_host.exe"] == 260796,
      f"got={sizes.get('task_layer/dist/bin/worker_host.exe')}")
check("modules.json 尺寸 3767", sizes["task_layer/dist/config/modules.json"] == 3767)
check("pipelines.json 尺寸 6207", sizes["task_layer/dist/config/pipelines.json"] == 6207)

# 交付说明：zip 内 == staging 且含 v2.3 标记
dn = [n for n in names if n.endswith(".md") and n.startswith("给UI")][0]
d = z23.read(dn).decode("utf-8")
check("交付说明 = v2.3 版", d.startswith("# PEEC Studio 交付说明（PEEC_Studio_20261110_v2.3）")
      and "修订（2026-11-10，v2.3）" in d and "## 八、v2.3 对 UI（2026-11-10）" in d)
check("交付说明 CRLF", b"\r\n" in z23.read(dn))

print()
print("GATE_RESULT:", "ALL_PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
