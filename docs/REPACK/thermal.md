# thermal 模块重打包请求日志（REPACK/thermal）

> 本文件为长期滚动日志：每次重打包请求一轮，最新一轮在最上面。每轮固定三段：**问题清单 → 交付要求 → 回执**（E2E 结果 / 偏差 / 收口状态），回执写回即该轮关闭。
> 关联活契约：`DATA_LAYOUT.md`（v1.5）/ `MODULE_MANIFEST_SPEC.md`；包内模块当前版本：**1.0.5（2026.11.02-native-1.0.5，apiVersion 6）**。

## 第 2 轮（2026-11-04 请求 → 待回执）

状态：**待回执** —— 2026-11-04 用户已与 thermal 侧确认以下布局结论；自 thermal 下一版
重发版起生效；现行包（1.0.5 时代）仍按 1.0.5 路径运行。DATA_LAYOUT 已升 **v1.5** 定形。

### 请求正文（2026-11-04，用户与 thermal 侧确认）

致 thermal 模块团队。以下三点为已确认结论，请在下一版重发版落地：

1. **OP0 默认路径 → `Model/` 目录**（`Model/thermal.op0`）：程序去 `source_thermal.json`
   所在目录（Model/）读取；1.0.5 时代的 `Result/thermal/post/thermal.op0`（暂存路径，
   1.0.5 README 中"临时路径待用户后续确定"所指）自下一版起不再使用；
2. **移除 `thermal_extract_control.json` 独立文件**：程序不再消费（1.0.5 暂存的
   `Result/thermal/thermal_extract_control.json` 位置问题的最终答案 = **移除该文件**）；
   提取控制数据（四面体域/薄层/材料关联）的载体请在下一版 manifest/README 明确声明——
   若移入 `source_thermal.json`，格式须写入发版说明（平台预检随 manifest consumed 走）；
3. **OP0 与 SPICE 网表暂不做校验**（1.0.1 时代绑定校验不恢复）；
4. **manifest 同步**（下一版）：consumed 移除 thermal_extract_control.json 条目；
   post 阶段声明 OP0 默认路径 = Model/thermal.op0；
5. **案例包同步**（如下一版仍提供 demo1_4.1 回归案例）：历史 OP0 位置迁至 Model/；
6. 设置默认件（set_thermal.json）归平台管理不变（包内无需携带；格式/默认值变更
   发版说明注明即可，2026-11-04 会议定）。

### 平台侧动作（下一版集成时执行，无需贵方处理）

- pipelines.json：thermal post 启动预检文件 → `Model/thermal.op0`；
- DATA_LAYOUT v1.5 + 交付说明修订行；templates/ 移除 thermal_extract_control.json 初值
  （过渡期保留 1.0.5 时代说明）；
- E2E 重跑（demo1_4.1 案例，OP0 迁 Model/ 后）验证全链 + 数值基线
  （344.1037 K / 9564 行 / 12750 节点）。

## 第 1 轮（2026-11-02 请求 → 2026-11-03 收口）

状态：**已收口** —— 重发版 1.0.5 已收到并集成（模块目录替换 + 新案例归档 dev/cases/thermal/demo1_4.1 + E2E 双通道全过；DATA_LAYOUT 升 v1.4 按 1.0.5 实况定形热侧）。

### 回执（2026-11-03）

- 交付：thermal_module_20261102_1.0.5.zip（模块四件套 5 文件）+ thermal_cases_20261102.zip
  （demo1_4.1 案例，含参考输出与历史 OP0）。
- 四项问题回执：① 设置作用域（EM 字段不再出现于热链设置文件）✓；② boundary_thermal.json
  独立 + --boundary + SHA 入内部绑定 ✓；③ TemperatureProcessReport.txt 正式声明为
  thermal/temperature.report ✓；④ 设置拆分——**按用户最新指示实施**，优先于本说明问题 4
  文案：Setting/set_thermal.json = SPICE 仿真设置（analysis/probe），thermal 对象移入
  Result/thermal/thermal_extract_control.json（暂存于此，拆分位置待定）；OP0 默认路径
  Result/thermal/post/thermal.op0（thermal_run.json 已于 1.0.3 取消）；manifest 标注
  dataLayoutRevision "1.2 revision 2"、apiVersion 6。
- 平台集成：仓库已换 1.0.5 模块目录 + 新案例归档（dev/cases/thermal/demo1_4.1）；
  pipelines.json post 段 requires 修正为 Result/thermal/post/thermal.op0；E2E 双通道
  全过（直接案例跑 10/10 产物与 vendor 参考逐字节一致；平台 pipeline 全链 2.6s，
  344.1037 K / 12750 节点 / 9564 行，网表 .op/.probe 注入正确）。
- DATA_LAYOUT 已升 v1.4 按 1.0.5 实况定形热侧（§八.1 含版本沿革）；本文件留档为
  重发请求历史。

### 请求正文（2026-11-02）

致 thermal 模块团队。平台对 2026-11-02 交付（ThermalDelivery_1.0，模块 1.0.1 +
案例包 thermal_cases_20261102）做了全链集成验证：模块包 5 文件 SHA 全过、
manifest 符合 v1.2、thermal_basic 平台 E2E 全链通过（2.4s）、4 个关键产物
（ThermalNetList.sp / ThermalNetList_RC.sp / extraction_report.json /
NodeTemperatureAtOperatingPoint.csv）与贵方参考输出**逐字节一致**、
MaximumTemperature 344.1037 K / 9564 节点。数值与稳定性没有问题。

但对照契约（DATA_LAYOUT 现已升 v1.3），发现**案例输入文件/设置共四处
需修正**，需要重打新的案例包（模块包 CLI/manifest 需相应改动，一并重打）：

### 问题 1：case 的 set.json 混入了 EM 配置字段

文件：thermal_cases_20261102/demo1_4.1/Setting/set.json

现状：除 thermal 对象外，还带了完整 EM 求解字段（Solver_SET / MNA_WEIGHT /
FS / FE / N_FP / DIM / Taylor / SAVE_BIN / SAVE_TXT / SAVE_SP /
POST_PROCESSING / POST_FREQ_INDEX / POST_PORT / POST_PLOT_J / POST_PLOT_QE /
POST_PLOT_VE / POST_FIELD）——demo1_4.1 是纯热算例，不涉及 EM 求解，
这些字段是 EM 模板复制残留。

docs_1.2 §八.1 作用域规则：
- EM 字段归 EM 链（compact_em 消费）；thermal 对象归热链（热模块消费）；
- **纯热算例：set.json 只写 schemaVersion + thermal 对象，不复制任何 EM 字段**；
- 联合算例（EM + 热同一 dataRoot）：同一 set.json 同时含 EM 字段与 thermal 对象。

修正：demo1_4.1 的 set.json 改为只含 schemaVersion + thermal（thermal 对象内容
不变，数值/实体 ID 保持原样）。

**注意（v1.3 联动，见问题 4）**：热链设置文件本身改名拆分为
`Setting/set_thermal.json`——即修正后的「只含 thermal 对象」文件直接以
set_thermal.json 交付，不再存在合并 set.json。

### 问题 2：边界条件内嵌在 source_thermal.json 里

文件：thermal_cases_20261102/demo1_4.1/Model/source_thermal.json

现状：default_external_boundary（"adiabatic"）与 fixed_temperature
（bottom_293p15K，surface_entities [18,22]，293.15 K）两个边界条件字段
内嵌在 source_thermal.json 中。

docs_1.2 §八.2 契约（本次新定义，v1.2 修订 2）：
- **热边界条件一律写入独立文件 Model/boundary_thermal.json，不得内嵌
  source_thermal.json**；
- source_thermal.json 只含源定义（meshFile / meshSha256 / total_power_W /
  sources / selection_id_type / mesh_length_unit / description 等）；
- boundary_thermal.json 格式（schemaVersion 1）：

```jsonc
{
  "schemaVersion": 1,
  "default_external_boundary": "adiabatic",
  "fixed_temperature": [ { "name": "bottom_293p15K",
    "location": "global_bottom_plane",
    "surface_entities": [ 18, 22 ],
    "temperature_K": 293.15 } ]
}
```

- boundary_em.json 是 EM 链的预留件（格式 TBD、暂无消费方），与
  boundary_thermal.json 互不包含内容；
- 热链算例必须提供 boundary_thermal.json（至少声明 default_external_boundary）；
- fixed_temperature.surface_entities 与 sources 实体选择同为 Elementary ID。

模块侧对应改动：
- CLI 增加 --boundary（默认 Model/boundary_thermal.json，路径规则同 --source）；
- manifest consumed 增加 boundary 工件（ref 如 thermal/boundary.def →
  Model/boundary_thermal.json）；extract 阶段同时读取 source + boundary 两文件；
- 平台 argsTemplate 将传 --boundary {artifact:thermal/boundary.def}
  （平台侧收到新 manifest 后同步 pipelines/manifest 声明即可）；
- ERROR_CODES 相应登记（boundary 文件缺失/格式错 → 对应既有码或新增）。

### 问题 3（平台侧补充）：produced 清单补全后处理报告

现状：post 阶段产物中 TemperatureProcessReport.txt（温度后处理报告）未列入
manifest produced，平台 task.result 的 resultFiles 因此不含它（文件在磁盘
Result/thermal/post/ 下，UI 可固定路径读取）。新包请将 `post` 阶段产物
`Result/thermal/post/TemperatureProcessReport.txt` 增补进 manifest produced
（ref 如 thermal/temperature.report）；如平台后续需要 thermal_nodes.csv /
thermal_K.mtx 进清单，也一并声明（当前按模块内部件处理，不强制）。

### 问题 4：求解设置拆分（DATA_LAYOUT v1.3，2026-11-02 升版）

平台契约 DATA_LAYOUT 已升 v1.3：`Setting/set.json` 按链路拆分为
`set_em.json`（EM 字段，compact_em 消费）与 `set_thermal.json`（热对象，
热模块消费）。热模块侧：

- **settings 文件改读 `Setting/set_thermal.json`**：manifest consumed 的
  `user/solver.set` 工件 path 由 `Setting/set.json` 改为 `Setting/set_thermal.json`
  （CLI `--settings` 由平台传解析后的路径，二进制不硬编码文件名）；
- **文件格式**：内容 = 原 set.json 的 `thermal` 对象部分，保留嵌套 `thermal`
  键与 `schemaVersion: 1`（即问题 1 修正后的文件形态；解析逻辑不变，仅文件来源变化）；
- **manifest capabilities**：`dataLayoutRevision` 升 `"1.3"`，`breakingChanges`
  增一条（settings 文件改读 set_thermal.json，set.json 不再消费）；
- 案例包 demo1_4.1 的 Setting/ 交付 `set_thermal.json`（无 set.json）；
- 数值回归基线不受影响（设置内容未变）。

### 交付要求（新包）

1. 新模块包（如版本升 1.0.2）：四件套 + 按上述 CLI/manifest 改动（含问题 4
   的 consumed 路径与 dataLayoutRevision）；包内仍只有四件套，无
   examples/templates/DATA；
2. 新案例包 thermal_cases_<新日期>/：demo1_4.1 按 `set_thermal.json` +
   source_thermal.json + boundary_thermal.json；历史 OP0 夹具
   （thermal.op0 / thermal_run.json）与 README 验收点保留；
3. 数值回归基线：修正后 extraction_report.json 关键数值应与当前版一致
   （networkNodes=12750、power=2000 W、capacity≈10.5897179444、
   positiveResistors=62134、signedCorrections=30711、
   fixedTemperatureSources=4452；MaximumTemperature≈344.1037 K）——
   字段搬家不改数值；若网表字节有差异请在交付说明中注明原因；
4. 验收记录（validation/）按新契约更新（verify.py 覆盖 boundary 文件校验）；
5. SHA256SUMS.txt 覆盖全部交付文件；
6. 交付说明中注明与 20261102 版的差异清单。

平台侧动作（无需贵方处理）：DATA_LAYOUT 已升 v1.3（§八 求解设置拆分、
§八.1 set_thermal.json 格式、目录树/所有权表/不变式联动）；平台读取由 manifest
consumed 驱动，无需代码改动；新包到位后平台重跑 thermal_basic 全链 E2E 并对照
回归基线。docs_1.3 契约包随本说明重发。

## 未决项（非重打包）

- ~~thermal_extract_control.json 存放位置待用户确认~~ → **已收口（2026-11-04）**：用户与 thermal 侧确认 = 下一版移除该文件 + OP0 迁 Model/ + 暂不做 OP0-网表校验（见第 2 轮；DATA_LAYOUT 升 v1.5）。
- **设置默认件归平台管理**（set_thermal.json）：SPICE 仿真设置默认值由平台维护（平台包 templates/Setting/set_thermal.json，平台负责最终内容并传给 UI）；模块包**无需携带 set_thermal.json**——下一版若改动设置格式/默认值，在发版说明注明即可（2026-11-04 会议定，DATA_LAYOUT v1.4 修订 1 ①）。
