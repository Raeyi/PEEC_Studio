# 版本描述（CHANGELOG）

> 每个正式版本（rel 标签）的变更描述：**大改动 / 小改动 / 验证 / 发版记录**。
> 规则（2026-11-16 用户确认，即日生效）：**以后每个版本都需要在本文件登记**——
> 打标签前完成登记（登记提交位于标签提交之下，标签树含本版本条目）；发版登记提交回填标签提交哈希与推送状态。
> 开发验证版（`_rc`）不单独登记，其变更在对应正式版条目内以「验证线」一笔带过。
> 本文件只增不改：历史条目打标签后冻结，勘误另加修订行、不改原文。

---

## v2.5.2 — PEEC_Studio_20261117_v2.5.2（hotfix 合入主线 + 按需 stage 输入浅检修复）

- **定版**：2026-11-17。hotfix 分支 hotfix/gds2geo-geoedit-20261116（v2.5.1_hotfix）经 UI 侧验证（2026-11-17 用户确认）与合入轮回归全绿后，整体 fast-forward 合入 main。
- **上一版**：PEEC_Studio_20261116_v2.5.1_hotfix（2026-11-16 开发验证包，**自本版起显式作废**）；上一正式版 v2.5.1（PEEC_Studio_20261113_v2.5.1）。
- **标签**：`rel/20261117_v2.5.2`；**基线**：main @ 标签提交（hotfix 分支 22ae27f/3ee9199/ba70add 三提交 fast-forward 合入 + 本版登记提交）。
- **包规模**：49 条目（vs hotfix 包 46 件逐字节一致 + 3 件变化：task_layer.exe / worker_host.exe / 交付说明；vs v2.5.1 39 件逐字节一致 + 10 件变化）。

### 大改动

1. **gds2geo 1.3.0 → 1.3.0_fix**（vendor 2026-11-16 交付，hotfix 轮集成）
   - 5 EXE + gmsh-4.11.dll 与 1.3.0 逐字节一致（SHA256 对账全过）；module.manifest.json stages 4→9：新增 geoedit / mshcheck / mshfix_plan / mshfix_heal / mshfix_verify 5 个声明 stage + capabilities.stageInvocation 逐阶段输入/边界/验收。
   - 新增 2 条按需流水线 gds_geoedit / gds_repair（optional stage 输入缺失 → QS-TL-SKIP-NOINPUT 跳过、不阻断主链）。
   - 闭环「三辅助 EXE 未在 manifest 声明 stage、任务层不识别、前端无从驱动」缺口。
2. **task_layer 重建**（1,126,626 B `94CC9D13…0446`，含两轮源码变更）
   - 声明式 pathPrepend（launch 子进程 PATH 前置——mshfix.exe 直接 PE 依赖 tools/gmsh/gmsh-4.11.dll；未声明路由行为与 v2.5.1 一致）+ launch 宿主预建 stage 输出父目录（vendor hostPreparation）。
   - **按需 stage 输入浅检修复**：ModuleRegistry synthesize 保守策略把模块级 consumed（gds2geo 的 Model/model.gds / model_info.csv）挂到每个 stage，使纯按需工程（无 model.gds）中 geoedit / mshcheck 等按需 stage 被误判输入缺失而跳过；optional stage 若 argsTemplate 显式声明输入令牌（{dataRoot}/Model/... 字面量或 {artifact:<ref>}）则仅附加其显式引用件；无显式声明的 optional stage（compact_em.post 的 --data-root 约定）保留保守集合；非 optional stage 行为逐字节不变。
3. **配置**：modules.json +5 路由（gds.geoedit / gds.mesh_check / gds.mshfix_plan / gds.mshfix_heal / gds.mshfix_verify，env GDS2GEO_GMSH_EXECUTABLE + pathPrepend + searchDirs）；pipelines.json 阶段库 +5（全 optional）+ 按需流水线 gds_geoedit / gds_repair；gds_full / peec_basic / thermal_basic 零改动。

### 小改动

1. **文档同步**：UI_API.md 流水线表 +2 行；REPACK/gds2geo.md 第 4 轮收口；DELIVERY_PACKAGE_SPEC §二 gds2geo 行注（白名单 49 不变）；task_layer README hotfix 轮记录。
2. **测试**：p2_stage_lib_test.py 更新 5 流水线断言（16 PASS + 3 已记录预期物化 FAIL——v1 算例 fixture 漂移 cc24e18，A/B v2.5.1 同 FAIL 证明内置物化不变）；project_switch_test.py r2 输出布局常量同步（Result/gds2geo/em → em_msh + matdb.json，r2 集成后首跑暴露 6 项假 FAIL）；**p1_selection_test.py 残留产物剥离加固**（11-13 在场地跑 standard 算例在算例目录残留 Result/ + .locks/，copy_case 使 T3 的 fresh 工程误含未选中上游产物 → 创建期 QS-TL-SEL-DEP-MISSING 拒绝被翻转为任务创建成功；A/B 两版 exe 复现证明与代码版本无关；剥离后 22/22 PASS）。
3. **算例目录清理**：dev/cases/compact_em/standard 的 Result/ + .locks/ 残留（11-13 运行产物，未跟踪）移除；tracked 5 件输入件不受影响。

### 验证

- 2026-11-17 合入轮回归（staging 全量，6 腿 + vendor E2E）：baseline ✅（MaximumTemperature=344.1037 K / 9564 行 / 12750 节点红线逐字节一致；gds_full 4 阶段 + 宿主发布 model_em.msh 66,834 B / matdb.json 878 B）/ p-switch ✅（A→B→A 核心验收全 PASS）/ **p1 22/22** / p3 ✅（module.run thermal.extract 真实运行，库级路由位）/ gate ✅（15 项含闸门 CLI 注入 + 断连语义）/ p2 16 PASS + 3 预期 FAIL（已记录绿色）/ vendor-e2e 22/22。
- hotfix 轮（2026-11-16）：vendor fixture 真跑全链；UI 验收 2026-11-17 用户确认（gds_geoedit / gds_repair 按需链路可用）。
- 未决（自 v2.5.1 承继）：ALPS 模式② remote 端到端（192.168.136.172 + IC618 VM + license）；数值回归 vs vendor validation/；UI_API v1.2 遗留条款清理。

### 发版记录

- （发版登记提交回填：标签提交哈希、推送状态、交付包名）

---

## v2.5.1 — PEEC_Studio_20261113_v2.5.1（v2.5 线定版）

- **定版**：2026-11-16。上周五（2026-11-13）因部分模块测试未通过未发版；2026-11-16 补齐测试后补充发版——用户确认（原文见交付说明「补充发版说明」条）。
- **包名日期 20261113** = 版本定形日（回填例外，用户显式确认；实际最终构建 2026-11-16；例外仅本包适用）。
- **标签**：`rel/20261113_v2.5.1`；**基线**：main @ 标签提交（自 origin/main `18576e0` fast-forward 合入全 24 提交；含 v2.5.0 交付基线 `0ccbdc0`）。
- **上一正式版**：v2.5.0（PEEC_Studio_20261111_v2.5.0，2026-11-11 交付，`0ccbdc0` 基线，37 条目；= v2.3 内容定版 + UI 协议 1.1/1.2）。
- **包规模**：49 条目（vs v2.5.0 37 条目；白名单按 DELIVERY_PACKAGE_SPEC v1.5）。
- **验证线**（开发验证版，均被本版显式作废）：v2.5.0_rc（11-12，42 条目）→ v2.5.0_rc1（11-12，46 条目）→ v2.5.0_rc2（11-13，49 条目）= 本版内容。

### 大改动

1. **compact_em v1.3 → v1.4.3**（vendor r3 换版 + 1.4.3 升级，API 7）
   - 三 EXE 换版：Model_Extractor 14,891,008→9,680,384 B；Post_Processing 825,856→838,144 B；Solver 15,784,448→16,871,424 B；新增 libiomp5md.dll 1,614,184 B。
   - module.manifest.json / ERROR_CODES.md / README.md 同步（138 / 240 / 367 行变更）。
   - 数值回归（FINAL）：S/Y/Z/Source.msh 与 v1.4 基线逐字节一致；schemaVersion 1 旧设置已拒。
2. **ALPS 远端仿真平台化**（新功能：thermal.alps 可选仿真阶段 + ALPS 结果导入通道）
   - 新模块 tools/alpsrunner（alpsrunner 1.1.2，apiVersion 1）：ALPSRunner.exe 4,436,071 B + README（1,076 行）+ ERROR_CODES + module.manifest.json。
   - task_layer 新增 ALPS 结果导入：op0Import 内置 + alps.importOp0 独立通道（CLI 同能力；AlpsImport 新增）。
   - thermal_basic 流水线重排：extract →（可选）alps 仿真 + op0 导入 → post；可选上游 Skipped 不再阻断下游（③b）。
   - 两运行模式：① offline（预置 OP0）② remote（SSH 192.168.136.172 + IC618 VM + license）。
   - ALPS 运行配置初值随包入 templates/：Model/thinfilm_thermal.json（268 B）+ Setting/ALPSRunner.json（396 B）+ Setting/alpsrunner.key（2,455 B，随包交付、本地保留、不入库）。
   - ALPS 验收期望：6094/4230/2570（293.15–500 K）。
3. **gds2geo V1.2 r2（API 4）→ 1.3.0（API 5）**（vendor r3 发版）
   - 两 EXE 换版：gds2geo.exe 10,820,551→11,594,603 B；geo2msh.exe 10,952,954→11,723,692 B。
   - 新增三 EXE：geoedit.exe 11,679,827 B；mshcheck.exe 880,640 B；mshfix.exe 1,441,280 B。
   - gmsh 共享库化：新增 tools/gmsh/gmsh-4.11.dll 85,817,344 B（gmsh.exe 不变）。
   - module.manifest.json / ERROR_CODES.md / README.md 同步。
4. **thermal 1.0.7 → 1.0.9**（apiVersion 8）
   - 两 EXE 换版：TemperatureProcess 460,800→472,064 B；ThermalExtract 488,960→499,200 B。
   - post OP0 默认位置 → Result/thermal/alps/thermal.op0（显式 --op0 可覆盖，无旧路径回退）。
   - set_thermal.json 新增可选 mesh_length_unit（m/mm/um）；source schema 3 支持可选 Model/thinfilm_thermal.json 薄层。
   - extract 转产公开网表工件 thermal/netlist.dc / thermal/netlist.rc（平台落位 Model/ThermalNetList.sp）。
5. **task_layer 重编译**（2026-11-12 ③b 构建件；task_layer.exe 1,120,622 B `9F1E8AE5…BE76`；含 ALPS 集成轮全部源码变更）
   - op0Import 内置 + alps.importOp0 独立通道（见大改动 2）；
   - thermal_basic 重排 + 可选阶段依赖语义（isReady 视 Skipped+optional 为依赖满足）；
   - extract 公开网表落位；p2 回归基线 9→10 阶段全 ref；modules.json / pipelines.json alps 阶段与 op0Import 语义。
6. **DATA_LAYOUT v1.7 + templates 初值重构 + 包白名单 37→49**
   - 预置 OP0 改址：templates/Result/thermal/alps/thermal.op0（5,337,005 B；原 templates/Model/thermal.op0 移除）。
   - 白名单演进：37（v2.5.0）→ 42（_rc 记法）→ 46 → **49**（ALPS 配置入 templates；cases 算例 10 件移除；案例数据不随包）。

### 小改动

1. **文档同步**：task_layer UI_API（alps.importOp0 独立通道 + CLI）/ ERROR_CODES（§3.5 ALPS 导入六码）/ README；MODULE_MANIFEST_SPEC；REPACK/{alpsrunner 新增，compact_em / gds2geo / thermal 更新}；DATA_LAYOUT v1.7；DELIVERY_PACKAGE_SPEC v1.5（三段式版本号 x.y.z + _rc 开发验证版记法 + 白名单 49）。
2. **dev/cases（包外）**：新增 compact_em R2_PEC_Standard 算例（5 件）；standard / two_layer_surface_port 算例 rebind demo1_4.1 r2；thermal 算例 OP0 全量迁址 Result/thermal/alps/ + 归档 heater_reextract_20261109（含验收产物 15 件）+ ALPS 验收算例 Setting（ALPSRunner.json + alpsrunner.key，入 git 经用户确认）。
3. **.gitignore 固化**：dev/cases/**/Result/ 运行产物不入仓；templates/Setting/alpsrunner.key 不入库（随包交付、本地保留）。
4. **仓库清理**：根目录治理模板 docx/pptx 3 件移出；内部工作文档归入 docs/archive/（1 件 pptx 删除）；modules.json / pipelines.json displayName 括号说明清理。
5. **打包流程**：docs 共享包 docs_1.8（共享面 5 条目 + REPACK×3 全保留入包）；包门禁（49 条目 + 负面清单 + 关键指纹 + EOL + CRC）。

### 验证

- 2026-11-13 全绿：baseline_e2e ✅ / p1 22/22 / gate 23/23 / rpc_smoke ✅ / alps_smoke 22/22 / p2 15 PASS + 3 预期 FAIL（gds2geo vendor 待交付项，豁免在案）。
- compact_em 1.4.3 数值回归 FINAL（S/Y/Z/Source.msh 与 v1.4 逐字节一致；schema1 已拒）。
- 2026-11-16 补齐测试通过（用户口径）→ 本版发版。
- 未决（依赖外部环境）：ALPS 模式② remote 端到端（192.168.136.172 + IC618 VM + license）；数值回归 vs vendor validation/；UI_API v1.2 遗留条款清理。

### 发版记录

- 2026-11-16：main fast-forward 合入（18576e0 → 标签提交，24 提交）并推送 origin（main + 标签）；**v2.5.0 交付基线 0ccbdc0 自本推送首次入 origin**。
- 标签提交哈希回填：**5a9bee6**（注释标签 `rel/20261113_v2.5.1`；发版登记提交回填）
- 交付包：PEEC_Studio_20261113_v2.5.1.zip（49 条目）+ PEEC_Studio_v2.5.1_delivery_20261113.zip（wrapper，5 条目）；48 条目与 v2.5.0_rc2 主包逐字节一致，第 49 条（交付说明）更新为定版宣告。

---

## 历史版本（本文件设立前，摘要；详情见各交付说明 / 交接文档）

| 版本 | 日期 | 包 | 基线 | 一句话 |
|---|---|---|---|---|
| v2.5.0 | 2026-11-11 | PEEC_Studio_20261111_v2.5.0（37 条目） | 0ccbdc0 | v2.3 内容定版 + UI 协议 1.1/1.2；GDS2GEO-V1.2-20261107-r2 集成；freespace_em 回退 |
| v2.3 | 2026-11-10 | PEEC_Studio_20261110_v2.3 | f8408ea | 标签 rel/20261110_v2.3 |
| v2.1fix | 2026-11-10 | PEEC_Studio_20261106_v2.1_fix | 85ae1a1 | v2.1 基线 + 切换工程修复（标签 rel/20261110_v2.1fix） |
| v2.1 | 2026-11-06 | PEEC_Studio_20261106 | 49c26f2 | 前端适配基线（标签 rel/20261106_v2.1） |
