# -*- coding: utf-8 -*-
"""v2.3 交付说明生成器：v2.2 基线文本 + 头部替换 + 修订条插入 + §二 第 12 条 + §八。"""
import io, sys

SRC = r"E:/code/PEEC_Studio_20261109/给UI负责人的交付说明.md"
DST = r"E:/code/PEEC_Studio_20261110/给UI负责人的交付说明.md"

text = io.open(SRC, encoding="utf-8").read()
lines = text.splitlines()

def rep(old, new, tag):
    global text
    n = text.count(old)
    assert n == 1, (tag, n)
    text = text.replace(old, new)
    print("OK  ", tag)

# ── 头部 ──
rep("# PEEC Studio 交付说明（PEEC_Studio_20261109_v2.2）",
    "# PEEC Studio 交付说明（PEEC_Studio_20261110_v2.3）", "标题")
rep("日期：2026-11-09（v2.2 构建）",
    "日期：2026-11-10（v2.3 构建）", "日期")
rep("上一版：PEEC_Studio_20261106_v2.1（v2.1 重切包，2026-11-06 构建；更早 v1.0/v1.1/v1.2/v2.0）",
    "上一版：PEEC_Studio_20261109_v2.2（2026-11-09 构建；更早 v2.1 重切包 / v1.0 / v1.1 / v1.2 / v2.0）", "上一版")
rep("仓库基线：feature/pipeline-optimization @ 447b3a0（2026-11-06；v2.0 基线 92c2eeb 之上 P1→P3 + gmsh 共享化 + thermal 1.0.7；DATA_LAYOUT v1.5 修订 3；thermal **1.0.7**；compact_em v1.3；gds2geo **V1.2**）",
    "仓库基线：feature/pipeline-optimization @ f8408ea（2026-11-10；c5b0cf7 gds2geo V1.2 r2 基线 + 59072c7 切换工程缺陷修复（fix 分支 71b185b 合入主线）+ f8408ea toNative 分隔符归一；DATA_LAYOUT v1.6；thermal **1.0.7**；compact_em v1.3；gds2geo **V1.2 r2**）", "仓库基线")

# ── 修订（2026-11-10，v2.3）条：插在 v2.2 ⑨ flags 条之后（## 一、 之前）──
REV = """修订（2026-11-10，v2.3）：task_layer 缺陷修复——**切换工程后产物路径串用**（v2.1/v2.2 基线上发现的同进程切工程缺陷，2026-11-10 修复并全量回归收口；**UI 零强制适配**）：
① **根因**：同一 task_layer 进程内切换工程（dataRoot A→B）时，rebindDataRoot 未从相对路径模板全量重建各阶段的 outputChecks / publishes 落位契约——首次任务的已展开绝对路径冻结在共享模块注册表中，后续任务的 publish 落位仍指向旧工程根（实测：B 工程 state.json 出现 20 条 A 工程根的 Model/ 路径，且真实跨工程写文件）。
② **修复**：rebind 时以相对路径模板快照（outputChecksTemplate / publishesTemplate）为源、按当前 dataRoot 全量重建；宿主 publishes 叠加经 publishOverrides 按 (ref, dst) 精确匹配（仅 Model/、Setting/ 目标）；新增契约包含性守卫 pathWithinDataRoot——发布目标越出数据根即拒绝（**QS-TL-PATH-ESCAPE / -32004**；validate 阶段同码报 valid:false + issues，stderr 留痕）。
③ **分隔符归一（本版收口）**：toNative 展开路径统一为系统分隔符形态（toNativeSeparators）——此前 publish 落位路径在 state.json outputs / task.result resultFiles / task.log [publish] 日志中以 POSIX 正斜杠形态出现（与同数组 outputChecks 的反斜杠形态混合）；本版起**全部路径字段统一为 Windows 反斜杠形态**（落位目标不变，仅形态归一；详见 §八.2）。
④ **EXE / 配置 / 文档**：task_layer.exe 重建（1,056,854 B，2026-11-10 构建）；bin\worker_host.exe 重建（260,796 B）；config\（modules.json 3,767 B / pipelines.json 6,207 B）与 README / UI_API / ERROR_CODES 同步主线最新（新错误码 / publishes 机制 / 守卫表述已写入）；4 个 DLL 与 v2.2 逐字节一致；包其余 25 件（compact_em / gds2geo / thermal / templates / tools）与 v2.2 **逐字节一致**（SHA256 全量对照）。
⑤ **回归（2026-11-10，全量 RC=0）**：P1 旧二进制复现（B 状态 20 条 A 根路径，缺陷证据固化）；P2 新二进制 A→B→A 单 serve 进程红线——outputs/resultFiles n=12 全在各自工程根、B 状态零 A 根命中、发布件 Model\model_em.msh 双根字节一致、B 运行后 A 树 diff=0、A→B→A 复跑 A outputs 全在 A 根；P3 守卫零误报（B 工程 validate valid=true、无 QS-TL-PATH-ESCAPE）。验收脚本 task_layer/tools/project_switch_test_mainline.py 已入库。
⑥ **workaround 废止**：此前临时规避（每进程单工程 / 切工程须重启 task_layer 进程）自本版起不再需要——同进程连续切换工程已验证安全。
"""
anchor = "## 一、目录结构与摆放"
assert text.count(anchor) == 1
text = text.replace(anchor, REV + anchor)
print("OK   修订条插入")

# ── §二 第 12 条（插在 ## 三、 之前，item 11 行后）──
ITEM12 = """12. **task_layer 切换工程缺陷修复（2026-11-10，v2.3 主题）**：同进程切换工程（dataRoot A→B→A）产物路径串用——rebind 模板全量重建 + 宿主 publishes 叠加 + 契约包含性守卫（QS-TL-PATH-ESCAPE / -32004）+ 路径形态统一为系统反斜杠。详见头部"修订（2026-11-10，v2.3）"条与 §八。

"""
anchor3 = "## 三、流水线与调用"
assert text.count(anchor3) == 1
text = text.replace(anchor3, ITEM12 + anchor3)
print("OK   §二 第 12 条")

# ── §八 追加（文末）──
SEC8 = """
## 八、v2.3 对 UI（2026-11-10）

**零强制适配**：协议形状、方法集、通知全部未变；本版为 task_layer 缺陷修复重切包。

**需感知的三点**：
1. **切换工程安全**：同一 task_layer 进程内连续切换工程（dataRoot 变化）不再串用产物路径（v2.1/v2.2 基线缺陷，本版修复）——同进程 A→B→A 连续任务已验证：各任务 outputs / resultFiles / publish 落位均在各自工程根内，跨工程零写。此前"每进程单工程"的临时规避不再需要。
2. **路径形态归一**：state.json 各阶段 outputs、task.result 的 resultFiles、task.log 的 [publish] 日志——路径字段统一为**系统分隔符形态**（Windows = 反斜杠）。此前 publish 落位路径存在 POSIX 正斜杠形态（与同数组其他路径混合）；UI 路径比较/展示逻辑现可按单一形态处理（建议仍按规范解析，勿做字符串假设）。
3. **新错误码**：QS-TL-PATH-ESCAPE / -32004（契约路径越出数据根）——标准部署下不应出现（内置流水线契约均在包根内）；若出现表示部署侧自定义配置错误，validate 阶段即报 valid:false 并附 issues（按既有 issues 渲染逻辑展示即可）。

**包内容**：task_layer.exe 重建（1,056,854 B）；worker_host.exe 重建（260,796 B）；config 2 件 + README / UI_API / ERROR_CODES 文档更新（新错误码 / publishes 机制已写入）；其余 29 件（含 4 个 DLL 及全部模块 / 模板 / gmsh）与 v2.2 逐字节一致（SHA256 对照）。
"""
text = text.rstrip() + "\n" + SEC8
print("OK   §八 追加")

io.open(DST, "w", encoding="utf-8", newline="\r\n").write(text)
print("WROTE", DST, len(text), "chars")
