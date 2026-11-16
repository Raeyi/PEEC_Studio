# GDS2GEO 1.3.0_fix 退出码与恢复

本批只修订 manifest；原可执行版本、退出码与恢复行为保持。mshfix 的 plan/heal/verify 在 manifest 分别为 mshfix_plan/mshfix_heal/mshfix_verify。

记录可执行文件、版本/SHA、参数、工作目录、进程退出状态、实际退出码和完整 stdout/stderr。先判断进程是否成功启动和正常退出，再解释模块退出码；stderr 非空不等于失败。退出码优先于残留文件和 JSON 中的 success 字段。

## 三个主程序

| 退出码 | 程序与触发条件 | 可恢复性与处置 |
| --- | --- | --- |
| 0 | 所选命令成功；帮助/版本查询也返回 0 | 业务调用还须核对本次请求全部正式件；both 须同时核对两侧。等待进程结束后再读取 |
| 2 | GDS2GEO：参数/路径/GDS/工艺初检不合法，包括直出误传 `--runtime-dir`；GEO2MSH：目标、输入包、单位/摘要初检不符；GEOEDIT：请求格式或输入不合法 | 可修正后重试；保持三件配套，受损模型从源输入重建，不手改摘要或删除单位声明 |
| 5 | GDS2GEO：几何/材料/接触处理、互斥或发布失败；GEO2MSH：Gmsh、网格/材料/单位验证、互斥或发布失败；GEOEDIT：Gmsh、几何关系、互斥、保存或恢复失败 | 按第一条诊断检查输入、Gmsh 4.11.1、资源、权限及目录占用；保留旧结果、journal 和备份，修正后重试。无法可靠判定的几何关系不能强行继续 |
| 6 | GDS2GEO 捕获未分类标准异常；GEO2MSH `export-thermal-entities` 捕获未分类标准异常。常规目标网格失败为 5；GEOEDIT 不使用 6 | 保留复现输入、版本/SHA 与完整诊断，交维护方处理 |

GDS2GEO 直出使用系统临时目录，GEOEDIT 私有工作暂存也不写安装目录；输出发布可能存在必要事务文件。GEO2MSH 传 `--runtime-dir <dataRoot>/Result/logs/gds2geo/`，由宿主逐次留存诊断。系统命名互斥保护共享目录，不生成可供手动删除的锁文件。

几何输出须包含 GEO、同主名 JSON、MATDB；网格输出须包含 MSH 和本目标 MATDB。已有旧网格回执或残缺包时，保留原目录另选空目录。GEOEDIT 异常留下 `.geoedit-transaction.json` 和备份时，让下次相同输出目录调用恢复；不要手动删改。后续网格失败不使此前已成功几何失效。

## mshcheck 0.23.0

| 退出码 | 触发条件 | 可恢复性与处置 |
| --- | --- | --- |
| 0 | 检测完成，包括发现问题 | 读取成功 JSON 的 `summary.issueCount`、`summary.topologyIssueCount`；两者为 0 也仅表示当前检测范围内未发现问题 |
| 2 | 参数、便携配置或路径约束错误 | 改用显式绝对路径并修正参数；不使用无参 DATA 模式 |
| 3 | 网格读取或检测失败 | 检查支持的 MSH ASCII 格式和三角表面，保留原输入；无法解析时不能推断网格无问题 |
| 4 | 检测 JSON、高亮 MSH/OPT 或其他输出失败 | 检查输出目录权限/空间；停止后续修复。先写出的 JSON 可能仍为 success:true，不能覆盖非零退出码 |

成功检测 JSON schema 1.5；进入读取/检测错误处理时可产生 schema 1.1 的错误 JSON，参数错误不保证有 JSON。高亮 MSH/OPT 是查看副本；JSON/MSH/OPT 不构成整体事务，不将其中任一残留件当成本轮成功。

## mshfix 1.5.0

| 退出码 | 触发条件 | 可恢复性与处置 |
| --- | --- | --- |
| 0 | 本次 plan/heal/verify/capabilities 子命令成功 | plan 后检查 unsupported；heal 后还需独立 verify 成功，再接收新网格 |
| 2 | 参数、路径、计划内容错误，包括显式 `no_connect_actions` | 修正参数/重建计划；无改动计划保留原网格，不调用 heal 做空操作 |
| 3 | 输入、schema、哈希或来源绑定不匹配 | 恢复原配对输入，重新检测和计划。当前带 `$GDS2GEOUnits` 或 Tet4 输入不在支持范围，不删除声明绕过 |
| 4 | Gmsh 几何运算或网格生成失败 | 核对匹配的 4.11.1 DLL、原 GEO 和资源；保存日志，不能把局部输出当成功 |
| 5 | 当前不支持安全修复 | 展示计划/错误中的原因，保留原网格；不是删除 unsupported 项后重试的信号 |
| 6 | 后验或独立验证失败 | 不接收修复结果；保存网格与报告用于诊断。verify 不会回滚已发布的 mesh/report |
| 7 | 输出、事务冲突、发布或恢复失败 | 检查权限/空间/占用，保留 journal/backup；按原 mesh/report 路径对重试恢复，不预删证据 |
| 8 | Detector 定位、版本、启动、执行或输出失败 | 显式 `--detector` 指向本包 mshcheck 0.23.0，保留其输出，修正后从有效输入重新运行 |

统一错误处理的 stdout 可为 schema 1.0：`success:false`，`error.code/message/exitCode` 给具体原因。这是模块自身 JSON，不是 `qs-json-stderr`。Windows 在启动前直接加载 `gmsh-4.11.dll`；DLL 缺失、架构不符、进程崩溃或通道错误时，可能没有模块退出码/JSON，应记录操作系统启动错误。

把 DLL 放在平台 `tools/gmsh/` 后，宿主必须在启动前将该目录的绝对路径前置到**子进程 PATH**。`GDS2GEO_GMSH_EXECUTABLE` 只选择 gmsh.exe，不能解决 DLL 加载。不要把 Windows 启动失败误归为上述业务退出码 4。

## 平台 QS 归一与验收

manifest 和辅助合同使用 `errorContract.type: "none"`，程序不直接发出 QS stderr JSON。按 docs_1.8，平台可将模块正常启动后的非零退出码归一为 `QS-TL-MOD-EXEC` 并保留原 exitCode、模块诊断和 stdout JSON。启动失败、异常终止、取消及 publisher 失败由平台分别登记。

平台只在各命令成功并满足其结果合同后继续。检测发现问题不自动等于程序失败；修复计划 supported 不保证执行成功；`heal` 成功不等于 `verify` 成功；表面检测/修复成功不证明体网格或求解器全链通过。MSH 网格和报告绑定必须保留，换源文件/检测器或移动导致路径绑定变化时重新生成相应报告。
