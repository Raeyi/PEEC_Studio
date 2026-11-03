# thermal 1.0.5 — docs_1.2 修订2

2026-11-02 重建；apiVersion=6。依据 DATA_LAYOUT §八.1/§八.2 与 THERMAL_REPACK_20261102.md。模块目录名必须为 thermal，包内仅两个 EXE、README.md、ERROR_CODES.md、module.manifest.json 五个文件。

## 安装与调用

Windows x64，原生 C++17，MSVC v143 /MT。无需 Python、Gmsh DLL、NumPy、SciPy 或 VC++ Redistributable。仅依赖 Windows 系统 KERNEL32/bcrypt；JSON、MSH 解析器为项目自有实现，无第三方运行库。原后处理算法沿用项目原生版本。

安装目录可只读；算例不放 EXE 目录内，不提供无参数自动搜索数据的模式。

```powershell
.\ThermalExtract.exe --data-root "D:\cases\demo1_4.1"
.\TemperatureProcess.exe --data-root "D:\cases\demo1_4.1"
```

第二条需要与该工程对应的真实 ALPS OP0，不要求用户维护额外绑定JSON。提取不启动 ALPS、不求温度、不生成 OP0。案例提供的历史 OP0 仅供原模型回归。

|参数|默认值/说明|
|---|---|
|--data-root|必填，独立算例根目录|
|--runtime-dir|Result/logs/thermal，可指定其子目录|
|--mesh|Model/model_thermal.msh|
|--materials|Model/matdb.json|
|--settings|Result/thermal/thermal_extract_control.json，热路提取配置|
|--simulation-settings|Setting/set_thermal.json，SPICE分析/输出设置|
|--source|Model/source_thermal.json，只含热源|
|--boundary|Model/boundary_thermal.json，独立热边界，extract/post 都接受|
|--op0|仅 post，Result/thermal/post/thermal.op0|
|--help / --version|单独使用，不要求 dataRoot|

相对路径基于 dataRoot；绝对路径必须仍在 dataRoot 内，不能指向 Result/tasks。项目目录名按规范使用字母数字下划线；父目录支持 Unicode。日志在 Result/logs/thermal，互斥锁在 .locks/thermal.pipeline.lock。只检测运行中的锁句柄，不因残留锁文件拒绝重跑。退出码0成功、2配置格式、3文件IO、4数值校验、5系统服务、6互斥锁；完整 QS 码见 ERROR_CODES.md。

## 输入契约

1. 网格：MSH4.1 ASCII，一阶三角形/四面体，兼容2.2 ASCII。高阶、二进制、六面体/棱柱不支持。后处理4.1节点标签上限10000000；2.2后处理要求连续标签。坐标按 thermal.mesh_length_scale_to_m 转为m，后处理支持m/mm/um。
2. matdb.json：schemaVersion=3；材料由 dimension + physical_material_id 指定。thermalConductivity、density、specificHeat 均为正数constant，单位 W/(m*K)、kg/m^3、J/(kg*K)。
4. source_thermal.json：schemaVersion=1，meshFile（dataRoot内实际热网格路径）、meshSha256（文件原始字节SHA256）、selection_id_type、total_power_W、sources；可有说明/单位。sources 支持 thin_layer_total_power，按 footprint_surface_entities 上均匀功率、厚度方向均匀分配。**禁止 default_external_boundary/fixed_temperature 字段**，旧格式明确报错。
5. boundary_thermal.json：独立必需输入，schemaVersion=1。当前仅支持 default_external_boundary=adiabatic，以及完整全局底面的固定温度。示例：

```json
{"schemaVersion":1,"default_external_boundary":"adiabatic","fixed_temperature":[{"name":"bottom_293p15K","location":"global_bottom_plane","surface_entities":[18,22],"temperature_K":293.15}]}
```

surface_entities 一律为 Elementary ID，不是 Physical ID。若省略 fixed_temperature，格式上视为空数组；本版独立稳态网表要求至少一组固定温度，故报“不具备固定温度边界”，不会把全绝热有热源的模型当作可求稳态。缺失文件、错误版本、负温度、无效实体等均报错。源字段不得写入边界文件。boundary_em.json 仍为预留，本模块不读取。

6. Result/thermal/post/thermal.op0：用户实际求解得到的 ALPS 输出，必须含全部 T1…TN 温度。

## 计算与输出

本次只调整文件职责，不改变算法：中层四面体 FEM、上下层双温度迹薄层 FEM；支持平行XY等厚薄层，参考面须与中层共形，材料投影须完整覆盖参考面。原薄层四面体只用于最终温度显示，不重复组装。尚未实现讨论中的 PEEC 四面体替代方法。

网表 V=温度K，I=功率W，R=K/W，C=J/K。保留 Rt 和带符号 Gcorr 才与 FEM 矩阵等价，不能删除 Gcorr。DC/RC网表自包含，默认.op；瞬态参数由用户另定。后处理不补造缺失节点，不截断低温。

- Result/thermal/extract：ThermalNetList.sp、ThermalNetList_RC.sp、thermal_nodes.csv、thermal_K.mtx、ThermalTemperatureMap.csv、extraction_report.json、thermal_binding.json、stage_receipt.json。
- Result/thermal/post：VolumeTemperatureAtOperatingPoint.msh（原网格追加K/℃ NodeData）、NodeTemperatureAtOperatingPoint.csv、MaximumTemperature.txt、TemperatureProcessReport.txt、post_binding.json、stage_receipt.json。

thermal_binding.json 对网格、材料、设置、源、**边界**和网表/映射做指纹绑定；修改边界必须重新提取，不能继续使用旧解。该内部格式变更需要新提取结果，旧版 binding 不能直接用于新版 post。receipt 是辅助排错信息，不参与调度。两个程序不写 Model/Setting、不发布正式件、不读写 Result/tasks。

## 平台接入和本地验证

manifest 用新版官方 Schema 校验；新增 thermal/boundary.def，两个阶段的 argsTemplate 均传 --boundary {artifact:thermal/boundary.def}。布局字段按Schema仍写 peec-data/v1，具体修订记 dataLayoutRevision=1.2 revision 2。当前API为6、批次1.0.5：按用户指示将提取配置移至Result/thermal/thermal_extract_control.json；继续保留此前取消结果绑定输入及临时OP0落点的改动。

案例、源码、验收记录在模块包外，不能装入 thermal/。案例包 thermal_cases_20261102 中含完整Model/Setting、历史OP0及已生成Result结果；字段搬家后网表与历史求解对应网表保持相同。两阶段调用见案例README。验证脚本使用本交付内的参考数据，不依赖外部案例；开发验证需要Python3.12/jsonschema，运行EXE不需要。

平台提供的 THERMAL_REPACK_20261102.md 说明旧1.0.1已通过平台E2E；该结论不等同于本1.0.5已通过平台E2E。本次完成模块本地测试，平台收到新manifest后需重跑thermal_basic。未运行新的ALPS物理求解。

重编译：包外 source/build.ps1，VS2022 C++和Windows SDK；输出source/build。无examples/templates/defaultTemplate机制。

正式结果清单已包括 thermal/temperature.report → Result/thermal/post/TemperatureProcessReport.txt，供 task.result/resultFiles 收录。thermal_nodes.csv 和 thermal_K.mtx 继续登记为内部工件，不改变既有用途。
## 用户指定的临时OP0路径（1.0.3）

取消 thermal_run.json 文件和 --result-binding 参数；无该文件也能正常后处理。OP0默认放 Result/thermal/post/thermal.op0，可用 --op0 指定 dataRoot 内其他路径。模块不会覆写或删除输入OP0。保留网格、材料、设置、源、边界与提取结果一致性校验，以及OP0节点集合/数值/映射检查；不再检查OP0是否确实来自当前网表，导入方负责选择正确结果。自动输出 post_binding.json 只是本次输入与输出指纹记录，op0NetlistBindingVerified=false，不作为额外前置输入。

该改动优先执行用户指示；docs_1.2重发说明中附带thermal_run.json的旧案例要求不再采用。临时路径待用户后续确定。案例附带Result/thermal/extract与post实际结果，其中温度来自历史OP0映射，未新增ALPS求解。

## 文件职责（1.0.5，按用户纠正）

- `Setting/set_thermal.json`：用户交互产生的 **SPICE 仿真设置**，只包含分析类型和输出设置。
- `Result/thermal/thermal_extract_control.json`：原提取控制文件，内容原样保留，暂存于此；含网格单位、体域、薄层、材料关联。它不是仿真设置，后续由用户决定拆分。
- `Model/source_thermal.json`、`Model/boundary_thermal.json`：热源和热边界。
- `Result/thermal/post/thermal.op0`：保持用户指定的临时路径。

当前仿真设置示例：
```json
{"schemaVersion":1,"analysis":"op","probe":["V(*)"]}
```
提取程序将其写入两份网表的 `.op` 和 `.probe V(*)`，末尾 `.end` 由程序生成。支持 `probe` 列表中的 `V(*)` 或 `V(Tn)`；当前分析类型仅支持 `op`，其他类型明确报错，尚不支持通过此文件配置瞬态。后处理仍要求 OP0 包含全部热节点，完整温度可视化应保留 `V(*)`。

`--settings` 继续指定提取控制文件，默认 `Result/thermal/thermal_extract_control.json`；新增 `--simulation-settings` 指定仿真设置，默认 `Setting/set_thermal.json`。两个文件都是输入，程序不覆写。仿真设置更改后需重新生成网表并使用相应求解结果；后处理校验设置指纹。`Setting/set.json` 不读取。

该划分按用户最新指示实施，优先于 docs_1.2 的旧布局。API6、版本1.0.5；平台应使用更新后的 manifest。未实现或运行 ALPS 求解器，也未新增 UI。
