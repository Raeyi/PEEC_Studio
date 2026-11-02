# Compact 输入契约

功能、输入输出与运行方式已统一到[项目发布说明](README.md)。本文保留原输入文档入口，避免同一契约多处维护。

| 内容 | 规范位置 |
| --- | --- |
| dataRoot 结构、必需文件 | [输入契约](README.md#输入契约) |
| Gmsh 版本、材料关联、坐标单位 | [网格与单位](README.md#输入契约) |
| MATDB schema、属性与厚度 | [材料库](README.md#输入契约) |
| 端口 ID、哈希、方向、阻抗与顺序 | [端口定义](README.md#输入契约) |
| set.json 格式、参数与默认值 | [计算设置](README.md#计算设置) |
| 频点与步长语义 | [频率采样](README.md#计算设置) |

旧 `materials.json`、网格激励线及 `MATERIALS_FILE`、`PEC_SURFACE_TAG`、`EXCITATION_LINE_TAG`、`PORT_Z0`、`PORT_Z0_N` 配置不受支持，须迁移到 MATDB 与 source_em.json。v1.2 固定读取 `Model/source_em.json`，内容仍为 `meshFile`、`meshSha256`、`ports`；`boundary_em.json` 暂无消费者。完整可运行样例单独交付于 `compact_em_cases_YYYYMMDD/standard/` 和 `two_layer_surface_port/`，每个案例包含输入、设置及验收 README。
