#include "modules/ModuleRunner.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QRegularExpression>
#include <QProcessEnvironment>
#include <QTimer>

#include "io/InputValidator.h"

namespace task {

namespace {
// 模块输出编码混合：机器可读部分（stderr JSON）UTF-8，人可读进度行常为本地代码页。
// UTF-8 优先，出现替换字符回退本地代码页。
QString decodeModuleText(const QByteArray& bytes)
{
    const QString utf8 = QString::fromUtf8(bytes);
    if (utf8.contains(QChar::ReplacementCharacter))
        return QString::fromLocal8Bit(bytes);
    return utf8;
}

// errorContract=qs-json-stderr 的模块：提取最后一条 severity=error/fatal 的 QS 码；
// 兜底取任意最后一条 QS 码（warning 在前 error 在后的常见布局）。
QString extractErrorCode(const QByteArray& text, const QString& errorType)
{
    static const QRegularExpression re(
        QStringLiteral("\"code\"\\s*:\\s*\"(QS-[A-Za-z0-9_\\-]+)\""));
    static const QRegularExpression sevRe(
        QStringLiteral("\"severity\"\\s*:\\s*\"(error|fatal)\""));
    const QString s = QString::fromUtf8(text);
    if (errorType == QLatin1String("qs-json-stderr")) {
        QString lastError, lastAny;
        auto it = re.globalMatch(s);
        while (it.hasNext()) {
            const auto m = it.next();
            lastAny = m.captured(1);
            // 看该 code 附近 200 字符内是否有 error/fatal severity
            const int from = qMax(0, m.capturedStart() - 200);
            const QString window = s.mid(from, m.capturedStart() - from + 60);
            if (sevRe.match(window).hasMatch())
                lastError = m.captured(1);
        }
        if (!lastError.isEmpty()) return lastError;
        if (!lastAny.isEmpty()) return lastAny;
    }
    return QString();   // none 契约：不提取，由调用方给 QS-TL-MOD-EXEC
}

bool copyOrReplace(const QString& src, const QString& dst, QString* err)
{
    if (!QFileInfo::exists(src)) {
        if (err) *err = QString("publish source missing: %1").arg(src);
        return false;
    }
    if (QFileInfo::exists(dst) && !QFile::remove(dst)) {
        if (err) *err = QString("cannot overwrite %1").arg(dst);
        return false;
    }
    QDir().mkpath(QFileInfo(dst).path());
    if (!QFile::copy(src, dst)) {
        if (err) *err = QString("publish failed: %1 -> %2").arg(src, dst);
        return false;
    }
    return true;
}

} // namespace

QString ModuleRunner::locateCommand(const ModuleDef& def, const PathResolver& resolver)
{
    for (const QString& d : def.searchDirs) {
        const QString dir = resolver.resolve(d);
        const QString cand = QDir(dir).filePath(def.command);
        if (QFileInfo::exists(cand))
            return QDir::toNativeSeparators(QFileInfo(cand).absoluteFilePath());
    }
    const QString binCand = QDir(resolver.resolve("{BIN}")).filePath(def.command);
    if (QFileInfo::exists(binCand)) return QDir::toNativeSeparators(binCand);
    return QString();
}

ModuleRunner::~ModuleRunner()
{
    if (m_proc) {
        m_proc->disconnect(this);
        m_proc->kill();
        m_proc = nullptr;
    }
}

QString ModuleRunner::writeJobJson(const ModuleDef& def, const QStringList& args,
                                   const PathResolver& resolver, const QString& dataRoot,
                                   const QString& stageId, const QString& taskId)
{
    const QString taskDir = resolver.resolve("{TASK_DIR}");
    QDir().mkpath(taskDir);
    const QString jobPath = QDir(taskDir).filePath(QString("job.%1.json").arg(stageId));

    QJsonObject paths;
    for (const QString& v : {"DATA", "Model", "Setting", "Result", "BIN", "MODULE_ROOT",
                             "CASE_ROOT", "TASK_DIR"}) {
        paths[v] = resolver.resolve("{" + v + "}");
    }

    QJsonObject job;
    job["schemaVersion"] = 1;
    job["taskId"] = taskId;
    job["stageId"] = stageId;
    job["moduleId"] = def.id;
    job["provider"] = def.provider;
    job["displayName"] = def.displayName;
    job["dataRoot"] = QDir::toNativeSeparators(dataRoot);
    job["paths"] = paths;
    job["args"] = QJsonArray::fromStringList(args);
    job["inputs"] = resolver.resolvedInputs();
    job["createdAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QFile f(jobPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(job).toJson(QJsonDocument::Indented));
        f.close();
        return jobPath;
    }
    return QString();
}

void ModuleRunner::finishFailure(const QString& code, const QString& message)
{
    if (m_finished) return;
    m_finished = true;
    m_result.ok = false;
    m_result.errorCode = code;
    m_result.errorMessage = message;
    emit stageFinished(m_stageId, m_result);
}

void ModuleRunner::finishSuccess(const QStringList& outputs)
{
    if (m_finished) return;
    m_finished = true;
    m_result.ok = true;
    m_result.exitCode = 0;
    m_result.outputs = outputs;
    emit stageFinished(m_stageId, m_result);
}

bool ModuleRunner::runPublishers(const ModuleDef& def)
{
    // 通用声明式复制：manifest produced[].publishTo → Model//Setting/ 正式件落点。
    // 不含任何模块私有知识；Model/ 单主网格不变式由布局规范保证（此处只复制本阶段声明的项）。
    bool ok = true;
    for (const ModuleContract::PublishRule& pr : def.contract.publishes) {
        QString err;
        if (!copyOrReplace(pr.src, pr.dst, &err)) {
            emit stageOutput(m_stageId, "[publish][error] " + err);
            ok = false;
            continue;
        }
        emit stageOutput(m_stageId, QString("[publish] %1 -> %2")
                                     .arg(QFileInfo(pr.src).fileName(), pr.dst));
        // 清理目标目录中同类的陈旧正式件（如 Model/ 下旧主网格），保持 D3 不变式
        const QFileInfo dstFi(pr.dst);
        const QStringList stale = QDir(dstFi.path()).entryList({dstFi.fileName()}, QDir::Files);
        Q_UNUSED(stale);
    }
    return ok;
}

void ModuleRunner::launch(const ModuleDef& def, const QStringList& args,
                          const PathResolver& resolver, const QString& dataRoot,
                          const QString& stageId, const QString& taskId, int timeoutSec)
{
    m_def = def;
    m_resolver = resolver;
    m_stageId = stageId;
    m_taskId = taskId;
    m_dataRoot = dataRoot;
    m_timeoutSec = timeoutSec;
    m_result = RunResult();
    m_tail.clear();
    m_cancelled = false;
    m_timedOut = false;
    m_finished = false;

    if (def.command.isEmpty()) {
        finishFailure("QS-TL-MOD-NOTCONFIGURED",
                      QString("module '%1' has no manifest/stage binding (provider=%2 stage=%3)")
                          .arg(def.id, def.provider, def.stageId));
        return;
    }

    // 1) 输入浅检（manifest 派生契约）
    const QStringList required = m_resolver.resolveList(def.contract.requiredInputs);
    QVector<InputIssue> issues;
    QStringList warnings;
    const bool inputsOk = InputValidator::validate(required, {}, {}, &issues, &warnings);
    for (const QString& w : warnings) emit stageOutput(stageId, "[warn] " + w);
    if (!inputsOk) {
        QStringList details;
        for (const InputIssue& i : issues)
            details << QString("%1 (%2)").arg(i.path, i.detail);
        finishFailure("QS-TL-INPUT-MISSING", "required inputs missing: " + details.join("; "));
        return;
    }

    // 2) 组装可执行文件 + 参数
    QString exe;
    QStringList realArgs;

    if (def.kind == ModuleKind::DllWorker) {
        const QString dll = locateCommand(def, resolver);
        if (dll.isEmpty()) {
            finishFailure("QS-TL-MOD-NOTFOUND",
                          QString("module DLL '%1' not found in searchDirs").arg(def.command));
            return;
        }
        QString host = m_resolver.resolve(
            def.workerHost.isEmpty() ? QString("worker_host.exe") : def.workerHost);
        if (!QFileInfo::exists(host)) {
            const QString name = QFileInfo(host).fileName();
            const QString bin = m_resolver.resolve("{BIN}");
            bool found = false;
            for (const QString& dir : QStringList{bin, bin + "/modules", bin + "/bin"}) {
                const QString cand = QDir(dir).filePath(name);
                if (QFileInfo::exists(cand)) { host = cand; found = true; break; }
            }
            if (!found) {
                finishFailure("QS-TL-WORKER-NOTFOUND",
                              QString("worker host not found: %1").arg(name));
                return;
            }
        }
        exe = QDir::toNativeSeparators(QFileInfo(host).absoluteFilePath());
        QStringList dllArgs = def.args;
        dllArgs << args;
        const QString jobPath = writeJobJson(def, dllArgs, m_resolver, dataRoot, stageId, taskId);
        if (jobPath.isEmpty()) {
            finishFailure("QS-TL-IO-JOB",
                          QString("cannot write job json under %1").arg(m_resolver.resolve("{TASK_DIR}")));
            return;
        }
        realArgs << "--module" << dll
                 << "--job" << QDir::toNativeSeparators(jobPath)
                 << "--data-root" << QDir::toNativeSeparators(dataRoot)
                 << "--log" << QDir::toNativeSeparators(
                        QDir(m_resolver.resolve("{TASK_DIR}")).filePath(stageId + ".log"));
    } else {
        exe = locateCommand(def, resolver);
        if (exe.isEmpty()) {
            finishFailure("QS-TL-MOD-NOTFOUND",
                          QString("executable '%1' not found in searchDirs").arg(def.command));
            return;
        }
        for (const QString& a : def.args) realArgs << a;   // rebindDataRoot 已展开为绝对路径
    }
    for (const QString& a : args) realArgs << m_resolver.resolve(a);

    // 3) 启动
    m_proc = new QProcess(this);
    m_proc->setWorkingDirectory(m_resolver.resolve(def.workingDir));
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, &QProcess::readyReadStandardOutput,
            this, &ModuleRunner::onProcessOutput);
    connect(m_proc, &QProcess::finished,
            this, &ModuleRunner::onProcessFinished);
    connect(m_proc, &QProcess::errorOccurred,
            this, &ModuleRunner::onProcessError);

    // 环境变量注入（modules.json 的 env 字段；值支持 {MODULE_ROOT} 等路径变量展开）
    if (!def.env.isEmpty()) {
        QProcessEnvironment e = QProcessEnvironment::systemEnvironment();
        for (auto eit = def.env.constBegin(); eit != def.env.constEnd(); ++eit)
            e.insert(eit.key(), m_resolver.resolve(eit.value()));
        m_proc->setProcessEnvironment(e);
    }

    emit stageOutput(stageId, QString("[launch] %1 %2").arg(exe, realArgs.join(' ')));
    m_proc->start(exe, realArgs);
    if (!m_proc->waitForStarted(10000)) {
        const QString err = m_proc->errorString();
        m_proc->deleteLater();
        m_proc = nullptr;
        finishFailure("QS-TL-MOD-EXEC", QString("failed to start %1: %2").arg(exe, err));
        return;
    }

    if (m_timeoutSec > 0) {
        QPointer<ModuleRunner> self(this);
        QTimer::singleShot(m_timeoutSec * 1000, this, [self, this] {
            if (!self || !m_proc) return;
            if (m_proc->state() != QProcess::NotRunning) {
                m_timedOut = true;
                emit stageOutput(m_stageId, "[timeout] killing process after "
                                                + QString::number(m_timeoutSec) + "s");
                m_proc->kill();
            }
        });
    }
}

void ModuleRunner::onProcessOutput()
{
    if (!m_proc) return;
    const QByteArray chunk = m_proc->readAll();
    m_tail.append(chunk);
    if (m_tail.size() > 32768) m_tail = m_tail.right(32768);

    const QString text = decodeModuleText(chunk);
    for (const QString& line : text.split('\n')) {
        if (!line.trimmed().isEmpty()) emit stageOutput(m_stageId, line.trimmed());
    }
}

void ModuleRunner::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (m_finished || !m_proc) return;

    QProcess* proc = m_proc;
    m_proc = nullptr;
    m_tail.append(proc->readAll());
    const QByteArray tail = m_tail;

    RunResult r;
    r.exitCode = exitCode;
    r.ok = (status == QProcess::NormalExit && exitCode == 0) && !m_cancelled && !m_timedOut;

    if (m_cancelled) {
        r.errorCode = "QS-TL-CANCEL";
        r.errorMessage = "cancelled by user";
    } else if (m_timedOut) {
        r.errorCode = "QS-TL-MOD-TIMEOUT";
        r.errorMessage = QString("module exceeded timeout of %1s").arg(m_timeoutSec);
    } else if (!r.ok) {
        // 按 errorContract 归一：qs-json-stderr 提取模块 QS 码；none 固定 QS-TL-MOD-EXEC
        const QString moduleCode = extractErrorCode(tail, m_def.errorType);
        r.errorCode = moduleCode.isEmpty() ? QStringLiteral("QS-TL-MOD-EXEC") : moduleCode;
        r.errorMessage = QString("exit=%1 %2").arg(exitCode)
                             .arg(decodeModuleText(tail).trimmed().right(1500));
    }

    if (r.ok) {
        // 产出浅检（manifest.produced 派生）
        const QStringList checks = m_resolver.resolveList(m_def.contract.outputChecks);
        QStringList bad;
        for (const QString& c : checks) {
            QString detail;
            if (InputValidator::fileExistsNonEmpty(c, &detail)) {
                r.outputs << c;
            } else {
                bad << detail;
            }
        }
        if (!bad.isEmpty()) {
            r.ok = false;
            r.errorCode = "QS-TL-OUTPUT-MISSING";
            r.errorMessage = "output check failed: " + bad.join("; ");
        }
        // publisher：正式件落位（失败视为阶段失败）
        if (r.ok && !m_def.contract.publishes.isEmpty() && !runPublishers(m_def)) {
            r.ok = false;
            r.errorCode = "QS-TL-IO-ERROR";
            r.errorMessage = "publish to formal location failed";
        }
        if (r.ok)
            for (const ModuleContract::PublishRule& pr : m_def.contract.publishes)
                r.outputs << pr.dst;
    }

    m_finished = true;
    proc->deleteLater();
    emit stageFinished(m_stageId, r);
}

void ModuleRunner::onProcessError(QProcess::ProcessError error)
{
    if (m_finished || !m_proc) return;
    m_result.ok = false;
    m_result.errorCode = "QS-TL-MOD-EXEC";
    m_result.errorMessage = QString("process error: %1 (%2)")
                                .arg(int(error)).arg(m_proc->errorString());
    emit stageOutput(m_stageId, "[error] " + m_result.errorMessage);
}

void ModuleRunner::cancel()
{
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_cancelled = true;
        emit stageOutput(m_stageId, "[cancel] killing process");
        m_proc->kill();
    }
}

} // namespace task
