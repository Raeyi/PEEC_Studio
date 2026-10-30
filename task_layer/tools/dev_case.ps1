# dev_case.ps1 - local self-test helper for module teams (ONBOARDING §五)
# Generates a clean dataRoot skeleton, seeds inputs from the module package's
# examples/, and runs the given stage through task_layer.
#
# Usage:
#   .\dev_case.ps1 -Module gds2geo -Stage mesh_em [-CaseDir D:\dev\mycase] [-Clean]
#
# NOTE: keep this file ASCII-only (PowerShell 5.1 code-page issue).

param(
    [Parameter(Mandatory = $true)][string]$Module,     # provider id, e.g. gds2geo
    [Parameter(Mandatory = $true)][string]$Stage,      # stage id, e.g. mesh_em
    [string]$CaseDir,                                   # default: %TEMP%\peec_dev\<module>
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$root  = Split-Path -Parent $PSScriptRoot              # task_layer
$repo  = Split-Path -Parent $root                      # PEEC_Studio
$exe   = Join-Path $root "build\task_layer.exe"
if (-not (Test-Path $exe)) { throw "task_layer not built: run build_local first" }

if ($CaseDir) {
    $dataRoot = (Resolve-Path $CaseDir).Path
} else {
    $dataRoot = Join-Path $env:TEMP "peec_dev\$Module"
}
$pkg = Join-Path $repo $Module

# 1) dataRoot skeleton
foreach ($d in @("Model", "Setting", "Result", ".locks")) {
    New-Item -ItemType Directory -Force -Path (Join-Path $dataRoot $d) | Out-Null
}

# 2) seed inputs from package examples/ (read-only samples)
$examples = Join-Path $pkg "examples"
if (Test-Path $examples) {
    Get-ChildItem $examples -File | ForEach-Object {
        Copy-Item $_.FullName (Join-Path $dataRoot "Model") -Force
        Write-Host "seeded Model/$($_.Name)"
    }
} else {
    Write-Warning "no examples/ under $pkg - place inputs into Model\ manually"
}

# 3) run the single-stage pipeline via task_layer
$stageModule = "$($Module.Replace('_','.')).$Stage"
Write-Host ""
Write-Host "=== task_layer run --pipeline single:$Stage --data-root $dataRoot ==="
& $exe run --pipeline "single:$Stage" --data-root $dataRoot
$rc = $LASTEXITCODE
Write-Host ""
Write-Host "exit=$rc ; case kept at $dataRoot (-Clean to remove)"
if ($Clean) { Remove-Item $dataRoot -Recurse -Force }
exit $rc
