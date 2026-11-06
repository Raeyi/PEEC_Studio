# gds2geo 模块重打包请求日志（REPACK/gds2geo）

> 本文件为长期滚动日志：每次重打包请求一轮，最新一轮在最上面。每轮固定三段：**问题清单 → 交付要求 → 回执**（E2E 结果 / 偏差 / 收口状态），回执写回即该轮关闭。
> 关联活契约：`DATA_LAYOUT.md`（v1.4）/ `MODULE_MANIFEST_SPEC.md`；包内模块当前版本：**V1.2（2026-11-06，gmsh 移出包至平台 tools/ 区）**。

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
