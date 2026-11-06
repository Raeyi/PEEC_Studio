# thermal 模块重打包请求日志（REPACK/thermal）

> 本文件为长期滚动日志：每次重打包请求一轮，最新一轮在最上面。每轮固定三段：**问题清单 → 交付要求 → 回执**（E2E 结果 / 偏差 / 收口状态），回执写回即该轮关闭。
> 关联活契约：`DATA_LAYOUT.md`（v1.5 修订 1）/ `MODULE_MANIFEST_SPEC.md`；包内模块当前版本：**1.0.6（2026.11.04-native-1.0.6，apiVersion 7）**。

## 第 3 轮（2026-11-06 请求 → 回执同日）

状态：**已收口（2026-11-06 同日）** —— 1.0.7 已收到并集成（模块目录替换 + 文档/配置值级同步 + 直跑/平台 E2E 三通道复测通过；DATA_LAYOUT v1.5 修订 3 记录 1.0.7 输入格式接受；v2.1 交付包重切）。

### 请求正文（2026-11-06，用户与 thermal 侧）

致 thermal 模块团队。本轮请求为**能力扩展**：为支持 UI 侧**最简单算例**跑通，输入契约简化如下（新旧格式并存）：
- `Model/source_thermal.json` 新增 **schemaVersion 3（简化）**：仅 `sources`（`{name, type:"total_power", volume_ids, power_W}`，空数组 = 无热源允许）；体材料按 mesh Physical 标签 + matdb.json 自动装配（带材料标签的表面按 matdb 厚度装配面内导热 + 热容 film）；mesh 坐标默认 mm，`mesh_length_unit`（m/mm/um）可选；无需 `thermal` 提取对象；
- `Model/boundary_thermal.json` 新增 **schemaVersion 2**：`default:"adiabatic"` + `boundaries`（fixed_temperature：surface_ids 为任意现有 Elementary Surface（含内部面）；convection：h_W_m2K>0 + ambient_temperature_K）；
- 成对版本解析：source 2 ↔ boundary 1、source 3 ↔ boundary 2，错配 → exit 2；
- OP0 后处理仍严格校验映射节点温度齐备（不自动补全）。

随包材料：**heater 最简单算例**（`dev/cases/thermal/heater`）：零热源样例（schema 3 空 sources + schema 2 两个固定温度面 500 K / 293.15 K，面 3 含内部 HEATER 薄膜面）；OP0 为 vendor 独立稀疏求解合成补齐（原用户 OP0 缺 T65/T68/T2681/T3806 四节点温度，最大温差 0.00005 K）。

### 交付要求

1. 1.0.7 模块包（双 exe + manifest + README/ERROR_CODES）；
2. demo1_4.1（1.0.6 既有算例）在 1.0.7 下的字节级兼容承诺（vendor 回归：五项主要提取产物 + 节点温度 CSV 与 1.0.6 逐字节一致；**未执行平台 task_layer 全链 E2E，需集成方复测**）;
3. heater 算例 vendor 回归数据（extract / post 验收数）。

### 回执（2026-11-06，集成侧）

**契约变化确认（值级；task_layer 零代码改动、零模块 manifest 改动）：**
- apiVersion 7 不变；errorContract `qs-json-stderr`；exit 码 0/2/3/4/5/6（新增 6 = `QS-TH-RUN-LOCKED` 流水线锁，**活跃句柄检查**——空闲残留锁文件不阻断运行：heater 案例 0 B `.locks/thermal.pipeline.lock` 残留不影响跑通，已实测）;
- 1.0.7 README 新增参数默认（`--mesh`/`--materials`/`--source`/`--boundary`/`--op0` 标准位置）——平台不传这些参，走 manifest 默认，路由不变；
- `extraction_report.json` 新增 3 字段（`surfaceFilmTriangles`/`convectionResistors`/`ambientTemperatureSources`）；

**E2E 复测（三通道，1.0.7 双 exe）：**
1. **demo1_4.1 直跑**（vendor CLI）：extract/post exit 0；五项主要物理产物 + 节点温度 CSV 与 1.0.6 **逐字节一致**（10/11；唯一差异 = 报告新增 3 个零值字段，数值字段无变化）；基线 344.1037 K / 9564 行 / 12750 节点 ✓ —— **vendor 字节兼容承诺成立**；
2. **heater 直跑**（vendor CLI，`--data-root` 单参）：extract/post exit 0；11/11 物理产物与 vendor 预烘焙参考**逐字节一致**；6089 节点 / 0 W / 608 @500 K / 1961 @293.15 K（fixed 2569）；post 6089/6089 映射、max 500 K、min 293.145 K ✓ —— **vendor 验收数成立**；
3. **平台全链**（task.run thermal_basic × 2 + 回归套件）：demo1_4.1 平台跑基线不变（344.1037 K / 9564 行 / 12750 节点、参考字节一致）；heater 平台跑（网格落标准路由 `Result/gds2geo/thermal/`、schema 3 空 sources，无需 meshFile 同步）6089 / 0 W / 6089 行 / max 500 K / min 293.145 K、物理产物与直跑逐字节一致；回归套件（rpc_smoke / p1_selection / gate / baseline_e2e）全绿——**回归套件明细见本段末尾复测记录**。

**集成动作：**
- 模块替换：`thermal/` + `task_layer/dist/thermal/`（ThermalExtract 468,480→488,960 B、TemperatureProcess 459,264→460,800 B、manifest 8,008 B，SHA 校验）;
- 案例归档：`dev/cases/thermal/heater/`（含预烘焙 Result + 0 B `.locks` 残留）;
- 文档/配置值级：pipelines.json thermal_basic 描述（config/build/dist 三份）、UI_API.md（thermal_basic 行 + 热链流程）、ERROR_CODES.md（`3.3 thermal 段）、templates/README（1.0.7 布局 + 成对表格 + 使用流程）、dev/cases/thermal/README（heater 段）、DATA_LAYOUT v1.5 修订 3;
- 交付：PEEC_Studio_20261106_v2.1.zip **重切**（同版本号重发先例：含 thermal 1.0.7 + heater 算例 + 更新交付说明）。

**留档 flags：**
- heater 算例 README 引用的 `validation/` 目录（原用户 OP0 + 补齐对照报告）未随分发包提供（**vendor 遗漏**）——预烘焙 Result 与 Model OP0 已经直跑与平台复测接受，不影响算例可用性;
- heater 算例 `.locks/thermal.pipeline.lock` 为 vendor 运行残留（0 B）——模块锁检查活跃句柄、不阻断运行，保留并加注;
- 双 exe 均有变更：直跑字节兼容已验证（见上），无漂移。

### 复测记录（2026-11-06，回归套件）

2026-11-06（build\task_layer.exe，同 P3 验收构建）回归套件 + heater 平台全链复测：
- rpc_smoke 8/8 PASS（ping / modules.list 12 模块 / pipelines.list 三条 / inputs.validate 0 问题 / task.run done / task.result 2 文件）；
- p1_selection_test 22/22 PASS（T11 S_parameter.csv 与参考逐字节一致 22,118 B——compact_em v1.3 红线不变）；
- gate_test 24/24 PASS（P1.5 用户闸门全场景：暂停/白名单拒绝/取消/断连/NOTWAITING/CLI 拼参）；
- baseline_e2e：A. thermal_basic@demo1_4.1（1.0.7 exe）done——MaximumTemperature 344.1037 K、NodeTemperature 9564 数据行、ThermalNetList.sp、ThermalTemperatureMap.csv 均与 1.0.6 参考**逐字节一致**，networkNodes=12750；
  B. gds_full——14/14 产物与 final3 参考逐字节一致（gmsh 共享化后行为不变）；
- heater_plat_test（新增）：task.run thermal_basic@heater（网格落标准路由 Result/gds2geo/thermal/、schema 3 空 sources，无需 meshFile 同步）done——extract/post 均成功，
  6089 节点 / 0 W / fixed 2569，post 6089 数据行、max 500.0 K / min 293.145 K，11/11 物理产物与 1.0.7 直跑参考**逐字节一致**。
结论：1.0.7 集成对既有三流水线基线**零回归**；heater 平台全链验收通过（与 vendor 直跑数一致）。

## 第 2 轮（2026-11-04 请求 → 待回执）

状态：**已收口（2026-11-04 同日）** —— 1.0.6 重发版已收到并集成（模块目录替换 + 平台预检切换 + E2E 双通道实测通过；DATA_LAYOUT 升 v1.5 修订 1 定形提取控制载体）。

### 请求正文（2026-11-04，用户与 thermal 侧确认）

致 thermal 模块团队。以下三点为已确认结论，请在下一版重发版落地：

1. **OP0 默认路径 → `Model/` 目录**（`Model/thermal.op0`）：程序去 `source_thermal.json`
   所在目录（Model/）读取；1.0.5 时代的 `Result/thermal/post/thermal.op0`（暂存路径，
   1.0.5 README 中"临时路径待用户后续确定"所指）自下一版起不再使用。注：平台已实测
   1.0.5 二进制——其默认路径仍是 Result/thermal/post/（T2 失败），"去 source_thermal.json
   目录读取"需下一版实现；manifest 的 --op0 路径须同步改（见下方实测）；
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

### 回执（2026-11-04，平台集成）

收到：`thermal_module_20261104_1.0.6.zip`（2026.11.04-native-1.0.6，apiVersion 7）+
`thermal_cases_20261104.zip`。核对：manifest（consumed 删控制件 / `--op0 {dataRoot}/Model/thermal.op0` /
source.def 要求 schema 2 + thermal / `--settings` 移除）与 README（OP0 随 source 目录、传入
--settings/--ports/--result-binding 报 exit=2、输出新增 stage_receipt.json、模块包不携带默认件）
与 v1.5 契约一致；提取控制载体按 v1.5 ② 声明 = 内嵌 source_thermal.json（schemaVersion 2）。

E2E 实测（2026-11-04）：
- ① 直接案例跑：extract=0 / post=0；参考输出 14/14 一致（stage_receipt×2 时间戳 + 报告内
  绝对路径为环境差异，物理产物逐字节一致）；基线 344.1037 K / 9564 行 / 12750 节点；
- ② 平台流水线（task_layer thermal_basic）：inputs.validate 通过（5 件必需，无控制件、
  无旧 OP0 路径）；全链 1.9s all stages done；post 未被跳过（OP0 在 Model/ 通过新预检）；
  344.1037 K / 9564 行 / 12750 节点；post_binding schema 2（指纹与 vendor 案例一致）。

集成动作：模块目录替换（repo + 交付包 staging）、pipelines post 预检 → `Model/thermal.op0`、
templates 移除控制件初值、UI_API/ERROR_CODES 更新、案例包重新归档
`dev/cases/thermal/demo1_4.1`（OP0 在 Model/；新增 开始测试.ps1 / 导入ALPS结果.ps1，
导入目标即 Model/thermal.op0）。交付包升 **v1.2**（PEEC_Studio_20261104_v1.2.zip）。

### 平台实测（1.0.5 二进制，2026-11-04，demo1_4.1 案例直跑，供参考）

| 测试 | 结果 |
|---|---|
| T0 基线（extract + post 原样） | exit=0；344.1037 K / 9564 行 / 12750 节点（基线一致） |
| T1 删控制件后原样跑 extract | **exit=3，QS-TH-IO-FILE：Cannot read Result/thermal/thermal_extract_control.json** |
| T2 OP0 只放 Model/ 后原样跑 post | **exit=3，QS-TH-IO-FILE：Cannot read operating-point file**（默认路径仍是 Result/thermal/post/） |
| T3 控制件只放 Model/ 后原样跑 extract | **exit=3**（--settings 默认路径仍是旧位置） |
| T4 OP0 放 Model/ + 显式 --op0 | **exit=0；数值与基线完全一致** |

结论：
- OP0 → Model/：1.0.5 **二进制已支持**（--op0 读任意路径，T4 通过）；**默认路径**仍是
  Result/thermal/post/（T2），且 manifest argsTemplate 写死旧路径 → 落实需要 manifest
  更新（改 --op0 路径，无需重编译二进制）或下一版改程序默认路径；
- 移除控制件：1.0.5 **二进制不支持**（T1/T3 缺失即报错；控制数据未内嵌于
  source_thermal.json）→ 下一版二进制须不再读取该独立文件，并明确提取控制数据载体
  （见请求正文第 2 条）；
- OP0 与网表暂不校验：1.0.5 现状即不校验（--result-binding 已取消），无需改动。

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
