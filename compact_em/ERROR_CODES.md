# Compact_EM 错误码登记

软件版本：1.4.3（x.y.z）｜发布批次：2026-11-13｜API：7｜provider：compact_em
模型格式：PEECFS02/version2｜数据布局：peec-data/v1 revision 1.6｜日期：2026-11-13

## 导语

本版交付三阶段 CLI：extract（PEEC_Model_Extractor.exe）、solve（PEEC_Solver.exe）、post（PEEC_Post_Processing.exe，可选）。不提供 Plotter 或 Touchstone 平台 stage。旧公开设置/源文件名及无参数据搜索入口已经关闭。历史结构化错误修订将 errorContract 声明为 `qs-json-stderr`：MODULE_MANIFEST_SPEC 中 B5（errorContract 类型变化必须 +1）与 C4（none→qs-json-stderr 视为兼容增强）文字相冲突，1.4.1历史版本按B5更严格处理并递增API；1.4.2历史版本按B1因显式数据根入口收窄递增API，此决策记录于 manifest breakingChanges，不改动九份原契约。

机器判别使用 stderr JSON 的 `code` 字段，不解析英文字符串。每次进程调用生成一个 trace_id（`qs-<hex>-<hex>`；初始化分配失败时降级为固定前缀应急号 `qs-emergency-<pid>-<tick>`）。标准布局在合法日志目录已确定后，将同一 UTF-8 记录写入 stderr 和 `Result/logs/compact_em/<module>-<trace_id>.jsonl`；指定 `--runtime-dir` 时使用所选子目录。legacy 调用、早期未知目录或目录不可写时可能仅有 stderr。`recoverable` 表示修正输入/环境后可重新启动该 CLI，并非在半成品上自动续跑。用户提供的源文件称“用户源”，matdb.json 称“材料库”；内部 state/model/post_source 等派生件按实际路径记录。

当前1.4.3/API7新增平铺设置schema2、同根进程级锁和QS-COMM-MODULE-BUSY；未知可选设置字段忽略，已知不支持字段仍拒绝。

## 当前公开输入与对应报错

公开计算调用必须给出 `--data-root <case>`。设置只读 `<case>/Setting/set_em.json`，源定义只读 `<case>/Model/source_em.json`。仍需网格 `Model/model_em.msh` 和材料库 `Model/matdb.json`；“仅支持”针对设置及源定义文件名。

拒绝 `Setting/set.txt`、`Setting/set.json`、root `SET_EM.json` / `set.txt` / `set.json`、`Model/port.json` / `Model/port_em.json` 和 root `port.json` / `port_em.json`。新旧文件并存也拒绝，禁止静默选择。Windows 上大小写别名按文件系统处理，`Setting/SET_EM.json` 不会被误认为另一个旧名。

| 情况 | code | 处理 |
|---|---|---|
| 检测到上述旧名（单独或与新名并存） | `QS-FMT-INPUT-LEGACY_NAME_REJECTED` | 转换为规定的新名、位置及JSON结构后重跑；file_refs指向实际旧路径 |
| 缺少 Setting/set_em.json | `QS-FMT-SETTING-INVALID` | 提供设置；报错不再称set.json |
| 缺少 Model/source_em.json | `QS-IO-FILE-NOT_FOUND` | 提供源定义 |
| 设置JSON语法、schemaVersion（公开平铺设置要求2）、类型或取值错误 | `QS-FMT-SETTING-INVALID` | 按实际设置文件及位置修改 |
| 设置出现不支持的键 | `QS-FMT-SETTING-UNKNOWN_KEY` | 修改设置键 |
| source_em.json语法错误 | `QS-FMT-JSON-INVALID` | 修改源JSON语法 |
| source_em.json缺字段、类型错误或未知键 | `QS-FMT-JSON-SCHEMA_VIOLATION` | 修改源字段；nextId/nextID仍禁止 |
| 源文件绑定网格hash不符 | `QS-IO-FILE-CORRUPTED` | 用当前网格重新生成源绑定 |
| 源阻抗非正或非有限 | `QS-SRC-REF_IMPEDANCE-MISMATCH` | 检查源定义的impedanceOhm |
| 未提供显式--data-root | `QS-FMT-SETTING-INVALID` | 提供数据根；不再搜索EXE旁Data/Input |

旧名、缺文件及缺数据根的拒绝发生在修改业务Result及阶段marker之前；诊断日志可以追加。`--self-test`不消费用户输入，仍可单独运行。内部私有派生格式不作为外部入口，也不接受直接调用私有根。

## 结构对象（9 字段）示意

以下为**示意**对象（字段与 QSError.h Serialize 顺序一致；路径与 trace_id 为占位，非原样真实记录）：

```json
{
  "code": "QS-POST-FREQ-POINT_NOT_FOUND",
  "severity": "error",
  "message_dev": "POST_FREQ_INDEX=2 is outside the generated frequency samples (1..N).",
  "message_user_zh": "请求频点不在实际扫频采样中。",
  "user_action": "edit_settings",
  "module": "solve",
  "trace_id": "qs-xxxxxxxxxxxx-xxxxxxxx",
  "file_refs": ["<dataRoot>/Setting/set_em.json"],
  "recoverable": true
}
```

规则：`message_dev` 为开发排查用技术信息（含实际路径、字段位置、系统原因，UTF-8，非法字节以 \\xHH 呈现，控制符/NUL 可见化如 \\u0000）；`message_user_zh` 为中文用户提示；`user_action` 是动作键，当前 CLI 不实施前端 UI；`file_refs` 只列有真实输入来源的文件，不猜测。severity 取值 info/warning/error/fatal：业务错误 error；QS-INTERNAL-* 兜底 fatal；日志/回执类 warning 不阻断计算。

## 代码目录与触发/恢复边界（44 项）

以下为本仓库版本化登记，共 44 个 code。【保留】表示当前目录存在但交付三程序无独立触发路径。不冒称规范枚举全部实现。

### QS-IO-*（文件/磁盘）

| code | 触发边界 | 恢复 |
| --- | --- | --- |
| QS-IO-FILE-NOT_FOUND | 直接必需文件 open 返回 ENOENT（errno/error_code 实际映射）：例如直接打开的用户网格、材料库或用户源不存在。注意分层：标准缺 settings → SETTING_INVALID；缺 postsource → POST_RESULT_NOT_FOUND；缺上游必要 state/context 或半套 model/state → DEPS_NOT_READY；已记录文件消失/指纹不符 → DATA-STALE。 | 检查路径后重跑 |
| QS-IO-FILE-PERMISSION_DENIED | 来自实际 errno/error_code 的权限拒绝（不凭“文件存在但打不开”猜测权限）；message_dev 保留 category/value/system_message 与 path1/path2。 | 解除占用/授权 |
| QS-IO-FILE-CORRUPTED | 内容不完整或已损坏：model.peec **短 magic/短版本/短 endian 前缀**、badEndian、坏偏移、尾部多余字节、digest 不符；矩阵/legacy PORT或后处理文本的尾部垃圾或坏读；stage_state.json/post_source.json等JSON语法尾垃圾归JSON-INVALID；任意坏文本标识符无法识别；source_em.json 的 meshSha256 与 meshFile 不匹配。 | 重新生成上游 |
| QS-IO-FILE-MAGIC_BAD | **完整陌生 magic**（整段 magic 可读出但不属于已知容器族），与截断/短 header（CORRUPTED）区分。 | 检查输入来源 |
| QS-IO-FILE-VERSION_MISMATCH | ① PEECFS02 **完整 magic/version/endian 前缀读出且 endian 合法、version != 2**；② 支持族 PEEC_POST_MODEL_Vn / PEEC_POST_SOURCE_Vn 的**完整数字未来版本**。完整但版本不同的同族文件报此类，不是 CORRUPTED。 | 转换/升级后重新生成 |
| QS-IO-DIR-NOT_WRITABLE | 明确的目录可写性检查失败，且没有更具体的系统错误分类；实际权限错误优先使用 PERMISSION_DENIED。 | 检查目录与权限 |
| QS-IO-DISK-FULL | filesystem_error 明确报告 no_space_on_device；不用无法分辨原因的写失败冒充磁盘满。 | 释放空间后重跑 |
| QS-IO-FILE-WRITE_FAILED | 输出打开/flush/close/阶段状态/发布失败且无可靠系统原因证据。 | 检查磁盘与权限 |
| QS-IO-FILE-OPERATION_FAILED | filesystem_error 无可细分原因，保留系统 error_code 与路径。 | 查日志系统原因 |

### QS-FMT-*（格式/Schema/设置）

| code | 触发边界 | 恢复 |
| --- | --- | --- |
| QS-FMT-JSON-INVALID | 共享 JSON 解析器的语法拒绝（含字符串内非法 UTF-8；设置专用解析器的坏 JSON 归 SETTING_INVALID），message_dev 带文件与字节位置。 | 修 JSON |
| QS-FMT-JSON-SCHEMA_VIOLATION | 字段缺失、类型或未知键；重复JSON对象键是语法错误，归JSON-INVALID（设置专用解析器归SETTING-INVALID）。**nextId 只属于 matdb.json**（schema 3，必须整数且大于全部材料 id；等于最大 id 也违规）；source_em.json 出现 nextId/nextID 一律 Unknown field。source 另要求 ports 非空数组、triangleIds 为正 Gmsh tag（0 不是地节点；**-1 仅在启用地面时允许**）、direction 有限单位向量（仅元数据，端点顺序定义极性，不做方向反向检查）、meshFile 固定 model_em.msh。 | 按 message_dev 字段位置修正 |
| QS-FMT-MSH-INVALID | 共享网格读取器拒绝格式、元素类型或标签契约（缺 $MeshFormat、坏 marker/NUL 段等）；**MSH 版本不支持仍归此类**，不是新增 IO VERSION。 | 检查网格 |
| QS-FMT-MSH-PHYSICAL_TAG_MISSING | 标准网格**无 PEC Tri3 或缺材料 physical label**。CAD Line2仅作几何元数据，不作激励定义。 | 检查物理组 |
| QS-FMT-SETTING-INVALID | CLI 选项缺值/数值格式/范围/重复项/已移除配置项；**唯一公开设置为 Setting/set_em.json，root SET_EM.json旧入口拒绝**；标准缺 settings 亦报此类；缺显式数据根、格式或数值错误归此类。 | 按日志修正 |
| QS-FMT-SETTING-UNKNOWN_KEY | 共享设置解析器不认识键名。 | 检查拼写 |
| QS-FMT-INPUT-LEGACY_NAME_REJECTED | 检测到已禁止的旧设置/源路径，新旧并存同样拒绝；file_refs为真实旧路径，recoverable=true。 | 按上节转换到新名及位置 |
| QS-FMT-INPUT-AMBIGUOUS 【保留】 | 当前交付三程序无独立触发路径。 | — |
| QS-POST-PLOT-FAILED 【保留】 | 历史 S 参数 Plotter 外部绘图失败；v1.4.2 不交付 Plotter，当前三程序不触发。 | — |

### QS-VAL-*（取值/单位）

| code | 触发边界 | 恢复 |
| --- | --- | --- |
| QS-VAL-NUM-NAN_INF | 修正后 P/L 非有限、输入矩阵非有限、求解数值非有限（**非有限值**才归此类；fast diagnostics 中 NaN 是有意省略诊断，不是故障；**非正频率与 NaN 区分**，不混归）。 | 检查数值输入 |
| QS-VAL-OUT_OF_RANGE | 支路端点、扫描参数、PEC 地面几何、观测网格范围或缩放/地址计算越界。 | 修正设置 |
| QS-VAL-DIM-MISMATCH 【保留】 | 当前交付三程序无独立触发路径；实际文件形状错误按读取边界归 CORRUPTED/SCHEMA。 | — |
| QS-VAL-UNIT-MISMATCH | 当前具体为 **thicknessUnit 不受支持**；不宣称校验所有材料属性单位。 | 修材料 |
| QS-VAL-EMPTY-DATA 【保留】 | 当前交付三程序无独立触发路径。 | — |

### QS-MAT-*（材料）

| code | 触发边界 | 恢复 |
| --- | --- | --- |
| QS-MAT-CODE-NOT_FOUND | 网格 Tri3 材料标签未在材料库（matdb.json surface C）登记；file_refs 同时列 msh 与 matdb。 | 修材料库 |
| QS-MAT-MODEL-UNSUPPORTED | MATDB **非 surface C**、**非 constant 属性模型**、或 PEC 需 vacuum εr/μr 但值 != 1。正电导/厚度仅元数据按 PEC 计算，**不暗示正 σ 表面本身被拒**。 | 换支持的材料模型 |

### QS-BC-* / QS-SRC-*（端口/激励）

| code | 触发边界 | 恢复 |
| --- | --- | --- |
| QS-BC-PORT-FLOATING | 用户源引用的 triangle tag 不存在、不能映射到端子，或引用了尚未启用的参考地。 | 检查端口几何 |
| QS-BC-PORT-MAPPING_AMBIGUOUS | **surface terminal 部分重叠且非相同 node 集合/非法映射**；不再有 direction 反向检查（direction 仅单位向量元数据）。 | 调整端点/网格 |
| QS-BC-PORT-SHORTED | 用户源中双 ground 映射到同一实际节点，或两端同一三角形/同一电位节点；legacy PORT.txt 同节点属于损坏的派生记录，归 CORRUPTED。 | 调整端口 |
| QS-SRC-PORT-COUNT_MISMATCH | PORT 声明数量不正、缺行或多行。 | 重跑上游 |
| QS-SRC-REF_IMPEDANCE-MISMATCH | 端口阻抗非有限或非正（impedanceOhm 校验）。 | 检查用户源输入（check_input_files） |
| QS-SRC-SWEEP-EMPTY | 求解实际频率列表为空。 | 修设置 |

### QS-SOLV-*（求解）

| code | 触发边界 | 恢复 |
| --- | --- | --- |
| QS-SOLV-MATRIX-SINGULAR | LAPACK **最终正 info**（对称分解正 info 可重试 LU，不提前判失败）；频域系统不能求解。本版无 PN_DD/介质 RHS 物理。 | 检查端口和模型 |
| QS-SOLV-CONVERGENCE-FAILED | **有限但超过现有阈值**的后向误差：Full-MNA/KCL 保持 1e-10；Schur 原接受水平 1e-8；ground-image 积分比较原 5e-5。非有限值归 NAN_INF，不混入。 | 检查电路/调参 |
| QS-SOLV-MEMORY-OUT_OF_MEMORY | 模型读入后规划预算不能容纳共享数据和一名 worker（内存预算类），顶层捕获 bad_alloc，**或 LAPACK 负 info -1010/-1011（官方工作区/transpose 内存失败）**；OOM 下仍以固定完整 UTF-8 JSON 应急 trace（非空）。 | 减少网格规模 |
| QS-SOLV-MODE-UNSUPPORTED | **标准 Solver_SET=1 拒绝**（当前标准流程仅支持频域）；公开入口只接受标准布局，原legacy根不再作为输入入口。 | 改用频域 |

### QS-POST-*（后处理）

| code | 触发边界 | 恢复 |
| --- | --- | --- |
| QS-POST-FREQ-POINT_NOT_FOUND | POST_FREQ_INDEX 超出实际采样列表（1-based）。启用源导出时越界明确拒绝，不再静默钳位；**-1 表示最后频点仍允许**。 | 修设置 |
| QS-POST-PORT-OUT_OF_RANGE | POST_PORT 超出已提取端口范围（1-based），同样明确拒绝。 | 修设置 |
| QS-POST-RESULT-NOT_FOUND | 后处理需要的 post_source.json 或 legacy Post_Source.txt 不存在；三程序的该读取路径不消费 CSV。 | 重跑上游 |

### QS-COMM-*（依赖/一致性）

| code | 触发边界 | 恢复 |
| --- | --- | --- |
| QS-COMM-MODULE-BUSY | 同一dataRoot/provider的独占文件锁正被其他阶段持有；在业务输出与marker写入前拒绝，exit1。陈旧文件本身不构成忙碌。 | 等待正在运行的阶段退出后重试，不删除活跃锁 |
| QS-COMM-DEPS-NOT_READY | 缺上游必要 state/context、仅有半套 model/state、阶段索引缺失/未完成。同一 dataRoot 的调用应按 extract→solve→post 串行执行。 | 重跑上游 |
| QS-COMM-DATA-STALE | **已记录文件消失或指纹不符**：stage_state.json/standard_inputs.json/model_state.json 及 incomplete markers 所记录的输入/输出与当前文件不一致、原始四文件（msh/matdb/source/set）任一变化、执行期间输入变化。 | 重跑上游 |

### QS-INTERNAL-*（兜底）

| code | 触发边界 | 恢复 |
| --- | --- | --- |
| QS-INTERNAL-UNHANDLED_EXCEPTION | 未分类 std::exception/非标准异常兜底，severity=fatal，recoverable=false。 | 凭 trace_id 联系开发 |
| QS-INTERNAL-ASSERTION_FAILED | **真实使用**：内部数据契约断言，含 **LAPACK 其余负 info**（非 -1010/-1011）的参数契约错误。 | 同上 |
| QS-INTERNAL-LOG-NOT_WRITABLE | 无法追加 JSONL 日志，stderr 降级；通常以 warning 出现。 | 保留终端记录 |

## 文件交接与指针

- **原始文件（用户侧）**：Model/model_em.msh（用户网格）、Model/matdb.json（材料库，schema 3，拥有 nextId）、Model/source_em.json（用户源，不允许 nextId/nextID；surface 形式 groundPlane:true）、Setting/set_em.json（唯一公开设置文件）。
- **内部派生件（模块自有，按实际路径记录）**：本版强制内部交接为 **stage_state.json（schemaVersion 2）、standard_inputs.json、model_state.json 及 incomplete markers**。另有 Result/compact_em/extract/model.peec（PEECFS02/version2，schemaVersion 2）、native/SET_EM.json、native/port.json（仅内部派生名，不是用户入口）、COMMON_DATA/Port_Mapping.csv；solve 产出 COMMON_DATA/S|Y|Z_parameter.csv、Solver_Diagnostics.csv、post_source.json 及 Result/compact_em/solve/ 副本；post 产出 Source.msh（全部源量开启时 14 个 _real/_imag/_mag 视图）与 Field.msh（空间场开启时 6 个视图）。shared JSON 拥有原始文件和字段/数组位置（$.ports[i].xxx 形式定位），不使用 TLS 标签。
- **stage_receipt.json**：各阶段可选 v1 best-effort 自证，缺失、损坏或写失败不作为消费者前置条件，不阻塞计算；写失败发结构化 warning 并保持计算退出码。模块不读写 Result/tasks；内部状态含义按 current 实现，不复制旧 Compact 批次规则。

## 日志、warning/OOM/trace 与退出语义

- 主错误先写 stderr（完整 9 字段 JSON），随后追加同名 jsonl；**主错误已输出后，日志写失败只发 QS-INTERNAL-LOG-NOT_WRITABLE warning，不覆盖主错**（含日志序列化再分配失败的情形）。
- 可选 receipt 写失败同理：warning，不改变业务退出码。正常 OOM、报告 OOM、日志 secondary failure、optional warning 四类相互区分。
- **OOM 应急**：bad_alloc 归 QS-SOLV-MEMORY-OUT_OF_MEMORY（severity=fatal）；正常报告不可用时走固定 Win32 调用、零分配的应急路径，输出完整 UTF-8 JSON 应急记录（非空、控制符可见化），trace_id 降级为 qs-emergency-<pid>-<tick>。
- **退出码**：extract 成功 0、诊断失败 1、未知（非标准）异常 2；solve 成功 0、失败 1；post 成功 0、运行失败 1、**UsageError（unknown option 或缺 data-root 值）2**。platform Filter 层未知/重复/非法 export/runtime-dir 等并非全都 2，通常报 QS-FMT-SETTING-INVALID 且退出 1；manifest exitCodes：extract {0,1,2}、solve {0,1}、post {0,1,2}。

## 验证与适用边界

主线本次验证见 `E:/PEEC/Workspaces/compact_input_names_20261113/green_mainline/report.json`、`mainline_native_tests.log`和`mainline_final_validation2/report.json`；发行验证位于Compact交付的validation/input-names。新增真实CLI覆盖旧名/新旧共存、缺文件、JSON错误、原始file_refs、无参拒绝、self-test和拒绝前业务结果不变。此前1.4.1的错误契约验证保存在 `validation/error-contract`，是历史证据。

四项登记码仍为保留。宿主GDS/UI端到端及上轮docs1.6审计的其他问题不因本次文件名收窄而被判为通过。用户2026-11-13的新指示明确结束set.txt/旧port名称过渡，原九份docs附件不改写。
