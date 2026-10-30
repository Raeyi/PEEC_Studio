# task_layer 代码审查（2026-10-30）

审查方式：agent-skills/code-review-and-quality 多轴静态审查（正确性 / 健壮性 / 安全 / 可维护性 / 测试），逐文件复核 `task_layer/src/*`、`worker_host/main.cpp`、`tools/`。
基线：`CODE_REVIEW_20261027.md`（A1-A3 编译阻断 + B1-B17 逻辑缺陷已修复并验证）。本次**未发现新的必现级缺陷**；以下为按严重度分级的新发现与遗留项。

结论：**可以合入**。无 P0/P1 阻断项；P2 建议在下个迭代处理，P3 为卫生项。

---

## P1（高：特定条件下会出错，建议尽快修）

### N1. 任务结束后的迟到日志会串到"下一个任务"的 task.log
位置：`App::onNotify`（src/App.cpp，`task.stage.log` 分支）+ `ResultStore::appendLog`。

`m_store` 是单例字段：上一个任务的 runner 若在 `task.finished` 之后还有残余输出回调（例如取消时 kill 后管道里残留的行排队到达），此时 `m_activeTaskId` 已清空、`m_lastTaskDir` 仍指向旧目录——但**下一次** `startTaskWith` 会 `m_store.open()` 到新任务目录，而迟到的旧日志行若恰好落在两次 open 之间，会被追加进**尚未写完的新 state.json 所在目录**的 task.log，造成跨任务日志污染。

当前触发窗口很小（kill 后 readAll 已在 onProcessFinished 收尾），但取消路径（`clearRunners` 在 `finishTask` 内先 cancel 再 deleteLater）存在理论窗口。
建议：`appendLog` 前校验 `params.taskId == m_taskId`（ResultStore 已持有 taskId），不匹配则丢弃或写 `<taskId>.late.log`。

### N2. `PathResolver::resolve` 对动态变量值中的 `{VAR}` 二次展开可能意外替换
位置：`config/PipelineLoader.cpp` `PathResolver::resolve`。

`setInputs` 把 UI 传入的值原样存入 `m_extraVars`，`resolve` 先展开动态变量再展开固定变量。若 UI 传的值本身含有 `{DATA}` 之类的字面文本（例如用户把带占位符的路径当值传入），会被静默替换。更实际的风险：两个动态键互为前缀时（如 `portJson` 与 `portJsonX`），`QString::replace` 按子串替换会互相污染。
当前 inputs 键只有 portJson/setFile/fs/fe/nFp，未触发；但这是协议开放给 UI 的扩展点。
建议：键集合确定后按长度降序替换，或对值做一次性展开（值的内部不再展开）。

## P2（中：设计层面，已有记录，本轮确认仍未关闭）

沿用 2026-10-27 审查第四节，本轮复核状态：

| # | 事项 | 本轮复核 |
|---|---|---|
| D1 | layerRead/layerWrite 只标注不校验 | 未变。`ModuleRunner::writeJobJson` 会把层信息传给 DLL，但调度前仍无"上游写层 ⊇ 下游读层"检查 |
| D2 | overrides 无白名单 | 未变。`TaskOrchestrator::launchStage` 直接把 `overrides[stageId]` 拼成 `--key value`；UI 传错键会静默变成模块参数甚至改变行为。建议 modules.json 增加 `allowedArgs` |
| D3 | 阶段日志无背压 | 未变。`task.stage.log` 全量逐行推送；大算例单阶段数千行，UI 侧需自行限流（UI_API.md §九已提示） |
| D4 | 取消硬杀进程可能留 .partial | 未变。compact_em 的 `.peec-data.lock` / IN_PROGRESS 状态在 kill 后不清理 |
| D5 | 单任务串行 | 未变。`m_busy` 限制；优化循环场景需要任务队列 |
| D6 | QS-TL 错误码登记 | **本轮已关闭**：`task_layer/ERROR_CODES.md` v0.1 落地（含透传规则与新增流程约定） |

## P3（低：卫生 / 一致性）

### N3. `ModuleRunner::sha256HexOfFile` 与 `InputValidator::sha256OfFile` 重复实现
两处 SHA-256 文件哈希逻辑不一致（前者 64KB 分块、后者 `addData(&f)` 整读）。建议收敛到 `InputValidator` 一处。

### N4. `decodeModuleText` 的回退判定过于敏感
任一替换字符即整体回退本地代码页。UTF-8 输出中混入一个非法字节（后端打印了非 UTF-8 符号）会导致**整段**按 GBK 解码，中文反而乱码。建议按行判定而非整块。

### N5. `extractErrorCode` 取"最后一条" QS 码的假设
注释已说明（warning 在前 error 在后），但若模块在失败后又输出了 warning（含不同 code），会误报。建议优先取 severity=error/fatal 的最后一条 JSON 记录，兜底再取任意最后一条。

### N6. `StdinReader::run` 用 `QFile(stdin)` 阻塞读
可行且简单，但 `readLine` 对超长行（> 默认缓冲）的行为依赖 Qt 版本；JSON-RPC 请求行理论上无上限。当前 UI 请求都很短，风险低；可在文档中约定单行上限。

### N7. `main.cpp` CLI `run` 的 overrides 解析
`--k v` 被拆成 positional args 后用 `=` 拼接（`kv.mid(1)` 要求 `key=value` 形式），与 serve 模式的 `overrides.{stageId}.{key}` 语义不一致（CLI 版没有 stageId 维度，所有 kv 会应用到每个阶段）。README 未说明该差异，容易误用。建议 CLI run 直接不支持 overrides，或改为 `--stage id --k v`。

### N8. `worker_host` 退出码语义
DLL 返回 0/非 0 直接透传，非 0 时任务层统一记 `QS-TL-MOD-EXEC`。与 external 一致，OK；但 worker_host 自身的启动失败（缺参/DLL 加载失败）返回 2/3，也会被归为 `QS-TL-MOD-EXEC`，无法区分"宿主没起来"和"模块跑挂了"。建议宿主自身失败用独立退出码段（如 12x）并在任务层映射为 `QS-TL-WORKER-NOTFOUND` 类错误。

## 正面确认（本轮重点核对过的关键点）

1. **pump 重入保护**（PumpGuard + launchStage 先出队）：同步 emit 路径全部覆盖，无二次启动窗口。
2. **取消收尾**：pending 清空、running 逐个 kill、`m_busy` 复位路径完整；`onStageFinished` 有 `if (!m_busy) return` 防迟到回调改状态。
3. **编码链路**：stdout `_O_BINARY` + UTF-8 字节直写；模块输出 UTF-8 优先/GBK 回退；job JSON 全 UTF-8；worker_host 传参 `toUtf8()`（B10 修复保持有效）。
4. **错误码提取**：滚动 32KB tail 保证失败前的 stderr JSON 不丢（B9 修复保持有效）。
5. **state.json**：finishedAt 仅终态写入（B17 修复保持有效）；`task.status/task.result` 支持 lastTask 回退（B4）。
6. **DAG 校验**：id 唯一、依赖存在、三色判环，配置错误在 load 期拦截而非运行期悬挂。
7. **handoff 不变式**：Model/ 单网格、port.json 指纹刷新、DIM=1000（mm）均正确；同路径 port.json 先关句柄再覆写的细节处理到位。

## 测试缺口（本轮无自动化测试可跑）

- 机器上无 Python，`tools/rpc_smoke.py` 未执行；建议在有 Python 的环境补一轮 serve 模式冒烟（ping → modules.list → inputs.validate → task.run 最小流水线 → task.cancel）。
- 取消竞态（N1 窗口）建议加一个"取消后立即起新任务"的手动回归用例。
- gds_full 全链路（gds2geo 四阶段 + handoff + peec_basic）上次已跑通，本轮代码未触及该链路，无需重跑。
