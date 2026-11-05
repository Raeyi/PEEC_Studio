# Compact EM 准静态 PEEC 模块

Compact EM 将导体表面网格及可选的无损介质体网格转换为 PEEC 模型，计算多端口频域 S/Y/Z 参数，并生成电流、电荷、电位和空间 E/H 场的 Gmsh 可视化结果。

模块标识为 `compact_em`，接口为 API 3，数据布局为 `peec-data/v1`、EM 布局修订为 `1.3`。运行链由三个独立程序组成：

```text
主网格 + 材料库 + 端口定义 + 求解设置
    → PEEC_Model_Extractor.exe：提取 P/L 与拓扑
    → PEEC_Solver.exe：多端口频域求解，输出 S CSV 和按需结果
    → PEEC_Post_Processing.exe：按需生成源图与空间场图
```

## 功能与适用范围

| 功能 | 支持范围 |
|---|---|
| 导体模型 | PEC 导体表面，支持纯金属自由空间模型 |
| 介质模型 | 无损异质介质体，材料关联由网格 Physical tag 与 MATDB 数字 ID 确定 |
| 参考地 | 可选水平无限 PEC 地平面，使用镜像源，无需对地平面划分网格 |
| 网络求解 | 正频率单点、线性扫描、对数扫描；多端口 S 参数与按需 Y/Z 参数 |
| 源后处理 | 表面电流密度、电荷面密度和电位；可选实部、虚部、幅值 |
| 场后处理 | 在独立采样网格上计算 E/H 场，输出 Gmsh 多视图文件 |
| 工程输出 | 模型包、按需散矩阵、端口映射、模型报告、SPICE 网表、求解诊断 |

导体电导率与厚度用于输入合法性检查，不进入导体电阻或损耗装配。实际参与计算的介质体必须无损。模块仅支持频域准静态计算；`Taylor` 非零不等于启用全波传播，通常应设为 `0`。S 参数绘图由前端或其他 CSV 工具完成，模块不输出 S 参数 SVG 或 Touchstone 文件。

## 快速开始

需要 Windows x64。将模块安装在独立目录，准备安装目录树之外的算例目录 `dataRoot`。独立案例包提供 `standard`（无损介质）和 `two_layer_surface_port`（双层纯金属）两个完整输入；先将所需案例复制到自己的工作目录。

```powershell
$module = 'D:\modules\compact_em'
$dataRoot = 'D:\cases\standard'

& "$module\PEEC_Model_Extractor.exe" --data-root $dataRoot --export post,report
& "$module\PEEC_Solver.exe" --data-root $dataRoot --export post,y,z
& "$module\PEEC_Post_Processing.exe" --data-root $dataRoot --source-plots j,qe,ve --parts mag
```

上述调用生成模型、S/Y/Z CSV、后处理快照及 `Source.msh`。如果只需 S 参数，可省略后处理和相关导出：

```powershell
& "$module\PEEC_Model_Extractor.exe" --data-root $dataRoot
& "$module\PEEC_Solver.exe" --data-root $dataRoot
```

三个程序均支持 `--help`，无需指定数据根即可查看帮助。默认输出策略为 `minimal`，只生成默认文件和 CLI 明确选择的可选文件。

## 输入契约

### 数据目录

```text
<dataRoot>/
  Model/
    <唯一主网格>.msh
    matdb.json
    source_em.json
  Setting/
    set_em.json
  Result/
    compact_em/
      extract/
      solve/
      post/
    logs/compact_em/
  .locks/
```

`Model/` 必须恰好有一份 `.msh` 主网格。计算程序只读 `Model/` 和 `Setting/`，结果写入 `Result/compact_em/`，日志写入 `Result/logs/compact_em/`。安装目录可设为只读；程序不向安装目录存放运行数据。

`--data-root` 的相对路径按启动工作目录解释，其余相对数据路径按 `dataRoot` 解释。`--runtime-dir` 可自定义，但必须位于同卷的 `Result/logs/compact_em/` 子树内。不同启动工作目录不会改变输入文件的选择。

### 主网格与单位

- 支持 Gmsh MSH 2.2、4.1 的 ASCII/二进制，以及 MSH 4.0 ASCII。
- 支持一阶三角形、四面体和点；拒绝高阶单元和线单元（Gmsh type 1），端口只通过 source_em.json 定义。
- 三角面 Physical Surface tag 对应 `matdb.json` 中表面材料的 `id`；四面体 Physical Volume tag 对应体材料 `id`。
- 端口使用原始 Gmsh 三角形元素 ID，不是节点 ID、Physical tag 或程序内部重编号。
- 坐标转换为 `x_m = x_mesh / DIM`。毫米网格设置 `DIM=1000`；单位必须显式指定，程序不按几何数值猜测。

空间场采样网格另行提供，不得作为第二份 `.msh` 放入 `Model/`。

### 材料库 matdb.json

材料库要求 `schemaVersion: 3` 和非空 `materials` 数组，材料 `id` 为唯一正整数。以下示例表示 Physical Surface tag 为 `1` 的导体表面：

```json
{
  "schemaVersion": 3,
  "materials": [{
    "id": 1,
    "name": "PEC",
    "dimension": "surface",
    "type": "C",
    "thickness": 0.035,
    "thicknessUnit": "mm",
    "properties": {
      "conductivity": {"model": "constant", "unit": "S/m", "value": 58000000}
    }
  }]
}
```

| 实际使用的材料 | 要求 |
|---|---|
| 导体三角面 | `dimension="surface"`、`type="C"`，正电导率和正厚度 |
| 介质四面体 | `dimension="volume"`、`type="D"`，电导率为 0，介电常数和磁导率实部为正且无损 |
| 隐式外部 | 真空 |

`thickness`、`thicknessUnit` 位于材料顶层，厚度单位支持 `m`、`mm`、`um`。材料属性放在 `properties` 中，使用 `constant` 模型；电导率单位为 `S/m`，`relativePermittivity` 与 `relativePermeability` 的单位为字符串 `"1"`。材料库可包含未使用条目，但不能由此推断支持导体体积或有损介质求解。所有材料须声明 conductivity；D 材料还须声明 relativePermittivity。relativePermeability 可省略，默认 1；C 材料的 relativePermittivity 默认 1。材料参数不通过求解设置覆盖。

### 端口定义 source_em.json

`source_em.json` 绑定主网格并定义有序端口数组。以下为字段示意，网格名、哈希、元素 ID 和方向须按实际模型填写：

```json
{
  "meshFile": "model_em.msh",
  "meshSha256": "<主网格原始字节的64位SHA-256十六进制值>",
  "ports": [{
    "triangleIds": [101, 102],
    "direction": [1, 0, 0],
    "impedanceOhm": 50
  }]
}
```

可用 PowerShell 获取实际网格哈希：

```powershell
(Get-FileHash -LiteralPath "$dataRoot\Model\model_em.msh" -Algorithm SHA256).Hash.ToLowerInvariant()
```

- `meshFile` 必须与所选主网格文件名一致，`meshSha256` 必须匹配其原始文件字节；修改网格后重新计算哈希，并重新核对端口元素 ID。
- `ports` 非空，数组顺序决定网络端口顺序。CSV 中第 `i` 个响应端口和第 `j` 个激励端口均按此顺序编号。
- `triangleIds` 为两个不同端点；真实端点须为导体三角形元素 ID，且不超过 `2^53-1`。
- `direction` 为单位向量。两个真实端点时，与“第二面心减第一面心”的向量点积必须为正，即指向第二端所在半空间，不要求严格共线。
- `impedanceOhm` 为有限正数，单位 Ω；各端口可以使用不同参考阻抗。
- 端点顺序定义端口极性，交换两端会改变极性。开启参考地后允许一个端点为 `-1`，见参考地设置。

`boundary_em.json` 是平台预留文件，模块当前不消费它。

## 计算设置

`Setting/set_em.json` 是顶层平铺 JSON，`schemaVersion` 必须是整数 `1`。常用设置示例：

```json
{
  "schemaVersion": 1,
  "DIM": 1000,
  "FS": "1e9",
  "FE": "10e9",
  "N_FP": 100,
  "PRECISION": 0,
  "Swap_Log": 0,
  "Solver_SET": 0,
  "MNA_WEIGHT": 0.8,
  "Taylor": 0,
  "SOLVER_MEMORY_MB": 0,
  "SOLVER_THREADS": 0
}
```

### 主要参数

| 参数 | 默认或必填条件 | 含义与约束 |
|---|---|---|
| `schemaVersion` | 必填，`1` | 设置结构版本 |
| `DIM` | 必填 | 正数，网格坐标到米的除数 |
| `FS`、`FE` | 必填 | 正整数 Hz，`FS <= FE` |
| `N_FP` | `PRECISION=0` 时必填 | 总采样点数，正整数，受实际采样及内存限制 |
| `PRECISION` | `0` | 0 表示按点数；正值表示线性 Hz 步长或对数 decade 步长 |
| `Swap_Log` | `0` | 0 线性，1 对数 |
| `Solver_SET` | `0` | 仅支持频域 0 |
| `MNA_WEIGHT` | `0.8` | 正的 MNA 权重参数，不是误差容差 |
| `Taylor` | `0` | 接受 0–10；非零提取额外系数，不启用全波传播求解 |
| `SOLVER_MEMORY_MB` | `0` | MiB 新增分配预算，0 自动；不是进程总内存硬上限 |
| `SOLVER_THREADS` | `0` | 频点并发数，0 自动，允许 0–4096；并发增加工作区内存需求 |
| `GROUND_PLANE_ENABLED` | `0` | 0 关闭，1 开启无限 PEC 参考地 |
| `GROUND_PLANE_Z_METERS` | 开启参考地时必填 | 有限全局 SI 米，与 DIM 无关 |

`FS/FE` 支持整数科学计数法和数字字符串，按精确十进制 token 解析为 int64。实际频率向量采用 double；无法区分或不能严格递增的采样会被拒绝。未知设置键忽略，重复键、坏 JSON 和非法已知值拒绝；布尔值仅用于 0/1 开关。

### 频率采样

- `PRECISION=0`：按 `N_FP` 个点采样。`N_FP>=2` 的区间扫描包含 FS、FE；`N_FP=1` 时只计算 FS。
- `PRECISION>0`：线性扫描按 Hz 步长，对数扫描按 decade 步长；末端不足一个完整步长时追加 FE。
- `FS=FE`：实际只计算该频点，与请求点数无关。
- 下游应读取 CSV 的实际 `frequency_Hz` 列，不根据输入点数重新推测频率数组。

### 输出相关设置

在默认 `minimal` 策略下，以 CLI 导出选项决定可选文件；以下设置主要供显式 `--output-profile legacy` 使用：

| 设置 | 默认值 | 用途 |
|---|---|---|
| `SAVE_BIN`、`SAVE_TXT`、`SAVE_SP` | `1`、`0`、`0` | 散矩阵与 SPICE 导出 |
| `POST_PROCESSING` | `1` | 求解源快照 |
| `POST_FREQ_INDEX` | `-1` | 选取最后实际频点；正数为 1-based 索引 |
| `POST_PORT` | `1` | 激励端口，1-based |
| `POST_PLOT_J/QE/VE/M/QM/VM` | 各 `1` | 源视图选择 |
| `POST_FIELD` | `0` | 空间场视图 |

默认设置由平台维护并传给 UI；安装包不附默认设置文件。CLI 在没有新 JSON 时仍可读取 `Setting/set.txt` 并给出弃用 warning；新 JSON 与 TXT 并存时 JSON 优先，坏 JSON 不回退。旧平铺 `set.json` 不作为候选；旧模块化 `SET_EM.json` 需要显式转换，不能直接当作平铺设置使用。Windows 下大小写文件名可能指向同一文件，转换时应把目标放到另一个目录，避免覆盖源文件。

## 无限 PEC 参考地

在设置文件中指定开关与全局高度：

```json
{"GROUND_PLANE_ENABLED": 1, "GROUND_PLANE_Z_METERS": 0}
```

此片段应合入完整设置文件。高度以全局 SI 米给出，与 DIM 独立；`0` 只是示例，应按实际几何坐标设置。开启时须满足：

- 仅有真空中的 PEC 导体表面，实际使用的表面材料 `er=mur=1`；没有介质四面体。
- `Solver_SET=0`、`Taylor=0`。
- 每片三角形满足 `min(z-zGround)/最长边 >= 1e-3`，不允许贴面、跨面、位于平面下方或间隙过小。

端口可使用一个参考地端点，例如：

```json
{"triangleIds": [101, -1], "direction": [0, 0, 1], "impedanceOhm": 50}
```

至少一端为真实三角形，不能两端均为 `-1`；关闭参考地时拒绝 `-1`。含参考地端点时，direction 只作为单位向量元数据，不推断地面高度。真实内部节点 0 仍为普通节点。

地面通过电荷镜像 `-q` 和电流镜像 `(-Jx,-Jy,+Jz)` 作用于 P/L 与场计算，包含自身镜像，不增加地面未知量。内部计算使用局部 SI 坐标，模型快照保存全局 SI 坐标与高度。此功能不表示支持有限地面、有限电导地或含介质的接地模型。

## 输出契约

| 阶段 | 默认或按需输出 | 内容 |
|---|---|---|
| extract | `model.peec`，默认 | P/L、支路拓扑、端口和模型元数据 |
| extract | `LL_OO.bin/txt`、`PP_OO.bin/txt`、`B2N.txt`、`PORT.txt` | 按散矩阵格式选择导出 |
| extract | `Post_Model.txt` | `--export post`，后处理模型快照 |
| extract | `Port_Mapping.csv` | `--export ports`，端口映射 |
| extract | `Model_Assumptions.txt`、`Geometry_Frame.txt` | `--export report`，模型假设和坐标框架 |
| extract | SPICE 网表 | `--export spice`，可能需要额外电容求逆内存 |
| solve | `S_parameter.csv`，默认 | 各实际频点的多端口复数 S 参数 |
| solve | `Y_parameter.csv`、`Z_parameter.csv` | `--export y,z`，单位 S、Ω |
| solve | `Post_Source.txt` | `--export post`，一个频点和激励端口的实际源快照 |
| solve | `Solver_Diagnostics.csv` | `--export diagnostics`，求解路径、阶数、条件估计、残差和耗时 |
| post | `Source.msh`、`Field.msh` | 按选择生成源/场多视图 Gmsh 文件 |
| post | `Post_Model.txt`、`Post_Source.txt` | 后处理消费的快照副本 |
| post | `Field_materials.csv` | 场计算时按 `--export field-materials` 保留 |

各文件位于 `Result/compact_em/<stage>/`。每个实际执行阶段还维护必需的 `artifacts.json`，并尽力写可选 `stage_receipt.json`。无源/场选择时，minimal Post 成功空操作，不要求生成 MSH。

`--matrix-format` 的 `package`、`bin`、`txt`、`both` 为不同选择：package 生成模型包；both 表示散矩阵 BIN+TXT，不是“模型包加散矩阵”。使用默认 package 即可运行正式流水线。

### 矩阵、网络与源数据

- 模型包为 `PEECPK01` V1，P/L 保存 float64；P 单位为 1/F，L 为 H。内部 B2N/PORT 使用 0-based 节点索引，参考地为 `-1`。
- S CSV 默认 `frequency_Hz` 加 `S1_1_real,S1_1_imag,...`。`i` 为响应端口、`j` 为激励端口，均为 1-based；S 无量纲。`--s-columns full` 另含 dB/相位角列。
- Y/Z 使用同样的端口和频率顺序，分别为导纳与阻抗。前端应保持 source_em.json 的端口数组顺序。
- `Post_Source.txt` 为 `PEEC_POST_SOURCE_V2`，显式保存电流 A、电位 V、电荷 C，包含端口注入贡献；后处理使用实际电荷生成面密度和 E 场。
- `Post_Model.txt` 关闭地面时为 V1，开启时为含全局地面高度的 V2。源快照、模型和设置必须来自一致批次。

二进制布局、快照字段及诊断列细节见源码 `OUTPUT_FORMAT.md`；完整交付资料中的对应文件为 `technical/OUTPUT_FORMAT.md`。

## 全部命令行选项

共同选项：`--data-root <dir>`、`--runtime-dir <dir>`、`--output-profile minimal|legacy`、`--pause`、`--help`。列表值以逗号分隔，不带空格。

| 程序 | 选项 |
|---|---|
| Extractor | `--matrix-format package/bin/txt/both`；`--export post,ports,report,spice` |
| Solver | `--export post,z,y,legacy-text,diagnostics`；`--s-columns ri/full`；`--post-freq-index -1或正整数`；`--post-port 正整数` |
| Post | `--source-plots j,j_vec,qe,ve,m,qm,vm`；`--field-plots e,h`；`--parts real,imag,mag`；`--space-mesh <path>`；`--export field-materials` |

Solver 的频点索引和端口索引为 1-based，`--post-freq-index -1` 选最后实际频点。minimal 下使用这两个快照选择器必须同时选择 `--export post`。

legacy 根据 SAVE/POST 设置生成兼容散文件与拆分后处理文件，S 列默认 full；minimal 默认 RI。显式 CLI 选择优先。`touchstone` 不是受支持的导出名。

## 源与空间场后处理

Post 需要先通过 Extractor 和 Solver 的 `--export post` 生成快照。每份源快照对应一个实际频点和一个激励端口；要更换快照，重新运行 Solver 并选择对应索引。

```powershell
& "$module\PEEC_Solver.exe" --data-root $dataRoot --export post --post-freq-index 10 --post-port 2
& "$module\PEEC_Post_Processing.exe" --data-root $dataRoot --source-plots j,qe,ve --parts real,imag,mag
```

| 选择 | 物理量 |
|---|---|
| `j` 或 `j_vec` | 表面电流密度，A/m；real/imag 为矢量，mag 为复矢量模长 |
| `qe` | 电荷面密度，C/m² |
| `ve` | 电位，V |
| `e`、`h` | 采样 E 场 V/m、H 场 A/m |
| `m`、`qm`、`vm` | 扩展磁等效源视图；当前 Solver 无磁激励，通常为 0 |

minimal 默认 parts 为 `real,imag`，幅值须显式选择 `mag`。矢量幅值采用 `sqrt(|Xx|²+|Xy|²+|Xz|²)`。

空间场需要独立采样网格，默认位置为 `Result/compact_em/post/input/Space_Mesh.msh`。显式路径须位于 dataRoot 的允许输入目录或本模块结果子树内；推荐统一放在 `Result/compact_em/post/input/`，不能另建任意根目录作为输入，也不能在 Model 添加第二份 .msh。以下示例假定已准备该目录的 `observations.msh`，坐标单位与主模型一致：

```powershell
& "$module\PEEC_Post_Processing.exe" --data-root $dataRoot --field-plots e,h --parts mag --space-mesh Result/compact_em/post/input/observations.msh --export field-materials
```

Gmsh 输出坐标乘回 DIM，保留输入网格的坐标单位，通常为 mm；物理场值仍按上表单位。内部 Post_Model 坐标为全局 SI 米。参考地场包含镜像源，允许在地平面上采样，拒绝平面下方；紧贴导体的场与畸形网格需另做采样/网格收敛检查。

## 结果一致性与错误处理

网格、材料、源定义和整个实际设置文件均纳入 SHA-256 指纹。输入修改后重新执行受影响上游阶段；不能只改 artifacts/receipt 的哈希来复用结果。旧残留文件不是本轮成功产物，以当前 artifacts.json 的 outputs 为准。

`artifacts.json` 是必需的内部完整性索引；`stage_receipt.json` 是可选运行自证，缺失或写失败不改变原计算退出码。平台任务状态由 task_layer 管理，模块不读写 `Result/tasks/`。

同一 dataRoot 的阶段共用 `.locks/compact_em.run.lock` 排他句柄。锁文件存在不等于正在运行，不要在运行中手工删除；不同 dataRoot 可分别运行。

三个程序正常退出码为 0，已知或标准异常为 1；Extractor 的未知非标准异常为 2，其余程序为 1。错误输出为 UTF-8 QS JSON，机器按 `code`、`severity` 判断，完整登记见安装目录的 `ERROR_CODES.md`。常见处置：

| 问题 | 处置 |
|---|---|
| 设置缺失或无效 | 核对 set_em.json 名称、schemaVersion、必填字段与参数范围 |
| 网格哈希不符 | 确认实际网格，更新 source_em.json 哈希并核对端口 ID |
| 上游工件缺失或过期 | 重跑 Extractor/Solver，不用旧文件拼接新批次 |
| 内存预算不足 | 降低频点并发或在可用资源范围内提高预算；精度回退仍需额外空间 |
| 后处理缺源或采样网格 | 上游选择 post 导出，并准备所需采样输入 |

## 求解行为与规模限制

频域求解使用 Compact 缩减系统的实数变换，保留材料、辅助节点和 MNA_WEIGHT 语义。条件或原方程残差不满足要求时，转入复数缩减或完整电荷精度回退；完整回退分别用电压和电流激励形成 Y/Z。诊断可通过 `--export diagnostics` 查看，程序不会为满足内存预算而关闭精度检查。

P/L 仍为稠密存储，规模约为 O(N²)，稠密分解约为 O(n³)。实际峰值还包括介质修正、频点并发与回退工作区，不能仅由矩阵元素大小推断模型容量。SOLVER_MEMORY_MB 同时受实时可用物理内存和提交额度约束。

## 构建与开发验证

源码构建需要 Windows x64、Visual Studio 2022/MSVC v143 和 Intel oneMKL。设置 `MKLROOT` 指向含 include/lib 的 oneMKL 目录，或设置可解析的 ONEAPI_ROOT。Release 使用 `/MT` 和 MKL LP64 sequential 静态库；运行程序依赖 Windows 系统 DLL。安装使用无需 Python，部分开发验证脚本需要 Python、NumPy 和 jsonschema。

在源码模块目录构建：

```powershell
.\build_pipeline.ps1 -Rebuild
```

构建输出位于 `pipeline/`。源码通过相对路径依赖仓库 `scripts/` 和 `01_Mainline/Shared/`，不宜只复制单个工程目录构建。

在仓库根目录可运行元数据检查，其他验证输出应指定新的外部目录：

```powershell
.\scripts\validate-repository.ps1
python .\scripts\test-compact-review-fixes.py --runtime .\01_Mainline\Quasi_Static_PEEC_Tool_Compact\pipeline --output D:\validation\compact-numerical
python .\01_Mainline\Quasi_Static_PEEC_Tool_Compact\Common\tests\test_file_sha256_lifetime.py --output D:\validation\compact-cng
```

契约、参考地、原生求解及稳定性验证入口分别位于源码 `scripts/test-compact-module-contract.py`、`scripts/test-compact-ground.py`、`tests/compact_solver_optimization/`、`scripts/test-compact-release-stability.py`。参数以各入口帮助为准。

## 项目与交付文件

源码中的 `Model_Extractor/`、`Solver/`、`Post_Processing/` 对应三个阶段，`Common/` 保存共享输入、模型包和完整性约定，`examples/` 保存案例来源快照。使用案例时优先选独立案例包，其输入名已符合当前契约。

模块安装目录严格为三个 EXE、`module.manifest.json`、`README.md`、`ERROR_CODES.md`，共六文件。完整交付的案例、schema、技术资料和验证报告位于模块目录外。设置 schema 在源码同目录或交付 `validation/set.schema.json`，详细输入输出说明位于源码 `INPUT_FORMAT.md`/`OUTPUT_FORMAT.md` 或交付 `technical/`。

README 描述当前功能与操作；版本差异、性能测量和具体运行验收记录见源码 `DEVELOPMENT_NOTES.md` 及交付目录的 RELEASE_NOTES/validation。
