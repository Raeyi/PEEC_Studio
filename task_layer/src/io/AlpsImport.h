// AlpsImport.h — ALPS 远程仿真结果导入（平台侧 OP0 落位 + 新鲜度守卫，2026-11-12 ALPS 集成）
//
// 背景（DATA_LAYOUT v1.7）：alpsrunner 模块（tools/alpsrunner/ALPSRunner.exe）的
//   manifest 声明 produced=[]（无固定 OP0 路径、无 publishTo）——OP0 落位是平台
//   责任（契约 §6.7）：
//   * runner 把运行 bundle 写到 Result/alpsrunner/simulate/<run_id>/（自由区，一次运行一个子目录）；
//   * 平台在阶段退出 0 后识别新增 run bundle，执行 run_summary 门 + SHA256SUMS 复核 +
//     唯一 .op0 条目 + 三向新鲜度守卫，然后把 OP0 拷贝到 thermal post 的固定读取点
//     Result/thermal/alps/thermal.op0，并写 import_receipt.json 留痕。
// 双入口（用户确认的双通道设计）：
//   * 流水线内置：TaskOrchestrator 在声明了 op0Import 的阶段退出 0 后调 importNewRunBundle()
//     （前后子目录 diff，禁止猜测"最近一次"）；
//   * 独立入口：JSON-RPC alps.importOp0 / CLI alps-import-op0 调 importFromRunDir()
//     （显式指定 run bundle 子目录名）。
// 错误码 QS-TL-ALPS-* 见 task_layer/ERROR_CODES.md §2.8（先落地代码抛出点，再回填文档）。
#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace task {
namespace alps {

// 运行 bundle 基准目录（相对 dataRoot，POSIX 分隔符）：一次 ALPS 运行 = 一个子目录 <run_id>
inline constexpr const char* const kSimulateBase = "Result/alpsrunner/simulate";
// ALPS 固定输入网表（manifest consumed: user/alpsrunner.netlist，maintainedBy ui）
inline constexpr const char* const kNetlistRel = "Model/ThermalNetList.sp";
// thermal extract 的 binding（新鲜度第三向：netlistSha256）
inline constexpr const char* const kBindingRel = "Result/thermal/extract/thermal_binding.json";
// OP0 缺省落位（DATA_LAYOUT v1.7：thermal post 固定读取点）
inline constexpr const char* const kDefaultOp0Dest = "Result/thermal/alps/thermal.op0";

struct ImportResult {
    bool ok = false;
    QString errorCode;      // QS-TL-ALPS-NORUN / -MULTIRUN / -BADSUMMARY / -BADSHA / -STALE / -IO
    QString errorMessage;   // 面向用户的细节
    QString runId;          // run bundle 子目录名（ok 时）
    QString op0Sha256;      // 已导入 OP0 的 sha256（小写 hex）
    QString inputSha256;    // run_summary.input_sha256（ok 时）
    QJsonObject receipt;    // import_receipt.json 内容（ok 时）
};

// 列出 Result/alpsrunner/simulate/ 下全部 run bundle 子目录名（按名称排序）。
// 基准目录不存在时返回空表（目录尚未产生 = 合法状态，不是错误）。
QStringList listRunBundles(const QString& dataRoot);

// 从显式指定的 run bundle 导入 OP0（runDir = 基准目录下的子目录名；
// 也接受 "Result/alpsrunner/simulate/<name>" 全相对路径形式，归一后等价）。
// 校验顺序：bundle 目录存在 → run_summary.json 门（schema_version==1 + 四条件成功 +
// input_sha256 非空 + run_id 与目录名一致）→ SHA256SUMS 复核 + 唯一 .op0 条目 →
// 三向新鲜度守卫（run_summary.input_sha256 == sha256(Model/ThermalNetList.sp) ==
// thermal_binding.json.netlistSha256）→ 拷贝 OP0 到 op0DestRel 并写 receipt。
// op0DestRel 为空时用 kDefaultOp0Dest；source ∈ {"pipeline","manual"} 记录入 receipt。
ImportResult importFromRunDir(const QString& dataRoot, const QString& runDir,
                              const QString& op0DestRel = QString(),
                              const QString& source = "manual");

// 流水线内置形态：diff 阶段调用前后的子目录集合（preRunDirs = 调用前 listRunBundles() 的快照），
// 要求恰好新增 1 个 run bundle，然后执行 importFromRunDir。
// 0 个新增 → QS-TL-ALPS-NORUN；>1 个 → QS-TL-ALPS-MULTIRUN。
ImportResult importNewRunBundle(const QString& dataRoot, const QStringList& preRunDirs,
                                const QString& op0DestRel = QString(),
                                const QString& source = "pipeline");

} // namespace alps
} // namespace task
