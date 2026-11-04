# demo1_4.1 / 20261104

完整回归案例：Model包含model_thermal.msh、matdb.json、source_thermal.json(schema2)、boundary_thermal.json、thermal.op0。Setting包含该案例的set_thermal.json仿真设置快照；平台默认件仍由平台管理。

source中的thermal是原控制数据原样迁移，不再有独立控制件。上薄层Volume2参考面3/10，下薄层Volume3参考面16，中层Volume4；材料脚印上3铜/10硅，下22铜/18硅；薄层厚0.5μm。铜k400、ρ8960、cp385，硅k130、ρ2330、cp700；每层金属1000W，总2000W。底面18/22固定293.15K，其余绝热。实体ID均为Elementary。

在本案例目录运行：
```powershell
& ..\..\thermal\ThermalExtract.exe --data-root $PWD.Path
& ..\..\thermal\TemperatureProcess.exe --data-root $PWD.Path
```
第二条默认读取Model/thermal.op0，不需要--op0。独立真实仿真可用ALPS运行Result/thermal/extract/ThermalNetList.sp，再用案例包的导入ALPS结果.ps1把结果导入Model/thermal.op0。脚本导入是用户操作，EXE自身只读输入。

验收：networkNodes=12750，meshNodes=9564，power_W≈2000，capacity_J_per_K≈10.5897179444；positiveResistors=62134，signedCorrections=30711，fixedTemperatureSources=4452。节点温度CSV有9564行数据，MaximumTemperature=344.1037K，minimum=293.1271K。VolumeTemperatureAtOperatingPoint.msh在原网格追加K/℃NodeData，可由Gmsh打开。

OP0为原模型历史真实求解结果，供回归，未重新运行ALPS。改变模型/材料/源/边界后需新求解。后处理不检验OP0与网表来源，但要求映射所需温度有效。Result含本版本实际生成的提取/后处理结果。默认网表、矩阵、映射、提取报告与节点温度CSV与基线逐字节一致。

适用限制：平行XY等厚薄层、共形计算界面、完整底面固定温度；不支持局部固定温度/对流/辐射。其余接口见模块README与总交付说明。
