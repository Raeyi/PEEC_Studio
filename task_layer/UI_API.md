# task_layer 对接说明（供 UI 集成）

本文件是**给前端/UI 侧**的对接文档：任务层以 `task_layer.exe` 形式交付，
UI 通过 `QProcess` 启动它，双方用 **stdin/stdout 上的 JSON-RPC 2.0** 通信。
UI 不需要、也不要直接调用计算层的 exe（`compact_em\`、`gds2geo\`）。

任务层职责：接收 UI 的算例目录与流水线名 → 按依赖调度后端程序 → 实时回传进度 → 回传结果文件清单。

---

## 一、交付物与部署

### 1.1 交付目录（UI 直接拿这个）

用打包脚本产出，一条命令：

```powershell
cd PEEC_Stdio\task_layer
powershell -ExecutionPolicy Bypass -File .\tools\package.ps1     # 产出 dist\
```

```text
dist\                     ← 整个目录交给 UI，拷走即用
├── task_layer.exe        ~935 KB  UI 唯一需要启动的进程
├── Qt6Core.dll           6.8 MB   必须与 exe 同目录
├── libstdc++-6.dll                 MinGW 运行时（MSVC 构建时没有这三项）
├── libgcc_s_seh-1.dll
├── libwinpthread-1.dll
├── config\                         必须：模块注册表 + 流水线定义
│   ├── modules.json
│   └── pipelines.json
├── bin\
│   ├── worker_host.exe             DLL 形态模块的宿主（当前流水线用不到，可删）
│   └── Qt6Core.dll
├── UI_API.md                       本文档
├── ERROR_CODES.md                  任务层自有错误码登记（QS-TL-*）
└── README.md
```

重新打包会**清空重建** `dist\`，不会残留旧产物；若 `task_layer.exe` 正被运行中的实例占用导致复制失败，脚本会直接报错中止（不会打出缺主程序的残包）。

**与 compact_em 打包规则的差异**：`compact_em` 的 4 个 exe 是纯静态的（只依赖 `KERNEL32.dll` / `bcrypt.dll`），单文件拷走即可；任务层用了 Qt 核心库，因此需要额外带上 `Qt6Core.dll` 等 4 个 DLL，**不能只拷 `task_layer.exe` 一个文件**。

`config\` 里的两个 JSON 是**可调配置**：后端 exe 换目录、改流水线编排时改它们即可，不用重新编译。

### 1.2 部署布局：程序与数据分离

```text
<部署根>\
├── compact_em\            程序：电磁计算 exe（只读）
├── gds2geo\               程序：建模与网格 exe（只读）
├── task_layer\
│   └── dist\              程序：任务层（UI 启动它）
└── cases\                 数据：dataRoot 只指向这里
    ├── demo\DATA\
    └── <其它项目>\DATA\
```

任务层启动时**从 exe 所在目录逐级向上**查找含 `compact_em` / `gds2geo` 的目录作为模块根。
`dist\` 在 `task_layer\` 下时会自动找到，已实测无需额外参数。

**算例数据不放在程序目录里**，理由：

- 后端程序目录保持"只有程序"，不会被网格、端口、求解结果污染（`model.peec` 单个文件可达 GB 级）；
- 多个版图项目各占一个 `cases\<项目>\DATA`，互不覆盖；
- 不会误删后端自带的示例算例。

已实测：`dataRoot` 指向 `cases\demo\DATA`（在后端目录之外）时，`gds_full` 正常跑完，
产物全部落在该算例目录，两个程序目录没有新增任何算例文件。
如果 UI 侧把 `task_layer\` 单独拷到别处，后端就找不到了，此时必须显式指定：

```powershell
task_layer.exe serve --module-root D:\app\backend
```

`--module-root` 指向**后端包的父目录**（上例中的 `<模块根>`）。

---

## 二、启动与进程管理

```powershell
task_layer.exe serve [--config <目录>] [--bin <目录>] [--module-root <目录>]
```

- `serve` 是固定子命令，缺了会走命令行模式（跑一次就退出），UI 集成时不要漏；
- 三个可选参数都有自动推导，正常部署下不用传；
- 工作目录建议设为 **exe 所在目录**；
- **通道模式必须是 `SeparateChannels`**：stdout 是纯 JSON 消息流，stderr 是任务层自身的告警/崩溃信息，两者不能合并；
- 退出：先发 `shutdown` 请求，再等待进程结束；超时（>10s）才 `kill`。
  任务层也监听了 stdin 关闭，UI 进程崩溃时它会自行退出，不会留僵尸进程。

---

## 三、通信协议

- 传输：stdio，UTF-8，**一行一个 JSON 消息**，以 `\n` 结尾（Compact 契约）；
- 方向：UI 写 stdin 发请求，任务层写 stdout 回响应/通知；
- 三种消息：
  - **请求**：含 `id` + `method` → 必有响应；
  - **响应**：`{jsonrpc, id, result}` 或 `{jsonrpc, id, error}`；
  - **通知**：含 `method` 无 `id` → 任务层主动推送，UI 不应回复；
- 配对：UI 自增 `id`，按 `id` 匹配响应；通知按 `method` 分发。

---

## 四、API 一览

| method | params | 返回 |
|---|---|---|
| `ping` | — | `{pong, app, schemaVersion}` |
| `modules.list` | — | `{modules:[{id,displayName,kind,command,available,resolvedPath}]}` |
| `pipelines.list` | — | `{pipelines:[{id,displayName,stages:[...]}]}` |
| `inputs.validate` | `{dataRoot, pipeline?, module?, inputs?}` | `{valid, checked, issues, warnings}` |
| `task.run` | `{pipeline, dataRoot, taskId?, inputs?}` | `{taskId, pipeline}` |
| `module.run` | `{moduleId, dataRoot, taskId?, inputs?}` | `{taskId, pipeline}` ← **单独启动一个模块** |
| `task.cancel` | `{taskId}` | `{cancelled}` |
| `task.status` | `{taskId}` | `state.json` 内容 |
| `task.result` | `{taskId}` | `state` + `resultFiles` |
| `shutdown` | — | `{bye}` 后退出 |

### 4.1 ping（握手）

```json
{"jsonrpc":"2.0","id":1,"method":"ping","params":{}}
```
```json
{"jsonrpc":"2.0","id":1,"result":{"pong":true,"app":"task_layer","schemaVersion":1}}
```

### 4.2 modules.list（拿到模块与可用性）

```json
{"jsonrpc":"2.0","id":2,"method":"modules.list","params":{}}
```

返回片段：

```json
{"id":2,"result":{"modules":[
  {"id":"phys.model_parse","displayName":"物理模型解析（GDS2GEO 源模型）",
   "kind":"external","command":"gds2geo.exe",
   "available":true,"resolvedPath":"C:\\...\\gds2geo\\gds2geo.exe"},
  {"id":"sim.solver","displayName":"仿真求解（MNA/ALPS，待接入）",
   "kind":"dll","command":"sim_solver.dll","available":false,"resolvedPath":""}
]}}
```

- `available: true` 表示当前部署下能真的找到程序；`false` 是尚未接入的占位模块；
- UI 的"单独运行某个模块"列表请只展示 `available: true` 的项。

### 4.3 pipelines.list（拿到可跑的流水线）

```json
{"jsonrpc":"2.0","id":2,"method":"pipelines.list","params":{}}
```

返回片段：

```json
{"jsonrpc":"2.0","id":2,"result":{"pipelines":[
  {"id":"peec_basic","displayName":"电磁基本链路（PEEC 三段）",
   "stages":[{"id":"extract","module":"peec.model_extract","optional":false,"dependsOn":[]},
             {"id":"solve","module":"peec.solver","optional":false,"dependsOn":["extract"]},,
             {"id":"post","module":"peec.post","optional":true,"dependsOn":["solve"]}]},
  {"id":"thermal_basic","displayName":"热基本链路（thermal 两段）",
   "stages":[{"id":"extract","module":"thermal.extract","optional":false,"dependsOn":[],
              "artifacts":{"user/thermal.mesh":"Result/gds2geo/thermal/model_thermal.msh"}},
             {"id":"post","module":"thermal.post","optional":true,"dependsOn":["extract"],
              "artifacts":{"user/thermal.mesh":"Result/gds2geo/thermal/model_thermal.msh"},
              "requires":["Model/thermal.op0"]}]}
]}}
```

stage 对象另有两个可选字段（仅当配置了才出现）：
- `artifacts`：`{ref: dataRoot 相对路径}`，跨模块工件路由——该阶段启动时，ref 对应的输入文件按此路径解析（覆盖模块 manifest 的默认位置）；
- `requires`：dataRoot 相对路径数组，可选阶段的启动前置文件，缺失时该阶段以 `QS-TL-SKIP-NOINPUT` 跳过（不阻断主链）。

**当前可用状态**（重要）：

| pipeline | 说明 |
|---|---|
| `gds_full` | **建模**：源模型 → 几何准备 → 电磁网格 → 热网格（1102 版起四段全必需）；EM 主网格 + 材料由任务层 **publisher 自动落位到 `Model/`**，热网格保留在 `Result/gds2geo/thermal/`（v1.1 布局，无 handoff）。gds2geo 1102 版需要的 gmsh 4.11.1 由 task_layer 经子进程环境变量 `GDS2GEO_GMSH_EXECUTABLE` 自动注入，UI 无需处理 |
| `peec_basic` | **电磁**：PEEC 提取 → 频域求解 → 源/场后处理（post 段可选）；S 参数绘图由前端完成（plot 阶段自 2026-11-04 / v1.1 起移除；绘图程序与 Touchstone 导出随 compact_em v1.3 一并取消，2026-11-05 起生效） |
| `thermal_basic` | **热**：热路提取（thermal.extract，必需）→ 温度后处理（thermal.post，可选，无 OP0 时自动跳过）。输入全部来自上游：热网格经 stage `artifacts` 路由到 `Result/gds2geo/thermal/model_thermal.msh`，材料用 `Model/matdb.json`（gds_full 发布），热链三件由 UI 写入：`Model/source_thermal.json`（热源 + thermal 提取对象（体域/薄层/材料关联），**thermal 1.0.6 起必需 schemaVersion 2**；1.0.5 时代提取控制为独立文件 `Result/thermal/thermal_extract_control.json`，已取消）+ `Model/boundary_thermal.json`（边界条件，必需，thermal 1.0.5 起；1.0.1 时代内嵌于 source_thermal.json）+ `Setting/set_thermal.json`（SPICE 仿真设置 analysis/probe，thermal 1.0.5 起）；post 依赖 `Model/thermal.op0`（ALPS 结果，1.0.6 起标准位置；1.0.5 时代为 Result/thermal/post/，已废止） |

进度说明：计算层模块没有百分比/进度条接口，任务层的进度是**阶段级**的（`task.stage.started` / `task.stage.log` / `task.stage.finished`），详见下文"五（补充）、进度与日志的边界"。
模块能否真正运行（程序是否找得到）用 `modules.list` 的 `available` 字段判断（见 4.2）。

### 典型业务流程（四步，中间插着 UI 选端口）

端口只能由人在网格上选，所以**不是一条流水线跑到底**，而是四步：

```text
① UI: task.run gds_full          → publisher 自动落位 Model/model_em.msh + matdb.json
                                   （此时还没有端口文件，建模照样成功；旧网格自动替换）
② UI: 加载 model_em.msh 渲染      → 用户选三角面 → 写 Model/source_em.json（规范名，v1.2）
                                   同时写求解设置（EM 链只读 Setting/set_em.json，平铺 schemaVersion=1；compact_em v1.3 起生效（2026-11-05），旧 set.json 不再读取）
                                   （格式见 DATA_LAYOUT §八；样例随平台交付包 templates/ 与案例包提供）
③ UI: inputs.validate（可选）     → 跑前体检：文件存在 + 端口↔网格 SHA-256 绑定
④ UI: task.run peec_basic        → 提取 → 求解 → S 参数
```

热链同样在 gds_full 之后展开（热网格不经过 `Model/`，直接引用 gds2geo 的产物）：

```text
① UI: task.run gds_full              → 热网格产出在 Result/gds2geo/thermal/（matdb 含热物性）
② UI: 写求解设置            → v1.2 时代：set.json 增加 "thermal" 对象；thermal 1.0.6 起：写 Setting/set_thermal.json（SPICE 仿真设置）；提取控制（thermal 对象）内嵌于 Model/source_thermal.json（schemaVersion 2）
                                        （tetrahedral_domains / thin_layers，elementary 实体 ID；
                                        薄层 material_footprints 必须完整铺满 reference_surfaces）
                                        写 Model/source_thermal.json        → meshFile 指向
                                        Result/gds2geo/thermal/model_thermal.msh + 其 SHA-256，
                                        sources/fixed_temperature 用网格 elementary ID
③ UI: task.run thermal_basic         → 热路提取（post 无 OP0 时自动跳过）
④（可选）OP0 就绪（Model/thermal.op0；thermal 1.0.6 起标准位置，
            thermal_run.json / 独立控制件均已取消）后重跑 → 温度后处理
```

```json
// ① 建模（不需要端口文件）
{"method":"task.run","params":{"pipeline":"gds_full","dataRoot":"D:/cases/demo/DATA"}}

// ② UI 自己写 D:/cases/demo/DATA/Model/source_em.json 和求解设置（不经过 task_layer）

// ③ 跑前体检（可选但推荐；issues 直接可展示给用户）
{"method":"inputs.validate","params":{"pipeline":"peec_basic","dataRoot":"D:/cases/demo/DATA"}}

// ④ 跑电磁
{"method":"task.run","params":{"pipeline":"peec_basic","dataRoot":"D:/cases/demo/DATA"}}

// 热链：建模完成后（求解设置 / source_thermal.json 已由 UI 写好）
{"method":"task.run","params":{"pipeline":"thermal_basic","dataRoot":"D:/cases/demo/DATA"}}
```

几点约定：

- ① ~ ④ 必须用**同一个 `dataRoot`**；
- 正式件落位（`Result/gds2geo/em/` → `Model/`）由任务层 publisher 按 manifest `publishTo`
  自动执行并**保证 `Model/` 下只有一份 `*.msh`**（多份 EM 会直接报 `QS-FMT-INPUT-AMBIGUOUS`）；
  旧版的 `internal.handoff` 模块已移除，UI 不需要再触发任何交接动作；
- ③ 不是必须的，但强烈推荐——否则端口与网格的 SHA-256 绑定错误要到后端运行中才会被发现
  （报 `QS-IO-FILE-CORRUPTED`）。

### 4.4 inputs.validate（跑之前先体检）

```json
{"jsonrpc":"2.0","id":3,"method":"inputs.validate","params":{
  "pipeline":"peec_basic",
  "dataRoot":"D:/cases/demo/DATA",
  "selection":{"mode":"stages","stages":["post"]}
}}
```

```json
{"jsonrpc":"2.0","id":3,"result":{
  "valid":false,
  "checked":["D:/cases/demo/DATA/Model/model_em.msh", "..."],
  "issues":[{"path":"D:/cases/demo/DATA/Model/source_em.json",
             "reason":"missing","detail":"file does not exist"}],
  "warnings":[]
}}
```

UI 应在点击"开始计算"前调用它，把 `issues` 直接展示给用户（路径 + 原因），`warnings` 以次要样式提示。

`selection`（P1 起，可选，语义与 `task.run` 相同）：带选择时只校验**执行入口**——
"依赖不都在选中集"的阶段（resume 先归一为待跑集）——的模块契约输入，加上这些入口
阶段在流水线 `requires` 中声明的上游中间产物（dataRoot 相对路径，如
`Result/compact_em/extract/model.peec`）。全链校验（不带 `selection`）行为不变。
假定完成阶段的产出不在 validate 期重复核验（`task.run` 期预检保证）。

注意：这是**静态输入检查**（文件存在性、source_em.json 与网格的 SHA-256 绑定等）。后端模块在运行中还会做自己的深度校验（格式、物理一致性等），失败时通过 `task.stage.finished` 的 `errorCode` 透传回来（见"六、错误码"与 [ERROR_CODES.md](ERROR_CODES.md)）。

### 4.5 task.run（启动任务）

```json
{"jsonrpc":"2.0","id":4,"method":"task.run","params":{
  "pipeline":"peec_basic",
  "dataRoot":"D:/cases/demo/DATA",
  "selection":{"mode":"stages","stages":["post"]}
}}
```
```json
{"jsonrpc":"2.0","id":4,"result":{"taskId":"t-20261028-104233-7f3a1c",
  "selection":{"mode":"stages","stages":["post"]}}}
```

- `dataRoot` **必须是绝对路径**（任务层会把后端进程的工作目录设为它，传相对路径会被二次拼接）；
- `taskId` 可省略，由任务层生成；UI 若要自己管理任务号，可传入（须唯一）；
- 一个任务层进程**同时只能跑一个任务**，重复调用返回 `-32002`；
- 该请求只是"启动"，真正完成靠 `task.finished` 通知。

#### 执行选择 `selection`（P1 起；省略或 `{"mode":"full"}` 即全链运行，行为与 P1 前一致）

| selection | 语义 |
|---|---|
| 省略 / `{"mode":"full"}` | 全流水线运行（默认） |
| `{"mode":"stages","stages":["solve","post"]}` | 只跑指定阶段（可为空集）。未选中的上游阶段记为**假定完成**：`status=done` + `assumed:true` + `errorCode=QS-TL-ASSUMED-DONE`，不执行、不参与失败/阻塞统计；其产出按**在盘工件**作为下游输入 |
| `{"mode":"resume","resumeTaskId":"t-..."}` | 续跑历史任务：历史任务中 `done` 且**记录产出全部在盘**的阶段假定完成，其余真实执行；历史任务中的 `assumed` 阶段不信任、重新执行；`resumeTaskId` 须在同一 dataRoot 下且 `pipelineId` 一致 |

行为约定：

- **创建前预检**：子集/续跑在创建任务**之前**校验"未选中上游产出在盘存在"（流水线 `requires` 声明的中间产物 + 模块契约输入）。不满足 → 直接返回 `QS-TL-SEL-DEP-MISSING`（`errorMessage` 列出具体路径），**不创建任务、不留目录**；
- **阶段内复检**：每个阶段 launch 前再检一次（防运行期间文件被删）。可选阶段缺输入 → `QS-TL-SKIP-NOINPUT` 跳过；必需阶段缺输入 → `QS-TL-SEL-DEP-MISSING` 失败；
- **状态落盘**：`state.json` 顶层增加 `selection`（用户原样传入的选择）与每阶段 `assumed` 标记；全链运行不写这两个字段（旧形态不变）；
- **通知**：非全链时 `task.started` 带 `selection`；每个假定阶段在 `task.started` 后各发一条 `task.stage.finished`（`status=done`、`assumed:true`、`errorCode=QS-TL-ASSUMED-DONE`、`outputs=[]`）；`task.finished` 的 `summary` 末尾追加 `| assumed: <阶段列表>`；
- **路由**：假定完成阶段按其在流水线中的 `artifacts` 路由照常生效；
- **CLI**：`task_layer.exe run`（CLI 子命令）仍只支持全链；`selection` 仅通过 JSON-RPC（UI 侧）传入。

续跑相关错误：pipeline 不匹配 → `resume: pipeline mismatch (state is for '<id>')`；历史阶段不在当前流水线 → `QS-TL-SEL-UNKNOWNSTAGE`；历史阶段记录产出在盘缺失 → `QS-TL-SEL-DEP-MISSING`。

### 4.6 模块独立执行（module.run）

不走流水线、单独跑一个模块时用这个（例如只重建网格、只跑一次源模型解析）：

```json
{"jsonrpc":"2.0","id":4,"method":"module.run","params":{
  "moduleId":"phys.model_parse",
  "dataRoot":"D:/cases/demo/DATA"
}}
```
```json
{"jsonrpc":"2.0","id":4,"result":{"taskId":"t1793168978448","pipeline":"single:phys.model_parse"}}
```

要点：

- `moduleId` 取 `modules.list` 里的 `id`；
- **与流水线执行完全一致**：同样收到 `task.started` / `task.stage.*` / `task.finished` 通知，
  同样能用 `task.status` / `task.result` / `task.cancel`；
- 返回的 `pipeline` 字段形如 `single:<moduleId>`，便于 UI 区分"流水线任务"与"单模块任务"；
- 同一进程仍然一次只能跑一个任务（`-32002`）；
- 模块自身的输入契约照常生效，缺输入时报 `QS-TL-INPUT-MISSING`。

同一个模块既可以用 `module.run` 单独启动，也可以作为 `task.run` 流水线里的一个阶段，
两种方式走的是同一套执行与状态逻辑，UI 不需要写两套处理。

### 4.7 端口（源）定义（UI 负责写进 `Model/`；规范名 `source_em.json`）

端口只能由用户在网格上选取，任务层既生成不了也猜不出来，所以**端口文件由 UI 直接写到
`{dataRoot}/Model/`**，不经过 task_layer。按 DATA_LAYOUT v1.2 D3，EM 链路规范名为 **`source_em.json`**
（热链路 `source_thermal.json`；`boundary_em.json` / `boundary_thermal.json` 为预留槽位，
暂无消费方、格式待定，UI 可不写）。**UI 只写 `source_em.json`**（当前交付的 compact_em v1.2
版 2026-11-02 起实读新名、不回退旧名）；平台预检仍兼容旧名 `port_em.json`
（新名优先），用于过渡存量算例：

```json
{
  "meshFile": "model_em.msh",
  "meshSha256": "<网格原始字节的 SHA-256>",
  "ports": [
    {"direction": [0,0,1],  "impedanceOhm": 50, "triangleIds": [1, 2]},
    {"direction": [0,0,-1], "impedanceOhm": 50, "triangleIds": [3, 4]}
  ]
}
```

要点：

- `triangleIds` 必须取自**当前这份网格**（UI 加载 `Model/model_em.msh` 渲染后让用户选）；
- `direction` 必须指向从 `triangleIds[0]` 到 `triangleIds[1]` 的方向，否则 EM 报 `QS-BC-PORT-MAPPING_AMBIGUOUS`；
- **`meshSha256` 必须非空且等于实际网格的 SHA-256**，EM 会逐个校验：

  | 填法 | EM 反应 |
  |---|---|
  | 留空 `""` | `QS-FMT-JSON-SCHEMA_VIOLATION`：`meshSha256 must be a non-empty JSON string` |
  | 非空但不匹配 | `QS-IO-FILE-CORRUPTED`：`meshSha256 does not match the selected mesh` |

  **推荐做法**：UI 写文件时自己算好网格 SHA-256（网格文件就是 UI 自己渲染的来源，
  算哈希很便宜），写完再调一次 `inputs.validate` 复核（绑定不符会直接报 issue）。

- `Model/` 下只能有一份 `*.msh`：版图迭代会不断产出新网格，publisher 落位正式件时会
  **自动替换上一轮的旧网格**，UI 不需要手动清理（否则 EM 报 `QS-FMT-INPUT-AMBIGUOUS`）。

**版图迭代时的失效检测**（重要）：网格重新生成后，端口文件里记的 `triangleIds` 可能与新网格
不再对应。当前由 SHA-256 绑定检查兜底：`inputs.validate` 会报 `source_em.json` 绑定不符
（后端运行时则报 `QS-IO-FILE-CORRUPTED`），此时 UI 应提示用户**重新选端口**；
建模任务本身仍会成功完成。

### 4.8 inputs：可选的动态输入（预留通道）

`task.run` / `inputs.validate` 都接受 `inputs` 对象，会写入 `state.json` 的 `inputs` 字段留痕。
该通道支持通用动态变量（驼峰键转大写下划线：`portJson` → `{PORT_JSON}`、`setFile` →
`{SET_FILE}`），供模块 `argsTemplate` 引用。**当前流水线的模块 manifest 一律走固定路径
（DATA_LAYOUT：`Model/source_em.json`、`Setting/` 求解设置 等），不经过该通道**——
为后续模块预留。

- 当前 `peec_basic` 的输入全部是固定名：`Model/model_em.msh`、`Model/matdb.json`、
  求解设置（`Setting/set_em.json`，平铺 schemaVersion=1，compact_em v1.3 起生效（2026-11-05）；旧 set.json 不再读取；
  compact_em v1.2 版起 set.txt 弃用；样例随平台交付包 templates/ 提供）。
- UI 要改求解设置（FS/FE/N_FP 等）直接写 dataRoot 的 `Setting/`，不经任务层代理；设置文件整体纳入模块 artifacts/receipt 指纹，内容变化后旧上游结果不可复用（需重新提取/求解）；
- S 参数 CSV（`S_parameter.csv`）默认列为 `frequency_Hz` + `S<i>_<j>_real`/`S<i>_<j>_imag`（1-based，i=响应端口、j=激励端口，S 无量纲；SI 赫兹）；模块支持 `--s-columns full`（附加模块计算的 dB/相位列），平台默认 RI 列，前端按 dB = 20·log10(|Sij|)、相位 = atan2(imag, real) 换算；
- `inputs.validate` 会校验必需输入文件存在、`meshFile` 存在且哈希与主网格一致。

### 4.9 task.status / task.result

```json
{"jsonrpc":"2.0","id":5,"method":"task.status","params":{"taskId":"t-20261028-104233-7f3a1c"}}
```

返回 `state.json` 内容：

```json
{"taskId":"t-20261028-104233-7f3a1c","pipeline":"peec_basic","status":"running",
 "startedAt":"2026-10-28T10:42:33","elapsedMs":0,
 "inputs":{},
 "stages":[
   {"id":"extract","moduleId":"peec.model_extract","name":"PEEC 模型提取",
    "status":"done","exitCode":0,"startedAt":...,"finishedAt":...,
    "outputs":["D:/cases/demo/DATA/Result/compact_em/extract/model.peec"],"errorCode":"","errorMessage":""},
   {"id":"solve","moduleId":"peec.solver","name":"频域求解","status":"running",...}
 ]}
```

`task.result` 在其基础上多一个 `resultFiles`（所有阶段产出的并集，均已确认存在）：

```json
{"taskId":"...","status":"done","resultFiles":[
  "D:/cases/demo/DATA/Result/compact_em/extract/model.peec",
  "D:/cases/demo/DATA/Result/compact_em/solve/S_parameter.csv"]}
```

P1 起（仅非全链任务）：顶层多 `selection`（用户原样传入），阶段记录多 `assumed` 布尔（假定完成阶段为 `true`，伴随 `errorCode=QS-TL-ASSUMED-DONE`、`outputs=[]`）。

任务结束后仍可查询（任务层保留最近一次任务），UI 不必在运行中抢答。

### 4.10 task.cancel / shutdown

```json
{"jsonrpc":"2.0","id":6,"method":"task.cancel","params":{"taskId":"..."}}
{"jsonrpc":"2.0","id":7,"method":"shutdown","params":{}}
```

---

## 五、通知（任务层 → UI，实时状态）

| method | params |
|---|---|
| `task.started` | `{taskId, pipeline, ts, selection?}` | `selection` 仅非全链时携带（用户原样传入的选择） |
| `task.stage.started` | `{taskId, stageId, name}` |
| `task.stage.log` | `{taskId, stageId, line}` | 后端 stdout/stderr 逐行转发（非结构化，见"五（补充）"） |
| `task.stage.finished` | `{taskId, stageId, status, exitCode, errorCode, errorMessage, outputs[], assumed?}` | 假定完成阶段（P1 执行选择）带 `assumed:true`，在 `task.started` 后即时发出 |
| `task.finished` | `{taskId, status, summary, elapsedMs}` |
| `task.cancelled` | `{taskId}` |

UI 建议映射：`task.stage.started` → 阶段打点；`task.stage.log` → 日志窗（后端 stdout/stderr 逐行转发，非结构化文本，高频，注意限流）；
`task.finished` → 收尾，随后调用 `task.result` 拉清单。

阶段 `status`：`pending` / `running` / `done` / `failed` / `skipped` / `notConfigured`。

---

## 五（补充）、进度与日志的边界

计算层模块（gds2geo / compact_em）目前**没有结构化的进度接口**：它们只在 stdout/stderr 输出人可读文本，机器侧只有进程退出码与（compact_em 的）stderr JSON 错误记录。因此任务层回传给 UI 的"进度"是三层信息，UI 需要区分使用：

| 来源 | 内容 | 性质 |
|---|---|---|
| `task.stage.started` / `task.stage.finished` | 阶段打点与终态（含 exitCode/errorCode/outputs） | **结构化**，可驱动进度条骨架与阶段状态 |
| `task.stage.log` | 后端 stdout/stderr 逐行转发（UTF-8 优先，出现替换字符回退本地代码页） | **非结构化文本**；compact_em 的 stderr JSON 错误行也在这里 |
| `state.json`（`task.status` / `task.result`） | 全量阶段记录 + 结果文件清单 | 结构化，可随时拉取兜底 |

要点：

1. 任务层**不会解析**后端的百分比/进度行（当前后端也没有这种输出），不做假进度；UI 进度条建议按"阶段数"打点，用 `task.stage.log` 填充日志窗（高频，需限流）。
2. 失败时 `task.stage.finished` 的 `errorCode` 优先取后端输出中最后一条 `"code":"QS-..."`（compact_em 契约），否则为 `QS-TL-MOD-EXEC`；gds2geo 无 QS 码，固定 `QS-TL-MOD-EXEC` + 退出码（2=输入/配置，5=执行）。
3. 完整错误对象字段（severity/message_user_zh/user_action/trace_id 等）由 compact_em 自己写在 stderr 与 `Result/logs/compact_em/<exe>-<trace_id>.jsonl`（DATA_LAYOUT 规范位置；当前 compact_em 2026.10.22 版仍写旧 `Result/Logs/`，过渡期两处都读），任务层不重组，只透传文本并提取 code；UI 若要展示中文提示，从 `task.stage.log` 里对应 JSON 行解析，或让 UI 直接读该 jsonl。
4. gds2geo 的日志由任务层经 `--runtime-dir` 写到算例目录的 `Result/logs/gds2geo/`（符合 DATA_LAYOUT R2）；V1.1 以系统命名互斥量控制并发、不创建 `.lock` 文件（不得与旧版文件锁程序混用同一算例）；任务层的 `task.log`/`state.json` 在 `Result/tasks/<taskId>/`。

---


## 六、错误码

### 6.1 RPC 层（`error.code`）

| code | 含义 | UI 建议 |
|---|---|---|
| -32000 | 已取消 | 提示用户已取消 |
| -32001 | 任务不存在 | 刷新任务列表 |
| -32002 | 已有任务在运行 | 禁用"开始"按钮，等 `task.finished` |
| -32003 | 输入非法 | 展示 `error.data` 里的 `issues` |
| -32004 | 配置错误（modules.json/pipelines.json 加载失败） | 提示部署不完整，看 stderr |
| -32005 | 模块执行失败 | 展示 `errorCode` |

### 6.2 阶段层（通知里的 `errorCode`）

机器按 `errorCode` 分支，**不要解析文本**。格式为 `QS-TL-<域>-<错误名>`，后端模块自身的错误码会原样透传。

| errorCode | 含义 |
|---|---|
| `QS-TL-INPUT-MISSING` | 必需输入缺失（`errorMessage` 含具体路径） |
| `QS-TL-MOD-NOTFOUND` | 后端程序未找到（部署路径不对） |
| `QS-TL-MOD-EXEC` | 后端程序非 0 退出 |
| `QS-TL-OUTPUT-MISSING` | 宣告成功但没有产出文件 |
| `QS-TL-MOD-TIMEOUT` | 阶段超时 |
| `QS-TL-CANCEL` | 用户取消 |
| `QS-TL-DEP-BLOCKED` | 上游失败导致跳过 |
| `QS-TL-DEP-SKIPPED` | 上游被跳过导致跳过 |
| `QS-TL-SKIP-NOINPUT` | 可选阶段自身输入缺失 |
| `QS-TL-SEL-UNKNOWNSTAGE` | 执行选择/stages 含流水线中不存在的阶段（或续跑的历史阶段不在当前流水线） |
| `QS-TL-SEL-DEP-MISSING` | 未选中上游产出在盘缺失（创建前预检或阶段内复检；`errorMessage` 含具体路径） |
| `QS-TL-ASSUMED-DONE` | 假定完成标记（非错误：执行选择下未选中阶段/续跑信任阶段不执行，直接按在盘产出记账） |
| `QS-TL-CFG-UNKNOWNMODULE` | 流水线引用了未注册的模块（配置错误） |
| `QS-TL-CFG-DEADLOCK` | 依赖无法解析/成环，任务被强制收尾（配置错误） |
| `QS-TL-MOD-NOTCONFIGURED` | 内建动作未识别（internal 模块 command 配置错误） |
| `QS-TL-WORKER-NOTFOUND` | DLL 模块的 worker_host 宿主未找到 |
| `QS-TL-IO-JOB` | job JSON 写入失败（DLL 模块任务定义落盘失败） |
| `QS-FMT-* / QS-SOLV-* / ...` | 计算层模块自身错误码，原样透传 |

---

## 七、算例目录约定（UI 需要准备的 dataRoot）

**`dataRoot` 是唯一的数据根。** 所有后端模块——gds2geo（建模）、compact_em（电磁），
以及后续接入的任何模块——输入与输出都在这个目录里完成；任务层不会往后端程序目录写算例数据。
UI 侧只需要维护这一个目录，新增模块也不会改变这个约定。

```
dataRoot = <部署根>/cases/<项目名>/DATA
```

**`peec_basic` 的算例布局**（按 DATA_LAYOUT v1.2）：

```text
<case>\DATA\
├── Model\        主网格 *.msh（恰好一份）、matdb.json、source_em.json（规范名，v1.2）
│                 + boundary_em.json（预留）；平台预检仍兼容旧名 port_em.json（compact_em 不再读旧名）
├── Setting\      求解设置：set_em.json（EM，compact_em v1.3 起生效）+ set_thermal.json（SPICE 仿真设置）；v1.2 时代合并件 set.json 已废止
├── Result\
│   ├── compact_em\  模块产物区（按 stage 子目录）：extract\ solve\ post\（plot\ 目录自 v1.1 起不再生成）
│   ├── logs\compact_em\  诊断 jsonl
│   └── tasks\<taskId>\  state.json + task.log（仅任务层写）
└── .locks\       锁文件（规范位置）
```

`source_em.json` 必须与主网格绑定（`meshFile` + `meshSha256`），改动网格后要重新生成源文件，
否则提取阶段会报文件损坏类错误（`QS-IO-FILE-CORRUPTED`）。

注意：当前交付 compact_em v1.2（2026-11-02）产物按阶段落到 `Result/compact_em/<stage>/`，
二进制实读 `source_em.json`（不再读旧名 `port_em.json`，见 4.7）。

**`gds_full` 的算例布局**（输入是版图而不是网格）：

```text
<case>\DATA\
├── Model\        输入：model.gds + model_info.csv（gds2geo V1.1 仅支持 CSV，不接受 XTF）
│                 输出（publisher 自动落位）：model_em.msh + matdb.json
├── Result\
│   ├── gds2geo\
│   │   ├── source\    源模型：model.geo / model.json / matdb.json
│   │   ├── prepared\  派生几何：em.geo / em.json / thermal.geo / thermal.json / matdb.json
│   │   ├── em\        电磁网格：model_em.msh / model_em.msh.json（receipt）/ matdb.json
│   │   └── thermal\   热网格：model_thermal.msh / model_thermal.msh.json（receipt）/ matdb.json（V1.1 起必需）
│   ├── logs\gds2geo\  日志/诊断/私有暂存（V1.1 无锁文件，系统命名互斥量）
│   └── tasks\<taskId>\  state.json + task.log
└── .locks\       锁文件（规范位置）
```

注意：gds2geo V1.1 以系统命名互斥量做并发保护、不创建锁文件（旧版文件锁位于
`Result/gds2geo/runtime/`，不得与旧版混用同一算例）；任务层自己的 `task.log` 和 `state.json` 始终在算例的
`Result\tasks\<taskId>\` 下。

---

## 八、Qt 客户端骨架（可直接抄）

```cpp
// TaskLayerClient.h
class TaskLayerClient : public QObject {
    Q_OBJECT
public:
    void start(const QString& exePath) {
        m_proc.setProgram(exePath);
        m_proc.setArguments({"serve"});
        m_proc.setWorkingDirectory(QFileInfo(exePath).absolutePath());
        m_proc.setProcessChannelMode(QProcess::SeparateChannels);   // 必须
        connect(&m_proc, &QProcess::readyReadStandardOutput, this, &TaskLayerClient::onStdout);
        connect(&m_proc, &QProcess::readyReadStandardError,  this, &TaskLayerClient::onStderr);
        m_proc.start();
    }
    void stop() {
        send("shutdown");
        m_proc.waitForFinished(10000);
        if (m_proc.state() != QProcess::NotRunning) m_proc.kill();
    }

    int runTask(const QString& pipeline, const QString& dataRoot,
                const QJsonObject& inputs = {}) {
        return send("task.run", QJsonObject{{"pipeline", pipeline},
                                            {"dataRoot", dataRoot},
                                            {"inputs", inputs}});
    }
    int query(const QString& method, const QJsonObject& params = {}) {
        return send(method, params);
    }

signals:
    void response(int id, const QJsonValue& result);
    void error(int id, int code, const QString& message, const QJsonObject& data);
    void notified(const QString& method, const QJsonObject& params);

private:
    int send(const QString& method, const QJsonObject& params = {}) {
        const int id = ++m_nextId;
        m_proc.write(QJsonDocument(QJsonObject{{"jsonrpc","2.0"}, {"id",id},
                                               {"method",method}, {"params",params}})
                         .toJson(QJsonDocument::Compact) + '\n');
        return id;
    }
    void onStdout() {
        while (m_proc.canReadLine()) {
            const QJsonObject o = QJsonDocument::fromJson(m_proc.readLine()).object();
            if (o.contains("method")) {           // 通知
                emit notified(o["method"].toString(), o["params"].toObject());
            } else if (o.contains("error")) {     // 错误响应
                const QJsonObject e = o["error"].toObject();
                emit error(o["id"].toInt(), e["code"].toInt(),
                           e["message"].toString(), e["data"].toObject());
            } else {                              // 成功响应
                emit response(o["id"].toInt(), o["result"]);
            }
        }
    }
    void onStderr() { qWarning() << "[task_layer]" << m_proc.readAllStandardError(); }

    QProcess m_proc;
    int m_nextId = 0;
};
```

---

## 九、对接注意事项（容易踩的坑）

1. `dataRoot` 传绝对路径；路径里可以用 `/`，任务层会按平台转换分隔符。
2. 一条 `task.run` 只负责启动；完成判定看 `task.finished`，不要同步等待。
3. 同一进程不支持并发任务，UI 要么串行，要么为每个任务起一个 `task_layer.exe` 进程。
4. `task.stage.log` 频率高，UI 侧要做缓冲/限流，避免日志窗卡死主线程。
5. 退出前发 `shutdown`；直接杀进程后端程序可能残留。
6. 后端程序报的错是**中文文本 + 错误码**，UI 用错误码分支，文本只用于展示。
7. 进度只有"阶段级"：计算层没有百分比进度接口，任务层也不做假进度；详见"五（补充）"节。
8. 部署到中文目录没问题，但不要把 `config\` 改名或拆散。

---

## 十、联调自查

```powershell
# 1) 任务层自检（工作目录 = dist\）
.\task_layer.exe modules
.\task_layer.exe pipelines
# 2) 算例自检（用交付包自带 cases，或按"七"自建的算例目录）
.\task_layer.exe run --pipeline gds_full --data-root <包根>\cases\demo\DATA
```

`rpc_smoke.py` 走完 ping / modules.list / pipelines.list / inputs.validate /
task.run / task.finished / task.status / task.result 八项断言并打印 `RESULT: PASS`，
UI 侧可用它先确认部署没问题，再接自己的进程封装。
