# Compact_EM 1.4.3 R2标准输入案例

输入来自此前冻结R2案例，Model及Setting四文件除设置schemaVersion升为2外保持原始内容。复制到安装树外后依次运行三EXE的 --data-root <case>。设置只读Setting/set_em.json，源定义只读Model/source_em.json；不得混入旧set.txt/set.json/SET_EM.json或port文件名。

预期：extract生成Result/compact_em/extract/model.peec（PEECFS02/version2）；solve生成S/Y/Z_parameter.csv（100数据行，实际1.09–10GHz）；post生成Source.msh（全部源量开启时14视图）。本案例POST_FIELD=0，不要求Field.msh。纯表面PEC/真空，无材料损耗；网格包括CAD点线，端口只由source定义。当前API7、revision docs16-contract-fix-20261113。错误及恢复动作见 ../../compact_em/ERROR_CODES.md。

本版设置schemaVersion=2；网格/材料/源文件字节不变。阶段共享.locks/compact_em.run.lock；忙碌时等待重试。仍需保留libiomp5md.dll。
