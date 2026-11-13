# Compact_EM 1.4.3 模块使用说明

软件版本1.4.3，API7，manifest格式版本1；本次交付重切标识release1（2026-11-13）。模型PEECFS02/version2，数据布局peec-data/v1 revision1.6。模块提供extract→solve→post三个阶段。

## 安装与运行依赖

将模块ZIP完整解压，保留compact_em目录中的7个文件：三个EXE、libiomp5md.dll、module.manifest.json、README.md、ERROR_CODES.md。DLL须与PEEC_Solver.exe同目录，程序与已经验收的1.4.3候选逐字节一致。

运行环境为Windows x64；libiomp5md.dll还需要系统UCRT和x64 VC运行库提供的VCRUNTIME140.dll。无需安装Visual Studio或Intel oneAPI SDK。缺少加载依赖时进程可能无法进入main，不会产生模块QS错误。请在部署时检查依赖齐全。

内部数值库为Intel oneMKL2025.0.0（静态链接），线程运行库为Intel OpenMP libiomp5md.dll（ProductVersion5.0，FileVersion20250910）。本批按用户明确要求保留该DLL；不宣称为全静态/零依赖模块。案例、技术文档、源码及验证工件不装入模块目录。

## 适用范围

均匀真空中的无损表面PEC金属，可选水平无限PEC参考地（默认关闭）。不提供材料损耗、分层介质、线激励、体或高阶物理。MATDB导体电导率/厚度仅作元数据校验，不进入PEC矩阵。开启PEC地要求所有金属三角形位于平面上方并满足逐片间隙比≥1e-3。

## 输入

所有路径相对显式--data-root指定的案例目录，模块安装目录只读使用。

- Model/model_em.msh：Gmsh MSH2.2或4.1（ASCII/二进制）。Tri3作为导体；CAD Point1/Line2仅作几何元数据。坐标换算为SI米：x_m=x_mesh/DIM。
- Model/matdb.json：schemaVersion3，Tri3物理标签须对应surface C材料；nextId为大于全部材料id的整数。
- Model/source_em.json：meshFile=model_em.msh，meshSha256为该原始网格的64位十六进制SHA256，ports为非空有序数组。端子使用正Gmsh triangle tag，端点顺序定义V0-V1；direction为单位向量元数据，impedanceOhm为正有限值。支持每侧多个三角形及negative:{groundPlane:true}。不接受nextId/nextID。
- Setting/set_em.json：平铺JSON，schemaVersion=2。设置及源定义仅接受这两个标准文件名和位置。

旧Setting/set.txt、Setting/set.json、root SET_EM.json/set.txt/set.json，以及Model或root中的port.json/port_em.json均拒绝；新旧并存也拒绝，不回退。请按新结构迁移，完整错误码见[ERROR_CODES.md](ERROR_CODES.md)。仍必须提供上述网格与材料文件。

## 设置字段

schemaVersion、DIM、FS、FE必填；DIM为正整数，FS/FE为正整数Hz（允许精确数字字符串），FS≤FE。计数扫频提供N_FP；0≤PRECISION≤1时按N_FP计数，PRECISION>1时按Hz步长。Swap_Log=1使用对数扫频，N_FP为含端点总数、PRECISION必须为0。

其他公开字段：SAVE_SP；Solver_SET（标准流程仅0=频域）；POST_PROCESSING；POST_FREQ_INDEX（1起，-1为最后点）；POST_PORT（1起）；POST_PLOT_J/QE/VE/M/QM/VM；POST_FIELD；POST_FIELD_ER（自由空间为1）；GROUND_PLANE_ENABLED（默认0）；GROUND_PLANE_Z_METERS（启用地面时必填，单位SI米，不再除DIM）。

SAVE_BIN、SAVE_TXT出现即拒绝（零也拒绝）；Taylor、MNA_WEIGHT、SOLVER_MEMORY_MB、SOLVER_THREADS仍拒绝；未知可选键忽略。设置格式或默认配置的变化需要平台同步其模板。本模块不携带默认设置文件。

## 三阶段调用

案例另见compact_em_cases_20261113_1.4.3_release1.zip，解压后使用compact_em_cases_20261113/R2_PEC_Standard。将它复制到安装树外新的可写位置后调用：

```powershell
$module = 'C:\PEEC\modules\compact_em'
$case = 'D:\PEEC_Runs\R2_PEC_Standard_1_4_3'
& (Join-Path $module 'PEEC_Model_Extractor.exe') --data-root $case
& (Join-Path $module 'PEEC_Solver.exe') --data-root $case
& (Join-Path $module 'PEEC_Post_Processing.exe') --data-root $case
```

同一dataRoot的阶段串行调用。三个计算入口均必须提供--data-root，不搜索EXE旁Data/Input。

| 程序 | 参数 |
|---|---|
| PEEC_Model_Extractor.exe | --data-root <case>；--runtime-dir <dir>；--export model（省略等价；native为历史兼容值）；--pause |
| PEEC_Solver.exe | --data-root <case>；--runtime-dir <dir>；--export csv；--accuracy strict\|fast（默认strict）；--workers N；--mkl-threads N；--pause |
| PEEC_Post_Processing.exe | --data-root <case>；--runtime-dir <dir>；--export post；--self-test；--pause |

--runtime-dir省略时为Result/logs/compact_em，显式指定须位于该目录或其子目录，相对路径以dataRoot解析。N为正整数；workers和mkl-threads都是上限，实际按CPU、内存、频点和MKL限制裁剪。保留原Intel OpenMP线程后端和频点间并行，没有改用sequential MKL。

fast减少残差复核/精度回退，仅用于频域。--help、--s-columns、--post-freq-index、--post-port、--source-plots、--field-plots、--format、--parts不属于本版公开CLI；后处理选择在set_em.json中配置。

## 输出、日志和退出码

- extract：Result/compact_em/extract/model.peec、model_state.json、阶段回执及端口映射；SAVE_SP=1时生成SPICE网表。不生成bin/txt松散矩阵。
- solve：Result/compact_em/solve/S_parameter.csv、Y_parameter.csv、Z_parameter.csv、Solver_Diagnostics.csv与后处理源快照。
- post：按配置生成Result/compact_em/post/Source.msh和Field.msh；全部关闭时写post_status.json。可选空间场采样网格为Model/Space_Mesh.msh，不在提取指纹中，可单独更换后重跑post。

Source.msh全部源量开启时含14个_real/_imag/_mag视图；Field.msh含E/H的6个视图。只导出设置选定的一个频点及一个激励端口，越界明确报错。源图和场图文件列于回执，不自动注册宿主UI路由。

| 程序 | 退出码 |
|---|---|
| extract | 0成功；1诊断失败；2未知非标准异常 |
| solve | 0成功；1失败 |
| post | 0成功；1运行/平台参数失败；2UsageError（未知选项或缺data-root参数值） |

失败stderr输出9字段UTF8 JSON（code/severity/message_dev/message_user_zh/user_action/module/trace_id/file_refs/recoverable），详情见ERROR_CODES.md。同次调用trace一致；合法日志目录已知时追加jsonl，日志失败只warning，不覆盖主错。各阶段stage_receipt.json为可选v1自证，写失败不改变计算退出码；必需内部完整性状态仍严格校验。模块不读写Result/tasks。

旧名、缺文件或缺显式dataRoot在业务Result及阶段marker修改前拒绝，诊断日志可以追加。OOM保留完整应急JSON。设置整体按字节、网格、材料或源定义变化后应重新extract/solve；损坏的新包不回退到历史松散结果。

## 本地自测与验收

```powershell
.\PEEC_Post_Processing.exe --self-test
```

不读取用户输入，预期退出0并报告20个命名视图。完整案例按上文三阶段执行；R2案例预期S/Y/Z各100数据行、实际频点1.09–10GHz，POST_FIELD=0时不要求Field.msh。

本版正式定版复用已验收1.4.3候选的三个EXE，保留原Intel OpenMP DLL；没有修改已验证程序。回归与docs1.6核对记录随本地冻结归档保留。平台GDS/task_layer/UI全流仍须联调。

## schema2迁移与同根互斥

旧受限FreeSpace设置错误沿用了schema1；当前accept为2..2。现有受支持案例只改schemaVersion为2，随后重新提取/求解；其他数值字段、单位和采样定义保持。已知不支持的物理/输出字段必须人工核对，迁移工具不会静默删除；JSON合法的未知可选键忽略但仍纳入全文指纹。accuracy/workers/mklThreads只允许通过CLI。设置schema及迁移工具在模块ZIP外的technical目录提供；平台模板需同步改成schema2。

extract/solve/post共同持有.locks/compact_em.run.lock的独占Windows字节锁。忙碌返回QS-COMM-MODULE-BUSY（exit1）且不改业务结果或marker；锁文件保留、陈旧文件可复用，正常/异常进程退出释放OS锁。不要删除运行中的锁文件。

求解器不再生成Touchstone文件，包括native私有目录中的sNp。历史结果不自动清理。DLL与标准文件名策略保留。本版已正式定版为当前Compact_EM发行，固定入口及CURRENT以本次1.4.3记录为准。
