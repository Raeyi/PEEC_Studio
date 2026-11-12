# REPACK / alpsrunner（1.1.2，2026-11-12）

> 长期滚动日志：本文件记录 alpsrunner 模块包的平台侧集成与收口过程。关联活契约：`DATA_LAYOUT.md`（v1.7）/ `MODULE_MANIFEST_SPEC.md`（v1.4）；thermal 联动见 `REPACK/thermal.md` 第 4 轮。模块包：`ALPSRunner_1.1.2.zip`（2026-11-10 用户交付）→ 平台 `tools/alpsrunner/`（完整包：ALPSRunner.exe + module.manifest.json 88 行 + README.md + ERROR_CODES.md）。

## 第 1 轮（2026-11-12 请求 → 回执同日）

状态：**已收口（同日 2026-11-12）** —— alpsrunner 1.1.2 作为 `tools/` 子模块集成完毕（完整包注册 + 平台双通道 OP0 导入 + thermal_basic 3 段链联动 + 文档同步）。

### 请求正文（2026-11-12，用户）

- ALPS 模块作为 `tools/` 下的子模块集成：`ALPSRunner_1.1.2.zip` 是 ALPS 模块，后续可作为任意一个 stage 或 pipeline 去调用。
- 热模块 1.0.9 联动：之前热模块读取的 op0 是写死的，现在运行热 pipeline 时，会调用上面 alps 模块动态去获取 op0。

### 模块契约（vendor 事实，`tools/alpsrunner/module.manifest.json` 88 行）

- 身份：`id=alpsrunner` / `version=1.1.2` / `apiVersion=1`（minApi 1）/ 单 EXE `ALPSRunner.exe`；唯一 stage `simulate`。
- errorContract `none`：不输出结构化 stderr；平台把非零退出统一归一为 `QS-TL-MOD-EXEC` 并保留原始 `exitCode`（退出码 0/2/3/4/5/124/130 摘要见 ERROR_CODES §3.5）。
- argsTemplate（完整形态）：`--input {dataRoot}/Model/ThermalNetList.sp` / `--output-dir {dataRoot}/Result/alpsrunner/simulate` / `--config {dataRoot}/Setting/ALPSRunner.json` / `--runtime-dir {runtimeDir}` / `--threads {override:--threads}`；allowedArgs `["--threads"]`。
- 3 个用户引用（均 `maintainedBy: ["ui"]`）：`user/alpsrunner.netlist` → `Model/ThermalNetList.sp`（.sp）/ `user/alpsrunner.config` → `Setting/ALPSRunner.json`（.json）/ `user/alpsrunner.key` → `Setting/alpsrunner.key`（.key）——"ALPS 三件"。dev 算例随案提交（用户确认 2026-11-12）；其中网表由 extract 阶段库 `publishes` 落位（alps 阶段固定输入，copy 语义）。
- `artifacts.produced=[]` + `producesInternal=["alpsrunner/run.bundle"]`：**无固定 OP0 产物、无 publishTo、不自动发布进 Model/Setting**（publicationPolicy 原文如此）——OP0 落位是平台导入步骤的责任（MODULE_MANIFEST_SPEC §6.7 平台责任）。
- `environment.alpsrunner_host`：version 1.1.0 / protocol 1 / **bundled false**（远端主机是运行前提，客户端 EXE 在 `tools/alpsrunner/`）/ requiredBy `["simulate"]`；requirements：已配置的 Windows SSH 服务器可达且其固定 RSA 主机指纹匹配 / 客户端公钥已授权用于固定 ALPSRunnerHost 命令 / IC618 运行中且 VMware Tools、ALPS 与其 license 可用。
- `runtimePolicy`（原文）：本交付 profile 要求输出与 runtime 目录位于同一 dataRoot 下的同一实际卷，不得有 junction 或符号链接；原生跨卷兼容回退不在本 profile 之内。`resultPolicy`：每次调用在 `Result/alpsrunner/simulate` 下创建新的时间戳+ID 目录；`alpsrunner/run.bundle` 是内部结果集，不是固定的对外产物路径。
- manifest `optional: false` → 平台阶段库覆写为 `optional: true`（ALPS 三件缺失 → `QS-TL-SKIP-NOINPUT`；2026-11-12 起可选上游跳过/取消不连带阻断下游）。

### 远端契约

- 主机：`192.168.136.172:22`（用户 `2208`，publickey 认证；固定指纹 `SHA256:rTaJqy2IpnFv0GpHZYWaQxzDyhF38AI34Vca6UbkWwg`）；`Setting/ALPSRunner.json` schema 2（ssh + execution 段）。
- 时限预算：timeout 3600 s / 轮询 2 s / 连接 15 s。
- 远端主机需具备 ALPSRunnerHost 1.1.0 + 公钥授权 + IC618（VMware Tools + ALPS license）（即上条 environment requirements）。**2026-11-12 实测：TCP 可达**；真远端全链路在统一 v2.5.0_rc1 交付轮执行（3600 s 预算）。

### 退出码表（包内 ERROR_CODES.md 逐字）

`ERROR_CODES.md` 开头声明：本模块使用进程退出码，不输出 `qs-json-stderr` 结构化错误行；manifest 的 `errorContract.type` 为 `none`；依据 MODULE_MANIFEST_SPEC，平台将非零退出归一为 `QS-TL-MOD-EXEC`，并保留原始 `exitCode`。以下数字不是另外一套 QS 字符串码，不能把普通 stderr 文本伪装成结构化 QS 错误。

| 退出码 | 平台归一码 | 触发条件 | 可恢复性与处置 |
| --- | --- | --- | --- |
| `0` | 无错误 | ALPS 仿真成功，结果已回传并通过完整性校验 | 可使用本次结果；仍应保存任务目录和摘要，区分不同运行记录。 |
| `2` | `QS-TL-MOD-EXEC` | 参数缺失、重复或无效；输入不是可接受的自包含 `.sp`；JSON、私钥路径或显式 runtime 路径无效；运行暂存预检失败 | 修正路径、权限和配置后重新运行。此类提交前错误可能没有结果任务目录；不能依赖摘要总是存在。 |
| `3` | `QS-TL-MOD-EXEC` | 凭据/主机指纹/网络/SSH/VMware Tools 等提交或通信问题 | 核实已授权公钥、服务器指纹及服务状态。若已尝试启动远端任务，先检查摘要中的 `simulation_status` 和远端目录，再决定是否重试；不要因连接断开就认定 ALPS 已停止。 |
| `4` | `QS-TL-MOD-EXEC` | ALPS 仿真失败，或远端结果收集失败 | 查看 `console.log`、ALPS 原始日志和摘要，修正网表或远端环境后重新运行。`result_scope=logs_only` 只表示诊断回传，不是完整结果。 |
| `5` | `QS-TL-MOD-EXEC` | 下载、解压、结果大小/路径/CRC/SHA-256 校验或发布失败 | 保留当前本机和远端证据，检查网络、磁盘和权限；不能把部分文件当成成功结果。不要覆盖或手工拼接校验失败的结果。 |
| `124` | `QS-TL-MOD-EXEC` | 仿真或完成阶段达到时限 | 查看摘要中的超时来源及远端状态，确认旧任务状态后再调整合理时限。上传、收集和回传时间不等同于仿真时限。 |
| `130` | `QS-TL-MOD-EXEC` | 用户发起正常取消 | 程序请求停止本次任务，并尽量回传已有日志。确认摘要状态后再启动新任务；不要删除运行中的任务目录或终止其他人的 ALPS。 |

补充（同文件，逐字）：

- CLI 的早期输入错误、进程被外部强制结束、操作系统启动失败等情况，可能没有完整的 `run_summary.json`。调用方必须同时读取进程退出状态、stdout 和 stderr，不能只检查结果文件是否存在。
- `run_summary.json` 的 `status` 是 ALPSRunner 本次调用的整体结果；`simulation_status` 和 `download_status` 分别说明仿真与回传状态。`simulation_status=unknown` 表示远端完成状态尚未确认，不代表仿真已失败或停止。任务 ID、已知远端路径及恢复说明用于人工诊断，不是 task_layer 的调度状态源。
- 平台调度状态以 task_layer 自己的 `Result/tasks/<taskId>/state.json` 为准。本模块不读取或写入该目录，也不自行写入 `Model/`、`Setting/` 的正式件。

### run bundle 布局

- `Result/alpsrunner/simulate/<runId>\`（根）：`console.log` / `run_summary.json` / `SHA256SUMS.txt` / `<netlist>.sp` / `<netlist>.sp.sim`
- `<netlist>.sp.sim\`：`.op0` / `.dcop0` / `.ic0` / `.log` / `.warn` / `.thread`（可选 `.profile`）
- `run_summary.json` 8 字段门（平台导入步骤）：`schema_version==1` + `run_id` 非空 + `run_id==bundle 目录名` + `input_sha256` 非空 + `status=="succeeded"` + `result_scope=="full"` + `simulator_exit_code==0` + `download_status=="verified"`。

### 平台导入（双通道；OP0 落位 = 平台职责）

- **流水线形态**：阶段库 `op0Import: {"to": "Result/thermal/alps/thermal.op0"}` —— 阶段模块 exit 0 后，平台在 `TaskOrchestrator::onStageFinished` 执行导入：run bundle 前后差集（恰 1 个新 run 目录）→ run_summary 8 字段门 → `SHA256SUMS.txt` 逐条复核 → I7 三向新鲜度守卫（run_summary.input_sha256 = 现 `Model/ThermalNetList.sp` sha = `thermal_binding.json.netlistSha256`，否则 `QS-TL-ALPS-STALE`）。
- **独立通道**：JSON-RPC `alps.importOp0`（params `{dataRoot, runDir, op0Dest?}`）+ CLI `task_layer.exe alps-import-op0 --data-root <dir> --run-dir <name> [--op0-dest <rel>]` —— 与流水线形态同一 `importFromRunDir` 实现（`task_layer/src/io/AlpsImport.{h,cpp}`）。
- **成功**：OP0 落位 + 审计件 `import_receipt.json`（8 字段：schemaVersion 1 / runId / srcRel / op0Dest / op0Sha256 / inputSha256 / importedAt / source ∈ {pipeline, manual}）写于 **OP0 同目录**（默认 `Result/thermal/alps/import_receipt.json`）。
- **失败**：6 个 `QS-TL-ALPS-*` 码（`NORUN / MULTIRUN / BADSUMMARY / BADSHA / STALE / IO`，ERROR_CODES §2.8）——流水线形态该阶段置 Failed、下游阻断；独立通道 -32005 + `data.errorCode`。
- **thermal post 读取**：`Result/thermal/alps/thermal.op0`（1.0.9 argsTemplate 内置 `--op0`，显式可覆盖，**无旧路径回退**）。D1a：task_layer 写 / thermal post 读。

### E2E 实测（离线形态）

- ALPS smoke 22/22（fake EXE + vendor 参考 run bundle，离线）：导入门（BADSUMMARY / BADSHA / STALE / NORUN / MULTIRUN / IO）+ 双通道返回契约 + 流水线 run bundle 差集。
- 离线 3 段链 live 15/15（新 dataRoot、无 ALPS 输入）：extract 网表与 vendor 参考逐字节一致 `7da064302ead1850…`（1,597,236 B）→ `publishes` 落位 → alps 跳过 `QS-TL-SKIP-NOINPUT`（不连带阻断）→ post 预置工作点运行：6094 行 / 293.15–500 K / max 500 K = 226.85 °C / 3 处就近填补（T65/T68/T3790，CSV `value_source` 列标记）。
- 真远端形态：留统一交付轮（2026-11-12 已实测 TCP 可达）。

### 归档标记

- `alpsrunner.key` 直接提交入 git = 用户决策（2026-11-12），覆盖 vendor README 的"不要放进源码库"提示（仅限 dev 算例范围）。
- vendor sha 事实：参考 OP0 `bbd8fb94d7f785ab…`（6,954,809 B）/ 参考网表 `3038e696…` / `run_summary.archive_sha256` `9bbf757e…`（≠ OP0 sha；archive 为 run bundle 归档件）；dev 预置工作点 `fd0f79098ff8d529…`（5,278,434 B）位于 `Result/thermal/alps/thermal.op0`。
