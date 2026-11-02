# Compact EM 准静态 PEEC 模块

模块包 id：`compact_em`。交付批次2026.10.30，API 2，遵循PEEC Studio数据布局规范文档v1.1；机器布局主版本为`peec-data/v1`。

面向纯金属自由空间与无损异质介质模型，提供参数提取、频域端口求解、S参数图及可选源/场后处理。导体按PEC近似，MATDB电导率与厚度仅校验，不形成导体电阻或损耗；仅支持频域，Taylor>0不启用全波传播。几何/积分仍有单精度兼容路径，不宣称端到端全双精度。

## 快速开始

安装目录只读；算例数据根必须在安装目录树之外，预先由UI/用户/开发工具准备。四程序的正常入口要求`--data-root`，`--help`可独立运行。

```powershell
$dataRoot = 'D:\cases\example'
.\PEEC_Model_Extractor.exe --data-root $dataRoot --export post,report
.\PEEC_Solver.exe --data-root $dataRoot --export post,touchstone
.\PEEC_S_Parameter_Plotter.exe --data-root $dataRoot
.\PEEC_Post_Processing.exe --data-root $dataRoot --source-plots j,qe,ve --parts mag
```

`--data-root`自身的相对路径按启动CWD转绝对；其余相对数据路径按dataRoot解析。运行目录不影响输入选择。`--runtime-dir`可选，缺省`Result/logs/compact_em/`，自定义值必须在该子树内且保持同卷。锁固定`.locks/compact_em.run.lock`，通过句柄占有，零字节文件可长期存在；不要手工删除。四阶段共享模块级排他锁。

## 输入契约

```text
<dataRoot>/
  Model/<唯一主网格>.msh
  Model/matdb.json
  Model/port_em.json
  Setting/set.json
```

主网格支持MSH2.2/4.1 ASCII与二进制、4.0 ASCII；一阶三角形/四面体与点，拒绝不支持的激励线和高阶单元。Model目录必须恰好一份.msh，不再排除Space_Mesh。建模链坐标单位为mm，对应DIM=1000；转换遵循`x_m=x_mesh/DIM`，禁止靠几何数值猜单位。MATDB schemaVersion=3，物理组数字匹配材料id；导体surface/C要求正电导率和正厚度，实际使用的volume/D必须无损。表面条目顶层thickness/thicknessUnit支持m/mm/um；properties中conductivity单位S/m、relativePermittivity/relativePermeability单位1，使用constant模型。隐式外部为真空。详细材料/端口示例见examples。

`port_em.json`包含meshFile、主网格原始字节meshSha256和非空ports。每端口含两个不同的原始Gmsh三角形元素triangleIds、单位direction、有限正impedanceOhm。元素ID上限2^53-1，端口数组顺序决定Sij端口顺序；direction与第一面心指向第二面心的连线同向。改网格后须更新端口与SHA-256。

## 计算设置

`Setting/set.json`为顶层平铺JSON，schemaVersion必须为整数1。示例：

```json
{"schemaVersion":1,"DIM":1000,"FS":"1e9","FE":"10e9","N_FP":100,"Solver_SET":0,"MNA_WEIGHT":1}
```

格式约束见set.schema.json。已知参数保持现有范围/默认值/跨字段验证；FS/FE使用精确int64十进制token解析，支持整数科学计数法及数字字符串。布尔值仅用于0/1开关；未知键解析后忽略，重复键、坏JSON、非法已知字段仍拒绝。材料常数来自matdb，不由设置覆盖。

常用参数：DIM必填且正；0<FS<=FE；PRECISION=0时N_FP必填且正，正PRECISION按Hz或decade步长；Swap_Log=0线性/1对数，不能整除时追加FE。MNA_WEIGHT缺省0.8且正；SOLVER_MEMORY_MB=0自动（MiB新增分配预算）、SOLVER_THREADS=0自动（最大4096）。Solver_SET仅0受支持。Taylor缺省0；SAVE_BIN/SAVE_TXT/SAVE_SP缺省1/0/0，POST_PROCESSING缺省1、POST_FREQ_INDEX=-1、POST_PORT=1、六个POST_PLOT_*缺省1、POST_FIELD缺省0。这些SAVE/POST开关仅在legacy输出策略下隐式启用文件，minimal由CLI显式选择。

过渡版直接CLI支持旧Setting/set.txt，发弃用warning；两者并存时set.json优先并warning；坏JSON不回退TXT。当前旧模块化SET_EM.json须用仓库scripts/convert-compact-settings.py显式转换，不参与自动搜索。转换保留原参数，拒绝覆盖目标。正式task_layer预检要求set.json，旧算例应先迁移，不能用defaultTemplate替代旧参数。模板templates/default_set.json由task_layer复制；计算模块不写Model/Setting。

## 输出契约

| 阶段 | 默认/按需产物 |
|---|---|
| extract | Result/compact_em/extract/model.peec；按需矩阵、报告、SPICE、Post_Model.txt |
| solve | Result/compact_em/solve/S_parameter.csv；按需Z/Y、Touchstone、Post_Source.txt |
| plot | Result/compact_em/plot/*.svg，PNG可选 |
| post | Result/compact_em/post/Source.msh、Field.msh，以及快照副本，均按请求 |

model.peec保持PEECPK01 V1二进制布局，float64 P/L，L为H、P为1/F。S CSV使用frequency_Hz与Sij_real/imag，S无量纲，i为响应端口、j为激励端口，1-based列名；Z/Y单位Ω/S。源快照仍为PEEC_POST_SOURCE_V2，显式CHARGES满足同模型Pq=V；V1源快照被拒绝。包及快照细节见OUTPUT_FORMAT.md。

各阶段artifacts.json是必须的内部数据完整性索引（schemaVersion=1，provider/stage/inputs/outputs，路径相对dataRoot，值为SHA-256），用来检查实际输入、输出及批次关系。索引不含任务状态或调度时间，下游不读取其他任务state。只登记本轮成功产物；输入变化、输出篡改、缺索引需重跑上游。索引最后发布；遗留未选文件不被当成本批产物。失败调用不删除旧完整结果；旧结果是否仍有效由其实际输入指纹决定。

stage_receipt.json是可选自证，按规范写输入输出SHA-256、时间、退出码与参数。成功/失败均尽力写，缺失或写失败不阻断完整工件消费，也不改变原始计算退出码。任务唯一调度状态由task_layer维护于Result/tasks，模块完全不读写该目录。

Post_Model/Post_Source原始内部快照分别属于extract/solve；post消费后在自身目录发布同名副本。空间场采样网格缺省放Result/compact_em/post/input/Space_Mesh.msh，或通过--space-mesh指定dataRoot内路径。该私有输入由开发工具显式准备；平台UI导入位置仍需联调约定，UI不能直接写Result。采样网格不能作为第二份.msh放Model。无视图选择时Post成功空操作，receipt可记录空outputs。

## 全部命令行选项

共同：--data-root <dir>、--runtime-dir <dir>、--help、--output-profile minimal|legacy（默认minimal）。Extractor/Solver/Post另支持--pause。列表值为无空格逗号分隔。

| 程序 | 选项 |
|---|---|
| Extractor | --matrix-format package/bin/txt/both；--export post,ports,report,spice |
| Solver | --export post,z,y,touchstone,legacy-text,diagnostics；--s-columns ri/full；--post-freq-index <-1或正整数>；--post-port <正整数> |
| Plotter | --format svg,png；--export-gnuplot；--script-only；--require-gnuplot；-h；--input <path>；--output-dir <path>；--gnuplot <exe> |
| Post | --source-plots j,j_vec,qe,ve,m,qm,vm；--field-plots e,h；--parts real,imag,mag；--export field-materials；--space-mesh <path> |

minimal选择--post-freq-index/--post-port须同时--export post。Plot输出覆盖只能位于plot阶段内，所有数据输入须在dataRoot；GNUplot属于外部部署工具可用绝对路径、GNUPLOT_EXE或PATH。默认SVG无需GNUplot。j与j_vec同义，real/imag是矢量，mag为sqrt(|Jx|²+|Jy|²+|Jz|²)标量。minimal默认S列RI，legacy默认full；显式选项优先。

退出码：所有程序0成功、1已知/标准异常失败；Extractor未知异常为2，其余未知异常为1。stderr为UTF-8 QS JSON，机器按code和severity判断；日志路径和errorContract见module.manifest.json，详细处置见ERROR_CODES.md。

## 构建与自测

Windows x64、VS2022/MSVC v143、oneMKL。Release使用/MT与MKL LP64 sequential静态库；系统Windows DLL仍由系统提供。构建依赖仓库scripts和01_Mainline/Shared相对布局。Shared Settings_IO与Mesh_IO/Plotter为仓库内部源码模块，随源码批次管理；本批实际使用MSVC 14.44.35207与oneMKL 2025.0.0；四程序PE导入表仅含Windows系统KERNEL32.dll、bcrypt.dll，记录见本次验证报告。

```powershell
.\build_pipeline.ps1 -Rebuild
python ..\..\scripts\test-compact-module-contract.py --runtime .\pipeline --baseline-runtime D:\baseline\compact --schema .\module.manifest.schema.json --output D:\validation\new-run
python ..\..\scripts\test-compact-review-fixes.py --runtime .\pipeline --output D:\validation\new-numerical-run
powershell -NoProfile -File ..\..\scripts\package-compact-module.ps1 -ProjectRoot . -Destination D:\deliveries\compact_em
```

examples只读，仅供开发复制到外置dataRoot运行，不是包内可写DATA。标准介质与双层纯金属用于内部一致性回归，不代表实测或外部全波精度验收。task_layer/dev_case注册、publisher与完整UI链需平台工具到位后联调。
