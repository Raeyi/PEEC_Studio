# demo1_4.1 自包含回归案例

来源：原 demo1_4.1 三层模型，MSH4.1 ASCII；物理输入继承已交付版本，本次按修订2拆分源和边界，并清除纯热设置中的EM字段。四面体与双迹薄层 FEM 方法不变。

## 输入

Model/model_thermal.msh：9564 个原网格节点。Model/matdb.json：铜 k=400 W/(m·K)、ρ=8960 kg/m³、cp=385 J/(kg·K)；硅 k=130、ρ=2330、cp=700。Result/thermal/thermal_extract_control.json 仅含 schemaVersion 和 thermal 对象，无EM字段。

Model/source_thermal.json：使用 Elementary ID，完整网格 SHA256 与 meshFile=Model/model_thermal.msh 绑定。上层 Volume2 的参考面 3/10，下层 Volume3 的参考面16，中层 Volume4；薄层厚度 5e-7 m。每层金属 1000 W，总功率 2000 W。独立 Model/boundary_thermal.json 定义完整底面18/22为293.15 K，其余绝热。投影材料面完整覆盖参考面。

Result/thermal/post/thermal.op0：原模型已有的 ALPS 历史结果，用于后处理回归；不是本次重发版重新启动 ALPS 得到的结果。此次已验证新旧网表逐字节一致。不要把此文件用于修改过的几何、材料、功率或边界。

## 运行（PowerShell，假定案例包与 thermal 同级）

```powershell
& ..\..\thermal\ThermalExtract.exe --data-root $PWD.Path
& ..\..\thermal\TemperatureProcess.exe --data-root $PWD.Path
```

在本案例目录执行上面命令；安装位置不同则修改 EXE 路径，所有输入均在本 dataRoot 内。案例分发时带 Result/thermal/extract 和 post 的实际输出，重跑时会更新各自产物。

## 预期输出和验收点

- Result/thermal/extract/ThermalNetList.sp 与 ThermalNetList_RC.sp：独立可运行网表，含 .op、.probe V(*)、单个 .end。
- extraction_report.json：status=SUCCESS，networkNodes=12750，meshNodes=9564，power_W≈2000（绝对误差<1e-8），capacity_J_per_K≈10.5897179444（误差<1e-9）；positiveResistors=62134、signedCorrections=30711、fixedTemperatureSources=4452。maxCoverageRelativeError<2e-7。
- Result/thermal/post/NodeTemperatureAtOperatingPoint.csv：9564 行节点温度。
- MaximumTemperature.txt：maximum_mapped_temperature_K≈344.1037（容差1e-4 K），℃≈70.9537。
- TemperatureProcessReport.txt：minimum_mapped_temperature_K≈293.1271、映射热节点12750、原网格节点9564、values_clamped=false。
- VolumeTemperatureAtOperatingPoint.msh：原网格上附加 K 与 ℃ NodeData，可由 Gmsh 打开。

最低温度略低于固定边界是既有离散/历史结果特征，本次保持原数据，不人为截断。该范围不是网格收敛或物理精度认证。

## 限制

当前支持平行XY的等厚薄层与中间四面体、完整底面固定温度、其他外界绝热；不支持局部固定温度/对流/辐射。薄层参考面须与中层共形。boundary_thermal.json 为必需输入；固定底面温度和默认绝热全部在该文件中。

温度样例只验证 OP0 解析与映射；新案例需用户实际运行 ALPS，再使用案例包的导入脚本进行后处理。使用接口规范见 thermal/README.md。

1.0.2重发说明：提取配置已独立且无EM字段；边界文件独立并经--boundary传入；后处理报告由manifest的thermal/temperature.report正式声明。模型、功率、厚度和数值基线不变。

1.0.3：取消thermal_run.json；OP0临时落在Result/thermal/post/thermal.op0。案例Result已包含实际提取/映射输出。


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
