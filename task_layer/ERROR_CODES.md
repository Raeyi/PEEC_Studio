# task_layer 错误码登记表 v0.1

登记日期：2026-10-30。依据《QS-PEEC Studio 错误码与异常处理规范 v0.1》（QS-<域>-<类别>-<错误名> 三段式）。
本文档是任务层（task_layer）的版本化登记，覆盖三类错误码：

1. **RPC 层**（JSON-RPC `error.code`，负整数）——协议级；
2. **阶段层**（通知里的 `errorCode`，`QS-TL-*` 字符串）——任务层自身判定；
3. **计算层处理**——thermal 保留 QS 透传契约（`QS-TH-*`，qs-json-stderr，见 3.3）；**2026-11-12 起 compact_em（v1.4）、gds2geo（1.3.0）、alpsrunner（1.1.2）均 errorContract=none**：不再发出结构化 QS 码，平台固定上报 `QS-TL-MOD-EXEC` + 进程退出码（见第三节归一规则）。ALPS 结果导入码（`QS-TL-ALPS-*`）为任务层自有域（见 2.8），不属计算层透传域。

机器判别一律使用 `code` / `errorCode` 字段，**不要解析文本**。人可读信息分两路：`message_dev`（英文技术描述，开发排查）与 `message_user_zh`（中文用户提示，UI 展示）。

## 一、RPC 层错误码（`error.code`）

来源：`rpc/StdioTransport.cpp`。标准 JSON-RPC 保留码 + 应用扩展码（-32xxx 段）。

| code | 常量 | 触发条件 | UI 建议 |
| --- | --- | --- | --- |
| -32700 | ParseError | stdin 行不是合法 JSON | 属集成缺陷，联系开发 |
| -32600 | InvalidRequest | JSON-RPC 结构不合法（缺 id/method 等） | 属集成缺陷，联系开发 |
| -32601 | MethodNotFound | 未知 method | 检查 API 版本是否匹配 |
| -32602 | InvalidParams | 参数缺失或类型错误 | 展示 `error.message` |
| -32603 | InternalError | 未分类内部错误 | 保留日志，联系开发 |
| -32000 | Cancelled | 任务已取消（`task.cancel` 生效后的收尾响应） | 提示"已取消" |
| -32001 | TaskNotFound | taskId 不存在或已过期清理 | 刷新任务列表 |
| -32002 | TaskAlreadyRunning | 已有任务在运行（当前单任务串行） | 禁用"开始"按钮，等 `task.finished` |
| -32003 | InputInvalid | `inputs.validate` / `task.run` 输入校验失败 | 展示 `error.data.issues[]`（逐条路径+原因） |
| -32004 | ConfigError | modules.json / pipelines.json 加载或 schema 失败（P2 起含加载期 fail-fast：阶段库未知 ref、`ref`/`module` 互斥、schemaVersion >= 3、阶段 params 键不在模块 `allowedArgs` 白名单；P3 起 `pipelines.local.json` 任何错误同样整文件拒载，错误带 `pipelines.local.json:` 前缀；serve 模式下启动即返回本错误并以退出码 2 结束；2026-11-10 起含执行前契约守卫拒绝：rebind 解析的契约路径越出 dataRoot 时 `task.run` / `inputs.validate` 直接拒绝，见 §2.7） | 提示部署不完整，看 stderr |
| -32005 | ModuleExecFailed | 模块执行失败（`module.run` 同步返回时） | 展示 `error.data.errorCode` |

## 二、阶段层错误码（`QS-TL-*`）

来源：`orchestrator/TaskOrchestrator.cpp`、`modules/ModuleRunner.cpp`、`App.cpp`、`io/AlpsImport.cpp`（2026-11-12 起，ALPS 结果导入码，见 2.8）。
出现在 `task.stage.finished` 通知的 `errorCode` 字段，以及 `state.json` 的阶段记录里。
格式：`QS-TL-<域>-<错误名>`，TL = Task Layer。

### 2.1 输入与配置

| errorCode | 触发条件 | 可恢复 | 处置 |
| --- | --- | --- | --- |
| `QS-TL-INPUT-MISSING` | 必需输入文件/目录缺失（`errorMessage` 含具体路径） | 是 | 补齐输入后重跑该阶段 |
| `QS-TL-INPUT-INVALID` | 输入内容静态校验失败（如 source_em.json 语法/字段） | 是 | 按 `errorMessage` 修正输入 |
| `QS-TL-CFG-UNKNOWNMODULE` | 流水线引用了 modules.json 中未注册的模块 | 否（配置错误） | 修正 pipelines.json 或补部署模块 |
| `QS-TL-CFG-DEADLOCK` | 阶段依赖无法解析或成环，任务被强制收尾 | 否（配置错误） | 修正依赖图 |
| `QS-TL-MOD-NOTCONFIGURED` | 模块缺少 manifest/stage 绑定（模块未登记或 manifest 缺 stage） | 否（配置错误） | 补 modules.json 路由与 manifest stage |

### 2.2 IO 与产物

| errorCode | 触发条件 | 可恢复 | 处置 |
| --- | --- | --- | --- |
| `QS-TL-IO-ERROR` | 任务层自身文件读写失败（state.json/task.log 落盘等） | 视系统原因 | 检查磁盘空间与权限 |
| `QS-TL-IO-JOB` | DLL 模块 job JSON 写入失败（worker 任务定义落盘失败） | 视系统原因 | 检查 `Result/tasks/<taskId>/` 可写性 |
| `QS-TL-OUTPUT-MISSING` | 阶段宣告成功但宣告的产出文件缺失 | 是 | 重跑该阶段；若反复出现联系开发 |

### 2.3 模块执行

| errorCode | 触发条件 | 可恢复 | 处置 |
| --- | --- | --- | --- |
| `QS-TL-MOD-NOTFOUND` | 后端可执行文件未找到（部署路径不对） | 是 | 修正部署/PATH |
| `QS-TL-WORKER-NOTFOUND` | DLL 模块的 worker_host.exe 宿主未找到 | 是 | 修正部署 |
| `QS-TL-MOD-EXEC` | 后端进程非 0 退出且未提取到 QS 码（compact_em v1.4 / gds2geo 1.3.0 / alpsrunner 1.1.2 恒为本码，见 3.1/3.2/3.5；thermal 按 3.3 透传） | 视后端 | 结合 `exitCode` 与 `task.stage.log` 定位（alpsrunner 退出码 0/2/3/4/5/124/130 见 3.5） |
| `QS-TL-MOD-TIMEOUT` | 阶段超过 modules.json 配置的 timeout | 是 | 调大超时或优化输入规模 |
| `QS-TL-CANCEL` | 用户取消导致该阶段终止 | — | 无需处置 |

### 2.4 依赖传播

| errorCode | 触发条件 | 可恢复 | 处置 |
| --- | --- | --- | --- |
| `QS-TL-DEP-BLOCKED` | 上游阶段失败，本阶段跳过 | 随上游 | 先修复上游 |
| `QS-TL-DEP-SKIPPED` | 上游阶段被跳过，本阶段连带跳过（2026-11-12 起：**可选**上游跳过/取消不连带阻断，下游按自身 `requires` 判定；ALPS 场景见 3.5） | 随上游 | 同上 |
| `QS-TL-SKIP-NOINPUT` | 可选阶段自身输入缺失，主动跳过（不算失败） | 是 | 需要该阶段时补齐输入 |

### 2.5 执行选择（P1，2026-11-05 起；来源 `App.cpp` / `TaskOrchestrator.cpp`）

`task.run` 的 `selection` 参数（stages 子集 / resume 续跑）引入。假定完成（assumed）是**记账行为不是错误**，但同样以 `errorCode` 显式标记，UI 按 `assumed:true` 字段区分展示。

| errorCode | 触发条件 | 可恢复 | 处置 |
| --- | --- | --- | --- |
| `QS-TL-SEL-UNKNOWNSTAGE` | `selection.stages` 含流水线中不存在的阶段；或 resume 的历史阶段不在当前流水线 | 否（请求错误） | 修正选择/流水线配置 |
| `QS-TL-SEL-DEP-MISSING` | 未选中上游产出在盘缺失（`errorMessage` 含具体路径）。两处触发：① 创建前预检（此时不创建任务）；② 阶段 launch 前复检（必需阶段失败） | 是 | 补齐上游产出（跑上游阶段或换完整算例）后重试 |
| `QS-TL-ASSUMED-DONE` | 假定完成标记：执行选择下未选中的上游阶段、或 resume 信任的历史阶段，不执行，按在盘产出记账（`status=done`、`assumed=true`、`outputs=[]`） | —（非错误） | 无需处置；UI 展示为"假定完成/已跳过" |

### 2.6 用户闸门（P1.5，2026-11-05 起；来源 `TaskOrchestrator.cpp` / `App.cpp`）

流水线阶段声明 `userGate`（`waitForFiles` / `inputs` / `confirm`）后，阶段就绪但闸门未满足 → 置 `WaitingInput`（序列化名 `waiting`，非终态），任务保持活动。等待时间不计入模块超时；UI 写文件/传值/确认后调 `task.provideInput` 唤醒。

| errorCode | 触发条件 | 可恢复 | 处置 |
| --- | --- | --- | --- |
| `QS-TL-INPUT-NOTWAITING` | `task.provideInput` 时任务非活动、阶段不在等待、或未声明闸门 | 是（按提示改操作） | 确认阶段状态（`task.status` 的 `waiting`）后重试 |
| （复用）overrides 白名单错误 | gate `values` 含 manifest `allowedArgs` 之外的键 → 整体拒绝（与 `task.run` overrides 同一校验路径与错误语义） | 是 | 按 `allowedArgs` 修正键名 |
| `QS-TL-CANCEL`（复用） | 等待中 `task.cancel` / UI 断连 → 该阶段 Skipped，任务收尾 | 是 | 需要时重新运行 |

### 2.7 执行前契约守卫（2026-11-10 起；来源 `ModuleRegistry.cpp` / `App.cpp`）

任务启动时 `rebindDataRoot` 从相对模板重新展开各模块的 `outputChecks` / `publishes` 并做 dataRoot
包含性检查（修复"切换工程后产物路径串用"缺陷：旧实现原地展开 + `isAbsolute()` 使契约在切换工程
后冻结于首个 dataRoot）。解析后的契约路径落在当前 dataRoot 之外 → **执行前拒绝**（不创建任务、
不启动阶段、无部分状态），错误经以下三条路径呈现：

| errorCode | 触发条件 | 可恢复 | 处置 |
| --- | --- | --- | --- |
| `QS-TL-PATH-ESCAPE` | 模块的 `outputChecks` 或 `publishes` src/dst 经 rebind 后不落在当前 dataRoot 内（manifest / 流水线 artifacts 路由的相对路径解析越出 dataRoot；合法 manifest 不会触发——produced 受限于 `Result/<id>/` 下、publishTo 受限于 Model/ 或 Setting/ 前缀） | 否（配置错误） | 修正 manifest / 流水线 artifacts 路由；本码不出现在 `task.stage.finished`（拒绝发生在执行前，阶段未执行） |

呈现路径（机器判别用 `QS-TL-PATH-ESCAPE` 字符串；人可读文本含越界路径与 dataRoot）：
- `task.run`：RPC 错误 `-32004`（ErrConfig），`message` 含本码与越界路径；
- `inputs.validate`：`valid:false`，`issues[]` 条目 `reason=QS-TL-PATH-ESCAPE`；
- `run --pipeline` CLI：stdout `config error: ...`，退出码 2。

### 2.8 ALPS 结果导入（2026-11-12 起；来源 `io/AlpsImport.cpp`）

ALPS（alpsrunner 1.1.2）远端仿真 OP0 导入：流水线形态在阶段模块 exit 0 后经阶段库 `op0Import` 触发；独立通道为 JSON-RPC `alps.importOp0` / CLI `alps-import-op0`（同一 `importFromRunDir` 实现）。成功落位 OP0（默认 `Result/thermal/alps/thermal.op0`）+ 同目录审计件 `import_receipt.json`（8 字段，`source` ∈ {pipeline, manual}）；失败报以下码（流水线形态该阶段置 Failed、下游阻断；独立通道 -32005 + `data.errorCode`）。

| errorCode | 触发条件 | 可恢复 | 处置 |
| --- | --- | --- | --- |
| `QS-TL-ALPS-NORUN` | `Result/alpsrunner/simulate/` 下无新 run 目录（前后差集为空） | 是 | 检查 alps 阶段是否实际执行并写出 bundle（console.log / 阶段日志） |
| `QS-TL-ALPS-MULTIRUN` | 前后差集出现 ≥2 个新 run 目录 | 否（状态异常） | 人工清理 `Result/alpsrunner/simulate/` 状态后重试 |
| `QS-TL-ALPS-BADSUMMARY` | `run_summary.json` 缺失或 8 字段门失败（schema_version / run_id / run_id==bundle 目录名 / input_sha256 / status=succeeded / result_scope=full / simulator_exit_code=0 / download_status=verified） | 视远端 | 按 alpsrunner ERROR_CODES 对应 exit 3/4/5 处置 |
| `QS-TL-ALPS-BADSHA` | `SHA256SUMS.txt` 逐条复核失败（bundle 产物 sha 不符） | 否 | 不得覆盖或手工拼接校验失败的结果；保留本机与远端证据 |
| `QS-TL-ALPS-STALE` | I7 三向新鲜度守卫失败：run_summary.input_sha256 ≠ 现 `Model/ThermalNetList.sp` sha 或 `thermal_binding.json.netlistSha256` | 是 | 重跑 extract 发布网表后重跑 alps 阶段（或独立导入通道） |
| `QS-TL-ALPS-IO` | OP0 落位 / 审计件写盘失败（文件系统 IO） | 视系统原因 | 检查磁盘空间与权限 |

## 三、计算层错误码处理规则（透传 + 归一）

任务层**不重新定义**计算层错误码。对带 QS 契约的模块（当前仅 thermal）做**提取 + 透传**；对 errorContract=none 的模块（compact_em v1.4、gds2geo 1.3.0、alpsrunner 1.1.2，2026-11-12 起）做**归一**——固定 `QS-TL-MOD-EXEC` + 原样 `exitCode` + `task.stage.log` 原文（`ModuleRunner.cpp`：none 契约不提取模块码）。ALPS 结果导入码（`QS-TL-ALPS-*`）为本文例外：属任务层自有域，由平台导入步骤发出（见 2.8 / 3.5）。

### 3.1 compact_em（errorContract=none，v1.4 2026-11-10 起）

- v1.4 **不再输出结构化 JSON 错误记录**（无 stderr JSON、无 jsonl）：失败只给人可读文本（英文）+ 进程退出码；任务层固定上报 `QS-TL-MOD-EXEC` + `exitCode`，不做码提取。
- 退出码：Extractor `0` 成功 / `1` 已诊断异常 / `2` 未知异常；Solver `0`/`1`；Post `0`/`1`/`2`（2=未知异常）。
- 各标准阶段写 `stage_receipt.json`（v1，best effort，与产出同目录）：原始输入指纹、参数、退出码、成功产出哈希；输入条目缺失会在回执中登记。
- 严格 schema 校验（v1.4）：DIM/FS/FE 必填（计点模式另需 N_FP）；SAVE_BIN/SAVE_TXT/Taylor/MNA_WEIGHT/SOLVER_MEMORY_MB/SOLVER_THREADS **出现即拒绝**（0 值亦拒绝）；Solver_SET 仅 0；GROUND_PLANE_ENABLED=1 需 GROUND_PLANE_Z_METERS。
- 模型容器 FreeSpace `PEECFS02`/version2（2026.11.10-v1.4），与旧 `PEECPK01` 二进制不兼容，旧算例必须重新提取（无历史文件回退）。
- 运行时依赖：`libiomp5md.dll` 须与 EXE 同目录；`--runtime-dir` 必须位于 dataRoot 内；日志经 `--runtime-dir` 写 `Result/logs/compact_em/`（人可读文本）。
- 人可读诊断清单（英文）与处置见 `compact_em/ERROR_CODES.md`（2026-11-10 版）。v1.2 时代的 QS 码（`QS-FMT-*` / `QS-IO-*` 等，如 `QS-FMT-INPUT-AMBIGUOUS`、`QS-FMT-JSON-SCHEMA_VIOLATION`）已随契约废止不再产生，仅作历史对照。

### 3.2 gds2geo（errorContract=none，现行 1.3.0 / r2，2026-11-12）

- gds2geo 无 QS 码、统一 JSON stderr 或 trace_id 合同；诊断为带程序名的文本 + 进程退出码。现行 1.3.0 为**严格串行**四段 geo_em→geo_thermal→mesh_em→mesh_thermal，em/thermal 两份网格均为必需；产出布局 `Result/gds2geo/{em_geo, thermal_geo, em_msh, thermal_msh}/`（r2 布局，无 `.msh.json` 强制回执），EM 主网格由任务层 publisher 自动落位 `Model/`。
- 因此任务层对 gds2geo 固定上报 `QS-TL-MOD-EXEC` + `exitCode`：`0` = 成功，`2` = 参数/路径/输入初检类，`5` = 几何/材料/网格/发布失败，`6` = 捕获未分类标准异常（GDS2GEO 与 GEO2MSH `export-thermal-entities`；GEOEDIT 不使用 6）。语义以 `gds2geo/ERROR_CODES.md`（1.3.0，2026-11-12）退出码表为准。
- **gmsh 依赖**：要求 **4.11.1 精确匹配**（版本不符硬失败）。平台在子进程环境注入绝对路径 `GDS2GEO_GMSH_EXECUTABLE`（task_layer 经 modules.json 的 `env` 字段注入，只选择 gmsh.exe，**不能解决 DLL 加载**）；gmsh 4.11.1（含 `gmsh-4.11.dll`）位于平台共享工具区 `tools/gmsh/`，宿主启动子进程前必须将该目录绝对路径**前置到子进程 PATH**。
- **锁机制**：Windows 命名互斥量，不产生 `.lock` 文件；不得与旧版文件锁程序混用同一算例（新旧 gds2geo 不得共用同一 dataRoot）。
- 详细诊断由各程序经 `--runtime-dir <dataRoot>/Result/logs/gds2geo/` 写入（宿主逐次留存）；任务层的 `task.log`/`state.json` 在 `Result/tasks/<taskId>/`。
- 辅助工具 geoedit / mshcheck（0.23.0）/ mshfix（1.5.0）随 r3 交付，**平台默认不接入流水线**（仅以辅助命令暴露）；mshfix 当前不支持带 `$GDS2GEOUnits` 单位声明或 Tet4 体网格输入（exit 3，不删除声明绕过）。《物理模块错误码清单.xlsx》的 G2G-001~166 建议码仍未接入程序，属规划项。

### 3.3 thermal（有 QS 契约，QS-TH-*）

- thermal 1.0.1 起（含现行 1.0.9，2026.11.12，apiVersion 8）的 `errorContract` 为 `qs-json-stderr`：失败时在 stderr 输出结构化 JSON（字段 `code`（`QS-TH-*`）、`severity`、`message` 等）。1.0.9 码表重构：exit 5 = `QS-TH-EXEC`（Windows SHA256 服务失败）、exit 6 = `QS-TH-RUN-LOCKED`（pipeline_locked）、新增 `QS-TH-CFG-PATH` / `QS-TH-CFG-BINDING`（exit 2）、`QS-TH-NUM-MAPPING`（exit 4）；`QS-TH-NUM-OP0` 移除（映射缺温改为就近填补，见下）。
- 任务层按与 compact_em 相同的机制从输出尾部提取最后一条 QS 码作为 `errorCode` 原样上报（实测码：`QS-TH-IO-FILE`（exit=3 文件缺失/不可读）、`QS-TH-NUM-COVERAGE`（exit=4 材料覆盖 gap/overlap）等，完整表见 `thermal/ERROR_CODES.md`）。
- extract 阶段必需件：热网格（经流水线 stage `artifacts` 路由，默认 `Result/gds2geo/thermal_msh/model_thermal.msh`）、`Model/matdb.json`（gds_full em 阶段 publisher 落位）、`Model/boundary_thermal.json`（边界条件，必需，thermal 1.0.5 起；**1.0.7 起支持 schemaVersion 2**（default adiabatic + boundaries：fixed_temperature（surface_ids 为任意现有 Elementary Surface，含内部面）/ convection（h_W_m2K>0 + ambient_temperature_K），未指定表面绝热；重复表面或共享节点冲突固定温度报 exit 2）**与原 schemaVersion 1**；1.0.1 时代内嵌于 source_thermal.json）、求解设置 `Setting/set_thermal.json`（SPICE 仿真设置，thermal 1.0.5 起）、`Model/source_thermal.json`（UI 维护；**thermal 1.0.7 起支持简化 schemaVersion 3 与原 schemaVersion 2**：3 = 仅 `sources`（volume_ids 上 total_power，允许空数组；体材料按 mesh Physical 标签 + matdb.json 自动装配，带材料标签的表面按 matdb 厚度装配面内导热与热容 film，无需 `thermal` 提取对象）；2 = meshFile/meshSha256 绑定实际网格 + `thermal` 提取对象（体域/薄层/材料关联，1.0.6 起引入）；原独立文件 `Result/thermal/thermal_extract_control.json` 已取消，旧参数 `--settings` 传入报 exit=2；v1.2 时代 `Setting/set.json` 含 `thermal` 对象的合并件不再读取）；与 boundary_thermal.json 按成对版本解析（source 2 配 boundary 1、3 配 2；错配 → 模块 exit 2 `QS-TH-FMT-INPUT`）；1.0.1 只读新名，旧名 port_thermal.json 不再接受）。
- post 阶段的 OP0：**thermal 1.0.9 起标准位置为 `Result/thermal/alps/thermal.op0`**（1.0.9 argsTemplate 内置 `--op0` 读本位置，显式可覆盖，**无旧路径回退**）；task_layer 按 pipelines.json 的 stage `requires` 字段在启动前做存在性检查，缺失则整段以 `QS-TL-SKIP-NOINPUT` 跳过（不阻断 extract 产物）；alps 阶段缺位时可用预置工作点（离线/回归形态）。工作点由平台导入步骤落位：alps 阶段 exit 0 → run bundle 前后差集 + run_summary 8 字段门 + SHA256SUMS 复核 + I7 三向新鲜度守卫 → OP0 落位 + 同目录 `import_receipt.json`（`QS-TL-ALPS-*`，见 2.8 / 3.5）；独立通道 `alps.importOp0` / CLI `alps-import-op0` 等价。沿革：1.0.6–1.0.8 标准位置为 `Model/thermal.op0`（程序默认读 source_thermal.json 所在目录，显式 `--op0` 优先，无旧路径回退）；1.0.5 时代为 `Result/thermal/post/thermal.op0`（1.0.6 起废止）；1.0.1 时代的 `Model/thermal.op0` + `Model/thermal_run.json` 已废止（1.0.5 取消 thermal_run.json 与 --result-binding；1.0.6 起不做 OP0 与网表的绑定校验，post 不读取/校验 .sp；post 不检查网表来源，1.0.7 起、1.0.9 延续）。
- 算法范围：**1.0.7 起**支持任意表面固定温度（含内部面）、对流边界（h_W_m2K>0 + ambient_temperature_K）、体域总功率（按体积均匀分布，`sources` 空数组 = 零功率源）、mesh Physical 材料标签 + matdb 自动装配（film 厚度非正报 exit 2）；退化材料三角形报 exit 4 `QS-TH-NUM-GEOMETRY`；1.0.6 时代形态（三层堆叠模型：中部线性四面体 + 平行 XY 薄层，薄层 `material_footprints` 必须完整铺满 `reference_surfaces`（无 gap/overlap，否则 exit=4）；uniform 功率 + 全局底面定温 + 其余绝热）完整保留（demo1_4.1 直跑与 1.0.6 逐字节一致，见 REPACK/thermal.md 第 3 轮）。**1.0.8 起** OP0 映射温度缺失 → post 用最近完整已知温度网格节点就近替代（CSV `value_source` 列 + 报告标记；OP0 不修改；无可用/重复/无效仍报 exit 2 `QS-TH-FMT-INPUT`；1.0.7 严格校验、不自动补全为旧版行为）；**1.0.9 起**两阶段读取 `Setting/set_thermal.json` 的可选 `mesh_length_unit`（m/mm/um），另支持可选薄膜覆写件 `Model/thinfilm_thermal.json`（缺位 = 不覆写）。

### 3.4 兜底

任何后端非 0 退出且提取不到 QS 码时，一律 `QS-TL-MOD-EXEC` + `exitCode` + 原始输出（`task.stage.log`），不做文本猜测。

### 3.5 alpsrunner（errorContract=none，现行 1.1.2，2026-11-12 集成）

- alpsrunner 1.1.2（`tools/alpsrunner/` 完整包；单 stage `simulate`；apiVersion 1）经 SSH 做 ALPS 网表远端仿真与结果回传；客户端 EXE 在平台侧，远端主机需 ALPSRunnerHost 1.1.0 + 公钥授权 + IC618（VMware Tools + ALPS license）（`environment.alpsrunner_host`，bundled=false）；runtimePolicy：输出与 runtime 目录同一 dataRoot 下同一实际卷，不得有 junction/符号链接。
- 退出码 0/2/3/4/5/124/130（vendor `ERROR_CODES.md` 逐字摘要）：`0` 成功；`2` 参数/输入/路径/暂存预检（此类提交前错误可能没有结果任务目录）；`3` 凭据/主机指纹/网络/SSH/VMware Tools（连接断开 ≠ ALPS 已停，先查 `simulation_status` 与远端目录）；`4` 仿真失败或远端收集失败（`result_scope=logs_only` 仅诊断回传）；`5` 下载/解压/校验/发布失败（部分文件不能当成功）；`124` 时限；`130` 用户取消（尽量回传已有日志）。调用方必须同读退出状态 + stdout + stderr；`simulation_status=unknown` ≠ 失败。
- 模块不自行写 `Model/`、`Setting/` 正式件（artifacts.produced=[]，publicationPolicy 原文如此）；产物为 `Result/alpsrunner/simulate/<runId>\` run bundle（console.log / run_summary.json / SHA256SUMS.txt / <netlist>.sp / <netlist>.sp.sim\{.op0, .dcop0, .ic0, .log, .warn, .thread}）。
- **OP0 落位是平台责任**（MODULE_MANIFEST_SPEC §6.7）：双通道导入（阶段库 `op0Import` + 独立通道 `alps.importOp0` / CLI `alps-import-op0`），失败 6 个 `QS-TL-ALPS-*` 码（见 2.8）；成功落位 `Result/thermal/alps/thermal.op0` + 同目录审计件 `import_receipt.json`（D1a：task_layer 写 / thermal post 读，1.0.9 post 内置 `--op0` 读本位置，无旧路径回退）。2026-11-12 实测远端 TCP 可达（192.168.136.172:22）；真远端全链路留统一 v2.5.0_rc1 交付轮（3600 s 预算）。详见 REPACK/alpsrunner.md。

## 四、与规范文档的映射

| 规范域 | 任务层现状 |
| --- | --- |
| QS-UI-* | 不归任务层管（前端自判，不发请求） |
| QS-IO-* / QS-FMT-* / QS-VAL-* / QS-MESH-* / QS-MAT-* / QS-BC-* / QS-SRC-* / QS-SOLV-* / QS-POST-* / QS-COMM-* / QS-INTERNAL-* | （历史行）compact_em v1.2 时代产生并透传；**2026-11-12 起 compact_em v1.4、gds2geo 1.3.0、alpsrunner 1.1.2 均 errorContract=none，不再产生这些码**，对应失败表现为人可读文本 + 退出码（见 3.1/3.2/3.5） |
| QS-TH-* | 由 thermal 产生并透传（qs-json-stderr，见 3.3） |
| QS-TL-* | 任务层自有域，本表第二节为唯一登记处 |

新增 `QS-TL-*` 码的流程：先在 `TaskOrchestrator.cpp` / `ModuleRunner.cpp` / `App.cpp` / `io/AlpsImport.cpp`（2026-11-12 起，ALPS 导入码）（或注册表层）落地实际抛出点，再回填本表（含触发条件、可恢复性、处置），最后同步 `UI_API.md` §6.2（独立导入通道 `alps.importOp0` / CLI `alps-import-op0` 共用同一组抛出点）。禁止只改文档不改代码。

## 五、边界说明

- 本表只登记**当前源码实际会发出**的码；规划中的码（如 gds2geo G2G 系列）不在此列。v1.2 时代 compact_em 的 QS 码在 3.1 中仅作历史对照列出，不属于当前可发出集合。
- RPC 层 -32xxx 与阶段层 QS-TL 是两层概念：RPC 码描述"这次调用本身"的结果，QS-TL 码描述"某个阶段"的结果；一次 `task.run` 可能先收到若干 `task.stage.*` 通知，最终 RPC 响应才给出整体成败。
- 取消（`task.cancel`）后，进行中的阶段以 `QS-TL-CANCEL` 收尾，后续未启动阶段以 `QS-TL-DEP-BLOCKED`/`QS-TL-DEP-SKIPPED` 标记，整体 RPC 响应为 -32000（2026-11-12 起：**可选**上游跳过/取消不连带阻断，下游按自身 `requires` 判定；非可选上游 Skipped/Failed 仍照常阻断）。
