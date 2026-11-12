# heater_reextract_20261109 热仿真算例

本算例使用 thermal 1.0.9（2026.11.12-native-1.0.9，apiVersion 8），基于 DATA_LAYOUT v1.7，并应用工作点动态获取（ALPS 联动）与长度单位约定。ALPS 集成（2026-11-12 起）：thermal_basic 流水线 alps 阶段（可选）经 ALPSRunner 1.1.2 动态获取工作点，平台导入步骤落位标准位置 `Result/thermal/alps/thermal.op0`；本算例随附离线预置工作点（离线/回归形态）。

| 路径 | 用途 |
| --- | --- |
| `Model/model_thermal.msh`、`Model/matdb.json` | Gmsh ASCII 4.1 网格和材料库 |
| `Model/thinfilm_thermal.json` | 面 3 为 HEATER 膜，面 9 为 SiO₂ 膜，均厚 0.0001 mm |
| `Model/boundary_thermal.json` | 面 3 定温 500 K，面 14 定温 293.15 K，其余外边界绝热 |
| `Model/source_thermal.json` | 热源列表，本算例为空 |
| `Setting/set_thermal.json` | SPICE 设置及可编辑的网格长度单位（1.0.9 可选 `mesh_length_unit`：m/mm/um） |
| `Result/thermal/alps/thermal.op0` | 本算例现有的 ALPS 稳态工作点（预置离线件；thermal 1.0.9 标准位置，post 内置 `--op0` 读取，无旧路径回退） |
| `Model/ThermalNetList.sp`（平台发布件）+ `Setting/ALPSRunner.json` + `Setting/alpsrunner.key` | ALPS 三件（随算例提交，用户确认 2026-11-12；网表由 extract 阶段库 `publishes` 落位） |
| `Result/thermal/extract/`、`Result/thermal/post/` | 参考网表与温度输出 |

## UI 集成

流水线为 3 段链（extract → alps（可选）→ post）：extract 必需，经阶段库 `publishes` 发布网表到 `Model/ThermalNetList.sp`（alps 固定输入）；alps 可选（ALPSRunner 1.1.2 经 SSH 远端仿真，ALPS 三件缺失时以 `QS-TL-SKIP-NOINPUT` 跳过、不阻断下游，exit 0 后平台导入步骤以 run_summary 8 字段门 + `SHA256SUMS.txt` 复核 + I7 三向新鲜度守卫把工作点落位 `Result/thermal/alps/thermal.op0`，同目录写审计件 `import_receipt.json`）；post 可选（requires 标准位置 OP0 + extract 网表，无旧路径回退；alps 缺位时用预置工作点）。算例附带预置工作点和参考输出，供回归验收。独立通道 `alps.importOp0` / CLI `alps-import-op0` 与流水线形态等价。

## 网格长度单位

`Setting/set_thermal.json` 里的 `mesh_length_unit` 支持 `m`、`mm`、`um`，分别将一个网格坐标单位解释为 1 m、1 mm、1 µm。本算例用 `mm`，网格厚度 0.001 mm，等于 1 µm。字段应与输入网格坐标单位一致。设置变更后重新提取；若物理尺寸改变，用新网表重新求解并更新 ALPS 工作点。薄膜 `thickness_m` 单独以米填写。

## 验收

提取为 6094 个热节点、4230 个薄膜三角形、2570 个定温节点。工作点的 6091 个已知温度被映射至全部 6094 个网格节点，范围 293.15–500 K。T65、T68、T3790 在 ALPS 工作点中缺值，温度后处理用最近的已知网格节点温度填补，来源和距离见 `Result/thermal/post/TemperatureProcessReport.txt`。原工作点字节保持不变。

薄膜、材料和边界字段见模块 README。
