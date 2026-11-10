# -*- coding: utf-8 -*-
"""Plain fill-in weekly requirements collection form (.docx)."""
from docx import Document
from docx.shared import Pt, Cm
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_ROW_HEIGHT_RULE
from docx.oxml.ns import qn

CJK = "宋体"
LATIN = "Times New Roman"
USABLE = 15.9  # cm, A4 with 2.54cm margins


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
sec = doc.sections[0]
sec.page_width = Cm(21)
sec.page_height = Cm(29.7)
sec.top_margin = Cm(2.54)
sec.bottom_margin = Cm(2.54)
sec.left_margin = Cm(2.54)
sec.right_margin = Cm(2.54)

normal = doc.styles["Normal"]
normal.font.name = LATIN
normal.font.size = Pt(10.5)
rPr = normal.element.get_or_add_rPr()
rPr.get_or_add_rFonts().set(qn("w:eastAsia"), CJK)


def para(text="", size=10.5, bold=False, align=None, space_before=0, space_after=4):
    p = doc.add_paragraph()
    if text:
        set_font(p.add_run(text), size=size, bold=bold)
    if align is not None:
        p.alignment = align
    p.paragraph_format.space_before = Pt(space_before)
    p.paragraph_format.space_after = Pt(space_after)
    return p


def _set_widths(table, widths):
    table.autofit = False
    for i, w in enumerate(widths):
        table.columns[i].width = Cm(w)
        for cell in table.columns[i].cells:
            cell.width = Cm(w)


def box(n_rows, row_h=0.9, labels=None):
    """One-column fill-in box (plain form field)."""
    table = doc.add_table(rows=n_rows, cols=1)
    table.style = "Table Grid"
    for idx, row in enumerate(table.rows):
        row.height = Cm(row_h)
        row.height_rule = WD_ROW_HEIGHT_RULE.AT_LEAST
        cell = row.cells[0]
        cell.width = Cm(USABLE)
        if labels and labels[idx]:
            set_font(cell.paragraphs[0].add_run(labels[idx]))
    _set_widths(table, [USABLE])
    para("", space_after=2)


def grid(headers, n_blank, widths, row_h=0.8):
    """Header row + blank rows."""
    table = doc.add_table(rows=1 + n_blank, cols=len(headers))
    table.style = "Table Grid"
    for i, h in enumerate(headers):
        p = table.rows[0].cells[i].paragraphs[0]
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        set_font(p.add_run(h), bold=True)
    for r in range(1, 1 + n_blank):
        table.rows[r].height = Cm(row_h)
        table.rows[r].height_rule = WD_ROW_HEIGHT_RULE.AT_LEAST
    _set_widths(table, widths)
    para("", space_after=2)


# ---------------- 标题 ----------------
para("开发需求收集表", size=14, bold=True, align=WD_ALIGN_PARAGRAPH.CENTER, space_after=6)
para("第____周（____月____日 ～ ____月____日）", align=WD_ALIGN_PARAGRAPH.CENTER, space_after=8)

# ---------------- 填写说明 ----------------
para("填写说明：", bold=True)
para("1. ②③④ 必须周四中午前填完；①⑤ 周内随时更新。")
para("2. 被别人用到的接口 / 格式 / 协议，多小的改动都要写进 ②；周五没完成的适配，直接滚进下周的 ③。")
para("3. 第二部分每人一节，按需复制。")

# ---------------- 一、本周要点 ----------------
para("一、本周要点（集成人填，周二会后写）", size=12, bold=True, space_before=8)
box(3, 0.9)

# ---------------- 二、每人一节 ----------------
para("二、模块：________　　负责人：________", size=12, bold=True, space_before=8)
para("本周开发目标：")
box(1, 0.9)

para("1. 本周需求清单", bold=True, space_before=4)
grid(
    ["需求内容", "来源", "工作量(人天)", "完成标准", "周五发版(上/进行中/顺延)"],
    4,
    [4.5, 2.5, 2.0, 4.0, 2.9],
)

para("2. 新增 / 变更接口（无则填“无”）", bold=True, space_before=4)
grid(
    ["接口（名 / 路径）", "类型（新增 / 变更 / 删除）", "影响哪些模块"],
    3,
    [7.0, 4.5, 4.4],
)

para("3. 我要做的适配（无则填“无”）", bold=True, space_before=4)
grid(
    ["适配谁", "适配什么（接口 / 格式 / 协议）", "完成时间"],
    3,
    [3.5, 8.5, 3.9],
)

para("4. 模块间协作（无则填“无”）", bold=True, space_before=4)
grid(
    ["协作方(模块/人)", "协作点(接口/数据/流程)", "我提供什么", "我需要他提供什么", "对完时间"],
    3,
    [3.0, 3.2, 3.4, 3.4, 2.9],
)

para("5. 依赖和风险", bold=True, space_before=4)
box(2, 1.1, labels=["等待：", "风险："])

# ---------------- 三、发版清单 ----------------
para("三、周五集成发版清单（集成人周四晚填）", size=12, bold=True, space_before=8)
grid(
    ["模块", "版本", "内容", "需要的适配(从③抄)"],
    4,
    [3.0, 2.5, 5.5, 4.9],
)

out = "开发需求收集表（模板）.docx"
doc.save(out)
print("saved:", out)
