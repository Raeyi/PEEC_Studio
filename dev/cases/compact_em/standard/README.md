# standard — compact_em 案例（20261104 v1.3 交付案例，2026-11-05 平台归档）

独立算例（无需 gds_full）：Model/（model_em.msh 主网格 + matdb.json + source_em.json）
+ Setting/set_em.json（compact_em v1.3 唯一 JSON 设置，schemaVersion 1）。

## 基线（2026-11-05，PEEC_Studio v2.0 平台全链 task_layer run peec_basic）

- 3 阶段 extract→solve→post 全部 done（~2.5s）
- S_parameter.csv = 表头 + 100 频点 = 101 行（N_FP=100，1–10 GHz 线性）
- 与 11-02 v1.2 基线对照：maxRel 7.1e-12（数值一致）
- 运行窗口 WER 干净（无 0xC0000005 等崩溃记录）

## 注意

- 设置文件名必须是 set_em.json（compact_em v1.3 起；旧 set.json 不再读取）；
- 网格固定名 model_em.msh（平台落位名）；
- 本案例不生成 Touchstone / S 参数 SVG（v1.3 起绘图程序取消）。
