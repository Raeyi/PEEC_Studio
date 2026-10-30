# 模块自描述契约规范（module.manifest.json）

版本 v1.0 · 2026-10-30 · 适用：所有计算层模块团队。
本文件定义 `module.manifest.json` 的 schema、命名规则、版本规则与校验边界。
manifest 是模块交付四件套之一（exe / README.md / ERROR_CODES.md / module.manifest.json），
放在模块包根目录，由**模块团队维护**；task_layer 只读取、不代写。

---

## 一、定位与职责切分

| 模块团队负责（自治） | task_layer 负责（仲裁） |
|---|---|
| 自己的 manifest（身份/版本/子阶段/工件/依赖/错误契约） | 发现并加载各包 manifest，合成调度模型 |
| 自己产物的格式/schema/版本与运行时深检 | 跨模块 ref 匹配、DAG 环检测、apiVersion 兼容窗口 |
| 自己消费的输入要求（accept 条件） | 路径变量展开（dataRoot 布局，见 DATA_LAYOUT.md） |
| 自己的 CLI 参数白名单（allowedArgs） | overrides 白名单校验（非法键拒绝） |
| 自己的退出码与 QS 错误码（ERROR_CODES.md） | 按 errorContract 归一 errorCode 并透传 |
| 自己内部子阶段的数据流（internal ref） | 状态聚合（state.json 的 providers 版本快照） |

**浅检/深检边界（重要）**：
- **浅检归 task_layer**：文件存在、非空、（可选）schemaVersion 字段可读——依据 manifest 声明机械执行；
- **深检归模块**：格式正确性、物理一致性、数值有效性——模块运行时自检并报自己的 QS 码；
- task_layer 永远不解析产物内容（除浅检允许的最小读取）。

## 二、文件位置与基本约束

- 路径：`<模块包>/module.manifest.json`（与 exe、README.md、ERROR_CODES.md 同级）；
- 编码 UTF-8，JSON 严格语法（无注释、无尾逗号）；
- 顶层 `manifestVersion` 必须为 1（当前唯一支持值）；
- 全部 path 字段为**相对 dataRoot** 的 POSIX 风格路径（见 DATA_LAYOUT.md §一.4）。

## 三、Schema 全字段（v1）

```jsonc
{
  "manifestVersion": 1,

  // ── 1. 身份与版本 ──
  "id": "gds2geo",                    // [必填] 全局唯一 providerId，小写字母/数字/点
  "name": "GDS2GEO 建模与网格",        // [必填] 展示名
  "version": "2026.10.30",            // [必填] 交付批次版本（任意字符串，建议日期版）
  "apiVersion": 1,                    // [必填] 整数。CLI 参数/退出码/对外工件 schema 有不兼容变化时 +1
  "minApiVersion": 1,                 // [可选] 本模块可接受的最低 apiVersion（滚动升级窗口）

  // ── 2. CLI 信息 ──
  "cli": {
    "executables": ["gds2geo.exe", "geo_prepare.exe", "geo2em.exe", "geo2thermal.exe"]
    // [必填] 随包交付的可执行文件清单（部署完整性检查用）
  },

  // ── 3. 子阶段（一个模块可含多个可执行单元）──
  "stages": [
    {
      "id": "source",                 // [必填] 阶段 id（= Result/<providerId>/<stageId>/ 目录名）
      "executable": "gds2geo.exe",    // [必填] cli.executables 之一
      "argsTemplate": [               // [必填] 命令模板；{dataRoot}/{runtimeDir} 由 task_layer 填充
        "--data-root", "{dataRoot}",
        "--runtime-dir", "{runtimeDir}",
        "source",
        "--output-dir", "{dataRoot}/Result/gds2geo/source"
      ],
      "allowedArgs": ["--top-cell", "--stack-mode"],   // [可选] UI/宿主可覆盖的参数白名单
      "exitCodes": {                  // [必填] 退出码语义登记
        "0": "ok", "2": "input_or_config", "5": "execution"
      },
      "errorContract": {              // [必填] 错误信息契约（决定 task_layer 提取策略）
        "type": "none"                // none | qs-json-stderr
        // type=qs-json-stderr 时可加：
        // "traceLog": "Result/logs/compact_em/{exe}-{trace_id}.jsonl"
      }
    }
  ],

  // ── 4. 数据工件 ──
  "artifacts": {
    "produced": [
      {
        "ref": "gds2geo/mesh.em",     // [必填] 工件引用（命名规则见 §四）
        "path": "Result/gds2geo/em/model_em.msh",   // [必填] 相对 dataRoot 的实际产出位置
        "format": "Gmsh MSH",         // [可选] 人读说明
        "schemaVersion": "4",         // [对外工件必填] 产物 schema 版本（receipt 版本等）
        "units": { "length": "mm" },  // [可选] 单位声明
        "publishTo": "Model/model_em.msh"   // [可选] 正式件落点（publisher 执行复制；仅 Model/Setting 允许）
      }
    ],
    "consumed": [
      {
        "ref": "user/layout.gds",     // [必填] 要消费的工件 ref
        "path": "Model/model.gds",    // [方式A必填] 按布局规范自取时的固定位置
        "accept": {                   // [可选] 验收条件（模块运行时深检的依据声明）
          "extensions": [".gds"],
          "schemaVersion": { "min": 4, "max": 4 },
          "fields": ["meshFile", "meshSha256"]
        },
        "defaultTemplate": "templates/default_set.txt"   // [可选] 缺省时从模块包内复制的模板（如 set.txt）
      }
    ]
  },

  // ── 5. 依赖声明 ──
  "dependsOn": {
    "modules": ["gds2geo"],           // [可选] 显式上游模块（providerId 列表）
    "artifacts": ["gds2geo/mesh.em", "user/solver.set"],   // [必填] 消费的全部对外 ref
    "environment": {                  // [可选] 外部依赖（非模块类）
      "gmsh": { "version": "4.11.1", "requiredBy": ["mesh_em", "mesh_thermal"] }
    }
  },

  // ── 6. 能力与兼容 ──
  "capabilities": {
    "dataLayout": "peec-data/v1",     // [必填] 遵循的 DATA_LAYOUT 版本
    "breakingChanges": [              // [可选] 版本演进记录（task_layer 原样透出给 UI）
      { "version": "2026.10.30", "note": "热 receipt 升 schema 5，新增 nonmaterialSurfaces；旧 prepared 包仍可读" }
    ]
  }
}
```

### argsTemplate 可用变量

| 变量 | 含义 |
|---|---|
| `{dataRoot}` | 算例数据根绝对路径 |
| `{runtimeDir}` | `<dataRoot>/Result/logs/<providerId>/`（R2 统一落点） |
| `{artifact:<ref>}` | 上游工件的绝对路径（task_layer 查 ref 路由表填充；方式 B 注入） |
| `{override:<key>}` | 经 allowedArgs 白名单校验后的 UI 覆盖值（未提供时该参数对整体省略） |

**禁止**在 argsTemplate 中出现其他模块的目录名字面量。

## 四、artifact ref 命名规则

格式：`<owner>/<name>`，owner 取值：

| owner | 含义 | 示例 |
|---|---|---|
| 模块 providerId | 该模块生产的工件（所有权 = 生产者） | `gds2geo/mesh.em`、`compact_em/sp.csv` |
| `user` | UI/用户提供的输入 | `user/layout.gds`、`user/techfile`、`user/solver.set` |
| `em` 等逻辑链名 | 多方维护的共享资产（须在条目中注明 maintainedBy） | `em/ports.def`（maintainedBy: ["ui"]） |

规则：
1. **消费者只写 ref，不写路径**——ref→磁盘路径的解析权 100% 归 task_layer（注册期建路由表）；
2. 每个对外 ref 有且只有一个生产者（重复生产 = 注册失败 `QS-TL-CFG-ARTIFACT-MISMATCH`）；
3. name 用小写下划线/点，自解释（`mesh.em` 优于 `out1`）；
4. 新工件 = 新 ref；**永不复用已废弃 ref 的名字**。

## 五、模块内部工件（internal ref）

模块多子阶段之间的交接工件同样登记，但标记为内部：

```jsonc
"stages": [
  { "id": "prepare",
    "consumesInternal": ["gds2geo/source.geo", "gds2geo/source.json", "gds2geo/source.matdb"],
    "producesInternal": ["gds2geo/prepared.em.geo", "gds2geo/prepared.em.json", "gds2geo/prepared.matdb"] }
]
```

- internal ref 的位置必须在 `Result/<providerId>/<stageId>/` 自由区内（DATA_LAYOUT D1/D2）；
- task_layer 不做 internal ref 的存在性检查（深检归模块），但会用它做单模块多阶段的调度依赖;
- **内部升级为对外**：补全 path/schemaVersion/accept 并从 artifacts.produced 正式登记 → 触发 apiVersion +1。

## 六、版本规则

1. **双版本号**：`version`（交付批次，任意格式，给人看）+ `apiVersion`（整数，给机器判兼容）；
2. **apiVersion +1 的触发条件**（任一）：CLI 必填参数增删/改名、退出码语义变化、对外工件位置或 schema 不兼容变化、errorContract 类型变化；
3. **兼容窗口**：消费者 manifest 可声明 `minApiVersion`；task_layer 注册期检查
   `consumer.minApiVersion ≤ producer.apiVersion`，不满足拒绝部署并给出双方版本；
4. **变更日志**：每次 apiVersion 变化必须在 `capabilities.breakingChanges` 追加一条（UI 可见）；
5. **产物版本戳**：模块自愿在产物旁写 receipt（如 `*.msh.json`），task_layer 不解析内容，当普通产出文件登记。

## 七、errorContract 与错误归一

| type | 语义 | task_layer 行为 |
|---|---|---|
| `none` | 模块无结构化错误输出（如 gds2geo 现状） | 非 0 退出 → `QS-TL-MOD-EXEC` + exitCode（按 exitCodes 表附语义） |
| `qs-json-stderr` | stderr 有 `{"code":"QS-...", ...}` JSON 行（compact_em 契约） | 提取最后一条 severity=error/fatal 的 code 原样透传；完整对象在 traceLog jsonl |

- 模块接入 QS 码后把 type 从 none 改为 qs-json-stderr 属于**增强型变更**（不升 apiVersion，向后兼容）；
- 模块自身错误码全集登记在自己的 ERROR_CODES.md（格式对齐《QS-PEEC 错误码与异常处理规范》）。

## 八、task_layer 注册期校验清单（模块自查用）

提交 manifest 前，模块团队可对照以下检查项自测（task_layer 会做同样的事）：

- [ ] 所有 consumed 的对外 ref 存在唯一生产者（本模块或上游或 user/*）
- [ ] 所有 produced 的 path 落在 `Result/<本模块id>/` 内（publishTo 除外，仅允许 Model//Setting/）
- [ ] stages[].id 与产物二级目录名一致
- [ ] argsTemplate 含 `{dataRoot}`；无他模块目录名字面量
- [ ] exitCodes 覆盖 ERROR_CODES.md 声明的全部退出码
- [ ] 对外工件均带 schemaVersion；accept 区间与上游声明相交非空
- [ ] dependsOn.artifacts ⊇ consumed 的对外 ref 集合

## 九、完整示例

### 9.1 gds2geo（节选）

```jsonc
{
  "manifestVersion": 1,
  "id": "gds2geo",
  "name": "GDS2GEO 建模与网格",
  "version": "2026.10.30",
  "apiVersion": 1,
  "cli": { "executables": ["gds2geo.exe", "geo_prepare.exe", "geo2em.exe", "geo2thermal.exe"] },
  "stages": [
    { "id": "source", "executable": "gds2geo.exe",
      "argsTemplate": ["--data-root", "{dataRoot}", "--runtime-dir", "{runtimeDir}",
                       "source", "--output-dir", "{dataRoot}/Result/gds2geo/source"],
      "allowedArgs": ["--top-cell", "--stack-mode"],
      "exitCodes": { "0": "ok", "2": "input_or_config", "5": "execution" },
      "errorContract": { "type": "none" } },
    { "id": "prepare", "executable": "geo_prepare.exe",
      "argsTemplate": ["--data-root", "{dataRoot}", "--runtime-dir", "{runtimeDir}",
                       "--input-dir", "{dataRoot}/Result/gds2geo/source",
                       "--output-dir", "{dataRoot}/Result/gds2geo/prepared"],
      "exitCodes": { "0": "ok", "2": "input_or_config", "5": "execution" },
      "errorContract": { "type": "none" } },
    { "id": "mesh_em", "executable": "geo2em.exe",
      "argsTemplate": ["--data-root", "{dataRoot}", "--runtime-dir", "{runtimeDir}",
                       "--input-dir", "{dataRoot}/Result/gds2geo/prepared",
                       "--output-dir", "{dataRoot}/Result/gds2geo/em"],
      "exitCodes": { "0": "ok", "5": "execution" },
      "errorContract": { "type": "none" } },
    { "id": "mesh_thermal", "executable": "geo2thermal.exe", "optional": true,
      "argsTemplate": ["--data-root", "{dataRoot}", "--runtime-dir", "{runtimeDir}",
                       "--input-dir", "{dataRoot}/Result/gds2geo/prepared",
                       "--output-dir", "{dataRoot}/Result/gds2geo/thermal"],
      "exitCodes": { "0": "ok", "5": "execution" },
      "errorContract": { "type": "none" } }
  ],
  "artifacts": {
    "produced": [
      { "ref": "gds2geo/source.geo",   "path": "Result/gds2geo/source/model.geo" },
      { "ref": "gds2geo/source.json",  "path": "Result/gds2geo/source/model.json" },
      { "ref": "gds2geo/source.matdb", "path": "Result/gds2geo/source/matdb.json", "schemaVersion": "3" },
      { "ref": "gds2geo/mesh.em",      "path": "Result/gds2geo/em/model_em.msh",
        "schemaVersion": "4", "units": { "length": "mm" }, "publishTo": "Model/model_em.msh" },
      { "ref": "gds2geo/matdb.model",  "path": "Result/gds2geo/em/matdb.json",
        "schemaVersion": "3", "publishTo": "Model/matdb.json" }
    ],
    "consumed": [
      { "ref": "user/layout.gds", "path": "Model/model.gds",
        "accept": { "extensions": [".gds"] } },
      { "ref": "user/techfile", "path": "Model/model_info.csv",
        "accept": { "extensions": [".csv", ".xtf"], "oneOf": true } }
    ]
  },
  "dependsOn": {
    "modules": [],
    "artifacts": ["user/layout.gds", "user/techfile"],
    "environment": { "gmsh": { "version": "4.11.1", "requiredBy": ["mesh_em", "mesh_thermal"] } }
  },
  "capabilities": {
    "dataLayout": "peec-data/v1",
    "breakingChanges": [
      { "version": "2026.10.30", "note": "热 receipt 升 schema 5（nonmaterialSurfaces）；准备策略 four-executable-6" }
    ]
  }
}
```

### 9.2 compact_em（节选）

```jsonc
{
  "manifestVersion": 1,
  "id": "compact_em",
  "name": "Compact EM 电磁求解链",
  "version": "2026.10.22",
  "apiVersion": 1,
  "cli": { "executables": ["PEEC_Model_Extractor.exe", "PEEC_Solver.exe",
                            "PEEC_S_Parameter_Plotter.exe", "PEEC_Post_Processing.exe"] },
  "stages": [
    { "id": "extract", "executable": "PEEC_Model_Extractor.exe",
      "argsTemplate": ["--data-root", "{dataRoot}", "--export", "post,report"],
      "exitCodes": { "0": "ok", "1": "failure" },
      "errorContract": { "type": "qs-json-stderr",
                         "traceLog": "Result/logs/compact_em/PEEC_Model_Extractor-{trace_id}.jsonl" } },
    { "id": "solve", "executable": "PEEC_Solver.exe",
      "argsTemplate": ["--data-root", "{dataRoot}", "--export", "post,touchstone"],
      "exitCodes": { "0": "ok", "1": "failure" },
      "errorContract": { "type": "qs-json-stderr",
                         "traceLog": "Result/logs/compact_em/PEEC_Solver-{trace_id}.jsonl" } }
    // plot / post 略
  ],
  "artifacts": {
    "produced": [
      { "ref": "compact_em/model.peec", "path": "Result/compact_em/extract/model.peec" },
      { "ref": "compact_em/sp.csv",     "path": "Result/compact_em/solve/S_parameter.csv" }
    ],
    "consumed": [
      { "ref": "gds2geo/mesh.em",     "path": "Model/model_em.msh",
        "accept": { "schemaVersion": { "min": 4, "max": 4 } } },
      { "ref": "gds2geo/matdb.model", "path": "Model/matdb.json",
        "accept": { "schemaVersion": { "min": 3, "max": 3 } } },
      { "ref": "em/ports.def",        "path": "Model/port.json",
        "accept": { "fields": ["meshFile", "meshSha256", "triangleIds"] } },
      { "ref": "user/solver.set",     "path": "Setting/set.txt",
        "defaultTemplate": "templates/default_set.txt" }
    ]
  },
  "dependsOn": {
    "modules": ["gds2geo"],
    "artifacts": ["gds2geo/mesh.em", "gds2geo/matdb.model", "em/ports.def", "user/solver.set"]
  },
  "capabilities": { "dataLayout": "peec-data/v1", "breakingChanges": [] }
}
```

## 十、FAQ

**Q：模块想让自己的中间产物被 UI 展示，怎么办？**
A：升级为对外 ref（§五），UI 通过 state.json 的 outputs 拿到路径；不需要单独接口。

**Q：两个模块都想生产同一个逻辑工件（比如两套材料库）？**
A：不允许同 ref 双生产者。用不同 ref（`a/matdb`、`b/matdb`），由下游消费者的 accept 选择。

**Q：manifest 里的 path 和实际 exe 行为不一致怎么办？**
A：以 exe 实际行为为准修正 manifest——manifest 是承诺，注册期/运行期的校验失败都会指向这种漂移。接入 checklist 要求最小算例回归（ONBOARDING §四）。

**Q：DLL 形态模块（sim.solver 等）适用本规范吗？**
A：适用。kind=dll 时 `cli.executables` 填 DLL 文件名，task_layer 走 worker_host 宿主；job JSON 中的 paths 由 task_layer 按本规范生成，模块入口函数收到的仍是同一套 dataRoot 语义。
