# task_layer 错误码登记表 v0.1

登记日期：2026-10-30。依据《QS-PEEC Studio 错误码与异常处理规范 v0.1》（QS-<域>-<类别>-<错误名> 三段式）。
本文档是任务层（task_layer）的版本化登记，覆盖三类错误码：

1. **RPC 层**（JSON-RPC `error.code`，负整数）——协议级；
2. **阶段层**（通知里的 `errorCode`，`QS-TL-*` 字符串）——任务层自身判定；
3. **计算层透传**（`QS-FMT-* / QS-SOLV-* / ...`）——后端模块原样透传，不在本表重复定义。

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
| -32004 | ConfigError | modules.json / pipelines.json 加载或 schema 失败 | 提示部署不完整，看 stderr |
| -32005 | ModuleExecFailed | 模块执行失败（`module.run` 同步返回时） | 展示 `error.data.errorCode` |

## 二、阶段层错误码（`QS-TL-*`）

来源：`orchestrator/TaskOrchestrator.cpp`、`modules/ModuleRunner.cpp`、`App.cpp`。
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
| `QS-TL-MOD-EXEC` | 后端进程非 0 退出且未提取到 QS 码（见第三节透传规则） | 视后端 | 结合 `exitCode` 与 `task.stage.log` 定位 |
| `QS-TL-MOD-TIMEOUT` | 阶段超过 modules.json 配置的 timeout | 是 | 调大超时或优化输入规模 |
| `QS-TL-CANCEL` | 用户取消导致该阶段终止 | — | 无需处置 |

### 2.4 依赖传播

| errorCode | 触发条件 | 可恢复 | 处置 |
| --- | --- | --- | --- |
| `QS-TL-DEP-BLOCKED` | 上游阶段失败，本阶段跳过 | 随上游 | 先修复上游 |
| `QS-TL-DEP-SKIPPED` | 上游阶段被跳过，本阶段连带跳过 | 随上游 | 同上 |
| `QS-TL-SKIP-NOINPUT` | 可选阶段自身输入缺失，主动跳过（不算失败） | 是 | 需要该阶段时补齐输入 |

## 三、计算层错误码透传规则

任务层**不重新定义**计算层错误码，只做两件事：提取 + 透传。

### 3.1 compact_em（有 QS 契约）

- compact_em 失败时在 stderr 输出结构化 JSON 错误记录（UTF-8），字段含 `code`（`QS-*`）、`severity`、`message_dev`、`message_user_zh`、`user_action`、`trace_id`、`recoverable` 等；同一记录同时追加到 `Result/logs/compact_em/<exe>-<trace_id>.jsonl`（DATA_LAYOUT 规范位置；2026.10.30 版起写规范位置，早期 2026.10.22 版写旧 `Result/Logs/`）。
- 任务层在阶段非 0 退出时，从输出尾部正则提取最后一条 `"code":"QS-[A-Za-z0-9_-]+"`，作为 `task.stage.finished` 的 `errorCode` 原样上报。
- 完整错误对象由 compact_em 自己负责写全，任务层**不重组、不改写字段**；UI 若要展示中文提示，从 `task.stage.log` 对应 JSON 行解析，或直接读 jsonl。
- 退出码：四程序 `0` 成功、`1` 失败；Extractor 另有 `2` = 未处理异常。原生崩溃（如 `0xC0000005`，无 QS JSON）走 3.4 兜底：`QS-TL-MOD-EXEC` + 原始 `exitCode`。
- 完整码表见 `compact_em/ERROR_CODES.md`（本地增量 + 规范码边界）。

### 3.2 gds2geo（无 QS 契约，1102 版起）

- gds2geo 没有 QS 编码、统一 JSON stderr 或 trace_id 合同；诊断为带程序名的文本 + 进程退出码。1102 版为**严格串行** source→prepared→em→thermal 四阶段，em/thermal 两份网格均为必需；热网格保留在 `Result/gds2geo/thermal/`（不进 `Model/`，保持 I1 单主网格不变式）。
- 因此任务层对 gds2geo 固定上报 `QS-TL-MOD-EXEC` + `exitCode`：`0` = 成功，`2` = CLI/输入/配置类，`5` = 执行类，`6` = 内部错误（仅 source 阶段）。语义以 `gds2geo/README.md` 的退出码表为准（1102 起交付不再单独提供 ERROR_CODES.md）。
- **gmsh 依赖**：1102 版不随包分发 gmsh，要求 **4.11.1 精确匹配**（版本不符硬失败）。平台须在子进程环境注入绝对路径 `GDS2GEO_GMSH_EXECUTABLE`；task_layer 经 modules.json 的 `env` 字段完成注入（当前指向 `{MODULE_ROOT}/gds2geo/gmsh.exe`，后续 gmsh 上移到项目根目录时只改这一处配置）。
- **锁机制**：1102 版改用 Windows 命名互斥量，**不再产生 `.lock` 文件**；不得与旧版文件锁程序混用同一算例（同一 dataRoot 上 1102 与旧版 gds2geo 不得共用）。
- 详细诊断由各程序经 `--runtime-dir` 写到 `Result/logs/gds2geo/`（每程序一份 `.log` + source 的 attempts 记录）；任务层的 `task.log`/`state.json` 在 `Result/tasks/<taskId>/`。
- 《物理模块错误码清单.xlsx》中的 G2G-001~166 建议码**尚未接入 gds2geo 程序**，属规划项；接入前任务层不会看到这些码。

### 3.3 thermal（有 QS 契约，QS-TH-*）

- thermal 1.0.1（20261102 docs_1.2 重发版）的 `errorContract` 为 `qs-json-stderr`：失败时在 stderr 输出结构化 JSON（字段 `code`（`QS-TH-*`）、`severity`、`message` 等）。
- 任务层按与 compact_em 相同的机制从输出尾部提取最后一条 QS 码作为 `errorCode` 原样上报（实测码：`QS-TH-IO-FILE`（exit=3 文件缺失/不可读）、`QS-TH-NUM-COVERAGE`（exit=4 材料覆盖 gap/overlap）等，完整表见 `thermal/ERROR_CODES.md`）。
- extract 阶段必需件：热网格（经流水线 stage `artifacts` 路由，默认 `Result/gds2geo/thermal/model_thermal.msh`）、`Model/matdb.json`（gds_full em 阶段 publisher 落位）、`Setting/set.json`（含 `thermal` 对象）、`Model/source_thermal.json`（UI 维护，meshFile/meshSha256 绑定实际网格；1.0.1 只读新名，旧名 port_thermal.json 不再接受）。
- post 阶段的 `Model/thermal.op0` 与 `Model/thermal_run.json` 是**显式 CLI 输入而非 manifest 公共前置**；task_layer 按 pipelines.json 的 stage `requires` 字段在启动前做存在性检查，缺失则整段以 `QS-TL-SKIP-NOINPUT` 跳过（不阻断 extract 产物）。
- 算法范围：仅三层堆叠模型（中部线性四面体 + 平行 XY 薄层），薄层 `material_footprints` 必须完整铺满其 `reference_surfaces`（无 gap/overlap，否则 exit=4）；uniform 功率 + 全局底面定温 + 其余绝热。

### 3.4 兜底

任何后端非 0 退出且提取不到 QS 码时，一律 `QS-TL-MOD-EXEC` + `exitCode` + 原始输出（`task.stage.log`），不做文本猜测。

## 四、与规范文档的映射

| 规范域 | 任务层现状 |
| --- | --- |
| QS-UI-* | 不归任务层管（前端自判，不发请求） |
| QS-IO-* / QS-FMT-* / QS-VAL-* / QS-MESH-* / QS-MAT-* / QS-BC-* / QS-SRC-* / QS-SOLV-* / QS-POST-* / QS-COMM-* / QS-INTERNAL-* | 由 compact_em 产生并透传；gds2geo 规划中 |
| QS-TH-* | 由 thermal 产生并透传（qs-json-stderr，见 3.3） |
| QS-TL-* | 任务层自有域，本表第二节为唯一登记处 |

新增 `QS-TL-*` 码的流程：先在 `TaskOrchestrator.cpp` / `ModuleRunner.cpp` 落地实际抛出点，再回填本表（含触发条件、可恢复性、处置），最后同步 `UI_API.md` §6.2。禁止只改文档不改代码。

## 五、边界说明

- 本表只登记**当前源码实际会发出**的码；规划中的码（如 gds2geo G2G 系列）不在此列。
- RPC 层 -32xxx 与阶段层 QS-TL 是两层概念：RPC 码描述"这次调用本身"的结果，QS-TL 码描述"某个阶段"的结果；一次 `task.run` 可能先收到若干 `task.stage.*` 通知，最终 RPC 响应才给出整体成败。
- 取消（`task.cancel`）后，进行中的阶段以 `QS-TL-CANCEL` 收尾，后续未启动阶段以 `QS-TL-DEP-BLOCKED`/`QS-TL-DEP-SKIPPED` 标记，整体 RPC 响应为 -32000。
