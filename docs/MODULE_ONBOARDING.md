# 模块接入手册（ONBOARDING）

v1.0 · 2026-10-30 · 读者：模块团队负责人。前置阅读：DATA_LAYOUT.md、MODULE_MANIFEST_SPEC.md。

## 一、交付四件套

| # | 文件 | 要求 |
|---|---|---|
| 1 | `*.exe`（+ 必要随附程序） | 静态链接；安装目录只读（R3）；CLI 满足 R1-R3 |
| 2 | `README.md` | 调用方式、全部 CLI 参数与退出码、本地自测方法、依赖（如 Gmsh 版本）、内部库版本 |
| 3 | `ERROR_CODES.md` | QS 错误码登记：code / 触发条件 / 可恢复性 / 处置 |
| 4 | `module.manifest.json` | 按 MANIFEST_SPEC 书写，模块团队维护 |

可选：`examples/`（只读输入样品，供 dev_case 自测取用）、`templates/`（defaultTemplate 模板）。
**不再接受包内可读写 `DATA/`。** 包内不含 UI 输入文件——UI 选取的输入直接写入算例 `<dataRoot>\Model\`。

## 二、CLI 合规自查

- [ ] `--data-root <dir>` 必填，相对路径按 dataRoot 解析
- [ ] 无"EXE 旁 input//output/ 缺省回退"行为
- [ ] 日志/私有锁/临时文件落 `--runtime-dir`（或固定 `Result/logs/<providerId>/`）
- [ ] 只读安装目录下跑通一次最小算例

## 三、去 DATA 化迁移（现有模块必做）

通用动作：
1. 包内 `DATA/Model/` 输入样品 → 移到 `examples/`（只留输入件）；
2. `DATA/Result/`、`DATA/runtime/` 历史产物与日志 → 删除（不属交付物）；
3. 按 EXE 目录解析数据路径的代码分支 → 改按 `--data-root`；
4. 输出位置迁到 `Result/<providerId>/<stageId>/`（对照 DATA_LAYOUT §二逐项改）。

gds2geo 具体项：
- 四个 exe 统一 `--data-root` 必填（source 阶段散参数 --gds/--tech 收敛为按 Model/ 固定名自取）；
- 废弃"缺设置文件回退 EXE 旁 input//output/"残留逻辑；
- 各阶段输出目录改为 `Result/gds2geo/<stage>/`；
- 锁文件随 runtime-dir 规则迁到 `<dataRoot>/.locks/`。

compact_em 具体项：
- 输出从 `Result/model.peec`、`Result/S_parameter.csv` 等扁平路径改为 `Result/compact_em/<stage>/...`；
- 阶段状态文件统一命名 `state.json`；
- trace jsonl 落点统一 `Result/logs/compact_em/`；
- port 文件名适配 `Model/port_em.json`（原 port.json）；
- 包内 `DATA/` 降级 `examples/`。

## 四、验收标准

部署期（注册时，失败拒绝启动）：
1. manifest 语法/schemaVersion 合法；
2. 消费 ref 唯一生产者、DAG 无环、apiVersion 窗口相交；
3. produced path 全在本模块 `Result/<id>/` 内（publishTo 仅 Model//Setting/）；
4. argsTemplate 含 `{dataRoot}`、无他模块目录名字面量；
5. 包内残留可读写 `DATA/` → 警告（一个版本周期后升拒绝）。

运行前（inputs.validate）：
6. 入口工件存在且非空；
7. 本链路 port 文件绑定校验（meshFile 存在、sha 匹配）。

运行后（stage.finished）：
8. 产出存在且非空（manifest.produced）；
9. 退出码按 errorContract 归一 errorCode；
10. 最小算例回归通过并保留日志（dev_case 工具，§五）。

代码审查（人工）：
11. 源码不出现其他模块 providerId 字符串（grep）；
12. ERROR_CODES.md 覆盖 exitCodes 表全部失败路径。

## 五、本地自测（dev_case）

```powershell
.\dev_case.ps1 -Module gds2geo -Stage mesh_em [-CaseDir D:\dev\mycase] [-Clean]
```

在 `%TEMP%\peec_dev\<module>\`（或指定目录）生成 dataRoot 骨架 → 从包内 `examples/` 复制输入样品到 `Model/` → 调 `task_layer.exe run` 跑单阶段流水线（上游产物缺失会明确报出）→ 保留现场，`-Clean` 删除。
原则：**本地目录结构 = 生产 dataRoot 布局**。

## 六、接入流程

```
模块团队                              平台方
──────────────────────────────────────────────────
1. 改造 CLI + 去 DATA 化
2. 编写四件套（manifest 参照 SPEC §九示例）
        │
        ▼
3. 提交交付包 ──────────────────────► 4. 放入 <MODULE_ROOT>/<模块目录>/
                                       5. modules.json 加路由条目（3 行）
                                       6. task_layer 重启 → 注册期校验
        ◄────────────────────────────  7. 反馈校验结果（明细到字段）
8. 修正重交（如有）
        │
        ▼
9. dev_case 最小算例回归 ───────────► 10. 全链路联调 → 打发布包
```

底线：全程不需要 task_layer 侧写任何模块适配代码。

## 七、FAQ（简）

**Q：想新增 dataRoot 下的全局共享目录？** 走 DATA_LAYOUT 版本变更流程；模块私有需求放 `Result/<providerId>/` 自由区。
**Q：产物 schema 小改要升 apiVersion 吗？** 加可选字段不升；删字段/改语义升。拿不准就升。
**Q：port_thermal.json 现在要做吗？** 热模块未接入前只需 UI 预留写入能力；compact_em 只认 port_em.json。
