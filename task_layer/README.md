# task_layer — QS-PEEC Studio 任务层

任务层是 UI（交互层）与计算层之间的"平台主 EXE"，承担架构图标注的四项职责：**输入检查、任务编排、模块路由、状态与结果管理**。计算层模块（现在为 Compact PEEC 4 个 exe，目标形态为功能 DLL）由任务层按流水线定义调用，各阶段可并行、可可选跳过。

> **给 UI/前端的对接文档见 [UI_API.md](UI_API.md)**（进程启动方式、完整 API、通知、错误码、Qt 客户端骨架）。本文件是任务层的设计与维护说明。

## 一、分层定位（对应架构图）

```
┌──────────────────────────────────────────────────┐
│ 交互层  UI（Qt）     项目/导入 物理设置 仿真任务 优化 结果 │
│        │  JSON-RPC 2.0 over stdio（QProcess 启动）  │
├──────────────────────────────────────────────────┤
│ 任务层  task_layer.exe  ← 本模块                     │
│        输入检查 · 任务编排 · 模块路由 · 状态与结果管理   │
│        │  按 pipeline 定义调度（依赖 DAG + 最大并行）    │
├──────────────────────────────────────────────────┤
│ 计算层  功能模块（exe / DLL worker）                   │
│        物理模型解析 | 等效电路(PEEC) | 仿真求解 | 优化 | 后处理 │
│        external：直接启动 exe（迁移期）                │
│        dll：worker_host.exe 隔离加载（目标形态）       │
├──────────────────────────────────────────────────┤
│ 数据层  geometry.geo / materials.json / simulation.json │
│        网格与结果映射（DATA 目录约定）                 │
└──────────────────────────────────────────────────┘
```

## 二、与 UI 的交互协议（JSON-RPC 2.0 over stdio）

UI 用 `QProcess::start("task_layer.exe", {"serve", ...})` 启动任务层，
双方以 **stdout 一行一个 JSON 消息**通信（UTF-8）。

### 请求（UI → 任务层）

| method | params | 返回 |
|---|---|---|
| `ping` | — | `{pong, app, schemaVersion}` |
| `modules.list` | — | `{modules:[{id,displayName,kind,command,available,resolvedPath}]}`（`available` 为可用性探测结果） |
| `pipelines.list` | — | `{pipelines:[{id,displayName,description,maxParallel,stages:[{id,module,name,optional,dependsOn, ref?,requires?,params?,userGate?,artifacts?}]}]}`（P2 起 v2 配置阶段带 `ref` 物化溯源；可选字段非空才出现） |
| `inputs.validate` | `{dataRoot, pipeline?, module?, inputs?}` | `{valid, checked, issues:[{path,reason,detail}], warnings, inputs?}` |
| `task.run` | `{pipeline, dataRoot, taskId?, inputs?, selection?, overrides?}` | `{taskId, pipeline}` |
| `task.provideInput` | `{taskId, stageId, values?}` | `{taskId, stageId, accepted}`（用户闸门等待中的阶段提供输入；`QS-TL-INPUT-NOTWAITING` 非等待时拒绝） |
| `module.run` | `{moduleId, dataRoot, taskId?, inputs?, stageRef?}` | `{taskId, pipeline}`（P3：接线自动路由；多候选需 stageRef 消歧） |
| `task.cancel` | `{taskId}` | `{cancelled}` |
| `task.status` | `{taskId}` | state.json 内容（各阶段状态） |
| `task.result` | `{taskId}` | state + `resultFiles` |
| `shutdown` | — | `{bye}` 后进程退出 |

错误响应：`{jsonrpc:"2.0", id, error:{code,message,data}}`；
错误码：-32000 取消 / -32001 任务不存在 / -32002 任务运行中 /
-32003 输入非法 / -32004 配置错误 / -32005 模块执行失败。
阶段层 `QS-TL-*` 错误码及后端透传规则见 [ERROR_CODES.md](ERROR_CODES.md)。

#### `inputs`：UI 动态输入（重点：port.json 由前端配置下发）

UI 侧由用户动态指定的文件（典型是端口定义 `port.json`）通过 `inputs` 下发，不要塞进 `overrides`：

```json
{"jsonrpc":"2.0","id":3,"method":"task.run","params":{
  "pipeline":"gds_to_peec",
  "dataRoot":"D:/cases/demo/DATA",
  "inputs":{"portJson":"D:/cases/demo/config/port.json"}
}}
```

规则：

- 键名驼峰转大写下划线作为路径变量：`portJson` → `{PORT_JSON}`；值可含固定变量（如 `{DATA}/Model/port.json`），会被二次展开；
- `inputs` 会写入 `state.json` 的 `inputs` 字段，并原样进 DLL 模块的 `job.json`（`job.inputs`）；
- 提供了 `{PORT_JSON}` 时，入口契约里名为 `port.json` 的必需项不再判缺失（改为 warning）——把它落到 `{Model}/` 是交接阶段的职责；
- `inputs.validate` 会额外校验该文件存在、JSON 合法、`meshFile` 存在且 `meshSha256` 与主网格一致。

### 单模块执行（module.run）

UI 可以脱离流水线单独启动一个模块（如只重建网格）。实现方式是构造一条
**只含该模块的"单阶段流水线"**（`single:<moduleId>`）交给编排器，
因此进度通知、状态持久化、`task.status` / `task.result` / `task.cancel` 与流水线完全一致，
UI 不需要两套处理逻辑。

`inputs.validate` 传 `module` 时按该模块的契约做输入检查。

### 通知（任务层 → UI，实时状态）

| method | params |
|---|---|
| `task.started` | `{taskId, pipeline, ts}` |
| `task.stage.started` | `{taskId, stageId, name}` |
| `task.stage.log` | `{taskId, stageId, line}` |
| `task.stage.finished` | `{taskId, stageId, status, exitCode, errorCode, errorMessage, outputs[]}` |
| `task.finished` | `{taskId, status, summary, elapsedMs}` |
| `task.cancelled` | `{taskId}` |

### Qt 侧 UI 最小示例

```cpp
// UI 进程内
auto* proc = new QProcess(this);
proc->start("task_layer.exe", {"serve"});
connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]{
    while (proc->canReadLine()) {
        const QJsonDocument d = QJsonDocument::fromJson(proc->readLine());
        // 判断是响应（有 id）还是通知（有 method）
    }
});
// 发请求
proc->write(QJsonDocument(QJsonObject{
    {"jsonrpc","2.0"},{"id",1},{"method","task.run"},
    {"params", QJsonObject{{"pipeline","peec_basic"},{"dataRoot", dataRoot}}
}}).toJson(QJsonDocument::Compact) + '\n');
```

## 三、编排文件

### config/modules.json — 模块注册表

每个模块声明：`kind`（external/dll）、`command`、`searchDirs`、`args`、`workingDir`、
`inputContract.required/optional`（执行前校验）、`outputCheck.files`（执行后校验）、
`layerRead/layerWrite`（Working Model 层契约，供审计与未来一致性检查）。

- `searchDirs`：按顺序查找 `command`，找不到再退到 `{BIN}`。**支持多候选**，后端包改名/搬迁时不要改代码：
  `["{MODULE_ROOT}/compact_em", "{MODULE_ROOT}/electromagnetic", "{MODULE_ROOT}/Compact_pipeline"]`。
- `workingDir`：子进程工作目录，默认 `{DATA}`。按“**EXE 所在目录**”解析相对路径的后端（如 gds2geo）必须显式写
  `"workingDir": "{MODULE_ROOT}/gds2geo"`，否则其设置 JSON 会指向错误位置。
  （gds2geo 无 `--data-root` 选项，须用 `--gds/--tech/--output-dir` 等显式绝对路径调用。）

路径变量：`{DATA} {Model} {Setting} {Result} {BIN} {MODULE_ROOT} {CASE_ROOT} {TASK_DIR}`，
以及 UI 通过 `inputs` 注入的动态变量（如 `{PORT_JSON}`）。

`{MODULE_ROOT}` 自动推导：从 config 目录 / exe 目录逐级向上，找到**含任一后端运行包**
（`Compact_pipeline` / `electromagnetic` / `gds2geo` / `pipeline`）的那一层；
也可用 `--module-root` 或环境变量 `TASK_LAYER_MODULE_ROOT` 显式指定。
`{BIN}` 默认 `<task_layer.exe 目录>/bin`；若该目录不存在而 exe 旁侧有 `worker_host.exe`
（单目录布局），则 `{BIN}` 直接取 exe 目录。

### config/pipelines.json — 流水线定义

```json
{ "id": "peec_basic", "maxParallel": 2, "stages": [
    { "id": "extract", "module": "peec.model_extract", "name": "...", "dependsOn": [] },
    { "id": "solve",   "module": "peec.solver",        "name": "...", "dependsOn": ["extract"] }
]}
```

- `dependsOn` 声明依赖，形成 DAG；就绪阶段按 `maxParallel` 并行启动；
- `optional: true`：输入缺失自动跳过（Skipped），不阻断主链（对应旁路：网格融合/DPEC/后处理等；S 参数绘图由前端完成，平台自 2026-11-04 起不再跑 plot 阶段）。
- **注意**：`peec_basic` 的 `maxParallel` 为 **1**。Compact 用 `DATA/.peec-data.lock` 独占句柄限制并发写，
  同一 DATA 上并行跑后处理会互相锁失败（plot 阶段已移除）；并行能力留给未来不共享 DATA 的 DLL 模块。

## 四、命令行

```powershell
task_layer.exe serve --config .\config --bin .\bin      # stdio RPC 服务（UI 用）
task_layer.exe run --pipeline peec_basic --data-root D:\Cases\demo   # 单次执行
task_layer.exe modules / pipelines                        # 打印注册表
```

## 五、DLL 模块接入（目标形态）

1. 模块负责人按 `src/modules/module_api.h` 实现 DLL，导出
   `qs_peec_module_entry(job_json_path, data_root, log_path)`；
2. DLL 放 `bin/modules/<moduleId>/`；
3. modules.json 中登记 `kind: "dll"` + `workerHost`；
4. 任务层为每个 DLL 阶段启动独立 `worker_host.exe` 进程加载执行（崩溃隔离、内存隔离）。

迁移期：现有 Compact 4 个 exe 以 `kind: "external"` 直接调用，无需改动。

## 六、状态与结果

- 每任务在 `DATA/Result/tasks/<taskId>/` 下生成 `state.json`（任务状态 + 各阶段记录）+ `task.log`；
- 阶段状态：pending → running → done / failed / skipped / notConfigured；
- 机器错误码格式 `QS-TL-<域>-<错误名>`（如 QS-TL-INPUT-MISSING），与《错误码与异常处理规范》对齐；
  **模块自身错误码会原样透传**（如 `QS-FMT-SETTING-UNKNOWN_KEY`、`QS-SOLV-MEMORY-OUT_OF_MEMORY`），UI 按 `errorCode` 分支，不要解析文本；
- 依赖失败/被跳过的下游阶段统一置 `skipped`，用 `errorCode` 区分原因：
  - `QS-TL-DEP-BLOCKED`：上游 failed；
  - `QS-TL-DEP-SKIPPED`：上游被跳过（可选阶段输入缺失等）；
  - `QS-TL-CFG-DEADLOCK`：`dependsOn` 无法解析（加载期已校验 DAG，正常不会出现）；
  - `QS-TL-SKIP-NOINPUT`：可选阶段自身输入缺失；
  - `QS-TL-CANCEL` / `QS-TL-MOD-TIMEOUT`：用户取消 / 超时；
- 任务结束后 `task.status` / `task.result` **仍可查询**（保留最近一次任务），UI 不必在运行中抢答；
- 任务结束后 `task.finished` 通知携带 elapsedMs，UI 可据此展示耗时。

## 七、构建

### 7.1 产线（MSVC，与 UI 同工具链）

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
cmake --build build --config Release
```

依赖：Qt6 Core（仅 Core，不依赖 Widgets）、MSVC v143 / CMake 3.16+。
worker_host 在 Windows 上使用 LoadLibrary/SetDllDirectoryW 保证 DLL 路径解析。

### 7.2 本地快速构建（免管理员，Qt6 MinGW）

无 MSVC 的机器可用 Qt 官方 MinGW 版 Qt + MinGW 13.1（均装在用户目录，不动系统）：

```powershell
python -m pip install aqtinstall
# Qt6 Core（只取 qtbase，体积小）
python -m aqt install-qt windows desktop 6.8.3 win64_mingw --archives qtbase -O C:\Qt
# MinGW 13.1 工具链（官方源慢时可从 USTC 镜像直接下 7z 后用 tar 解压到 C:\Qt\Tools）
python -m aqt install-tool windows desktop tools_mingw1310 -O C:\Qt

# 一键构建（有 cmake 走 cmake，没有则 moc+g++ 直编）
powershell -ExecutionPolicy Bypass -File .\build_local.ps1 -Clean
```

产物在 `build\`：`task_layer.exe`、`worker_host.exe`、`config\`、`Qt6Core.dll` 及 MinGW 运行库（已 `-static` 链接 GCC 运行时）。
脚本做了两处必要的规避：中文工程目录下全部用**相对路径**调用 moc/g++，脚本文件保存为 **UTF-8 BOM**（PowerShell 5.1 才能正确解析中文）。

## 八、本地测试运行

目录布局（**模块根 = 后端运行包目录的父目录**）：

```text
PEEC_Stdio\
├── compact_em\        # 计算层：PEEC 四 exe + DATA\ 样例
├── gds2geo\           # 计算层：建模/网格四 exe + DATA\
└── task_layer\        # 本工程（任务层）源码
    ├── config\        # modules.json / pipelines.json（构建时拷到 exe 旁）
    └── build\         # task_layer.exe、bin\worker_host.exe、config\
```

### 8.1 不编译也能先验证 PEEC 链路（Compact 四 exe 是编译好的二进制）

```powershell
$dataRoot = 'C:\...\Compact_pipeline_20261023\Compact_pipeline\DATA'
& '...\Compact_pipeline\PEEC_Model_Extractor.exe'   --data-root $dataRoot
& '...\Compact_pipeline\PEEC_Solver.exe'            --data-root $dataRoot
```

成功退出码为 0；DATA\Result 下出现 model.peec / extract.state / S_parameter.csv / solve.state 即链路通。
样例 DATA 的 `.peec-data.lock` 是零字节持久锁，无需删除。

> 注意：`Compact_pipeline\DATA` 的大样例有 10916 个 PEC 面，提取约 33–165 s、`model.peec` 约 2.9 GB；
> `N_FP=100` 时求解会因内存预算不足报 `QS-SOLV-MEMORY-OUT_OF_MEMORY`（32 GB 机器）。冒烟请用 8.2 的小算例。

### 8.2 生成小算例并跑通三段流水线

```powershell
# 1) 生成小算例（平行板 + 两端端口，16~32 个三角形；自动算网格 SHA-256）
python .\tools\make_mini_case.py --out ..\demo\mini_strip

# 2) 注册表自检
.\build\task_layer.exe modules
.\build\task_layer.exe pipelines

# 3) 输入检查（含 port.json 与网格 SHA-256 绑定校验）
.\build\task_layer.exe validate --data-root ..\demo\mini_strip\DATA --pipeline peec_basic

# 4) 跑编排流水线
.\build\task_layer.exe run --pipeline peec_basic --data-root ..\demo\mini_strip\DATA
```

预期：终端按 extract→solve→post 顺序打印事件，最后 `task.finished {"status":"done"}`；
`DATA\Result\tasks\<taskId>\` 下生成 `state.json` + `task.log`；
S 参数 CSV 在 `Result\compact_em\solve\S_parameter.csv`，源/场后处理件在 `Result\compact_em\post\`（plot 阶段已移除，SVG 绘图由前端完成）。

exe 放在 `build\` 下运行时，模块根会自动向上找到含后端运行包（`compact_em` / `gds2geo` /
`electromagnetic` / `Compact_pipeline`）的目录；
也可显式指定：`--module-root <父目录>` 或环境变量 `TASK_LAYER_MODULE_ROOT`。

### 8.3 UI 联调（serve 模式）

```powershell
.\build\task_layer.exe serve
```

UI 以 QProcess 启动并走 JSON-RPC（见第二节协议表），`inputs.validate` 可先于 `task.run` 做输入检查。
无 UI 时可用冒烟脚本验证整套协议（8 项断言：ping / 列表 / 校验 / 运行 / 通知 / 状态 / 结果）：

```powershell
python .\tools\rpc_smoke.py --exe .\build\task_layer.exe --data-root ..\demo\mini_strip\DATA
```

## 九、扩展点（下一步）

- 阶段级参数透传：`task.run` 的 `overrides.{stageId}.{key}` 会追加到模块参数（建议加 allowedArgs 白名单校验）；
- 错误码域扩展：`QS-TL-*` 已预留，模块内部错误由模块自身定义 `QS-<模块域>-*`；
- Working Model 层一致性检查：注册表 `layerRead/layerWrite` 已就位，可在调度前做"上游写 ⊇ 下游读"校验并记录层指纹；
- 任务队列：当前 `m_busy` 保证同一时刻只跑一条流水线，做"优化循环/批量扫描"时需要队列 + 并发池。

## 十、变更记录

- 2026-10-27：修复 Qt6 编译阻断（`QTextStream` 传输层重写、`QMap` 类型）、任务悬挂、pump 重入、取消后无法再启动、结果查询、DLL 分支、错误码透传等 17 项问题；新增 DAG 校验、输入哈希校验、小算例生成器与 RPC 冒烟脚本。详见 `CODE_REVIEW_20261027.md`。
- 2026-10-28：修复路径定位——① 模块根推导改为"向上找含后端运行包（`compact_em`/`electromagnetic`/`gds2geo`/`Compact_pipeline`）的目录"；② `searchDirs` 支持多候选，电磁计算层目录更名 `electromagnetic` → `compact_em` 后配置同步（**`pipelines.json` 只引用模块 id、不含路径，无需改动**）；
  ③ `{BIN}` 与 `worker_host` 布局对齐（产物落 `<exe目录>/bin`）并补齐回退查找；④ 新增模块级 `workingDir`（默认 `{DATA}`），供按 EXE 目录解析相对路径的后端使用；⑤ 修复 `optional` 输入未做变量替换导致的假警告。新增 UI 动态输入通道 `inputs`（`portJson` → `{PORT_JSON}`，随 `job.json`/`state.json` 下发与留痕），`inputs.validate` 会校验该文件与网格的 SHA-256 绑定。新增前端对接文档 `UI_API.md`。
- 2026-10-28：`phys.model_parse` 由 DLL 占位改为真实的 GDS2GEO 源模型阶段（external），
  并按"一个 exe 一个模块"的粒度新登记 `gds.prepare` / `gds.mesh_em` / `gds.mesh_thermal`
  （均显式 `workingDir: {MODULE_ROOT}/gds2geo`：gds2geo 按 EXE 目录解析相对路径且无 `--data-root`）；
  新增流水线 `gds_full`。新增 **`module.run`**（单模块独立执行，内部复用"单阶段流水线"），
  `modules.list` 增加 `available` / `resolvedPath`，`inputs.validate` 支持 `module`。
  修复 `build_local.ps1` 拷贝 config 时目标已存在会嵌套成 `build\config\config\` 的问题。
- 2026-10-28：新增内建阶段（`kind: "internal"`）：`ModuleRunner::launch` 此前会把 internal 当 external
  去找不存在的 exe，现单独分支处理；首个内建动作为 `gds2em_handoff`（`Result/em/` → `Model/` +
  `Setting/`，改写 port.json 的 meshFile/meshSha256，按需生成 set.txt）。
  `em_full` 据此串成真实全链路：gds 三段 → handoff → peec 提取/求解 → 绘图/后处理。
- 2026-10-28：按实际业务流程改为**三段式**（中间要插 UI 选端口交互，无法一条流水线跑到底），
  故**移除 `em_full`**，把交接阶段并入 `gds_full` 末尾（`gds_full` 现在产出的
  `Model/` + `Setting/` 可直接被 `peec_basic` 使用）。交接阶段相应调整：
  ① `port.json` 改为**可选**（建模阶段 UI 还没选端口，缺失只告警不失败，由 `peec_basic`
  的输入检查兜底）；② 新增**网格指纹失效检测**——比对 port.json 记录的 `meshSha256`
  与本次生成的网格，不一致时告警 `triangleIds` 可能失效、提示重新选端口，并刷新绑定；
  ③ 交接时清理 `Model/` 下的旧网格（EM 要求有且仅有一份 `*.msh`，否则报
  `QS-FMT-INPUT-AMBIGUOUS`）。实测确认 EM 会校验 `meshSha256` 非空且与实际网格一致
  （留空报 `QS-FMT-JSON-SCHEMA_VIOLATION`，不符报 `QS-IO-FILE-CORRUPTED`），
  故 UI 选完端口后应再跑一次 `module.run internal.handoff` 刷新绑定。
  修复：改写 port.json 前未关闭读句柄，导致 Windows 下覆写静默失败（只出告警、文件没变）。
- 2026-10-28：新增打包脚本 `tools\package.ps1`（产出 `dist\`，对齐 compact_em 的"平铺目录、拷走即用"规则；
  因任务层链接 QtCore，需额外带 4 个 DLL）。修复 `modules.list`：internal 模块此前被报成
  `kind=external` 且 `available=false`（按文件探测，而内建动作没有实体文件），
  会导致 UI 把"刷新端口绑定"的 `internal.handoff` 当成不可用而过滤掉。
- 2026-11-05（P1 执行选择，分支 `feature/pipeline-optimization`）：`task.run` 新增可选
  `selection` 参数（UI 驱动；CLI `run` 仍仅全链）：
  ① `{"mode":"stages","stages":[...]}` 子集运行——未选中上游阶段记为**假定完成**
  （`done` + `assumed:true` + `QS-TL-ASSUMED-DONE`，不参与失败/阻塞统计），其产出按在盘工件
  供下游使用；② `{"mode":"resume","resumeTaskId":...}` 续跑——历史 `done` 且记录产出在盘
  的阶段假定完成，历史 `assumed` 阶段重跑；pipeline 不匹配/阶段不存在/产出缺失分别以
  `resume: pipeline mismatch` / `QS-TL-SEL-UNKNOWNSTAGE` / `QS-TL-SEL-DEP-MISSING` 拒绝；
  ③ 子集/续跑在**创建任务前**做上游产出预检（不满足直接拒绝、不留目录），阶段 launch 前
  再复检一次（可选阶段缺输入 → `QS-TL-SKIP-NOINPUT`）；
  ④ `state.json` 顶层增 `selection`（用户原样）、阶段记录增 `assumed`；`task.started`
  非全链时携带 `selection`，假定阶段各发一条 `task.stage.finished(assumed:true)`，
  `task.finished.summary` 追加 `| assumed: ...`；
  ⑤ `inputs.validate` 支持 `selection`：带选择时只校验执行入口（依赖不都在选中集的阶段）
  的模块契约输入 + 其 `requires` 声明。全链路径行为不变（无 selection 的响应/通知与 P1 前一致）。
  **上游产出守卫的实现机制（设计偏离，留档）**：三个计算层模块 manifest 的阶段级输入均为空
  （`inputs:{}`；中间产物只记在 `consumesInternal/producesInternal`，不含平台可用的在盘路径），
  故"未选中上游产出在盘存在"的守卫声明在 `pipelines.json` 各阶段的 `requires`（dataRoot 相对
  路径，解析为 `PipelineStage.optionalInputs`，复用平台既有的 requires 机制，与
  `thermal_basic.post` 的既有用法同型）：peec `solve`/post、gds `prepared`/`em`/`thermal`、
  thermal `post` 共 6 处声明。未改动任何模块 manifest。
  验收：`tools\p1_selection_test.py`（T1–T11 共 22 断言）+ `rpc_smoke.py`（8 断言）+
  三流水线基线（peec S_parameter.csv 与参考逐字节一致、thermal 344.1037 K / 9564 数据行 /
  networkNodes 12750 / 4 数值件逐字节一致、gds_full 14 件产物逐字节一致）。
- 2026-11-05（P1.5 用户闸门 user gate，同分支）：流水线阶段可声明 `userGate`
  （`pipelines.json` 阶段级可选键：`waitForFiles` 等文件 / `inputs` CLI 值 / `confirm` 确认，
  缺省 = 无闸门，内置三流水线不变）：
  ① 新阶段状态 `WaitingInput`（序列化名 `waiting`，非终态）：阶段就绪但闸门未满足时
  不启动、留在调度队列，发新通知 `task.stage.awaiting`（`gate` 原样 + `missing` 缺失明细：
  files/confirmed/inputs）；等待时间**不计入**模块超时（超时自 ModuleRunner::launch 起算）；
  ② 新 RPC `task.provideInput {taskId, stageId, values?}`：阶段必须处于 waiting（否则
  `QS-TL-INPUT-NOTWAITING`）；values 支持**增量提供**（文件出现 → 传值 → 确认，values 可为
  空对象的唤醒事件）；required 完整性由闸门满足判定承担（未满足重发 awaiting），单次调用只校验
  已提供值的类型与 `allowedArgs` 白名单（与 overrides 同一校验路径，`checkAllowedArgs` 复用）；
  ③ values 拼进该阶段 CLI（`--key value`，与 overrides 同机制；**与 manifest 模板同键时覆盖
  模板默认**——模板侧出现被移除，不产生重复 flag，防模块解析顺序依赖；可见于 [launch] 日志 / job.json
  args）；
  ④ state.json 顶层 `providedInputs`（stageId→{values,ts,confirmed?}）审计留痕；
  ⑤ **pump() 死锁兜底修复（§4.2.4 陷阱）**：存在等待阶段时 pump 作为合法停等直接返回，
  不再把等待阶段判死（QS-TL-CFG-DEADLOCK 误杀）；由 provideInput 唤醒后重新 pump；
  ⑥ 等待中 cancel → 该阶段 Skipped（QS-TL-CANCEL）正常收尾；UI 断连按既有
  inputClosed→cancel 逻辑覆盖；`selection` 与闸门正交（resume 因等待被 cancel 的任务 →
  该阶段重新进入等待）。
  顺带修复：state.json 顶层 `status` 运行期恒为 `queued`（仅 finish 写终态），与 UI_API §4.9
  文档语义（`running`）不符 → 任务启动且仍 busy 时写 `running`（全假定任务同步收尾不覆写）。
  验收：`tools\gate_test.py`（T1–T5：三缺法 awaiting / 白名单拒绝 / 等待中 cancel / 断连 cancel /
  8s 无死锁回归 / job.args CLI 注入 / providedInputs 留痕）；全链基线回归另跑。
- 2026-11-05（P2 阶段库 + ref 组合，同分支）：`pipelines.json` 升至 **schemaVersion 2**
  （v1 全内联格式继续兼容；>=3 拒绝）：顶层新增 `stages` 阶段库对象（key = ref，条目为同一
  模块接线的库级默认 `{module(必填), name?, params?, artifacts?, requires?, optional?, userGate?}`），
  pipeline 阶段条目改 `{id, ref, dependsOn, [本地覆盖]}`；`ref` 与内联 `module` 必须恰有其一。
  ① **物化**在加载期完成（`PipelineLoader` 解析出与 v1 完全相同的 `PipelineStage` 结构，
  编排器/运行器无感）：params/artifacts 按键级合并（本地键赢）、requires/optional/name 本地有则
  本地否则库定义（name 兜底 stage id）、userGate 库定义为默认且本地只能 extends 新增键
  （库级闸门键不可被本地覆盖/取消）、dependsOn/id 恒为本地；未知 ref 加载期 fail-fast
  （serve 启动即 ErrConfig 退出 2）。② 阶段 `params`（此前为死字段）接线到模块 CLI：
  按 `--key value` 追加，与 overrides/闸门值同一机制，加载期对照 manifest `allowedArgs`
  白名单 fail-fast（模块注册存在性仍留启动期）；同键时最具体者赢（闸门值 > overrides >
  params，`ModuleRunner::dedupeInjectedArgs` 扩展为注入段内同 flag 保留最后一次出现，
  CLI 与 job.json 不产生重复 flag）。③ `pipelines.list` 载荷补全物化字段
  （`ref`/`requires`/`params`/`userGate`，非空才出现）。④ 内置三流水线迁移 v2 形态：
  接线（name/requires/optional/artifacts/userGate）全部上收阶段库、本地仅留 DAG，
  物化结果与 v1 逐字段等价（验收①以 P1 提交的 v1 配置对照 pipelines.list 全载荷）。
  验收：`tools\p2_stage_lib_test.py`（18 断言：v1/v2 物化等价 / 未知 ref 拒载 / 合并规则 /
  同 ref 双链独立覆盖 / params 白名单拒载 / ref 溯源）；回归四套件（rpc_smoke、
  p1_selection_test、baseline_e2e 三流水线字节红线、gate_test）均按 v2 配置重跑通过。
  未改动任何模块 manifest（计划五不碰清单保持）。
- 2026-11-05（P3 部署级覆盖 + module.run 路由修复，同分支）：
  ① **L1 层 `config/pipelines.local.json`（可选文件）**：集成商/客户 IT 用阶段库组合自定义流水线；
  格式与主文件相同（schemaVersion 1/2），同名 pipeline id / 同名阶段库 ref 均 **local 赢**
  （启动日志留痕，`pipelines.list` 该条目标 `source:"local"`，其余 `"builtin"`）；
  local pipeline 的 ref 可跨主/local 两个库解析；与主文件同一套加载期校验，任何错误
  **整文件拒载**（fail-fast：serve 启动即 ErrConfig 退出 2，错误带 `pipelines.local.json` 前缀）。
  算例级（dataRoot）不允许定义流水线（L2 收紧，计划 §4.4.1）。
  ② **module.run 路由修复（H2）**：单模块任务的阶段接线（artifacts 路由 / requires / params /
  userGate）从该模块在所有流水线（含 local）物化阶段条目中自动采用——恰好 1 处直接用其
  库级默认（回归：`module.run thermal.extract` 单跑时 `user/thermal.mesh` 正确路由到
  `Result/gds2geo/thermal/model_thermal.msh`，而非 manifest 默认 `Model/model_thermal.msh`）；
  0 处 = 现行为（manifest 默认位置）；>=2 处报错并列候选，新可选参数 `stageRef`
  （`"pipelineId/stageId"`，取候选之一）消歧。
  ③ **module.run 生命周期修复（验收期发现的既有隐患）**：编排器原以指针持有流水线
  （`const PipelineDef* m_pipeline`），而 `module.run` 的合成单阶段流水线是调用方栈上临时对象——
  任务收尾时 pump 解引用已析构对象，ACCESS_VIOLATION 崩溃（serve 进程死在 `task.stage.finished`
  之后、`task.finished` 之前，state.json 永久停留 `running`）。P3 验收 ③ 首次跑通单模块到
  完成时暴露；改为值拷贝持有 + 存在标志（`m_hasPipeline`），行为其余不变。
  **计划偏离（留档）**：计划 §7.3 的 stage 记录 `upstreamFingerprints`（size/mtime）字段本期
  顺延（P3 验收 ①-⑤ 不依赖；按 §5.2"指纹先软后硬"，待 state.json 数据积累后再定）。
  验收：`tools\p3_local_test.py`（local 覆盖/标记/日志留痕、非法 local 整文件拒载、H2 路由
  [launch] 断言、多候选报错 + stageRef 消歧）；回归四套件 + p2 测试均按最终构建重跑通过。
  未改动任何模块 manifest（计划五不碰清单保持）。
- 2026-11-06（gmsh 共享化：部署配置级变更，task_layer 无代码改动、未重编译）：
  gmsh 4.11.1 由 gds2geo 包内移至部署根共享工具区 `tools/gmsh/gmsh.exe`（git mv；
  平台所有，后续模块如 emopt 经同一 tools/ 区取用各自 gmsh 工件）。
  ① `config/modules.json`：gds2geo 四阶段路由的 `GDS2GEO_GMSH_EXECUTABLE` env 注入值改为
  `{MODULE_ROOT}/tools/gmsh/gmsh.exe`（路径变量机制，ModuleRunner 注入）；
  ② gds2geo manifest 值级 **V1.2**：`bundled: false`、`deliveryFiles` 8→7、`configuration`
  注明平台注入位（解析链代码行为不变：同目录遗留兜底 → env → PATH；模块 exe 零改动）；
  ③ 交付布局：dist 同步 modules.json；gds2geo/README 与 docs/REPACK/gds2geo.md（第 2 轮）同步。
  验证：`baseline_e2e` 重跑（gds_full 14 工件 vs final3 零差异 + thermal 数值不变）——
  同一 gmsh 二进制经 env 解析，行为等价。回退兼容：旧部署包内 gmsh.exe 仍在时同目录优先照旧。
- 2026-11-06（thermal 1.0.7 集成：task_layer 零代码改动、零模块 manifest 改动、未重编译）：
  ① **模块替换**：`thermal/` + `task_layer/dist/thermal/`（2026.11.06-native-1.0.7，apiVersion 7 不变：
  ThermalExtract 468,480→488,960 B / TemperatureProcess 459,264→460,800 B，manifest 8,008 B，SHA 校验）。
  ② **契约增量（模块侧输入接受能力；平台零适配）**：source_thermal.json 新增**简化 schemaVersion 3**
  （仅 sources：volume_ids 上 total_power，空数组 = 无热源允许；体材料按 mesh Physical 标签 + matdb 自动装配，
  无 thermal 提取对象、无 meshFile 绑定）与原 schemaVersion 2 并存；boundary_thermal.json 新增 **schemaVersion 2**
  （default adiabatic + boundaries：fixed_temperature（任意现有面，含内部面）/ convection）与原 schemaVersion 1 并存；
  程序按成对版本解析（2+1 或 3+2，错配 → 模块 exit 2 QS-TH-FMT-INPUT）；extraction_report.json 新增 3 个零值字段
  （surfaceFilmTriangles/convectionResistors/ambientTemperatureSources）；exit 6 = 流水线锁 QS-TH-RUN-LOCKED
  （活跃句柄检查，空闲残留锁文件不阻断运行）。
  ③ **最简单算例**：`dev/cases/thermal/heater`（vendor 20261106 案例包归档，归档件与 vendor zip **33/33 逐字节一致**）：
  零热源（schema 3 空 sources）+ 两个固定温度面（500 K / 293.15 K，面 3 含内部 HEATER 薄膜面）；
  OP0 = vendor 独立稀疏求解合成补齐件（原用户 OP0 缺 4 节点温度）。
  ④ **验证（1.0.7 双 exe，三通道）**：demo1_4.1 直跑——五项主要产物 + 节点温度 CSV 与 1.0.6 **逐字节一致**
  （10/11；唯一差异 = 报告新增 3 零值字段，数值字段无变化），基线 344.1037 K / 9564 行 / 12750 节点不变；
  heater 直跑 11/11 与 vendor 预烘焙参考逐字节一致（6089 节点 / 0 W / fixed 2569 / post 6089 行 / max 500 K /
  min 293.145 K）；平台全链——回归四套件全绿（rpc_smoke 8/8；p1_selection 22/22，S_parameter.csv 22,118 B
  字节红线不变；gate 24/24；baseline_e2e：thermal 基线 344.1037 K / 9564 行 / 12750 节点、参考字节一致，
  gds_full 14/14 产物与 final3 参考字节一致），heater 平台跑（网格落标准路由 Result/gds2geo/thermal/、
  schema 3 空 sources、无需 meshFile 同步）12/12 验收通过、物理产物与直跑逐字节一致。
  ⑤ **值级同步**：pipelines.json thermal_basic 描述（config/build/dist 三份）、UI_API / ERROR_CODES / 本 README
  （dist 同步）、templates/README（1.0.7 布局 + 成对表格 + 使用流程）、dev/cases/thermal/README（heater 段）、
  docs/REPACK/thermal.md（第 3 轮）、docs/DATA_LAYOUT.md（v1.5 修订 3）。
  ⑥ **交付**：PEEC_Studio_20261106_v2.1.zip 重切（同版本号重发先例；含 thermal 1.0.7 + heater 算例 + 更新交付说明）。
  未改动任何模块 manifest（计划五不碰清单保持）。

## 2026-11-09 gds2geo V1.2 r2 集成（publisher publishes 声明机制，增量改动）

**模块**：gds2geo V1.2（GDS2GEO-V1.2-20261107-r2，2026-11-09 集成；manifest apiVersion 4，产品版本 V1.2 / EXE 1.2.0）。2 EXE（gds2geo.exe / geo2msh.exe）+ README / ERROR_CODES / manifest，共 5 文件；旧 4-EXE（gds2geo / geo_prepare / geo2em / geo2thermal）交付形态作废。

**破坏性变更（vendor 声明）**：① 交付 API 3 + prepared target schema 3，Direct GDS2GEO 取代旧 source/geo_prepare 交付阶段；GEO 用 GDS 版图用户单位（dim = 每坐标单位米数），MSH 输出 mm；各 target 独立 MATDB；gmsh 由平台提供；② 交付 API 4：MSH 保留全部原生 CAD 实体（维度 0–3，Point1/Line2/Tri3/Tet4，可含未引用节点）；限制于 face/body 或元素 2/4 的消费方需适配（见 REPACK/compact_em.md 第 2 轮）。

**布局**：`Result/gds2geo/` 重组为 em_geo / thermal_geo / em_msh / thermal_msh 四目录（10 件产物，receipt 取消）；两链 em_geo→mesh_em ∥ thermal_geo→mesh_thermal。

**平台侧适配（增量，未改任何模块 manifest）**：
- r2 manifest 取消 `produced[].publishTo`（vendor 声明：Platform registration and downstream artifact routing require separate integration；见 manifest `capabilities.publicationPolicy` / `capabilities.platformIntegration`）。正式件落位改由②平台 `pipelines.json` 阶段条目 `publishes` 声明（DATA_LAYOUT D7 机制②；`gds2geo/mesh.em → Model/model_em.msh`、`gds2geo/matdb.model → Model/matdb.json`，仅 EM 对）：
  - PipelineLoader：解析阶段条目 `publishes`（ref→路径 字符串表）；
  - ModuleRegistry：新增 `basePublishes` / `rebindDataRoot(..., publishOverrides)`（publishes 路径的 dataRoot 重绑定）；（2026-11-10 缺陷修复中 `basePublishes` 并入 `publishesTemplate` 快照机制，`rebindDataRoot` 增包含性守卫与 error 出参，见下条）
  - App.cpp：`publishOverridesOf(stage)` 收集阶段 publishes（pipeline 级 + local 覆盖级）并传入 3 处 publisher 重绑点；
  - 语义：params / artifacts / publishes 均按键级合并、local 赢；两机制并存时 manifest publishTo 仍生效（thermal / compact_em 不变）。
- `modules.json`：gds2geo 4 个模块键 gds.geo_em / gds.geo_thermal / gds.mesh_em / gds.mesh_thermal（旧 em.source / em.prepare 移除）；gmsh 仅注入 msh 阶段 env `GDS2GEO_GMSH_EXECUTABLE`。
- `pipelines.json`：阶段库条目 gds.geo_em / gds.geo_thermal / em.mesh / em.mesh_thermal（库键 em.mesh / em.mesh_thermal 沿用 v2.1 名，module 绑定改为 gds.mesh_em / gds.mesh_thermal）；gds_full 流水线四阶段（geo_em / geo_thermal / mesh_em / mesh_thermal，两链）；thermal_basic 条目 `user/thermal.mesh` 路由 → `Result/gds2geo/thermal_msh/model_thermal.msh`。

**验证（全绿）**：gds_full r2 E2E 10 件产物 + 2 Model/ 落位（sha 基线 `E:/code/_integ/r2regress/gds2geo_r2_baseline.json`）；PEEC 红线 S_parameter.csv 22,118 B 逐字节一致；thermal demo1_4.1 344.1037 K / 9564 行；heater 11/11 逐字节一致；gate 24/24；sel 22/22；rpc_smoke 8/8。

**API-4 探针结论（→ REPACK）**：compact_em v1.3 标准输入拒绝 r2 em 网格的激励线元素（`QS-FMT-MSH-INVALID`，网格解析本身正常：608 点 / 1277 元素 / 2 材料）→ GDS 生成网格 → peec 全流阻塞，预烘焙网格算例不受影响（红线 PASS）；REPACK/compact_em.md 第 2 轮 + REPACK/gds2geo.md 第 3 轮已提交，待 vendor 回执。thermal 1.0.7 可正常解析 r2 热网格（8095 节点），探针失败系算例 boundary surface_ids 陈旧绑定（算例准备问题，非求解器缺陷）。

**文档**：DATA_LAYOUT v1.6；MODULE_MANIFEST_SPEC v1.3（示例更新为 r2 / compact_em v1.3）；DELIVERY_PACKAGE_SPEC v1.1（主包 39→37 条目，gds2geo 7→5 文件）；UI_API.md / ERROR_CODES.md / 本 README 同步更新。

未改动任何模块 manifest（计划五不碰清单保持）。


- 2026-11-10（缺陷修复：切换工程后产物路径串用；分支 `fix/project-switch-path-freeze`，基线 = 20261106 v2.1 发版切点）：
  同一 serve 进程先跑工程 A 再跑工程 B 时，B 的 `stages[].outputs` / `resultFiles` 指向 A 的
  `Result/`，B 的 `Model/` 发布件缺失（后端判 `done`、前端报"本次成功清单缺少必需产物"）。
  根因：`ModuleRegistry::rebindDataRoot` 对 `outputChecks` / `publishes` src/dst **原地**展开，且以
  `QFileInfo::isAbsolute()` 判"已展开"——第二次 rebind（切换工程）把已是绝对的路径当"已展开"跳过，
  契约冻结在首个任务的 dataRoot（`args` 与 `requiredInputs` 每次都从模板重建，不受影响）。
  ① `ModuleDef` 新增相对形态快照（`outputChecksTemplate` / `publishesTemplate`，加载期取自 manifest，
  为唯一事实源），`rebindDataRoot` 每次经 `toNative` 重新展开（与 `argsTemplate` 同机制），不再原地展开；
  ② 新增 dataRoot 包含性守卫（`pathWithinDataRoot`）：契约产出路径（outputChecks / publishes src/dst）
  解析越出当前 dataRoot → rebind 返回 false，三个调用点均在**执行前**拒绝：`task.run` 返回
  `-32004`（ErrConfig）且不创建任务、`inputs.validate` 返回 `valid:false`
  （`reason=QS-TL-PATH-ESCAPE`）、`run` CLI 打 `config error:` 退出码 2。合法 manifest
  （produced 受限于 `Result/<id>/` 下、publishTo 受限于 Model/ 或 Setting/ 前缀）不触发守卫——
  纵深防御异常包/路由，非正常路径的阻断（P3 验收确认无误报）；
  ③ publish 机制本身语义不变：发布失败仍按既有 `QS-TL-OUTPUT-MISSING` 判未完成（本缺陷
  不新增 `task.stage.finished` 码，守卫码仅在执行前呈现）；
  ④ 文档：ERROR_CODES.md 增 §2.7（QS-TL-PATH-ESCAPE 及三条呈现路径、"从不进 stage.finished"
  说明）、-32004 行更新；UI_API.md §4.4 / §6 对应说明。
  验收：`tools\project_switch_test.py`（v2.1 布局：同一 serve 进程 A→B→A，dataRoot 含中文+空格；
  P1 旧版基线二进制复现（outputs/resultFiles 指向 A 根）、P2 B 的 outputs/resultFiles 全部在 B 根且
  A 根 0 命中 + 发布件 SHA-256 相等 + A 树快照不变 + 删旧产物后重跑不复用、P3 守卫对合法 manifest
  无误报）。未改动任何模块 manifest。
