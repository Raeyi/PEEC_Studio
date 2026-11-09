# gds2geo 模块重打包请求日志（REPACK/gds2geo）

> 本文件为长期滚动日志：每次重打包请求一轮，最新一轮在最上面。每轮固定三段：**问题清单 → 交付要求 → 回执**（E2E 结果 / 偏差 / 收口状态），回执写回即该轮关闭。
> 关联活契约：`DATA_LAYOUT.md`（v1.6）/ `MODULE_MANIFEST_SPEC.md`；包内模块当前版本：**V1.2 r2（GDS2GEO-V1.2-20261107-r2，2026-11-09 集成；2 EXE / 5 文件 / manifest apiVersion 4；gmsh 在平台 tools/ 区）**。

## 第 3 轮（2026-11-09 请求，进行中）

状态：**进行中 —— 待 vendor 回执**。V1.2 r2（GDS2GEO-V1.2-20261107-r2）已于 2026-11-09 集成（2 EXE / 5 文件 / apiVersion 4；Result/gds2geo 布局重构为 em_geo/thermal_geo/em_msh/thermal_msh；EM 正式件落位改为平台 `publishes` 声明，见 task_layer README 2026-11-09 条目）。

**问题清单（平台 2026-11-09 API-4 探针发现）**：
- r2 em MSH（Delivery API 4）保留全部原生 CAD 实体（dimensions 0–3，含 Line2），而 compact_em v1.3 的标准 MATDB/source 输入契约**拒绝激励线元素**（QS-FMT-MSH-INVALID "Standard MATDB/source_em.json input must not contain excitation line elements"）；网格文件本身解析正常（608 点 / 1277 元素 / 2 材料 + vacuum id 4 全部解析成功，端口 triangleIds 有效）。平台 r2 manifest 的 executionProfiles **无线元素排除选项**，vendor 程序无相应开关 ⇒ **GDS→PEEC 生成网格全流目前阻塞**（预烘焙网格算例不受影响，红线算例字节一致 PASS）。
- r2 MSH 为 vendor 多行记录方言（manifest 自述 "Gmsh 4.1 ASCII"），两求解器（compact_em v1.3 / thermal 1.0.7）均可正常解析，非格式不兼容；thermal 解析 r2 热网格正常（mesh loaded: 8095 nodes），失败源于旧算例 boundary_thermal.json 的 surface_ids 绑定旧网格编号（算例制备绑定问题，非求解器缺陷）。

**交付要求**：
1. （阻塞项）二选一：a) r2 MSH 提供**无线元素导出选项**（如 `--no-1d` / profile 开关）；b) r2 README/manifest 明确消费端适配方法（如何区分 CAD Line2 与激励线、以何实体为端口定义）。r2 manifest capabilities.mesh 已声明 "do not treat CAD Line2 as ports"，消费端适配口径需 vendor 确认。
2. 案例包 `gds2geo_cases_20261107.zip`（r2 manifest externalCasePackage 声明）随下一轮交付；`validation/` 目录交付（r2 与 V1.2 1106 轮均缺）。

**回执**：待 vendor 回执。回执后平台复测（compact_em 标准输入 × r2 生成网格），本轮回执写回即关闭。

---

## 第 2 轮（2026-11-06 gmsh 共享化 → 已收口）

状态：**已收口** —— gmsh 4.11.1（86 MB）由 gds2geo 包内移出至平台共享工具区
`{MODULE_ROOT}/tools/gmsh/gmsh.exe`（git mv，单一事实源），后续模块（emopt 等）经同一 tools/ 区
取用各自声明的 gmsh 工件（emopt 计划为 `gmsh-4.8.dll` + `EMOPT_GMSH_LIBRARY`，见
`docs/plans/EMOPT_INTEGRATION_20261104.md`）。gds2geo 主程序**零代码变更**：manifest 既有解析链
（同目录遗留兜底 → `GDS2GEO_GMSH_EXECUTABLE` → `PATH`）+ modules.json 路由表 4 阶段 env 注入值
改为 `{MODULE_ROOT}/tools/gmsh/gmsh.exe`（纯配置，task_layer 无需重编译）。

### 问题清单（2026-11-06）

- gmsh.exe 所有权属 gds2geo 模块包，但后续多个模块（emopt 等）都要调用 gmsh；
  工具二进制应平台所有（D7 单一所有者），不寄生在任一 vendor 包内。

### 交付要求（2026-11-06）

- gmsh.exe 移至部署根 `tools/gmsh/`（平台所有，与模块包平级）；
- gds2geo manifest 值级变更 **V1.2**：`externalDependencies.gmsh.bundled: false`、
  `deliveryFiles` 8 → 7（去 `gmsh.exe`）、`configuration` 注明平台注入为默认路径
  （解析链顺序与代码行为不变：同目录遗留优先 → env → PATH）；
- modules.json 路由表 gds2geo 4 阶段（source/prepared/em/thermal）`GDS2GEO_GMSH_EXECUTABLE`
  指向 tools/ 区；包 README 同步（七文件布局 + tools/ 说明 + PowerShell 参考改 tools/ 路径）。

### 回执（2026-11-06）

- 基线验证：`baseline_e2e`（gds_full 四阶段 + thermal）按变更重跑，14 工件 vs final3 零差异、
  thermal 数值不变（同一 gmsh 二进制经 env 解析，行为等价）；
- 无偏差：manifest 契约 schema 未动（仅本包值级变更）；四主 EXE 未动；
  回退兼容——旧部署包内仍有 gmsh.exe 时同目录优先规则照旧生效。

状态：**已收口** —— V1.1 已集成：四个主 EXE + gmsh 与 1102 版逐字节一致（本版为文档/打包修正：manifest version=V1.1、gmsh bundled=true、退出码契约 0/2/5/6）；gds_full 四阶段必需 / 仅 CSV / 命名互斥量（无锁文件）已验证；E2E gds_full 约 1.9s done（heater_strip 模型）。

### 请求正文（2026-11-02）

- 模块包重打包为四件套齐备（8 文件：4 EXE + gmsh + manifest + README + ERROR_CODES）；
- README 引用的案例包 gds2geo_cases_20261102.zip 需随交付（当时未交付）。

## 未决项（非重打包）

- **案例包 gds2geo_cases_20261102.zip 未交付**（README 引用）：仅影响平台侧 dev_case 回归自测，不阻塞 UI 发布；经用户催收。交付后归档 dev/cases/gds2geo/。
- **r2 案例包 gds2geo_cases_20261107.zip 亦未交付**（r2 manifest externalCasePackage 声明；2026-11-09 集成时核对包内无此件）：仅影响 dev_case 回归自测，不阻塞主包交付；已列入第 3 轮交付要求。`validation/` 目录仍缺（r2 交付 = 2 EXE + README + ERROR_CODES.md + module.manifest.json 共 5 文件）。
