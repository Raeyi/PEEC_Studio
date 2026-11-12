// AlpsImport.cpp — ALPS 远程仿真结果导入实现（平台侧 OP0 落位 + 新鲜度守卫）
//
// 校验链（任一失败即中止，错误码见 task_layer/ERROR_CODES.md §2.8）：
//   1) QS-TL-ALPS-NORUN    run bundle 不存在 / 前后 diff 无新增（CLI/JSON-RPC 传错目录、
//                          流水线形态 runner 未产生新 run 目录）
//   2) (流水线形态) QS-TL-ALPS-MULTIRUN  前后 diff 新增 >1 个 run 目录（并发/脏环境）
//   3) QS-TL-ALPS-BADSUMMARY  run_summary.json 缺读/非对象/schema_version≠1/run_id 与目录名
//                          不一致/四条件门不满足（status=succeeded ∧ result_scope=full ∧
//                          simulator_exit_code=0 ∧ download_status=verified）/ input_sha256 空
//   4) QS-TL-ALPS-BADSHA    SHA256SUMS.txt 缺读/无 .op0 条目/多个 .op0 条目/OP0 文件缺失或
//                          实际 sha 与记录不符/拷贝后复核不符
//   5) QS-TL-ALPS-STALE     三向新鲜度守卫失败：Model/ThermalNetList.sp 或
//                          thermal_binding.json 缺失，或
//                          run_summary.input_sha256 ≠ sha256(Model/ThermalNetList.sp) ≠
//                          thermal_binding.json.netlistSha256（防"用旧 OP0 跑新算例"）
//   6) QS-TL-ALPS-IO        目标目录不可写 / OP0 拷贝失败 / receipt 写失败
#include "io/AlpsImport.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>

namespace task {
namespace alps {

namespace {

// 相对路径（POSIX 分隔符）→ dataRoot 下原生绝对路径
QString toNativeRel(const QString& dataRoot, const QString& relPosix)
{
    QString p = QDir::cleanPath(dataRoot + "/" + relPosix);
    p.replace('/', QDir::separator());
    return p;
}

// 文件 sha256（小写 hex）；失败（缺文件/不可读）返回空串
QString sha256File(const QString& dataRoot, const QString& relPosix)
{
    QFile f(toNativeRel(dataRoot, relPosix));
    if (!f.open(QIODevice::ReadOnly)) return QString();
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!h.addData(&f)) {
        f.close();
        return QString();
    }
    f.close();
    return QString::fromLatin1(h.result().toHex()).toLower();
}

// run_summary.json 门（schema_version 1）：四条件成功 + input_sha256 非空 + run_id 与目录名一致
bool runSummaryGate(const QJsonObject& s, const QString& bundleDirName,
                    QString* runId, QString* inputSha,
                    QString* code, QString* msg)
{
    if (s.value("schema_version").toInt(-1) != 1) {
        *code = QStringLiteral("QS-TL-ALPS-BADSUMMARY");
        *msg = QStringLiteral("run_summary.json schema_version != 1");
        return false;
    }
    *runId = s.value("run_id").toString();
    *inputSha = s.value("input_sha256").toString().trimmed().toLower();
    if (runId->isEmpty()) {
        *code = QStringLiteral("QS-TL-ALPS-BADSUMMARY");
        *msg = QStringLiteral("run_summary.json: run_id missing/empty");
        return false;
    }
    if (inputSha->isEmpty()) {
        *code = QStringLiteral("QS-TL-ALPS-BADSUMMARY");
        *msg = QStringLiteral("run_summary.json: input_sha256 missing/empty");
        return false;
    }
    if (*runId != bundleDirName) {
        *code = QStringLiteral("QS-TL-ALPS-BADSUMMARY");
        *msg = QStringLiteral("run_summary.json run_id '%1' != bundle dir name '%2'")
                   .arg(*runId, bundleDirName);
        return false;
    }
    if (s.value("status").toString() != QStringLiteral("succeeded")) {
        *code = QStringLiteral("QS-TL-ALPS-BADSUMMARY");
        *msg = QStringLiteral("run_summary.json status='%1' (expected 'succeeded')")
                   .arg(s.value("status").toString());
        return false;
    }
    if (s.value("result_scope").toString() != QStringLiteral("full")) {
        *code = QStringLiteral("QS-TL-ALPS-BADSUMMARY");
        *msg = QStringLiteral("run_summary.json result_scope='%1' (expected 'full')")
                   .arg(s.value("result_scope").toString());
        return false;
    }
    if (s.value("simulator_exit_code").toInt(-1) != 0) {
        *code = QStringLiteral("QS-TL-ALPS-BADSUMMARY");
        *msg = QStringLiteral("run_summary.json simulator_exit_code=%1 (expected 0)")
                   .arg(s.value("simulator_exit_code").toInt());
        return false;
    }
    if (s.value("download_status").toString() != QStringLiteral("verified")) {
        *code = QStringLiteral("QS-TL-ALPS-BADSUMMARY");
        *msg = QStringLiteral("run_summary.json download_status='%1' (expected 'verified')")
                   .arg(s.value("download_status").toString());
        return false;
    }
    return true;
}

// SHA256SUMS.txt 中唯一的 .op0 条目（排除 .dcop0）：
//   格式 "<64位hex>  ./相对路径"（sha256sum 文本模式两空格；二进制模式第二字符为 *）
bool soleOp0Entry(const QString& sumsText, QString* op0Rel, QString* sha,
                  QString* code, QString* msg)
{
    // 注意：C++ 字面量里 \\S 才是正则 \S（未知转义 \S 会被编译器丢弃反斜杠）
    const QRegularExpression re(QStringLiteral("^[0-9a-fA-F]{64}[ ][ *]?([^ ]\\S*)$"));
    QString foundRel;
    QString foundSha;
    for (const QString& raw : sumsText.split('\n')) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        const auto m = re.match(line);
        if (!m.hasMatch()) continue;
        const QString path = m.captured(1);
        if (!path.endsWith(QStringLiteral(".op0"))) continue;   // .dcop0 不以 .op0 结尾之外的前缀混入：见下
        if (path.endsWith(QStringLiteral(".dcop0"))) continue;
        if (!foundRel.isEmpty()) {
            *code = QStringLiteral("QS-TL-ALPS-BADSHA");
            *msg = QStringLiteral("SHA256SUMS.txt: multiple .op0 entries (%1 and %2)")
                       .arg(foundRel, path);
            return false;
        }
        foundRel = path;
        foundSha = line.left(64).toLower();
    }
    if (foundRel.isEmpty()) {
        *code = QStringLiteral("QS-TL-ALPS-BADSHA");
        *msg = QStringLiteral("SHA256SUMS.txt: no .op0 entry found");
        return false;
    }
    *op0Rel = foundRel;
    *sha = foundSha;
    return true;
}

// thermal_binding.json 的 netlistSha256（小写）；缺失/不可读返回空串
QString bindingNetlistSha(const QString& dataRoot)
{
    QFile f(toNativeRel(dataRoot, kBindingRel));
    if (!f.open(QIODevice::ReadOnly)) return QString();
    const QJsonDocument d = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!d.isObject()) return QString();
    return d.object().value(QStringLiteral("netlistSha256")).toString().trimmed().toLower();
}

// 拷贝覆盖（目标父目录自动创建）：成功返回空串，失败返回错误描述
QString copyTo(const QString& srcAp, const QString& dstAp)
{
    QDir().mkpath(QFileInfo(dstAp).path());
    if (QFile::exists(dstAp) && !QFile::remove(dstAp))
        return QStringLiteral("cannot remove existing %1").arg(dstAp);
    if (!QFile::copy(srcAp, dstAp))
        return QStringLiteral("copy failed: %1 -> %2").arg(srcAp, dstAp);
    return QString();
}

ImportResult doImport(const QString& dataRoot, const QString& bundleDirName,
                      const QString& op0DestRel, const QString& source)
{
    ImportResult r;
    // ---- 1) run bundle 目录必须存在（显式子目录，不猜"最近一次"） ----
    const QString bundleDirPosix = QString::fromLatin1(kSimulateBase) + "/" + bundleDirName;
    const QFileInfo bfi(toNativeRel(dataRoot, bundleDirPosix));
    if (!bfi.exists() || !bfi.isDir()) {
        r.errorCode = QStringLiteral("QS-TL-ALPS-NORUN");
        r.errorMessage = QStringLiteral("run bundle not found: %1").arg(bundleDirPosix);
        return r;
    }
    // ---- 2) run_summary.json 门 ----
    QFile sf(toNativeRel(dataRoot, bundleDirPosix + "/run_summary.json"));
    if (!sf.open(QIODevice::ReadOnly)) {
        r.errorCode = QStringLiteral("QS-TL-ALPS-BADSUMMARY");
        r.errorMessage = QStringLiteral("cannot read run_summary.json in %1").arg(bundleDirPosix);
        return r;
    }
    const QByteArray sb = sf.readAll();
    sf.close();
    const QJsonDocument sd = QJsonDocument::fromJson(sb);
    if (!sd.isObject()) {
        r.errorCode = QStringLiteral("QS-TL-ALPS-BADSUMMARY");
        r.errorMessage = QStringLiteral("run_summary.json is not a JSON object");
        return r;
    }
    QString runId, inputSha, gCode, gMsg;
    if (!runSummaryGate(sd.object(), bundleDirName, &runId, &inputSha, &gCode, &gMsg)) {
        r.errorCode = gCode;
        r.errorMessage = gMsg;
        return r;
    }
    r.runId = runId;
    r.inputSha256 = inputSha;
    // ---- 3) SHA256SUMS 复核 + 唯一 .op0 条目 ----
    QFile uf(toNativeRel(dataRoot, bundleDirPosix + "/SHA256SUMS.txt"));
    if (!uf.open(QIODevice::ReadOnly)) {
        r.errorCode = QStringLiteral("QS-TL-ALPS-BADSHA");
        r.errorMessage = QStringLiteral("cannot read SHA256SUMS.txt in %1").arg(bundleDirPosix);
        return r;
    }
    const QString sumsText = QString::fromUtf8(uf.readAll());
    uf.close();
    QString op0Rel, op0Sha, sCode, sMsg;
    if (!soleOp0Entry(sumsText, &op0Rel, &op0Sha, &sCode, &sMsg)) {
        r.errorCode = sCode;
        r.errorMessage = sMsg;
        return r;
    }
    while (op0Rel.startsWith(QStringLiteral("./")))
        op0Rel.remove(0, 2);
    const QString op0RelInBundle = bundleDirPosix + "/" + op0Rel;
    const QString actualSha = sha256File(dataRoot, op0RelInBundle);
    if (actualSha.isEmpty()) {
        r.errorCode = QStringLiteral("QS-TL-ALPS-BADSHA");
        r.errorMessage = QStringLiteral("OP0 file missing in run bundle: %1").arg(op0RelInBundle);
        return r;
    }
    if (actualSha != op0Sha) {
        r.errorCode = QStringLiteral("QS-TL-ALPS-BADSHA");
        r.errorMessage = QStringLiteral("OP0 sha mismatch: recorded %1, actual %2 (%3)")
                            .arg(op0Sha, actualSha, op0RelInBundle);
        return r;
    }
    // ---- 4) 三向新鲜度守卫 ----
    // run_summary.input_sha256 == sha256(当前 Model/ThermalNetList.sp) == binding.netlistSha256
    // 三者任一不一致 = OP0 与当前网表/热网络不配套（防"用旧 OP0 跑新算例"）
    const QString curSha = sha256File(dataRoot, kNetlistRel);
    const QString bndSha = bindingNetlistSha(dataRoot);
    if (curSha.isEmpty() || bndSha.isEmpty()) {
        r.errorCode = QStringLiteral("QS-TL-ALPS-STALE");
        r.errorMessage = QStringLiteral(
            "freshness guard: missing %1 or %2 (netlist not published / thermal extract not run)")
            .arg(QString::fromLatin1(kNetlistRel), QString::fromLatin1(kBindingRel));
        return r;
    }
    if (curSha != inputSha || curSha != bndSha) {
        r.errorCode = QStringLiteral("QS-TL-ALPS-STALE");
        r.errorMessage = QStringLiteral(
            "freshness guard: run_summary.input_sha256=%1, sha256(%2)=%3, "
            "thermal_binding.netlistSha256=%4 — OP0 does not match current netlist")
            .arg(inputSha, QString::fromLatin1(kNetlistRel), curSha, bndSha);
        return r;
    }
    // ---- 5) 拷贝 OP0 到固定读取点 + 写 receipt ----
    const QString dstRel = op0DestRel.isEmpty() ? QString::fromLatin1(kDefaultOp0Dest)
                                                : op0DestRel;
    const QString dstAp = toNativeRel(dataRoot, dstRel);
    const QString cerr = copyTo(toNativeRel(dataRoot, op0RelInBundle), dstAp);
    if (!cerr.isEmpty()) {
        r.errorCode = QStringLiteral("QS-TL-ALPS-IO");
        r.errorMessage = cerr;
        return r;
    }
    const QString dstSha = sha256File(dataRoot, dstRel);
    if (dstSha != op0Sha) {
        r.errorCode = QStringLiteral("QS-TL-ALPS-BADSHA");
        r.errorMessage = QStringLiteral("post-copy verification failed for %1").arg(dstRel);
        return r;
    }
    r.op0Sha256 = op0Sha;
    QJsonObject receipt;
    receipt.insert(QStringLiteral("schemaVersion"), 1);
    receipt.insert(QStringLiteral("runId"), runId);
    receipt.insert(QStringLiteral("srcRel"), op0RelInBundle);
    receipt.insert(QStringLiteral("op0Dest"), dstRel);
    receipt.insert(QStringLiteral("op0Sha256"), op0Sha);
    receipt.insert(QStringLiteral("inputSha256"), inputSha);
    receipt.insert(QStringLiteral("importedAt"),
                   QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    receipt.insert(QStringLiteral("source"), source);
    QFile rf(QDir(QFileInfo(dstAp).path()).filePath(QStringLiteral("import_receipt.json")));
    if (!rf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        r.errorCode = QStringLiteral("QS-TL-ALPS-IO");
        r.errorMessage = QStringLiteral(
            "cannot write import_receipt.json beside %1").arg(dstRel);
        return r;
    }
    rf.write(QJsonDocument(receipt).toJson(QJsonDocument::Indented));
    rf.close();
    r.receipt = receipt;
    r.ok = true;
    return r;
}

} // namespace

QStringList listRunBundles(const QString& dataRoot)
{
    const QString base = toNativeRel(dataRoot, kSimulateBase);
    QDir d(base);
    if (!d.exists()) return QStringList();
    const QFileInfoList dirs = d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    QStringList out;
    for (const QFileInfo& fi : dirs)
        out << fi.fileName();
    return out;
}

ImportResult importFromRunDir(const QString& dataRoot, const QString& runDir,
                              const QString& op0DestRel, const QString& source)
{
    ImportResult r;
    QString clean = runDir.trimmed().replace('\\', '/');
    while (clean.startsWith(QStringLiteral("./")))
        clean.remove(0, 2);
    const QString baseSlash = QString::fromLatin1(kSimulateBase) + "/";
    if (clean.startsWith(baseSlash))
        clean.remove(0, baseSlash.size());
    if (clean.isEmpty() || clean.contains(QStringLiteral("..")) ||
        clean.contains(QLatin1Char('/')) || clean.contains(QChar(0))) {
        r.errorCode = QStringLiteral("QS-TL-ALPS-NORUN");
        r.errorMessage = QStringLiteral(
            "runDir must be a run-bundle subdirectory name directly under %1")
        .arg(QString::fromLatin1(kSimulateBase));
        return r;
    }
    return doImport(dataRoot, clean, op0DestRel, source);
}

ImportResult importNewRunBundle(const QString& dataRoot, const QStringList& preRunDirs,
                                const QString& op0DestRel, const QString& source)
{
    ImportResult r;
    const QStringList now = listRunBundles(dataRoot);
    QStringList added;
    for (const QString& d : now)
        if (!preRunDirs.contains(d))
            added << d;
    if (added.isEmpty()) {
        r.errorCode = QStringLiteral("QS-TL-ALPS-NORUN");
        r.errorMessage = QStringLiteral(
            "no new run bundle under %1 (runner exited 0 but produced no run dir)")
            .arg(QString::fromLatin1(kSimulateBase));
        return r;
    }
    if (added.size() > 1) {
        r.errorCode = QStringLiteral("QS-TL-ALPS-MULTIRUN");
        r.errorMessage = QStringLiteral("multiple new run bundles: %1")
                            .arg(added.join(QStringLiteral(", ")));
        return r;
    }
    return doImport(dataRoot, added.first(), op0DestRel, source);
}

} // namespace alps
} // namespace task
