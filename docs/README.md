# docs/ 文档治理（2026-11-03 定）

平台契约与模块协作文档的分类、命名与增量管理规则。

## 一、活契约文档（文件名固定、不带日期，版本写在文档内）

| 文件 | 版本位置 |
|---|---|
| DATA_LAYOUT.md | 文档头部（现 v1.6）+ §十 变更记录 |
| MODULE_MANIFEST_SPEC.md | 文档头部 |
| MODULE_ONBOARDING.md | 文档头部 |
| module.manifest.schema.json | schemaVersion 字段 |
| DELIVERY_PACKAGE_SPEC.md | 文档头部（打包白名单 / staging 规则 / 验证 gate / 变更记录口径） |

规则：
- 文件名永不变；变更 = 改正文（只保留当前真，旧表述被覆盖不堆叠）+ 文档内版本/变更记录加 1 行 + git 提交（git log 即完整审计轨迹）；
- 破坏性变更升版本（DATA_LAYOUT §十：移动契约件 / 改固定名 / 改 set 格式 → 版本 +1）；文档内小修订（如 v1.2 修订 2）不升版本；
- 影响 vendor 的契约变更，同步在该模块的 REPACK 日志开新轮次。

## 二、重打包请求日志（REPACK/<模块>.md，每模块一份，长期滚动）

- 每次重打包请求 = 一个「## 第 N 轮（日期）」小节，**最新一轮在最上面**；日期只进小节标题，不进文件名；
- 每轮固定三段：**问题清单 → 交付要求 → 回执**（E2E 结果 / 偏差 / 收口状态）；回执写回该轮 → 该轮关闭；
- 文件本身不编版本号；模块在包内当前版本写在文件头部引用行；
- 轮次模板参考 REPACK/thermal.md 第 1 轮；
- 非重打包的跟办事项（如案例包催收、待用户确认项）进文件尾部「## 未决项」段，办结后删除。

现有日志：REPACK/thermal.md（第 1、2 轮已收口）、REPACK/compact_em.md（第 1 轮已收口 + 第 2 轮进行中，待 vendor 回执）、REPACK/gds2geo.md（第 1、2 轮已收口 + 第 3 轮进行中，待 vendor 回执 + 案例包未决项）。新模块接入时各建一份。

## 三、归档（archive/）

- 一次性动作、流程已闭环的文档移入（如 REISSUE_20261102.md：v1.2 重发版清单，2026-11-02 已闭环）；
- 归档文件只读；历史内容以 git 为准，不在活文档中重复。

## 四、对外发布（zip 命名）

- **契约包** `docs_<DATA_LAYOUT 版本>.zip`（当前 docs_1.6.zip，**7 条目**，2026-11-12 起收窄）：内容 = DATA_LAYOUT.md / MODULE_MANIFEST_SPEC.md / MODULE_ONBOARDING.md / module.manifest.schema.json / REPACK/compact_em.md / REPACK/gds2geo.md / REPACK/thermal.md（纯契约 + 三模块重打包轮次记录，用户确认保留）；包名版本跟 DATA_LAYOUT 走，**共享面件内容变化或 DATA_LAYOUT 升版才重打**，不带日期；每次重切记录 SHA256 指纹（整包 + 逐件）于切包 manifest / UI_HANDOFF / 交付说明，消费方按指纹对账判新旧（见 DELIVERY_PACKAGE_SPEC.md §八 指纹规则）；
  **平台内务文档不入共享包**（2026-11-12 起）：DELIVERY_PACKAGE_SPEC.md、docs/README.md（本文件）——留存本目录，经主包交付说明 / UI_HANDOFF 承载；
- **主交付包** `PEEC_Studio_YYYYMMDD_vX.Y.Z.zip`：交付快照 + 交付包版本号（2026-11-03 起启用；开发验证版记法 vX.Y.Z_rc（feature 分支未合 main，供 UI 验证窗口）；当前最新 **v2.5.0_rc**（开发验证版）= 2026-11-12 构建：仓库基线 feature/vendor-r3-20261112（DATA_LAYOUT v1.6 / thermal 1.0.7 / compact_em **v1.4** / gds2geo **1.3.0** / task_layer 源码零改动），42 条目，供 2026-11-13 UI 验证窗口；最新正式版 **v2.5.0** = 2026-11-11 构建：仓库基线 0ccbdc0（DATA_LAYOUT v1.6 / thermal 1.0.7 / compact_em v1.3 / gds2geo V1.2 r2 / task_layer P1→P3），37 条目；v2.5 功能线在三段版本号规则下的首个包；打包按 DELIVERY_PACKAGE_SPEC.md 执行）；
  升版规则（2026-11-11 起三段 x.y.z）：X = 平台大能力 / 契约大版本，Y = 功能线 / 里程碑，Z = 线内重切序号（每次重切 Z+1，不再每次重切消耗 Y）；「定版 / 正式发布」= 当时的最高 Z，交付时显式宣告（如「v2.5 线定版 = v2.5.3」）；v2.1–v2.4 为旧规则（重打 +0.1）编号；
  **日期 = 该版本的构建日期（此版本最终重打包的日期）**，版本变化则日期与版本号同时更新；版本对应内容记在 git 提交与《给UI负责人的交付说明.md》修订行；
- 案例包 / 模块包：按 vendor 交付命名；平台归档到 dev/cases/<模块>/，不作为契约件。
