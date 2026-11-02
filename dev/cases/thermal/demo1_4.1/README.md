# demo1_4.1 自包含回归案例

来源：原 demo1_4.1 三层模型，MSH4.1 ASCII；物理输入继承已交付版本，只把输入文件重命名为 source_thermal.json。四面体与双迹薄层 FEM 方法不变。

## 输入

Model/model_thermal.msh：9564 个原网格节点。Model/matdb.json：铜 k=400 W/(m·K)、ρ=8960 kg/m³、cp=385 J/(kg·K)；硅 k=130、ρ=2330、cp=700。Setting/set.json 含 thermal 对象。

Model/source_thermal.json：使用 Elementary ID，完整网格 SHA256 与 meshFile=Model/model_thermal.msh 绑定。上层 Volume2 的参考面 3/10，下层 Volume3 的参考面16，中层 Volume4；薄层厚度 5e-7 m。每层金属 1000 W，总功率 2000 W。完整底面18/22为293.15 K，其余绝热。投影材料面完整覆盖参考面。

Model/thermal.op0 + thermal_run.json：原模型已有的 ALPS 历史结果及带来源说明的绑定文件，用于后处理回归；不是本次重发版重新启动 ALPS 得到的结果。此次已验证新旧网表逐字节一致。不要把此文件用于修改过的几何、材料、功率或边界。

## 运行（PowerShell，假定案例包与 thermal 同级）

```powershell
& ..\..\thermal\ThermalExtract.exe --data-root $PWD.Path
& ..\..\thermal\TemperatureProcess.exe --data-root $PWD.Path
```

在本案例目录执行上面命令；安装位置不同则修改 EXE 路径，所有输入均在本 dataRoot 内。案例分发时不带 Result，两个阶段会生成各自产物。

## 预期输出和验收点

- Result/thermal/extract/ThermalNetList.sp 与 ThermalNetList_RC.sp：独立可运行网表，含 .op、.probe V(*)、单个 .end。
- extraction_report.json：status=SUCCESS，networkNodes=12750，meshNodes=9564，power_W≈2000（绝对误差<1e-8），capacity_J_per_K≈10.5897179444（误差<1e-9）；positiveResistors=62134、signedCorrections=30711、fixedTemperatureSources=4452。maxCoverageRelativeError<2e-7。
- Result/thermal/post/NodeTemperatureAtOperatingPoint.csv：9564 行节点温度。
- MaximumTemperature.txt：maximum_mapped_temperature_K≈344.1037（容差1e-4 K），℃≈70.9537。
- TemperatureProcessReport.txt：minimum_mapped_temperature_K≈293.1271、映射热节点12750、原网格节点9564、values_clamped=false。
- VolumeTemperatureAtOperatingPoint.msh：原网格上附加 K 与 ℃ NodeData，可由 Gmsh 打开。

最低温度略低于固定边界是既有离散/历史结果特征，本次保持原数据，不人为截断。该范围不是网格收敛或物理精度认证。

## 限制

当前支持平行XY的等厚薄层与中间四面体、完整底面固定温度、其他外界绝热；不支持局部固定温度/对流/辐射。薄层参考面须与中层共形。boundary_thermal.json 为平台预留，当前不创建/消费。

温度样例只验证 OP0 解析与映射；新案例需用户实际运行 ALPS，再使用案例包的导入脚本填写绑定。使用接口规范见 thermal/README.md。
