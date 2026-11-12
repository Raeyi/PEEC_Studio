# heater_reextract_20261109 热仿真算例

本算例使用 thermal 1.0.9，基于 DATA_LAYOUT 1.5，并应用本次交付的工作点路径和长度单位约定。

| 路径 | 用途 |
| --- | --- |
| `Model/model_thermal.msh`、`Model/matdb.json` | Gmsh ASCII 4.1 网格和材料库 |
| `Model/thinfilm_thermal.json` | 面 3 为 HEATER 膜，面 9 为 SiO₂ 膜，均厚 0.0001 mm |
| `Model/boundary_thermal.json` | 面 3 定温 500 K，面 14 定温 293.15 K，其余外边界绝热 |
| `Model/source_thermal.json` | 热源列表，本算例为空 |
| `Setting/set_thermal.json` | SPICE 设置及可编辑的网格长度单位 |
| `Result/thermal/alps/thermal.op0` | 本算例现有的 ALPS 稳态工作点 |
| `Result/thermal/extract/`、`Result/thermal/post/` | 参考网表与温度输出 |

## UI 集成

UI 按模块 manifest 调用 extract 和 post。ALPS 求解 `Result/thermal/extract/ThermalNetList.sp`，将工作点保存到 `Result/thermal/alps/thermal.op0` 后调用 post。算例附带工作点和参考输出，供回归验收。

## 网格长度单位

`Setting/set_thermal.json` 里的 `mesh_length_unit` 支持 `m`、`mm`、`um`，分别将一个网格坐标单位解释为 1 m、1 mm、1 µm。本算例用 `mm`，网格厚度 0.001 mm，等于 1 µm。字段应与输入网格坐标单位一致。设置变更后重新提取；若物理尺寸改变，用新网表重新求解并更新 ALPS 工作点。薄膜 `thickness_m` 单独以米填写。

## 验收

提取为 6094 个热节点、4230 个薄膜三角形、2570 个定温节点。工作点的 6091 个已知温度被映射至全部 6094 个网格节点，范围 293.15–500 K。T65、T68、T3790 在 ALPS 工作点中缺值，温度后处理用最近的已知网格节点温度填补，来源和距离见 `Result/thermal/post/TemperatureProcessReport.txt`。原工作点字节保持不变。

薄膜、材料和边界字段见模块 README。
