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
