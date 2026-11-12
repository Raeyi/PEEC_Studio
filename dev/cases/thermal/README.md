# thermal 案例包（2026-11-12 / v2.5.0_rc1 更新）

本目录是包外案例包，不能放进 thermal 模块安装目录。三个案例均为自包含 dataRoot（Model + Setting + 预烘焙 Result 参考输出），不引用案例外的输入。每案例验收指标见各案例 README。

## OP0 默认位置变更（thermal 1.0.9，2026-11-12）

- post 默认 op0 位置由 `Model/thermal.op0` 改为 **`Result/thermal/alps/thermal.op0`**（显式 --op0 可覆盖）；本目录三个案例的 OP0 已随本次提交迁移至新位置。
- ALPS 结果导入改由平台承担，三通道等价：task_layer RPC `alps.importOp0 {dataRoot, runDir}` / CLI `task_layer.exe alps-import-op0` / thermal_basic 流水线 alps 阶段内置导入（判定 = run 输出目录前后子目录 diff 恰一个 + run_summary 门 + SHA256SUMS 唯一 .op0 条目 + 三向新鲜度守卫：run_summary 输入 sha == 现 Model/ThermalNetList.sp sha == extract 绑定 sha；回执 `Result/thermal/alps/import_receipt.json`）。
- 旧用户脚本 **导入ALPS结果.ps1**（写 Model/thermal.op0 + thermal_run.json，1.0.6 时代契约）**已弃用**；**开始测试.ps1**（vendor CLI 手动提取便捷入口）同期弃用——平台路径统一为 `task.run {pipeline: "thermal_basic"}`。两脚本 v2.5.0_rc1 交付时移除。

## 平台集成（task_layer）

- **thermal_basic 三阶段（2026-11-12 重排）：extract →（可选）alps → post**。alps 阶段三输入件齐备（`Model/ThermalNetList.sp`——extract 1.0.9 发布的公开网表——+ `Setting/ALPSRunner.json` + `Setting/alpsrunner.key`，后两者为用户侧件）时执行 ALPS 远端仿真并导入 op0 至新默认位置；缺件跳过（QS-TL-SKIP-NOINPUT），post 用预置 op0（离线回归/参考输出形态即此）。
- 平台流程下热网格经 gds_full 落位 `Result/gds2geo/thermal_msh/model_thermal.msh`（不经过 Model/，保持 Model/ 单主网格不变式 I1）；
  此时 UI 写 `source_thermal.json` 的 `meshFile` 应指向该**落位路径** + SHA（模块要求 --mesh 路径与 meshFile 路径一致，
  字节 SHA 也须匹配）。本案例包为自包含形态（meshFile=Model/model_thermal.msh，可独立跑通 vendor CLI），
  经平台 thermal_basic 跑时先按上式把网格复制到落位路径并同步 meshFile（见 20261102 集成记录）。
- 平台实测（2026-11-02，1.0.6 时代）：thermal_basic 全链 2.4s；ThermalNetList.sp / ThermalNetList_RC.sp /
  extraction_report.json / NodeTemperatureAtOperatingPoint.csv 与 vendor 参考输出逐字节一致；
  MaximumTemperature 344.1037 K、节点 9564（demo1_4.1 基线，rc1 重验见 20261112 回归记录）。

## 案例

### demo1_4.1（20261104，完整回归）

schema2 source + 历史真实 OP0（未重跑 ALPS）。验收：networkNodes=12750、meshNodes=9564、
MaximumTemperature=344.1037 K、minimum=293.1271 K（详见案例 README）。OP0 已迁至
`Result/thermal/alps/thermal.op0`。

### heater（2026-11-06，thermal 1.0.7，最简单算例）

零热源 + 两个固定温度面（面 3 = 500 K、面 14 = 293.15 K、其余绝热）；source 为简化
schemaVersion 3（空 sources）、boundary schemaVersion 2。OP0 为 vendor 补齐件
（T65/T68/T2681/T3806 独立稀疏求解补值，与 vendor 求解最大温差 0.00005 K）。
验收：extract 6089 节点 / 608 @500 K / 1961 @293.15 K / 总功率 0 W；post 6089/6089 映射，
max 500 K、min 293.145 K。OP0 已迁至 `Result/thermal/alps/thermal.op0`。

### heater_reextract_20261109（thermal 1.0.9，本集成轮）

thermal 1.0.9 重提取 + 重跑 post 的 vendor 案例包（DATA_LAYOUT 工作点路径与长度单位约定）：
- 含 1.0.9 新增可选件 `Model/thinfilm_thermal.json`（面 3 HEATER 膜 / 面 9 SiO₂ 膜，均厚 0.0001 mm）；
- `Setting/set_thermal.json` 含 `mesh_length_unit`（本例 `mm`，网格厚度 0.001 mm = 1 µm；
  设置变更后须重新提取，物理尺寸改变须用新网表重新求解并更新工作点）；
- OP0 由 vendor 直接按新默认位置交付：`Result/thermal/alps/thermal.op0`。

验收：extract 6094 热节点 / 4230 薄膜三角形 / 2570 定温节点；post 6091 个已知温度映射至
全部 6094 个网格节点，范围 293.15–500 K；T65/T68/T3790 缺值由最近已知网格节点温度填补
（来源与距离见 `Result/thermal/post/TemperatureProcessReport.txt`）。

ALPS 集成形态：本算例**不带** `Setting/ALPSRunner.json` 与 `Setting/alpsrunner.key`
（用户侧件，不随案例包分发）。真机 E2E 时向 Setting/ 补齐两件套即走完整链
（extract → alps 执行 + 平台导入 → post）；离线回归直接走 pre-baked op0
（alps 缺件跳过，post 用现成 OP0）。
