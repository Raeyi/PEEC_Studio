# Compact_EM FreeSpace计算模块（v1.4）

## 适用范围

本模块计算均匀真空中的无损表面PEC金属结构，支持可选的水平无限PEC参考地（默认关闭）。不支持有限电导率地面、分层介质、材料损耗、线激励、体单元或高阶物理；MATDB中导体的电导率/厚度仅作元数据校验，不进入矩阵。开启PEC地时要求频域求解，且所有金属三角形须位于平面上方并满足逐片间隙比≥1e-3。

## 输入

标准输入共四个文件，均相对dataRoot：

- `Model/model_em.msh`：Gmsh MSH 2.2或4.1（ASCII/二进制），一阶Tri3为导体；Point1/Line2仅作几何元数据，不产生支路或激励。坐标按`x_m = x_mesh/DIM`换算为米。
- `Model/matdb.json`：schemaVersion 3。每个Tri3物理标签必须解析到一个surface C材料；无需额外真空材料记录。
- `Model/source_em.json`：含`meshFile`、`meshSha256`（原始网格的64位十六进制SHA-256）与有序`ports`数组。端口端点为正Gmsh element tag（uint64精确读取），两端顺序定义V0-V1，`direction`为单位向量元数据，`impedanceOhm`必须正且有限；支持每侧多三角形的等电位端子及`negative:{groundPlane:true}`引用PEC地。端口顺序即网络端口顺序。
- `Setting/set_em.json`：schemaVersion 1扁平设置。必填`DIM`（正整数）、`FS`/`FE`（Hz正整数，FS≤FE）；计数模式需`N_FP`。`Swap_Log=1`时为对数扫频（N_FP为含端点总点数，PRECISION须为0）。标准平台仅公开频域`Solver_SET=0`；`SAVE_BIN`/`SAVE_TXT`出现即拒绝（零也拒绝）；`Taylor`、`MNA_WEIGHT`、`SOLVER_MEMORY_MB`、`SOLVER_THREADS`一律拒绝。`GROUND_PLANE_ENABLED=1`时必须给出SI米的`GROUND_PLANE_Z_METERS`（不再除DIM）。后处理开关`POST_PROCESSING`、`POST_FREQ_INDEX`（-1取最后点）、`POST_PORT`、`POST_PLOT_*`、`POST_FIELD`、`POST_FIELD_ER`（自由空间用1）。完整字段见[设置schema](../technical/schemas/set_em.schema.json)。

## 三阶段标准CLI

示例中`$case`必须是安装树之外的复制案例（安装目录保持只读）：

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

- 提取器：`--data-root`（另有调试用`--pause`）。原子发布`Result/compact_em/extract/model.peec`（PEECFS02/version2，包含几何/拓扑和P/L矩阵；完整文件SHA-256记录于阶段回执，容器尾部校验不覆盖矩阵载荷）。不写任何.bin/.txt矩阵、拓扑或临时网表；`SAVE_SP=1`时直接生成`.sp`。同时产出`Port_Mapping.csv`、`Port_Terminals.csv`、`model_state.json`、`stage_receipt.json`。
- 求解器：`--data-root`、`--accuracy strict|fast`（缺省strict；fast减少残差复核与精度回退，仅限频域）、`--workers N`、`--mkl-threads N`（正整数，缺省自动策略）。输出`Result/compact_em/solve/S_parameter.csv`、`Y_parameter.csv`、`Z_parameter.csv`（标准CSV结果）与`Solver_Diagnostics.csv`。模型在solve/post期间逐字节不变。
- 后处理器：`--data-root`（另有`--self-test`、`--pause`）。读取model.peec与求解结果，按配置在`Result/compact_em/post`下生成源图/场图；可选空间场采样网格`Model/Space_Mesh.msh`（一阶三角形，不在提取指纹内，可单独更换）。无端口频域求解会被明确拒绝。

退出码：提取0成功/1诊断失败/2未知异常；求解0/1；后处理0/1/2（无效CLI）。各阶段输出best-effort v1 `stage_receipt.json`，写失败仅警告，不改变计算退出码。

## 格式迁移与重新提取

容器PEECFS02/version2与Compact PEECPK01不二进制兼容；旧松散文件仅保留读取兼容，损坏的新包不得回退到历史文件。整个设置JSON按字节指纹绑定：改动任何设置（包括空白）、端口定义、网格或材料后，必须重新提取并重新求解；修改场采样网格只需重跑后处理。旧root布局的时域入口仍保留但不属于本发布能力。

## 构建与使用方式

本版冻结源码快照位于`../source/01_Mainline/Quasi_Static_PEEC_Tool_Free_Space`（含.slnx与build_pipeline.ps1），要求Visual Studio 2022、MSVC v143与Intel oneMKL：先设置`MKLROOT`，再打开slnx或运行`.\build_pipeline.ps1 -Rebuild`。oneMKL路径也可经`ONEAPI_ROOT`或MSBuild属性`PeecMklRoot`提供；求解线程库`libiomp5md.dll`由工程验证并复制到EXE旁（可用`PeecOpenMpRoot`/`PEEC_OPENMP_ROOT`指定）。`../source/tests/free_space_ground_kernel`为独立内核参考测试，构建产物、夹具与日志一律放入独立输出目录。定版根`E:\PEEC\Code\05_Custom_Release\Compact_EM`按冻结manifest与SHA256SUMS只读分发；本地开发根`E:\PEEC\Code\01_Mainline`用于后续迭代。

## 限制

研究性建议、宿主GDS集成与UI端到端流程尚未验收，不属于本模块交付任务，不构成阻断；源图/场图文件列于receipt但不自动注册宿主artifact/UI路由。Release x64构建、26项原生测试和10项设置schema测试已通过。三阶段解包运行及与FreeSpace基线的独立对照见[运行验收记录](../validation/runtime-verification.json)。

平台适配层还支持`--runtime-dir`（位于dataRoot的`Result/logs/compact_em`下）以及`--export`：提取为`model`、求解为`csv`、后处理为`post`。`--help`和`--source-plots`不属于本实现的CLI。旧Compact的组合导出参数不适用。

## 合并后处理与视图命名

按设置输出 `Result/compact_em/post/Source.msh` 和 `Result/compact_em/post/Field.msh`，每个文件只保存一套网格，包含多个命名 NodeData 视图。源量与空间场使用不同网格，同时启用时为两个文件。

- 实部统一后缀 `_real`，虚部 `_imag`，幅值 `_mag`。
- 全部源量开启时：`J_real/J_imag`、`Qe_mag/Qe_real/Qe_imag`、`Ve_real/Ve_imag`、`M_real/M_imag`、`Qm_mag/Qm_real/Qm_imag`、`Vm_real/Vm_imag`，共 14 视图。
- 空间场开启时：`E_real/E_imag/E_mag`、`H_real/H_imag/H_mag`，共 6 视图。
- 仍只保存 POST_FREQ_INDEX、POST_PORT 选定的一个实际频点和一个激励端口。原开关、单位和数值恢复语义不变。
- 不再生成拆分 `.msh_Re/.msh_Im/.msh_Mag`；历史拆分结果保留但不作为本次产物重新发布。标准布局全部关闭时输出 post_status.json。

新输出契约为 API 4，计算来源为 FreeSpace API 2；模型仍为 PEECFS02/version2，平台契约仍为 1.6。详情及真实验收记录见 validation/runtime-verification.json。

本次同版修订标识：`merged-post-20261111`。按用户要求，软件 v1.4、Compact API 4、FreeSpace API 2、模型格式 2、平台契约 1.6、layoutRevision 2 均保持原值。合并输出和实/虚/幅值命名保留；使用修订标识和包 SHA-256 区分交付内容。
