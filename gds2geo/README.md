# GDS2GEO

交付版本：**V1**。

本包用于 task_layer 集成，只交付以下六个文件：

```text
gds2geo/
  gds2geo.exe
  geo_prepare.exe
  geo2em.exe
  geo2thermal.exe
  README.md
  module.manifest.json
```

目录名为小写 `gds2geo`，与 manifest.id 相同。四程序采用 Windows x64 Release 静态运行库构建，
无须另配 MinGW 运行库 DLL，仍依赖 Windows 系统 DLL。安装目录可以只读，
输入、结果及运行记录全部使用外部算例 dataRoot。本模块生成几何与网格，不执行物理场求解。

## 平台运行依赖

Gmsh **4.11.1** 由平台另外安装，不在本六文件包内。只有 EM 和 thermal 网格阶段调用它。
平台应给子进程配置 `GDS2GEO_GMSH_EXECUTABLE` 环境变量，值为实际 gmsh.exe 的绝对路径；
也可把该版本所在目录加入子进程 `PATH`。版本不匹配明确失败，不自动改用其他版本。
四主程序本身不依赖 PowerShell；下方 PowerShell 代码仅供平台参考。

内部版本：GDS2GEO/MATDB 0.1.0、CLI11 2.6.2、nlohmann/json 3.12.0、Clipper2 2.0.1、
tinyxml2 11.0.0。交付批次由 manifest.version 记录，与源码库版本分开。

## 输入、阶段与输出

本交付接口仅支持 GDS + CSV。UI 把固定输入写入 `<dataRoot>/Model/model.gds` 和
`<dataRoot>/Model/model_info.csv`。不以 XTF 替代该 CSV，不依赖安装目录中的输入或设置。
GDS 使用自身 DBU；CSV 的 Height/Thickness 为 mm，全部正式 GEO/MSH 坐标统一为 mm。
多个顶层单元时，平台为 source 增加 `--top-cell NAME`。

| 必需 stage ID | 程序 | 输出目录（相对 dataRoot） | 必需文件 |
| --- | --- | --- | --- |
| source | gds2geo.exe source | Result/gds2geo/source | model.geo、model.json、matdb.json |
| prepared | geo_prepare.exe | Result/gds2geo/prepared | em.geo、em.json、thermal.geo、thermal.json、matdb.json |
| em | geo2em.exe | Result/gds2geo/em | model_em.msh、model_em.msh.json、matdb.json |
| thermal | geo2thermal.exe | Result/gds2geo/thermal | model_thermal.msh、model_thermal.msh.json、matdb.json |

平台“一键建模”必须**串行执行 source → prepared → em → thermal**，任一步失败立即停止。
四步全部成功、两份网格与当前 source/prepared 同代、产物完整且所需发布动作成功后，
才可把整项任务标记为成功。同一算例运行期间，平台应避免 UI 改写输入或启动另一项建模。
两个网格阶段均为必需，不得只完成一种网格便报告成功。

所有配套 JSON 都是算法合同，必须保留，不能改名、删除或跨批次拼接。
当前准备策略为 `four-executable-7`，网格为 **MSH 4.1 ASCII**（文件头 `4.1 0 8`），
MATDB 为 schema 3，`*.msh.json` 为 schema 6。receipt 绑定输入 GEO/JSON/MATDB 与输出 MSH 的 SHA-256。
MSH 保留实际面/体 Elementary 实体、有向体面边界、未分 Physical 的真实面；不建立 Physical 0。

manifest 中两个网格工件沿用平台示例的 `schemaVersion: "4"`；本包约定它表示 MSH 主格式版本，
不表示 receipt 版本。实际 MSH 4.1、MATDB 3、receipt 6 在 capabilities 另行明确；
该约定及下游对 4.1 的读取能力仍须平台实际联调确认。

## 可复制的串行调用参考

替换开头三个绝对路径后，可在 PowerShell 5.1 或 7 中执行。此代码写在说明中供平台参考，
平台也可直接以进程 API 顺序启动四个 EXE，并检查真实退出码。

<!-- BEGIN GDS2GEO_ONE_CLICK -->
```powershell
$ModuleDir = 'C:/Modules/gds2geo'
$DataRoot = 'D:/Cases/demo'
$GmshExe = 'C:/Tools/Gmsh-4.11.1/gmsh.exe'
$env:GDS2GEO_GMSH_EXECUTABLE = $GmshExe
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
foreach ($name in @('Microsoft.PowerShell.Utility', 'Microsoft.PowerShell.Management')) {
    Import-Module ([IO.Path]::Combine($PSHOME, 'Modules', $name, "$name.psd1")) -ErrorAction Stop
}
$runtime = Join-Path $DataRoot 'Result/logs/gds2geo'
$result = Join-Path $DataRoot 'Result/gds2geo'
$source = Join-Path $result 'source'
$prepared = Join-Path $result 'prepared'

& "$ModuleDir/gds2geo.exe" --runtime-dir $runtime source --gds "$DataRoot/Model/model.gds" --tech "$DataRoot/Model/model_info.csv" --output-dir $source
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& "$ModuleDir/geo_prepare.exe" --runtime-dir $runtime --input-dir $source --output-dir $prepared
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& "$ModuleDir/geo2em.exe" --runtime-dir $runtime --input-dir $prepared --output-dir "$result/em"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& "$ModuleDir/geo2thermal.exe" --runtime-dir $runtime --input-dir $prepared --output-dir "$result/thermal"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

foreach ($relative in @(
    'source/model.geo', 'source/model.json', 'source/matdb.json',
    'prepared/em.geo', 'prepared/em.json', 'prepared/thermal.geo', 'prepared/thermal.json', 'prepared/matdb.json',
    'em/model_em.msh', 'em/model_em.msh.json', 'em/matdb.json',
    'thermal/model_thermal.msh', 'thermal/model_thermal.msh.json', 'thermal/matdb.json'
)) {
    $file = Join-Path $result $relative
    if (-not (Test-Path -LiteralPath $file -PathType Leaf) -or (Get-Item -LiteralPath $file).Length -eq 0) {
        throw "Required output is missing or empty: $relative"
    }
}
$s = Get-Content -LiteralPath "$source/model.json" -Raw -Encoding UTF8 | ConvertFrom-Json
$p = Get-Content -LiteralPath "$prepared/em.json" -Raw -Encoding UTF8 | ConvertFrom-Json
$t = Get-Content -LiteralPath "$prepared/thermal.json" -Raw -Encoding UTF8 | ConvertFrom-Json
$em = Get-Content -LiteralPath "$result/em/model_em.msh.json" -Raw -Encoding UTF8 | ConvertFrom-Json
$thermal = Get-Content -LiteralPath "$result/thermal/model_thermal.msh.json" -Raw -Encoding UTF8 | ConvertFrom-Json
if ($s.sourceFingerprint -notmatch '^[0-9a-f]{64}$' -or $p.preparationFingerprint -notmatch '^[0-9a-f]{64}$') {
    throw 'Invalid source or preparation fingerprint.'
}
foreach ($document in @($p, $t, $em, $thermal)) {
    if ($document.sourceFingerprint -cne $s.sourceFingerprint -or
        $document.preparationFingerprint -cne $p.preparationFingerprint) {
        throw 'Source, prepared, EM and thermal outputs are from different generations.'
    }
}
Write-Host 'source, prepared, em and thermal all completed.'
```
<!-- END GDS2GEO_ONE_CLICK -->

平台须并发异步读取 stdout/stderr，避免管道写满阻塞；等待每个进程退出并保存完整诊断。
上述参考检查产物完整性及同代关系，程序内部仍执行几何、材料、网格及摘要深检。

## 主流程参数

| 程序/参数 | 说明 |
| --- | --- |
| 全部：`-h, --help` / `--version` | 显示帮助/源码版本，返回 0，不创建运行数据 |
| 全部：`--runtime-dir PATH` | 平台必传绝对路径 `<dataRoot>/Result/logs/gds2geo`；日志、诊断及私有暂存 |
| gds2geo：`source` | 源模型主流程子命令 |
| source：`--gds PATH` / `--tech PATH` / `--output-dir PATH` | 必填绝对路径；固定 GDS、CSV 输入及 source 输出目录 |
| source：`--top-cell NAME` | 可选；省略时要求 GDS 顶层唯一 |
| source：`--matdb PATH` | 可选完整原生材料 seed；源 Volume ID/type 必须匹配，常规平台链不传 |
| source：`--stack-mode strict-slots` | CSV 只允许默认 strict-slots，无须传入 |
| geo_prepare：`--input-dir PATH` / `--output-dir PATH` | 平台必传绝对 source/prepared 目录 |
| geo2em/geo2thermal：`--input-dir PATH` / `--output-dir PATH` | 平台必传绝对 prepared 和各自 em/thermal 目录 |

无参双击、EXE 相对 input/output 回退及路径设置历史模式已在 manifest 声明 deprecated，
不属于本交付接口。保留的开发/历史兼容子命令不登记为本包 stage。
CSV 可提供 ThermalConductivity、Density、SpecificHeat，单位为 W/(m*K)、kg/m^3、J/(kg*K)；
程序不会凭材料名字猜测缺失物性。

## 退出码与恢复

四阶段使用文本 stdout/stderr，`errorContract.type` 为 `none`。task_layer 把非零退出归为
`QS-TL-MOD-EXEC`，同时保留实际 exitCode；本模块没有自行定义额外 QS 细分码。

| 退出码 | 范围及触发条件 | 可恢复性与处置 |
| --- | --- | --- |
| 0 | 四阶段成功；帮助/版本正常返回也为 0 | 平台须核查产物；四阶段全部成功后才能完成任务 |
| 2 | 四阶段：参数/路径错误、运行目录不可用、输入缺失或初检拒绝、源/目标包加载失败 | 修正固定输入、CSV、绝对路径或权限；必要时从上游重建 |
| 5 | 四阶段：几何/材料/接触政策拒绝、互斥量忙或 abandoned、Gmsh 缺失/错版/失败/超时、网格验收、写入/事务发布失败 | 阅读原始错误；等待占用者退出，或修正输入、资源、权限及外置 Gmsh 后重跑 |
| 6 | source：CLI 顶层捕获的未分类标准异常/内部错误 | 保留输入、版本和日志交开发排查，不反复强行重试 |

`5` 不是专用的磁盘错误或超时码。Windows 入口前启动失败、操作系统异常终止等状态不属于业务退出码。
每阶段分别事务发布，失败保留该阶段的上一组有效文件；后续失败不会回滚已经成功的上游。
旧下游文件存在不代表本轮成功，应修正原因并重跑完整四阶段。

日志位于 Result/logs/gds2geo 下，四程序各写自身 `.log`，覆盖其上次调用；
源导入诊断在同目录 source-attempts/，平台应另外保留每任务 stdout/stderr。
Windows 程序级及数据共享互斥使用系统命名互斥量，不创建 `.lock` 文件；
保证范围为同一主机，不承诺跨主机共享目录协调，不得与旧版文件锁程序混用同一算例。

## 平台发布与验收

模块只写自己的 Result/gds2geo 阶段目录和运行目录，不读写 Result/tasks，不向 Model/Setting 复制正式件。
task_layer publisher 按 publishTo 把 EM MSH 和配套 MATDB 发布到 Model/model_em.msh、Model/matdb.json；
Model 中只保留一份 EM 主网格，热网格留在 thermal 目录。publisher 失败属于平台 `QS-TL-IO-ERROR`。
端口、求解配置与 UI 行为由平台负责。本六文件包不附带 task_layer 或平台自测工具。

平台验收应包含：四阶段串行完成、两份同代 MSH 4.1/MATDB3/receipt6 完整、
只读安装目录运行、无 `.lock` 文件、任一步失败整项任务失败并保留有效旧阶段产物，
以及实际 publisher 和下游求解器联调。平台注册和完整联调需以平台方实测结果为准。
