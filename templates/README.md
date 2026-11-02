# templates/ — 算例初始值模板

新建算例（dataRoot）时的初始值来源。UI 按算例形态拷入对应文件到
<dataRoot>/ 下，跑 gds_full 生成网格后回填绑定值（SHA / 实体 ID）。
模块包不携带模板（四件套），模板归平台交付包。

## 文件清单

| 文件 | 时代 | 写者 | 说明 |
|---|---|---|---|
| Setting/set_em.json | v1.3 起 | UI | EM 求解设置（平铺字段，schemaVersion 1）；纯热算例不需要 |
| Setting/set_thermal.json | v1.3 起 | UI | 热求解设置（thermal 对象：四面体域 + 薄层）；纯 EM 算例不需要 |
| Setting/set.json | v1.2 时代 | UI | 合并件 = EM 字段 + thermal 对象（联合算例形态）；纯热算例只留 schemaVersion + thermal，不复制 EM 字段 |
| Model/source_em.json | 全时代 | UI | EM 源定义（meshFile/meshSha256 + 端口 triangleIds） |
| Model/source_thermal.json | 全时代 | UI | 热源定义（meshFile 指向落位路径 + SHA + sources）；不含边界条件 |
| Model/boundary_thermal.json | thermal 1.0.2 起 | UI | 热边界条件（default_external_boundary + fixed_temperature）；1.0.1 时代内嵌于 source_thermal.json |

（1.0.1 → 1.0.2 之间：boundary 字段仍写在 source_thermal.json 内；新包集成后按上表。）

## 使用流程

1. UI 建算例：放入 Model/model.gds + Model/model_info.csv（固定名）+ Setting/ 设置文件；
2. 跑 gds_full：网格/材料生成；task_layer publisher 落位 Model/model_em.msh + matdb.json
   （落位件由 task_layer 写，UI 不创建、不改写）；
3. UI 回填绑定值：
   - source_em.json：meshSha256 = 落位主网格 SHA-256；ports 的 triangleIds 在新网格上选取；
   - source_thermal.json：meshFile = Result/gds2geo/thermal/model_thermal.msh（落位路径），
     meshSha256 = 该文件 SHA-256；sources 的实体 ID 为新网格的 Elementary ID；
   - set_thermal.json（或 v1.2 时代 set.json 的 thermal 对象）：四面体域/薄层的
     elementary ID / reference_surfaces 按新网格选取（薄层参考面必须完整铺满）；
   - boundary_thermal.json：固定温度面 surface_entities 为新网格 Elementary ID；
4. 跑 peec_basic / thermal_basic（OP0 就绪后 post 才可跑）。

## 注意

- 示例数值（实体 ID / SHA / 端口 / 薄层参数）取自 demo1_4.1 与 compact_em standard
  两个真实案例，仅示范格式；新模型必须全部替换为实际值；
- 实体选择一律 Elementary ID（不是 Physical ID）；网格坐标 mm（DIM=1000）；
- 字段规范以 DATA_LAYOUT §六/§八 为准；模板与契约版本对应关系见包内交付说明。
