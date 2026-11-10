# -*- coding: utf-8 -*-
"""生成 PEEC_Studio_20261106_v2.1_fix 交付说明（v2.1 基线 + 缺陷修复修订条）。

从 v2.1 zip 内交付说明取基线文本，锚点替换后写出 CRLF。
用法：python make_delivery_v21fix.py <out.md>
"""
import sys
import zipfile

V21_ZIP = r"E:/code/PEEC_Studio_20261106_v2.1.zip"

# 锚点 = 原文精确子串；替换 = 新文本。每项 (anchor, replacement)，要求恰好出现一次。
REPS = [
    (
        "# PEEC Studio 交付说明（PEEC_Studio_20261106_v2.1）",
        "# PEEC Studio 交付说明（PEEC_Studio_20261106_v2.1_fix）",
    ),
    (
        "日期：2026-11-06（v2.1 构建）",
        "日期：2026-11-10（v2.1_fix 构建）",
    ),
    (
        "上一版：PEEC_Studio_20261105_v2.0（v2.0，2026-11-05 构建；更早 v1.0/v1.1/v1.2）",
        "上一版：PEEC_Studio_20261106_v2.1（v2.1，2026-11-06 构建；更早 v2.0/v1.x）",
    ),
    (
        "仓库基线：feature/pipeline-optimization @ 447b3a0（2026-11-06；v2.0 基线 92c2eeb 之上 P1→P3 + gmsh 共享化 + thermal 1.0.7；DATA_LAYOUT v1.5 修订 3；thermal **1.0.7**；compact_em v1.3；gds2geo **V1.2**）",
        "仓库基线：fix/project-switch-v2.1fix @ 85ae1a1（2026-11-10；v2.1 交付基线 49c26f2 之上叠加 71b185b 切换工程缺陷修复 + 85ae1a1 toNative 分隔符归一；DATA_LAYOUT v1.5 修订 3；thermal **1.0.7**；compact_em v1.3；gds2geo **V1.2（r2 前，4-EXE 布局，不含 2026-11-09 主线 r2 变更）**）",
    ),
    (
        "包 41 → 39 条目，其余内容不变。",
        "包 41 → 39 条目，其余内容不变。\n"
        "修订（2026-11-10，v2.1_fix）：task_layer 缺陷修复重切包（**基于 v2.1 包，不含 2026-11-09/11-10 主线变更**——"
        "前端仅适配 11-06 版；主线变更（gds2geo r2 等）待前端适配后由 PEEC_Studio_20261110_v2.3 交付）："
        "① 根因：同一 serve 进程切换工程后，ModuleRegistry rebind 冻结首工程的已展开契约路径，"
        "工程 B 的 outputChecks/publishes 指向工程 A 的 Result/，B 的产物串用 A 根、B 的 Model/ 发布件缺失；"
        "② 修复：rebind 以模块相对路径模板快照重建 outputChecks/publishes + 执行前契约守卫"
        "（rebind 解析的契约路径越出 dataRoot 即拒绝，新错误码 QS-TL-PATH-ESCAPE / -32004，见包内 ERROR_CODES）；"
        "③ 分隔符归一：toNative 统一为 native 分隔符（反斜杠）形态，state.json outputs / resultFiles 与 publish 日志不再出现正斜杠混形；"
        "④ 相对 v2.1 仅 6 文件变化：task_layer.exe / bin/worker_host.exe（重构建）+ README / UI_API / ERROR_CODES（补新错误码文档）+ 本交付说明；"
        "config / 模块 / templates / tools 等其余 33 件与 v2.1 包**逐字节一致**；"
        "⑤ 回归验收 RC=0（P1 旧二进制复现冻结 + P2 A→B→A 同进程红线全过 + P3 validate 无误报）；"
        "⑥ 临时规避「每进程单工程 / 切换工程须重启后端」自本包起不再需要（v2.1 及更早包用户仍需要）。",
    ),
    (
        "10. **gmsh 共享化（gds2geo V1.2）**：gmsh 4.11.1 移至包根 `tools\\gmsh\\`（见 ⑥/修订条）；gds2geo 包 7 文件、四主 EXE 逐字节未变；对 UI 无输入/输出变化（gmsh 仍由平台经 env 注入，UI 不接触）。",
        "10. **gmsh 共享化（gds2geo V1.2）**：gmsh 4.11.1 移至包根 `tools\\gmsh\\`（见 ⑥/修订条）；gds2geo 包 7 文件、四主 EXE 逐字节未变；对 UI 无输入/输出变化（gmsh 仍由平台经 env 注入，UI 不接触）。\n"
        "11. **task_layer 切换工程缺陷修复（v2.1_fix，2026-11-10）**：同一 serve 进程切换工程后产物路径串用"
        "（B 的 stages[].outputs / resultFiles 指向 A 根、B 的 Model/ 发布件缺失）——根因 = rebind 冻结首工程已展开契约路径；"
        "修复 = 模板快照重建 + 执行前契约守卫（新错误码 QS-TL-PATH-ESCAPE / -32004，合法 manifest 不触发）+ 分隔符形态归一（native 反斜杠）。"
        "UI 零强制适配（行为修复 + 文档补码，见 §八）。",
    ),
]

APPEND = """
## 八、v2.1_fix 对 UI（2026-11-10）

**零强制适配**：协议、方法集、事件、数据布局（DATA_LAYOUT v1.5 修订 3）均未变；本包为 v2.1 的行为修复重切。
- 切换工程安全：同一 serve 进程内 A→B→A 任意顺序切换，产物严格落在各自工程根（不再串用）；「每进程单工程 / 切工程重启后端」的临时规避自本包起废止（v2.1 及更早包用户仍需要）；
- 路径形态归一：state.json `stages[].outputs` / `resultFiles` 与 publish 落位统一为 native 分隔符（Windows 反斜杠）形态，不再出现正斜杠混形（消费端无需改动；原按字符串前缀判定的逻辑同样可用）；
- 新错误码：`-32004`（ErrConfig，机器码 `QS-TL-PATH-ESCAPE`）——仅当配置（manifest / 流水线路由）的相对路径解析越出 dataRoot 时触发，属部署配置错误；标准部署不出现；
- 包内容：相对 v2.1 共 39 条目，6 件变化（task_layer.exe / bin/worker_host.exe / README / UI_API / ERROR_CODES / 本说明），其余 33 件与 v2.1 包逐字节一致（含 config、全部模块、templates、tools/gmsh）；
- 版本关系：本包 = v2.1 + 缺陷修复；主线 PEEC_Studio_20261110_v2.3 含同一修复 + 2026-11-09/11-10 主线变更（gds2geo r2 等），前端适配后以 v2.3 为后续版本。
"""


def main():
    out_path = sys.argv[1]
    z = zipfile.ZipFile(V21_ZIP)
    names = [n for n in z.namelist() if n.endswith(".md") and "/" not in n]
    assert len(names) == 1, names
    raw = z.read(names[0])
    text = raw.decode("utf-8-sig")
    n_crlf = raw.count(b"\r\n")
    print("base: %s (%d bytes, CRLF=%d)" % (names[0], len(raw), n_crlf))
    for i, (a, b) in enumerate(REPS):
        c = text.count(a)
        if c != 1:
            raise SystemExit("anchor %d not unique (count=%d): %r" % (i + 1, c, a[:60]))
        text = text.replace(a, b)
        print("anchor %d ok" % (i + 1))
    if not text.endswith("\n"):
        text += "\n"
    # 统一 CRLF（与包内既有交付说明一致）
    text = text.replace("\r\n", "\n").replace("\n", "\r\n")
    with open(out_path, "wb") as f:
        f.write(text.encode("utf-8"))
    print("written %s (%d chars, CRLF)" % (out_path, len(text)))


if __name__ == "__main__":
    main()
