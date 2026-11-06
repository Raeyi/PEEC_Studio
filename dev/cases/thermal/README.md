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

## heater 算例（2026-11-06，thermal 1.0.7，最简单算例）

- vendor 最简单算例样例（为支持 UI 侧最简单案例跑通）：自包含 dataRoot（Model + Setting），
  含预烘焙 Result（vendor 参考输出）。
- `Model/source_thermal.json` = **简化 schemaVersion 3**（sources 空数组、零热源、无 thermal 提取对象、
  无 meshFile 绑定）；`Model/boundary_thermal.json` = **schemaVersion 2**（default adiabatic；
  面 3 = 500 K（含内部 HEATER 薄膜面，材料/厚度按 mesh Physical 标签 + matdb.json 自动装配）、
  面 14 = 293.15 K、其余绝热）。
- `Model/thermal.op0` 为 vendor 经独立稀疏求解合成的完整 OP0（原用户 OP0 缺 T65/T68/T2681/T3806
  四节点温度；模块严格拒收缺失节点、不自动补全，vendor 随包提供补齐件，与 vendor 求解结果
  最大温差 0.00005 K）。
- 验收：extract 6089 节点 / 608 固定 @500 K / 1961 固定 @293.15 K / 总功率 0 W；post 6089/6089 映射，
  max 500 K、min 293.145 K（略低于 293.15 K，结果未截断）。
- 本算例分发含预烘焙 `Result/`（vendor 1.0.7 提交自带，作为 vendor 参考输出保留；与 demo1_4.1
  “分发件不含运行结果”的惯例不同）与 `.locks/thermal.pipeline.lock`（vendor 运行残留，0 B；
  模块锁为活跃句柄检查，空闲残留锁文件不阻断运行）。
- 注：算例 README 引用的 `validation/` 目录（原用户 OP0 + 补齐对照报告）未随分发包提供（vendor 遗漏），
  预烘焙 Result 与 Model OP0 已经直跑与平台全链复测接受。
- 经平台 thermal_basic 跑：网格复制到 `Result/gds2geo/thermal/model_thermal.msh`（标准路由；
  简化 schema 3 的 source 无需绑定 meshFile，故亦无需同步 meshFile）。
