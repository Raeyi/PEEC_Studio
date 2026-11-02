# 模块接入手册（ONBOARDING）

v1.2 · 2026-11-02 · 读者：模块团队负责人。前置阅读：DATA_LAYOUT.md、MODULE_MANIFEST_SPEC.md。
v1.2 变更：① 包内只含四件套，不再携带 `examples/`、`templates/`（统一各模块包结构）；② 新增**案例包交付**要求（§1.1，包外单独交付）；③ 输入文件名改名 `port_em.json` → `source_em.json`、`port_thermal.json` → `source_thermal.json`，新增预留件 `boundary_em.json`、`boundary_thermal.json`（DATA_LAYOUT v1.2）。
机器校验工具：`docs/module.manifest.schema.json`（JSON Schema draft-07，平台方提供）。

## 一、交付四件套

| # | 文件 | 要求 |
|---|---|---|
| 1 | `*.exe`（+ 必要随附程序） | 静态链接；安装目录只读（R3）；CLI 满足 R1-R3 |
| 2 | `README.md` | 调用方式、全部 CLI 参数与退出码、本地自测方法、依赖（如 Gmsh 版本）、内部库版本 |
| 3 | `ERROR_CODES.md` | QS 错误码登记：code / 触发条件 / 可恢复性 / 处置 |
| 4 | `module.manifest.json` | 按 MANIFEST_SPEC 书写；**提交前用 JSON Schema 校验器验证** |

包内**只含四件套**——不再携带 `examples/`、`templates/` 或任何其他目录/文件（统一所有模块包结构，降低平台集成成本）。
**不再接受包内可读写 `DATA/`。** 包内不含 UI 输入文件——UI 选取的输入直接写入算例 `<dataRoot>\Model\`。

### 1.1 案例交付（必做，包外单独交付）

每次交付随附 **≥1 个完整可跑案例**（供平台 dev_case 回归与联调自测）：

- 案例 = 一个完整 dataRoot 布局：`Model/` + `Setting/`（含 `source_em.json` 等 UI 输入文件与 `set.json`）；涉及网格的案例连网格一起给（不引用外部资源）；
- 每案例一份 README：输入怎么来的、**预期输出/验收点**（关键产物、字段或数值参考）、已知局限；
- 案例目录命名 `<module>_cases_YYYYMMDD/`，**与模块包分开交付**（放进模块包会被打回）；平台方归档到 `dev/cases/<module>/` 后用于 dev_case 回归（§五）。

## 二、CLI 合规自查

- [ ] 有数据根入口参数（推荐 `--data-root`；散参数设计须在 manifest cli.dataRootArg 说明或省略）
- [ ] 无"EXE 旁 input//output/ 缺省回退"作为文档化接口（保留代码路径则声明 deprecatedModes）
- [ ] 日志/私有锁/临时文件落 `--runtime-dir`（或固定 `Result/logs/<providerId>/`）
- [ ] 只读安装目录下跑通一次最小算例
- [ ] 不自行往 Model//Setting/ 写正式件（publisher 归 task_layer）

## 三、去 DATA 化迁移（现有模块必做）

通用动作：
1. 包内 `DATA/Model/` 输入样品 → 移到 `examples/`（只留输入件）；
2. `DATA/Result/`、`DATA/runtime/` 历史产物与日志 → 删除（不属交付物）；
3. 按 EXE 目录解析数据路径的代码分支 → 改按 dataRoot 参数（R1）；
4. 输出位置迁到 `Result/<providerId>/<stageId>/`（对照 DATA_LAYOUT §二逐项改）；
5. 阶段状态文件统一命名 `stage_receipt.json`（可选写，schema 见 DATA_LAYOUT §七）；
6. `set.txt` → `set.json`（格式见 DATA_LAYOUT §八；过渡期两文件并存时 set.json 优先）。

gds2geo 具体项：
- 四个 exe 的数据路径解析收敛到显式参数（当前 --gds/--tech/--output-dir 已满足，manifest argsTemplate 传绝对路径即可，**不必强制加 --data-root**）;
- 废弃"缺设置文件回退 EXE 旁 input//output/"残留逻辑（manifest 声明 deprecatedModes）；
- 各阶段输出目录改为 `Result/gds2geo/<stage>/`；
- 锁文件随 runtime-dir 规则迁到 `<dataRoot>/.locks/`；
- **不自己往 Model/ 搬文件**（publishTo 由 task_layer publisher 执行）。

compact_em 具体项：
- 输出从 `Result/model.peec`、`Result/S_parameter.csv` 等扁平路径改为 `Result/compact_em/<stage>/...`；
- 阶段状态文件 extract.state/solve.state → `stage_receipt.json`（按 schema 写）；
- trace jsonl 落点统一 `Result/logs/compact_em/`；
- 输入文件名适配 v1.2 契约：`Model/source_em.json`（v1.1 名 `port_em.json`，更早名 `port.json`；二进制须实读新名，manifest consumed path 同步更新，ref 用 `em/source.def`）；
- `set.txt` → `set.json`（CLI 同时接受两者，set.json 优先 + warning）；
- 包内 `DATA/` 移除，测试案例改为包外案例包单独交付（§1.1）。

## 四、验收标准

### 部署期（注册时，失败拒绝启动）
1. manifest 通过 JSON Schema 校验（`docs/module.manifest.schema.json`）；
2. 消费 ref 唯一生产者、DAG 无环、apiVersion 窗口相交（公式见 MANIFEST_SPEC §6.3）；
3. produced path 全在本模块 `Result/<id>/` 内（publishTo 仅 Model//Setting/）；
4. argsTemplate 含 dataRoot 绑定、无他模块目录名字面量；
5. 包内残留可读写 `DATA/` 或案例目录（`examples/` 等）→ 警告（一个版本周期后升拒绝）；案例须按 §1.1 包外交付。

### 运行前（inputs.validate）
6. 入口工件存在且非空；
7. 本链路 source 文件绑定校验（`source_em.json`：meshFile 存在、sha 匹配；过渡期旧名 `port_em.json` 同样校验）；
8. set.json 存在且 schemaVersion 在 accept 区间内（I6）。

### 运行后（stage.finished）
9. 产出存在且非空（manifest.produced）；
10. 退出码按 errorContract 归一 errorCode；
11. publisher 执行 publishTo 成功（失败 = 阶段失败 QS-TL-IO-ERROR）；
12. 最小算例回归通过并保留日志（dev_case 工具，§五）。

### 代码审查（人工）
13. 源码不出现其他模块 providerId 字符串（grep）；
14. ERROR_CODES.md 覆盖 exitCodes 表全部失败路径；
15. 模块不写 Result/tasks/、不自行往 Model//Setting/ 复制正式件。

## 五、本地自测（dev_case）

```powershell
.\dev_case.ps1 -Module gds2geo -Stage mesh_em [-CaseDir D:\dev\mycase] [-Clean]
```

在 `%TEMP%\peec_dev\<module>\`（或指定目录）生成 dataRoot 骨架 → 从**案例包**（`dev/cases/<module>/`，即 §1.1 包外交付归档）复制输入件到 dataRoot 对应位置 → 调 `task_layer.exe run` 跑单阶段流水线（上游产物缺失会明确报出）→ 保留现场，`-Clean` 删除。
原则：**本地目录结构 = 生产 dataRoot 布局**。

## 六、接入流程

```
模块团队                              平台方
──────────────────────────────────────────────────
1. 改造 CLI + 去 DATA 化
2. 编写四件套（manifest 参照 SPEC §十一示例）
3. 本地 JSON Schema 校验通过
        │
        ▼
4. 提交交付包 ──────────────────────► 5. 放入 <MODULE_ROOT>/<模块目录>/
                                       6. modules.json 加路由条目（3 行）
                                       7. task_layer 重启 → 注册期校验
        ◄────────────────────────────  8. 反馈校验结果（明细到字段）
9. 修正重交（如有）
        │
        ▼
10. dev_case 最小算例回归 ──────────► 11. 全链路联调 → 打发布包
```

底线：全程不需要 task_layer 侧写任何模块适配代码。

## 七、FAQ（简）

**Q：想新增 dataRoot 下的全局共享目录？** 走 DATA_LAYOUT 版本变更流程；模块私有需求放 `Result/<providerId>/` 自由区。
**Q：产物 schema 小改要升 apiVersion 吗？** 加可选字段不升（C2）；删字段/改语义升（B4）。拿不准就升。
**Q：source_thermal.json / boundary_*.json 现在要做吗？** 热模块未接入前只需 UI 预留写入能力；compact_em 只认 `source_em.json`（过渡期旧名 `port_em.json` 亦可）。`boundary_em.json` / `boundary_thermal.json` 是预留槽位：暂无消费方、格式待定，UI 可不写，写了平台不做校验。
**Q：案例（自测样品）能放进模块包吗？** 不能。包内只含四件套；案例按 §1.1 单独交付（`<module>_cases_YYYYMMDD/`），平台归档 `dev/cases/<module>/`。
**Q：set.txt 什么时候彻底废弃？** compact_em 下一版 CLI 同时接受 set.json + set.txt（过渡）；再下一版只留 set.json。过渡期两个文件同时存在时 set.json 优先 + warning。
**Q：stage_receipt.json 必须写吗？** 不必须。它是增强信息（排错/复现），不写不影响链路调度。写了就按 DATA_LAYOUT §七 schema。
**Q：我们的模块没有 --data-root 参数，要用散参数，合规吗？** 合规（R1 允许）。manifest argsTemplate 里用 `{dataRoot}/...` 拼出绝对路径传给散参数即可；cli.dataRootArg 字段省略。
