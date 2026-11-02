# Compact CLI 错误码表 v0.1

登记日期：2026-10-22。依据用户提供的《QS-PEEC 错误码与异常处理规范 v0.1》，仅登记当前 CLI 实际使用的子集及以下增量。本文档是本仓库版本化登记，未改写微信附件或历史交付包。

`Common/QSError.h` 定义 `qs::Error` 与统一顶层序列化。失败退出码为非零；机器判别使用 JSON 的 `code`，不解析英文字符串。stderr 与 `Result/logs/compact_em/<module>-<trace_id>.jsonl` 的错误记录相同，均为 UTF-8。输出目录未解析或不可写时至少保留 stderr；日志写失败另发 `QS-INTERNAL-LOG-NOT_WRITABLE` warning。每次程序调用生成一个 trace_id，消息包含 module、severity、message_dev、message_user_zh、user_action、file_refs、recoverable。`user_action` 是动作键，当前 CLI 不实施前端 UI。

已识别业务错误 severity=error；未知 std::exception 与非标准异常兜底 `QS-INTERNAL-UNHANDLED_EXCEPTION`，severity=fatal。bad_alloc 归 `QS-SOLV-MEMORY-OUT_OF_MEMORY`。recoverable 表示修正输入/环境后可重新启动 CLI，并非在半成品上自动续跑。未分类内部不变量保留内部异常，不根据任意文本猜测原因。

## 本地增量及触发条件

| 新登记错误码 | 触发条件 |
| --- | --- |
| `QS-BC-PORT-MAPPING_AMBIGUOUS` | `source_em.json` 的方向与 `triangleIds` 顺序相反；历史几何端点的多重匹配记录属于旧输入。 |
| `QS-BC-PORT-SHORTED` | PORT记录两端相同，或几何映射后两端是同一实际电位节点。 |
| `QS-COMM-DATA-STALE` | 阶段记录的输入/输出指纹与当前文件不同，或执行期间输入变化。 |
| `QS-FMT-INPUT-AMBIGUOUS` | 输入目录含多个网格/材料候选，无法唯一选择。 |
| `QS-FMT-MSH-INVALID` | 共享网格读取器拒绝格式、元素或标签契约；不冒称所有失败均为版本问题。 |
| `QS-FMT-SETTING-INVALID` | CLI选项缺值、数值格式、范围、重复项或已移除配置项。 |
| `QS-FMT-SETTING-UNKNOWN_KEY` | 共享设置解析器不认识键名，包括无效PORT_Z0_N后缀。 |
| `QS-IO-FILE-OPERATION_FAILED` | filesystem_error没有可可靠细分的文件系统原因，保留系统error_code与路径。 |
| `QS-IO-FILE-WRITE_FAILED` | 输出打开、flush、close、阶段状态或发布失败，但没有可靠系统原因证据；不伪装磁盘满或权限拒绝。 |
| `QS-MAT-MODEL-UNSUPPORTED` | 模型实际使用有损体材料、非导体表面或不满足导体表面属性约束；当前实 P/L 流程不装配这些模型。正电导导体表面按 PEC 近似计算。 |
| `QS-POST-PLOT-FAILED` | 外部GNUplot执行失败或未生成所要求的文件。 |
| `QS-POST-PORT-OUT_OF_RANGE` | POST_PORT不在已加载端口范围。 |
| `QS-SOLV-MODE-UNSUPPORTED` | Solver_SET=1时域实现尚未验收，CLI在计算前拒绝。 |

## 使用的规范码及对应边界

| 规范错误码 | 当前触发边界 |
| --- | --- |
| `QS-FMT-MSH-PHYSICAL_TAG_MISSING` | 无激励线、无 PEC 三角面、Line/Surface tag 不匹配。 |
| `QS-BC-PORT-FLOATING` | `source_em.json` 引用了不存在或未映射到 PEC 面节点的导体三角形元素 ID。 |
| `QS-SRC-PORT-COUNT_MISMATCH` | PORT 声明数量不正、缺行或多行。 |
| `QS-SRC-REF_IMPEDANCE-MISMATCH` | 端口阻抗非有限或非正。 |
| `QS-SRC-SWEEP-EMPTY` | 求解实际频率列表为空。 |
| `QS-VAL-NUM-NAN_INF` | 修正后P/L非有限、输入矩阵非有限、求解数值非有限、绘图必须有限的单元无效。 |
| `QS-VAL-DIM-MISMATCH` | 方阵、B2N列数/行数或内部装配尺寸契约不匹配。 |
| `QS-VAL-OUT_OF_RANGE` | 支路端点、扫描或数值参数越界。 |
| `QS-VAL-UNIT-MISMATCH` | 材料属性的单位不符。 |
| `QS-IO-FILE-NOT_FOUND` | 必需设置、模型、材料、矩阵、端口、CSV或所要求的GNUplot可执行文件不存在。 |
| `QS-IO-FILE-PERMISSION_DENIED` | 文件存在但无法打开，限相关输入读取边界。 |
| `QS-IO-FILE-CORRUPTED` | 矩阵/PORT/阶段记录/后处理源格式不完整、尾部垃圾或坏读；也包括 `source_em.json` 的网格 SHA-256 不匹配。 |
| `QS-IO-FILE-MAGIC_BAD` | 二进制矩阵非PEECMAT1。 |
| `QS-IO-DIR-NOT_WRITABLE` | 结果目录创建失败。 |
| `QS-IO-DISK-FULL` | filesystem_error明确报告no_space_on_device；未用无法分辨原因的写失败冒充磁盘满。 |
| `QS-FMT-JSON-INVALID` | JSON解析器语法拒绝。 |
| `QS-FMT-JSON-SCHEMA_VIOLATION` | 材料schema、属性、重复标识或必需字段无效。 |
| `QS-MAT-CODE-NOT_FOUND` | Volume材料编号未在库中登记。 |
| `QS-SOLV-MATRIX-SINGULAR` | LAPACK返回正info；PN_DD或频域系统不能求解。负info是内部参数契约错误，归QS-INTERNAL-ASSERTION_FAILED。 |
| `QS-SOLV-CONVERGENCE-FAILED` | 介质任一RHS列或逐频MNA任一激励列的尺度化后向误差超过现有1e-8工程接受水平。 |
| `QS-POST-FREQ-POINT_NOT_FOUND` | POST_FREQ_INDEX超出实际采样列表。 |
| `QS-POST-RESULT-NOT_FOUND` | 所需CSV或本次未产出的Post_Source不存在。 |
| `QS-COMM-DEPS-NOT_READY` | 上游缺工件索引、仅有半套矩阵，或同dataRoot已有活动Compact运行/当前进程已有阶段所有者；锁冲突在写出产物前拒绝。 |
| `QS-SOLV-MEMORY-OUT_OF_MEMORY` | 模型读入后规划预算不能容纳共享数据和一名worker，或顶层捕获bad_alloc；报告路径在再次分配失败时仍以固定完整JSON降级。 |
| `QS-INTERNAL-UNHANDLED_EXCEPTION` | 未分类标准异常/未知异常。 |
| `QS-INTERNAL-ASSERTION_FAILED` | 内部数据契约断言（预留）。 |
| `QS-INTERNAL-LOG-NOT_WRITABLE` | 无法追加JSONL日志，stderr降级。 |

## 文件交接与兼容边界

各阶段artifacts.json是必需的内部数据一致性附件，包含schemaVersion=1、provider、stage和实际inputs/outputs的相对路径/SHA-256。它不记录任务状态。输出批次先完成关闭与检查，索引最后原子发布；发布中断后旧索引与新文件不匹配时下游拒绝混合批次。输入在执行期间变化同样拒绝发布新索引。

索引缺失、坏格式或指纹不符须重跑上游；完整旧结果只有与当前输入匹配时才可继续消费。整个实际选中的set.json或过渡set.txt纳入指纹。外部自定义CSV仍作格式及本次读取前后校验，数据路径限制在共享输入和本模块产物内。B2N/PORT保持0-based、-1地节点，V1模型包与V2源快照不改变。

stage_receipt.json只作可选自证；缺失、损坏或写失败不作为消费者前置条件。任务调度仅由task_layer维护；模块完全不读写Result/tasks。锁通过.locks/compact_em.run.lock句柄实现，锁文件存在不等于活动占用。

## API 2 接入行为

0表示成功，1表示已知或标准异常失败；Extractor未知异常返回2，其余程序返回1。缺data-root、路径越界、非法runtime/设置JSON/已知参数报QS-FMT-SETTING-INVALID；未知新设置JSON键按v1.2忽略。缺内部artifacts索引报QS-COMM-DEPS-NOT_READY，索引/实际文件不一致报QS-COMM-DATA-STALE。receipt写入失败只warning，不改变退出码。端口固定名source_em.json，缺失时报QS-IO-FILE-NOT_FOUND，不自动读取旧名port_em.json；源定义坏JSON报QS-FMT-JSON-INVALID。设置为set.json，日志Result/logs/compact_em。
