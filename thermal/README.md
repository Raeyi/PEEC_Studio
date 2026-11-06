# thermal 1.0.7 — DATA_LAYOUT 1.5

Windows x64原生C++17，VS2022 v143 /MT；无需Python、Gmsh DLL或VC++运行库安装。系统依赖KERNEL32/bcrypt。模块目录只含ThermalExtract.exe、TemperatureProcess.exe、README.md、ERROR_CODES.md、module.manifest.json。安装目录支持只读；案例与模块目录分离。

## 调用

```powershell
.\ThermalExtract.exe --data-root "D:\cases\demo1_4.1"
.\TemperatureProcess.exe --data-root "D:\cases\demo1_4.1"
```

extract只提取和导出网表，不运行ALPS，不生成OP0；post读取Model中的OP0并映射到原网格，不自动求解或补全缺失节点。

| 参数 | 默认值/职责 |
|---|---|
| --data-root | 必填，独立案例根目录 |
| --runtime-dir | Result/logs/thermal，或其子目录 |
| --mesh | Model/model_thermal.msh |
| --materials | Model/matdb.json |
| --source | Model/source_thermal.json，支持简化格式schemaVersion=3和原格式schemaVersion=2 |
| --boundary | Model/boundary_thermal.json |
| --simulation-settings | Setting/set_thermal.json，SPICE设置 |
| --op0 | 仅post，默认实际source所在目录/thermal.op0；显式值优先 |
| --help / --version | 单独使用，无需dataRoot |

--settings、--ports、--result-binding均已移除，传入报exit2。相对路径相对dataRoot；绝对路径也必须位于dataRoot内，禁止访问Result/tasks。父目录支持Unicode。互斥锁位于.locks/thermal.pipeline.lock，检查活动句柄，不因残留空闲锁文件拒绝运行。两个程序不写Model/Setting或安装目录。

退出码：0成功；2输入/配置错误；3文件IO错误；4数值校验失败；5运行环境错误；6同一算例热流水线被占用。具体错误码见ERROR_CODES.md。

## 输入格式

- 网格：一阶三角形/四面体，ASCII MSH4.1或2.2；不支持二进制、高阶、六面体/棱柱。4.1后处理节点标签上限10000000；2.2后处理要求连续标签。
- matdb.json：schemaVersion=3；按dimension+physical_material_id匹配材料。thermalConductivity、density、specificHeat为正数constant，单位分别为W/(m*K)、kg/m^3、J/(kg*K)。
- source_thermal.json：**schemaVersion=3**为简化格式，`sources`数组中每项为`{name,type:"total_power",volume_ids,power_W}`；`volume_ids`为Elementary Volume ID，`power_W`是所选体域合计功率，默认按总体积均匀分配。允许空数组（本次heater案例只指定温度边界）。网格坐标默认mm；可选`mesh_length_unit`为m/mm/um。可选`meshSha256`若提供则校验。体材料由网格Physical标签和matdb匹配；带Physical材料标签的表面按matdb厚度组装面内导热与热容。
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

Setting/set_thermal.json由平台/UI提供，格式保持1.0.5不变：
```json
{"schemaVersion":1,"analysis":"op","probe":["V(*)"]}
```
仅支持op；probe支持V(*)或V(Tn)，至少一个。写入两份网表的.op/.probe，.end由程序补入。全节点后处理保留V(*)；瞬态设置尚不支持。缺少文件不自动创建模板。模块包不携带默认件，案例中的设置只是回归快照。

OP0：标准位置Model/thermal.op0；默认跟随实际source所在目录，--op0优先。不读旧Result路径，不要求用户运行绑定JSON。读取映射所需的T节点温度，不补造缺失节点，不截断低温。

## 算法、输出和限制

schemaVersion=3：体域采用四面体FEM。带材料标签的二维表面按matdb指定厚度组装表面薄膜的面内导热和热容；体总功率由单元体积权重分配到节点；对流采用一致表面积分，导出与热矩阵等价的环境温度源和支路。schemaVersion=2保留原中层四面体加双温度迹薄层FEM算法（平行XY、等厚、参考面共节点）；原薄层四面体仅用于映射，不重复组装热阻。两种格式均未改为PEEC中心节点方法。

网表V=温度K、I=热功率W、R=K/W、C=J/K；必须保留带符号Gcorr，不能只留正电阻。独立DC/RC网表均按SPICE设置写.op，RC含集总热容。

- Result/thermal/extract：ThermalNetList.sp、ThermalNetList_RC.sp、thermal_nodes.csv、thermal_K.mtx、ThermalTemperatureMap.csv、extraction_report.json、thermal_binding.json、stage_receipt.json。
- Result/thermal/post：NodeTemperatureAtOperatingPoint.csv、VolumeTemperatureAtOperatingPoint.msh、MaximumTemperature.txt、TemperatureProcessReport.txt、post_binding.json、stage_receipt.json。
- Result/logs/thermal：日志；不生成thermal_extract_control.json，不搬动输入OP0。

thermal_binding内部schema升2，sourceSha256覆盖热源及内嵌提取数据；移除settingsSha256，保留simulationSettingsSha256及其他输入/输出指纹。post_binding内部schema升2，记录mapSha256而非referenceNetlistSha256。post不读取或校验.sp，也不判断OP0是否来自当前网表；仍检查提取输入和映射文件的一致性以及映射所需温度是否有效。更改输入应重新提取。只认退出码0，失败可能留下部分输出。

## 集成和验证

apiVersion=7，capabilities.dataLayout=peec-data/v1（官方Schema格式），dataLayoutRevision=1.5。manifest接受source schema2/3与boundary schema1/2；程序按成对版本解析（2+1或3+2）。标准post显式传Model/thermal.op0。覆盖source位置时注意同步显式op0参数。正式产物保留thermal/temperature.report。

独立案例thermal_cases_20261106/heater含Model/Setting/Result；与安装模块分开。源码在交付包source/，source/build.ps1可编译。案例README记录heater输入、OP0缺失节点的独立求解补全及验收数值。重跑会改变日志和结果文件，完整性校验应先执行。

本地回归：旧demo1_4.1提取五项主要产物和后处理节点温度CSV均与1.0.6逐字节一致。新heater案例6089节点，500K固定面608节点、293.15K固定面1961节点、源功率0W；后处理6089/6089节点完成映射，最高500K。独立体功率+对流测试回收1W，导出网表矩阵与thermal_K.mtx一致。未执行平台task_layer全链E2E，需集成方复测。
