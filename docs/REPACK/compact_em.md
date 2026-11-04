# compact_em 模块重打包请求日志（REPACK/compact_em）

> 本文件为长期滚动日志：每次重打包请求一轮，最新一轮在最上面。每轮固定三段：**问题清单 → 交付要求 → 回执**（E2E 结果 / 偏差 / 收口状态），回执写回即该轮关闭。
> 关联活契约：`DATA_LAYOUT.md`（v1.4 修订 1）/ `MODULE_MANIFEST_SPEC.md`；包内模块当前版本：**v1.2（2026-11-02，API 2）**。

## 第 1 轮（2026-11-02 请求 → 待回执）

状态：**待回执** —— v1.3 重发包（set_em.json）未交付。关联状态：P0 间歇性 0xC0000005 崩溃 vendor 于 2026-11-03 确认**已修复并合入主线**；平台将在 v1.3 重发版上验证（全链 + WER 干净 + 数值基线 101 行 S 参数）。

### 2026-11-04 会议补充（并入 v1.3 重发要求）

1. **设置默认件归平台管理**（set_em.json）：
   - 默认求解设置文件由平台维护（初值在平台包 templates/Setting/set_em.json，
     平台负责最终内容并传给前端 UI 作默认值）；
   - **包内无需携带 set_em.json**；
   - v1.3 若改动设置格式/字段/默认值，**须在发版说明中注明**，平台据此更新
     templates/ 文件并传给 UI；
   - 依据：DATA_LAYOUT v1.4 修订 1 ①、MODULE_ONBOARDING 修订（2026-11-04）。
2. **移除 plot 阶段 / plotter exe**：
   - 平台 peec_basic 流水线已移除 plot 阶段（2026-11-04）：仅 extract → solve → post；
     S 参数绘图由前端完成（直接读 S_parameter.csv 绘图）；
   - v1.3 重发包**不再提供 PEEC_S_Parameter_Plotter.exe**，同时从 manifest 移除 plot 阶段条目
     （平台 modules.json 的 peec.plotter 路由随 v1.3 集成同步移除）；
   - s2p/SVG 产物不再生成，原「未登记 resultFiles」缺口随之消失（绘图归前端职责）。

### 请求正文（2026-11-02）

致 compact_em 模块团队。平台契约 DATA_LAYOUT 已升 v1.3（2026-11-02）：
Setting/set.json 按链路拆分为 set_em.json（EM 字段）与
set_thermal.json（热对象，thermal 模块消费）。EM 链侧变化如下，
需要重打模块包 + 案例包。改动量小（读文件约定 + manifest + 文档），
数值与 CLI 行为不变。

### 变更点

1. 设置文件改读 Setting/set_em.json（模块内部读取约定变更）：
   - 内容格式不变 = 现行 set.json 的 EM 平铺字段（Solver_SET / MNA_WEIGHT /
     FS / FE / N_FP / DIM / Taylor / SAVE_* / POST_*，schemaVersion 整数 1）；
   - 只读新名：set_em.json 为唯一接受的 JSON 设置名（对齐
     source_thermal.json / source_em.json 先例，旧名 set.json 不再读取，
     不做双名过渡）；
   - set.txt（旧文本格式）维持现状：接受并打弃用 warning，JSON 优先；
   - 输入指纹（stage_receipt / 结果缓存）同步改对新文件的指纹
     （"整个实际选中的设置文件纳入指纹"规则不变，对象改为 set_em.json）。
2. manifest：
   - artifacts.consumed 的 user/solver.set 工件 path：
     Setting/set.json → Setting/set_em.json；
   - capabilities.dataLayoutRevision："1.2" → "1.3"；
   - capabilities.breakingChanges 增一条（设置文件改读 set_em.json，
     set.json 不再消费；CLI 与数值基线不变）。
3. README / ERROR_CODES：设置文件路径与命名更新（set_em.json 缺失/坏格式
   → 既有 QS-FMT-SETTING-INVALID 语义不变）；历史布局说明（SET_EM.json /
   set.txt / port_em.json 等）保留为历史记载。
4. 案例包：standard / two_layer_surface_port 的 Setting/set.json 改名
   set_em.json（内容逐字节不变）；其余输入（source_em.json / 网格 / matdb）
   不变。

### 不变项（验收基线）

- CLI 参数（--data-root / --runtime-dir / --export / --s-columns /
  --post-freq-index / --post-port / --format / --source-plots /
  --field-plots / --parts）与默认行为不变；
- 产物路径与清单不变（model.peec / S_parameter.csv 入 produced；
  s2p / SVG / post 件的 resultFiles 增补不在本次范围，平台另行确认）；
- 数值回归基线：standard 案例 S_parameter.csv 101 行（表头 + 100 频点）、
  四阶段全链通过；two_layer_surface_port 基线同现版——文件仅改名，
  内容未变，数值必须一致；若字节有差异请在交付说明注明原因。

### 交付要求（新包）

1. 新模块包（建议版本 2026.11.03-v1.3 或贵方版本号）：四件套
   （manifest / exe / README / ERROR_CODES），无 examples/templates/DATA；
2. 新案例包 compact_em_cases_<新日期>/：两案例 Setting/ 改为 set_em.json；
3. 验收记录（validation/）按新契约更新；
4. SHA256SUMS.txt 覆盖全部交付文件；
5. 交付说明中注明与 20261102 版（v1.2）的差异清单。

平台侧动作（无需贵方处理）：平台读取由 manifest consumed 驱动，
收到新 manifest 后无需代码改动；新包到位后平台重跑 peec_basic 全链 E2E
并对照回归基线。

