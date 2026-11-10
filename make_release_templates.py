# -*- coding: utf-8 -*-
"""Build a shareable .docx: multi-module release management plan + templates."""
from docx import Document
from docx.shared import Pt, Cm
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn

CJK = "微软雅黑"
LATIN = "Microsoft YaHei"


def set_font(run, size=10.5, bold=False):
    run.font.name = LATIN
    run.font.size = Pt(size)
    run.font.bold = bold
    rPr = run._element.get_or_add_rPr()
    rFonts = rPr.get_or_add_rFonts()
    rFonts.set(qn("w:eastAsia"), CJK)
    rFonts.set(qn("w:ascii"), LATIN)
    rFonts.set(qn("w:hAnsi"), LATIN)


doc = Document()

# --- page setup: A4, 2cm margins ---
sec = doc.sections[0]
sec.page_width = Cm(21)
sec.page_height = Cm(29.7)
sec.left_margin = Cm(2)
sec.right_margin = Cm(2)
sec.top_margin = Cm(2)
sec.bottom_margin = Cm(2)

# Normal style font (belt and suspenders; every run also gets set_font)
normal = doc.styles["Normal"]
normal.font.name = LATIN
normal.font.size = Pt(10.5)
rPr = normal.element.get_or_add_rPr()
rFonts = rPr.get_or_add_rFonts()
rFonts.set(qn("w:eastAsia"), CJK)


def add_heading(text, level):
    sizes = {0: 18, 1: 15, 2: 13}
    h = doc.add_heading(level=level)
    run = h.add_run(text)
    set_font(run, size=sizes.get(level, 12), bold=True)
    h.paragraph_format.space_before = Pt(10)
    h.paragraph_format.space_after = Pt(6)
    return h


def add_para(text, bold=False, size=10.5, style=None, align=None, space_after=4):
    p = doc.add_paragraph(style=style)
    run = p.add_run(text)
    set_font(run, size=size, bold=bold)
    if align is not None:
        p.alignment = align
    p.paragraph_format.space_after = Pt(space_after)
    return p


def add_table(headers, rows, widths_cm, font_size=9):
    table = doc.add_table(rows=1, cols=len(headers))
    table.style = "Table Grid"
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.autofit = False
    hdr = table.rows[0].cells
    for i, h in enumerate(headers):
        hdr[i].text = ""
        run = hdr[i].paragraphs[0].add_run(h)
        set_font(run, size=font_size, bold=True)
    for r in rows:
        cells = table.add_row().cells
        for i, val in enumerate(r):
            cells[i].text = ""
            run = cells[i].paragraphs[0].add_run(str(val))
            set_font(run, size=font_size)
    for i, w in enumerate(widths_cm):
        table.columns[i].width = Cm(w)
        for cell in table.columns[i].cells:
            cell.width = Cm(w)
    doc.add_paragraph().paragraph_format.space_after = Pt(2)
    return table


# ===================== Title =====================
t = doc.add_paragraph()
t.alignment = WD_ALIGN_PARAGRAPH.CENTER
run = t.add_run("多模块发版管理方案与模板")
set_font(run, size=20, bold=True)
sub = doc.add_paragraph()
sub.alignment = WD_ALIGN_PARAGRAPH.CENTER
run = sub.add_run("（解决“快的模块发版拖垮慢的模块”问题 · v1.0）")
set_font(run, size=11, bold=False)
doc.add_paragraph()

# ===================== 一、速览 =====================
add_heading("一、速览：先记住 4 件事", 1)
add_para("核心机制（4 件套）", bold=True)
for s in [
    "1. 发布列车：定一个统一的发版窗口，所有模块按同一节奏走，不允许谁先做完谁单独发版。",
    "2. 发版门禁：每次冻结前开 30 分钟“发版影响评审”（评的是影响，不是需求）。",
    "3. 接口契约：接口变更必须走弃用窗口，禁止直接删掉旧接口。",
    "4. 台账 + 数据：声明表、适配清单、发版计划表、数据跟踪表，四张表把适配成本盯住。",
]:
    add_para(s, style="List Bullet")
doc.add_paragraph()
add_para("节奏示例（双周发版）", bold=True)
add_para("T-3 天：各发版模块提交《发版影响声明表》")
add_para("T-2 天：开《发版影响评审》30 分钟，产出《适配清单》")
add_para("T 日：发版窗口（集成构建 → 测试 → 发布）")
add_para("效果：慢模块从声明到发版有完整的准备时间；快模块不能“想发就发”。")

# ===================== 二、发布列车规则 =====================
add_heading("二、发布列车规则（先向全员公告）", 1)
for s in [
    "1. 发版窗口：例如每 2 周一次，周五发版、周三冻结（日期按团队情况调整）。",
    "2. 想本期发版的模块，必须在冻结日前提交声明表；未提交自动顺延到下一期。",
    "3. 需求多的模块：需求分批，只把本期已完成且能通过集成的部分放进去，其余进下一期需求池，不搞“全量硬塞”。",
    "4. 需求少的模块：同样按节奏走，发小版本可以，但也要走声明表 + 评审。",
    "5. 任何包含“直接破坏性变更”（见第五节定义）的发版，默认拒收本期，待弃用窗口满足后再发。",
    "6. 例外（紧急热修）：可绕过节奏，但必须补声明表、书面知会所有受影响模块，并记入《适配清单》跟踪。",
]:
    add_para(s, style="List Bullet")

# ===================== 三、模板一 =====================
add_heading("三、模板一：发版影响声明表（发版模块填写）", 1)
add_para("T-3 天前提交，每个发版一份。", size=10)
decl_rows = [
    ("模块名称", ""),
    ("负责人", ""),
    ("本次版本号", ""),
    ("计划发版周期", ""),
    ("本次功能 / 需求变更（逐条，1~3 句）", ""),
    ("新增 / 变更的接口（列名，写接口名 / 路径）", ""),
    ("是否含破坏性变更（是 / 否）", ""),
    ("破坏性变更明细（旧接口 → 新接口，改了什么；无则填“无”）", ""),
    ("旧接口是否保留（是 / 否；若是，计划何时移除）", ""),
    ("数据格式 / 配置 / 协议变更", ""),
    ("可能受影响的模块（自查，列模块名 + 原因）", ""),
    ("风险与备注", ""),
]
add_table(["项目", "内容"], decl_rows, [8, 9], font_size=9)
add_para("填写说明", bold=True, size=10)
add_para("· “破坏性变更”＝其他模块按旧接口 / 旧格式调用会失效。包括：删除 / 改名接口、改入参或出参、改返回结构、改数据格式 / 协议。", size=10)
add_para("· 拿不准是否破坏性变更时，一律按“有”申报；漏报会在数据跟踪里记为管理事件。", size=10)

# ===================== 四、模板二 =====================
add_heading("四、模板二：发版影响评审会（30 分钟）", 1)
add_para("会议时间：________　　时长：30 分钟", size=10)
add_para("参加人：集成负责人（主持）、各发版模块负责人、受影响模块负责人", size=10)
add_para("会前材料：各《发版影响声明表》（必须会前收齐）", size=10)
add_para("议程", bold=True, size=10)
add_table(
    ["时间", "事项", "产出"],
    [
        ("0–10 分钟", "发版模块逐一宣讲（按声明表，重点讲破坏性变更）", "确认各模块影响范围"),
        ("10–25 分钟", "受影响模块确认适配项、工作量，登记进适配清单", "《适配清单》（见模板三）"),
        ("25–30 分钟", "拍板：哪些模块进本期、谁适配、截止时间；记录风险", "下方结论表"),
    ],
    [3, 9, 5],
    font_size=9,
)
add_para("结论表", bold=True, size=10)
add_table(
    ["发版模块", "版本", "本期是否上（是/否）", "需适配的模块", "负责人", "截止时间", "备注"],
    [
        ("", "", "", "", "", "", ""),
        ("", "", "", "", "", "", ""),
        ("", "", "", "", "", "", ""),
    ],
    [2.6, 2, 3, 3, 1.8, 2, 2.6],
    font_size=9,
)
add_para("风险 / 阻塞项：", size=10)
add_para("", size=10)

# ===================== 五、模板三 =====================
add_heading("五、模板三：适配清单（跟踪到关闭）", 1)
add_para("集成负责人维护，跨发版跟踪。状态：未开始 / 进行中 / 适配完成 / 验证通过 / 关闭。", size=10)
add_para("“关闭”＝适配完成、集成测试通过、且受影响模块无回归。任何超时任务必须在下次发版评审上通报。", size=10)
add_table(
    ["编号", "来源(发版模块+版本)", "受影响模块", "适配内容", "负责人", "截止时间", "状态", "备注"],
    [
        ("1", "", "", "", "", "", "", ""),
        ("2", "", "", "", "", "", "", ""),
        ("3", "", "", "", "", "", "", ""),
    ],
    [1, 3, 2, 4.5, 1.6, 1.9, 1.8, 1.2],
    font_size=9,
)

# ===================== 六、模板四 =====================
add_heading("六、模板四：发布列车计划表", 1)
add_table(
    ["期数", "周期", "冻结日", "发版日", "本期上的模块+版本", "适配任务数", "发版状态", "问题与复盘"],
    [
        ("", "", "", "", "", "", "", ""),
        ("", "", "", "", "", "", "", ""),
        ("", "", "", "", "", "", "", ""),
    ],
    [1.4, 2, 1.8, 1.8, 4.2, 1.8, 2, 3],
    font_size=9,
)

# ===================== 七、模板五 =====================
add_heading("七、模板五：接口变更规则（团队公约）", 1)
for s in [
    "1. 所有对外的模块接口必须有版本号（至少 major.minor）。",
    "2. 禁止直接生效破坏性变更。流程：先标记旧接口弃用（deprecated）、新旧并存至少 1 个完整发版周期；弃用期满后可移除旧接口，移除需评审 + 声明。",
    "3. 新增接口 / 新增参数（向后兼容）可以，但要在声明表里列出来。",
    "4. 发现某发版含直接破坏性变更，默认拒收本期，待弃用窗口满足后再发。",
    "5. 长期目标：慢模块通过“适配层 / 抽象层”对接快模块，隔离直接依赖，让快模块内部变化不波及慢模块。",
    "6. 规则对快慢模块一视同仁：慢模块要改接口，同样要走这套流程。",
]:
    add_para(s, style="List Bullet")

# ===================== 八、数据跟踪 =====================
add_heading("八、数据跟踪（每次发版后花 5 分钟看）", 1)
add_table(
    ["期数", "发版数", "破坏性变更数", "适配任务数", "适配超时数", "适配工作量(人天)", "适配来源 Top1 模块"],
    [
        ("", "", "", "", "", "", ""),
        ("", "", "", "", "", "", ""),
        ("", "", "", "", "", "", ""),
    ],
    [1.8, 2, 2.4, 2.2, 2.2, 3, 3.4],
    font_size=9,
)
add_para("这份数据用来做什么", bold=True, size=10)
for s in [
    "1. 调频率：某模块几乎每期都触发适配，评估是降它的发版频率，还是逼它把接口稳定下来。",
    "2. 治耦合：适配来源 Top1 的模块就是耦合源，优先给它做“适配层”或“接口冻结”。",
    "3. 调节奏：适配工作量过大、一个周期消化不掉，就放慢发版节奏或收紧弃用规则。",
]:
    add_para(s, style="List Bullet")

# ===================== 九、落地路径 =====================
add_heading("九、落地路径（本周就能开始）", 1)
add_table(
    ["时间", "动作"],
    [
        ("第 1 周", "公告发布列车节奏（先定“双周一次”）；把 4 张模板发给各负责人；约定下期开始必须交声明表。"),
        ("第 2 周", "开第一次“发版影响评审”（30 分钟）；建好《适配清单》和《发布列车计划表》。"),
        ("第 1 个月", "跑通至少 2 期；每期记录数据。"),
        ("第 2 个月", "复盘数据，调频率 / 规则；对 Top1 模块定接口稳定化方案。"),
    ],
    [3, 14],
    font_size=9.5,
)
add_para("")
add_para("一句话总结：把“谁先做完谁发、发完再让所有人适配”，换成“统一窗口发版 + 发版前评影响 + 接口变更走弃用窗口”。评审评的是影响，不是需求，所以每次只要 30 分钟，不用开大需求会。", size=10.5)

out = "模块发版管理方案与模板.docx"
doc.save(out)
print("saved:", out)
