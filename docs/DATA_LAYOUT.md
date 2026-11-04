# PEEC Studio 数据布局规范（DATA_LAYOUT）

v1.4 修订 1 · 2026-11-04 · 适用：所有计算层模块与 UI、task_layer。路径与目录的唯一事实源；manifest 中所有 path 以本文件为基准；冲突时以本文件为准。
变更（v1.2）：`port_em.json` → `source_em.json`、`port_thermal.json` → `source_thermal.json`；新增预留件 `boundary_em.json`、`boundary_thermal.json`（均 UI 输入）；模块包不再携带 examples//templates/（案例改为包外单独交付，见 ONBOARDING §1.1）。
变更（v1.3）：求解设置拆分为 `Setting/set_em.json`（EM 字段，compact_em 消费）与 `Setting/set_thermal.json`（热对象，热模块消费）；原合并件 `set.json` 为 v1.2 时代格式（见 §八）。生效条件：thermal ≥1.0.2 与 compact_em ≥v1.3 重发版集成起（模块按各自 manifest consumed 声明读取，平台代码不改）。
变更（v1.4）：热侧设置定形按 thermal 1.0.5 重发版（2026-11-02，按用户最新指示实施，优先于 v1.3 热侧稿）：`Setting/set_thermal.json` 为 SPICE 仿真设置（analysis/probe，§八.1）；`thermal` 对象移入 `Result/thermal/thermal_extract_control.json`（提取控制，暂存于此、拆分位置待定）；`Model/boundary_thermal.json` 为必需输入（§八.2）；OP0 默认路径 `Result/thermal/post/thermal.op0`。EM 侧 `set_em.json` 改名不变（compact_em ≥v1.3 重发版待到位）。
变更（v1.4 修订 1，2026-11-04，布局不变、交付细化）：① 模块交付包增**设置默认件**：compact_em 包根携 `set_em.json`、thermal 包根携 `set_thermal.json`（内容 = 该版本默认求解/仿真设置；模块二进制不消费——二进制读 dataRoot 的 Setting/ 同名文件；供平台/UI 新建算例与加载设置页取默认值；2026-11-04 会议定，自各家下一版发版起）；② peec_basic 流水线移除 plot 阶段（S 参数绘图由前端完成；平台 2026-11-04 起不跑 plot，`Result/compact_em/plot\` 不再生成；compact_em v1.3 重发版起不再提供 plotter exe）。

## 一、总则

1. **dataRoot 单根**：一个算例的全部输入/配置/产物/日志/锁在同一数据根下。位置与目录名由 UI/用户定（约定：目录名=项目名，禁空格特殊字符）。
2. **安装目录只读**：模块不得向 EXE 所在目录树写任何运行数据。
3. **所有权即隔离**：每个目录/文件唯一写者（见 §四表）；跨模块数据交换只经"契约工件 + publisher 落位"，禁止读写他人目录。
4. **相对路径**：本文所有路径相对 dataRoot（POSIX 风格）；实际使用时 task_layer 展开为绝对路径传给模块 CLI。

## 二、目录结构

```
<dataRoot>\
├── Model\            【输入与共享资产】跨任务持久
│   ├── model.gds             [UI]       版图（UI 选取放入，固定名）
│   ├── model_info.csv          [UI]       工艺（固定名；gds2geo V1.1 仅接受 CSV，不支持 XTF）
│   ├── <主网格>.msh           [gds2geo]  EM 主网格正式件（Model 下 *.msh 恰好一份）
│   ├── matdb.json            [gds2geo]  材料库正式件
│   ├── source_em.json         [UI]       EM 源定义（compact_em 消费；v1.1 名 port_em.json）
│   ├── source_thermal.json    [UI]       热源定义（热模块消费；v1.1 名 port_thermal.json）
│   ├── boundary_em.json       [UI]       EM 边界定义（预留，格式 TBD，暂无消费方）
│   └── boundary_thermal.json  [UI]       热边界定义（热模块消费，格式见 §八.2；
│                                             2026-11-02 前内嵌于 source_thermal.json）
├── Setting\          【配置】跨任务持久
│   ├── set_em.json           [UI]       EM 求解设置（compact_em 消费；v1.3 起）
│   └── set_thermal.json      [UI]       SPICE 仿真设置（analysis/probe；热模块消费；thermal 1.0.5 起）
│                                 （v1.2 时代为合并单件 set.json，格式与作用域见 §八）
├── Result\
│   ├── gds2geo\        【模块产物区：gds2geo 独占】
│   │   ├── source\    model.geo / model.json / matdb.json
│   │   ├── prepared\  em.geo / em.json / thermal.geo / thermal.json / matdb.json
│   │   ├── em\        model_em.msh（工作副本）/ model_em.msh.json / matdb.json
│   │   └── thermal\   model_thermal.msh / model_thermal.msh.json / matdb.json（必需）
│   ├── compact_em\     【模块产物区：compact_em 独占】
│   │   ├── extract\   model.peec / stage_receipt.json
│   │   ├── solve\     S_parameter.csv / stage_receipt.json
│   │   ├── plot\      *.svg（平台 2026-11-04 起不再跑此阶段；目录不再生成，旧算例可残留）
│   │   └── post\      Source.msh / Post_Model.txt / Post_Source.txt（可选）
│   ├── thermal\       【模块产物区：thermal 独占】
│   │   ├── thermal_extract_control.json  [UI 初始值] 提取控制（暂存于此，1.0.5 起，§八.1）
│   │   ├── extract\  ThermalNetList.sp / ThermalNetList_RC.sp / thermal_nodes.csv /
│   │   │              thermal_K.mtx / ThermalTemperatureMap.csv / extraction_report.json /
│   │   │              thermal_binding.json / stage_receipt.json
│   │   └── post\      VolumeTemperatureAtOperatingPoint.msh / NodeTemperatureAtOperatingPoint.csv /
│   │                   MaximumTemperature.txt / TemperatureProcessReport.txt /
│   │                   thermal.op0 / post_binding.json / stage_receipt.json
│   ├── logs\           【模块诊断日志】按模块子目录隔离
│   │   ├── gds2geo\   gds2geo.log / geo_prepare.log / ...
│   │   ├── compact_em\<exe>-<trace_id>.jsonl
│   │   └── thermal\   extract-<trace_id>.jsonl / post-<trace_id>.jsonl
│   └── tasks\          【任务编排记录：task_layer 独占，模块不写不读】
│       └── <taskId>\  state.json / task.log / job.<stageId>.json
└── .locks\         【锁集中处】<providerId>.*.lock
```

注：
- 模块交付包**只含四件套**（manifest / exe / README / ERROR_CODES）**+ 设置默认件**（compact_em: `set_em.json` / thermal: `set_thermal.json`，放包根；2026-11-04 起；内容 = 该版本默认求解/仿真设置，模块二进制不消费，供平台/UI 加载默认值；变更 v1.4 修订 1 ①），不含输入文件、DATA/ 或 examples//templates/：UI 选取的输入直接写入 `<dataRoot>\Model\`，自测案例改为**包外案例包单独交付**（ONBOARDING §1.1），平台方归档 `dev/cases/<模块>/`。
- `Result/tasks/` 下的 `state.json` 是 **task_layer 的任务编排记录**（阶段状态/errorCode/outputs/providers 版本快照），模块不写、不读、不依赖其内容做调度判断。详见 §七。

## 三、分区规则

| # | 规则 |
|---|---|
| D1 | `Result/<providerId>/` 模块独占；providerId = manifest 的 id（= 模块包目录名，小写） |
| D2 | 模块内二级目录 = 子阶段名（manifest.stages[].id），一一对应 |
| D3 | `Model/` 是唯一跨模块共享区；源/边界文件按链路命名（source_em.json / source_thermal.json，boundary_em.json / boundary_thermal.json），各消费方只认自己那份；热边界条件一律写 boundary_thermal.json，不得内嵌 source_thermal.json（格式见 §八.2） |
| D4 | 模块诊断日志 → `Result/logs/<providerId>/`；任务编排记录 → `Result/tasks/`（task_layer 所有） |
| D5 | 锁机制按模块：compact_em / thermal 用 `.locks/` 文件锁（命名 `<providerId>.*.lock`，锁文件存在不代表正在运行，陈旧锁由模块自行处理）；gds2geo V1.1 用系统命名互斥量、不创建锁文件（不得与旧版文件锁混用同一算例）。运行中禁止手工干预 |
| D6 | 模块阶段自证文件统一命名 `stage_receipt.json`（可选，放各自阶段目录内）；取代历史上的 extract.state / solve.state |
| D7 | **publisher 机制**：正式件落位（如 `Result/gds2geo/em/model_em.msh` → `Model/model_em.msh`）由 **task_layer 统一执行**（按 manifest `publishTo` 声明复制）。模块只负责把阶段产物正确写到 `Result/<providerId>/<stageId>/`，**不自己往 Model//Setting/ 搬文件** |

## 四、所有权表

| 目录/文件 | 写者 | 读者 |
|---|---|---|
| `Model/model.gds`、`model_info.*` | UI | gds2geo |
| `Model/*.msh`、`matdb.json` | task_layer publisher（源：gds2geo） | compact_em、UI（渲染） |
| `Model/source_em.json` | UI | compact_em（sha 校验） |
| `Model/source_thermal.json` | UI | 热模块（1.0.1 起消费；meshFile 指向热网格实际落位路径） |
| `Model/boundary_em.json` | UI | 预留（暂无消费方） |
| `Model/boundary_thermal.json` | UI | 热模块（边界条件，**必需输入**；格式见 §八.2；thermal 1.0.5 起经 --boundary 传入、SHA 入内部绑定；1.0.1 时代内嵌于 source_thermal.json） |
| `Setting/set_em.json` | UI | compact_em（v1.3 起；v1.2 时代读合并件 set.json） |
| `Setting/set_thermal.json` | UI | 热模块（SPICE 仿真设置；thermal 1.0.5 起，格式见 §八.1） |
| `Result/thermal/thermal_extract_control.json` | UI | 热模块（提取控制：体域/薄层 = 原 thermal 对象；1.0.5 起暂存模块产物区，程序不覆写，拆分位置待定） |
| `Result/gds2geo/**` | gds2geo | 其下游阶段、task_layer publisher |
| `Result/compact_em/**` | compact_em | 其下游阶段、UI |
| `Result/logs/<providerId>/` | 对应模块 | 排错 |
| `Result/tasks/` | task_layer | 排错、UI |
| `.locks/` | 各后端 | —（禁手工操作） |

## 五、CLI 规则（R1-R3）

| # | 规则 | 说明 |
|---|---|---|
| R1 | **数据根入口**：每个可执行单元必须有一个参数指定 dataRoot（推荐名 `--data-root`；允许别名但须在 manifest 的 `cli.dataRootArg` 字段登记）。其读取的所有相对路径按 dataRoot 解析。**禁止**按 EXE 所在目录解析数据路径 | 已有散参数设计（如 gds2geo 的 --gds/--tech/--output-dir）可通过 argsTemplate 显式传绝对路径适配，不必强行改 CLI |
| R2 | **运行痕迹出口**：日志/私有锁/临时文件经 `--runtime-dir` 指定（task_layer 统一指到 `<dataRoot>/Result/logs/<providerId>/`）；不支持该参数的模块日志固定落此处 |
| R3 | **安装目录只读**：安装目录完全只读时可正常工作（静态链接，不依赖安装目录内 DLL） |

**历史兼容命令约束**：
- 无参模式（双击默认）、EXE 旁 input//output/ 缺省回退、gds2geo_setting.json 等历史行为：**交付版中废弃**（不再作为文档化接口）；
- 若二进制仍保留这些代码路径（未物理删除），manifest 可在 `cli.deprecatedModes` 数组中声明（如 `["no-arg-default", "exe-relative-fallback"]`），task_layer 注册期给出警告；
- 约束范围仅限**数据路径解析**；纯功能性兼容入口（如 gds2geo 的 `convert` 子命令）不受影响，但须单独登记为独立 stage。

## 六、命名与不变式

固定名工件（名字定死，模块不得改）：`model.gds`、`model_info.csv/.xtf`、`matdb.json`、`source_em.json`、`source_thermal.json`、`boundary_em.json`、`boundary_thermal.json`、`set.json`（v1.2 时代）/ `set_em.json` / `set_thermal.json`（v1.3 起）、`thermal_extract_control.json`（Result/thermal/，thermal 1.0.5 起）、`stage_receipt.json`。
模块内部自由区：`Result/<providerId>/<stageId>/` 之内中间产物命名自定，须以 internal ref 登记（MANIFEST_SPEC §五）。

| # | 不变式 | 违反时报 |
|---|---|---|
| I1 | peec_basic 入口时 `Model/*.msh` 数量 == 1 | QS-FMT-INPUT-AMBIGUOUS / 预检 warning |
| I2 | 建模链产物坐标单位 = mm（set_em.json 的 DIM=1000 依赖此条；v1.2 时代为 set.json） | 模块自检 |
| I3 | `source_em.json.meshSha256` == 当前主网格实际 SHA-256（过渡期旧名 port_em.json 同样适用） | QS-IO-FILE-CORRUPTED / 预检 |
| I4 | receipt schemaVersion 在消费者 accept 区间内 | 消费者自报 QS-FMT-* |
| I5 | 锁文件存在 ≠ 错误；运行中不得删除 `.locks/` 内文件 | — |
| I6 | 求解设置文件（按消费者 manifest consumed 声明：set_em.json / set_thermal.json；v1.2 时代为 set.json）存在且 schemaVersion 在消费者 accept 区间内 | 消费者自报 / 预检 |

## 七、状态文件职责划分（消除双源）

| 文件 | 写者 | 用途 | 模块是否感知 |
|---|---|---|---|
| `Result/tasks/<taskId>/state.json` | **task_layer** | 任务编排记录：阶段状态机（pending/running/done/failed/skipped）、errorCode、outputs 清单、providers 版本快照、时间戳。UI 通过 task.status/task.result API 读取 | **否**。模块不写、不读、不依赖 |
| `Result/<providerId>/<stageId>/stage_receipt.json` | **模块**（可选） | 模块自证：本阶段执行摘要（输入指纹、输出指纹、关键参数、耗时）。排错与复现用 | 是。模块自愿写；task_layer 不解析内容、不校验、不作为调度依据 |

规则：
1. **调度事实只有一个来源**：task_layer 的 state.json。模块退出码 + task_layer 浅检决定阶段成败，receipt 不影响调度；
2. receipt 是**增强信息**（有更好，没有不影响链路）；写了就按 §七 schema 写，不写也合法；
3. 模块**禁止**写 `Result/tasks/` 下任何文件；
4. 模块**禁止**读其他任务的 state.json 做判断（每次调用是独立的）。

### stage_receipt.json schema（v1，模块可选遵循）

```jsonc
{
  "schemaVersion": 1,
  "provider": "compact_em",        // providerId
  "stage": "extract",              // stage id
  "startedAt": "2026-10-30T14:30:00+08:00",
  "finishedAt": "2026-10-30T14:31:22+08:00",
  "exitCode": 0,
  "inputs": {                      // 本阶段实际消费的工件指纹
    "Model/model_em.msh": { "sha256": "ab12...", "size": 50436 },
    "Setting/set.json": { "sha256": "cd34..." }
  },
  "outputs": {                     // 本阶段产出的工件指纹
    "Result/compact_em/extract/model.peec": { "sha256": "ef56...", "size": 102400 }
  },
  "params": { "export": "post,report" },   // 关键运行参数（人可读）
  "notes": []                       // 可选备注（warning 级别信息）
}
```

写入时机：阶段成功或失败后、进程退出前写入（失败时也写，exitCode 非 0）。
失败恢复：写 receipt 失败**不改变**退出码（receipt 是增强信息，丢了不阻塞）；stderr 打一条 warning 即可。

## 八、求解设置（set 文件）格式（取代 set.txt）

**v1.3 起按链路拆分**：EM 侧为 `Setting/set_em.json`（下文 EM 字段样例，compact_em 消费；compact_em ≥v1.3 重发版起生效）；热侧按 thermal 1.0.5 重发版定形（v1.4）为两个文件：`Setting/set_thermal.json`（SPICE 仿真设置）+ `Result/thermal/thermal_extract_control.json`（提取控制 = thermal 对象，暂存于此），格式见 §八.1。v1.2 时代（compact_em v1.2 / thermal 1.0.1 读合并件 `Setting/set.json` = EM 字段 + `thermal` 对象）在重发版集成前继续有效。以下样例为 `set_em.json` 内容（= v1.2 时代 set.json 的 EM 字段部分）：

```jsonc
{
  "schemaVersion": 1,
  "Solver_SET": 0,
  "MNA_WEIGHT": 1,
  "FS": "1e9",
  "FE": "10e9",
  "N_FP": 100,
  "DIM": 1000,
  "Taylor": 0,
  "SAVE_BIN": 1,
  "SAVE_TXT": 0,
  "SAVE_SP": 0,
  "POST_PROCESSING": 1,
  "POST_FREQ_INDEX": -1,
  "POST_PORT": 1,
  "POST_PLOT_J": 1,
  "POST_PLOT_QE": 1,
  "POST_PLOT_VE": 1,
  "POST_FIELD": 1
}
```

- 键名与原 set.txt 一致（大写蛇形），值为 JSON number/string/bool；
- `schemaVersion` 必填；新增键向后兼容（消费者忽略未知键），删键/改语义 → schemaVersion +1；
- 过渡期：compact_em 下一版 CLI 同时接受 JSON 设置（新）与 `set.txt`（旧，deprecated）；两个文件同时存在时 JSON 优先并打 warning；
- 设置文件样例随**平台交付包的 `templates/` 目录**与案例包提供（ONBOARDING §1.1）；模块包不携带模板（v1.1 的 defaultTemplate 机制从未实现，一并废止）。

### 八.1 热侧设置文件格式（set_thermal.json + thermal_extract_control.json，thermal 1.0.5 起）

**`Setting/set_thermal.json`（SPICE 仿真设置，用户交互产生）**：

````jsonc
{
  "schemaVersion": 1,
  "analysis": "op",            // 当前仅支持 op；其他类型明确报错，瞬态暂不支持
  "probe": [ "V(*)" ]          // V(*) = 全部节点（完整温度可视化须保留）；或 V(Tn) 单节点
}
````

- 提取程序将其写入两份网表的 `.op` 与 `.probe` 指令（末尾 `.end` 由程序生成）；后处理校验设置指纹——仿真设置更改后需重新生成网表并使用相应求解结果；
- 文件是输入，程序不覆写；默认路径 `Setting/set_thermal.json`（CLI `--simulation-settings` 可指别处）。

**`Result/thermal/thermal_extract_control.json`（提取控制，暂存于此）**：内容为原 `thermal` 对象（v1.2 时代合并件 set.json 去掉全部 EM 字段）：

````jsonc
{
  "schemaVersion": 1,
  "thermal": {
    "name": "demo1_4.1",                       // 可选标识
    "selection_id_type": "elementary_entity",  // 固定值（实体选择一律 Elementary ID）
    "mesh_length_scale_to_m": 0.001,           // 网格坐标 → m 比例（mm 网格 = 0.001）
    "tetrahedral_domains": [ { "elementary_volume": 4,
      "material": { "dimension": "volume", "physical_material_id": 3 } } ],
    "thin_layers": [ { "name": "upper",
      "visualization_volume": 2,
      "reference_surfaces": [ 3, 10 ],          // 必须完整铺满（覆盖性检查）
      "reference_position": "bottom",           // bottom | top
      "thickness_m": 5e-07,
      "material_footprints": [ { "surfaces": [ 3 ],
        "material": { "dimension": "surface", "physical_material_id": 4 } } ] } ]
  }
}
````

- 默认路径 `Result/thermal/thermal_extract_control.json`（CLI `--settings` 可指别处）；程序不覆写；
- **暂存说明**：1.0.5 按用户最新指示将该文件置于模块产物区（Result/thermal/），后续拆分/迁移位置由用户再定，届时本规范升版本；
- **版本沿革**：v1.3（2026-11-02 上午稿）曾将 set_thermal.json 定义为 thermal 对象独立件；thermal 1.0.5 重发版（2026-11-02）按用户最新指示将 set_thermal.json 改为 SPICE 仿真设置、thermal 对象移入 thermal_extract_control.json，本条（v1.4）按 1.0.5 实况定形；
- v1.2 过渡期作用域规则（纯热算例 set.json 不复制 EM 字段）自 1.0.5 起随热侧定形取消：热链 set.json 不再存在，set_thermal.json 为纯仿真设置、提取控制无 EM 字段（20261102 案例 set.json 混入 EM 字段的问题已解决）。

### 八.2 `boundary_thermal.json` 格式（热边界条件，独立文件）

````jsonc
{
  "schemaVersion": 1,
  "default_external_boundary": "adiabatic",     // 未声明边界面的默认处理
  "fixed_temperature": [ { "name": "bottom_293p15K",
    "location": "global_bottom_plane",
    "surface_entities": [ 18, 22 ],              // 网格 elementary ID
    "temperature_K": 293.15 } ]
}
````

- **热边界条件（外部边界默认 + 固定温度面）一律写入 `Model/boundary_thermal.json`，不得内嵌于 `source_thermal.json`**；source_thermal.json 只含源定义（meshFile / meshSha256 / total_power_W / sources 等）。20261102 交付的 source_thermal.json 内含 `default_external_boundary` 与 `fixed_temperature` 字段，重发版时移出（见 REPACK/thermal.md 第 1 轮）；
- 热链算例必须提供 boundary_thermal.json（至少声明 default_external_boundary）；热模块经 CLI `--boundary` 读取（固定名 Model/boundary_thermal.json）；
- `boundary_em.json` 仍为预留件（格式 TBD、暂无消费方），与 boundary_thermal.json 互不包含内容；
- `fixed_temperature.surface_entities` 与 sources 的实体选择同为 Elementary ID（不是 Physical ID）。

## 九、禁止事项

1. 写非自己声明的目录（拦截码 `QS-TL-CFG-LAYOUT-VIOLATION`）；
2. 代码硬编码其他模块的目录名/providerId 字符串；
3. 假设 dataRoot 之外路径存在；
4. 交付包携带可读写 `DATA/` 或测试案例（`examples/` 等）——案例包外单独交付（ONBOARDING §1.1）;
5. UI 改写 `Result/` 内文件（UI 只写 `Model/`、`Setting/` 中归它的文件）；
6. 模块写 `Result/tasks/` 下任何文件；
7. 模块自行往 `Model/`、`Setting/` 复制正式件（publisher 职责归 task_layer）。

## 十、版本

本规范带版本（当前 v1.4），manifest 的 `capabilities.dataLayout` 声明遵循版本。破坏性变更（移动契约件/改固定名/改 set 格式）→ 版本 +1，窗口期内全模块适配；新增模块或自由区目录不升版本。
版本记录：v1.1（2026-10-30）→ v1.2（2026-11-02）：port_* → source_* 改名、新增 boundary_* 预留件、包内 examples//templates/ 改为包外案例包交付 → v1.3（2026-11-02）：Setting/set.json 拆分为 set_em.json + set_thermal.json；自 thermal ≥1.0.2 与 compact_em ≥v1.3 重发版集成起生效（模块读取由 manifest consumed 驱动，平台代码不变；v1.2 时代模块继续读合并件 set.json） → v1.4（2026-11-03）：热侧设置按 thermal 1.0.5 重发版定形（set_thermal.json = SPICE 仿真设置；thermal 对象 → Result/thermal/thermal_extract_control.json 暂存；boundary_thermal.json 必需；OP0 默认路径 Result/thermal/post/thermal.op0）；按用户最新指示实施，优先于 v1.3 热侧稿（§八.1）；EM 侧 set_em.json 不变（compact_em v1.3 重发版待到位）。

v1.2 修订 2（2026-11-02 下午，布局不变、契约细化）：① 定义 boundary_thermal.json 格式（§八.2），热边界条件移出 source_thermal.json（thermal 重发版起落地）；② 明确 set.json 作用域（§八.1）：纯热算例不携带 EM 字段；③ 目录树补 Result/thermal 产物区与 logs/thermal 日志行；④ gds2geo 热网格标记为必需（V1.1 起）；⑤ 工艺文件固定为 model_info.csv（gds2geo V1.1 仅接受 CSV，XTF 不再支持）；⑥ D5 锁机制按模块细化（gds2geo V1.1 为系统命名互斥量、无锁文件）。

v1.4 修订 1（2026-11-04，布局不变、交付细化）：① 模块交付包增设置默认件（compact_em 包根 `set_em.json` / thermal 包根 `set_thermal.json`，内容 = 该版本默认求解/仿真设置；模块二进制不消费，供平台/UI 加载默认值；2026-11-04 会议定，自各家下一版发版起）；② peec_basic 移除 plot 阶段（S 参数绘图交前端；平台不再生成 `Result/compact_em/plot\`；compact_em v1.3 重发版起不再提供 plotter exe）。
