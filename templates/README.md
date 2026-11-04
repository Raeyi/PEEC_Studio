# templates/ — 算例初始值模板

新建算例（dataRoot）时的初始值来源。UI 按算例形态拷入对应文件到
<dataRoot>/ 下，跑 gds_full 生成网格后回填绑定值（SHA / 实体 ID）。
模块包不携带模板（四件套），模板归平台交付包。

维护：本目录内容由**平台负责最终修改**——模块发版若改动设置格式/默认值，在发版说明中
注明即可，平台据此更新本目录文件并传给 UI；模块包无需携带 set_*.json（2026-11-04 定）。
切换信号：本目录出现新文件名的初值**不构成切换信号**——写哪个设置文件以平台通知
（交付说明修订行）为准。

## 文件清单

| 文件 | 时代 | 写者 | 说明 |
|---|---|---|---|
| Setting/set_em.json | v1.3 起 | UI | EM 求解设置（平铺字段，schemaVersion 1）；纯热算例不需要 |
| Setting/set_thermal.json | thermal 1.0.5 起 | UI | SPICE 仿真设置（analysis/probe；当前仅支持 op、瞬态未支持）；程序不覆写；纯 EM 算例不需要 |
| Result/thermal/thermal_extract_control.json | thermal 1.0.5 起 | UI | 提取控制（网格单位/四面体域/薄层/材料关联，即 thermal 对象）；暂存于此，后续拆分位置待定 |
| Setting/set.json | v1.2 时代 | UI | 合并件 = EM 字段 + thermal 对象（联合算例形态）；纯热算例只留 schemaVersion + thermal，不复制 EM 字段 |
| Model/source_em.json | 全时代 | UI | EM 源定义（meshFile/meshSha256 + 端口 triangleIds） |
| Model/source_thermal.json | 全时代 | UI | 热源定义（meshFile 指向落位路径 + SHA + sources）；不含边界条件 |
| Model/boundary_thermal.json | thermal 1.0.5 起 | UI | 热边界条件（default_external_boundary + fixed_temperature），**必需输入**（经 --boundary 传入，SHA 入内部绑定）；1.0.1 时代内嵌于 source_thermal.json |

（1.0.1 时代：boundary 字段内嵌于 source_thermal.json、无独立仿真设置文件；1.0.5 起按上表。1.0.5 的划分按用户最新指示实施，优先于 DATA_LAYOUT v1.3 上午的拆分文案。）

## 使用流程

1. UI 建算例：放入 Model/model.gds + Model/model_info.csv（固定名）+ Setting/ 设置文件；
2. 跑 gds_full：网格/材料生成；task_layer publisher 落位 Model/model_em.msh + matdb.json
   （落位件由 task_layer 写，UI 不创建、不改写）；
3. UI 回填绑定值：
   - source_em.json：meshSha256 = 落位主网格 SHA-256；ports 的 triangleIds 在新网格上选取；
   - source_thermal.json：meshFile = Result/gds2geo/thermal/model_thermal.msh（落位路径），
     meshSha256 = 该文件 SHA-256；sources 的实体 ID 为新网格的 Elementary ID；
   - thermal_extract_control.json：四面体域/薄层的 elementary ID / reference_surfaces 按新网格选取
     （薄层参考面必须完整铺满）；set_thermal.json 为仿真设置，默认 {"analysis":"op","probe":["V(*)"]}
     即可（完整温度可视化需保留 V(*)）；
   - boundary_thermal.json：固定温度面 surface_entities 为新网格 Elementary ID；
4. 跑 peec_basic / thermal_basic（OP0 就绪后 post 才可跑）。

## 注意

- 示例数值（实体 ID / SHA / 端口 / 薄层参数）取自 demo1_4.1 与 compact_em standard
  两个真实案例，仅示范格式；新模型必须全部替换为实际值；
- 实体选择一律 Elementary ID（不是 Physical ID）；网格坐标 mm（DIM=1000）；
- 字段规范以 DATA_LAYOUT §六/§八 为准；模板与契约版本对应关系见包内交付说明。
