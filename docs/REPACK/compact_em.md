# compact_em 模块重打包请求日志（REPACK/compact_em）

> 本文件为长期滚动日志：每次重打包请求一轮，最新一轮在最上面。每轮固定三段：**问题清单 → 交付要求 → 回执**（E2E 结果 / 偏差 / 收口状态），回执写回即该轮关闭。
> 关联活契约：`DATA_LAYOUT.md`（v1.6）/ `MODULE_MANIFEST_SPEC.md`；包内模块当前版本：**v1.4.3（2026-11-13 交付，API 7，2026-11-13 集成，revisionId docs16-contract-fix-20261113，releaseBatch 2026-11-13）**。

## 第 3 轮（2026-11-13，compact_em 1.4.3 集成，已收口）

状态：**已收口** —— v1.4.3（2026-11-13）交付并集成（平台 peec_basic 全流 E2E + R2_PEC_Standard 100 频点 + 红线回归全绿）。v1.4（2026.11.10-v1.4，API 4，2026-11-12 集成，revisionId merged-post-20261111）被 v1.4.3（apiVersion 7，revisionId docs16-contract-fix-20261113，releaseBatch 2026-11-13）取代。

**交付接收（vendor 包 Compact_EM_Delivery_Bundle_20261113_1.4.3_release1.zip）**：模块 6 件 SHA256 全对、三 EXE 与 vendor 映像逐字节一致；libiomp5md.dll 1,614,184 B 不变（运行依赖）。

| 文件 | 尺寸（B） |
| --- | --- |
| PEEC_Model_Extractor.exe | 9,680,384 |
| PEEC_Solver.exe | 16,871,424 |
| PEEC_Post_Processing.exe | 838,144 |
| module.manifest.json | 8,332 |
| README.md | 8,432 |
| ERROR_CODES.md | 18,088 |

**契约变化（API7 / breakingChanges `1.4.3 / docs16-contract-fix-20261113`）**：
- 公开 flat 设置 **schemaVersion 2 only**（schemaVersion 1 拒绝）；未知可选键忽略，已知不支持键拒绝（SAVE_BIN/SAVE_TXT/Taylor/MNA_WEIGHT/SOLVER_MEMORY_MB/SOLVER_THREADS/accuracy/workers/mklThreads）。
- 新增 `GROUND_PLANE_ENABLED`（=1 → 需 `GROUND_PLANE_Z_METERS`（SI m）+ Solver_SET=0 + POST_FIELD_ER=1）；默认 `SAVE_SP`=0、`POST_FIELD`=0、`POST_FIELD_ER`=1。
- `Swap_Log=1` → 需 N_FP + PRECISION=0 + Solver_SET=0；`PRECISION>1` → 需 N_FP。
- **移除隐式 Touchstone 写**（s2p 不再自动生成）。
- 旧文件名（set.txt/set.json/SET_EM.json/port.json）→ `QS-FMT-INPUT-LEGACY_NAME_REJECTED`。
- **新增 runLock**：三阶段写业务前取 `.locks/compact_em.run.lock`（busy → `QS-COMM-MODULE-BUSY`/exit 1，stale 经 OS 字节锁复用，scope 覆盖全阶段）。
- extract/solve/post 的 CLI 与产物**不变**（exe 名/argsTemplate/produces/consumes 与 v1.4 一致）；solve 产 S/Y/Z 参数 CSV；errorContract 仍 none。

**平台侧集成（2026-11-13，分支 feature/rc2-compact-em-20261113，基线 f20aa88）**：
- 模块目录替换（6 件，与 vendor 字节一致）；`templates/Setting/set_em.json` 升级 schemaVersion 2（450 B，新增 GROUND_PLANE_ENABLED=0 / POST_FIELD_ER=1）；`task_layer/config/modules.json` `_comment` 更新 1.4.3/apiVersion 7（仅注释，路由不变）；`task_layer.exe` 未重建（1,120,622 B `9f1e8ae5…be76` 不变）。
- **R2_PEC_Standard 100 频点 E2E（全绿）**：extract→solve→post 全 done；model.peec 19,419,844 B（PEECFS02/version2）；S_parameter.csv 12,282 B / Y 7,434 B / Z 7,434 B（各 100 数据行，1.09–10 GHz）；Source.msh 358,047 B（14 视图；POST_FIELD=0 无 Field.msh）。
- **数值一致性（红线，已实测）**：1.4.3 与 v1.4 基线（同输入 mesh/source/matdb 三件 SHA 一致）**S/Y/Z 参数 CSV 与 post/Source.msh 逐字节一致**（S `a7c4b93e…`/Y `8a250814…`/Z `32861db1…`/Source.msh `abe39dd9…`）⇒ 物理行为不变；model.peec（同尺寸 19,419,844 B）、Solver_Diagnostics.csv、post_source.json 随 EXE 版本序列化变化（非物理量，回归不对其做字节比对）。
- **收口**：1.4.3 为 stage-compatible 升级（无 C++/流水线改动，task_layer.exe 哈希不变）；公开设置转 schemaVersion 2，平台模板已同步。本轮收口。


## 第 2 轮（2026-11-09 请求 → 2026-11-12 已收口）

状态：**已收口** —— v1.4 已交付并集成（平台 6 腿复测全绿 + r2 全维网格全流 E2E 通过）。v1.3（2026.11.09-v1.3，API 3，2026-11-09 集成）已被 v1.4（2026.11.10-v1.4，API 4，2026-11-12 集成，revisionId merged-post-20261111）取代（2026-11-09 平台集成 gds2geo V1.2 r2 时 API-4 探针发现；分支 `feature/vendor-r3-20261112`，未合 main）。

**问题清单（2026-11-09 探针证据，E:/code/_integ/peec_api4b）**：
- compact_em v1.3（2026.11.04）对 r2 em MSH（vendor 多行记录方言，Delivery API 4：dimensions 0–3 含 Point1/Line2/Tri3/Tet4）**解析正常**（608 点 / 1277 元素 / 2 材料 / vacuum id 4 全部解析成功），但标准 MATDB/source 输入契约拒绝网格中的**激励线元素**：`QS-FMT-MSH-INVALID` "Standard MATDB/source_em.json input must not contain excitation line elements"（exit 1）。
- r2 MSH 的 manifest 已声明（capabilities.mesh）：CAD Point1/Line2 **不是激励定义**（ungroupedEntities："CAD Point1/Line2 are not excitation definitions"；consumerRequirement："do not treat CAD Line2 as ports"）——即网格线元素为原生 CAD 几何，与现契约"禁止激励线元素"冲突 ⇒ 契约需 compact_em 侧对齐（或 r2 提供无线 MSH 选项，两者取一即可）。
- 影响面：**GDS→PEEC 生成网格全流阻塞**（peec_basic 的标准输入必须消费 Model/model_em.msh 正式件，该正式件现为 r2 全维 MSH）；预烘焙网格算例（红线算例 standard，经典 gmsh 4.1 单记录行格式）不受影响（S_parameter.csv 22,118 B 字节一致 PASS）。

**交付要求**：
1. （阻塞项）标准 MATDB/source 输入接受 r2 全维 MSH（Delivery API 4）：将 CAD Line2 视为几何（端口/激励仅由 source_em.json 的 ports 定义，不扫描网格线元素），或提供无线 MSH 输入模式；二选一。
2. （与 gds2geo 第 3 轮联动）mesh 侧选项或消费端适配二选一，由 vendor 单方定口径，避免双 vendor 互相等待。
3. 若改输入契约：发版说明注明 apiVersion / breakingChanges（现 apiVersion 3）。

**回执（2026-11-12，已收口）**：
- 平台侧执行阻塞已排除（2026-11-12）：SD 标记问题（vendor 映像文件对象携带 DSH 工作区写入 ACE `S-1-4-805033334-89673629`，`.locks` 创建拒绝访问，根因与过程见 REPACK/gds2geo.md 第 3 轮回执）经同字节新文件对象原位替换修复，compact_em 三 EXE 写入与执行全部恢复。v1.4 新映像不携带该 SD。
- v1.3 时代复测（2026-11-12 上午，v1.4 落位前）：r2 生成网格复测（peec_api4b 四输入件与 11-09 探针 SHA256 逐字节一致）复现 11-09 同一结论 —— 网格解析正常（608 点 / 1277 元素 / 2 材料 / MATDB schema 3 接受），标准 MATDB/源契约拒绝激励线元素 `QS-FMT-MSH-INVALID`（exit 1）⇒ 当时交付要求 1（阻塞项）仍待 vendor 交付；红线回归（pswitch / baseline / p1 21/21 / p3 / gate）全 GREEN。
- **v1.4 交付接收（2026-11-12 落位：2026.11.10-v1.4，apiVersion 4，PEECFS02/version2，revisionId merged-post-20261111；正式交付树含 RELEASE_NOTES / validation / cases / contracts / SHA256SUMS.txt）**：vendor README 声明 `Model/model_em.msh` 为 Gmsh MSH 2.2/4.1（ASCII/二进制）、一阶 Tri3 导体、**Point1/Line2 仅作几何元数据，不产生支路或激励** ⇒ **交付要求 1（阻塞项）vendor 声明侧满足**（选项 A：CAD Line2 视为几何；端口/激励仅由 source_em.json ports 定义）；交付要求 3 满足（apiVersion 3→4，manifest breakingChanges 明示 PEECPK01/schema1 → PEECFS02/version2，旧模型需重提取）。
- **平台 v1.4 集成复测（2026-11-12，6 腿全绿；分支 `feature/vendor-r3-20261112`，基线 ae88dc0，未合 main）**：gate 22/22、p1 21/21、p3 ✓、pswitch ✓、baseline ✓（thermal 基线 344.1037 K + gds_full r2 四段 + Model/ 落位 66,834 B / 878 B）、p2 ✓1（恰为 3 个已记录预期物化 FAIL）。
- **r2 全维网格 → peec_basic 全流 E2E（交付要求 1 平台侧闭环，2026-11-12）**：红线算例 `dev/cases/compact_em/standard` 的 Model 三件（mesh 69,662 B `f8cad342…` / source 290 B `7ad37075…` / matdb 878 B `48cd560e…`）与 11-09 探针 peec_api4b 四输入件 **SHA256 逐字节一致**（同含 Point1/Line2 的 r2 全维网格）—— 即 v1.4 红线算例本身就是 r2 全维网格算例（vendor R2_PEC_Standard 参考算例，树内件件 SHA 一致）。p1 std 与 gate 全链（extract→solve→post）在 v1.4 下全 exit 0：model.peec 19,419,844 B（PEECFS02/version2，100 点提取实测 280 ms）、S_parameter.csv 12,282 B / Y 7,434 B / Z 7,434 B / Solver_Diagnostics.csv 36,903 B、Source.msh 358,047 B（14 视图）—— `QS-FMT-MSH-INVALID` 不再出现，**GDS→PEEC 生成网格全流解除阻塞**。
- 实测产物与契约（v1.4，standard 算例）：post `Source.msh` 358,047 B（14 视图；receipt-only，不入平台 resultFiles，UI 经 stage 回执或算例目录读取）；`Field.msh`（E/H real/imag/mag 6 视图）仅空间场采样开启时生成（需可选 `Model/Space_Mesh.msh` + POST_FIELD_ER=1；红线算例未启用 → 未生成）。`--s-columns` 已移除（列固定 RI）；新增运行依赖 `libiomp5md.dll`（EXE 同目录）；`--runtime-dir` 须位于 dataRoot 内。
- 契约变化：**errorContract=none** —— 失败不输出结构化 stderr JSON / jsonl，平台按固定 `QS-TL-MOD-EXEC` + 进程退出码上报（extract 0/1/2、solve 0/1、post 0/1/2；task_layer/ERROR_CODES.md §3.1 已同步）；set_em.json 严格 schema（DIM/FS/FE 必填，计数模式另需 N_FP，SAVE_BIN/SAVE_TXT/Taylor/MNA_WEIGHT/SOLVER_MEMORY_MB/SOLVER_THREADS 出现即拒绝、零值同拒，Solver_SET 仅 0，设置件按字节指纹）；模型容器 PEECPK01 二进制不兼容（旧算例需重提取）。
- 树内适配：standard = vendor R2_PEC_Standard 5 件（SHA 逐件一致）；two_layer_surface_port 与 templates 的 set 文件去除 6 个禁止键（迁 v1.4 严格 schema）；modules.json `_comment` 更新。
- **收口说明**：交付要求 1–3 均满足（1 = vendor 声明 + 平台全流 E2E 通过；2 = 口径由 compact_em vendor 单方定案 “Line2 为几何”，gds2geo 保持 r2 全维 MSH 输出，无双 vendor 互等，联动 gds2geo.md 第 3 轮收口；3 = release notes 已记录）；vendor 正式发版包已交付，平台 6 腿复测与全流 E2E 全绿。**本轮收口**。
---

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

