# ALPSRunner 1.1.2 错误与恢复

本模块使用进程退出码，不输出 `qs-json-stderr` 结构化错误行。manifest 的 `errorContract.type` 为 `none`；依据 MODULE_MANIFEST_SPEC，平台将非零退出归一为 `QS-TL-MOD-EXEC`，并保留原始 `exitCode`。以下数字不是另外一套 QS 字符串码，不能把普通 stderr 文本伪装成结构化 QS 错误。

| 退出码 | 平台归一码 | 触发条件 | 可恢复性与处置 |
| --- | --- | --- | --- |
| `0` | 无错误 | ALPS 仿真成功，结果已回传并通过完整性校验 | 可使用本次结果；仍应保存任务目录和摘要，区分不同运行记录。 |
| `2` | `QS-TL-MOD-EXEC` | 参数缺失、重复或无效；输入不是可接受的自包含 `.sp`；JSON、私钥路径或显式 runtime 路径无效；运行暂存预检失败 | 修正路径、权限和配置后重新运行。此类提交前错误可能没有结果任务目录；不能依赖摘要总是存在。 |
| `3` | `QS-TL-MOD-EXEC` | 凭据/主机指纹/网络/SSH/VMware Tools 等提交或通信问题 | 核实已授权公钥、服务器指纹及服务状态。若已尝试启动远端任务，先检查摘要中的 `simulation_status` 和远端目录，再决定是否重试；不要因连接断开就认定 ALPS 已停止。 |
| `4` | `QS-TL-MOD-EXEC` | ALPS 仿真失败，或远端结果收集失败 | 查看 `console.log`、ALPS 原始日志和摘要，修正网表或远端环境后重新运行。`result_scope=logs_only` 只表示诊断回传，不是完整结果。 |
| `5` | `QS-TL-MOD-EXEC` | 下载、解压、结果大小/路径/CRC/SHA-256 校验或发布失败 | 保留当前本机和远端证据，检查网络、磁盘和权限；不能把部分文件当成成功结果。不要覆盖或手工拼接校验失败的结果。 |
| `124` | `QS-TL-MOD-EXEC` | 仿真或完成阶段达到时限 | 查看摘要中的超时来源及远端状态，确认旧任务状态后再调整合理时限。上传、收集和回传时间不等同于仿真时限。 |
| `130` | `QS-TL-MOD-EXEC` | 用户发起正常取消 | 程序请求停止本次任务，并尽量回传已有日志。确认摘要状态后再启动新任务；不要删除运行中的任务目录或终止其他人的 ALPS。 |

CLI 的早期输入错误、进程被外部强制结束、操作系统启动失败等情况，可能没有完整的 `run_summary.json`。调用方必须同时读取进程退出状态、stdout 和 stderr，不能只检查结果文件是否存在。

`run_summary.json` 的 `status` 是 ALPSRunner 本次调用的整体结果；`simulation_status` 和 `download_status` 分别说明仿真与回传状态。`simulation_status=unknown` 表示远端完成状态尚未确认，不代表仿真已失败或停止。任务 ID、已知远端路径及恢复说明用于人工诊断，不是 task_layer 的调度状态源。

平台调度状态以 task_layer 自己的 `Result/tasks/<taskId>/state.json` 为准。本模块不读取或写入该目录，也不自行写入 `Model/`、`Setting/` 的正式件。
