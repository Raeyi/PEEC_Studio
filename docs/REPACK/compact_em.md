# compact_em 模块重打包请求日志（REPACK/compact_em）

> 本文件为长期滚动日志：每次重打包请求一轮，最新一轮在最上面。每轮固定三段：**问题清单 → 交付要求 → 回执**（E2E 结果 / 偏差 / 收口状态），回执写回即该轮关闭。
> 关联活契约：`DATA_LAYOUT.md`（v1.5 修订 2）/ `MODULE_MANIFEST_SPEC.md`；包内模块当前版本：**v1.3（2026-11-04，API 3，2026-11-05 集成）**。

## 第 1 轮（2026-11-02 请求 → 2026-11-05 回执，已收口）

状态：**已收口** —— v1.3 重发包 2026-11-05 交付并集成（平台交付包 v2.0 / PEEC_Studio_20261105）。P0 间歇性 0xC0000005：vendor 声明已修复（vendor 稳定性证据：两案例各 20 轮共 120 次阶段调用全部成功 + 运行窗口 WER 查询干净）；**平台复核：11-02 崩溃模型（v12_crash1）于 v1.3 上全链完成（21.4s，20 频点全扫），崩溃未复现；平台运行窗口 WER 干净**。

### 回执（2026-11-05）

**交付**：`Compact_EM_Delivery_20261104_v1.3_docs1.zip`（9,284,983 B，2026-11-04 18:37）：`compact_em/` 六文件（三 exe + manifest + README + ERROR_CODES，SHA256SUMS 全量校验通过）+ `compact_em_cases_20261104/`（standard、two_layer_surface_port）+ `technical/` + `validation/` + RELEASE_NOTES.md + SHA256SUMS.txt。

**请求落实核对**：

| 请求 | 结果 |
|---|---|
| set_em.json 唯一 JSON 设置（manifest consumed = `Setting/set_em.json`，schemaVersion 1；set.json 不再读取、无双名过渡；set.txt 弃用 warning、JSON 优先、坏 JSON 不回退） | ✅ 按请求实施（manifest + README + ERROR_CODES 一致） |
| manifest：dataLayoutRevision "1.3"、breakingChanges 增条、consumed 路径更新 | ✅ 已实施（另 apiVersion 3；平台 task_layer（11-02 17:28 构建）实测接受该 manifest，注册/全链正常） |
| 移除 plot 阶段 / PEEC_S_Parameter_Plotter.exe / Touchstone 导出 | ✅ 交付无 plotter；stages 仅 extract/solve/post；`touchstone` 非受支持导出名（显式旧导出报 QS-FMT-SETTING-INVALID）；s2p/SVG 不再生成 |
| 发版说明注明设置格式/默认值变化（11-04 会议补充 ①） | ✅ RELEASE_NOTES.md：字段/默认值/单位/解析精度不变，新增 `GROUND_PLANE_ENABLED`/`GROUND_PLANE_Z_METERS`（无限 PEC 参考地，默认关闭）；默认设置由平台 templates/ 管理（安装包不附设置件） |
| 案例包两案例 Setting 改名 set_em.json | ✅ standard 内容逐字节不变（与平台 11-02 基线案例 set.json 同字节，已核实）；two_layer_surface_port 同现版 |
| SHA256SUMS.txt 覆盖全部交付文件 | ✅ 已覆盖（模块六文件逐一核验通过） |

**新增能力（超出第 1 轮请求，发版说明已注明）**：无限 PEC 参考地（镜像源，默认关闭）、实数缩减求解、辅助关联稀疏化、内存准入与有界积分复用、低频完整电荷精度回退、独立电流激励 Z 计算；S CSV `--s-columns full` 可选输出模块计算的 dB/相位列（平台流水线默认 RI 列）。数值影响见下方平台 E2E。

**平台 E2E（2026-11-05，task_layer run peec_basic，3 阶段全链）**：

| 案例 | 结果 |
|---|---|
| standard N_FP=101 | done 2.7s，101 数据行 |
| standard N_FP=100（= 基线案例） | done 2.5s，101 行（表头 + 100 频点）；**与 11-02 v1.2 基线 maxRel 7.1e-12（数值一致）** |
| two_layer_surface_port（表面端口） | done 0.36s（平台固定名落位后） |
| v12_crash1（P0 回归，N_FP=20） | done 21.4s，20 频点全扫完成（v1.2 时代 0xC0000005 未复现） |

- WER：运行窗口内无三程序相关 1000/1001 条目（干净）；
- solve 目录无 Touchstone/SVG（artifacts.json + Post_Source.txt + stage_receipt.json + S_parameter.csv）；
- 歧义检查生效：Model/ 双网格候选被 QS-FMT-INPUT-AMBIGUOUS 拒收（平台落位行为一致）。

**偏差/备注**：① manifest dataLayoutRevision 声明 "1.3"（EM 条款出处），平台契约运行于 v1.5 修订 2，无冲突（EM 侧布局自 v1.3 起未变）；② two_layer_surface_port 独立案例网格名为 model2.msh，平台归档时按固定名 model_em.msh 落位（同字节，meshSha256 不变，source_em.json meshFile 已更新）；③ 设置文件整体纳入 artifacts/receipt 指纹（vendor 行为），内容变化后旧上游结果不可复用——与平台缓存规则一致。

**平台侧动作（2026-11-05 完成）**：模块目录替换（六文件）；modules.json 移除 peec.plotter 路由；templates/ 删除 set.json（set_em.json 为现行默认值）；UI_API / ERROR_CODES / DATA_LAYOUT（**v1.5 修订 2**）更新；dev/cases 归档更新（standard = v1.3 案例 + 新增 two_layer_surface_port）；交付包升 **v2.0（PEEC_Studio_20261105）**。

**收口**：第 1 轮关闭（2026-11-05）。compact_em 无未决 REPACK 请求。

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

