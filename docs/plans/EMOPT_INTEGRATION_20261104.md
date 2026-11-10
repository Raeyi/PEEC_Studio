# 有限元仿真优化模块（emopt）平台接入设计

> **状态**：设计基线（待评审）。只出方案，未动代码。
> 日期：2026-11-04 · 范围：emopt 模块侧最小改造 + task_layer 接入（注册/流水线/数据布局）+ UI 对接要点 + 分期路线。
> **关联活契约**：`docs/DATA_LAYOUT.md`（v1.5 修订 1）/ `docs/MODULE_MANIFEST_SPEC.md`（v1.2）/ `docs/MODULE_ONBOARDING.md`（v1.2）/ `task_layer/UI_API.md` / `docs/plans/PIPELINE_OPTIMIZATION_20261104.md`。
> 输入材料：`C:\Users\enine\Desktop\dht\`（PPT《有限元优化引擎1103》、模块功能说明/算子接口说明/自测报告、`FEM_opt_20261030/01_程序`、`02_源码`、两案例）。
> 代码锚点基于 2026-11-04 两侧代码库，行号可能漂移，**以函数名为准**。

---

## 一、背景与目标

### 1.1 待接入模块（emopt）

交付包 `FEM_opt_20261030`：GUI 0.5.0-four-json-20261030 + 三个计算模块 0.5.1-residual-record-only-20261030。
基于参数化 Gmsh 几何（base.geo）与对应网格（base.msh，MSH 2.2 ASCII，单位 m），完成**尺寸驱动网格变形、S 参数（含 SOAR 降阶快扫）及一阶伴随灵敏度、有界优化**，面向波导四极腔体滤波器的尺寸优化（随附 WG_4pole_1 / WG_4pole_2 两案例）。

| 程序 | 职责 | 实测耗时（i7-11700 单线程） |
|---|---|---|
| `emopt_morph.exe` | GEO 尺寸驱动网格变形（Gmsh 原生库 + RBF 内部插值），可选输出节点坐标对尺寸的导数 | 11.39 / 9.81 s（例1/例2） |
| `emopt_sim.exe` | 四面体矢量 FEM + 双端口 TE10：`full_order` 全阶 / `fast_s` SOAR 降阶快扫 / `fast_sensitivity` 伴随灵敏度（S 与 dS/dp 一并输出） | 38.06/173.94 s；71.47/149.90 s |
| `emopt_opt.exe` | 有界 BFGS + Armijo 线搜索；目标为 S11/S21 分频段 dB 超差加权平方平均 cost | 869.45 / 966.79 s |
| `emopt_gui.exe` | 独立 Qt GUI（打开案例、编辑设置、调用三 EXE、绘图） | — |

技术栈：C++17 + MSVC 2022 + Eigen(MPL2，`EIGEN_DONT_PARALLELIZE` 单线程) + nlohmann/json + Gmsh 原生 C API（`runtime/gmsh-4.8.dll`，75 MB，从 EXE 目录向上 7 级搜索）。源码分层清晰：`emopt_input → emopt_fem → emopt_solver → emopt_mor → emopt_morph → emopt_sensitivity → emopt_workflow → emopt_gmsh_eval → emopt_modules(3 EXE)`，GUI 为独立子工程（Qt 仅 GUI 依赖，计算链零 Qt 依赖）。

### 1.2 平台侧现状

QS-PEEC Studio 三层架构（`task_layer/README.md`）：

```
UI（Qt，JSON-RPC 2.0 over stdio）
  → task_layer.exe：输入检查 / 任务编排 / 模块路由 / 状态与结果管理
    → 计算层模块：kind=external（直接启动 EXE，迁移期）/ kind=dll（worker_host 隔离加载，目标形态）
      → 数据层：dataRoot 布局（Model/Setting/Result，DATA_LAYOUT v1.5）
```

模块接入已完全契约化：**四件套**（exe / README.md / ERROR_CODES.md / `module.manifest.json`，ONBOARDING §一）+ 包外**案例包**（`dev/cases/<module>/`）+ `modules.json` 三行路由 + 注册期 schema/DAG/工件校验。**平台代码零适配**是既定底线。

平台已为本模块预留位：`modules.json` 中 `opt.optimizer`（displayName「优化计算（伴随灵敏度，待接入）」，`kind: dll`，provider `opt_optimizer`）——语义与 emopt 完全对应，见 §5.4 处置建议。

### 1.3 目标

1. emopt 以计算层模块身份接入：UI 经 task_layer 发起**变形 / S 参数快扫（含灵敏度）/ 优化**，获得统一的状态、日志、取消、错误码、结果清单；
2. 模块数值行为与交付基线**逐位一致**（自测报告为验收基线，§7）；
3. 迁移期不动 task_layer 代码（纯配置 + 模块侧最小改造）；DLL 隔离形态作为目标形态在后续期落地。

---

## 二、接口盘点（emopt 侧现状，接入依据）

### 2.1 CLI 契约（v2 请求）

| 模块 | 命令 | 说明 |
|---|---|---|
| morph | `emopt_morph.exe DESIGN.json REQUEST.json NEW_OUTPUT` | 请求 schema `emopt.morph.request.v1`：`operation=apply`、`purpose=validation|optimization`、`parametersMeter`（命名绝对尺寸 m）、`contextFile`、`emitNodeJacobian` |
| sim | `emopt_sim.exe REQUEST.json NEW_OUTPUT` | schema `emopt.module.sim.v2`：`profile=gmsh_wave_v1`、`mode=full_order|fast_s|fast_sensitivity`、`case`、`design`、`parametersMeter`（前向两模式为 null）；支持 `--validate-only` |
| opt | `emopt_opt.exe OPT.json NEW_OUTPUT` | opt.json 自身即请求，schema `emopt.module.opt.v2`：`case`/`design`/`parametersMeter` + `targets`（kind=s11_db|s21_db，rules[]：range/unit/bound/levelDb/weight）+ `optimizer`（maxIter/costMin/alphaMin/…）；支持 `--validate-only` |

关键行为约束：
- **输出目录必须是尚不存在的新目录**（`EMOPT:OutputExists`），保证每次运行独立可审计；
- 尺寸单位 m、频率 Hz（GUI 显示 mm/GHz，接口恒为 m/Hz）；
- 退出码：0 成功；2 = InputError（stderr 一行 JSON `{"status":"ERROR","error":"EMOPT:*","message":...}`）；3 = 内部异常；
- 相对路径**以请求文件所在目录为基准**解析（`module_cli::resolve`）；
- sim/opt 运行前后做输入 SHA-256 复核（`EMOPT:ChangedModuleInput`），输出带 `meshStateId`（拓扑+坐标哈希）与完整输入绑定（`inputs[].sha256`）。

### 2.2 案例数据（四 JSON + GEO + MSH）

| 文件 | schema | 内容 |
|---|---|---|
| `case.json` | `emopt.gmsh.wave.v2` | 网格路径、lengthUnit(m)、volumes/pec/ports（TE10 端口 + electricAxis）、materials 引用、frequencySweep（Hz）、referenceFrequencyHz、order（SOAR 阶数） |
| `materials.json` | schemaVersion 3 | materials[]（materialCode/id/name/properties[]：epsilonR、muR、conductivitySm） |
| `design.json` | `emopt.design.v2` | parameters[]（name/nominalMeter/lowerMeter/upperMeter，**顺序=灵敏度方向=优化变量顺序**）、optimizationBoundsComplete、frozenGeometryParametersMeter、deformation（profile=gmsh_cad_rbf_v1、baseCase/baseMesh/geometrySource、gmshLibrary=bundled、端口刚性平移约束、RBF r_log_r/affine、validation 容差） |
| `opt.json` | `emopt.module.opt.v2` | 当前尺寸（parametersMeter）+ 目标 + 优化器设置 |
| `base.geo` / `base.msh` | Gmsh | 参数化几何（尺寸变量=米制表达式）/ 基准网格 2.2 ASCII |

### 2.3 输出（按阶段）

| 阶段 | 规范产物 |
|---|---|
| morph | `deformed.msh`、`case.json`+`materials.json`（变形后配套）、`mesh_state.json`、`nodes.bin`（小端 F64，node×xyz）、`directions.bin`（按请求，parameter×node×xyz）、`result.json`、`module_result.json`、`run.log` |
| sim | `responses.csv`（frequency_index,frequency_hz,port_out,port_in,value_real,imag,gamma_real,imag）、`residuals.csv`、`result.json`、`module_result.json`、`run.log`；灵敏度另加 `ds.csv`（+parameter_index）、`projected_tangent_residuals.csv`、`conditioning.csv` |
| optimize | `best_design.json`（parameterNames/designMeter/cost/evaluation_id）、最佳点配套 `deformed.msh`/`case.json`/`responses.csv`/`ds.csv`、`history.csv`、`trials.csv`、`target_comparison.csv`、`progress/`（GUI 轮询用）、`result.json`、`module_result.json`、`run.log` |

### 2.4 交付基线（自测报告，验收锚点）

- 例1：2640 节点/10524 四面体，自由 DOF 57784，101 频点（10.5–11.5 GHz），8 尺寸；变形 +10% 后 0 翻转 0 退化，端口截面不动；快扫 S 与独立全阶一致（S12≈S21 复差 3.8e-14）；优化初始 cost 38.6188 → 最佳 0.000336208，9 轮接受/10 次评估。
- 例2：3116 节点/11565 四面体，DOF 61768，401 频点（9–13 GHz），6 尺寸；输出端口刚性平移 9.47 mm；优化初始 7.03151 → 最佳 0.000268961。
- 交付 EXE 重编译复算 `fast_s`，`responses.csv` **逐字节一致**（数值回归方法已验证）。

### 2.5 与平台契约的差距清单（改造需求来源）

| # | 差距 | 平台规则 | 处置 |
|---|---|---|---|
| G1 | 输出目录必须新建 | manifest `produced` 要求**固定** `Result/<id>/<stage>/` 路径供 outputCheck | 模块侧加"平台输出模式"（§4.2.1） |
| G2 | stderr 错误为 `EMOPT:*` 一行 JSON | `errorContract=qs-json-stderr` 提取 `"code":"QS-*"` | 模块侧加 QS 码行（§4.2.2） |
| G3 | 无 `--data-root`/`--runtime-dir`（位置参数） | R1 允许散参数（argsTemplate 传绝对路径），R2 日志固定落 `Result/logs/<id>/` | 保持位置参数 + argsTemplate 绝对路径（合规，ONBOARDING FAQ）；run.log 落点由输出目录决定，另写 `Result/logs/emopt/` 一份（§4.2.4） |
| G4 | gmsh-4.8.dll 从 EXE 目录向上 7 级搜索（安装目录内依赖） | R3 安装目录只读/不依赖安装目录内 DLL | 支持 env `EMOPT_GMSH_LIBRARY` 覆盖（modules.json `env` 注入，先例 `GDS2GEO_GMSH_EXECUTABLE`），bundled 保留为缺省（§4.2.3） |
| G5 | EXE 动态链接 MSVC 运行库（旁侧 vcruntime140 等 9 个 DLL） | R3 静态链接（compact_em 纯静态） | 二选一：/MT 重发版（推荐）或声明例外（§4.2.5） |
| G6 | 长耗时（优化 ~16 min，含 10 次完整评估） | 当前 `def.timeoutSec=0`（`ModuleRegistry::resolveRoute` 恒 0，无超时）——**现状安全**；将来引入超时时 optimize 阶段须给足余量 | 记录为决策项（§8-D4） |
| G7 | 进度靠 `progress/` 文件（GUI 轮询） | 平台进度 = stdout 行（task.stage.log）+ 结果清单 | 模块侧加 stdout 进度行（§4.2.6）；UI 直读 progress 文件做实时曲线（前端直读产物有平台先例） |
| G8 | 自带 Qt GUI | UI 由平台统一 | 平台化后 emopt_gui 退出平台链路（保留独立工具身份，§4.5） |

---

## 三、接入形态选择

**Phase 1 采用 `kind: external`（EXE 直调）**，DLL worker 为 Phase 3 目标形态。理由：

1. external 是平台现行迁移期主通道（compact_em/gds2geo/thermal 全部 external），**平台侧零代码**，只需模块包 + manifest + 三行路由；
2. emopt 计算链本身已是独立 EXE + 独立进程，external 语义（崩溃隔离、进程级取消 kill、stdout 流日志）**天然成立**；
3. DLL 收益（worker_host 隔离加载、job.json 统一入口）对当前"单进程 16 分钟"场景收益有限，且 gmsh-4.8.dll 依赖、/MT 静态化等改造与形态选择可解耦——先 external 跑通链路，DLL 仅替换壳；
4. 预留位 `opt.optimizer` 是 DLL 占位：Phase 3 落地时 emopt 以 DLL 形态**顶替/复用**该语义（§5.4），不新建第二套优化模块。

---

## 四、Phase 1 设计（external EXE 接入）

### 4.1 模块身份与包

- **providerId = `emopt`**（模块包目录名，小写唯一）；模块包放 `<MODULE_ROOT>/emopt/`（与 `compact_em/`、`gds2geo/`、`thermal/` 平级，模块根自动推导兼容）；
- 包内容（四件套 + 随附程序，ONBOARDING §一要求"四件套"，随附程序列 README）：

```
<MODULE_ROOT>\emopt\
├── emopt_morph.exe / emopt_sim.exe / emopt_opt.exe   （/MT 静态，单文件拷走即可）
├── runtime\gmsh-4.8.dll                              （75 MB 随附；缺省 bundled，可 env 覆盖）
├── README.md        （CLI、退出码、依赖 Gmsh 4.8、版本 0.5.1）
├── ERROR_CODES.md   （QS-EMOPT-* 登记，§6.2）
└── module.manifest.json
```

- 版本：`version: "2026.11.04"`（批次）、`apiVersion: 1`、`dataLayout: "peec-data/v1"`。
- **案例包包外交付**：`emopt_cases_20261104/`（WG_4pole_1 + WG_4pole_2，§4.3 布局 + 各自 README 与验收点），平台归档 `dev/cases/emopt/`。

### 4.2 模块侧改造清单（最小，全部可选实现于 `module_cli.cpp` + 少量 I/O 封装）

> 原则：**数值路径零改动**（fem/mor/morph/optimizer 各库不动）；只动 CLI 壳与输出落位；standalone 模式（dht 原交付用法）行为不变。

#### 4.2.1 平台输出模式（解决 G1，核心改动）

CLI 不变（仍为位置参数）。语义增强：

- 输出目录**不存在**（standalone）：现状行为（新建、直接写）；
- 输出目录**已存在**（平台传入的 `Result/emopt/<stage>/`）：
  1. 实际运行写入 `<output>/runs/<yyyyMMdd-HHmmss>-<pid>/`（保证"新目录"不变式，运行全程独立可审计、失败不污染上次结果）；
  2. 成功后把**规范产物**（每阶段固定清单，见 §6.1 outputCheck 表）复制到 `<output>/` 根，覆写上一轮规范件；
  3. 写 `<output>/stage_receipt.json`（DATA_LAYOUT §七 schema v1：输入/输出指纹、关键参数、elapsed、notes），并把 `run.log` 复制一份到 `Result/logs/emopt/<stage>-<runId>.log`（R2 落点）。
- 失败时不复制、不写 receipt（保留上次规范件 + 本 run 现场在 `runs/` 内），stderr 按 §4.2.2 报 QS 码。

规范产物清单（= outputCheck）：

| stage | 复制到阶段目录根的规范件 |
|---|---|
| morph | `deformed.msh`、`case.json`、`materials.json`、`mesh_state.json`、`nodes.bin`、`result.json`、`module_result.json`、`run.log` |
| sim | `responses.csv`、`residuals.csv`、`result.json`、`module_result.json`、`run.log`（灵敏度模式另复制 `ds.csv`、`projected_tangent_residuals.csv`、`conditioning.csv`；outputCheck 只查公共件） |
| optimize | `best_design.json`、`deformed.msh`、`case.json`、`materials.json`、`responses.csv`、`ds.csv`、`history.csv`、`trials.csv`、`target_comparison.csv`、`result.json`、`module_result.json`、`run.log` |

#### 4.2.2 错误码 QS 化（解决 G2）

`main_entry` 的 stderr 错误行在原 JSON 基础上增加 `code` 字段（保留原字段，向后兼容）：

```json
{"status":"ERROR","module":"sim","code":"QS-EMOPT-INVALID-REQUEST","error":"EMOPT:InvalidModuleRequest","message":"..."}
```

映射（最终登记以 ERROR_CODES.md 为准）：

| 现 EMOPT:* | 退出码 | QS-EMOPT-* |
|---|---|---|
| `EMOPT:Usage` | 2 | `QS-EMOPT-INVALID-CLI` |
| `EMOPT:InvalidModuleRequest` / 配置字段类 | 2 | `QS-EMOPT-INVALID-REQUEST` |
| 文件缺失/不可读（`Required input file does not exist` 等） | 2 | `QS-EMOPT-INPUT-MISSING` |
| `EMOPT:OutputExists` | 2 | `QS-EMOPT-OUTPUT-EXISTS`（平台模式下不触发，保留 standalone 语义） |
| `EMOPT:ChangedModuleInput` | 3 | `QS-EMOPT-INPUT-CHANGED` |
| `EMOPT:IncompleteModule` | 3 | `QS-EMOPT-RESULT-INCOMPLETE` |
| `EMOPT:OutputError` | 3 | `QS-EMOPT-IO-ERROR` |
| 网格无效（翻转/退化超差，`is_invalid_mesh_state`） | 2 | `QS-EMOPT-MESH-INVALID` |
| `EMOPT:InternalError` | 3 | `QS-EMOPT-INTERNAL` |

`errorContract.type = "qs-json-stderr"`（对齐 compact_em 先例；task_layer 提取最后一条 error/fatal 级 QS 码透传，UI 按码分支）。

#### 4.2.3 gmsh 库路径可配置（解决 G4）

`deformation.gmshLibrary` 现状支持 `"bundled"` 或显式路径（`simplified_input::deformation_document`）。补一层覆盖：**env `EMOPT_GMSH_LIBRARY` 优先于 bundled 搜索**（显式路径配置仍最高优先）。modules.json 路由注入：

```json
"env": { "EMOPT_GMSH_LIBRARY": "{MODULE_ROOT}/emopt/runtime/gmsh-4.8.dll" }
```

（先例：gds2geo 各阶段的 `GDS2GEO_GMSH_EXECUTABLE` 注入。bundled 缺省保留并声明 `deprecatedModes: ["bundled-gmsh-dll-search"]`。）

#### 4.2.4 日志落点（G3 之 R2）

平台模式下阶段 `run.log` 复制一份到 `Result/logs/emopt/`（带 runId 前缀）；原输出目录内 run.log 保留。

#### 4.2.5 静态链接（解决 G5，推荐）

三 EXE 改 `/MT`（Eigen MPL2、nlohmann 头文件、gmsh 走 dll 调用均不受影响），交付单文件 EXE，包内除 `runtime/gmsh-4.8.dll` 无其他 DLL。**验收含数值一致性**：重编译后对两案例跑 `fast_s`，`responses.csv` 与交付 EXE 基线对照（交付自测已验证"重编译逐字节一致"，/MT 仅换 CRT 堆实现，预期不变；如有差异按 1e-12 量级核对并留痕）。
备选（若 /MT 引入不可控风险）：动态链接 + 包内 MSVC 运行库 DLL，manifest 声明环境依赖、平台部署保证包目录只读可加载——不推荐。

#### 4.2.6 stdout 进度行（解决 G7）

- optimize：每次评估结束输出一行 `[emopt-opt] eval=7 iteration=5 cost=0.00211 best=0.00189 params=[...]`；线搜索接受/拒绝不逐行（防刷屏），迭代接受行必发；
- sim：快扫每 25% 频点一行、灵敏度每 25% 频点一行（`[emopt-sim] mode=fast_sensitivity 100/401 freqs`）；
- morph：`[emopt-morph] boundary=... rbf=... validation=VALID` 收尾一行。
这些行进 task_layer 的 `task.stage.log` 通知，UI 消息面板直接可见；**实时曲线不依赖 stdout**（UI 直读 `Result/emopt/optimize/progress|journal/` 与 `Result/emopt/<stage>/runs/` 当前 run，见 §5.3）。

#### 4.2.7 不改项（红线）

- 三个请求 schema（v2）字段、数值行为、CSV 列定义、mesh_stateId 计算：全部不动；
- standalone 模式（dht 原交付命令）行为不动（案例复现说明.txt 命令继续可用）；
- `--validate-only` 保留（平台 inputs.validate 做浅检，模块深检留在启动时快失败，§5.3）。

### 4.3 dataRoot 布局（FEM-opt 链路约定）

新链路的 dataRoot 子约定（不改动 EM/热链路既有文件与不变式；`Model/*.msh` 恰好一份的 I1 不变式指 EM 链主网格，FEM-opt 基准网格放 `Model/femopt/` 子目录不冲突）：

```
<dataRoot>\
├── Model\femopt\                     【UI 写，跨任务持久】案例包（自 dht 案例导入）
│   ├── base.geo                      参数化几何（m）
│   ├── base.msh                      基准网格（MSH 2.2 ASCII，m）
│   ├── case.json                     emopt.gmsh.wave.v2
│   ├── materials.json                schemaVersion 3
│   └── design.json                   emopt.design.v2
├── Setting\femopt\
│   └── opt.json                      emopt.module.opt.v2（当前尺寸+目标+优化器）【UI 写】
├── Result\
│   ├── emopt\morph\                  【emopt 独占】+ runs\<runId>\ 运行现场
│   ├── emopt\sim\
│   ├── emopt\optimize\
│   ├── logs\emopt\                   诊断日志（runId 前缀）
│   └── tasks\...                     task_layer 所有（模块不写不读）
```

**逐次运行的请求文件**（含本次所选尺寸、mode/purpose、emitNodeJacobian 等，dht GUI 每次运行生成 `requests/*.json` 的等价物）：
- 由 **UI 生成**（用户勾选参与尺寸、选模式、填目标 → 落盘）；
- 请求内 `case`/`design` 引用**用绝对路径**（指向 `Model/femopt/`、`Setting/femopt/`），避免依赖请求文件落位；
- 经 task_layer **`inputs` 动态输入通道**下发：`inputs.femRequest` → 变量 `{FEM_REQUEST}`（先例 `portJson → {PORT_JSON}`），写入 state.json `inputs` 留痕；`inputs.validate` 校验该文件存在、JSON 合法（深检 schema 由模块启动时完成）。

### 4.4 manifest 草案（模块包内，提交前过 JSON Schema 校验）

```jsonc
{
  "manifestVersion": 1,
  "id": "emopt",
  "name": "有限元仿真优化（Gmsh 参数化 FEM · 变形/S参数/灵敏度/优化）",
  "version": "2026.11.04",
  "apiVersion": 1,
  "cli": {
    "executables": ["emopt_morph.exe", "emopt_sim.exe", "emopt_opt.exe"],
    "deprecatedModes": ["bundled-gmsh-dll-search", "standalone-new-output-dir"]
  },
  "stages": [
    {
      "id": "morph",
      "executable": "emopt_morph.exe",
      "argsTemplate": [
        "{dataRoot}/Model/femopt/design.json",
        "{FEM_REQUEST}",
        "{dataRoot}/Result/emopt/morph"
      ],
      "allowedArgs": [],
      "exitCodes": { "0": "ok", "2": "input_or_config", "3": "execution" },
      "errorContract": { "type": "qs-json-stderr" },
      "consumesInternal": [],
      "producesInternal": ["emopt/morph.deformed"]
    },
    {
      "id": "sim",
      "executable": "emopt_sim.exe",
      "argsTemplate": [ "{FEM_REQUEST}", "{dataRoot}/Result/emopt/sim" ],
      "allowedArgs": [],
      "exitCodes": { "0": "ok", "2": "input_or_config", "3": "execution" },
      "errorContract": { "type": "qs-json-stderr" }
    },
    {
      "id": "optimize",
      "executable": "emopt_opt.exe",
      "argsTemplate": [ "{dataRoot}/Setting/femopt/opt.json", "{dataRoot}/Result/emopt/optimize" ],
      "allowedArgs": [],
      "exitCodes": { "0": "ok", "2": "input_or_config", "3": "execution" },
      "errorContract": { "type": "qs-json-stderr" }
    }
  ],
  "artifacts": {
    "produced": [
      { "ref": "emopt/morph.deformed", "path": "Result/emopt/morph/deformed.msh",
        "schemaVersion": "2.2", "units": { "length": "m" } },
      { "ref": "emopt/sim.responses",  "path": "Result/emopt/sim/responses.csv",   "schemaVersion": "1" },
      { "ref": "emopt/sim.derivatives","path": "Result/emopt/sim/ds.csv",          "schemaVersion": "1" },
      { "ref": "emopt/opt.best",       "path": "Result/emopt/optimize/best_design.json", "schemaVersion": "1" }
    ],
    "consumed": [
      { "ref": "user/fem.geo",       "path": "Model/femopt/base.geo",       "maintainedBy": ["ui"], "accept": { "extensions": [".geo"] } },
      { "ref": "user/fem.mesh",      "path": "Model/femopt/base.msh",       "maintainedBy": ["ui"], "accept": { "extensions": [".msh"] } },
      { "ref": "user/fem.case",      "path": "Model/femopt/case.json",      "maintainedBy": ["ui"], "accept": { "schemaVersion": { "min": 2, "max": 2 } } },
      { "ref": "user/fem.materials", "path": "Model/femopt/materials.json", "maintainedBy": ["ui"], "accept": { "schemaVersion": { "min": 3, "max": 3 } } },
      { "ref": "user/fem.design",    "path": "Model/femopt/design.json",    "maintainedBy": ["ui"], "accept": { "schemaVersion": { "min": 2, "max": 2 } } },
      { "ref": "user/fem.optset",    "path": "Setting/femopt/opt.json",     "maintainedBy": ["ui"], "accept": { "schemaVersion": { "min": 2, "max": 2 } } },
      { "ref": "user/fem.request",   "path": "{FEM_REQUEST}",               "maintainedBy": ["ui"] }
    ]
  },
  "dependsOn": {
    "modules": [],
    "artifacts": ["user/fem.geo", "user/fem.mesh", "user/fem.case",
                  "user/fem.materials", "user/fem.design", "user/fem.optset", "user/fem.request"],
    "environment": { "gmsh": { "version": "4.8", "requiredBy": ["morph", "sim", "optimize"],
                                "note": "native C API via gmsh-4.8.dll；env EMOPT_GMSH_LIBRARY 注入" } }
  },
  "capabilities": { "dataLayout": "peec-data/v1", "breakingChanges": [] }
}
```

> 注意：`user/fem.request` 的 path 含动态变量，若 manifest schema 不支持 `{FEM_REQUEST}` 变量，则该项从 `consumed` 移除（存在性由 `inputs.validate` 的 inputs 通道校验承担），argsTemplate 引用不变。**Phase 0 合同会议确认**（§8-D5）。
> `accept.schemaVersion`：emopt case/design 的 schema 值为字符串 `emopt.gmsh.wave.v2` 而非整数——accept 的整数区间不适用，**改为字段存在性检查**（`accept.fields` 或仅 extensions），schema 深检归模块（浅检/深检边界，MANIFEST_SPEC §一）。上草案中 schemaVersion 区间为示意，Phase 0 定稿。

### 4.5 modules.json 路由（平台侧，3 行）

```jsonc
"emopt.morph":    { "displayName": "FEM 网格变形（GEO 尺寸驱动 + RBF）",
                    "kind": "external", "provider": "emopt", "stage": "morph",
                    "env": { "EMOPT_GMSH_LIBRARY": "{MODULE_ROOT}/emopt/runtime/gmsh-4.8.dll" },
                    "searchDirs": ["{MODULE_ROOT}/emopt"] },
"emopt.sim":      { "displayName": "FEM S 参数与伴随灵敏度（SOAR 快扫）",
                    "kind": "external", "provider": "emopt", "stage": "sim",
                    "env": { "EMOPT_GMSH_LIBRARY": "{MODULE_ROOT}/emopt/runtime/gmsh-4.8.dll" },
                    "searchDirs": ["{MODULE_ROOT}/emopt"] },
"emopt.optimize": { "displayName": "FEM 有界优化（有界 BFGS + 伴随灵敏度）",
                    "kind": "external", "provider": "emopt", "stage": "optimize",
                    "env": { "EMOPT_GMSH_LIBRARY": "{MODULE_ROOT}/emopt/runtime/gmsh-4.8.dll" },
                    "searchDirs": ["{MODULE_ROOT}/emopt"] }
```

`workingDir` 缺省 `{DATA}` 即可（emopt 相对路径全部锚定请求文件目录，不依赖 CWD；`bundled_library()` 锚定 EXE 目录）。

**emopt_gui.exe 不在平台链路内**：迁移期保留为独立工具（dht 交付原样）；平台 UI 提供等价功能后（Phase 2）其"平台工具"身份结束，代码继续独立演进。

### 4.6 流水线与调用方式

三个 EXE 各自自包含（请求文件自带当前尺寸，optimize 内部自行闭环 morph+sim），**阶段间无强制数据流**。Phase 1 调用面：

1. **主用：`module.run` 单模块**（task_layer 现有能力，零新增 pipeline）：
   - `module.run {moduleId:"emopt.morph"|"emopt.sim"|"emopt.optimize", dataRoot, inputs:{femRequest:"..."}}`
   - `inputs.validate {module:"emopt.sim", dataRoot, inputs:{...}}` 先浅检；
2. **可选演示流水线**（价值低，Phase 1 可不做）：`fem_preview` = `morph → sim`（变形预览 + 变形后快扫；sim 请求由 UI 指向 morph 产物目录的 `case.json`，经 pipeline stage `artifacts` 路由或 UI 绝对路径完成）；
3. 优化任务**不走流水线**（单阶段长任务），用 `module.run emopt.optimize`。

与 `docs/plans/PIPELINE_OPTIMIZATION_20261104.md` 的关系：P1（selection）/P1.5（user gate）落地后，"优化前用户确认目标"可落为 `userGate.confirm`，"批量多案例扫描"落为任务队列（§4.5 预留，均不改本期设计）。

### 4.7 状态、进度、结果、取消

| 关注点 | 机制 |
|---|---|
| 阶段状态机 | task_layer `state.json`（pending/running/done/failed/cancelled），模块不感知 |
| 成败判定 | 退出码（0/2/3）+ 浅检 outputCheck（§4.2.1 规范件存在且非空）+ QS 码透传 |
| 实时进度 | ① stdout 行 → `task.stage.log` 通知（§4.2.6）；② UI 直读 `Result/emopt/optimize/progress|journal/` 画 cost 曲线与响应曲线（**前端直读模块产物**，与"S 参数绘图由前端直接读 CSV"同先例；路径经 `task.status` 的 outputs 清单告知 UI） |
| 结果获取 | `task.result` → `state + resultFiles`（outputCheck 清单 + publishes）；UI 读 `best_design.json`/`responses.csv`/`ds.csv`/`history.csv`/`target_comparison.csv` 渲染（S 曲线、dS 灵敏度、cost 收敛、目标逐点对照、最佳尺寸表） |
| 取消 | `task.cancel` → kill 进程 → `QS-TL-CANCEL`；emopt 侧 `runs/<runId>/` 现场保留（journal/progress 含最佳接受点），与 dht GUI 的"停止后保留部分文件"语义一致 |
| 超时 | 现状 `timeoutSec=0` 无超时；optimize ~16 min 安全。若未来平台引入超时：optimize 阶段建议 `≥4 h`（含 20 迭代 × 大算例余量），登记 manifest 决策项（§8-D4） |
| 重跑 | 平台模式下重复 `module.run` 合法（新 runId + 规范件覆写）；standalone 语义（必须新目录）仅对 dht 独立工具保留 |

### 4.8 输入校验分工

- **浅检（task_layer，manifest 驱动）**：`Model/femopt/*`、`Setting/femopt/opt.json`、`{FEM_REQUEST}` 存在且非空；
- **深检（模块启动即做，快失败）**：v2 请求 schema 严格校验（字段集合精确匹配）、case/design 一致性（`matches_base_case`、参数范围、bounds 完整性）、网格/材料加载——全部复用现有 `validate_*` 逻辑，错误经 §4.2.2 QS 码透传；
- `--validate-only` 不暴露为平台调用（inputs.validate 不执行模块）；保留供模块侧自测与 Phase 3 job.json 预检模式。

---

## 五、平台侧与 UI 侧工作项

### 5.1 平台侧（task_layer，预期零代码）

| # | 工作 | 载体 |
|---|---|---|
| P-1 | 放置模块包 `<MODULE_ROOT>/emopt/`（四件套 + runtime/） | 部署 |
| P-2 | `modules.json` 增 3 条路由（§4.5） | 配置 |
| P-3 | 案例包归档 `dev/cases/emopt/`（两案例，§4.3 布局） | 归档 |
| P-4 | `dev_case.ps1 -Module emopt -Stage morph/sim/optimize` 回归（现有工具，若脚本按 provider 目录名取包则零改动；案例包结构对齐 §4.3 即可） | 验证 |
| P-5 | 注册期校验通过（schema/唯一生产者/DAG/apiVersion 窗口）+ 三条模块 `inputs.validate` + 数值基线回归（§7） | 验收 |
| P-6 | （可选）`fem_preview` pipeline 条目 | 配置 |

> 依赖确认：`inputs` 通道变量（`{FEM_REQUEST}`）在 argsTemplate 中展开、且 `inputs.validate` 校验 inputs 文件——现有能力（README §二、ONBOARDING 先例 portJson），**无新 RPC**；若 schema 对 manifest consumed 的变量路径受限（§4.4 注），按 Phase 0 结论微调 manifest，仍不动代码。

### 5.2 UI 侧（独立团队，列需求不列实现）

1. **案例导入**：选 dht 案例文件夹（或案例包）→ 按 §4.3 布局写入 dataRoot（`Model/femopt/` 五件 + `Setting/femopt/opt.json`）；
2. **参数编辑**（对齐 dht GUI 左栏）：勾选参与尺寸、标称/上下界、扫频 start/stop/step、f0、SOAR order、材料（只读展示）；保存即改写 `Model/femopt/design.json`、`case.json`、`Setting/femopt/opt.json`（单位转换：界面 mm/GHz ↔ 文件 m/Hz）；
3. **运行请求生成**：按当前编辑生成 `{FEM_REQUEST}`（morph/sim/opt 三种请求；optimize 的 targets/optimizer 同步进 `opt.json`）；
4. **发起**：`inputs.validate` → `module.run`；
5. **进度展示**：消息面板（task.stage.log）+ cost 实时曲线（读 `Result/emopt/optimize/progress|journal/`）+ 频点进度（sim stdout 行）；
6. **结果展示**（阶段完成后）：S 曲线（responses.csv）、灵敏度 dS/dp（ds.csv + parameterNames）、cost 收敛（history.csv）、目标逐点（target_comparison.csv）、最佳尺寸（best_design.json）、变形网格质量（mesh_state.json）；
7. **取消**：task.cancel + 提示"保留 runs/ 现场与最佳接受点"。

### 5.3 数据流总览（平台化后单次优化）

```
UI: 导入案例 → 编辑参数/目标
  → inputs.validate {module:emopt.optimize, inputs:{femRequest}}
  → module.run {emopt.optimize, inputs:{femRequest: <UI 生成的请求>}}
      task_layer: 浅检 → 启动 emopt_opt.exe opt.json {FEM_REQUEST? 见 §4.4} Result/emopt/optimize
      emopt_opt: 深检 → 内部循环{morph → fast_sensitivity → cost/gradient → BFGS+线搜索}
                 stdout 进度行 → task.stage.log；progress/ 实时更新
  → task.finished done（~16 min）
UI: task.result → 读 best_design.json / responses.csv / ds.csv / history.csv / target_comparison.csv 渲染
```

（注：optimize 阶段按 §4.4 草案以 `Setting/femopt/opt.json` 为请求入口——opt.json 自身即 v2 请求，与 dht 现行 CLI 完全一致；sim/morph 请求经 `{FEM_REQUEST}` 下发。）

### 5.4 预留位 `opt.optimizer` 处置（决策项，建议）

- Phase 1：`opt.optimizer`（dll 占位）**保留不动**（`available=false` 被 UI 过滤，无副作用）；
- Phase 3（emopt DLL 化）：emopt DLL 顶替该语义——二选一：
  (a) 保留 `opt.optimizer` 模块 id，把 provider 指向 `emopt` 包（manifest stage `optimize`），仅优化走 DLL，morph/sim 仍 external（形态混合，简单）；
  (b) 三阶段全部转 `emopt.*` 三模块（kind=dll，`workerHost` 指向 worker_host），删除 `opt.optimizer` 占位（干净，推荐）；
- 对应 `sim.solver`（MNA/ALPS 电路求解）与 emopt 无关，**不动**。

---

## 六、交付物清单

### 6.1 模块团队（emopt 侧）

| # | 交付 | 说明 |
|---|---|---|
| M-1 | 三 EXE（/MT 静态，含 §4.2 全部改动） | 数值零改动；standalone 模式兼容 |
| M-2 | `README.md` | CLI（位置参数 + env + 进度行）、退出码 0/2/3、依赖（Gmsh 4.8 native、单线程）、版本、standalone 与平台双模式说明 |
| M-3 | `ERROR_CODES.md` | QS-EMOPT-* 全量登记（code/触发/可恢复性/处置），覆盖 §4.2.2 表全部路径 |
| M-4 | `module.manifest.json` | §4.4 草案定稿 + JSON Schema 校验通过 |
| M-5 | `emopt_cases_20261104/`（包外） | 两案例按 §4.3 布局 + README（输入来源/验收点/已知局限：固定拓扑直边平面模型、单线程、costMin 0.001 自测口径） |

### 6.2 平台团队

| # | 交付 | 说明 |
|---|---|---|
| P-1…P-6 | §5.1 工作项 | 配置 + 归档 + 回归 |

### 6.3 UI 团队

| # | 交付 | 说明 |
|---|---|---|
| U-1…U-7 | §5.2 工作项 | 独立排期，依赖 Phase 1 注册完成 |

---

## 七、验收与回归基线

### 7.1 数值基线（dev/cases/emopt 两案例，单线程，Windows x64）

| 验收点 | WG_4pole_1 | WG_4pole_2 |
|---|---|---|
| morph：状态/翻转/退化 | VALID / 0 / 0 | VALID / 0 / 0 |
| morph：节点导数 shape | 8×2640×3 | 6×3116×3 |
| fast_s：responses.csv 行数 | 404 | 1604 |
| fast_s：数值 | 与交付 EXE 基线对照（/MT 重编译后按 §4.2.5 核对，交付自测为逐字节一致） | 同左 |
| sensitivity：ds.csv 行数 | 3232 | 9624 |
| sensitivity：S 与独立快扫最大复差 | 0 | 0 |
| optimize：初始/最佳 cost | 38.6188 / 0.000336208 | 7.03151 / 0.000268961 |
| optimize：接受迭代/评估 | 9 / 10 | 9 / 10 |
| optimize：最佳尺寸 | 在 ±10% 界内，与自测报告 best_design.json 对照（浮点容差 1e-9 相对） | 同左 |
| 耗时量级（参考，不作硬判据） | morph 11.4 s / sim 38/174 s / opt 869 s | morph 9.8 s / sim 71/150 s / opt 967 s |

### 7.2 平台链路验收

1. 注册期：schema 校验、唯一生产者、DAG、apiVersion 窗口全部通过；`modules.list` 三模块 `available=true`；
2. `inputs.validate`（三模块）：缺件/坏 JSON/请求缺字段三类负例各一，issues 正确；
3. `module.run` 三模块正例：state.json 全 done、resultFiles 齐全、stdout 进度行可见、stage_receipt.json 落位、`Result/logs/emopt/` 有日志；
4. 负例：坏请求（schema 缺字段）→ `QS-EMOPT-INVALID-REQUEST` 透传；输入运行中被改（可构造）→ `QS-EMOPT-INPUT-CHANGED`；
5. 取消：optimize 运行中 `task.cancel` → `QS-TL-CANCEL`，`runs/` 现场与 journal 保留；
6. 重跑：同 dataRoot 连续两次 `module.run emopt.morph` → 两次 runId、规范件为最新一次；
7. E2E：UI（或 rpc_smoke 变体脚本）走 validate → run → result 全链，两案例数值基线（§7.1）复现。

### 7.3 回归红线

- compact_em / gds2geo / thermal 三链 E2E 与数值基线不变（本方案不触碰其配置，仅需打包后冒烟确认 modules.json 改动无副作用）；
- emopt standalone 模式（dht 复现说明.txt 八条命令）在改动后仍可原样复现。

---

## 八、分期路线与开放问题

### 8.1 分期

| 期 | 内容 | 交付判据 | 负责 | 估时（建议） |
|---|---|---|---|---|
| **Phase 0** | 合同会议：§8.2 决策项 D1–D6 定稿（命名/布局/错误码/超时/manifest 变量路径/GMsh 依赖声明） | 决策纪要 + manifest 定稿 | 三方 | 0.5 d |
| **Phase 1a** | 模块侧改造（§4.2 六项）+ /MT 静态化 + 双模式自测 | standalone 八命令复现 + 平台模式冒烟（手工命令）+ 数值基线 | emopt 团队 | 3–5 d |
| **Phase 1b** | 四件套 + 案例包 + 平台注册 + dev_case + E2E | §7 全项通过 | 双方 | 2–3 d |
| **Phase 2** | UI 接入（§5.2 U-1…U-7）+ 优化结果渲染 | 两案例 UI 全链可跑、实时 cost 曲线、结果页 | UI 团队 | 独立排期（1–2 周量级） |
| **Phase 3** | DLL worker 目标形态：`qs_peec_module_entry` 壳（emopt 计算库已分层，壳 = 解析 job.json → 调 `emopt_modules` 现有入口函数）+ worker_host 集成 + 崩溃/内存隔离验证 + `opt.optimizer` 占位处置（§5.4 方案 b） | worker 模式数值与 EXE 模式一致；kill/crash 不伤 task_layer | emopt + 平台 | 3–5 d |
| **Phase 4**（可选/长期） | GDS 链路衔接：消费 `Model/model_em.msh`（mm）/`matdb.json`；单位适配（m↔mm）；参数化 GEO 来源（CAD/参数化 GDS）；批量多案例优化（任务队列，依赖 PIPELINE_OPTIMIZATION P4） | 另立设计 | 三方 | — |

### 8.2 开放问题（Phase 0 决策项）

| # | 问题 | 倾向 |
|---|---|---|
| D1 | providerId 命名：`emopt`（随源码/EXE 命名，推荐）vs `femopt` | emopt |
| D2 | dataRoot 文件命名：`Model/femopt/` 五件 + `Setting/femopt/opt.json`（§4.3，推荐）vs 案例包原样目录名映射 | §4.3 |
| D3 | gmsh-4.8.dll（75 MB）随模块包 vs 平台共享目录 | 随包（env 注入路径，部署只读可加载） |
| D4 | optimize 阶段未来超时值（现状 0=不超时） | 引入超时时 ≥4 h；manifest 预留 `timeoutSec` 声明 |
| D5 | manifest `consumed` 是否支持 `{FEM_REQUEST}` 动态路径（§4.4 注）；不支持则该项移出 consumed，由 inputs.validate 承担 | 会议确认 task_layer 行为 |
| D6 | `accept.schemaVersion` 对字符串 schema（`emopt.gmsh.wave.v2`）的表达能力；必要时仅用字段存在性浅检，schema 深检归模块 | 会议确认 |
| D7 | emopt_gui.exe 去留：平台 UI 覆盖后是否从交付中移除 | 迁移期保留独立工具，平台化后由产品决定 |
| D8 | 单位约定长期策略：FEM-opt 链恒 m/Hz（case.json lengthUnit），与 EM 链 mm 约定（I2）并存；UI 负责换算 | 接受并存，UI 换算 |

### 8.3 风险

| 风险 | 等级 | 缓解 |
|---|---|---|
| /MT 静态化引入数值/内存行为差异 | 低 | 数值基线回归（§7.1）+ 交付自测已验证重编译一致性；差异即回退 G5 备选（包内运行库 DLL） |
| 16 min 长任务期间 UI 断连/用户误操作 | 中 | 现状 inputClosed→cancel 语义保留；runs/ 现场保留 + 最佳接受点可恢复 |
| `{FEM_REQUEST}` 变量路径在 manifest/validate 链路的行为不确定性 | 中 | Phase 0 实测确认（D5）；最坏情况 UI 将请求写入 dataRoot 固定位（如 `Setting/femopt/run_request.json`），argsTemplate 改固定路径，仅损失"每次运行独立文件名"的审计粒度 |
| 两案例固定拓扑直边平面假设对通用模型不成立（emopt 声明的适用范围） | 中 | 文档明示适用域；mesh 无效时 `QS-EMOPT-MESH-INVALID` 快失败 |
| gmsh 4.8 版本漂移（平台 gds2geo 用 4.11.1） | 低 | emopt 自带 4.8 dll，互不依赖；env 注入路径隔离 |
| 优化 10 次评估 × 单次评估 ~60–90 s，大算例（>10k 四面体）耗时线性增长 | 中 | 文档给出耗时估算式；UI 运行前提示量级；（长期）Phase 4 队列化 |

---

## 九、附录

### 9.1 emopt 请求 schema 速查（Phase 1 不变）

- morph：`{"schema":"emopt.morph.request.v1","operation":"apply","purpose":"validation|optimization","parametersMeter":{...},"contextFile":null,"emitNodeJacobian":true|false}`
- sim：`{"schema":"emopt.module.sim.v2","profile":"gmsh_wave_v1","mode":"full_order|fast_s|fast_sensitivity","case":"...case.json","design":null|"...design.json","parametersMeter":null|{...}}`
- opt：`{"schema":"emopt.module.opt.v2","profile":"gmsh_wave_v1","case":"...","design":"...","parametersMeter":{...},"targets":{...},"optimizer":{...}}`

### 9.2 dht 案例 → 平台 dataRoot 映射

| dht 案例目录（现状） | 平台 dataRoot（Phase 1） |
|---|---|
| `WG_4pole_1/base.geo` / `base.msh` | `Model/femopt/base.geo` / `base.msh` |
| `WG_4pole_1/case.json` / `materials.json` / `design.json` | `Model/femopt/`（同名） |
| `WG_4pole_1/opt.json` | `Setting/femopt/opt.json` |
| `runs/run-*/`（GUI 运行现场） | `Result/emopt/<stage>/runs/<runId>/` |
| `自测记录/`（复现输入快照） | 案例包 README 附复现命令（standalone 模式，§7.3） |

### 9.3 代码锚点（emopt 侧改造位置，2026-11-04 基线）

| 改动 | 位置 |
|---|---|
| 平台输出模式（runs/ + 规范件复制 + stage_receipt） | `src/module_cli.cpp`（`main_entry` 输出目录判定、`result_meta`/`simulation_result` 落位后追加复制步骤）+ 新增小工具函数 |
| QS 错误码 | `src/module_cli.cpp` `main_entry` catch 分支（stderr JSON 增 `code` 字段）+ `InputError.id` 映射表 |
| gmsh 库 env 覆盖 | `src/simplified_input.cpp` `bundled_library()` 前置 env 读取 |
| stdout 进度行 | `src/gmsh_optimization.cpp`（优化循环迭代点）、`src/gmsh_wave_fast.cpp`（快扫频点）、`src/morph.cpp`（收尾） |
| /MT | `02_源码/build.ps1` 或 CMake `CMAKE_MSVC_RUNTIME_LIBRARY` |
| DLL 壳（Phase 3） | 新增 `apps/dll_main.cpp`：导出 `qs_peec_module_entry`，解析 job.json（args 字段）→ 复用 `emopt::modules::main_entry` |
