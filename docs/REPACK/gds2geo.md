# gds2geo 模块重打包请求日志（REPACK/gds2geo）

> 本文件为长期滚动日志：每次重打包请求一轮，最新一轮在最上面。每轮固定三段：**问题清单 → 交付要求 → 回执**（E2E 结果 / 偏差 / 收口状态），回执写回即该轮关闭。
> 关联活契约：`DATA_LAYOUT.md`（v1.4）/ `MODULE_MANIFEST_SPEC.md`；包内模块当前版本：**V1.1（2026-11-02，docs_1.2 重打包）**。

## 第 1 轮（2026-11-02 docs_1.2 重打包 → 已收口）

状态：**已收口** —— V1.1 已集成：四个主 EXE + gmsh 与 1102 版逐字节一致（本版为文档/打包修正：manifest version=V1.1、gmsh bundled=true、退出码契约 0/2/5/6）；gds_full 四阶段必需 / 仅 CSV / 命名互斥量（无锁文件）已验证；E2E gds_full 约 1.9s done（heater_strip 模型）。

### 请求正文（2026-11-02）

- 模块包重打包为四件套齐备（8 文件：4 EXE + gmsh + manifest + README + ERROR_CODES）；
- README 引用的案例包 gds2geo_cases_20261102.zip 需随交付（当时未交付）。

## 未决项（非重打包）

- **案例包 gds2geo_cases_20261102.zip 未交付**（README 引用）：仅影响平台侧 dev_case 回归自测，不阻塞 UI 发布；经用户催收。交付后归档 dev/cases/gds2geo/。
