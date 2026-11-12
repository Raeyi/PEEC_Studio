# Compact_EM v1.4 R2_PEC_Standard输入

这是提供的FreeSpace包中的纯金属标准案例。Model和Setting文件保持原始字节；请复制到安装树之外运行。提取后生成PEECFS02/version2模型，结果写Result/compact_em。

```powershell
$releaseRoot = 'E:\PEEC\Code\05_Custom_Release\Compact_EM'
$module = Join-Path $releaseRoot 'compact_em'
$caseParent = 'E:\PEEC\Runs'
$case = Join-Path $caseParent 'R2_PEC_Standard_v1_4'
if (Test-Path -LiteralPath $case) { throw '请为本次运行指定一个新的案例目录' }
New-Item -ItemType Directory -Path $caseParent -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $releaseRoot 'cases\R2_PEC_Standard') -Destination $case -Recurse
& (Join-Path $module 'PEEC_Model_Extractor.exe') --data-root $case
& (Join-Path $module 'PEEC_Solver.exe') --data-root $case
& (Join-Path $module 'PEEC_Post_Processing.exe') --data-root $case
```

本次同版修订标识：`merged-post-20261111`。按用户要求，软件 v1.4、Compact API 4、FreeSpace API 2、模型格式 2、平台契约 1.6、layoutRevision 2 均保持原值。合并输出和实/虚/幅值命名保留；使用修订标识和包 SHA-256 区分交付内容。
