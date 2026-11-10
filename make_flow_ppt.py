# -*- coding: utf-8 -*-
"""One-slide PPT template: weekly development process flow."""
from pptx import Presentation
from pptx.util import Inches, Pt
from pptx.dml.color import RGBColor
from pptx.enum.shapes import MSO_SHAPE, MSO_CONNECTOR
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR
from pptx.oxml.ns import qn

FONT = "微软雅黑"
BLUE = RGBColor(0x30, 0x54, 0x96)
LIGHT = RGBColor(0xDC, 0xE6, 0xF1)
GRAY = RGBColor(0x59, 0x59, 0x59)
DARK = RGBColor(0x26, 0x26, 0x26)
RED = RGBColor(0xC0, 0x00, 0x00)
BOX_FILL = RGBColor(0xF2, 0xF2, 0xF2)
BOX_LINE = RGBColor(0xBF, 0xBF, 0xBF)
WHITE = RGBColor(0xFF, 0xFF, 0xFF)


def style_run(run, size, bold=False, color=DARK):
    run.font.size = Pt(size)
    run.font.bold = bold
    run.font.name = FONT
    run.font.color.rgb = color
    rPr = run._r.get_or_add_rPr()
    ea = rPr.find(qn("a:ea"))
    if ea is None:
        ea = rPr.makeelement(qn("a:ea"), {})
        rPr.append(ea)
    ea.set("typeface", FONT)


def fill_text(shape, lines, align=PP_ALIGN.CENTER, anchor=MSO_ANCHOR.MIDDLE):
    tf = shape.text_frame
    tf.word_wrap = True
    tf.vertical_anchor = anchor
    tf.margin_left = tf.margin_right = Inches(0.04)
    tf.margin_top = tf.margin_bottom = Inches(0.03)
    for i, (text, size, bold, color) in enumerate(lines):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.alignment = align
        r = p.add_run()
        r.text = text
        style_run(r, size, bold, color)


prs = Presentation()
prs.slide_width = Inches(13.333)
prs.slide_height = Inches(7.5)
slide = prs.slides.add_slide(prs.slide_layouts[6])

# ---------- 标题 ----------
t = slide.shapes.add_textbox(Inches(0.4), Inches(0.28), Inches(9), Inches(0.6))
fill_text(t, [("开发流程", 26, True, DARK)], align=PP_ALIGN.LEFT)
st = slide.shapes.add_textbox(Inches(0.4), Inches(0.92), Inches(12.5), Inches(0.3))
fill_text(
    st,
    [("周节奏：周二需求收集 → 周内目标 / 评审 / 开发 / 送测 → 周五集成发版（对应《开发需求收集表》，每步产出记入表中）", 11, False, GRAY)],
    align=PP_ALIGN.LEFT,
)

# ---------- 流程 ----------
STEPS = [
    ("需求收集", "周二", "负责人：老师 + 全员", "产出：本周要点"),
    ("设定开发目标", "周内", "负责人：各人", "产出：收集表（个人部分）"),
    ("技术评审", "周四上午", "负责人：集成人主持", "产出：接口变更已定稿"),
    ("开发", "周内", "负责人：各人", "产出：可集成的模块版本"),
    ("送测", "周四", "负责人：各人 → 测试", "产出：测试通过"),
    ("集成", "周五上午", "负责人：集成人", "产出：集成构建"),
    ("发版", "周五下午", "负责人：集成人", "产出：发版清单/版本号"),
]

X0, W, GAP = 0.25, 1.8, 0.04
CY, CH = 1.4, 1.0
BY, BH = 2.6, 1.5

for i, (name, when, who, out) in enumerate(STEPS):
    x = X0 + i * (W + GAP)
    chev = slide.shapes.add_shape(MSO_SHAPE.CHEVRON, Inches(x), Inches(CY), Inches(W), Inches(CH))
    chev.fill.solid()
    chev.fill.fore_color.rgb = BLUE
    chev.line.fill.background()
    chev.shadow.inherit = False
    try:
        chev.adjustments[0] = 0.25  # 收窄箭头，给文字留空间
    except Exception:
        pass
    fill_text(chev, [(name, 14, True, WHITE), (when, 10.5, False, LIGHT)])

    box = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(x), Inches(BY), Inches(W), Inches(BH))
    box.fill.solid()
    box.fill.fore_color.rgb = BOX_FILL
    box.line.color.rgb = BOX_LINE
    box.line.width = Pt(0.75)
    box.shadow.inherit = False
    fill_text(box, [(who, 10, False, GRAY), (out, 10, False, DARK)])


def dashed_line(x1, y1, x2, y2, color=RED, width=1.5):
    c = slide.shapes.add_connector(MSO_CONNECTOR.STRAIGHT, Inches(x1), Inches(y1), Inches(x2), Inches(y2))
    c.line.color.rgb = color
    c.line.width = Pt(width)
    c.shadow.inherit = False
    ln = c.line._get_or_add_ln()
    ln.append(ln.makeelement(qn("a:prstDash"), {"val": "dash"}))
    ln.append(ln.makeelement(qn("a:tailEnd"), {"type": "triangle", "w": "med", "len": "med"}))
    return c


# ---------- 回退箭头 ----------
# 送测失败 -> 开发
dashed_line(8.1, 4.45, 6.75, 4.45)
l1 = slide.shapes.add_textbox(Inches(6.3), Inches(4.18), Inches(2.6), Inches(0.26))
fill_text(l1, [("测试失败 → 修复重提", 10.5, False, RED)])

# 集成失败/需适配 -> 开发
dashed_line(10.2, 4.98, 6.3, 4.98)
l2 = slide.shapes.add_textbox(Inches(6.9), Inches(4.7), Inches(4.9), Inches(0.26))
fill_text(l2, [("集成失败 / 需要适配 → 退回开发（周五没完的滚入下周适配）", 10.5, False, RED)])

# ---------- 底部说明 ----------
note = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(0.4), Inches(5.5), Inches(12.53), Inches(1.1))
note.fill.solid()
note.fill.fore_color.rgb = RGBColor(0xF7, 0xF7, 0xF7)
note.line.color.rgb = BOX_LINE
note.line.width = Pt(0.75)
note.shadow.inherit = False
fill_text(
    note,
    [
        ("周循环：每周二开始新一轮需求收集；未完成的需求、适配、风险全部滚入下周《开发需求收集表》。", 11, False, DARK),
        ("每次发版后，集成人更新“周五集成发版清单”并归档；发版清单可作为版本追溯记录。", 11, False, GRAY),
    ],
)

out = "开发流程图（模板）.pptx"
prs.save(out)
print("saved:", out)
