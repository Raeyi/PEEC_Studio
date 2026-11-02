# compact_em 重打包说明（DATA_LAYOUT v1.3 求解设置拆分）

致 compact_em 模块团队。平台契约 DATA_LAYOUT 已升 v1.3（2026-11-02）：
Setting/set.json 按链路拆分为 set_em.json（EM 字段）与
set_thermal.json（热对象，thermal 模块消费）。EM 链侧变化如下，
需要重打模块包 + 案例包。改动量小（读文件约定 + manifest + 文档），
数值与 CLI 行为不变。

## 变更点

1. 设置文件改读 Setting/set_em.json（模块内部读取约定变更）：
   - 内容格式不变 = 现行 set.json 的 EM 平铺字段（Solver_SET / MNA_WEIGHT /
     FS / FE / N_FP / DIM / Taylor / SAVE_* / POST_*，schemaVersion 整数 1）；
   - 只读新名：set_em.json 为唯一接受的 JSON 设置名（对齐
     source_thermal.json / source_em.json 先例，旧名 set.json 不再读取，
     不做双名过渡）；
   - set.txt（旧文本格式）维持现状：接受并打弃用 warning，JSON 优先；
   - 输入指纹（stage_receipt / 结果缓存）同步改对新文件的指纹
     （"整个实际选中的设置文件纳入指纹"规则不变，对象改为 set_em.json）。
2. manifest：
   - artifacts.consumed 的 user/solver.set 工件 path：
     Setting/set.json → Setting/set_em.json；
   - capabilities.dataLayoutRevision："1.2" → "1.3"；
   - capabilities.breakingChanges 增一条（设置文件改读 set_em.json，
     set.json 不再消费；CLI 与数值基线不变）。
3. README / ERROR_CODES：设置文件路径与命名更新（set_em.json 缺失/坏格式
   → 既有 QS-FMT-SETTING-INVALID 语义不变）；历史布局说明（SET_EM.json /
   set.txt / port_em.json 等）保留为历史记载。
4. 案例包：standard / two_layer_surface_port 的 Setting/set.json 改名
   set_em.json（内容逐字节不变）；其余输入（source_em.json / 网格 / matdb）
   不变。

## 不变项（验收基线）

- CLI 参数（--data-root / --runtime-dir / --export / --s-columns /
  --post-freq-index / --post-port / --format / --source-plots /
  --field-plots / --parts）与默认行为不变；
- 产物路径与清单不变（model.peec / S_parameter.csv 入 produced；
  s2p / SVG / post 件的 resultFiles 增补不在本次范围，平台另行确认）；
- 数值回归基线：standard 案例 S_parameter.csv 101 行（表头 + 100 频点）、
  四阶段全链通过；two_layer_surface_port 基线同现版——文件仅改名，
  内容未变，数值必须一致；若字节有差异请在交付说明注明原因。

## 交付要求（新包）

1. 新模块包（建议版本 2026.11.03-v1.3 或贵方版本号）：四件套
   （manifest / exe / README / ERROR_CODES），无 examples/templates/DATA；
2. 新案例包 compact_em_cases_<新日期>/：两案例 Setting/ 改为 set_em.json；
3. 验收记录（validation/）按新契约更新；
4. SHA256SUMS.txt 覆盖全部交付文件；
5. 交付说明中注明与 20261102 版（v1.2）的差异清单。

平台侧动作（无需贵方处理）：平台读取由 manifest consumed 驱动，
收到新 manifest 后无需代码改动；新包到位后平台重跑 peec_basic 全链 E2E
并对照回归基线。
