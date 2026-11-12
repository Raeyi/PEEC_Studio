# gds2geo 模块重打包请求日志（REPACK/gds2geo）

> 本文件为长期滚动日志：每次重打包请求一轮，最新一轮在最上面。每轮固定三段：**问题清单 → 交付要求 → 回执**（E2E 结果 / 偏差 / 收口状态），回执写回即该轮关闭。
> 关联活契约：`DATA_LAYOUT.md`（v1.6）/ `MODULE_MANIFEST_SPEC.md`；包内模块当前版本：**1.3.0（GDS2GEO-1.3.0-20261112，2026-11-12 集成；5 EXE / 8 文件 / manifest apiVersion 5；gmsh 4.11.1 含 gmsh-4.11.dll 在平台 tools/ 区）**。

## 第 3 轮（2026-11-09 请求 → 2026-11-12 已收口）

状态：**已收口** —— 1.3.0 已交付并集成（平台 6 腿复测全绿 + r2 全维网格全流 E2E 通过）。V1.2 r2（GDS2GEO-V1.2-20261107-r2）2026-11-09 集成，1.3.0（GDS2GEO-1.3.0-20261112，apiVersion 5）2026-11-12 落位（分支 `feature/vendor-r3-20261112`，未合 main）。

**问题清单（平台 2026-11-09 API-4 探针发现）**：
- r2 em MSH（Delivery API 4）保留全部原生 CAD 实体（dimensions 0–3，含 Line2），而 compact_em v1.3 的标准 MATDB/source 输入契约**拒绝激励线元素**（QS-FMT-MSH-INVALID "Standard MATDB/source_em.json input must not contain excitation line elements"）；网格文件本身解析正常（608 点 / 1277 元素 / 2 材料 + vacuum id 4 全部解析成功，端口 triangleIds 有效）。平台 r2 manifest 的 executionProfiles **无线元素排除选项**，vendor 程序无相应开关 ⇒ **GDS→PEEC 生成网格全流目前阻塞**（预烘焙网格算例不受影响，红线算例字节一致 PASS）。
- r2 MSH 为 vendor 多行记录方言（manifest 自述 "Gmsh 4.1 ASCII"），两求解器（compact_em v1.3 / thermal 1.0.7）均可正常解析，非格式不兼容；thermal 解析 r2 热网格正常（mesh loaded: 8095 nodes），失败源于旧算例 boundary_thermal.json 的 surface_ids 绑定旧网格编号（算例制备绑定问题，非求解器缺陷）。

**交付要求**：
1. （阻塞项）二选一：a) r2 MSH 提供**无线元素导出选项**（如 `--no-1d` / profile 开关）；b) r2 README/manifest 明确消费端适配方法（如何区分 CAD Line2 与激励线、以何实体为端口定义）。r2 manifest capabilities.mesh 已声明 "do not treat CAD Line2 as ports"，消费端适配口径需 vendor 确认。
2. 案例包 `gds2geo_cases_20261107.zip`（r2 manifest externalCasePackage 声明）随下一轮交付；`validation/` 目录交付（r2 与 V1.2 1106 轮均缺）。

**回执（2026-11-12，已收口）**：
- 平台侧执行阻塞已排除（2026-11-12）：此前 vendor 主 EXE 无法创建 `.locks`（拒绝访问，工作区内/外均现）。根因 = 原交付映像的文件对象安全描述符物化了 DSH 工作区写入 ACE（`S-1-4-805033334-89673629`，继承自工作区根 standing grant），且该 SD 内核级不可就地修改；2026-11-12 以同字节新文件对象原位替换 6 个带标记 vendor 映像（SHA256 逐一对齐、git 零 diff，vendor 包字节未动），全部恢复写入（`.locks` 创建成功）。1.3.0 新映像不携带该 SD，问题不再适用。
- v1.3 时代复测（2026-11-12 上午，1.3.0 落位前）：compact_em 标准输入 × r2 生成网格，输入四件与 11-09 探针 SHA256 逐字节一致；EXE 正常启动、`.locks` 创建成功、网格解析正常（608 点 / 1277 元素 / 2 材料 / vacuum id 4），复现 11-09 同一结论 `QS-FMT-MSH-INVALID`（exit 1）⇒ 当时交付要求 1（阻塞项）仍待 vendor 二选一交付；红线回归（pswitch / baseline / p1 21/21 / p3 / gate）全 GREEN，p2 维持 2 个已知预期 FAIL。
- **1.3.0 交付接收（2026-11-12 落位：GDS2GEO-1.3.0-20261112.zip + GDS2GEO-1.3.0-third-party.zip（tools/gmsh，含 gmsh-4.11.dll 85,817,348 B）+ gds2geo_cases_20261107.zip / gds2geo_cases_20261112.zip + SHA256SUMS.txt + validation/ 目录 + 给UI负责人的交付说明.md）**：manifest version 1.3.0 / **apiVersion 4→5**，`platformIntegrationTested:false`；**交付要求 2 满足**（案例包交付、externalCasePackage 更新为 gds2geo_cases_20261112.zip；validation/ 含 summary.json / package-verification.json / case-zip-verification.json 等回执）；**交付要求 1（阻塞项）按选项 b（消费端适配）vendor 声明侧收口**：manifest capabilities.mesh consumerRequirement 明示 “CAD Line2 is geometry, not an excitation/port definition”，与 compact_em v1.4 README “Point1/Line2 仅作几何元数据” 联动，口径由 compact_em vendor 单方确认，无双 vendor 互等（联动 REPACK/compact_em.md 第 2 轮收口说明）。
- **1.3.0 平台集成复测（2026-11-12，6 腿全绿；分支 `feature/vendor-r3-20261112`，基线 ae88dc0，未合 main）**：baseline ✓（gds_full r2 四阶段严格串行 em_geo→thermal_geo→em_msh→thermal_msh 全 exit 0，publisher 落位 Model/model_em.msh 66,834 B + matdb.json 878 B；thermal 基线 344.1037 K）、gate 22/22、p1 21/21、p3 ✓、pswitch ✓、p2 ✓1（恰为 3 个已记录预期物化 FAIL：gds_full / thermal_basic / peec_basic —— v1.4/1.3.0 二进制与契约变化导致与 cc24e18 基线字节差异，属预期）。
- 契约变化：**errorContract=none**（失败不输出结构化 stderr JSON，平台固定 `QS-TL-MOD-EXEC` + 退出码 0/2/5/6；task_layer/ERROR_CODES.md §3.2 已同步）；锁机制改为 **Windows 命名互斥量（不产生锁文件）** —— `.locks` SD 标记问题不再适用；gmsh 4.11.1 需精确匹配，`GDS2GEO_GMSH_EXECUTABLE` 仅选 exe（平台经 modules.json env 注入），**宿主启动前须将 `tools/gmsh/` 绝对目录前置到子进程 PATH**（gmsh-4.11.dll 加载）；新增辅助工具 geoedit / mshcheck（0.23.0）/ mshfix（1.5.0）仅以辅助命令暴露、不接入平台流水线（mshfix exit 3 = $GDS2GEOUnits / Tet4 不支持，与网格契约一致）。
- 树内适配：modules.json gds2geo 4 阶段 env 指向 tools/gmsh/gmsh.exe 并配置 PATH 前置；案例包 gds2geo_cases_20261112.zip 已接收，平台侧归档 dev/cases/gds2geo/ 待办（仅影响 dev_case 回归自测，不阻塞主包）。
- **收口说明**：交付要求 1/2 均满足（1 = vendor 声明 + 平台 r2 全维网格全流 E2E 通过 —— 红线 standard 算例即 r2 全维网格算例，详见 compact_em.md 第 2 轮收口说明；2 = 案例包 + validation/ 交付）；平台 6 腿复测全绿，本复测证据即 `platformIntegrationTested` 收口证据（1.3.0 manifest 自报 false，平台侧不改动 vendor 文件）。**本轮收口**。
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

- **案例包 gds2geo_cases_20261102.zip（V1.2 1102 轮 README 引用）**：由 1.3.0 交付的 gds2geo_cases_20261107.zip / gds2geo_cases_20261112.zip 取代（2026-11-12 交付树内）；平台侧归档 dev/cases/gds2geo/ 待办，不阻塞 UI 发布。
- **r2 案例包 gds2geo_cases_20261107.zip 已交付**（随 1.3.0，2026-11-12；1.3.0 manifest externalCasePackage 已更新为 gds2geo_cases_20261112.zip）；**validation/ 目录已交付**（summary.json / package-verification.json / case-zip-verification.json 等，1.3.0 交付树内）—— 第 3 轮交付要求 2 收口。
