# 错误码登记

所有失败在 stderr 输出一行 JSON（code、severity=error、message）；进程退出码是执行结果依据。日志在 Result/logs/thermal；receipt 仅辅助排错。

|退出码|code|条件|可恢复性与处理|
|---|---|---|---|
|0|无|成功|继续后续阶段|
|2|QS-TH-FMT-INPUT|JSON/MSH/OP0 格式、字段、单位或不支持的设置|修正输入后重跑|
|2|QS-TH-CFG-PATH|数据根位于安装目录、路径越界、访问任务目录|改用独立算例目录|
|2|QS-TH-CFG-BINDING|网格、材料、设置、映射或求解结果指纹不一致|重新提取并用对应网表求解，不能仅改哈希绕过|
|3|QS-TH-IO-FILE|文件不存在、无权限、空间不足或文件系统错误|修正路径/权限/磁盘后重跑|
|4|QS-TH-NUM-GEOMETRY|退化单元或交叠积分异常|修正网格|
|4|QS-TH-NUM-COVERAGE|材料面未完整覆盖参考面|修正实体选择或网格|
|4|QS-TH-NUM-CONSERVATION|矩阵守恒、热源或热容检查失败|检查材料、网格和源设置|
|4|QS-TH-NUM-MAPPING|不能映射全部网格节点或线性一致性失败|检查薄层几何及参考面|
|5|QS-TH-EXEC|Windows SHA256 服务失败|检查操作系统运行环境|
|6|QS-TH-RUN-LOCKED|同一算例已有热模块运行或锁文件无法打开|等待进程完成，检查权限；不能删除运行中的锁|

失败可能留下部分输出。仅退出码为 0 才允许使用本次结果；不要把文件存在视为成功。系统强制终止/崩溃可能没有 JSON 行，由调用方按非零退出处理。

## 2026-11-02 / docs_1.2 重发变更

本模块目录仅含两个 EXE、README.md、ERROR_CODES.md、module.manifest.json 共五个文件（四类交付件）。案例独立交付 thermal_cases_20261102.zip，不再携带 templates/examples。

默认读取 Model/source_thermal.json，显式参数为 --source，原 --ports 不再接受，故 apiVersion 升至 2；minApiVersion=1 只表示上游兼容窗口。此版不会回退读取旧端口文件。配置内容及计算方法不变，仍为已验证 FEM/双迹薄层，尚未实现讨论中的 PEEC 四面体方案。

boundary_thermal.json 在 v1.2 尚无格式及消费方；本版不读取、不创建、不声明该 ref。原 fixed_temperature/default_external_boundary 字段继续放在 source_thermal.json，直至正式边界契约定义。

按新版官方 Schema，capabilities.dataLayout 仍为 peec-data/v1；具体遵循规范修订号另记 dataLayoutRevision=1.2。案例项目名使用字母、数字、下划线，不含空格或特殊字符；父路径支持 Unicode。

源码和开发测试在交付总目录中另附；只有 thermal/ 或模块 zip 应安装到 MODULE_ROOT。
