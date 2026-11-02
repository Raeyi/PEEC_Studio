# thermal 原生模块 1.0

依据 docs_1.2 内 v1.2 数据布局、模块契约和接入规范。Windows x64，C++17，MSVC v143 /MT 静态运行库。无需 Python、NumPy、SciPy、Gmsh DLL 或 VC++ Redistributable。系统 DLL（KERNEL32、BCRYPT 等）仍由 Windows 提供。解析器为项目自有实现，没有第三方运行时库。

## 安装及调用

把本目录以 `thermal` 名称部署到 MODULE_ROOT。安装目录可只读。数据必须在独立 dataRoot。无参数启动不会在 EXE 旁搜索文件。

```powershell
.\ThermalExtract.exe --data-root "D:\cases\demo1"
# 用户另行使用 ALPS 求解 Result/thermal/extract/ThermalNetList.sp
.\TemperatureProcess.exe --data-root "D:\cases\demo1"
```

|CLI|默认值/作用|
|---|---|
|--data-root|必填，独立算例根目录|
|--runtime-dir|Result/logs/thermal，可指定其子目录|
|--mesh|Model/model_thermal.msh|
|--materials|Model/matdb.json|
|--settings|Setting/set.json|
|--source|Model/source_thermal.json|
|--op0|仅 post，Model/thermal.op0|
|--result-binding|仅 post，Model/thermal_run.json|
|--help / --version|独立使用，打印帮助或版本|

路径可为 dataRoot 内相对路径或绝对路径，支持中文和空格。文件路径不能越出 dataRoot；不读取/写入 Result/tasks。私有互斥锁在 .locks/thermal.pipeline.lock，锁文件存在不代表正在运行。退出码及 QS 错误见 ERROR_CODES.md。

## 输入文件职责

- 网格：MSH 4.1 ASCII，一阶三角形/四面体；兼容 2.2 ASCII。不能读取 binary、高阶单元、棱柱/六面体。后处理节点标签上限 10000000，2.2 后处理要求连续标签。
- matdb.json：schemaVersion=3，按 dimension + physical_material_id 查材料。thermalConductivity、density、specificHeat 均为正数常量，单位分别 W/(m*K)、kg/m^3、J/(kg*K)。电导率不参与热计算。
- Setting/set.json：schemaVersion=1，保留标准电磁字段，新增 thermal 对象。thermal.mesh_length_scale_to_m 指定坐标到 m 的比例（后处理支持 m/mm/um）；tetrahedral_domains 定义体网格和材料；thin_layers 定义参考面、厚度、材料投影和仅用于显示的体。
- Model/source_thermal.json：schemaVersion=1，meshFile 为 dataRoot 内路径，meshSha256 是网格文件原始字节 SHA256；sources 定义各层总功率，fixed_temperature 定义底面温度，default_external_boundary=adiabatic。
- Model/thermal.op0：由用户/求解器导入的真实 ALPS 结果，不由本模块生成。必须包含全部 T1…TN 节点温度。
- Model/thermal_run.json：schemaVersion=1，netlistSha256 为实际求解网表 SHA256，op0Sha256 为结果 SHA256。用来防止误选文件；哈希本身不能证明求解器确实运行过，导入方须保证来源真实。

实体选择全部用 Elementary ID；材料 ID 属于材料库。不能把 Physical ID 当成几何实体 ID。

## 算法范围

本版针对当前三层模型：中层线性四面体；上下层为平行 XY 的等厚薄层、双温度迹。参考面须与中层网格共形；外表面网格可不对齐。薄层材料通过三角形交叠积分，包含面内和厚度方向传热。原薄层四面体不重复组装，只用于求解后的温度插值。

支持金属投影区域内均匀功率、全局完整底面固定温度、其他外面绝热。不支持局部底面固定温度、对流、辐射、温变材料或曲面薄层。遇到未支持设置报错，不静默代替。

网表 V 对应 K，I 对应 W，R 为 K/W，C 为 J/K。Rt 与带符号 Gcorr 共同表示 FEM 矩阵；不能删除 Gcorr。RC 网表自包含，默认仍为 .op，不自动设置瞬态工况。提取程序不求温度、不启动 ALPS、不生成 .op0。后处理不补造缺失温度，也不截断低于边界温度的数值。

## 输出

`Result/thermal/extract/`：ThermalNetList.sp、ThermalNetList_RC.sp（主交付），thermal_nodes.csv（节点坐标/热容/热源），thermal_K.mtx（MatrixMarket 热导矩阵），ThermalTemperatureMap.csv（原网格节点到热节点的权重），extraction_report.json，thermal_binding.json。

`Result/thermal/post/`：VolumeTemperatureAtOperatingPoint.msh（原网格加 K/℃ NodeData）、NodeTemperatureAtOperatingPoint.csv、MaximumTemperature.txt、TemperatureProcessReport.txt、post_binding.json。

每阶段可写 stage_receipt.json；调度不能依赖它，task_layer 状态由平台维护。映射、绑定、节点表与矩阵是本模块内部件；公共产物见 manifest。此版不声明 publishTo，也不自行修改 Model/Setting。

## 平台接入与待联调项

manifest 已用随标准的 JSON Schema 校验。样例采用 UI 提供的 user/thermal.mesh 和 user/thermal.materials ref；集成建模链时由平台路由上游工件，并传入 --mesh/--materials，不能在程序中硬编码上游目录。包外样例 Model 只有一份热网格；与 EM 同算例时不得向 Model 再放第二份主网格，应通过 ref 指向热链产物。

post 是可选阶段；OP0 和 thermal_run.json 是该阶段显式 CLI 输入，不作为整个模块的公共前置工件，避免 extract 被尚未生成的 OP0 阻塞。平台在 post 启动前可做存在性检查，本程序始终做内容及指纹深检。

标准正文说消费者只写 ref，但官方 Schema 要求 consumed.path；本包按 Schema 保留声明路径，argsTemplate 实际使用 artifact ref 路由。

未提供 task_layer.exe，尚未执行真实平台注册、路由表/DAG/API 窗口校验和 task_layer dev_case。不能把本地模块测试当作平台全链路验收。交付根目录 validation 中记录已测与未测项。

## 本地测试及源码

包外 thermal_cases_20261102/demo1_4.1 是独立算例；运行案例包目录“开始测试.ps1”。用户完成 ALPS 后运行案例包目录“导入ALPS结果.ps1 -Op0 ... -SolvedNetlist ...”，该脚本是 UI/用户导入辅助工具，不属于模块安装目录。

源码在交付根目录 source，运行 source/build.ps1（VS2022 C++/Windows SDK）。输出 source/build。开发验收脚本需要 Python3.12 + jsonschema；EXE 运行不需要这些环境。

## 2026-11-02 / docs_1.2 重发变更

本模块目录仅含两个 EXE、README.md、ERROR_CODES.md、module.manifest.json 共五个文件（四类交付件）。案例独立交付 thermal_cases_20261102.zip，不再携带 templates/examples。

默认读取 Model/source_thermal.json，显式参数为 --source，原 --ports 不再接受，故 apiVersion 升至 2；minApiVersion=1 只表示上游兼容窗口。此版不会回退读取旧端口文件。配置内容及计算方法不变，仍为已验证 FEM/双迹薄层，尚未实现讨论中的 PEEC 四面体方案。

boundary_thermal.json 在 v1.2 尚无格式及消费方；本版不读取、不创建、不声明该 ref。原 fixed_temperature/default_external_boundary 字段继续放在 source_thermal.json，直至正式边界契约定义。

按新版官方 Schema，capabilities.dataLayout 仍为 peec-data/v1；具体遵循规范修订号另记 dataLayoutRevision=1.2。案例项目名使用字母、数字、下划线，不含空格或特殊字符；父路径支持 Unicode。

源码和开发测试在交付总目录中另附；只有 thermal/ 或模块 zip 应安装到 MODULE_ROOT。
