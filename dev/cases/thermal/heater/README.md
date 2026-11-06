# heater 完整热算例

输入来自 `heater/heater` 的原始 `model_thermal.msh`、`matdb.json`，以及 `INPUT` 样例定义的简化热源/边界JSON。网格坐标按mm处理。Elementary Surface 3为内部HEATER面，固定500K；Surface 14为顶部面，固定293.15K；其余未指定外表面绝热。没有功率源。Surface 3和9的材料与厚度由matdb提供，作为表面薄膜组装。

在本目录运行（模块与案例在同一交付总目录）：

```powershell
& ..\..\thermal\ThermalExtract.exe --data-root $PWD.Path
& ..\..\thermal\TemperatureProcess.exe --data-root $PWD.Path
```

第一步生成`Result/thermal/extract/ThermalNetList.sp`、RC网表、热矩阵、映射表和报告；第二步默认读取`Model/thermal.op0`，生成`Result/thermal/post/`中的节点温度CSV、带NodeData的MSH、最高温度和报告。两个程序均不调用ALPS，也不改写Model/Setting。

## OP0来源

用户给出的原始`ThermalNetList.sp.op0`保存在交付总目录`validation/original_user_ThermalNetList.sp.op0`。它包含6085/6089个T节点温度，缺少T65、T68、T2681、T3806。按本案例提取的`thermal_K.mtx`和网表固定温度条件，用独立稀疏线性求解得到四个缺失值；其余6085个用户提供的温度数值保留。合成的完整工作点放在`Model/thermal.op0`，具体数值、原件与完整件SHA256见`validation/op0_completion_report.json`。原件与独立求解的对应节点最大差0.00005K，符合原OP0四位小数精度。后处理程序本身仍严格拒绝缺失节点，不自动补值，也不校验OP0是否由当前网表生成。

## 验收点

- 提取：6089个热节点；固定500K节点608个，固定293.15K节点1961个；总功率0W。
- 后处理：6089个网格节点全部映射；固定温度节点值与输入相符；最低293.145K、最高500K。最低值略低于293.15K，结果未被截断。
- 改变网格、材料、热源、边界或仿真设置后须重新提取并重新求解；原OP0不保证仍适用。
