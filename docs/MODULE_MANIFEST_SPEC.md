# 模块自描述契约规范（module.manifest.json）

v1.2 · 2026-11-02 · 模块交付四件套之一（exe / README.md / ERROR_CODES.md / module.manifest.json），放模块包根目录，**模块团队维护**。v1.2 变更：对外 ref 端口类改名 `em/ports.def` → `em/source.def`、`thermal/ports.def` → `thermal/source.def`（路径对应 `Model/source_em.json` / `Model/source_thermal.json`）；新增预留 ref `em/boundary.def` / `thermal/boundary.def`（→ `Model/boundary_em.json` / `Model/boundary_thermal.json`，暂无消费方）；`defaultTemplate` 字段废止（templates/ 不再随包）。
机器校验：平台方提供 `docs/module.manifest.schema.json`（JSON Schema draft-07），模块团队可用任意 JSON Schema 校验器本地自查；task_layer 加载时按同一 schema 做结构校验。

## 一、职责切分

| 模块团队（自治） | task_layer（仲裁） |
|---|---|
| manifest 全部内容 | 加载 manifest，合成调度模型 |
| 产物格式/schema 的运行时深检 | 跨模块 ref 匹配、DAG 环、apiVersion 窗口 |
| 输入验收条件（accept） | 路径展开（dataRoot 布局） |
| CLI 白名单（allowedArgs） | overrides 白名单校验 |
| 退出码与 QS 码（ERROR_CODES.md） | 按 errorContract 归一 errorCode 并透传 |
| 内部子阶段数据流（internal ref） | state.json 编排记录 + providers 版本快照 |
| stage_receipt.json（可选自证） | publisher 执行 publishTo 复制（模块不搬文件） |

**浅检/深检边界**：存在性/非空/schemaVersion 可读 → task_layer（依 manifest 机械执行）；格式正确性/物理一致性/数值有效性 → 模块自检并报自己的 QS 码。task_layer 不解析产物内容。

## 二、基本约束

- 位置 `<模块包>/module.manifest.json`；UTF-8；严格 JSON（无注释无尾逗号）；
- 顶层 `manifestVersion: 1`；
- path 字段一律相对 dataRoot 的 POSIX 路径；
- 提交前用 `docs/module.manifest.schema.json` 跑一遍 JSON Schema 校验（VS Code / ajv / jsonlint 均可）。

## 三、Schema（v1）

```jsonc
{
  "manifestVersion": 1,

  // ── 身份与版本 ──
  "id": "gds2geo",              // [必填] providerId = 模块包目录名，全局唯一小写
  "name": "GDS2GEO 建模与网格",  // [必填]
  "version": "2026.10.30",      // [必填] 交付批次（整包原子交付，子阶段不单独记版本）
  "apiVersion": 1,              // [必填] 整数，判定规则见 §六
  "minApiVersion": 1,           // [可选] 本模块可接受的最低上游 apiVersion

  // ── CLI ──
  "cli": {
    "executables": ["gds2geo.exe", "geo_prepare.exe"],  // [必填] 随包 exe 清单
    "dataRootArg": "--data-root",   // [可选] 数据根参数名（R1；散参数设计可省略，由 argsTemplate 显式传路径）
    "deprecatedModes": []           // [可选] 仍保留的历史兼容模式声明（如 "no-arg-default"）
  },

  // ── 子阶段 ──
  "stages": [
    {
      "id": "source",                    // [必填] = Result/<providerId>/<stageId>/ 目录名
      "executable": "gds2geo.exe",       // [必填] cli.executables 之一
      "argsTemplate": [                  // [必填] 命令模板
        "--runtime-dir", "{runtimeDir}",
        "source",
        "--gds", "{dataRoot}/Model/model.gds",
        "--output-dir", "{dataRoot}/Result/gds2geo/source"
      ],
      "allowedArgs": ["--top-cell"],     // [可选] UI 可覆盖参数白名单
      "optional": false,                 // [可选]
      "exitCodes": { "0": "ok", "2": "input_or_config", "5": "execution" },  // [必填]
      "errorContract": { "type": "none" } // [必填] none | qs-json-stderr
    }
  ],

  // ── 数据工件 ──
  "artifacts": {
    "produced": [
      { "ref": "gds2geo/mesh.em",
        "path": "Result/gds2geo/em/model_em.msh",   // [必填] 实际产出位置
        "schemaVersion": "4",                           // [对外必填]
        "units": { "length": "mm" },                    // [可选]
        "publishTo": "Model/model_em.msh" }             // [可选] 正式件落点（仅 Model//Setting/）
    ],
    "consumed": [
      { "ref": "user/layout.gds",
        "path": "Model/model.gds",                      // [方式A] 按布局自取时的固定位置
        "accept": { "extensions": [".gds"] },           // [可选] 验收条件
        "defaultTemplate": "templates/default_set.json" }// [可选] 缺省时从包内复制的模板
    ]
  },

  // ── 依赖 ──
  "dependsOn": {
    "modules": [],                                      // [可选] 上游 providerId
    "artifacts": ["user/layout.gds", "user/techfile"],  // [必填] 消费的全部对外 ref
    "environment": { "gmsh": { "version": "4.11.1", "requiredBy": ["mesh_em"] } }  // [可选]
  },

  "capabilities": {
    "dataLayout": "peec-data/v1",                       // [必填]
    "breakingChanges": [                                // [可选] apiVersion 变化时必须追加
      { "version": "2026.10.30", "note": "热 receipt 升 schema 5" }
    ]
  }
}
```

### argsTemplate 变量

| 变量 | 含义 |
|---|---|
| `{dataRoot}` | 算例数据根绝对路径 |
| `{runtimeDir}` | `<dataRoot>/Result/logs/<providerId>/`（R2 统一落点） |
| `{moduleDir}` | 模块包安装目录绝对路径（只读） |
| `{artifact:<ref>}` | 上游工件绝对路径（task_layer 查路由表填充） |
| `{override:<key>}` | 经 allowedArgs 白名单校验后的 UI 覆盖值（未提供则整对省略） |

**禁止**在 argsTemplate 中出现其他模块的目录名字面量。

## 四、artifact ref 规则

格式 `<owner>/<name>`：owner = 生产者 providerId（`gds2geo/mesh.em`）、`user`（UI 输入）、或逻辑链名（`em/ports.def`，注明 maintainedBy）。

1. **消费者只写 ref，不写路径**；ref→路径解析权 100% 归 task_layer（注册期建路由表）；
2. 每个对外 ref 唯一生产者（重复 = 注册失败 `QS-TL-CFG-ARTIFACT-MISMATCH`）；
3. name 小写自解释；废弃 ref 名字永不复用；
4. 源/边界文件按链路区分：`em/source.def` → `Model/source_em.json`、`em/boundary.def` → `Model/boundary_em.json`（预留）；`thermal/source.def` → `Model/source_thermal.json`、`thermal/boundary.def` → `Model/boundary_thermal.json`（预留）。v1.1 旧 ref（em/ports.def / thermal/ports.def，指向 port_*.json）废弃。各消费方只校验自己那份。

## 五、内部工件（internal ref）

子阶段间交接工件登记在 stage 上，位置限 `Result/<providerId>/<stageId>/` 自由区：

```jsonc
{ "id": "prepare",
  "consumesInternal": ["gds2geo/source.geo", "gds2geo/source.matdb"],
  "producesInternal": ["gds2geo/prepared.em.geo", "gds2geo/prepared.matdb"] }
```

task_layer 不做 internal ref 存在性检查（深检归模块），用它做单模块多阶段调度依赖。**内部升级为对外**（补全 path/schemaVersion/accept 并进 artifacts.produced）→ apiVersion +1。

## 六、apiVersion 完整判定规则

### 6.1 必须 +1 的变化（不兼容）

| # | 变化类型 | 示例 |
|---|---|---|
| B1 | CLI 必填参数增删/改名 | 新增必填 --xxx；--input-dir 改名为 --src |
| B2 | 退出码语义变化 | exit 5 从"执行错误"改为"超时" |
| B3 | 对外工件位置变化 | model.peec 从 Result/ 移到 Result/compact_em/extract/ |
| B4 | 对外工件 schema 不兼容变化 | matdb.json 删字段/改类型/receipt schema 大版本 +1 |
| B5 | errorContract 类型变化 | none → qs-json-stderr（反向不允许） |
| B6 | 数据布局版本变化 | peec-data/v1 → v2 |

### 6.2 不需要 +1 的变化（兼容）

| # | 变化类型 | 示例 |
|---|---|---|
| C1 | 新增可选 CLI 参数 | 新增 --verbose |
| C2 | 对外工件加可选字段 | matdb.json 新增 optionalField |
| C3 | 新增 produced ref（新工件） | 新增 gds2geo/debug.dmp |
| C4 | errorContract 增强 | none → qs-json-stderr（向后兼容：老消费者忽略 JSON 行） |
| C5 | 内部重构（不改对外行为） | 换算法、改内部临时文件名 |
| C6 | version（批次号）变化 | 2026.10.30 → 2026.11.15 |

### 6.3 兼容窗口与注册期检查

- 消费者声明 `minApiVersion`（默认 1）= "我能接受的最旧生产者 apiVersion"；
- 注册期检查公式：**consumer.minApiVersion ≤ producer.apiVersion**（不满足 → 拒绝部署，报双方版本）;
- 方向性：生产者升级 apiVersion 后，未声明 minApiVersion 的旧消费者默认 minApiVersion=1，只要 producer.apiVersion ≥ 1 就通过——**这意味着生产者升级不会自动阻断老消费者**；真正阻断靠消费者的 accept 条件（schemaVersion 区间等）在运行时报错;
- 需要硬阻断时：消费者显式声明 `"maxApiVersion"`（可选字段），检查变为 `consumer.minApiVersion ≤ producer.apiVersion ≤ consumer.maxApiVersion`。

### 6.4 变更日志

每次 apiVersion +1 必须在 `capabilities.breakingChanges` 追加一条（UI 可见，task_layer modules.list 透出）。

## 七、errorContract

| type | 语义 | task_layer 行为 |
|---|---|---|
| `none` | 无结构化错误输出（gds2geo 现状） | 非 0 退出 → `QS-TL-MOD-EXEC` + exitCode |
| `qs-json-stderr` | stderr 有 `{"code":"QS-...", "severity":"error/fatal", ...}` JSON（compact_em 契约） | 提取最后一条 severity=error/fatal 的 code 透传；完整对象在 traceLog jsonl |

- 模块错误码全集登记在自己的 ERROR_CODES.md（对齐《QS-PEEC 错误码与异常处理规范》）；
- `traceLog` 字段（可选）：jsonl 落点模板，`{trace_id}` 由模块运行时生成。

## 八、publisher 与正式件落位

- 模块**只写** `Result/<providerId>/<stageId>/` 内的阶段产物；
- 正式件（Model//Setting/ 下的共享资产）由 **task_layer publisher** 按 manifest `publishTo` 声明执行复制；
- 模块**禁止**自行往 Model//Setting/ 写文件（违反 D7/I7）；
- publisher 失败 → 该阶段标记失败（`QS-TL-IO-ERROR`），模块本身不算错。

## 九、state.json 与 stage_receipt.json（状态双轨说明）

| | task_layer state.json | 模块 stage_receipt.json |
|---|---|---|
| 位置 | `Result/tasks/<taskId>/state.json` | `Result/<providerId>/<stageId>/stage_receipt.json` |
| 写者 | task_layer（自动） | 模块（可选，进程退出前写） |
| 内容 | 阶段状态机/errorCode/outputs/providers 快照/时间戳 | 输入输出指纹/关键参数/耗时/notes |
| 调度作用 | **唯一调度事实源** | 无（不影响调度） |
| 模块感知 | 不感知、不读、不依赖 | 自愿写；写了按 DATA_LAYOUT §七 schema |
| 缺失影响 | —（task_layer 必写） | 无（可选增强信息） |

**不存在两套状态源**：调度只看 task_layer state.json；receipt 是排错/复现的增强证据。

## 十、提交前自查

- [ ] JSON Schema 校验通过（`docs/module.manifest.schema.json`）
- [ ] consumed 的对外 ref 均有唯一生产者
- [ ] produced path 全在本模块 `Result/<id>/` 内（publishTo 仅 Model//Setting/）
- [ ] stages[].id 与产物二级目录名一致
- [ ] argsTemplate 含 dataRoot 绑定（`{dataRoot}` 或显式绝对路径参数），无他模块目录名字面量
- [ ] exitCodes 覆盖 ERROR_CODES.md 全部失败路径
- [ ] 对外工件带 schemaVersion，且与上游声明区间相交非空
- [ ] dependsOn.artifacts ⊇ consumed 对外 ref 集合
- [ ] 包内无可读写 DATA/（只有 examples/）
- [ ] 最小算例回归通过（dev_case 工具）

## 十一、示例

### 11.1 gds2geo（节选）

```jsonc
{
  "manifestVersion": 1, "id": "gds2geo", "name": "GDS2GEO 建模与网格",
  "version": "2026.10.30", "apiVersion": 1,
  "cli": { "executables": ["gds2geo.exe", "geo_prepare.exe", "geo2em.exe", "geo2thermal.exe"],
           "deprecatedModes": ["no-arg-default"] },
  "stages": [
    { "id": "source", "executable": "gds2geo.exe",
      "argsTemplate": ["--runtime-dir", "{runtimeDir}", "source",
                       "--gds", "{dataRoot}/Model/model.gds",
                       "--tech", "{dataRoot}/Model/model_info.csv",
                       "--output-dir", "{dataRoot}/Result/gds2geo/source"],
      "allowedArgs": ["--top-cell", "--stack-mode"],
      "exitCodes": { "0": "ok", "2": "input_or_config", "5": "execution" },
      "errorContract": { "type": "none" } },
    { "id": "mesh_em", "executable": "geo2em.exe",
      "argsTemplate": ["--runtime-dir", "{runtimeDir}",
                       "--input-dir", "{dataRoot}/Result/gds2geo/prepared",
                       "--output-dir", "{dataRoot}/Result/gds2geo/em"],
      "exitCodes": { "0": "ok", "5": "execution" },
      "errorContract": { "type": "none" } }
    // prepare / mesh_thermal 同构，略
  ],
  "artifacts": {
    "produced": [
      { "ref": "gds2geo/source.matdb", "path": "Result/gds2geo/source/matdb.json", "schemaVersion": "3" },
      { "ref": "gds2geo/mesh.em", "path": "Result/gds2geo/em/model_em.msh",
        "schemaVersion": "4", "units": { "length": "mm" }, "publishTo": "Model/model_em.msh" },
      { "ref": "gds2geo/matdb.model", "path": "Result/gds2geo/em/matdb.json",
        "schemaVersion": "3", "publishTo": "Model/matdb.json" }
    ],
    "consumed": [
      { "ref": "user/layout.gds", "path": "Model/model.gds", "accept": { "extensions": [".gds"] } },
      { "ref": "user/techfile", "path": "Model/model_info.csv",
        "accept": { "extensions": [".csv", ".xtf"], "oneOf": true } }
    ]
  },
  "dependsOn": { "modules": [], "artifacts": ["user/layout.gds", "user/techfile"],
                 "environment": { "gmsh": { "version": "4.11.1", "requiredBy": ["mesh_em", "mesh_thermal"] } } },
  "capabilities": { "dataLayout": "peec-data/v1",
                    "breakingChanges": [ { "version": "2026.10.30", "note": "热 receipt 升 schema 5" } ] }
}
```

### 11.2 compact_em（节选）

```jsonc
{
  "manifestVersion": 1, "id": "compact_em", "name": "Compact EM 电磁求解链",
  "version": "2026.10.22", "apiVersion": 1,
  "cli": { "executables": ["PEEC_Model_Extractor.exe", "PEEC_Solver.exe",
                            "PEEC_S_Parameter_Plotter.exe", "PEEC_Post_Processing.exe"],
           "dataRootArg": "--data-root" },
  "stages": [
    { "id": "extract", "executable": "PEEC_Model_Extractor.exe",
      "argsTemplate": ["--data-root", "{dataRoot}", "--export", "post,report"],
      "exitCodes": { "0": "ok", "1": "failure" },
      "errorContract": { "type": "qs-json-stderr",
                         "traceLog": "Result/logs/compact_em/Model_Extractor-{trace_id}.jsonl" } },
    { "id": "solve", "executable": "PEEC_Solver.exe",
      "argsTemplate": ["--data-root", "{dataRoot}", "--export", "post,touchstone"],
      "exitCodes": { "0": "ok", "1": "failure" },
      "errorContract": { "type": "qs-json-stderr",
                         "traceLog": "Result/logs/compact_em/Solver-{trace_id}.jsonl" } }
    // plot / post 略
  ],
  "artifacts": {
    "produced": [
      { "ref": "compact_em/model.peec", "path": "Result/compact_em/extract/model.peec" },
      { "ref": "compact_em/sp.csv", "path": "Result/compact_em/solve/S_parameter.csv" }
    ],
    "consumed": [
      { "ref": "gds2geo/mesh.em", "path": "Model/model_em.msh",
        "accept": { "schemaVersion": { "min": 4, "max": 4 } } },
      { "ref": "gds2geo/matdb.model", "path": "Model/matdb.json",
        "accept": { "schemaVersion": { "min": 3, "max": 3 } } },
      { "ref": "em/source.def", "path": "Model/source_em.json",
        "maintainedBy": ["ui"],
        "accept": { "fields": ["meshFile", "meshSha256", "triangleIds"] } },
      { "ref": "user/solver.set", "path": "Setting/set.json" }
    ]
  },
  "dependsOn": { "modules": ["gds2geo"],
                 "artifacts": ["gds2geo/mesh.em", "gds2geo/matdb.model", "em/source.def", "user/solver.set"] },
  "capabilities": { "dataLayout": "peec-data/v1", "breakingChanges": [] }
}
```
