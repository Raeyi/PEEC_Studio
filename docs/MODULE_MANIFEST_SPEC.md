# 模块自描述契约规范（module.manifest.json）

v1.0 · 2026-10-30 · 模块交付四件套之一（exe / README.md / ERROR_CODES.md / module.manifest.json），放模块包根目录，**模块团队维护**。

## 一、职责切分

| 模块团队（自治） | task_layer（仲裁） |
|---|---|
| manifest 全部内容 | 加载 manifest，合成调度模型 |
| 产物格式/schema 的运行时深检 | 跨模块 ref 匹配、DAG 环、apiVersion 窗口 |
| 输入验收条件（accept） | 路径展开（dataRoot 布局） |
| CLI 白名单（allowedArgs） | overrides 白名单校验 |
| 退出码与 QS 码（ERROR_CODES.md） | 按 errorContract 归一 errorCode 并透传 |
| 内部子阶段数据流（internal ref） | state.json 的 providers 版本快照 |

**浅检/深检边界**：存在性/非空/schemaVersion 可读 → task_layer（依 manifest 机械执行）；格式正确性/物理一致性/数值有效性 → 模块自检并报自己的 QS 码。task_layer 不解析产物内容。

## 二、基本约束

- 位置 `<模块包>/module.manifest.json`；UTF-8；严格 JSON（无注释无尾逗号）；
- 顶层 `manifestVersion: 1`；
- path 字段一律相对 dataRoot 的 POSIX 路径。

## 三、Schema（v1）

```jsonc
{
  "manifestVersion": 1,

  // 身份与版本
  "id": "gds2geo",              // [必填] providerId = 模块包目录名，全局唯一小写
  "name": "GDS2GEO 建模与网格",  // [必填]
  "version": "2026.10.30",      // [必填] 交付批次（整包原子交付，子阶段不单独记版本）
  "apiVersion": 1,              // [必填] CLI 参数/退出码/对外工件 schema 不兼容变化时 +1
  "minApiVersion": 1,           // [可选] 可接受的最低 apiVersion（兼容窗口）

  "cli": { "executables": ["gds2geo.exe", "geo_prepare.exe"] },  // [必填] 随包 exe 清单

  // 子阶段（一个模块可含多个可执行单元；stage id = Result/<providerId>/<stageId>/ 目录名）
  "stages": [
    {
      "id": "source",                    // [必填]
      "executable": "gds2geo.exe",       // [必填] cli.executables 之一
      "argsTemplate": [                  // [必填] {dataRoot}/{runtimeDir}/{artifact:<ref>}/{override:<key>} 由 task_layer 填充
        "--data-root", "{dataRoot}",
        "--runtime-dir", "{runtimeDir}",
        "source", "--output-dir", "{dataRoot}/Result/gds2geo/source"
      ],
      "allowedArgs": ["--top-cell"],     // [可选] UI 可覆盖参数白名单
      "optional": false,                 // [可选] 可选阶段
      "exitCodes": { "0": "ok", "2": "input_or_config", "5": "execution" },  // [必填]
      "errorContract": { "type": "none" } // [必填] none | qs-json-stderr（后者可加 "traceLog"）
    }
  ],

  // 数据工件
  "artifacts": {
    "produced": [
      { "ref": "gds2geo/mesh.em",                       // [必填] 工件引用
        "path": "Result/gds2geo/em/model_em.msh",       // [必填] 实际产出位置
        "schemaVersion": "4",                           // [对外必填]
        "units": { "length": "mm" },                    // [可选]
        "publishTo": "Model/model_em.msh" }             // [可选] 正式件落点（仅允许 Model//Setting/）
    ],
    "consumed": [
      { "ref": "user/layout.gds",
        "path": "Model/model.gds",                      // [方式A] 按布局自取时的固定位置
        "accept": { "extensions": [".gds"] },           // [可选] 验收条件
        "defaultTemplate": "templates/default_set.txt" }// [可选] 缺省时从包内复制的模板
    ]
  },

  // 依赖
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

argsTemplate 变量：`{dataRoot}` 数据根绝对路径；`{runtimeDir}` = `<dataRoot>/Result/logs/<providerId>/`；`{artifact:<ref>}` 上游工件绝对路径（task_layer 查路由表填充）；`{override:<key>}` 经白名单校验的 UI 覆盖值（未提供则整对省略）。**禁止出现其他模块目录名字面量。**

## 四、artifact ref 规则

格式 `<owner>/<name>`：owner = 生产者 providerId（`gds2geo/mesh.em`）、`user`（UI 输入）、或逻辑链名（`em/ports.def`，注明 maintainedBy）。

1. **消费者只写 ref，不写路径**；ref→路径解析权 100% 归 task_layer（注册期建路由表）；
2. 每个对外 ref 唯一生产者（重复 = 注册失败 `QS-TL-CFG-ARTIFACT-MISMATCH`）；
3. name 小写自解释；废弃 ref 名字永不复用。

端口文件按链路区分：`em/ports.def` → `Model/port_em.json`（compact_em 消费）；`thermal/ports.def` → `Model/port_thermal.json`（热模块消费，预留）。各消费方只校验自己那份。

## 五、内部工件（internal ref）

子阶段间交接工件登记在 stage 上，位置限 `Result/<providerId>/<stageId>/` 自由区：

```jsonc
{ "id": "prepare",
  "consumesInternal": ["gds2geo/source.geo", "gds2geo/source.matdb"],
  "producesInternal": ["gds2geo/prepared.em.geo", "gds2geo/prepared.matdb"] }
```

task_layer 不做 internal ref 存在性检查（深检归模块），用它做单模块多阶段调度依赖。**内部升级为对外**（补全 path/schemaVersion/accept 并进 artifacts.produced）→ apiVersion +1。

## 六、版本规则

1. `version`（批次，给人看）+ `apiVersion`（整数，机器判兼容）；**子阶段/内部库不单独记版本**，内部库版本写 README；
2. apiVersion +1 触发条件（任一）：CLI 必填参数增删改名、退出码语义变化、对外工件位置/schema 不兼容变化、errorContract 类型变化；加可选字段不加；
3. 兼容窗口：`consumer.minApiVersion ≤ producer.apiVersion`，注册期检查，不满足拒绝部署；
4. 每次 apiVersion 变化必须在 breakingChanges 追加一条（UI 可见）；
5. errorContract 从 none 升为 qs-json-stderr 属增强型变更，不升 apiVersion。

## 七、errorContract

| type | 语义 | task_layer 行为 |
|---|---|---|
| `none` | 无结构化错误输出（gds2geo 现状） | 非 0 退出 → `QS-TL-MOD-EXEC` + exitCode |
| `qs-json-stderr` | stderr 有 `{"code":"QS-..."}` JSON（compact_em 契约） | 提取最后一条 severity=error/fatal 的 code 透传；完整对象在 traceLog jsonl |

模块错误码全集登记在自己的 ERROR_CODES.md（对齐《QS-PEEC 错误码与异常处理规范》）。

## 八、提交前自查

- [ ] consumed 的对外 ref 均有唯一生产者
- [ ] produced path 全在本模块 `Result/<id>/` 内（publishTo 仅 Model//Setting/）
- [ ] stages[].id 与产物二级目录名一致
- [ ] argsTemplate 含 `{dataRoot}`，无他模块目录名字面量
- [ ] exitCodes 覆盖 ERROR_CODES.md 全部失败路径
- [ ] 对外工件带 schemaVersion，且与上游声明区间相交非空
- [ ] dependsOn.artifacts ⊇ consumed 对外 ref 集合

## 九、示例

### gds2geo（节选）

```jsonc
{
  "manifestVersion": 1, "id": "gds2geo", "name": "GDS2GEO 建模与网格",
  "version": "2026.10.30", "apiVersion": 1,
  "cli": { "executables": ["gds2geo.exe", "geo_prepare.exe", "geo2em.exe", "geo2thermal.exe"] },
  "stages": [
    { "id": "source", "executable": "gds2geo.exe",
      "argsTemplate": ["--data-root", "{dataRoot}", "--runtime-dir", "{runtimeDir}",
                       "source", "--output-dir", "{dataRoot}/Result/gds2geo/source"],
      "allowedArgs": ["--top-cell", "--stack-mode"],
      "exitCodes": { "0": "ok", "2": "input_or_config", "5": "execution" },
      "errorContract": { "type": "none" } },
    { "id": "mesh_em", "executable": "geo2em.exe",
      "argsTemplate": ["--data-root", "{dataRoot}", "--runtime-dir", "{runtimeDir}",
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

### compact_em（节选）

```jsonc
{
  "manifestVersion": 1, "id": "compact_em", "name": "Compact EM 电磁求解链",
  "version": "2026.10.22", "apiVersion": 1,
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
      { "ref": "compact_em/sp.csv", "path": "Result/compact_em/solve/S_parameter.csv" }
    ],
    "consumed": [
      { "ref": "gds2geo/mesh.em", "path": "Model/model_em.msh",
        "accept": { "schemaVersion": { "min": 4, "max": 4 } } },
      { "ref": "gds2geo/matdb.model", "path": "Model/matdb.json",
        "accept": { "schemaVersion": { "min": 3, "max": 3 } } },
      { "ref": "em/ports.def", "path": "Model/port_em.json",
        "accept": { "fields": ["meshFile", "meshSha256", "triangleIds"] } },
      { "ref": "user/solver.set", "path": "Setting/set.txt",
        "defaultTemplate": "templates/default_set.txt" }
    ]
  },
  "dependsOn": { "modules": ["gds2geo"],
                 "artifacts": ["gds2geo/mesh.em", "gds2geo/matdb.model", "em/ports.def", "user/solver.set"] },
  "capabilities": { "dataLayout": "peec-data/v1", "breakingChanges": [] }
}
```
