# 错误码登记

所有失败在 stderr 输出一行 JSON（code、severity=error、message）；进程退出码是执行结果依据。日志在 Result/logs/thermal；receipt 仅辅助排错。

|退出码|code|条件|可恢复性与处理|
|---|---|---|---|
|0|无|成功|继续后续阶段|
|2|QS-TH-FMT-INPUT|JSON/MSH/OP0 格式、字段、单位或不支持的设置|修正输入后重跑|
|2|QS-TH-CFG-PATH|数据根位于安装目录、路径越界、访问任务目录|改用独立算例目录|
|2|QS-TH-CFG-BINDING|网格、材料、设置、映射指纹不一致|重新提取并用对应网表求解，不能仅改哈希绕过|
|3|QS-TH-IO-FILE|文件不存在、无权限、空间不足或文件系统错误|修正路径/权限/磁盘后重跑|
|4|QS-TH-NUM-GEOMETRY|退化单元或交叠积分异常|修正网格|
|4|QS-TH-NUM-COVERAGE|材料面未完整覆盖参考面|修正实体选择或网格|
|4|QS-TH-NUM-CONSERVATION|矩阵守恒、热源或热容检查失败|检查材料、网格和源设置|
|4|QS-TH-NUM-MAPPING|不能映射全部网格节点或线性一致性失败|检查薄层几何及参考面|
|5|QS-TH-EXEC|Windows SHA256 服务失败|检查操作系统运行环境|
|6|QS-TH-RUN-LOCKED|同一算例已有热模块运行或锁文件无法打开|等待进程完成，检查权限；不能删除运行中的锁|

失败可能留下部分输出。仅退出码为 0 才允许使用本次结果；不要把文件存在视为成功。系统强制终止/崩溃可能没有 JSON 行，由调用方按非零退出处理。


边界专项：boundary_thermal.json 缺失/不可读 → exit3 QS-TH-IO-FILE；JSON错误、schemaVersion不为1、源中内嵌边界、边界中包含热源、无效温度/面选择、不支持的默认边界 → exit2 QS-TH-FMT-INPUT。边界路径越界 → exit2 QS-TH-CFG-PATH；提取后边界被修改 → post exit2 QS-TH-CFG-BINDING。--boundary 规则与 --source 相同。

1.0.3取消用户结果绑定文件；缺少OP0仍为exit3 QS-TH-IO-FILE，OP0节点缺失/格式不符为exit2 QS-TH-FMT-INPUT；--result-binding为已移除参数，传入会报未知参数。

1.0.6：source_thermal.json必须为schemaVersion=2并含thermal；缺失thermal或旧schema为exit2 QS-TH-FMT-INPUT。不读取独立控制件，--settings为未知参数(exit2)。OP0缺失为exit3 QS-TH-IO-FILE，默认位置为实际source的同目录/thermal.op0，无旧路径回退。post不检查网表，只检查映射所需节点数据；缺失温度仍为exit2。source(含thermal)、边界或仿真设置变化使旧提取绑定失效(exit2 QS-TH-CFG-BINDING)。
