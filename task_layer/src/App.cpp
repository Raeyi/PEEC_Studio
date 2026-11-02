#include "App.h"

#include <cstdio>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QTextStream>
#include <QUuid>

#include "io/InputValidator.h"
#include "modules/ModuleRunner.h"

namespace task {

namespace {
// 流水线 stage 的 artifacts 覆盖 → 按 moduleId 归组（跨模块工件路由注入 rebind）
QMap<QString, QMap<QString, QString>> artifactOverridesOf(const PipelineDef& pipe)
{
    QMap<QString, QMap<QString, QString>> ov;
    for (const PipelineStage& s : pipe.stages)
        for (auto it = s.artifacts.constBegin(); it != s.artifacts.constEnd(); ++it)
            ov[s.moduleId][it.key()] = it.value();
    return ov;
}
} // namespace

App::App(const QString& configDir, const QString& moduleRoot, const QString& binRoot,
         QObject* parent)
    : QObject(parent), m_configDir(configDir), m_moduleRoot(moduleRoot),
      m_binRoot(binRoot)
{
    m_orchestrator = new TaskOrchestrator(this);
    connect(m_orchestrator, &TaskOrchestrator::notify, this, &App::onNotify);
}

bool App::loadConfig(QString* detail)
{
    QString err;
    if (!m_registry.loadRouting(m_configDir + "/modules.json", &err)) {
        if (detail) *detail = err;
        return false;
    }
    QStringList missing;
    if (!m_registry.loadManifests(m_moduleRoot, {}, &missing, &err)) {
        if (detail) *detail = err;
        return false;
    }
//     // 聚合校验（注册期）：ref 路由 / DAG / apiVersion 窗口 / 布局越权
//     QMap<QString, RefRoute> routes;
//     QStringList errors, warnings;
//     const QStringList pkgDirs = m_registry.manifests().keys().toList();
//     if (!ManifestLoader::validateAggregate(m_registry.manifests(), pkgDirs,
//                                            &routes, &errors, &warnings)) {
//         if (detail) *detail = errors.join("; ");
//         return false;
//     }
//     for (const QString& w : warnings + missing)
//         qWarning("[config] %s", qPrintable(w));
//     m_refRoutes = routes;
    if (!m_loader.loadFromFile(m_configDir + "/pipelines.json", &err)) {
        if (detail) *detail = err;
        return false;
    }
    return true;
}

int App::serve()
{
    QString err;
    if (!loadConfig(&err)) {
        // 配置错误也要走协议回复，让 UI 能拿到错误码（直接写 stdout，不启动传输层）
        const QByteArray line = rpc::makeError(QJsonValue(), rpc::ErrConfig, err);
        std::fwrite(line.constData(), 1, size_t(line.size()), stdout);
        std::fflush(stdout);
        return 2;
    }

    m_transport = new rpc::StdioTransport(this);
    connect(m_transport, &rpc::StdioTransport::requestReceived,
            this, &App::handleRequest);
    connect(m_transport, &rpc::StdioTransport::inputClosed, this, [this] {
        const QString running = m_orchestrator->currentTaskId();
        if (!running.isEmpty()) m_orchestrator->cancel(running);
        emit exitRequested(0);
    });
    connect(this, &App::exitRequested, this, [](int code) {
        QCoreApplication::exit(code);
    });
    return 0;
}

int App::runOnce(const QString& pipelineId, const QString& dataRoot,
                 const QJsonObject& overrides, const QJsonObject& inputs)
{
    QString err;
    if (!loadConfig(&err)) {
        QTextStream(stdout) << "config error: " << err << "\n";
        return 2;
    }
    const PipelineDef* pipe = m_loader.pipeline(pipelineId);
    if (!pipe) {
        QTextStream(stdout) << "unknown pipeline: " << pipelineId << "\n";
        return 2;
    }
    const QString taskId = QStringLiteral("cli_%1")
                               .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    const QString root = QDir::cleanPath(QFileInfo(dataRoot).absoluteFilePath());
    m_activeTaskId = taskId;
    m_activeDataRoot = root;
    m_activeTaskDir = root + "/Result/tasks/" + taskId;
    m_activeInputs = inputs;
    m_resolver.setBase(root, m_moduleRoot, m_binRoot, m_activeTaskDir);
    m_resolver.setInputs(inputs);
    const QMap<QString, QMap<QString, QString>> ov = artifactOverridesOf(*pipe);
    m_registry.rebindDataRoot(root, &ov);   // 展开 args/契约路径（含 stage artifacts 路由）
    m_store.open(m_activeTaskDir, taskId, pipelineId);
    m_store.setInputs(m_resolver.resolvedInputs());

    connect(m_orchestrator, &TaskOrchestrator::notify, this,
            [](const QString& m, const QJsonObject& p) {
        if (m == "task.stage.log") return;
        QTextStream(stdout) << "[event] " << m << " "
                            << QString::fromUtf8(QJsonDocument(p).toJson(QJsonDocument::Compact))
                            << "\n";
        QTextStream(stdout).flush();
    });

    if (!m_orchestrator->startTask(taskId, *pipe, m_registry, m_resolver, root, overrides)) {
        QTextStream(stdout) << "task failed to start (another task running?)\n";
        return 1;
    }
    return 0;
}

int App::validateOnce(const QString& dataRoot, const QString& pipelineId,
                      const QJsonObject& inputs)
{
    QString err;
    if (!loadConfig(&err)) {
        QTextStream(stdout) << "config error: " << err << "\n";
        return 2;
    }
    const QJsonObject rep = inspectInputs(dataRoot, pipelineId, QString(), inputs);
    QTextStream(stdout) << QString::fromUtf8(QJsonDocument(rep).toJson(QJsonDocument::Indented));
    QTextStream(stdout).flush();
    return rep.value("valid").toBool() ? 0 : 1;
}

QJsonObject App::inspectInputs(const QString& dataRoot, const QString& pipelineId,
                               const QString& moduleId, const QJsonObject& inputs)
{
    const QString root = QDir::cleanPath(QFileInfo(dataRoot).absoluteFilePath());
    PathResolver r;
    r.setBase(root, m_moduleRoot, m_binRoot,
              QDir(root).filePath("Result/tasks/_validate"));
    r.setInputs(inputs);
    // 校验用临时 rebind：把契约路径展开到本 dataRoot（不改活动任务状态）
    QMap<QString, QMap<QString, QString>> vov;
    if (!pipelineId.isEmpty() && m_loader.pipeline(pipelineId))
        vov = artifactOverridesOf(*m_loader.pipeline(pipelineId));
    m_registry.rebindDataRoot(root, vov.isEmpty() ? nullptr : &vov);

    // 只校验"入口阶段"（无 dependsOn）的输入契约：这是"这条流水线能否启动"的判据。
    QStringList entryModuleIds;
    if (!moduleId.isEmpty()) {
        entryModuleIds << moduleId;
    } else if (!pipelineId.isEmpty()) {
        const PipelineDef* p = m_loader.pipeline(pipelineId);
        if (p) {
            for (const PipelineStage& s : p->stages)
                if (s.dependsOn.isEmpty()) entryModuleIds << s.moduleId;
        }
    } else {
        for (const PipelineDef& p : m_loader.allPipelines())
            for (const PipelineStage& s : p.stages)
                if (s.dependsOn.isEmpty()) entryModuleIds << s.moduleId;
    }

    QVector<InputIssue> issues;
    QStringList warnings;
    QSet<QString> seen;
    QStringList required;
    for (const QString& mid : entryModuleIds) {
        const ModuleDef* d = m_registry.module(mid);
        if (!d) continue;
        for (const QString& path : d->contract.requiredInputs) {
            if (seen.contains(path)) continue;
            seen.insert(path);
            required << path;
        }
    }

    bool ok = InputValidator::validate(required, {}, {}, &issues, &warnings);

    // 端口绑定预检：EM 链入口检查 Model/source_em.json（DATA_LAYOUT v1.2 D3/I3；
    // 过渡期同时接受旧名 port_em.json —— 当前 compact_em 2026.10.30 版仍实读旧名）
    const QString modelDir = r.resolve("{Model}");
    QString portEm;
    for (const QString& name : { "source_em.json", "port_em.json" })
        if (QFileInfo::exists(QDir(modelDir).filePath(name))) {
            portEm = QDir(modelDir).filePath(name);
            break;
        }
    const bool emEntry = [this, &entryModuleIds]() {
        for (const QString& mid : entryModuleIds) {
            const ModuleDef* d = m_registry.module(mid);
            if (d && d->provider == "compact_em") return true;
        }
        return false;
    }();
    if (emEntry && !portEm.isEmpty()) {
        if (!InputValidator::validatePortFile(portEm, modelDir, &issues, &warnings))
            ok = false;
    } else if (emEntry) {
        warnings << "Model/source_em.json not present yet - UI must write it "
                   "(triangleIds picked on the mesh) before running peec_basic";
    }

    QJsonArray issueArr;
    for (const InputIssue& i : issues)
        issueArr.append(QJsonObject{{"path", i.path}, {"reason", i.reason}, {"detail", i.detail}});
    QJsonArray warnArr;
    for (const QString& w : warnings) warnArr.append(w);
    QJsonArray checkedArr;
    for (const QString& p : required) checkedArr.append(p);

    QJsonObject rep{{"valid", ok},
                    {"dataRoot", root},
                    {"checked", checkedArr},
                    {"issues", issueArr},
                    {"warnings", warnArr}};
    const QJsonObject resolved = r.resolvedInputs();
    if (!resolved.isEmpty()) rep["inputs"] = resolved;
    return rep;
}

void App::onNotify(const QString& method, const QJsonObject& params)
{
    // 同步 ResultStore 状态并持久化
    if (method == "task.stage.log") {
        const QString line = params.value("line").toString();
        m_store.appendLog(QString("[%1] %2")
                              .arg(params.value("stageId").toString(), line.left(4000)));
    } else if (method == "task.stage.started") {
        StageRecord rec;
        rec.id = params.value("stageId").toString();
        rec.moduleId = params.value("module").toString();
        rec.name = params.value("name").toString();
        rec.status = StageStatus::Running;
        rec.startedAt = QDateTime::currentMSecsSinceEpoch();
        m_store.setStage(rec);
        m_store.appendLog(QString("stage %1 started (%2)").arg(rec.id, rec.name));
    } else if (method == "task.stage.finished") {
        StageRecord rec;
        rec.id = params.value("stageId").toString();
        rec.moduleId = params.value("module").toString();
        rec.name = params.value("name").toString();
        const QString st = params.value("status").toString();
        rec.status = st == "done" ? StageStatus::Done
                   : st == "skipped" ? StageStatus::Skipped
                   : st == "pending" ? StageStatus::Pending
                   : StageStatus::Failed;
        rec.exitCode = params.value("exitCode").toInt();
        rec.errorCode = params.value("errorCode").toString();
        rec.errorMessage = params.value("errorMessage").toString();
        // 保留启动时间（阶段记录是整体替换的）
        rec.startedAt = params.value("startedAt").toVariant().toLongLong();
        rec.finishedAt = params.value("finishedAt").toVariant().toLongLong();
        if (rec.finishedAt == 0) rec.finishedAt = QDateTime::currentMSecsSinceEpoch();
        for (const QJsonValue& o : params.value("outputs").toArray())
            rec.outputs << o.toString();
        m_store.setStage(rec);
        m_store.appendLog(QString("stage %1 %2 exit=%3 %4")
                              .arg(rec.id, st)
                              .arg(rec.exitCode)
                              .arg(rec.errorCode.isEmpty() ? QString() : rec.errorCode));
    } else if (method == "task.finished") {
        const QString status = params.value("status").toString();
        m_store.finish(status, params.value("summary").toString(),
                       params.value("elapsedMs").toVariant().toLongLong());
        // providers 版本快照：本结果由哪个模块版本组合产生（排错/复现依据）
        QJsonObject providers;
        for (const ModuleManifest& mm : m_registry.manifests())
            providers[mm.id] = QJsonObject{
                {"version", mm.version}, {"apiVersion", mm.apiVersion}};
        m_store.setProviders(providers);
        // 保留最近任务的目录，UI 在任务结束后仍可查 task.status / task.result
        m_lastTaskId = params.value("taskId").toString();
        m_lastTaskDir = m_activeTaskDir;
        m_activeTaskId.clear();
        m_activeTaskDir.clear();
        const bool isCli = m_lastTaskId.startsWith("cli_");
        if (isCli) {
            m_cliFinished = true;
            m_cliCode = (status == "done") ? 0 : 1;
            emit exitRequested(m_cliCode);
        }
    }

    // 转发给 UI（serve 模式）
    if (m_transport) m_transport->notify(method, params);
}

void App::handleRequest(const QJsonValue& reqId, const QString& method,
                        const QJsonObject& params)
{
    if (method == "ping") rpcPing(reqId);
    else if (method == "modules.list") rpcListModules(reqId);
    else if (method == "pipelines.list") rpcListPipelines(reqId);
    else if (method == "inputs.validate") rpcValidateInputs(reqId, params);
    else if (method == "task.run") rpcRunTask(reqId, params);
    else if (method == "module.run") rpcRunModule(reqId, params);
    else if (method == "task.cancel") rpcCancelTask(reqId, params);
    else if (method == "task.status") rpcTaskStatus(reqId, params);
    else if (method == "task.result") rpcTaskResult(reqId, params);
    else if (method == "shutdown") rpcShutdown(reqId);
    else m_transport->sendError(reqId, rpc::MethodNotFound, "unknown method: " + method);
}

void App::rpcPing(const QJsonValue& id)
{
    m_transport->sendResponse(id, QJsonObject{{"pong", true},
                                               {"app", "task_layer"},
                                               {"schemaVersion", 1}});
}

void App::rpcListModules(const QJsonValue& id)
{
    QJsonArray arr;
    for (const ModuleDef& d : m_registry.allModules()) {
        QJsonObject o;
        o["id"] = d.id;
        o["displayName"] = d.displayName;
        o["kind"] = d.kind == ModuleKind::DllWorker ? "dll" : "external";
        o["provider"] = d.provider;
        o["stage"] = d.stageId;
        const ModuleManifest* mm = nullptr;
        {
            auto mit = m_registry.manifests().find(d.provider);
            if (mit != m_registry.manifests().end()) mm = &mit.value();
        }
        if (mm) {
            o["providerVersion"] = mm->version;
            o["apiVersion"] = mm->apiVersion;
            o["breakingChanges"] = mm->breakingChanges;
        }
        const ManifestStage* st = mm ? mm->stage(d.stageId) : nullptr;
        if (st) o["allowedArgs"] = QJsonArray::fromStringList(st->allowedArgs);
        // available：当前部署下能否真的执行
        QString exe = d.command.isEmpty() ? QString() : ModuleRunner::locateCommand(d, m_resolver);
        o["available"] = !exe.isEmpty();
        o["resolvedPath"] = exe;
        arr.append(o);
    }
    m_transport->sendResponse(id, QJsonObject{{"modules", arr}});
}

void App::rpcListPipelines(const QJsonValue& id)
{
    QJsonArray arr;
    for (const PipelineDef& p : m_loader.allPipelines()) {
        QJsonObject o;
        o["id"] = p.id;
        o["displayName"] = p.displayName;
        o["description"] = p.description;
        o["maxParallel"] = p.maxParallel;
        QJsonArray stages;
        for (const PipelineStage& s : p.stages) {
            QJsonObject so;
            so["id"] = s.id;
            so["module"] = s.moduleId;
            so["name"] = s.name;
            so["optional"] = s.optional;
            QJsonArray deps;
            for (const QString& d : s.dependsOn) deps.append(d);
            so["dependsOn"] = deps;
            QJsonObject arts;
            for (auto ait = s.artifacts.constBegin(); ait != s.artifacts.constEnd(); ++ait)
                arts[ait.key()] = ait.value();
            if (!arts.isEmpty()) so["artifacts"] = arts;
            stages.append(so);
        }
        o["stages"] = stages;
        arr.append(o);
    }
    m_transport->sendResponse(id, QJsonObject{{"pipelines", arr}});
}

void App::rpcValidateInputs(const QJsonValue& id, const QJsonObject& params)
{
    const QString dataRoot = params.value("dataRoot").toString();
    if (dataRoot.isEmpty()) {
        m_transport->sendError(id, rpc::InvalidParams, "missing dataRoot");
        return;
    }
    m_transport->sendResponse(id, inspectInputs(dataRoot, params.value("pipeline").toString(),
                                                params.value("module").toString(),
                                                params.value("inputs").toObject()));
}

void App::rpcRunTask(const QJsonValue& id, const QJsonObject& params)
{
    const QString pipelineId = params.value("pipeline").toString();
    const PipelineDef* pipe = m_loader.pipeline(pipelineId);
    if (!pipe) {
        m_transport->sendError(id, rpc::InvalidParams,
                               "unknown pipeline: " + pipelineId);
        return;
    }
    startTaskWith(id, *pipe, params.value("dataRoot").toString(), params);
}

void App::rpcRunModule(const QJsonValue& id, const QJsonObject& params)
{
    const QString moduleId = params.value("moduleId").toString();
    const ModuleDef* def = m_registry.module(moduleId);
    if (!def) {
        m_transport->sendError(id, rpc::InvalidParams, "unknown module: " + moduleId);
        return;
    }

    // 单模块执行 = 只含该模块的"单阶段流水线"：
    // 进度通知、状态持久化、task.status / task.result / task.cancel 全部复用流水线那套。
    PipelineDef single;
    single.id = QStringLiteral("single:%1").arg(moduleId);
    single.displayName = def->displayName;
    single.description = QStringLiteral("单模块执行（module.run）");
    single.maxParallel = 1;
    PipelineStage st;
    st.id = def->id;
    st.moduleId = def->id;
    st.name = def->displayName;
    single.stages.append(st);

    startTaskWith(id, single, params.value("dataRoot").toString(), params);
}

bool App::startTaskWith(const QJsonValue& id, const PipelineDef& pipe,
                        const QString& dataRoot, const QJsonObject& params)
{
    if (dataRoot.isEmpty()) {
        m_transport->sendError(id, rpc::InvalidParams, "missing dataRoot");
        return false;
    }
    if (!m_activeTaskId.isEmpty()) {
        m_transport->sendError(id, rpc::ErrTaskRunning,
                               "another task is running: " + m_activeTaskId);
        return false;
    }
    const QString taskId = params.value("taskId").toString();
    const QString tid = taskId.isEmpty()
        ? QStringLiteral("t%1").arg(QDateTime::currentMSecsSinceEpoch()) : taskId;

    m_activeTaskId = tid;
    const QString root = QDir::cleanPath(QFileInfo(dataRoot).absoluteFilePath());
    m_activeDataRoot = root;
    m_activeTaskDir = QDir(root).filePath("Result/tasks/" + tid);
    m_activeInputs = params.value("inputs").toObject();
    m_resolver.setBase(root, m_moduleRoot, m_binRoot, m_activeTaskDir);
    m_resolver.setInputs(m_activeInputs);
    const QMap<QString, QMap<QString, QString>> ov = artifactOverridesOf(pipe);
    m_registry.rebindDataRoot(root, &ov);
    m_store.open(m_activeTaskDir, tid, pipe.id);
    m_store.setInputs(m_resolver.resolvedInputs());

    // overrides 白名单校验（manifest allowedArgs）：非法键直接拒绝，不静默透传
    const QJsonObject overrides = params.value("overrides").toObject();
    for (auto oit = overrides.constBegin(); oit != overrides.constEnd(); ++oit) {
        const PipelineStage* st = nullptr;
        for (const PipelineStage& s : pipe.stages)
            if (s.id == oit.key()) { st = &s; break; }
        if (!st) continue;   // 未知 stageId 由编排器按依赖处理
        const ModuleDef* def = m_registry.module(st->moduleId);
        if (!def) continue;
        for (auto kit = oit.value().toObject().constBegin();
             kit != oit.value().toObject().constEnd(); ++kit) {
            const QString key = kit.key().startsWith("--") ? kit.key() : "--" + kit.key();
            if (!def->allowedArgs.contains(key)) {
                m_transport->sendError(id, rpc::ErrInputInvalid,
                    QString("override '%1.%2' not in allowedArgs of module %3")
                        .arg(oit.key(), kit.key(), def->id));
                return false;
            }
        }
    }

    if (!m_orchestrator->startTask(tid, pipe, m_registry, m_resolver, root, overrides)) {
        m_activeTaskId.clear();
        m_activeTaskDir.clear();
        m_activeInputs = QJsonObject();
        m_transport->sendError(id, rpc::InternalError, "task failed to start");
        return false;
    }
    m_transport->sendResponse(id, QJsonObject{{"taskId", tid}, {"pipeline", pipe.id}});
    return true;
}

void App::rpcCancelTask(const QJsonValue& id, const QJsonObject& params)
{
    const QString taskId = params.value("taskId").toString();
    if (taskId != m_activeTaskId || !m_orchestrator->cancel(taskId)) {
        m_transport->sendError(id, rpc::ErrTaskNotFound, "task not running: " + taskId);
        return;
    }
    m_transport->sendResponse(id, QJsonObject{{"cancelled", true}});
}

QString App::taskDirOf(const QString& taskId) const
{
    if (!taskId.isEmpty() && taskId == m_activeTaskId && !m_activeTaskDir.isEmpty())
        return m_activeTaskDir;
    if (!taskId.isEmpty() && taskId == m_lastTaskId && !m_lastTaskDir.isEmpty())
        return m_lastTaskDir;
    return QString();
}

void App::rpcTaskStatus(const QJsonValue& id, const QJsonObject& params)
{
    const QString taskId = params.value("taskId").toString();
    QString dir = taskDirOf(taskId);
    if (dir.isEmpty()) {
        const QString dataRoot = params.value("dataRoot").toString();
        if (!dataRoot.isEmpty())
            dir = QDir(dataRoot).filePath("Result/tasks/" + taskId);
    }
    QJsonObject state;
    if (dir.isEmpty() || !ResultStore::readTaskState(dir, &state, nullptr)) {
        m_transport->sendError(id, rpc::ErrTaskNotFound, "no state for task: " + taskId);
        return;
    }
    m_transport->sendResponse(id, state);
}

void App::rpcTaskResult(const QJsonValue& id, const QJsonObject& params)
{
    const QString taskId = params.value("taskId").toString();
    QString dir = taskDirOf(taskId);
    if (dir.isEmpty()) {
        const QString dataRoot = params.value("dataRoot").toString();
        if (!dataRoot.isEmpty())
            dir = QDir(dataRoot).filePath("Result/tasks/" + taskId);
    }
    QJsonObject state;
    if (dir.isEmpty() || !ResultStore::readTaskState(dir, &state, nullptr)) {
        m_transport->sendError(id, rpc::ErrTaskNotFound, "no state for task: " + taskId);
        return;
    }
    // 汇总结果文件清单
    QJsonArray files;
    for (const QJsonValue& sv : state.value("stages").toArray()) {
        for (const QJsonValue& o : sv.toObject().value("outputs").toArray())
            files.append(o);
    }
    QJsonObject out = state;
    out["taskDir"] = dir;
    out["resultFiles"] = files;
    m_transport->sendResponse(id, out);
}

void App::rpcShutdown(const QJsonValue& id)
{
    m_transport->sendResponse(id, QJsonObject{{"bye", true}});
    m_transport->flush();
    const QString running = m_orchestrator->currentTaskId();
    if (!running.isEmpty()) m_orchestrator->cancel(running);
    emit exitRequested(0);
}

} // namespace task