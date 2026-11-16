# GDS2GEO 1.3.0_fix · 几何、网格、编辑与检测修复接口

本次为 manifest 阶段声明修订，交付版本 **1.3.0_fix**；五个 EXE 与 DLL 均沿用 2026-11-12 冻结原件，程序 `--version` 不变。按外部 `docs_1.8/` 规范校验。新材料三件套等后续工作树功能不在本批范围内。

交付批次：`GDS2GEO-1.3.0_fix-20261116`，Windows x64。产品及 `gds2geo.exe`、`geo2msh.exe`、`geoedit.exe` 版本为 **1.3.0**；随附独立工具 `mshcheck.exe` 为 **0.23.0**，`mshfix.exe` 为 **1.5.0**。平台 manifest 使用 `manifestVersion: 1`、`apiVersion: 5`，数据布局标识仍为 `peec-data/v1`。文件身份以交付 manifest 中的 SHA-256 为准。

## 相对 r2 的四项主要更新

1. **GEOEDIT 点、线、面编辑**：新增独立 `geoedit.exe`，按实际位置嵌入或切分几何，支持累计编辑、原位更新、另存和保存重开；原材料参数保留，配套 GEO2MSH 直接剖分保存的实际 GEO。
2. **MSH 检测**：随附 `mshcheck.exe`，检查三角形表面并输出问题和定位证据。
3. **MSH 修复**：随附 `mshfix.exe`，对支持的输入生成计划、修复/重网格并独立复检。
4. **GEO → MSH 的 dim 传递**：新完整 MSH 保持 GEO 坐标尺度并保存单位比例，不再统一转换为 mm。

薄导体默认阈值等属于现有参数调整，不列为新增功能。材料文件仍为现有 `matdb.json`；`mapping.json + materials.json` 三件套迁移尚未实施。

MSH 两个工具是独立调用能力，未自动接入几何/网格主流水线。**当前 mshfix 1.5.0 拒绝含 `$GDS2GEOUnits` 的源 MSH，也不支持 Tet4 体网格；不能直接修复本版带 dim 的正式网格。** mshcheck 可跳过该单位段读取，但不解释或校验 dim，对含体单元的文件也只检查 Tri3 表面。接收方不得将表面检测成功当作体网格验收。

## 1. 安装布局和依赖

模块压缩包名为 `GDS2GEO-1.3.0_fix-20261116.zip`，展开后包含：

```text
GDS2GEO-1.3.0_fix-20261116/
├── gds2geo/
│   ├── gds2geo.exe
│   ├── geo2msh.exe
│   ├── geoedit.exe
│   ├── mshcheck.exe
│   ├── mshfix.exe
│   ├── README.md
│   ├── ERROR_CODES.md
│   └── module.manifest.json
└── tools/gmsh/
    └── gmsh-4.11.dll
```

将 `gds2geo/` 放到平台部署根 `{MODULE_ROOT}/gds2geo/`。**建议将随附 `gmsh-4.11.dll` 放入平台共享 `tools/gmsh/` 文件夹**，与平台已提供的 `gmsh.exe` 并列；两个文件均须是配套的 **Gmsh 4.11.1 / Windows x64**。本模块包不重复携带 `gmsh.exe`。第三方许可材料单独放在 `GDS2GEO-1.3.0-third-party.zip`，必须与二进制一同转交并保留，不计入模块目录八文件。

三个主程序静态集成编译器运行库和内部 MATDB 0.1.0；构建依赖为 Clipper2 2.0.1、CLI11 2.6.2、nlohmann/json 3.12.0。生成几何不调用 Gmsh；网格化和编辑通过外置 `gmsh.exe` 工作。`mshcheck` 不依赖 Gmsh，`mshfix` **直接导入 `gmsh-4.11.dll`**，包括查询版本、帮助和 capabilities 时也须先让 Windows 找到 DLL。

两个依赖路径需分别配置：

- `GDS2GEO_GMSH_EXECUTABLE`：指向 `tools/gmsh/gmsh.exe` 的**绝对路径**，供 GEO2MSH/GEOEDIT 使用。程序仍按“EXE 同目录的旧 Gmsh → 此环境变量 → PATH”解析，因此新模块目录不要混放其他版本 `gmsh.exe`。
- 子进程 `PATH`：**启动 mshfix 之前**，前置平台 `tools/gmsh/` 的绝对目录。仅设置 `GDS2GEO_GMSH_EXECUTABLE` 不能帮助 Windows 加载 DLL；无需修改系统全局 PATH，也不要依靠当前工作目录找 DLL。

PowerShell 配置示例，后文命令共用这些变量；将前三个路径改为实际位置：

```powershell
$moduleRoot = 'D:/PEEC_Studio'
$dataRoot = 'D:/Cases/demo1'
$gmshDir = Join-Path $moduleRoot 'tools/gmsh'
$moduleDir = Join-Path $moduleRoot 'gds2geo'
$geo = Join-Path $moduleDir 'gds2geo.exe'
$mesh = Join-Path $moduleDir 'geo2msh.exe'
$editor = Join-Path $moduleDir 'geoedit.exe'
$detector = Join-Path $moduleDir 'mshcheck.exe'
$healer = Join-Path $moduleDir 'mshfix.exe'
$result = Join-Path $dataRoot 'Result/gds2geo'
$runtime = Join-Path $dataRoot 'Result/logs/gds2geo'
$gds = Join-Path $dataRoot 'Model/model.gds'
$tech = Join-Path $dataRoot 'Model/model_info.csv'
$env:GDS2GEO_GMSH_EXECUTABLE = Join-Path $gmshDir 'gmsh.exe'
$env:PATH = $gmshDir + [IO.Path]::PathSeparator + $env:PATH
& $geo --version
& $mesh --version
& $editor --version
& $detector --version
& $healer --version
```

`$moduleRoot`、`$dataRoot` 必须是绝对路径。上述 PATH 变更只影响当前 PowerShell 及其子进程。显式 CLI 的相对路径按工作目录解释，平台接入统一传绝对路径。数据目录需可写，安装目录可以只读。

Qt 原生桌面程序使用参数数组，并在启动前设置子进程环境，例如：

```cpp
// moduleRoot 和 dataRoot 是宿主已确认的绝对路径。
const QString gmshDir = QDir(moduleRoot).filePath("tools/gmsh");
const QString moduleDir = QDir(moduleRoot).filePath("gds2geo");
QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
env.insert("PATH", QDir::toNativeSeparators(gmshDir) + QDir::listSeparator()
                   + env.value("PATH"));
env.insert("GDS2GEO_GMSH_EXECUTABLE", QDir(gmshDir).filePath("gmsh.exe"));
QProcess process;
process.setProcessEnvironment(env);
process.setWorkingDirectory(dataRoot);
process.setProgram(QDir(moduleDir).filePath("mshfix.exe"));
process.setArguments({"capabilities", "--json"});
process.start();
// 通过 readyReadStandardOutput / readyReadStandardError 持续读取输出；
// 通过 errorOccurred / finished 检查启动状态、exitStatus 与 exitCode。
```

不要拼接 `cmd /c` 命令字符串。宿主持续读取 stdout/stderr，记录进程状态及退出码；取消时管理完整子进程树。MSH 修复工具还会启动检测器。

## 2. 几何和网格主接口

六个请求组合仍由 manifest `capabilities.executionProfiles` 指定。`prepared` 表示停在几何，`mesh` 表示生成几何后继续网格；`both` 调用使用原生双目标入口，不再重复执行两个单目标。已有独立合格几何包可直接网格化。

### 2.1 单目标几何

```powershell
& $geo --gds $gds --tech $tech --target em --output-dir "$result/em_geo"
if ($LASTEXITCODE -ne 0) { throw 'EM geometry failed' }
```

```powershell
& $geo --gds $gds --tech $tech --target thermal --output-dir "$result/thermal_geo"
if ($LASTEXITCODE -ne 0) { throw 'Thermal geometry failed' }
```

两者必填 `--gds FILE --tech FILE --target em|thermal --output-dir DIR`。可用 `--geo-name NAME.geo` 改主名，默认分别为 `model_em.geo`、`model_thermal.geo`，JSON 自动同主名。单目录只放一套独立几何包；文件名需以小写 `.geo` 结尾，不使用 `matdb.geo` 或 Windows 设备名。

本平台 profile 使用 CSV：`Height/Thickness` 按 mm 输入。原生 CLI 还支持既有 XTF 子集，长度按 XTF 声明；需要使用时应由宿主显式更改输入合同。热直出可选 `--top-cell NAME`、`--matdb FILE`、`--stack-mode strict-slots|embedded-top`；CSV 要求 `strict-slots`。EM 和 both 使用默认源模型选项。直出几何**不接受 `--runtime-dir`**，内部工作使用系统临时目录，诊断由进程输出提供。

### 2.2 双目标几何

```powershell
& $geo --gds $gds --tech $tech --target both --em-output-dir "$result/em_geo" --thermal-output-dir "$result/thermal_geo"
if ($LASTEXITCODE -ne 0) { throw 'Both geometries failed' }
```

两侧输出目录均必填且须分开，不与单目标 `--output-dir` 混用。可分别指定 `--em-geo-name NAME.geo`、`--thermal-geo-name NAME.geo`；双目标不使用 `--geo-name`。源模型只建立一次，两个几何包完整核验后成组发布。

### 2.3 单目标网格

```powershell
& $mesh --runtime-dir $runtime --target em --input-dir "$result/em_geo" --output-dir "$result/em_msh"
if ($LASTEXITCODE -ne 0) { throw 'EM mesh failed' }
```

```powershell
& $mesh --runtime-dir $runtime --target thermal --input-dir "$result/thermal_geo" --output-dir "$result/thermal_msh"
if ($LASTEXITCODE -ne 0) { throw 'Thermal mesh failed' }
```

必填 `--target em|thermal --input-dir DIR --output-dir DIR`；平台应同时指定 `--runtime-dir`，将日志与工作暂存写入可写的 `Result/logs/gds2geo/`，避免使用 EXE 旁的旧默认目录。源模型 GDS/CSV 不参与这一步；GEO、同主名 JSON 和 MATDB 必须一起交接。目标身份、文件摘要、单位与材料不匹配时拒绝。

### 2.4 双目标网格

```powershell
& $mesh --runtime-dir $runtime --target both --em-input-dir "$result/em_geo" --thermal-input-dir "$result/thermal_geo" --em-output-dir "$result/em_msh" --thermal-output-dir "$result/thermal_msh"
if ($LASTEXITCODE -ne 0) { throw 'Both meshes failed' }
```

四个目录参数都必填，不与 `--input-dir/--output-dir` 混用。两套输入先同时核验，再执行网格化并成组发布。各输出目录与任一输入、另一输出不得重合或互为父子目录。

| 阶段目录 | 默认正式文件 |
| --- | --- |
| `Result/gds2geo/em_geo/` | `model_em.geo`、`model_em.json`、`matdb.json` |
| `Result/gds2geo/thermal_geo/` | `model_thermal.geo`、`model_thermal.json`、`matdb.json` |
| `Result/gds2geo/em_msh/` | `model_em.msh`、`matdb.json` |
| `Result/gds2geo/thermal_msh/` | `model_thermal.msh`、`matdb.json` |

每目标材料库只与本目标、本次成功输出配套；不存在强制 `.msh.json` 回执。正常退出 0 后才读取全部正式件。含旧回执或残缺正式件的输出目录应保留，另选空目录。失败时保留原包及恢复证据，不预删旧结果强行重试。

### 2.5 热边界/源选区导出

```powershell
& $mesh export-thermal-entities --mesh "$result/thermal_msh/model_thermal.msh" --out "$result/thermal_selection.json"
if ($LASTEXITCODE -ne 0) { throw 'Thermal selection export failed' }
```

必填 `--mesh FILE --out FILE`；可重复 `--source-entity ID` 选择材料体，`--source-surface-entity ID` 选择真实材料薄面。可选 `--matdb FILE`，默认读取网格同目录材料库；`--unit nm|um|mm|m` 是显式单位断言，有 dim 时必须一致，无声明的历史网格按既有 mm 默认或显式单位处理。此子命令不传网格阶段的 `--runtime-dir`。自动读取 dim；选区 JSON `mesh.dim` 保留比例，`planeZMm` 仍以 mm 表示。

## 3. GEOEDIT 编辑接口

```powershell
& $editor --input-dir "$result/em_geo" --output-dir "$result/em_geo_edited" --request "$dataRoot/Model/add_point.json"
if ($LASTEXITCODE -ne 0) { throw 'Geometry edit failed' }
& $mesh --runtime-dir $runtime --target em --input-dir "$result/em_geo_edited" --output-dir "$result/em_msh_edited"
if ($LASTEXITCODE -ne 0) { throw 'Edited geometry meshing failed' }
```

只有三个业务参数且均必填：`--input-dir DIR --output-dir DIR --request FILE`。输出目录同输入表示原位更新，不同表示另存。累计编辑时读取上次保存的输出三件。新增点、线、面的请求格式分别为：

```json
{"requestVersion":1,"operation":"add-point","point":{"x":10,"y":20,"z":0}}
```

```json
{"requestVersion":1,"operation":"add-line","start":{"x":0,"y":0,"z":0},"end":{"x":10,"y":0,"z":0}}
```

```json
{"requestVersion":1,"operation":"add-surface","vertices":[{"x":0,"y":0,"z":0},{"x":10,"y":0,"z":0},{"x":10,"y":20,"z":0},{"x":0,"y":20,"z":0}]}
```

这些是格式示例，坐标须换成实际目标位置；全部沿用输入 GEO 的尺度。请求上限 1 MiB，拒绝未知字段、重复键、非有限坐标和非法类型；不传额外 `unit`、`target`、实体 ID 或吸附参数。点自动判断已有点/边/面/体/自由位置；线端点必须不同，不吸附、不延伸；新增面必须是非退化、无自交、单外环无孔的平面多边形，可为倾斜面，末点自动连首点。

真正切断面/体时产生独立实体；否则按实际拓扑嵌入。跨界、交点、重叠及自由部分按几何关系保留。普通新增几何不自动赋材料；原材料面/体分割后的子实体继承归属，MATDB 原字节保持。实体编号可能变化，应从结果重新读取绑定。

输入仅限本模块完整配套 schema 2/3/4 模型，不是任意第三方 GEO/STEP 导入。输出仍为同名 GEO、同主名 JSON、MATDB 三件；编辑模型为 schema 4，含新增面时 `geometryEdits.version=3`。GEO2MSH 直接剖分实际保存的 GEO，配套 JSON/MATDB 仍不可省略。GEOEDIT 没有 `--runtime-dir` 参数，私有工作目录位于系统临时目录；保存过程在指定输出目录可能暂存 `.geoedit-transaction.json` 及备份，均不写安装目录。成功会清理，异常时保留并在下次相同输出目录调用时恢复，不要手动删除。数值关系无法可靠判定时会拒绝编辑并保留原包。

## 4. MSH 检测和修复

### 4.1 检测

```powershell
$checkDir = Join-Path $result 'mesh_check'
New-Item -ItemType Directory -Force -Path $checkDir | Out-Null
& $detector "$result/em_msh/model_em.msh" --output "$checkDir/detection.json" --compact
if ($LASTEXITCODE -ne 0) { throw 'Mesh detection failed' }
$check = Get-Content -LiteralPath "$checkDir/detection.json" -Raw | ConvertFrom-Json
if (-not $check.success) { throw 'Detection JSON is not successful' }
$check.summary | Select-Object issueCount, topologyIssueCount
```

必填位置参数 `<input.msh>`。全部可选参数：`-o/--output FILE`（默认 JSON 到 stdout）、`--annotated-msh FILE`（另生成 MSH/OPT 高亮副本）、`--abs-tol NUMBER`（默认 `1e-12`，有限正数）、`--rel-tol NUMBER`（默认 `1e-9`，有限非负数乘模型对角线）、`--compact`、`-h/--help`、`--version`。

输入支持 MSH 2.2/4.1 ASCII，检测对象是 Tri3 表面。**退出 0 只表示检测完成**，还须读取 `summary.issueCount` 和 `summary.topologyIssueCount`。成功检测 JSON schema 为 `1.5`；NodeTag/ElementTag 使用十进制字符串，避免经过浮点数丢失精度。高亮文件只是辅助显示副本，不是修复结果。

### 4.2 修复的独立调用

当前修复输入须是配对的 GEO、受支持 MSH 和检测报告；MSH 只能含 Point1/Line2/Tri3，不能含 Tet4 或 `$GDS2GEOUnits`。不要删除新网格的单位声明来绕过限制。使用随交付的受支持修复案例验证接入，不能把前节新输出直接接入以下流程。

```text
mshfix plan --geo FILE --source-msh FILE --detection FILE --output PLAN --detector EXE
mshfix heal --geo FILE --source-msh FILE --detection FILE --plan PLAN --output MSH --report REPORT --detector EXE
mshfix verify --source-msh FILE --healed-msh MSH --detection FILE --plan PLAN --healing-report REPORT --report VERIFICATION --detector EXE
mshfix capabilities --json
```

`FILE/PLAN/MSH/REPORT/VERIFICATION/EXE` 替换为绝对路径；`--detector` 显式指向本包 `mshcheck.exe`。输出使用 `Result/gds2geo/` 下每轮独立的可写目录，预先创建目录，不覆盖输入。

| 命令 | 必填业务参数 | 可选业务参数 |
| --- | --- | --- |
| `plan` | `--geo --source-msh --detection --output` | `--detector`、`--keep-temp` |
| `heal` | `--geo --source-msh --detection --plan --output --report` | `--detector`、`--keep-temp`、`--mesh-size-factor NUMBER`（默认 1，有限正数）、可重复 `--set-number NAME=VALUE` |
| `verify` | `--source-msh --healed-msh --detection --plan --healing-report --report` | `--detector` |
| `capabilities` | `--json` | 无 |

先检测，再生成计划。计划 schema `1.5`；若 `cadNormalization.status` 或任一 action 的 `repairability` 为 `unsupported`，停止并显示原因。原样传递计划，不删除问题项、篡改 hash 或自行指定策略绕过检查。若没有待连接 action、`cadNormalization.hasChanges=false` 且 `meshRegeneration.required=false`，报告无需修复并保留原网格；此时显式调用 heal 返回 `2 / no_connect_actions`。

修复成功输出 MSH **2.2 ASCII**，节点和单元编号各连续为 `1..count`；修复报告 schema `1.4`。随后独立 verify 必须退出 0，验证 JSON `success=true`、两个问题计数为 0 且验证门通过，宿主才登记修复结果有效。verify 失败不会回滚已经发布的 mesh/report。无操作分支保留原格式，不代表完成 MSH 2.2 转换。

`heal` 正常 stdout 是 schema `1.0` JSON，完整报告另存；verify 报告 schema `1.4`，capabilities schema `1.4`。统一错误处理可输出 schema `1.0` 的 `success:false` 及 `error.code/message/exitCode`，但这不是 QS stderr 协议。缺 DLL、启动失败或崩溃时不保证产生 JSON。保持输入、检测器及所有报告原样，搬动源文件或更换检测器后重新检测和计划。

## 5. dim、实体与下游接入

`dim` 是每个坐标单位对应的米数，有限且为正。**物理长度（米）= 坐标长度 × dim**。GDS `UNITS=(a,b)` 得 `dim=b/a`；GEO 头部为 `// dim = ...`，模型 JSON 为 `coordinates.dim`。例如 `dim=1e-6` 表示坐标按 μm 解释。新 MSH 4.1 的节点、实体和包围盒保持对应 GEO 尺度，写入：

```text
$GDS2GEOUnits
version = 1
dim = 1e-6
$EndGDS2GEOUnits
```

该段正常在 `$EndMeshFormat` 后，Gmsh 另存可能移到文件末尾；消费者需支持两种位置。版本号只属于单位扩展段，不是 MSH/schema 版本。拒绝重复、非法、未知版本及不一致的声明。Gmsh 不自动按 dim 缩放；使用 Gmsh 另存时以 `-save_all` 保留未分组低维单元。历史 schema 2 几何仍为 mm，新网格声明 `dim=0.001`；无声明的历史 MSH 只能按其明确合同解释，不从坐标大小猜测单位。

MSH 保留 dimensions 0/1/2/3、Point1/Line2/Tri3/Tet4、带方向直接边界及原生节点分类。允许未分组 CAD 实体和未被单元引用的原生节点；所有单元引用必须有效。**CAD Line2 不是激励线或端口**：求解器按维度和 Physical/MATDB 选取求解单元；端口由宿主的 `source_em.json` 明确定义并绑定具体网格，不能把所有一维单元自动当成激励，也不能仅因存在 CAD Line2 拒绝整个模型。不得通过删除线单元或补假材料代替正确的消费端适配。

编辑/重新剖分后，实体、节点、单元 ID 不保证稳定。`source_em.json` 的 meshFile、meshSha256、端口 triangleIds，以及热边界的 surface_ids，必须针对新网格重建。普通编辑面不天然具有唯一材料，也不自动成为热边界条件。

平台导入、显示、编辑和求解模块应读取 dim，并在自己的单位边界只换算一次。既有端口请求/绑定接口及 `planeZMm` 保持 mm 语义。MSH 的单位扩展不改变 MATDB 厚度/物性的各自单位。API 5 本身不保证阻断旧消费者，宿主应更新接受条件并验证消费逻辑后启用新单位合同。

## 6. 平台路由、验证与边界

manifest 的 `stages[]` 现在注册 **9 个阶段**，五个 EXE 均有正式 stage；原四阶段、六 profiles 和两个 nativeBothCalls 原样保留。新增的五阶段由宿主按需选择，不加入原六 profiles。`optional:true` 不能代替平台注册或输入门控。接收方须把选定 stage 接入平台路由与用户动作，在该分支校验输入并创建输出父目录，再按 `argsTemplate` 展开绝对路径调用。`capabilities.auxiliaryCommands` 保留兼容说明，`registeredStages` 指向正式阶段。

| 新阶段 | 固定输入（相对 dataRoot） | 输出（相对 `Result/gds2geo/`） |
| --- | --- | --- |
| `geoedit` | `Model/geoedit/model.geo`、`model.json`、`matdb.json` 和 `Model/geoedit_request.json` | `geoedit/model.geo`、`model.json`、`matdb.json` |
| `mshcheck` | `Model/mshfix/input.msh` | `mshcheck/detection.json` |
| `mshfix_plan` | `Model/mshfix/input.geo`、`input.msh` 和前述 detection | `mshfix_plan/plan.json` |
| `mshfix_heal` | 同一 GEO/MSH、detection、plan | `mshfix_heal/healed.msh`、`healing_report.json` |
| `mshfix_verify` | 同一源 MSH、detection、plan、healed MSH、healing report | `mshfix_verify/verification.json` |

`geoedit` 输入必须是原生有效的完整同名三件包；准备时用 `gds2geo --target em|thermal --geo-name model.geo` 生成，不要直接重命名受摘要/文件名约束的现有包。点、线、面由 request 的 operation 选择。`mshcheck` 也可单独选用；其固定目录名不意味着一定要修复。

修复依赖由 `consumesInternal` / `producesInternal` 声明：检测 → 计划 → 修复 → 验证。宿主必须在计划后检查 unsupported 和可执行动作；无改动计划保留原网格并跳过 heal。检测退出 0 只代表完成，仍要读取问题计数；heal 成功后仅在 verify 正常退出 0、success=true、两类问题计数为 0 且全部 validationGates 通过后接收结果。检测与修复全过程须使用同一源 MSH **绝对路径及字节**，报告同时绑定路径、哈希和检测器。

`capabilities.stageInvocation.requiredInputs` 列出每阶段所需文件，acceptance 列出验收条件；这是补充接入说明，不是宣称 task_layer 已支持新的门控字段。原全局 GDS/CSV consumed/dependencies 保持兼容，平台须按选中分支检查输入，不把所有辅助输入设成原流程的统一前置条件。产物 ref/path/schema 在标准 `artifacts.produced` 中声明，所有修复子命令均显式指定 `{moduleDir}/mshcheck.exe`。普通带 dim 网格仍不可直接进入原版 mshfix。

实际 task_layer 注册、路由与求解全链需要平台侧验收；本包提供 schema 和本地按模板调用验证，不把它等同于平台联调通过。

模块只写阶段结果，不写 `Model/`、`Setting/` 或平台 `Result/tasks/`。正式件发布沿用平台 `pipelines.json` 的 `publishes`；manifest 不添加 `publishTo`。发布时网格和 MATDB 必须属于同目标、同次成功生成。设置默认件、源/边界定义由平台和接收方维护，不由本模块凭空生成。

本次原字节随附既有独立案例包 `gds2geo_cases_20261112.zip` 与包外 `validation/` 验证材料；另按既有催收要求原字节附上冻结基线 `gds2geo_cases_20261107.zip`，该旧案例不代表 1.3.0 的单位协议。案例不放入模块安装目录。按新案例 README 运行，在可写 dataRoot 中复现几何、网格、编辑、检测及受支持修复路径，并检查对应预期工件/摘要。验证包含 manifest schema 校验、最终 ZIP 白名单/逐文件 SHA 核对、只读安装调用及依赖路径检查；具体完成项与结果以本次 validation 记录为准。

模块级成功不等于接收平台或求解器全链验收。r2 接入发现的 CAD Line2 消费问题按本 README 提供消费端适配合同；本包不新增 `--no-1d`，不宣称 compact_em/thermal 已适配新 dim 或完成全链联调。

所有五个程序支持 `--help`、`--version`。GDS2GEO 的无参/无 target 源包入口、GEO2MSH 的无参/无 target 旧双包入口、MSH 工具的无参便携 DATA 模式均保留在代码中，但不作为本平台交付接口；不要双击 EXE 代替显式调用。常规和辅助命令的退出码、失败恢复见 [ERROR_CODES.md](ERROR_CODES.md)。
