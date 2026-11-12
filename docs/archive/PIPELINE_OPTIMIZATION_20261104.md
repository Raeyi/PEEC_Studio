# Pipeline 编排优化设计：执行选择 / 阶段复用 / 客户自定义 / 用户闸门

> **状态**：设计基线（待开发）。本文档为跨对话开发交接材料，只出方案，未动代码。
> 日期：2026-11-04 · 范围：task_layer 编排层（pipeline 定义 + 执行语义 + UI API）。
> **不动**：模块 manifest 契约、publisher 落位机制、D7 所有权表、模块 timeout 语义。
> 关联活契约：`docs/DATA_LAYOUT.md`（v1.4）/ `docs/MODULE_MANIFEST_SPEC.md`（v1.2）/ `task_layer/UI_API.md`。
> 代码锚点基于 2026-11-04 代码库，行号可能漂移，**以函数名为准**。

---

## 一、背景与需求

### 1.1 客户诉求

| # | 诉求 | 一句话 |
|---|---|---|
| R1 | 部分 pipeline 不需要前端一键跑完整链路 | 需要"只跑部分 stage / 断点续跑"，而不是全有全无 |
| R2 | 不同 pipeline 的 stage 复用 | 热/电建模分开，求解器 stage 可能一致（同一个求解 exe），后处理再分开 |
| R3 | 某 stage 跑完后，UI 侧交互输入的信息传给下一个 stage 的 exe | 需要 pipeline 内部的"人机交互点"（human-in-the-loop） |

### 1.2 结论速览

| 诉求 | 现状 | 方案 | 期 |
|---|---|---|---|
| R1 | `task.run` 全有全无；无 resume | **执行选择 selection**（full / stages / resume） | P1 |
| R2 | 同模块可被多条 pipeline 引用（能跑），但接线配置（params/artifacts/requires）逐 pipeline 重复、无法按链差异化参数 | **阶段库 + ref 组合**（pipelines.json schemaVersion 2） | P2 |
| R3（文件交互，pipeline 边界） | **满足**（现有 idiom：pipeline 边界 + 文件媒介，见 §2.4） | 不新增；P1 的 selection/resume 让体验更顺 | — |
| R3（pipeline 内部暂停等 UI） | **不满足**（无等待状态、无输入通道、无闸门，见 §2.5） | **user gate**（WaitingInput 状态 + task.provideInput） | P1.5 |
| 客户自定义 pipeline 组合 | 部署时定死，改集合需重部署 | **pipelines.local.json** 部署级自定义层 | P3 |
| 附带缺陷 | `module.run` 丢失 pipeline stage 的 artifacts 路由（单阶段执行半残） | 从阶段库/内置 stages 取默认路由 | P3 |

### 1.3 文档使用方式

- P1 / P1.5 / P2 / P3 每期**独立可交付、独立验收**（§6）；P1 先做（不改配置格式），P1.5 紧随，P2 改配置 schema，P3 加自定义层。
- 每期完成后：三条内置 pipeline（gds_full / peec_basic / thermal_basic）的 E2E 与**数值基线必须逐字节不变**（复用 `docs/REPACK/*.md` 的验收基线机制：standard 案例 `S_parameter.csv` 101 行等）。
- 实现细节若与本文冲突：以本文为准更新本文（设计基线文档，增量管理），不要静默偏离。

---

## 二、现状盘点

### 2.1 三层配置架构（现状，保持该分工）

| 层 | 文件 | 职责 | 所有者 |
|---|---|---|---|
| 路由表 | `task_layer/config/modules.json`（schemaVersion 2） | `moduleId` → (provider 模块包, manifest stage, searchDirs, env, workerHost) | 平台 |
| 模块契约 | 各包 `module.manifest.json` | CLI 参数（args/allowedArgs）、consumed/produced、publishTo、errorContract、timeout | 模块团队（vendor） |
| 流水线 | `task_layer/config/pipelines.json`（schemaVersion 1） | 静态 DAG：stage = `{id, module, dependsOn, optional, artifacts, requires, params}` | 平台 |

关键类：`PipelineLoader`（解析 + DAG 校验）、`TaskOrchestrator`（DAG 调度 pump）、`ModuleRunner`（单阶段执行 + publisher）、`App`（RPC 面 + 任务装配）、`ResultStore`（state.json）。

### 2.2 已有的弹性（不要重复建设）

- DAG 动态调度、支持并行分支（gds_full 的 em/thermal 并行）、`maxParallel`；
- `optional` + `requires`：**数据驱动**自动跳过（输入缺失 → Skipped，不阻断主链；如 thermal.post 无 OP0）；
- `overrides[stageId]`：阶段级 CLI 参数覆盖，白名单 = manifest `allowedArgs`（非法键直接拒绝，不静默透传）；
- `module.run`：单模块执行（合成 `single:<moduleId>` 单阶段 pipeline，复用全套状态/日志/取消设施）；
- 配置驱动：pipeline 集合在 `pipelines.json`，不是编译进代码。

### 2.3 五个死点（R1/R2 的根因）

| # | 死点 | 证据 |
|---|---|---|
| H1 | **`task.run` 全有全无**：`startTask` 把 `pipeline.stages` 整个塞进 `m_pending`，无"跑子集"入口 | `TaskOrchestrator::startTask`（`m_pending = pipeline.stages`） |
| H2 | **`module.run` 丢路由**：合成的单阶段 pipeline 没有 `artifacts`，而工件路由只从传入 pipeline 的 stages 提取（`artifactOverridesOf`）→ 依赖跨模块路由的 stage（如 `thermal.extract` 需把 `user/thermal.mesh` 路由到 `Result/gds2geo/thermal/model_thermal.msh`）单独跑会读错路径 | `App::rpcRunModule` + `App::artifactOverridesOf` |
| H3 | **就绪判据是任务内的**：`isReady` 要求上游在**本次任务** Done → 无 resume 语义；solve 挂了只能整条重跑或 module.run（带 H2 缺陷） | `TaskOrchestrator::isReady` |
| H4 | **pipeline 集合部署时定死**：加/裁剪 pipeline 必须改 `config/pipelines.json` 重部署；客户/集成商无自定义组合通道 | `PipelineLoader::loadFromFile`（单文件、schemaVersion 硬校验） |
| H5 | **输入预检只查入口**：`inputs.validate` 只校验无 dependsOn 的阶段 → "跑子集"场景没有对应预检 | `App::inspectInputs`（entryModuleIds 只取 `dependsOn.isEmpty()`） |

### 2.4 R3 的现状：pipeline 边界 = 交互边界（文件媒介）

现有产品的 UI 交互点**全部**落在两条 pipeline 之间，媒介是文件（`Model/` 单写者规则下的 UI 写件）：

```
① task.run gds_full   → 结束
② UI 加载 Model/model_em.msh，用户选三角形 → UI 写 Model/source_em.json
③ task.run peec_basic → 读 source_em.json 开跑
```

文件媒介的好处：可审计（state.json 留痕）、可重跑、与 D7 所有权模型一致。
**结论：交互点在 pipeline 边界 → 现状满足；pipeline 内部暂停 → 见 §2.5 不满足。**

### 2.5 无 pipeline 内输入通道（R3 内部形态的证据）

| 维度 | 现状 |
|---|---|
| 输入注入时点 | `inputs`（动态变量 `{PORT_JSON}` 等）与 `overrides` 在 `task.run` 那一刻冻结：`PathResolver` 只在 `App::startTaskWith` 里 set 一次，编排器持有快照，运行中不可再改 |
| 阶段状态 | `Pending / Ready / Running / Done / Failed / Skipped / NotConfigured`（`ResultStore.h`）——无"等待输入"状态 |
| 调度 | `pump()` 见上游 Done 自动启动下游，中间无闸门 |
| RPC 面 | 无"提供输入"方法；运行中 UI 唯一可操作的是 `task.cancel`（API 一览：ping / modules.list / pipelines.list / inputs.validate / task.run / module.run / task.cancel / task.status / task.result / shutdown） |
| 通知面 | `task.started / task.stage.started / task.stage.log / task.stage.finished / task.finished / task.cancelled`——无"等待交互"通知 |

---

## 三、设计原则

1. **定义与执行正交**：pipeline 定义 = 静态 DAG（加载期校验）；本次跑什么 = 运行时的**执行选择（selection）**。H1 的根因是两者混在一个参数里。
2. **不造新执行引擎**：selection / 子集 / 续跑 / 闸门全部复用现有 DAG pump、状态机、日志、取消、state.json 设施；改动集中在"就绪判据"、"launchStage 一个分支"、"加载期组合"三处。
3. **三层所有权不变形**：manifest = 模块真相（CLI 契约归 vendor）；阶段库 = 平台侧接线真相（工件路由 / 前置检查 / 默认参数）；pipeline = 组合（DAG + 局部参数）。**模块 CLI 契约不进阶段库**，避免双事实源。
4. **向后兼容**：v1 内联 stage 写法继续可解析；CLI `run --pipeline` 不变；三条内置 pipeline 行为与数值基线不变。

---

## 四、方案细节

### 4.1 P1：执行选择 selection（解决 R1）

#### 4.1.1 API 变更

`task.run` 增加可选参数 `selection`（缺省 = 现状全跑）：

```json
{ "jsonrpc":"2.0","id":4,"method":"task.run","params":{
    "pipeline":"peec_basic", "dataRoot":"D:/cases/demo/DATA",
    "selection": { "mode":"stages", "stages":["solve","post"] } } }
```

`mode` 三值：

| mode | 语义 |
|---|---|
| `full`（缺省） | 现状全链，一键跑 |
| `stages` | 跑 DAG 子集（按 pipeline 内 stage **本地 id** 选） |
| `resume` | 基于上一个任务的 state.json 续跑未完成部分：`{mode:"resume", resumeTaskId:"t..."}` |

`inputs.validate` 同步增加 `selection` 参数（按选择集预检，见 §4.1.5）。

#### 4.1.2 选择规则（mode=stages）

启动前（startTask 之前）校验，失败即拒绝、不建任务：

1. 选中的 id 必须存在于该 pipeline → 否则 `QS-TL-SEL-UNKNOWNSTAGE`；
2. 对每个选中阶段 s、每个**不在选中集**的依赖 d：d 的产出契约必须已被磁盘工件满足。具体检查 = s 的 manifest `requiredInputs`（经 registry rebind 解析后）+ s 的 `requires` 逐项存在性检查（**复用 optional 阶段已有的检查逻辑**，`TaskOrchestrator::launchStage` 内 optional 分支那段）→ 缺失即 `QS-TL-SEL-DEP-MISSING`，消息列出缺什么（UI 可据此给"把上游一起勾上"的建议）;
3. 工件路由按**选中子集**计算（`artifactOverridesOf` 改为接收有效 stage 集合）——顺带消除现有"同模块两个 stage 同 ref 键 last-wins 覆盖"的隐患（全量模式保持现行为，子集模式只取选中者）。

**resume 规则**：
- 读 `resumeTaskId` 的 state.json：pipeline id 必须一致；
- 上次状态为 `Done` 的阶段视为"假定完成"（assumed done）：其 `outputs` 必须仍在盘上（存在性检查；指纹不符 → `QS-TL-DEP-STALE` **warning，不阻断**，先软后硬）；
- 上次 `Skipped`（optional 输入缺失）/`Failed`/`Pending` 的阶段重新进入待执行（optional 阶段按当前输入重新评估：输入现在有了就跑——这是期望行为）;
- resume 产生**新 taskId**（state.json 按任务隔离），`selection.resumeTaskId` 留痕。

#### 4.1.3 编排器改动（核心：最小 diff）

**推荐实现：合成"假定完成"记录**，使现有 `isReady` / `propagateBlocked` 几乎不动：

- `startTask` 时，对每个"未选中但是选中阶段依赖"的 stage，插入一条合成 `StageRecord`：`status=Done`、新字段 `assumed=true`、`errorCode="QS-TL-ASSUMED-DONE"`（标记性质）；
- `isReady` 不改（合成记录 Done → 放行）；`propagateBlocked` 不改（合成记录非 Failed/Skipped → 不阻断）；
- **必须排除合成记录的地方**：
  - `pump()` 收尾的 failedStages/blockedStages 统计（按 `assumed` 过滤）；
  - state.json / `task.result`：合成阶段**要**出现（审计可见），带 `assumed:true` 标记；UI 展示为"（来自上次/盘上工件）"。
- 防御性复检：选中集入口阶段（依赖不在选中集）在 `launchStage` 启动前再查一次输入存在性（防运行中被删）→ 失败置 Failed `QS-TL-SEL-DEP-MISSING`。

#### 4.1.4 状态与留痕（state.json）

- 新增顶层字段：`"selection": { "mode": "...", "stages": [...], "resumeTaskId": "..." }`；
- 阶段记录新增可选字段 `"assumed": true`（合成记录）；
- P3 再补 `"upstreamFingerprints"`（size/mtime；严格 sha 后置）——P1 只落 selection 与 assumed。

#### 4.1.5 预检扩展（inputs.validate）

- 接受 `selection` 参数；入口集合 = 选中阶段中"依赖不在选中集（stages 模式）/ 上次非 Done（resume 模式）"者；
- 检查内容 = 入口的 `requiredInputs` + `requires` + 现有 EM 链端口绑定预检（入口含 compact_em 模块时查 `Model/source_em.json`）；
- 返回结构不变（`valid/checked/issues/warnings`）。

#### 4.1.6 新错误码（P1）

| 码 | 级别 | 含义 |
|---|---|---|
| `QS-TL-SEL-UNKNOWNSTAGE` | 拒绝启动 | selection 含 pipeline 中不存在的 stage id |
| `QS-TL-SEL-DEP-MISSING` | 拒绝启动 / 启动前失败 | 未选上游的产出契约未满足（列缺失文件） |
| `QS-TL-ASSUMED-DONE` | 标记（非错误） | 合成记录：该阶段未在本任务执行，按盘上工件假定完成 |
| `QS-TL-DEP-STALE` | warning（P1 起记录，不阻断） | 上游工件指纹与上次记录不一致 |

### 4.2 P1.5：user gate —— pipeline 内 UI 交互（解决 R3 内部形态）

#### 4.2.1 三种交互形态（按"交互产出是什么"区分）

| 形态 | 例子 | 机制 |
|---|---|---|
| F1 产出**文件** | 选网格面 → 写 source_em.json / boundary json | 闸门等文件出现（**优先支持**：可审计、可重跑、合 D7） |
| F2 产出 **CLI 参数** | 用户确认频率范围/频点数 | `task.provideInput` 传值，走 allowedArgs 白名单 |
| F3 纯**确认** | 长时间求解前确认 | 闸门等显式确认 |

#### 4.2.2 配置：stage 级 `userGate`（可选键）

```json
{ "id":"extract", "module":"peec.model_extract",
  "userGate": {
    "waitForFiles": ["Model/source_em.json"],
    "confirm": true,
    "inputs": [ { "name":"nfp", "type":"number", "required":true, "description":"频点数" } ] } }
```

三个键按需取用（F1/F2/F3）；全部缺省 = 无闸门（现行为）。P2 之后 `userGate` 可放阶段库定义（默认）+ pipeline 本地覆盖（extends，不允许取消）。

#### 4.2.3 运行时语义

1. `StageStatus` 增加 `WaitingInput`（序列化名 `"waiting"`；`task.status` / state.json / 通知统一用该名）；
2. `launchStage` 增加一个分支（唯一调度改动点）：阶段就绪（`isReady` 通过）但闸门未满足 → 置 `WaitingInput`、**留在 `m_pending`**、发新通知后返回，不启动：

   ```
   task.stage.awaiting: { taskId, stageId, name,
     gate: { waitForFiles:[...], confirm:true, inputs:[...] },
     missing: { files:[...], confirmed:false, inputs:[...] } }
   ```

   满足条件 = `waitForFiles` 全部存在 **且** `confirm` 已确认 **且** 全部 required `inputs` 已提供；任一缺 → 继续等待，通知里给出缺什么。
3. 新 RPC `task.provideInput { taskId, stageId, values? }`：
   - 任务必须活动且该阶段处于 `WaitingInput` → 否则报错 `QS-TL-INPUT-NOTWAITING`；
   - `values` 按 `gate.inputs` 校验（required 存在、类型），且每个 (name,value) **过 manifest `allowedArgs` 白名单**（与 overrides 同一条代码路径，复用 `App` 里 overrides 白名单校验）→ 不通过整体拒绝；
   - 存任务记录（state.json `"providedInputs": { stageId: {values, ts} }`，审计）；
   - 闸门全满足 → 阶段转 `Ready`，`pump()` 启动；`values` 作为该阶段 extraArgs 追加（与 overrides 同机制拼 `--key value`）；
   - **无文件监听**：UI 写完文件后必须调 `task.provideInput`（values 可为空对象）作为唤醒事件；平台只在收到该调用时复检文件。
4. `task.cancel` 等待中 → 该阶段 Skipped（`QS-TL-CANCEL`），任务正常收尾（与 running 同等对待）。
5. UI 断连：现有 inputClosed → cancel 活动任务逻辑天然覆盖等待态。

#### 4.2.4 必须处理的集成陷阱

- **防悬挂兜底会误杀等待阶段**：现 `pump()` 逻辑是"无 running、pending 非空、`propagateBlocked` 无变化 → 判死锁，全部 pending 置 Failed（`QS-TL-CFG-DEADLOCK`）"。等待阶段留在 `m_pending` 时，这条路径会把等待阶段直接杀掉。**修复：pump 循环中，若存在 `WaitingInput` 阶段则作为合法停等条件直接 return**（类比 `m_running` 非空的停等），由 `task.provideInput` 唤醒后重新 `pump()`。这是 P1.5 最大的坑，写测试时必须覆盖（§6 验收 c/e 项）。
- **timeout 边界**：模块 `timeoutSec` 从 `ModuleRunner::launch` 起算（现有语义），等待时间**不计入**阶段超时（用户可能离开几小时）；任务级 idle 上限 v1 **不设**（产品决定，文档化）。
- `task.finished.elapsedMs` 含等待时长（如实反映墙钟，不改）。
- 等待不是终态：任务保持 busy（`m_busy`），`task.status` 可见 `waiting` 阶段；`selection` 与 gate 交互：被 gate 的阶段可处于 selection 子集中；resume 一个"因等待被 cancel 的任务"→ 该阶段重新进入等待。
- 不改全局 `PathResolver`：`inputs` 动态变量保持 task.run 时冻结；gate 输入是**阶段级**注入（拼该阶段 extraArgs），不引入"运行中重解析全局变量"。

#### 4.2.5 新错误码 / 通知（P1.5）

| 项 | 说明 |
|---|---|
| `task.stage.awaiting`（新通知） | 参数见 §4.2.3(2) |
| `task.provideInput`（新 RPC） | 参数 `{taskId, stageId, values?}` |
| `QS-TL-INPUT-NOTWAITING` | provideInput 时任务非活动或阶段非 waiting |
| （复用）overrides 白名单错误 | values 非法键 → 与 overrides 相同的拒绝语义 |

### 4.3 P2：阶段库 + 引用组合（解决 R2）

#### 4.3.1 pipelines.json schemaVersion 2

```json
{
  "schemaVersion": 2,
  "stages": {
    "solver.mna": {
      "module": "sim.solver",
      "name": "频域求解（MNA）",
      "params": { },
      "artifacts": { },
      "requires": []
    },
    "em.model_extract": { "module": "peec.model_extract", "name": "PEEC 模型提取" },
    "thermal.extract": { "module": "thermal.extract",
      "artifacts": { "user/thermal.mesh": "Result/gds2geo/thermal/model_thermal.msh" } }
  },
  "pipelines": {
    "peec_basic": {
      "displayName": "电磁基本链路（PEEC 三段）",
      "description": "...",
      "maxParallel": 1,
      "stages": [
        { "id":"extract", "ref":"em.model_extract", "dependsOn":[] },
        { "id":"solve",   "ref":"solver.mna", "dependsOn":["extract"], "params":{ "nfp":100 } },
        { "id":"post",    "ref":"em.post",    "dependsOn":["solve"], "optional":true }
      ]
    }
  }
}
```

#### 4.3.2 解析规则

- pipeline 的 stage 条目：`ref`（库引用）与内联 `module`（v1 形态）**二选一，必居其一**；
- **物化**（loader 内完成，产出与今天完全相同的 `PipelineStage` 结构体 → 编排器无感）：
  - `params` / `artifacts`：对象按键合并，**本地键赢**；
  - `requires`：本地有则用本地，否则用库定义；
  - `name`：本地有则本地，否则库定义，否则 stage id（现有兜底）；
  - `optional`：本地有则本地，否则库定义，缺省 false；
  - `dependsOn`、`id`：**永远本地的**（DAG 局部命名）；
  - `userGate`：库定义为默认，本地可 extends（新增键），**不允许本地取消库级闸门**；
- **校验**：物化后跑现有 DAG 校验原样（stage id 唯一 / 依赖存在 / 无环，`PipelineLoader::parsePipeline` 现有三段）；`ref` 必须存在（加载期报错，fail-fast）；`module` 注册存在性仍留到启动期（`QS-TL-CFG-UNKNOWNMODULE`，现状）。
- **向后兼容**：`loadFromFile` 同时接受 schemaVersion 1（全内联，现路径）与 2（混合）；≥3 拒绝。
- **迁移**：三条内置 pipeline 迁到 ref 形式；验收 = 物化结果与 v1 解析逐字段等价（见 §6 P2 验收）。

#### 4.3.3 客户场景落位（R2 验收场景）

```
EM 链:   em建模(独立ref) → solve(ref=solver.mna) → EM后处理(独立ref)
热链:    热建模(独立ref) → solve(同一ref)        → 热后处理(独立ref)
```

同一求解 exe 的调用接线（参数/路由/前置/闸门）只在 `solver.mna` 定义一次；EM/热两链经本地 `params` 覆盖实现"同一 exe、不同作业"；改接线一处生效两链。`modules.json` 中 `sim.solver`（MNA/ALPS 待接入）已预留共享求解模块位，P2 补的是其组合层。

### 4.4 P3：客户自定义分层 + module.run 路由修复

#### 4.4.1 分层

| 层 | 位置 | 谁可写 | 能力 |
|---|---|---|---|
| L0 内置 | `config/pipelines.json` | 产品 | 出厂流水线 |
| L1 部署级 | `config/pipelines.local.json`（**可选文件**） | 集成商/客户 IT | 用阶段库组合自定义 pipeline；同名 id **local 赢**（启动日志留痕，`pipelines.list` 标记 `source:"local"`）；加载期校验，错误 **fail-fast 整文件拒载**（部署级配置错误要响，不静默忽略） |
| L2 算例级 | `<dataRoot>/` | 最终用户 | **只允许** selection（P1）+ overrides/gate values（P1.5），**不允许**定义 pipeline |

L2 收紧理由：dataRoot 是用户可写区，开放 pipeline 定义会把校验面（DAG 合法性 / 工件路由越权 / publisher 目标）扩到用户输入，排查成本远超收益。要"自己的流程"→ L1；要"这次少跑两步"→ L2 selection。

#### 4.4.2 module.run 路由修复（H2）

`rpcRunModule` 合成单阶段 pipeline 时：

1. 在所有 pipeline（含 local，P3 后）的 stage 条目（物化后）中找 `moduleId` 匹配的条目；
2. **恰好 1 个** → 采用其 `artifacts` / `requires` / `params` / `userGate` 默认；
3. **0 个** → 现行为（无路由）；
4. **≥2 个** → 报错（列出候选 pipeline/stage），要求调用方传新可选参数 `stageRef` 消歧。

### 4.5 P4：预留（有真实需求再启动）

- 严格指纹闸门（`QS-TL-DEP-STALE` 从 warning 升硬拒）；
- 并发任务模型（per-dataRoot 任务实例 + `Model/` 写锁；当前进程级单任务 `m_busy` + Model/ 单写者是不变式对）；
- stage 级动态条件（`when` 表达式）——v1 明确不做，见 §5；
- UI 可视化 pipeline 编排器。

---

## 五、边界（明确不做）

1. **不引入动态条件**：v1 动态性只来自 `optional+requires`（数据驱动）与 selection/user gate（用户驱动）。DAG 保持静态、加载期可全量校验——这是"任务绝不悬挂"兜底（`pump()` 死锁分支）的前提。
2. **指纹先软后硬**：stale 只 warning；硬闸门等 state.json 数据积累一轮后再开。
3. **并行任务不在本期**：客户 R1/R2/R3 都不需要 EM/热链同时跑；要并发是另一个设计（§4.5）。
4. **模块 CLI 契约不进阶段库**：manifest 仍是 vendor 的 CLI 唯一事实源；阶段库只有平台侧接线（路由/前置/默认参数/闸门）。
5. **publisher 机制、manifest 契约、D7 所有权表、optional/requires 语义完全不动**。
6. **不改全局 PathResolver / 不引入运行中重解析**（§4.2.4）。
7. **L2 算例级不开放 pipeline 定义**（§4.4.1）。

---

## 六、分期落地与验收

| 期 | 内容 | 独立交付判据 |
|---|---|---|
| **P1** | selection（full/stages/resume）+ 合成 assumed 记录 + `artifactOverridesOf` 按子集 + inputs.validate 扩展 + state.json selection 留痕 + SEL 错误码 | ① `rpc_smoke.py` 八项断言全过（全链行为不变）；② 三条内置 pipeline E2E 数值基线逐字节对照（REPACK 基线机制）；③ 新断言：stages 模式跑后缀子集成功（上游工件在盘）；④ 缺上游 → `QS-TL-SEL-DEP-MISSING`；⑤ 未知 stage → `QS-TL-SEL-UNKNOWNSTAGE`；⑥ solve 失败后 resume 只跑尾部；⑦ state.json 含 selection 与 assumed 标记 |
| **P1.5** | user gate：WaitingInput 状态 + launchStage 分支 + pump 停等修复 + `task.stage.awaiting` + `task.provideInput` + 白名单注入 + state.json providedInputs | ① 被 gate 阶段暂停并发 awaiting（缺文件/未确认/缺值三种缺法各有断言）；② provideInput 非法值被白名单拒绝；③ 等待中 cancel → 干净收尾；④ 等待中 UI 断连 → cancel；⑤ **回归：等待不触发 `QS-TL-CFG-DEADLOCK`（§4.2.4 陷阱）**；⑥ values 确实拼进该阶段 CLI（job.json args / 模块 receipt 可见）；⑦ 全链基线不变 |
| **P2** | schemaVersion 2 阶段库 + ref 物化 + 三条内置 pipeline 迁移 | ① v1/v2 双格式解析测试：内置三 pipeline 物化结果逐字段等价；② 未知 ref → 加载期报错；③ 本地覆盖合并规则测试（params 键级合并 / requires 本地赢 / optional 本地赢）；④ 同一 solver ref 被两条链引用 + 各自 params 覆盖生效；⑤ 基线不变 |
| **P3** | pipelines.local.json + module.run 路由修复 + stale warning 落盘 | ① local 文件加载 + 同名覆盖 + 标记；② local 文件非法 → 整文件拒载（启动日志可见）；③ `module.run thermal.extract` 单独跑时 `user/thermal.mesh` 路由正确（H2 修复回归）；④ 多候选 → 报错列候选；⑤ 基线不变 |
| **P4** | 预留 | 有真实需求另立设计 |

通用红线（每期）：`run --pipeline` CLI 不变；数值基线（standard 案例 S_parameter.csv 101 行、四阶段全链）逐字节不变；单任务实例约束不变。

---

## 七、附录

### 7.1 API / 通知变更清单

| 项 | 期 | 变更 |
|---|---|---|
| `task.run` | P1 | +`selection` 参数（`{mode, stages?, resumeTaskId?}`） |
| `inputs.validate` | P1 | +`selection` 参数 |
| `module.run` | P3 | +`stageRef` 可选参数（多候选消歧）；路由修复 |
| `task.provideInput` | P1.5 | 新 RPC：`{taskId, stageId, values?}` |
| `task.stage.awaiting` | P1.5 | 新通知（参数见 §4.2.3） |
| `pipelines.list` | P3 | +`source`（"builtin"/"local"）字段 |

### 7.2 错误码清单（新增）

| 码 | 期 | 级别 |
|---|---|---|
| `QS-TL-SEL-UNKNOWNSTAGE` | P1 | 拒绝启动 |
| `QS-TL-SEL-DEP-MISSING` | P1 | 拒绝启动 / 启动前失败 |
| `QS-TL-ASSUMED-DONE` | P1 | 标记（合成记录，非错误） |
| `QS-TL-DEP-STALE` | P1（warning）/ P4（可升硬） | warning |
| `QS-TL-INPUT-NOTWAITING` | P1.5 | RPC 拒绝 |

### 7.3 state.json 字段（新增）

| 字段 | 期 | 说明 |
|---|---|---|
| `selection`（顶层） | P1 | `{mode, stages?, resumeTaskId?}` |
| stage 记录 `assumed` | P1 | true = 合成记录（未在本任务执行） |
| `providedInputs`（顶层 map stageId→{values,ts}） | P1.5 | gate 输入留痕 |
| stage 记录 `upstreamFingerprints` | P3 | size/mtime；sha 后置 |

### 7.4 关键代码锚点（2026-11-04 基线，行号可能漂移，以函数名为准）

| 位置 | 相关函数/段 | 用途 |
|---|---|---|
| `task_layer/src/orchestrator/TaskOrchestrator.cpp` | `startTask`（`m_pending = pipeline.stages`） | H1；P1 选择注入点 |
| 同上 | `isReady` / `propagateBlocked` | P1 合成记录需兼容（预期不改） |
| 同上 | `pump()`（死锁兜底分支） | P1.5 必须加"等待中停等"分支（§4.2.4 陷阱） |
| 同上 | `launchStage`（optional 分支 = requiredInputs 存在性检查） | P1 子集入口复检复用；P1.5 gate 分支插入点 |
| `task_layer/src/App.cpp` | `artifactOverridesOf` | P1 改按子集计算 |
| 同上 | `inspectInputs`（入口集合只取无依赖者） | P1 预检扩展 |
| 同上 | `rpcRunModule` | P3 路由修复 |
| 同上 | `startTaskWith`（resolver 快照 + overrides 白名单校验） | P1.5 provideInput 白名单复用同路径 |
| `task_layer/src/config/PipelineLoader.cpp` | `parsePipeline`（DAG 三段校验）/ `loadFromFile`（schemaVersion 硬校验） | P2 ref 物化 + schemaVersion 2 |
| `task_layer/src/io/ResultStore.h` | `StageStatus` 枚举 / `StageRecord` | P1.5 加 `WaitingInput`；P1 加 `assumed` |
| `task_layer/config/pipelines.json` | 三条内置 pipeline 全文 | P2 迁移对象 |
| `task_layer/config/modules.json` | `sim.solver` 等 | R2 共享求解模块位（已预留） |
| `task_layer/UI_API.md` | §四 API 一览 / §五 通知 | 随 P1/P1.5/P3 同步更新（文档跟代码同批提交） |

### 7.5 实现顺序建议（开发对话内）

1. P1 先落（不动配置格式，风险最小，R1 闭环）；
2. P1.5 紧随（与 P1 共享"阶段级注入"基础设施——extraArgs 拼装与白名单是同一套）；
3. P2（配置 schema 变更，需迁移 + 等价性测试，独立 PR）；
4. P3（依赖 P2 的阶段库做 module.run 消歧；可部分提前做 module.run 修复，从内置 stages 取路由，P2 后切到库）。
