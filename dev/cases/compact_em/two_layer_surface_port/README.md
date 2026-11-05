# 双层 PEC 表面端口：v1.3 独立回归案例

来源：用户原始三文件案例；网格、材料与端口 JSON 内容按原字节保留，源定义在案例包中命名为 `Model/source_em.json`。设置来源是历史平铺 `Setting/set.json`；交付脚本把原始字节复制为 `Setting/set_em.json`。源码仓库 examples 保留历史输入名（设置仍为 set.json，源定义可能仍为 port_em.json）；本说明中的新名与运行命令针对交付的独立案例包。

输入包含 `Model/model2.msh`、`Model/matdb.json`、`Model/source_em.json` 与 `Setting/set_em.json`，无需外部资源。网格 SHA-256：`d791fb96b0d57aca43c66126765f188b35c1f54fe1057ba2a5c32d2bc9ebb896`。244 个节点、320 个三角形，无介质体；提取 P 为 320×320、L 为 398×398。端口按 [241,25]、[128,185] 排序，参考阻抗均为 120 Ω，方向分别为 +z、−z。

设置：schemaVersion=1、DIM=1000（网格单位 mm）、FS=1 GHz、FE=10 GHz、N_FP=100、MNA_WEIGHT=1。频率含两端点。原 SAVE/POST 开关保值；以下 minimal CLI 显式选择源图，不要求空间场采样网格。

## 运行 peec_basic 三阶段

将整个案例复制到安装目录外的新 dataRoot；下列 `$case` 指向该案例，`$module` 指向安装包。案例输入已齐全，无需包内模板；平台默认设置由平台 templates/ 管理，不随模块包携带。

```powershell
$module = 'D:\modules\compact_em'
$case = 'D:\cases\two_layer_surface_port'
& "$module\PEEC_Model_Extractor.exe" --data-root $case --export post,report
& "$module\PEEC_Solver.exe" --data-root $case --export post
& "$module\PEEC_Post_Processing.exe" --data-root $case --source-plots j,qe,ve --parts mag
```

## 预期输出与验收点

- 三阶段退出码均为 0；产物仅位于 `Result/compact_em/<stage>/`，诊断日志位于 `Result/logs/compact_em/`。`Model/`、`Setting/` 输入字节不变，不产生 `Result/tasks/` 编排记录。
- extract：非空 `model.peec`、`Post_Model.txt` 与提取报告；`artifacts.json.inputs` 包含 `Model/source_em.json` 和 `Setting/set_em.json`，网格指纹匹配上面的 SHA-256。
- solve：非空 `S_parameter.csv`、`Post_Source.txt`。S CSV 共 101 行（表头加 100 行数值），首/末频点为 1000000000/10000000000 Hz；9 列为 frequency_Hz 及两端口 Sij 的 real/imag，所有值有限。端口数组顺序决定 Sij 顺序。
- S 参数绘图由前端读取 CSV 完成；新算例不生成 plot 目录、SVG 或 Touchstone sNp。旧算例残留不用于此验收，也不自动删除。
- post：非空 `Source.msh` 以及本阶段的 `Post_Model.txt`/`Post_Source.txt` 副本，含所选电流、荷电、电压幅值视图。
- 三个阶段均有非空 `artifacts.json`：schemaVersion=1、provider=compact_em、stage 等于目录名；inputs/outputs 为相对 dataRoot 的路径与实际 SHA-256，输出索引只列本轮成功产物。可选 `stage_receipt.json` 为自证，缺失不改变阶段成败。
- 本次仅设置改名的对照使用迁移前当前主线，P/L/PORT、频点和网络/源数据要求精确一致。与 2026-11-02 旧版比较时，须另行报告现有参考地、求解优化及低频精度修复差异，不能把旧低频错误作为真值。

## 已知局限

导体按 PEC 计算，材料电导率和厚度不进入电阻或损耗。该案例用于流程与内部方程一致性回归，无外部实测或全波精度基准。没有空间采样网格，以上命令只生成源图；E/H 场图需另行准备私有采样输入，不能向 Model 添加第二份 msh。不要把存在产物或三阶段成功当作物理精度验收。


## 平台适配说明（2026-11-05）

- 网格按平台固定名落位：model2.msh → model_em.msh（同字节拷贝，meshSha256 不变）；
- source_em.json meshFile 相应更新；
- 全链 task_layer run peec_basic 实测通过（2026-11-05，v2.0）。
