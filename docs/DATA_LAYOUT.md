# PEEC Studio 数据布局规范（DATA_LAYOUT）

版本 v1.0 · 2026-10-30 · 适用：所有计算层模块（gds2geo / compact_em / 后续接入模块）与 UI、task_layer。
本文件是**路径与目录的唯一事实源**。模块的 `module.manifest.json` 中所有 path 字段以本规范为基准书写；
task_layer 在注册期与运行期按本规范做校验。与本文件冲突的 README 描述以本文件为准。

---

## 一、总则

1. **dataRoot 单根**：一个算例的全部输入、配置、产物、日志、锁都在同一个数据根目录下。
   dataRoot 的位置与目录名由 UI/用户决定（约定：目录名 = 项目名，禁止空格与特殊字符），本规范不规定其绝对位置。
2. **模块安装目录只读**：任何模块不得向自己的 EXE 所在目录树写入运行数据（日志、锁、中间文件一律进 dataRoot）。
3. **所有权即隔离**：每个目录/文件有唯一写者（见所有权表）。模块只能写自己声明的目录；
   跨模块数据交换只通过"契约区工件 + publisher 落位"完成，禁止直接读写他人目录。
4. **相对路径语义**：本规范中所有路径均为**相对 dataRoot** 的 POSIX 风格写法（`Result/gds2geo/em/model_em.msh`）；
   实际使用时由 task_layer 展开为系统绝对路径。模块 CLI 收到的路径参数一律是绝对路径。

## 二、目录结构（v2）

```
<dataRoot>\
│
├── Model\                                【输入与共享资产区】跨任务持久
│   ├── model.gds                          [UI]       版图（UI 选取放入，固定名）
│   ├── model_info.csv | model_info.xtf    [UI]       工艺文件（二选一，固定名）
│   ├── <主网格>.msh                        [gds2geo]  EM 主网格正式件（全局恰好一份 *.msh）
│   ├── matdb.json                         [gds2geo]  材料库正式件
│   └── port.json                          [UI]       端口绑定（meshFile/meshSha256/triangleIds）
│
├── Setting\                              【配置区】跨任务持久
│   └── set.txt                            [UI 或 compact_em 模板]  求解设置
│
├── Result\                               【产物区】可整体重建
│   │
│   ├── gds2geo\                          【模块产物区：gds2geo 独占】
│   │   ├── source\
│   │   │   ├── model.geo
│   │   │   ├── model.json
│   │   │   └── matdb.json
│   │   ├── prepared\
│   │   │   ├── em.geo / em.json
│   │   │   ├── thermal.geo / thermal.json
│   │   │   └── matdb.json
│   │   ├── em\
│   │   │   ├── model_em.msh               （工作副本；正式件经 publisher 落 Model\）
│   │   │   ├── model_em.msh.json          （receipt schema 4）
│   │   │   └── matdb.json
│   │   └── thermal\                       （可选阶段）
│   │       ├── model_thermal.msh
│   │       ├── model_thermal.msh.json     （receipt schema 5）
│   │       └── matdb.json
│   │
│   ├── compact_em\                       【模块产物区：compact_em 独占】
│   │   ├── extract\
│   │   │   ├── model.peec
│   │   │   └── state.json
│   │   ├── solve\
│   │   │   ├── S_parameter.csv
│   │   │   └── state.json
│   │   ├── plot\                          （可选）
│   │   │   └── *.svg
│   │   └── post\                          （可选）
│   │       ├── Source.msh
│   │       ├── Post_Model.txt
│   │       └── Post_Source.txt
│   │
│   ├── logs\                             【模块诊断日志】各模块按子目录隔离
│   │   ├── gds2geo\                       （--runtime-dir 统一落点）
│   │   │   └── gds2geo.log / geo_prepare.log / geo2em.log / geo2thermal.log
│   │   └── compact_em\                    （trace jsonl）
│   │       └── <exe名>-<trace_id>.jsonl
│   │
│   └── tasks\                            【任务编排记录：task_layer 独占】
│       └── <taskId>\
│           ├── state.json                 （阶段记录 + errorCode + outputs + providers 版本快照）
│           ├── task.log                   （[stageId] 前缀的转发日志）
│           └── job.<stageId>.json         （仅 DLL 模块）
│
└── .locks\                               【锁文件集中处】
    ├── gds2geo.model.lock
    └── compact_em.data.lock
```

## 三、分区规则（硬性条款）

| # | 规则 | 说明 |
|---|---|---|
| D1 | `Result/<providerId>/` 模块独占 | providerId = manifest 的 `id`（小写）。模块只能写自己名下；产物归属看目录名即知 |
| D2 | 模块内二级目录 = 子阶段名 | `Result/gds2geo/source/` ↔ manifest.stages[].id 一一对应；模块内部重构不改对外目录名 |
| D3 | `Model/` 是唯一跨模块共享区 | 用户资产 + 正式交付件必须模块无关地固定在此；除此之外禁止跨模块读写 |
| D4 | logs 与 tasks 分离 | 模块自身诊断 → `Result/logs/<providerId>/`；任务编排记录 → `Result/tasks/`（task_layer 所有） |
| D5 | 锁集中到 `.locks/` | 命名 `<providerId>.*.lock`；锁是并发保护不是目录划分依据；运行时禁止手工删除 |
| D6 | 阶段状态文件统一命名 `state.json` | 放在各自阶段目录内；取代历史上的 extract.state / solve.state 等不一致命名 |

### 所有权速查表

| 目录/文件 | 写者 | 读者 |
|---|---|---|
| `Model/model.gds`、`Model/model_info.*` | UI | gds2geo |
| `Model/*.msh`、`Model/matdb.json` | gds2geo（经 publisher） | compact_em、UI（渲染） |
| `Model/port.json` | UI | compact_em（sha 校验） |
| `Setting/set.txt` | UI 或 compact_em 模板 | compact_em |
| `Result/gds2geo/**` | gds2geo | gds2geo 下游阶段、publisher |
| `Result/compact_em/**` | compact_em | compact_em 下游阶段、UI |
| `Result/logs/<providerId>/` | 对应模块 | 排错 |
| `Result/tasks/` | task_layer | 排错、UI 查询 |
| `.locks/` | 各后端 | —（禁手工操作） |

## 四、CLI 铁律（R1-R3，对模块团队的硬性要求）

| # | 规则 | 要求 |
|---|---|---|
| R1 | **dataRoot 入口必填** | 每个可执行单元必须支持 `--data-root <dir>`（参数名统一，不允许自定义别名）；其读取的所有相对路径按 dataRoot 解析。**禁止**按 EXE 所在目录解析、禁止缺省回退到包内 input//output/ |
| R2 | **运行痕迹出口** | 日志/临时文件/私有锁通过 `--runtime-dir <dir>` 指定（task_layer 统一指到 `<dataRoot>/Result/logs/<providerId>/`）；不支持该参数的模块，日志固定落 `Result/logs/<providerId>/` |
| R3 | **安装目录只读** | 模块必须在完全只读的安装目录下正常工作（所有写操作落在 dataRoot 内）；静态链接运行库，不依赖安装目录内的 DLL |

task_layer 部署期会检查 stage 命令模板包含 dataRoot 绑定（C6 校验项），不合规的模块拒绝注册。

## 五、命名规则

1. **固定名工件**（跨模块契约件）：`model.gds`、`model_info.csv/.xtf`、`matdb.json`、`port.json`、`set.txt`——名字由本规范定死，模块不得改名；
2. **主网格唯一性**：`Model/` 下 `*.msh` 全局恰好一份（EM 链的输入契约）；建模链的工作副本在 `Result/gds2geo/em/` 内，可以同名；
3. **模块内部自由区**：`Result/<providerId>/<stageId>/` 之内的中间产物命名模块自定，但须在 manifest 以 internal ref 登记（见 MANIFEST_SPEC §五）；
4. **taskId**：task_layer 生成（`t<yyyyMMdd_HHmmss>` 或 UI 指定），模块不生成也不解析任务 id。

## 六、全局不变式（机器可查）

| # | 不变式 | 违反时的错误码 |
|---|---|---|
| I1 | `Model/` 下 `*.msh` 数量 == 1（peec_basic 入口时） | QS-FMT-INPUT-AMBIGUOUS（compact_em 报）/ 任务层预检 warning |
| I2 | 建模链产物坐标单位 = mm（gds2geo 输出承诺；set.txt 的 DIM=1000 依赖此条） | 模块自检 |
| I3 | `port.json.meshSha256` == 当前主网格实际 SHA-256 | QS-IO-FILE-CORRUPTED（compact_em 报）；任务层 inputs.validate 预检 |
| I4 | 各模块 receipt（`*.msh.json`）schemaVersion 在消费者 accept 区间内 | 消费者模块自报 QS-FMT-* |
| I5 | 锁文件存在 ≠ 错误；运行中不得删除 `.locks/` 内文件 | —（运维纪律） |

## 七、禁止事项

1. 模块向非自己声明的目录写文件（部署期 manifest 越权检查拦截，`QS-TL-CFG-LAYOUT-VIOLATION`）；
2. 模块代码硬编码其他模块的目录名或 providerId 字符串（checklist 审查项）；
3. 假设 dataRoot 之外的路径存在（如 C:\Tools\... 的绝对路径出现在数据流中）；
4. 交付包内携带可读写 `DATA/`（测试数据降级为只读 `examples/`，见 ONBOARDING §三）；
5. UI 直接改写 `Result/` 内任何文件（UI 只写 `Model/` 与 `Setting/` 中声明归它的文件）。

## 八、版本与演进

- 本规范带版本号（当前 v1.0）；manifest 的 `capabilities.dataLayout` 字段声明模块遵循的布局版本；
- 破坏性变更（移动契约件位置、改固定名）→ 布局版本 +1，所有模块在一个窗口期内完成适配，task_layer 注册期按版本分组校验；
- 新增模块、新增自由区目录 → 不升版本，只需 manifest 登记。
