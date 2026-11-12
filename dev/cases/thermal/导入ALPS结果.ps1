# [弃用 2026-11-12 / v2.5.0_rc1] ALPS 结果导入已由平台承担，三通道等价：
#   1) task_layer RPC: alps.importOp0 {dataRoot, runDir}
#   2) CLI: task_layer.exe alps-import-op0 --data-root <root> --run-dir <bundle 子目录名>
#   3) 流水线: thermal_basic 的 alps 阶段（run bundle 子目录 diff + run_summary 门
#      + SHA256SUMS 唯一 .op0 + 三向新鲜度守卫 + import_receipt.json 回执）
# 本脚本写的 Model/thermal.op0 + thermal_run.json 为 thermal 1.0.6 时代旧契约；
# thermal 1.0.9 起 post 默认读 Result/thermal/alps/thermal.op0。v2.5.0_rc1 交付时移除本脚本。
param([Parameter(Mandatory=$true)][string]$Op0,[Parameter(Mandatory=$true)][string]$SolvedNetlist,[string]$DataRoot="$PSScriptRoot\demo1_4.1")
$ErrorActionPreference="Stop"
$DataRoot=(Resolve-Path -LiteralPath $DataRoot).Path
$binding=Get-Content -LiteralPath "$DataRoot\Result\thermal\extract\thermal_binding.json" -Raw | ConvertFrom-Json
$netHash=(Get-FileHash -LiteralPath $SolvedNetlist -Algorithm SHA256).Hash.ToLowerInvariant()
if ($netHash -ne $binding.netlistSha256 -and $netHash -ne $binding.rcNetlistSha256) {throw 'Solved netlist differs from this extraction. Do not bind an old OP0.'}
$dest=Join-Path $DataRoot 'Model\thermal.op0'
if ((Resolve-Path -LiteralPath $Op0).Path -ne $dest) {Copy-Item -LiteralPath $Op0 -Destination $dest}
@{schemaVersion=1;netlistSha256=$netHash;op0Sha256=(Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash.ToLowerInvariant()} | ConvertTo-Json | Set-Content -LiteralPath "$DataRoot\Model\thermal_run.json" -Encoding UTF8
& "$PSScriptRoot\..\thermal\TemperatureProcess.exe" --data-root $DataRoot
if ($LASTEXITCODE) {throw "Post failed: $LASTEXITCODE"}
