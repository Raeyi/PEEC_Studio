# thermal 1.0.6 — DATA_LAYOUT 1.5

Windows x64原生C++17，VS2022 v143 /MT；无需Python、Gmsh DLL或VC++运行库安装。系统依赖KERNEL32/bcrypt。模块目录只含ThermalExtract.exe、TemperatureProcess.exe、README.md、ERROR_CODES.md、module.manifest.json。安装目录支持只读；案例与模块目录分离。

## 调用

```powershell
.\ThermalExtract.exe --data-root "D:\cases\demo1_4.1"
.\TemperatureProcess.exe --data-root "D:\cases\demo1_4.1"
```

extract只提取和导出网表，不运行ALPS，不生成OP0；post读取外部求解得到的真实OP0并映射到原网格。

| 参数 | 默认值/职责 |
|---|---|
| --data-root | 必填，独立案例根目录 |
| --runtime-dir | Result/logs/thermal，或其子目录 |
| --mesh | Model/model_thermal.msh |
| --materials | Model/matdb.json |
| --source | Model/source_thermal.json，schemaVersion=2，含热源和thermal提取数据 |
| --boundary | Model/boundary_thermal.json |
| --simulation-settings | Setting/set_thermal.json，SPICE设置 |
| --op0 | 仅post，默认实际source所在目录/thermal.op0；显式值优先 |
| --help / --version | 单独使用，无需dataRoot |

--settings、--ports、--result-binding均已移除，传入报exit2。相对路径相对dataRoot；绝对路径也必须位于dataRoot内，禁止访问Result/tasks。父目录支持Unicode。互斥锁位于.locks/thermal.pipeline.lock，检查活动句柄，不因残留空闲锁文件拒绝运行。两个程序不写Model/Setting或安装目录。

## 输入格式

- 网格：一阶三角形/四面体，ASCII MSH4.1或2.2；不支持二进制、高阶、六面体/棱柱。4.1后处理节点标签上限10000000；2.2后处理要求连续标签。
- matdb.json：schemaVersion=3；按dimension+physical_material_id匹配材料。thermalConductivity、density、specificHeat为正数constant，单位分别为W/(m*K)、kg/m^3、J/(kg*K)。
- source_thermal.json：**schemaVersion=2**，保留原sources及其字段，新增必需thermal对象。顶层meshFile按dataRoot解析，meshSha256为该网格字节SHA256。实体选择均为Elementary ID。不得内嵌边界字段。

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

boundary_thermal.json：schemaVersion=1，default_external_boundary=adiabatic，fixed_temperature数组。当前支持global_bottom_plane固定温度，含name/location/surface_entities/temperature_K；必须至少一组。示例：
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

中层为四面体FEM，上下薄层为双温度迹薄层FEM。支持平行XY等厚薄层，参考面与中层界面共用节点；材料投影完整覆盖参考面。原薄层四面体只用于温度映射，不重复组装热阻。未改为PEEC中心节点方法。

网表V=温度K、I=热功率W、R=K/W、C=J/K；必须保留带符号Gcorr，不能只留正电阻。独立DC/RC网表均按SPICE设置写.op，RC含集总热容。

- Result/thermal/extract：ThermalNetList.sp、ThermalNetList_RC.sp、thermal_nodes.csv、thermal_K.mtx、ThermalTemperatureMap.csv、extraction_report.json、thermal_binding.json、stage_receipt.json。
- Result/thermal/post：NodeTemperatureAtOperatingPoint.csv、VolumeTemperatureAtOperatingPoint.msh、MaximumTemperature.txt、TemperatureProcessReport.txt、post_binding.json、stage_receipt.json。
- Result/logs/thermal：日志；不生成thermal_extract_control.json，不搬动输入OP0。

thermal_binding内部schema升2，sourceSha256覆盖热源及内嵌提取数据；移除settingsSha256，保留simulationSettingsSha256及其他输入/输出指纹。post_binding内部schema升2，记录mapSha256而非referenceNetlistSha256。post不读取或校验.sp，也不判断OP0是否来自当前网表；仍检查提取输入和映射文件的一致性以及映射所需温度是否有效。更改输入应重新提取。只认退出码0，失败可能留下部分输出。

## 集成和验证

apiVersion=7，capabilities.dataLayout=peec-data/v1（官方Schema格式），dataLayoutRevision=1.5。manifest移除user/thermal.extract_settings与--settings；thermal/source.def要求schema2和thermal；标准post显式传Model/thermal.op0。覆盖source位置时注意同步显式op0参数。正式产物保留thermal/temperature.report。

独立案例thermal_cases_20261104含Model/Setting/Result与历史OP0；与安装模块分开。源码在交付包source/，source/build.ps1可编译。验证：py -3.12 -X utf8 validation/verify.py（仅开发验证需Python/jsonschema），validation/verify_readonly.ps1验证安装目录只读；verify_sha256.py校验原交付。重跑会改变日志，完整性校验应先执行。

本地回归：12750热节点、9564原节点、2000W、总热容10.5897179444J/K、最高344.1037K、最低293.1271K。保留历史结果的轻微低温，不人为截断；未重新运行ALPS或验证物理收敛。平台1.0.5的E2E通过不代表新版平台E2E通过，需集成方复测。
