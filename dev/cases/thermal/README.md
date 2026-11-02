# thermal 案例包 20261102

本目录是包外案例包，不能放进 thermal 模块安装目录。demo1_4.1 是完整 dataRoot（Model + Setting），含提取输入和历史 OP0 后处理样品，不引用案例外的输入。每案例验收指标见 demo1_4.1/README.md。

与 thermal 模块目录放在同一父目录后，执行“开始测试.ps1”提取；也可按案例 README 运行两个 EXE。输入与输出分开，分发件不含运行结果。

“导入ALPS结果.ps1”是用户/UI 侧工具，只有导入新求解结果时才运行；它核对实际求解网表哈希并写入 Model/thermal.op0 和 thermal_run.json，然后启动后处理。不要对旧 OP0 指定新的不对应网表。

## 平台侧说明（task_layer 集成）

- 平台流程下热网格经 gds_full 落位 `Result/gds2geo/thermal/model_thermal.msh`（不经过 Model/，保持 Model/ 单主网格不变式 I1）；
  此时 UI 写 `source_thermal.json` 的 `meshFile` 应指向该**落位路径** + SHA（模块要求 --mesh 路径与 meshFile 路径一致，
  字节 SHA 也须匹配）。本案例为自包含形态（meshFile=Model/model_thermal.msh，可独立跑通 vendor CLI），
  经平台 thermal_basic 跑时先按上式把网格复制到落位路径并同步 meshFile（见 20261102 集成记录）。
- 平台实测（2026-11-02）：thermal_basic 全链 2.4s；ThermalNetList.sp / ThermalNetList_RC.sp /
  extraction_report.json / NodeTemperatureAtOperatingPoint.csv 与 vendor 参考输出逐字节一致；
  MaximumTemperature 344.1037 K、节点 9564。
