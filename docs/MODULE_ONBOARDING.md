# 模块接入手册（ONBOARDING）

版本 v1.0 · 2026-10-30 · 读者：各模块团队负责人。
本手册是"新模块/新版本接入 PEEC Studio 平台"的操作清单。前置阅读：`DATA_LAYOUT.md`（路径规范）、`MODULE_MANIFEST_SPEC.md`（manifest 规范）。

---

## 一、交付四件套

每个模块（或每个不兼容版本）交付以下文件，放在模块包根目录：

| # | 文件 | 要求 |
|---|---|---|
| 1 | `*.exe`（+ 必要随附程序） | 静态链接运行库；安装目录只读假设（R3）；CLI 满足 R1-R3 |
| 2 | `README.md` | 调用方式、全部 CLI 参数与退出码、本地自测方法、依赖（如 Gmsh 版本） |
| 3 | `ERROR_CODES.md` | 本模块 QS 错误码登记：code / 触发条件 / 可恢复性 / 用户处置；格式对齐《QS-PEEC 错误码与异常处理规范》 |
| 4 | `module.manifest.json` | 按 MANIFEST_SPEC 书写；**模块团队维护，task_layer 不代写** |

可选：`examples/`（只读输入样品，供人工参考与 dev_case 脚本取用）、`templates/`（defaultTemplate 引用的模板文件）。

**不再接受**：包内可读写 `DATA/`（见 §三迁移要求）。

## 二、CLI 合规自查（R1-R3）

- [ ] `--data-root <dir>` 必填，相对路径按 dataRoot 解析
- [ ] 无"EXE 旁 input//output/ 缺省回退"类行为
- [ ] 日志/私有锁/临时文件全部落在 `--runtime-dir`（或固定 `Result/logs/<providerId>/`）
- [ ] 只读安装目录下完整跑通一次最小算例

## 三、去 DATA 化迁移（现有模块必做）

1. 包内 `DATA/Model/` 的**输入样品** → 移到 `examples/`（只留 model.gds + model_info.csv 这类输入件）；
2. `DATA/Result/`、`DATA/runtime/` 的**历史产物与日志** → 直接删除（不属于交付物）;
3. 代码中所有按 EXE 目录解析数据路径的分支 → 改为按 `--data-root`；
4. 输出位置从旧布局迁到 `Result/<providerId>/<stageId>/`（对照 DATA_LAYOUT §二目录树逐项改）。

以 gds2geo 为例的具体动作：
- `--data-root` 统一为四个 exe 的必填参数（当前 source 阶段用散参数 --gds/--tech，需收敛）；
- 废弃"缺设置文件时回退 EXE 旁 input//output/"的最后残留逻辑；
- 四个阶段的 `--output-dir` 默认值/文档示例改为 `Result/gds2geo/<stage>/`；
- 锁文件位置随 runtime-dir 规则迁移到 `<dataRoot>/.locks/`。

以 compact_em 为例的具体动作：
- 输出位置从 `Result/model.peec`、`Result/S_parameter.csv` 等扁平路径改为 `Result/compact_em/<stage>/...`；
- 阶段状态文件统一命名 `state.json`；
- trace jsonl 落点统一 `Result/logs/compact_em/`；
- 包内 `DATA/` 降级为 `examples/`。

## 四、验收标准（task_layer 侧会执行的检查）

### 部署期（注册时，失败拒绝启动）
1. manifest 语法与 schemaVersion 合法；
2. 消费 ref 有唯一生产者、DAG 无环、apiVersion 窗口相交；
3. produced path 全部在本模块 `Result/<id>/` 内（publishTo 仅 Model//Setting/）；
4. argsTemplate 含 `{dataRoot}`、无他模块目录名字面量；
5. 包内残留可读写 `DATA/` → 警告（首个版本周期后升为拒绝）。

### 运行前（inputs.validate）
6. 入口工件（user/* 及上游缺失项）存在且非空；
7. port.json 绑定校验（meshFile 存在、sha 匹配）。

### 运行后（stage.finished）
8. 产出工件存在且非空（manifest.produced）；
9. 退出码按 errorContract 归一为 errorCode；
10. **最小算例回归**：接入方自行跑通一条完整链路并保留日志（task_layer 提供 dev_case 工具，见 §五）。

### 代码审查项（人工）
11. 源码中不出现其他模块 providerId 字符串（grep 检查）；
12. ERROR_CODES.md 覆盖 exitCodes 表声明的全部失败路径。

## 五、本地自测工具（dev_case）

模块去掉包内 DATA 后，日常开发用统一工具生成干净测试环境（本地与生产目录语义一致）：

```powershell
# 在 task_layer/tools/ 下（S2 交付）
.\dev_case.ps1 -Module gds2geo -Stage mesh_em [-CaseDir D:\dev\mycase] [-Clean]
```

行为：
1. 在 `%TEMP%\peec_dev\<module>\`（或指定 CaseDir）生成 dataRoot 骨架（Model/Setting/Result/.locks）；
2. 从模块包 `examples/` 复制输入样品到 `Model/`；
3. 调 `task_layer.exe run` 跑该模块的单阶段流水线（上游阶段产物若缺失会明确报出，提示先跑上游 stage）；
4. 结束后保留现场；`-Clean` 删除临时目录。

原则：**模块本地跑的目录结构 = 生产环境 dataRoot 布局**，"本地能跑集成挂"从结构上消除。

## 六、接入流程（时间线）

```
模块团队                          平台方（task_layer）
─────────────────────────────────────────────────────
1. 按本手册改造 CLI + 去 DATA 化
2. 编写四件套（manifest 可参照 SPEC §九示例）
        │
        ▼
3. 提交交付包 ──────────────────►  4. 放入 <MODULE_ROOT>/<模块目录>/
                                      5. modules.json 加路由条目（3 行）
                                      6. task_layer 重启 → 注册期校验
        ◄──────────────────────────   7. 反馈校验结果（明细到字段）
8. 修正（如有）→ 重交
        │
        ▼
9. dev_case 最小算例回归通过 ─────► 10. 全链路联调（UI 四步流程）
                                      11. 打发布包，modules.list 透出新版本
```

全程**不需要 task_layer 侧写任何模块适配代码**——这是本规范的验收底线。

## 七、常见问题

**Q：我们的模块要读 UI 生成的 port.json，但 UI 还没写好怎么办？**
A：port.json 属于 `em/ports.def`（maintainedBy: ui）。你的模块在运行时深检并报自己的 QS 码（如 QS-FMT-JSON-SCHEMA_VIOLATION）；任务层 inputs.validate 会在入口预检拦截"明显缺失"的情况。两者不冲突。

**Q：想新增一个全局共享目录（不在 DATA_LAYOUT 里）？**
A：走布局规范变更流程（DATA_LAYOUT 版本 +1），不要私自在 dataRoot 下建目录。模块私有需求一律放 `Result/<providerId>/` 自由区。

**Q：我们的产物 schema 要小改（加个可选字段），要升 apiVersion 吗？**
A：不加不升（向后兼容）；删字段/改语义才升。拿不准时升——兼容窗口机制就是为这种保守策略设计的。

**Q：多个可执行文件共享一个内部库，版本怎么记？**
A：manifest.version 记整个交付批次；内部库版本写进 README。apiVersion 只看对外行为（CLI/工件/退出码）。
