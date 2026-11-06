# templates/ — 算例初始值模板

新建算例（dataRoot）时的初始值来源。UI 按算例形态拷入对应文件到
<dataRoot>/ 下，跑 gds_full 生成网格后回填绑定值（SHA / 实体 ID）。
模块包不携带模板（四件套），模板归平台交付包。

维护：本目录内容由**平台负责最终修改**——模块发版若改动设置格式/默认值，在发版说明中
注明即可，平台据此更新本目录文件并传给 UI；模块包无需携带 set_*.json（2026-11-04 定）。
切换信号：本目录出现新文件名的初值**不构成切换信号**——写哪个设置文件以平台通知
（交付说明修订行）为准。

热链 1.0.6 布局（2026-11-04 集成，DATA_LAYOUT v1.5 修订 1）：OP0 标准位置 =
`Model/thermal.op0`（程序去 source_thermal.json 所在目录读取）；thermal_extract_control.json
独立文件取消（提取控制内嵌于 source_thermal.json，schemaVersion 2）；OP0 与 SPICE 网表暂不做校验。

热链 1.0.7 布局（2026-11-06 集成，DATA_LAYOUT v1.5 修订 3）：source_thermal.json 新增**简化
schemaVersion 3**（仅 `sources`：volume_ids 上 total_power，允许空数组；体材料按 mesh Physical
标签 + matdb 自动装配，无需 thermal 提取对象）与原 schemaVersion 2 并存；boundary_thermal.json
新增 **schemaVersion 2**（default adiabatic + boundaries：fixed_temperature（任意面，含内部面）/
convection）与原 schemaVersion 1 并存；程序按成对版本解析（2+1 或 3+2，错配 → 模块 exit 2）。
**本目录 Model/ 初值 = 最简单算例标准（vendor 案例包 thermal_cases_20261106.zip 的 heater 案例，
2026-11-06 起）**：source_thermal.json 简化 schemaVersion 3（空 sources = 无热源）/ boundary_thermal.json
schemaVersion 2 / thermal.op0（OP0 初值，vendor 独立稀疏求解合成补齐件）；**matdb.json 不在本目录初值内——案例专属材料库：全流量算例由 gds_full 自动落位、最简单算例由案例自带（heater 案例包内含）**——
对应算例 dev/cases/thermal/heater（验收 6089 节点 / 608 @500 K / 1961 @293.15 K / 0 W；post 6089/6089、
max 500 K、min 293.145 K）。OP0 后处理仍严格校验映射节点温度齐备（不自动补全）；全流量/新案例
仍须经案例导入脚本导入**自己的** OP0（导入目标 Model/thermal.op0，覆写本目录初值）。

## 文件清单

| 文件 | 时代 | 写者 | 说明 |
|---|---|---|---|
| Setting/set_em.json | **现行**（compact_em v1.3 起生效，2026-11-05） | UI | EM 求解设置（平铺字段，schemaVersion 1；EM 链唯一 JSON 设置）；纯热算例不需要 |
| Setting/set_thermal.json | thermal 1.0.5 起（2026-11-06 与 vendor 案例包字节同步） | UI | SPICE 仿真设置（analysis/probe；当前仅支持 op、瞬态未支持）；程序不覆写；纯 EM 算例不需要 |
| ~~Result/thermal/thermal_extract_control.json~~ | ~~thermal 1.0.5 时代~~ | — | **已取消（thermal 1.0.6，2026-11-04 集成）**：提取控制（thermal 对象）改内嵌于 Model/source_thermal.json（schemaVersion 2），本目录不再提供该初值 |
| ~~Setting/set.json~~ | ~~v1.2 时代~~ | — | **已废止（2026-11-05，compact_em v1.3 集成）**：EM 链只读 set_em.json，合并件不再被任何模块读取；初值件已从本目录删除 |
| Model/source_em.json | 全时代 | UI | EM 源定义（meshFile/meshSha256 + 端口 triangleIds） |
| Model/source_thermal.json | 全时代（**初值 = 简化 schemaVersion 3，2026-11-06 vendor 标准**） | UI | 3 = 仅 sources（volume_ids 上 total_power，允许空数组 = 无热源；体材料按 mesh Physical 标签 + matdb.json 自动装配；无 thermal 提取对象、无 meshFile 绑定）；2 = 热源定义 + **thermal 提取对象**（四面体域/薄层/材料关联）+ meshFile（指向落位路径）+ SHA——全流量算例用，初值件已不再随包（格式见仓库 dev/cases/thermal/demo1_4.1 与 git 历史）；不含边界条件 |
| Model/boundary_thermal.json | thermal 1.0.5 起（**初值 = schemaVersion 2，2026-11-06 vendor 标准**） | UI | 热边界条件，**必需输入**（经 --boundary 传入，SHA 入内部绑定）；schemaVersion 2 = default adiabatic + boundaries（fixed_temperature（任意面，含内部面；初值为 heater 面 3=500 K / 14=293.15 K）/ convection（h_W_m2K + ambient_temperature_K））；原 schemaVersion 1（default_external_boundary）仍可用；1.0.1 时代内嵌于 source_thermal.json |
| Model/thermal.op0 | **初值新设（2026-11-06，vendor 标准）** | UI / 案例导入脚本 | OP0 初值（vendor 独立稀疏求解合成补齐的完整件，heater 标准；post 严格校验映射节点温度齐备，不自动补全）；全流量/新案例必须导入自己的 OP0（导入目标 = 本文件，覆写初值）；1.0.6 时代 OP0 为纯算例产物、本目录不提供初值，2026-11-06 起重切包起提供 |

（1.0.1 时代：boundary 字段内嵌于 source_thermal.json、无独立仿真设置文件；1.0.5 起按上表；1.0.6 起提取控制内嵌 source_thermal.json（schemaVersion 2），上表控制件行取消。1.0.5 的划分按用户最新指示实施，优先于 DATA_LAYOUT v1.3 上午的拆分文案。1.0.7 起 source 新增简化 schemaVersion 3（无 thermal 提取对象）、boundary 新增 schemaVersion 2（default adiabatic + boundaries，含内部面/对流）；程序按成对版本解析（2+1 或 3+2，错配 → 模块 exit 2）；最简单算例 = dev/cases/thermal/heater，**本目录 Model/ 初值即该标准（2026-11-06 起，vendor 案例包为准）**。）

## 使用流程

**最简单算例（thermal 1.0.7 标准，无 GDS 流程）——初值即本目录 Model/（heater 标准）**：
1. UI 建算例：拷本目录 Model/ 3 件（source_thermal.json / boundary_thermal.json /
   thermal.op0）+ Setting/set_thermal.json + 案例自带 matdb.json（案例专属件，如 heater 案例包）到 <dataRoot>/；
2. 将算例的热网格（model_thermal.msh）放到 `Result/gds2geo/thermal/model_thermal.msh`（标准路由；
   schema 3 无 meshFile 绑定，无需回填 meshFile/meshSha256）；
3. 若边界面 ID（初值 = heater 面 3/14）或材料与算例网格不一致，改 boundary_thermal.json 的
   surface_ids / 案例 matdb.json（初值仅示范格式，新模型须换实际值）；
4. 跑 thermal_basic（2 阶段 extract → post；OP0 已随初值就绪，post 直接可跑）。
   验收（heater 标准）：6089 节点 / 0 W / post 6089 数据行、max 500 K、min 293.145 K、
   MaximumTemperature.txt = 500。

**全流量算例（带 GDS 流程）**：
1. UI 建算例：放入 Model/model.gds + Model/model_info.csv（固定名）+ Setting/ 设置文件 +
   Model/source_thermal.json（**schemaVersion 2**，全流量初值件不随包，格式见仓库
   dev/cases/thermal/demo1_4.1）+ Model/boundary_thermal.json（本目录初值可用）；
2. 跑 gds_full：网格/材料生成；task_layer publisher 落位 Model/model_em.msh + matdb.json
   （落位件由 task_layer 写，UI 不创建、不改写）；
3. UI 回填绑定值：
   - source_em.json：meshSha256 = 落位主网格 SHA-256；ports 的 triangleIds 在新网格上选取；
   - source_thermal.json（schemaVersion 2）：meshFile = Result/gds2geo/thermal/model_thermal.msh
     （落位路径），meshSha256 = 该文件 SHA-256；sources 的实体 ID 为新网格的 Elementary ID；
   - source_thermal.json 的 thermal 提取对象：四面体域/薄层的 elementary ID / reference_surfaces
     按新网格选取（薄层参考面必须完整铺满，material_footprints 必须完整覆盖
     reference_surfaces）；
   - set_thermal.json 为仿真设置，默认 {"analysis":"op","probe":["V(*)"]} 即可
     （完整温度可视化需保留 V(*)）；
   - boundary_thermal.json：固定温度面 surface_entities 为新网格 Elementary ID（原 schemaVersion 1；
     1.0.7 起 schemaVersion 2：boundaries[].surface_ids 可为任意现有面（含内部面），另有对流边界）；
   - thermal.op0：用户实际运行 ALPS 后经案例导入脚本导入（覆写本目录初值；post 才可跑）；
4. 跑 peec_basic / thermal_basic（OP0 就绪后 post 才可跑）。

## 注意

- 初值的面 ID（3/14）/ SHA / 端口 / 薄层参数等为 heater 标准（最简单算例）或取自 demo1_4.1 与
  compact_em standard 两个真实案例，仅示范格式；新模型必须全部替换为实际值；
- 实体选择一律 Elementary ID（不是 Physical ID）；网格坐标 mm（DIM=1000）；
- 字段规范以 DATA_LAYOUT §六/§八 为准；模板与契约版本对应关系见包内交付说明。
