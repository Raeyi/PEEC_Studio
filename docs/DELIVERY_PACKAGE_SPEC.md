# 打包交付规范（DELIVERY_PACKAGE_SPEC）

版本：v1.5（2026-11-12）｜维护方：平台集成侧｜UI 模块组可对照 §二/§四 审查交付包
修订（2026-11-12，用户确认）：§八 共享面修正 5→7 条目——REPACK 三模块轮次记录（compact_em / gds2geo / thermal）全保留入包；内务文档收窄为 DELIVERY_PACKAGE_SPEC.md + README.md 2 件。
修订（2026-11-12，ALPS 集成轮，用户确认 v2.5.0_rc1）：§二 白名单 42→46 条目——tools/alpsrunner/ 新增 4 件（ALPSRunner 1.1.2）；thermal 1.0.9（EXE 重交付，OP0 默认位置 → `Result/thermal/alps/thermal.op0`，无旧路径回退）；templates/ 初值 thermal.op0 改址 `Result/thermal/alps/thermal.op0`（件数仍 7）；§六/§八 计数同步；docs 共享包 → docs_1.7.zip（8 条目，+REPACK/alpsrunner.md）。
基线：PEEC_Studio_20261112_v2.5.0_rc1（2026-11-12，46 条目，主包内容基线 = 分支 feature/alps-20261112 提交 0910b00，未合 main；task_layer exe = build 目录 ③b 终构建件 1,120,622 B）+ docs_1.7.zip（8 条目，v1.7 共享面 = 4 契约 + REPACK×4）

## 一、目的与范围

规范交付给 UI 模块组的打包件「什么进包 / 怎么装配 / 怎么验证 / 怎么记录变更」，防止两类问题（v2.1 均实际发生并已修复）：
- **多余交付物**：开发残留件（配置回滚文件 *.v1.bak、字节码缓存 __pycache__）随目录拷贝被扫进包；
- **过度收纳**：案例专属件（matdb.json）误当包内初值件收入。

**核心原则——白名单制：包内只允许出现 §二 所列文件；不在表内 = 不打包。**

## 二、包内结构白名单（当前 46 条目）

包根 = MODULE_ROOT。

| 区域 | 文件 | 数量 |
|---|---|---|
| 包根 | 给UI负责人的交付说明.md（唯一根文档；变更说明权威件，见 §五） | 1 |
| compact_em/ | module.manifest.json / README.md / ERROR_CODES.md / PEEC_Model_Extractor.exe / PEEC_Solver.exe / PEEC_Post_Processing.exe / libiomp5md.dll（v1.4 起运行依赖，须与 EXE 同目录） | 7 |
| gds2geo/ | module.manifest.json / README.md / ERROR_CODES.md / gds2geo.exe / geo2msh.exe / geoedit.exe / mshcheck.exe / mshfix.exe（1.3.0，2026-11-12 起；后三件为辅助命令，不接入平台流水线） | 8 |
| thermal/ | module.manifest.json / README.md / ERROR_CODES.md / ThermalExtract.exe / TemperatureProcess.exe（1.0.9，2026-11-12 起：apiVersion 8；OP0 默认位置 → `Result/thermal/alps/thermal.op0`（`--op0` 显式可覆盖，无旧路径回退）；post optional） | 5 |
| tools/gmsh/ | gmsh.exe / gmsh-4.11.dll（平台共享工具区；模块包不含 gmsh；1.3.0 起 gmsh 4.11.1 精确匹配，gds 子进程 PATH 前置 tools/gmsh/） | 2 |
| tools/alpsrunner/ | ALPSRunner.exe / module.manifest.json / README.md / ERROR_CODES.md（ALPS 远端仿真模块 1.1.2，2026-11-12 起；apiVersion 1，errorContract none → `QS-TL-MOD-EXEC`；`artifacts.produced` 空、无固定公开 OP0 / `publishTo`（§6.7 平台责任）；结果 OP0 由平台导入步骤落位 `Result/thermal/alps/thermal.op0`（流水线 alps 阶段 / 独立通道 `alps.importOp0` / CLI `alps-import-op0`）） | 4 |
| task_layer/dist/ | task_layer.exe / README.md / UI_API.md / ERROR_CODES.md / Qt6Core.dll / libgcc_s_seh-1.dll / libstdc++-6.dll / libwinpthread-1.dll / bin/Qt6Core.dll / bin/worker_host.exe / config/modules.json / config/pipelines.json | 12 |
| templates/ | README.md / Model/source_thermal.json / Model/boundary_thermal.json / Model/source_em.json / Setting/set_thermal.json / Setting/set_em.json / Result/thermal/alps/thermal.op0（2026-11-12 起自 Model/thermal.op0 改址，DATA_LAYOUT v1.7） | 7 |
| **合计** | | **46** |

注：
- 模块 EXE 清单以该模块当前版 module.manifest.json 为准（如 compact_em v1.3 起不含绘图 EXE）；构成变化时同步更新本表与 §六 验证计数；
- `task_layer/dist` = 平台交付目录，拷贝规则见 §三 第 2 条（**显式文件清单，禁止整目录镜像**）；
- `templates/` = 算例输入初值（详见 templates/README.md）；**matdb.json 为案例专属件，不随包**（全流量 gds_full 时由 gds2geo 自动落位 Model/matdb.json；最简单算例由案例自带）。

## 三、Staging（装配目录）规则

Staging 目录 `E:\code\PEEC_Studio_YYYYMMDD`（日期 = 交付批次日），其内容 = 包内全部内容：

1. 模块目录 compact_em / gds2geo / thermal / tools 自仓库同名目录整拷（模块目录本身只含交付件）；
2. `task_layer/dist` 按**显式文件清单**拷贝（§二 12 件）——repo dist 可能含开发残留（如 config\*.v1.bak），禁止整目录镜像（v2.1 .bak 事件根因）；
3. `templates/` 按显式文件清单拷贝（§二 7 件）；
4. **切 zip 前所有改动件（含文档：交付说明、templates/README、各模块 README / ERROR_CODES）必须先同步进 staging**，同步完成再切（"staging 文档滞后"事件根因）；
5. staging 出现白名单外的新文件需求 → 不放入；确需入包 → 先改 §二（版本 +1 行并在交付说明注明），再装配。

## 四、负面清单（必须排除）

| 类别 | 示例 |
|---|---|
| 回滚/开发备份件 | *.v1.bak、*.bak、*.orig、*.tmp、*.swp |
| 语言缓存/构建产物 | __pycache__/、*.pyc、build/ |
| 版本控制/编辑器/系统元数据 | .git/、.vscode/、Thumbs.db、.DS_Store |
| 案例与运行产物 | dev/cases/（案例按惯例包外交付）、Result/、.locks/、*.lock |
| 文档包内容 | docs/（另以 docs_X.Y.zip 打包） |
| **一切白名单外文件** | **最终规则；上表仅为典型类别提示** |

## 五、变更记录（给 UI 侧的口径）

三件套，按权威顺序：

1. **给UI负责人的交付说明.md（包根，权威）**——每次交付必须列明"改了什么"：
   - 头部"修订（日期，版本）：…"每正式版本一行；
   - "**重切（日期，同版，说明）：①…②…③…**"块——**每次重切必加**，编号条目按需覆盖：模块版本变化 / 契约与布局变化 / templates 初值变化 / EXE 变化 / 回归结果（字节对比基线）/ flags（已知问题与影响）；
   - §二 本版变更、§六 UI 适配要点 常新——**UI 需要动作的事项集中在 §六**。
2. **UI_HANDOFF_YYYYMMDD_vX.Y.Z.md（包外）**——批次工作交接件；与 in-zip 交付说明不一致时以 in-zip 为准；
3. **包内各 README / UI_API.md / ERROR_CODES.md**——对应侧契约变化（协议 / 流水线 / 输入格式 / 错误码）时必须与交付说明同批同步，口径一致。

## 六、切包前验证（强制 gate，全过方可切包）

Python `zipfile`（in-memory 读取 + SHA256；pwsh Compress-Archive 仅用于切包，验证必须走 Python）：

1. 条目数 = §二 白名单计数（当前 **46**）；
2. §四 负面清单零命中（*.bak / __pycache__ / .git / Result / cases / .locks 等逐一检查）；
3. 关键件 SHA256 与源文件（repo 或 staging）一致——至少含：各模块双 EXE + manifest（alpsrunner 单 EXE + manifest 同口径）、templates 7 件、config 2 件、UI_API.md、task_layer/dist/README.md、PEEC_Solver.exe、geo2msh.exe、ALPSRunner.exe、交付说明（≥14 件）；
4. in-zip 交付说明 == staging 文件（逐字节一致）；
5. 改动文档 EOL 完好（LF 文档：CRLF = 0）。

任一不过 → 不切包，修复后重新验证。

## 七、版本与重切规则

- 命名 `PEEC_Studio_YYYYMMDD_vX.Y.Z.zip`：日期 = 该版本最终构建日；**X.Y.Z = 三段版本号（2026-11-11 起，v2.5 线生效）**——X = 平台大能力 / 契约大版本，Y = 功能线 / 里程碑，Z = 线内重切序号（每次重切 Z+1，不再每次重切消耗 Y）；「定版 / 正式发布」= 当时的最高 Z，交付时显式宣告（如「v2.5 线定版 = v2.5.3」）。v2.1–v2.4 为旧规则（重打 +0.1）编号，历史件不改名；
- **开发验证版（feature 分支未合 main、供 UI 验证窗口）**：记法 `vX.Y.Z_rc`（例：v2.5.0_rc，2026-11-12）——版本号 = 所基于正式版号段 + `_rc` 后缀，日期 = 构建日；验证通过合入 main 后按本节规则定版（正式版号另行宣告，不与 _rc 同号）；
- **同版重切**（当日修正）：zip 同名覆盖，**交付说明头部追加一个重切块**（编号条目），并明示旧版"已被本版替代，如已获取请作废"；
- 跨版本：新 zip 新名，交付说明头部"上一版"指向前版；
- **交付版本被替代后，旧件必须显式作废（交付说明注明或口头交接），防止 UI 侧误用旧版。**

## 八、交付清单（给 UI 模块组的完整集合）

1. `PEEC_Studio_YYYYMMDD_vX.Y.Z.zip`——主包（46 条目，§二）；
2. `docs_X.Y.zip`——平台契约文档包（当前 docs_1.7.zip，**8 条目**，2026-11-12 v1.7 重切）：DATA_LAYOUT.md（**UI 必读**）/ MODULE_MANIFEST_SPEC.md / MODULE_ONBOARDING.md / module.manifest.schema.json / REPACK/alpsrunner.md / REPACK/compact_em.md / REPACK/gds2geo.md / REPACK/thermal.md（四模块重打包轮次记录，UI 侧参照）；
3. `UI_HANDOFF_YYYYMMDD_vX.Y.md`——包外交接说明。

**共享面收窄与版本/指纹规则（v1.4，2026-11-12，用户确认）：**
- 共享包只收**纯契约**文档（UI / 平台两侧实际以代码实现的：DATA_LAYOUT / MODULE_MANIFEST_SPEC / MODULE_ONBOARDING / module.manifest.schema.json）+ REPACK 轮次记录（2026-11-12 ALPS 集成轮起 = ×4：compact_em / gds2geo / thermal / alpsrunner 四模块，同类件全保留，用户确认；v1.4 时代为 ×3）；
- **平台内务文档**（不再随共享包交付，留存仓库 docs/，经主包交付说明 / UI_HANDOFF 承载）：DELIVERY_PACKAGE_SPEC.md（本文件）、README.md（文档索引与「当前最新」行）；
- **版本号规则**：包名 `docs_X.Y.zip` 的 X.Y = **DATA_LAYOUT 版本**；仅当共享面契约变化（DATA_LAYOUT / MODULE_MANIFEST_SPEC / MODULE_ONBOARDING / module.manifest.schema.json 任一内容变化）才重切共享包，DATA_LAYOUT 升版时包名同步变更；SPEC 升版、REPACK 轮次收口、README 当前行等内务变化**不触发共享包重切、不升版本**；
- **指纹规则**：每次共享包重切记录 **SHA256 指纹**（整包 + 逐件）三处——切包 manifest（`docs_X.Y_YYYYMMDD_sha256.txt`）、UI_HANDOFF、交付说明（行式「docs 包：不变 / 重切（zip sha16，变化项）」）；消费方按指纹对账判新旧：**指纹不变 = 无需动作，变了 = 读变化项**，不以包名/版本号猜新旧。
