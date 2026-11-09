# GDS2GEO V1.2 · 几何与网格接口

本包面向 Windows x64。输入二维 GDS 版图和 CSV/XTF 工艺叠层，`gds2geo.exe` 生成电磁或电热三维几何；`geo2msh.exe` 读取相应的几何包，生成网格。两个程序均支持 `em`（电磁）、`thermal`（电热）和 `both`（两者）。只需要查看几何时，完成第一步即可。

## 本次交付的变化

| 接收方原先的调用 | 本包的调用方式 |
| --- | --- |
| 依次运行源模型、几何准备，再分别启动两个网格程序 | 从原始 GDS 和工艺文件直接调用 `gds2geo --target …`。需要网格时再调用 `geo2msh --target …`。只使用这两个 EXE。 |
| 共享准备目录和材料库 | EM 和电热各用一个几何目录、一个网格目录。每份目标 GEO 带同主名 JSON 和本目标 MATDB；每份 MSH 带本目标 MATDB。 |
| 用网格回执判断独立目标完成 | 等进程正常退出且退出码为 0，再取本次目标的 MSH 和 MATDB。双目标的几何阶段一次建立源模型，网格阶段核验两套输入后成组发布四件结果。 |
| 从模块包取网格工具 | 平台提供 Gmsh **4.11.1**，通过 `GDS2GEO_GMSH_EXECUTABLE` 指定绝对路径。生成 GEO 可独立运行。 |

## 本批次网格与下游读取要求

本批次标识为 **GDS2GEO-V1.2-20261107-r2**；`r2` 只区分同日交付，产品仍为 **V1.2**，两个 EXE 的 `--version` 仍为 **1.2.0**。请成对部署并按 manifest 核对 SHA-256。

生成的 MSH 保留对应目标 GEO 经 Gmsh 4.11.1 加载后的全部真实 Point、Curve、Surface、Volume，实体身份为 `(dimension, tag)`；同时保留直接边界及适用方向、点坐标/实体范围和节点原生归属。单元包括 Point1（类型 15）、Line2（类型 1）、Tri3（类型 2）、Tet4（类型 4），以实际目标中存在的维度为准。坐标为 mm，即 GEO 坐标乘 `coordinates.dim * 1000`。

接收方读取器须支持以上四维实体及单元，允许无 Physical 分组的 CAD 实体以及未被单元引用的原生节点；所有单元引用的节点仍必须存在。CAD 点线不是端口或激励，不能自动作为热边界条件。求解单元按维度及 Physical/MATDB 语义选取，不为无分组实体补 Physical 0 或材料。材料 Physical 分组与配套 MATDB 的含义保持不变。实体身份仅对应本目标几何，不保证不同目标、几何修改或重新剖分后的节点/单元编号相同。

本批次按 doc1.5 不兼容工件规则将 manifest `apiVersion` 从 3 升至 **4**；GEO/MATDB schema 仍为 3，MSH 格式仍为 4.1 ASCII。接口版本检查本身不会自动阻止所有旧消费者，接收方应先验证读取兼容性，再启用本包。六组 CLI 参数与工件路径保持不变。

## 坐标单位与 `dim`

**本次 V1.2 交付使用带 `dim` 的配套双 EXE。** 几何阶段从每个 GDS 的 `UNITS` 读取每个 DBU 对应的用户单位数 `a` 和米数 `b`，用 `dim = b/a` 算出一个 GEO 坐标单位对应多少米。正式 GEO 的 XY、Z、厚度以及几何容差统一按该版图的用户单位写出。GEO 头部的 `// dim = …` 可直接读取，程序同时将数值写入同主名 JSON 的 `coordinates.dim`。这个注释记录单位，修改注释本身不会缩放几何。

| 读取对象 | 长度约定 |
| --- | --- |
| GDS 与工艺输入 | 版图按 GDS 自身 DBU；CSV 的 Height、Thickness 以 mm 输入，XTF 按 `Config.LengthUnit`。默认 XY 轮廓近似容差为 50 nm。 |
| 目标 GEO 与配套 JSON | 正式 GEO 的长度乘以 `dim` 得米。目标 JSON 为 schema 3，`coordinates.outputUnit` 描述 GEO 单位，`coordinates.modelUnit` 描述内部模型的 mm 单位。读取 `model.z` 等模型字段时使用后者。 |
| 网格与材料 | MSH 4.1 ASCII 的节点坐标为 **mm**，换算成米乘以 `1e-3`。MATDB schema 3 的材料厚度、宽度和物性按各自字段声明的单位读取。 |

例如 `UNITS = (0.001, 1e-9)` 时，GEO 的 `dim = 1e-6`。10 µm 的边长在 GEO 中是 `10`，CSV 的 0.002 mm 层厚在 GEO 中是 `2`；同一段 10 µm 距离在最终 MSH 中是 `0.01` mm。

**部署时成对替换两个程序。** 早期同样显示 `1.2.0` 的网格程序仅支持旧版目标几何包，读取本包新生成的 schema 3 几何会退出 2。安装前可用 `Get-FileHash -Algorithm SHA256` 将两个 EXE 分别与 `module.manifest.json` 中登记的摘要核对。

## 安装与自测准备

打开交付文件夹 `GDS2GEO-V1.2-20261107-r2/`，其中的 `gds2geo/` 模块目录包含 `gds2geo.exe`、`geo2msh.exe`、本 README、`ERROR_CODES.md` 和 `module.manifest.json`。将该目录放在平台部署根目录下，将 Gmsh 放在同一部署根的 `tools/gmsh/gmsh.exe`。程序只需读取安装目录。准备一个可写的算例目录，将版图和叠层放在 `Model/model.gds` 和 `Model/model_info.csv`。使用 XTF 时，把下方 `$tech` 指向对应的 `.xtf`。

以下 PowerShell 命令在**算例目录**执行。先替换部署位置，再任选第 1 节中的一个调用；如需网格，接着执行第 2 节的对应调用。

```powershell
$moduleRoot = 'C:\PEEC\modules'  # 改为平台的实际部署根目录
$geo = Join-Path $moduleRoot 'gds2geo\gds2geo.exe'
$mesh = Join-Path $moduleRoot 'gds2geo\geo2msh.exe'
$env:GDS2GEO_GMSH_EXECUTABLE = Join-Path $moduleRoot 'tools\gmsh\gmsh.exe'
$gds = '.\Model\model.gds'
$tech = '.\Model\model_info.csv'
$result = '.\Result\gds2geo'
$logs = '.\Result\logs\gds2geo'

& $geo --version
& $mesh --version
& $env:GDS2GEO_GMSH_EXECUTABLE -version 2>&1 | Out-String
```

版本检查应分别返回 `gds2geo 1.2.0`、`geo2msh 1.2.0` 和 `4.11.1`。网格程序依次查找 EXE 同目录的历史 Gmsh、环境变量指定的文件和 PATH。按上述五文件布局安装时使用平台设置的路径。若模块目录残留其他版本的 `gmsh.exe`，会优先于环境变量；应使用本包的干净五文件目录。4.15.2 等其他版本不能替代 4.11.1。

两份 x64 程序静态集成编译器运行库。构建依赖包括内部 MATDB 0.1.0、Clipper2 2.0.1、CLI11 2.6.2 和 nlohmann/json 3.12.0。平台另行部署的运行工具为 Gmsh 4.11.1。

## 物理模型解析

几何阶段和网格阶段分别列出电磁、电热、双目标三种调用。下列示例沿用上节的 PowerShell 变量。目录树均从算例根目录起算，对应 manifest 的 `Result/gds2geo/` 布局。

### 1 GDS2GEO

#### 1.1 输出电磁 GEO

##### 1.1.1 CLI 传参

```powershell
& $geo `
  --gds $gds `
  --tech $tech `
  --target em `
  --output-dir "$result/em_geo" `
  --geo-name model_em.geo
```

| 参数 | 含义 |
| --- | --- |
| `--gds $gds` | 输入的 GDS 版图文件，示例为 `Model/model.gds`。 |
| `--tech $tech` | 输入的工艺叠层文件，提供层映射、高度、厚度和材料信息。示例使用 `Model/model_info.csv`。 |
| `--target em` | 生成电磁几何模型。 |
| `--output-dir "$result/em_geo"` | 将电磁几何包直接写入该目录。 |
| `--geo-name model_em.geo` | 可选。指定 GEO 文件名；省略时默认使用 `model_em.geo`。 |

##### 1.1.2 输出文件

```text
Result/gds2geo/em_geo/
├── model_em.geo
├── model_em.json
└── matdb.json
```

- **`model_em.geo`** 描述电磁模型的形状、尺寸、位置和材料物理分组，可用 Gmsh 打开查看。头部的 `dim` 给出每个 GEO 坐标单位对应的米数。
- **`model_em.json`** 是同主名的电磁模型描述文件，记录几何区域与材料编号的对应关系、坐标单位以及 GEO 和材料库的文件摘要。
- **`matdb.json`** 是本电磁模型的材料库，记录材料编号、名称和物理参数，例如电导率、相对介电常数。

这三个文件放在同一目录。继续生成电磁网格时，将整个目录传给第 2.1 节的 `geo2msh.exe`。

#### 1.2 输出电热 GEO

##### 1.2.1 CLI 传参

```powershell
& $geo `
  --gds $gds `
  --tech $tech `
  --target thermal `
  --output-dir "$result/thermal_geo" `
  --geo-name model_thermal.geo
```

| 参数 | 含义 |
| --- | --- |
| `--gds $gds` | 输入的 GDS 版图文件。 |
| `--tech $tech` | 输入的工艺叠层文件，用于确定版图层对应的材料、层高和厚度。 |
| `--target thermal` | 生成电热几何模型。 |
| `--output-dir "$result/thermal_geo"` | 将电热几何包直接写入该目录。 |
| `--geo-name model_thermal.geo` | 可选。指定 GEO 文件名；省略时默认使用 `model_thermal.geo`。 |

电热直出还支持 `--top-cell NAME` 选择顶层单元、`--matdb FILE` 指定原生材料库种子、`--stack-mode strict-slots|embedded-top` 选择叠层方式。CSV 使用 `strict-slots`。默认要求 GDS 有唯一顶层单元。电磁与双目标直出使用默认源模型选项。

##### 1.2.2 输出文件

```text
Result/gds2geo/thermal_geo/
├── model_thermal.geo
├── model_thermal.json
└── matdb.json
```

- **`model_thermal.geo`** 描述电热模型的形状、尺寸和位置，可用 Gmsh 打开查看。文件头同样记录本版图的 `dim`。
- **`model_thermal.json`** 是与 GEO 同主名的电热模型描述文件，记录几何区域与材料编号的对应关系、坐标单位及配套文件摘要。
- **`matdb.json`** 是本电热模型的材料库，保存材料编号、名称和输入中已有的热导率、密度、比热容等热物性。程序报告材料缺项时，可按后续求解所需补齐输入属性。

这三个文件放在同一目录。继续生成电热网格时，将整个目录传给第 2.2 节的 `geo2msh.exe`。

#### 1.3 同时输出电磁和电热 GEO

##### 1.3.1 CLI 传参

```powershell
& $geo `
  --gds $gds `
  --tech $tech `
  --target both `
  --em-output-dir "$result/em_geo" `
  --thermal-output-dir "$result/thermal_geo" `
  --em-geo-name model_em.geo `
  --thermal-geo-name model_thermal.geo
```

| 参数 | 含义 |
| --- | --- |
| `--gds $gds` | 两个目标共用的输入 GDS。 |
| `--tech $tech` | 两个目标共用的工艺叠层，提供层映射、厚度和材料信息。 |
| `--target both` | 一次生成一份源模型，再从同一份模型生成电磁和电热几何。此接口已通过 V1.2 双目标实测。 |
| `--em-output-dir "$result/em_geo"` | 必填。电磁几何三件的最终目录。 |
| `--thermal-output-dir "$result/thermal_geo"` | 必填。电热几何三件的最终目录，须与电磁目录分开。 |
| `--em-geo-name model_em.geo` | 可选。指定电磁 GEO 名，默认值如示例所示。 |
| `--thermal-geo-name model_thermal.geo` | 可选。指定电热 GEO 名，两侧可独立命名。 |

##### 1.3.2 输出文件

```text
Result/gds2geo/
├── em_geo/
│   ├── model_em.geo
│   ├── model_em.json
│   └── matdb.json
└── thermal_geo/
    ├── model_thermal.geo
    ├── model_thermal.json
    └── matdb.json
```

`em_geo/` 保存第 1.1.2 节列出的电磁三件，`thermal_geo/` 保存第 1.2.2 节列出的电热三件。两份 `matdb.json` 各自与所在目录的模型配套。进程返回 0 后，两套几何即可独立使用。

三个几何入口都支持用以小写 `.geo` 结尾的普通文件名。自定义 GEO 名时，配套 JSON 自动采用相同主名；搬迁时按原名一起复制三件。直出过程使用系统临时目录，诊断写到 stdout/stderr。桌面程序调用本节的命令时请省略 `--runtime-dir`。

### 2 GEO2MSH

#### 2.1 输出电磁 MSH

##### 2.1.1 CLI 传参

```powershell
& $mesh `
  --runtime-dir $logs `
  --target em `
  --input-dir "$result/em_geo" `
  --output-dir "$result/em_msh"
```

| 参数 | 含义 |
| --- | --- |
| `--runtime-dir $logs` | 将网格阶段日志和临时文件放在可写的 `Result/logs/gds2geo/`。 |
| `--target em` | 生成电磁网格。 |
| `--input-dir "$result/em_geo"` | 指向完整的电磁几何包。程序读取 GEO、同主名 JSON 及本目标的 `matdb.json`。原始 GDS 和工艺文件可留在建模端。 |
| `--output-dir "$result/em_msh"` | 电磁网格输出目录，成功后包含 MSH 和配套材料库。 |

如果把电热几何包传给 `--target em`，程序在网格划分前返回退出码 `2`（输入或参数错误），保留已有网格。

##### 2.1.2 输出文件

```text
Result/gds2geo/em_msh/
├── model_em.msh
└── matdb.json
```

- **`model_em.msh`** 记录电磁网格节点、网格单元和物理分组，可用 Gmsh 打开查看。本版输出的节点坐标以 mm 表示。
- **`matdb.json`** 是随电磁网格发布的材料库，给出网格物理分组对应的材料编号、名称和物理参数。

#### 2.2 输出电热 MSH

##### 2.2.1 CLI 传参

```powershell
& $mesh `
  --runtime-dir $logs `
  --target thermal `
  --input-dir "$result/thermal_geo" `
  --output-dir "$result/thermal_msh"
```

| 参数 | 含义 |
| --- | --- |
| `--runtime-dir $logs` | 指定本次网格调用的日志和暂存目录。 |
| `--target thermal` | 生成电热网格。 |
| `--input-dir "$result/thermal_geo"` | 指向完整的电热几何包。程序读取 GEO、同主名 JSON 和该目标的 `matdb.json`。 |
| `--output-dir "$result/thermal_msh"` | 电热网格输出目录，成功后包含 MSH 和配套材料库。 |

如果把电磁几何包传给 `--target thermal`，程序在网格划分前返回退出码 `2`，保留已有网格。输入缺件、文件摘要与几何包记录不一致，也会在划分前被拒绝。

##### 2.2.2 输出文件

```text
Result/gds2geo/thermal_msh/
├── model_thermal.msh
└── matdb.json
```

- **`model_thermal.msh`** 记录电热网格节点、网格单元和物理分组，可用 Gmsh 打开查看。节点坐标同样以 mm 表示。
- **`matdb.json`** 是随电热网格发布的材料库，包含材料编号、名称以及输入中已有的热导率、密度和比热容等参数。

#### 2.3 同时输出电磁和电热 MSH

##### 2.3.1 CLI 传参

```powershell
& $mesh `
  --runtime-dir $logs `
  --target both `
  --em-input-dir "$result/em_geo" `
  --thermal-input-dir "$result/thermal_geo" `
  --em-output-dir "$result/em_msh" `
  --thermal-output-dir "$result/thermal_msh"
```

| 参数 | 含义 |
| --- | --- |
| `--runtime-dir $logs` | 指定本次双目标网格调用共用的日志和暂存目录。 |
| `--target both` | 在一次调用中分别生成电磁网格和电热网格，两个输入包均通过校验后才开始划分。 |
| `--em-input-dir "$result/em_geo"` | 输入电磁几何包目录。 |
| `--thermal-input-dir "$result/thermal_geo"` | 输入电热几何包目录。 |
| `--em-output-dir "$result/em_msh"` | 输出电磁网格和配套材料库。 |
| `--thermal-output-dir "$result/thermal_msh"` | 输出电热网格和配套材料库。 |

四个专用目录参数均为必填。两份输入和两份输出分别使用独立目录，避开重合和父子路径。目标错配会在划分前返回退出码 `2`，保留两侧已有结果。

##### 2.3.2 输出文件

```text
Result/gds2geo/
├── em_msh/
│   ├── model_em.msh
│   └── matdb.json
└── thermal_msh/
    ├── model_thermal.msh
    └── matdb.json
```

`em_msh/` 保存第 2.1.2 节列出的电磁网格及其材料库，`thermal_msh/` 保存第 2.2.2 节列出的电热网格及其材料库。等待进程正常退出且返回 0，再取两个目录中的四件结果。重跑可使用本目标已有的完整结果目录；含旧网格回执或残缺文件时，请指定新目录。

## 随附案例

案例单独交付为 `gds2geo_cases_20261107.zip`，不放入五文件模块目录。解压后阅读案例 README，使用自带 `Model/` 输入，按本 README 的 1.3 → 2.3 重现。案例带有本批次的几何、网格和配套材料基准结果；外置 Gmsh 4.11.1 由平台提供。验收范围是本模块的几何与网格，不代表 EM/热求解器或平台全链联调完成。

## 本地验收与 Qt 集成

1. 在新的算例目录运行 **1.3 → 2.3**。用 `$LASTEXITCODE` 检查每个外部进程，任一步非零就停。两份 GEO 可分别用 Gmsh 打开。两个几何目录各有三件；两个网格目录各有两件。
2. 核对同一目标几何目录与网格目录的 `matdb.json` 的 SHA-256。可用 PowerShell 的 `Get-FileHash`。下游读取 MSH 时同时传入该目标的材料文件。
3. Qt 桌面程序用 `QProcess` 设置 EXE 绝对路径、算例工作目录和网格进程的 Gmsh 环境变量。通过参数列表逐项传值，路径中含空格时仍作为单个参数。检查 `NormalExit` 和 `exitCode() == 0`，保留 stdout/stderr。网格调用的 `--runtime-dir` 指向可写日志目录。
4. `module.manifest.json` 使用平台清单格式 `manifestVersion: 1`，产品为 `V1.2`，本次完整实体交付合同为 `apiVersion: 4`。单目标调用见 `stages`，原生双目标调用见 `capabilities.nativeBothCalls`。按 `capabilities.executionProfiles` 选择目标与终点（GEO 或 MSH），依次执行该组合列出的调用。模板使用 CSV 和默认文件名。对接旧四阶段调度时，需按本包入口和工件映射更新平台路由；平台若发布到 `Model/`，将目标 MSH 与其 MATDB 配对发布。
5. 常用退出码为 `0`（成功）、`2`（参数或输入）、`5`（生成、校验或发布失败）、`6`（未分类异常）。按程序和失败阶段的具体处置见 `ERROR_CODES.md`。运行 `gds2geo.exe --help`、`geo2msh.exe --help` 可查看原生程序的完整帮助。省略目标的历史兼容调用与本包的显式目标流程分别处理。

需要从热网格导出边界或热源选区时，使用独立子命令 `export-thermal-entities`，调用时省略网格阶段的 `--runtime-dir`。下列命令导出上下边界并列出可选实体。用可重复的 `--source-entity ID` 选体，`--source-surface-entity ID` 选材料薄面；`--matdb FILE` 和 `--unit nm|um|mm|m` 可显式指定材料库和网格单位。本版网格采用默认的 mm。

```powershell
& $mesh export-thermal-entities --mesh "$result/thermal_msh/model_thermal.msh" --out "$result/thermal_selection.json"
```

## 参数与兼容入口说明

以上 1.1–2.3 列出本包正式目标流程的全部业务参数；`-h`/`--help`、`--version` 用于查询。热直出的 `--top-cell`、`--matdb`、`--stack-mode` 见 1.2。热选区子命令的 `--mesh`、`--out`、`--source-entity`、`--source-surface-entity`、`--matdb`、`--unit` 见上节，其 `--help` 可独立查询。

帮助文本还列出历史兼容入口：无参数启动使用 EXE 旁设置及 input/output 默认目录；`gds2geo` 省略 `--target` 只生成源包，并允许 `--runtime-dir`；`geo2msh` 省略 `--target` 接受旧双网格 bundle。这些已在 manifest 的 `cli.deprecatedModes` 登记，不属于本包平台调用模板。平台应始终显式传目标和数据路径，几何直出不传 `--runtime-dir`。几何直出的临时工作区使用系统 TEMP/TMP，可由宿主为子进程指定可写位置，stdout/stderr 由宿主保存。
