# -*- coding: utf-8 -*-
"""PEEC_Studio_20261106_v2.1_fix 交付包门禁（39 条目 = v2.1 白名单 + 6 件变化）。

用法：python package_verify_v21fix.py
GATE_RESULT: ALL_PASS / FAIL
"""
import hashlib
import io
import os
import sys
import zipfile

ZIP = r"E:/code/PEEC_Studio_20261106_v2.1_fix.zip"
STAGING = r"E:/code/PEEC_Studio_20261106_fix"
V21_ZIP = r"E:/code/PEEC_Studio_20261106_v2.1.zip"

CHANGED_EXPECTED = {
    "task_layer/dist/task_layer.exe": 1051946,
    "task_layer/dist/bin/worker_host.exe": 260796,
    "task_layer/dist/README.md": 35905,
    "task_layer/dist/UI_API.md": 48384,
    "task_layer/dist/ERROR_CODES.md": 17162,
}
DOC_NAME = "给UI负责人的交付说明.md"
NEGATIVE_MARKERS = [".bak", "_integ", "validation", "scratch", "probe", ".git", "~", "deliv_v2", "build_prefix", "accept_"]


def sha256_bytes(b):
    return hashlib.sha256(b).hexdigest()


def main():
    failures = []
    checks = 0

    def ck(cond, label, detail=""):
        nonlocal checks
        checks += 1
        tag = "PASS" if cond else "FAIL"
        line = "  [%s] %s%s" % (tag, label, (" — " + detail) if (detail and not cond) else "")
        print(line)
        if not cond:
            failures.append(label)

    def entry_names(z):
        return sorted(n for n in z.namelist() if not n.endswith("/"))

    def load_zip(zp):
        z = zipfile.ZipFile(zp)
        return z, {n: z.read(n) for n in entry_names(z)}

    z_new, new = load_zip(ZIP)
    z21, old = load_zip(V21_ZIP)

    names_new = sorted(new.keys())
    names_old = sorted(old.keys())

    # 1. 39 条目、无目录条目
    ck(len(names_new) == 39, "39 file entries", "got %d" % len(names_new))
    dir_entries = [n for n in z_new.namelist() if n.endswith("/")]
    ck(not dir_entries, "no dir entries", repr(dir_entries[:5]))

    # 2. 白名单 = v2.1 名称集（零增减）
    ck(names_new == names_old, "name set == v2.1 whitelist (no add/remove)",
       "only_new=%s only_old=%s" % (sorted(set(names_new) - set(names_old))[:5],
                                    sorted(set(names_old) - set(names_new))[:5]))

    # 3. 负清单
    neg = [n for n in names_new if any(m in n for m in NEGATIVE_MARKERS)]
    ck(not neg, "negative list zero", repr(neg[:5]))

    # 4. gds2geo = v2.1 七文件（4 EXE，无 r2）
    gds = sorted(n for n in names_new if n.startswith("gds2geo/") and not n.endswith("/"))
    gds_old = sorted(n for n in names_old if n.startswith("gds2geo/") and not n.endswith("/"))
    ck(gds == gds_old and len(gds) == 7, "gds2geo = v2.1 7 files (4 EXE, pre-r2)", repr(gds))

    # 5. zip 逐条目 SHA == staging
    mismatch = []
    for n in names_new:
        p = os.path.join(STAGING, n.replace("/", os.sep))
        if not os.path.isfile(p) or sha256_bytes(open(p, "rb").read()) != sha256_bytes(new[n]):
            mismatch.append(n)
    ck(not mismatch, "zip per-entry SHA == staging (39/39)", repr(mismatch[:5]))

    # 6. 与 v2.1 zip 逐条目比对：33 件一致 + 6 件变化（指定集合）
    identical, changed = [], []
    for n in names_new:
        (identical if new[n] == old[n] else changed).append(n)
    ck(len(changed) == 6 and all(n in CHANGED_EXPECTED or n == DOC_NAME for n in changed),
       "exactly 6 changed vs v2.1 (2 EXE + 3 docs + delivery note)", repr(changed))
    ck(len(identical) == 33, "33 byte-identical to v2.1", "got %d" % len(identical))

    # 7. 变化件尺寸
    for n, sz in CHANGED_EXPECTED.items():
        ck(len(new[n]) == sz, "size %s == %d" % (n, sz), "got %d" % len(new[n]))

    # 8. 交付说明标记 + CRLF
    doc = new.get(DOC_NAME)
    ck(doc is not None, "delivery note present")
    if doc is not None:
        t = doc.decode("utf-8-sig")
        ck("PEEC_Studio_20261106_v2.1_fix" in t, "note marker: package name v2.1_fix")
        ck("修订（2026-11-10，v2.1_fix）" in t, "note marker: 修订条 v2.1_fix")
        ck("不含 2026-11-09/11-10 主线变更" in t, "note marker: 明示不含 11-09/11-10 主线变更")
        ck(b"\r\n" in doc, "delivery note CRLF")

    # 9. task_layer.exe 与 v2.1 旧二进制不同（修复已入）
    ck(new["task_layer/dist/task_layer.exe"] != old["task_layer/dist/task_layer.exe"],
       "task_layer.exe differs from v2.1 pre-fix binary")

    total_bytes = sum(len(v) for v in new.values())
    print("---")
    print("entries: %d | total bytes: %d | v2.1 zip bytes: %d" %
          (len(names_new), total_bytes, sum(len(v) for v in old.values())))
    print("identical to v2.1: %d/39 | changed: %s" % (len(identical), ", ".join(changed)))
    print("GATE_RESULT: %s (%d checks, %d failed)" %
          ("ALL_PASS" if not failures else "FAIL", checks, len(failures)))
    sys.exit(0 if not failures else 1)


if __name__ == "__main__":
    main()
