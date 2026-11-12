# thermal 1.0.9 — DATA_LAYOUT 1.5（本次交付调整）

Windows x64原生C++17，VS2022 v143 /MT；无需Python、Gmsh DLL或VC++运行库安装。系统依赖KERNEL32/bcrypt。模块目录只含ThermalExtract.exe、TemperatureProcess.exe、README.md、ERROR_CODES.md、module.manifest.json。安装目录支持只读；案例与模块目录分离。

## 调用

```powershell
.\ThermalExtract.exe --data-root "D:\cases\demo1_4.1"
.\TemperatureProcess.exe --data-root "D:\cases\demo1_4.1"
```

extract提取并导出网表。ALPS负责求解网表。post读取 `Result/thermal/alps/thermal.op0` 并映射到原网格。OP0缺少映射所需的T节点温度时，post从当前网格里选最近的、具有完整已知温度的节点复制温度。

| 参数 | 默认值/职责 |
|---|---|
| --data-root | 必填，独立案例根目录 |
| --runtime-dir | Result/logs/thermal，或其子目录 |
| --mesh | Model/model_thermal.msh |
| --materials | Model/matdb.json |
| --source | Model/source_thermal.json，支持简化格式schemaVersion=3和原格式schemaVersion=2 |
| --boundary | Model/boundary_thermal.json |
| --simulation-settings | Setting/set_thermal.json，SPICE设置和网格长度单位 |
| --op0 | 仅post，默认 Result/thermal/alps/thermal.op0；显式值优先 |
| --help / --version | 单独使用，无需dataRoot |

--settings、--ports、--result-binding均已移除，传入报exit2。相对路径相对dataRoot；绝对路径也必须位于dataRoot内，禁止访问Result/tasks。父目录支持Unicode。互斥锁位于.locks/thermal.pipeline.lock，检查活动句柄，不因残留空闲锁文件拒绝运行。两个程序不写Model/Setting或安装目录。

退出码：0成功；2输入/配置错误；3文件IO错误；4数值校验失败；5运行环境错误；6同一算例热流水线被占用。具体错误码见ERROR_CODES.md。

## 输入格式

- 网格：一阶三角形/四面体，ASCII MSH4.1或2.2；不支持二进制、高阶、六面体/棱柱。4.1后处理节点标签上限10000000；2.2后处理要求连续标签。
- matdb.json：schemaVersion=3；按dimension+physical_material_id匹配材料。thermalConductivity、density、specificHeat为正数constant，单位分别为W/(m*K)、kg/m^3、J/(kg*K)。
- source_thermal.json：**schemaVersion=3**为简化格式，`sources`数组中每项为`{name,type:"total_power",volume_ids,power_W}`；`volume_ids`为Elementary Volume ID，`power_W`是所选体域合计功率，默认按总体积均匀分配。允许空数组（本次heater案例只指定温度边界）。网格单位读取 `Setting/set_thermal.json`。可选`meshSha256`若提供则校验。体材料由网格Physical标签和matdb匹配；带Physical材料标签的表面按matdb厚度组装面内导热与热容。
- 兼容原source_thermal.json **schemaVersion=2**：保留原sources、必需thermal对象、meshFile/meshSha256及以下旧格式规则。两种格式均不得内嵌边界条件。

thermal对象结构：

| 字段 | 内容 |
|---|---|
| name | 案例标识 |
| selection_id_type | elementary_entity |
| mesh_length_scale_to_m | 网格坐标转m比例，mm=0.001 |
| tetrahedral_domains[] | elementary_volume及material{dimension,physical_material_id} |
| thin_layers[] | name、visualization_volume、reference_surfaces、reference_position(top/bottom)、thickness_m、material_footprints[] |
| material_footprints[] | surfaces实体数组及material{dimension,physical_material_id} |

即旧控制件 `{schemaVersion:1, thermal:{...}}` 中的thermal对象移入source顶层，旧source的schemaVersion改2；不把控制件再嵌套为独立文件路径。热源sources仍支持thin_layer_total_power、uniform_over_selected_footprint和平面均匀/厚度均匀分布。完整可运行JSON随独立案例的Model/source_thermal.json提供。

boundary_thermal.json：新**schemaVersion=2**使用`default:"adiabatic"`和`boundaries`数组。每项`name`可自由命名；`type:"fixed_temperature"`需要`surface_ids`与`temperature_K`，可选任意存在的Elementary Surface，包括内部面；`type:"convection"`需要`surface_ids`、正数`h_W_m2K`和`ambient_temperature_K`。重复面或共享节点上的冲突固定温度会报错。未指定边界绝热；至少有一处固定温度或对流以锚定稳态网络。示例见独立heater案例。旧schemaVersion=1仍支持完整全局底面固定温度：
```json
{"schemaVersion":1,"default_external_boundary":"adiabatic","fixed_temperature":[{"name":"bottom","location":"global_bottom_plane","surface_entities":[18,22],"temperature_K":293.15}]}
```

Setting/set_thermal.json由平台/UI提供，包含可选的网格长度单位。
```json
{"schemaVersion":1,"analysis":"op","probe":["V(*)"],"mesh_length_unit":"mm"}
```
仅支持op；probe支持V(*)或V(Tn)，至少一个。写入两份网表的.op/.probe，.end由程序补入。全节点后处理保留V(*)；瞬态设置尚不支持。缺少文件不自动创建模板。模块包不携带默认件，案例中的设置只是回归快照。

`mesh_length_unit` 可填 `m`、`mm` 或 `um`，分别对应每个网格坐标单位为 1、10⁻³ 或 10⁻⁶ m。提取和后处理使用同一值。字段缺省时读取旧的 source 单位或比例，简化格式无单位时按 mm。薄膜的 `thickness_m` 始终以米填写。变更设置后重新提取，若物理尺度变更则重新用 ALPS 求解。

OP0 的标准位置是 `Result/thermal/alps/thermal.op0`。旧算例可以通过 `--op0` 显式传入原位置。缺少T节点时，post以欧氏距离选最近的完整已知温度网格节点。候选节点只来自原始OP0，等距时选较小节点ID。输入OP0不修改，CSV的value_source标为nearest_mesh_node，报告记录缺失T节点、来源和距离。若无任何已知温度可用或映射无效，程序报错。

## 算法、输出和限制

schemaVersion=3：体域采用四面体FEM。可选 `Model/thinfilm_thermal.json` 形如 `{"films":[{"name":"thinfilm1","surface_ids":[3],"material_id":3,"thickness_m":1e-7}]}`。几何面使用 Elementary ID，材料引用 matdb 中的 surface 材料。膜三角形必须与相邻四面体共面、共节点，面内导热和热容装配到共享温度节点。文件存在时由其清单决定膜面，空数组表示无膜。缺省时沿用网格的面 Physical 材料标签和 matdb 厚度。体热源按体积权重分配，对流按面积积分。schemaVersion=2 保留原双温度迹薄层FEM。

网表V=温度K、I=热功率W、R=K/W、C=J/K；必须保留带符号Gcorr，不能只留正电阻。独立DC/RC网表均按SPICE设置写.op，RC含集总热容。

- Result/thermal/extract：ThermalNetList.sp、ThermalNetList_RC.sp、thermal_nodes.csv、thermal_K.mtx、ThermalTemperatureMap.csv、extraction_report.json、thermal_binding.json、stage_receipt.json。
- Result/thermal/alps：平台或用户保存的 ALPS 工作点 `thermal.op0`，供 post 读取。
- Result/thermal/post：NodeTemperatureAtOperatingPoint.csv、VolumeTemperatureAtOperatingPoint.msh、MaximumTemperature.txt、TemperatureProcessReport.txt、post_binding.json、stage_receipt.json。
- Result/logs/thermal：日志；不生成thermal_extract_control.json，不搬动输入OP0。

thermal_binding内部schema升2，sourceSha256覆盖热源及内嵌提取数据；移除settingsSha256，保留simulationSettingsSha256及其他输入/输出指纹。post_binding内部schema升2，记录mapSha256而非referenceNetlistSha256。post不读取或校验.sp，也不判断OP0是否来自当前网表；仍检查提取输入和映射文件的一致性。更改输入应重新提取。只认退出码0，失败可能留下部分输出。

## 集成和验证

apiVersion=8，capabilities.dataLayout=peec-data/v1，dataLayoutRevision=1.5。manifest 接受 source schema2/3 与 boundary schema1/2，按 2+1 或 3+2 配套使用。post 阶段传 `Result/thermal/alps/thermal.op0`，平台的 post 启动预检和 ALPS 结果导入应使用同一路径。正式产物保留 thermal/temperature.report。

参考算例单独交付。验收基准为 6094 个热节点、4230 个薄膜三角形、293.15–500 K。

本地回归：旧demo1_4.1提取五项主要产物和后处理节点温度CSV均与1.0.6逐字节一致。新heater案例6089节点，500K固定面608节点、293.15K固定面1961节点、源功率0W；后处理6089/6089节点完成映射，最高500K。独立体功率+对流测试回收1W，导出网表矩阵与thermal_K.mtx一致。未执行平台task_layer全链E2E，需集成方复测。
