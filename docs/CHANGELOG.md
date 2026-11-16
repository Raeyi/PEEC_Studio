# 版本描述（CHANGELOG）

> 每个正式版本（rel 标签）的变更描述：**大改动 / 小改动 / 验证 / 发版记录**。
> 规则（2026-11-16 用户确认，即日生效）：**以后每个版本都需要在本文件登记**——
> 打标签前完成登记（登记提交位于标签提交之下，标签树含本版本条目）；发版登记提交回填标签提交哈希与推送状态。
> 开发验证版（`_rc`）不单独登记，其变更在对应正式版条目内以「验证线」一笔带过。
> 本文件只增不改：历史条目打标签后冻结，勘误另加修订行、不改原文。

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
