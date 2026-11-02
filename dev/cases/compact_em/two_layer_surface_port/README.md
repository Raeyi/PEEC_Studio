# 双层纯金属表面端口

本目录是只读输入样品。先复制到安装目录外的新dataRoot，再运行四程序；不要直接把包内examples目录作为dataRoot。

输入：Model下唯一.msh、matdb.json、port_em.json及Setting/set.json。配置来自原对应Cases样例的保值平铺转换，MNA权重/频率等未改。模型变化需重绑端口SHA-256。

提取与求解可用默认minimal模式；源图须两上游都--export post，Post再--source-plots j,qe,ve。场图另需显式准备私有采样网格，不能把第二份msh放入Model。

样例仅用于内部一致性和流程回归，不构成外部电磁精度基准。
