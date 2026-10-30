# PEEC Studio 数据布局规范（DATA_LAYOUT）

v1.0 · 2026-10-30 · 适用：所有计算层模块与 UI、task_layer。路径与目录的唯一事实源；manifest 中所有 path 以本文件为基准；冲突时以本文件为准。

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
│   ├── model_info.csv|.xtf   [UI]       工艺（二选一，固定名）
│   ├── <主网格>.msh           [gds2geo]  EM 主网格正式件（Model 下 *.msh 恰好一份）
│   ├── matdb.json            [gds2geo]  材料库正式件
│   ├── port_em.json          [UI]       EM 端口绑定（compact_em 消费）
│   └── port_thermal.json     [UI]       热端口绑定（热模块消费，预留）
├── Setting\          【配置】跨任务持久
│   └── set.txt               [UI 或 compact_em 模板]
├── Result\
│   ├── gds2geo\        【模块产物区：gds2geo 独占】
│   │   ├── source\    model.geo / model.json / matdb.json
│   │   ├── prepared\  em.geo / em.json / thermal.geo / thermal.json / matdb.json
│   │   ├── em\        model_em.msh（工作副本）/ model_em.msh.json / matdb.json
│   │   └── thermal\   model_thermal.msh / model_thermal.msh.json / matdb.json（可选）
│   ├── compact_em\     【模块产物区：compact_em 独占】
│   │   ├── extract\   model.peec / state.json
│   │   ├── solve\     S_parameter.csv / state.json
│   │   ├── plot\      *.svg（可选）
│   │   └── post\      Source.msh / Post_Model.txt / Post_Source.txt（可选）
│   ├── logs\           【模块诊断日志】按模块子目录隔离
│   │   ├── gds2geo\   gds2geo.log / geo_prepare.log / ...
│   │   └── compact_em\<exe>-<trace_id>.jsonl
│   └── tasks\          【任务编排记录：task_layer 独占】
│       └── <taskId>\  state.json / task.log / job.<stageId>.json
└── .locks\         【锁集中处】<providerId>.*.lock
```

注：模块交付包内**不含**输入文件与 DATA/；UI 选取的输入直接写入 `<dataRoot>\Model\`。包内可选 `examples/` 仅为模块团队本地自测样品（只读），与 UI 流程无关。

## 三、分区规则

| # | 规则 |
|---|---|
| D1 | `Result/<providerId>/` 模块独占；providerId = manifest 的 id（= 模块包目录名，小写） |
| D2 | 模块内二级目录 = 子阶段名（manifest.stages[].id），一一对应 |
| D3 | `Model/` 是唯一跨模块共享区；port 文件按链路命名（port_em.json / port_thermal.json），各消费方只认自己那份 |
| D4 | 模块诊断日志 → `Result/logs/<providerId>/`；任务编排记录 → `Result/tasks/`（task_layer 所有） |
| D5 | 锁集中 `.locks/`，命名 `<providerId>.*.lock`；运行中禁止手工删除 |
| D6 | 阶段状态文件统一命名 `state.json`（放各自阶段目录内） |

## 四、所有权表

| 目录/文件 | 写者 | 读者 |
|---|---|---|
| `Model/model.gds`、`model_info.*` | UI | gds2geo |
| `Model/*.msh`、`matdb.json` | gds2geo（经 publisher） | compact_em、UI（渲染） |
| `Model/port_em.json` | UI | compact_em（sha 校验） |
| `Model/port_thermal.json` | UI | 热模块（预留） |
| `Setting/set.txt` | UI 或 compact_em 模板 | compact_em |
| `Result/gds2geo/**` | gds2geo | 其下游阶段、publisher |
| `Result/compact_em/**` | compact_em | 其下游阶段、UI |
| `Result/logs/<providerId>/` | 对应模块 | 排错 |
| `Result/tasks/` | task_layer | 排错、UI |
| `.locks/` | 各后端 | —（禁手工操作） |

## 五、CLI 铁律（R1-R3）

| # | 规则 |
|---|---|
| R1 | `--data-root <dir>` 必填（参数名统一，不允许别名）；相对路径一律按 dataRoot 解析；禁止按 EXE 目录解析、禁止缺省回退包内 input//output/ |
| R2 | 日志/私有锁/临时文件经 `--runtime-dir` 指定（task_layer 统一指到 `<dataRoot>/Result/logs/<providerId>/`）；不支持该参数的模块日志固定落此处 |
| R3 | 安装目录完全只读时可正常工作（静态链接，不依赖安装目录内 DLL） |

task_layer 部署期检查 stage 命令含 dataRoot 绑定，不合规拒绝注册。

## 六、命名与不变式

固定名工件（名字定死，模块不得改）：`model.gds`、`model_info.csv/.xtf`、`matdb.json`、`port_em.json`、`port_thermal.json`、`set.txt`。
模块内部自由区：`Result/<providerId>/<stageId>/` 之内中间产物命名自定，须以 internal ref 登记（MANIFEST_SPEC §五）。

| # | 不变式 | 违反时报 |
|---|---|---|
| I1 | peec_basic 入口时 `Model/*.msh` 数量 == 1 | QS-FMT-INPUT-AMBIGUOUS / 预检 warning |
| I2 | 建模链产物坐标单位 = mm（set.txt DIM=1000 依赖此条） | 模块自检 |
| I3 | `port_em.json.meshSha256` == 当前主网格实际 SHA-256 | QS-IO-FILE-CORRUPTED / 预检 |
| I4 | receipt schemaVersion 在消费者 accept 区间内 | 消费者自报 QS-FMT-* |
| I5 | 锁文件存在 ≠ 错误；运行中不得删除 `.locks/` 内文件 | — |

## 七、禁止事项

1. 写非自己声明的目录（拦截码 `QS-TL-CFG-LAYOUT-VIOLATION`）；
2. 代码硬编码其他模块的目录名/providerId 字符串；
3. 假设 dataRoot 之外路径存在；
4. 交付包携带可读写 `DATA/`（测试数据降级只读 `examples/`）；
5. UI 改写 `Result/` 内文件（UI 只写 `Model/`、`Setting/` 中归它的文件）。

## 八、版本

本规范带版本（当前 v1.0），manifest 的 `capabilities.dataLayout` 声明遵循版本。破坏性变更（移动契约件/改固定名）→ 版本 +1，窗口期内全模块适配；新增模块或自由区目录不升版本。
