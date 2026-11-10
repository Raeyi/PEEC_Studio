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
// effectiveStages（P1 执行选择）：有效阶段集合（选中 + 上游闭包）；null = 全链（保持现行为）。
// 子集模式只取有效集合内 stage 的路由，顺带消除"同模块两个 stage 同 ref 键 last-wins 覆盖"隐患。
QMap<QString, QMap<QString, QString>> artifactOverridesOf(const PipelineDef& pipe,
                                                          const QSet<QString>* effectiveStages = nullptr)
{
    QMap<QString, QMap<QString, QString>> ov;
    for (const PipelineStage& s : pipe.stages) {
        if (effectiveStages && !effectiveStages->contains(s.id)) continue;
        for (auto it = s.artifacts.constBegin(); it != s.artifacts.constEnd(); ++it)
            ov[s.moduleId][it.key()] = it.value();
    }
    return ov;
}

// 流水线 stage 的 publishes 声明 → 按 moduleId 归组（宿主侧发布声明注入 rebind，
// 覆盖 manifest 未带 publishTo 的模块：平台声明 Model/|Setting/ 正式件落位）
QMap<QString, QMap<QString, QString>> publishOverridesOf(const PipelineDef& pipe,
                                                         const QSet<QString>* effectiveStages = nullptr)
{
    QMap<QString, QMap<QString, QString>> ov;
    for (const PipelineStage& s : pipe.stages) {
        if (effectiveStages && !effectiveStages->contains(s.id)) continue;
        for (auto it = s.publishes.constBegin(); it != s.publishes.constEnd(); ++it)
            ov[s.moduleId][it.key()] = it.value();
    }
    return ov;
}

// P1 resume 归一：读上次任务 state.json → pipeline 一致性校验 + Done 阶段产出在盘核验
// → stagesToRun（未完成重进）+ assumed（假定完成）。失败写 *error 并返回 false。
bool normalizeResumeSelection(const PipelineDef& pipe, const QString& root,
                              const QString& resumeTaskId, QStringList* stagesToRun,
                              QSet<QString>* assumed, QString* error)
{
    const QString priorDir = QDir(root).filePath("Result/tasks/" + resumeTaskId);
    QJsonObject prior;
    if (!ResultStore::readTaskState(priorDir, &prior, error)) return false;
    const QString priorPipe = prior.value("pipelineId").toString();
    if (priorPipe != pipe.id) {
        *error = "resume: pipeline mismatch (state is for '" + priorPipe + "')";
        return false;
    }
    QSet<QString> stageIds;
    for (const PipelineStage& s : pipe.stages) stageIds.insert(s.id);
    QSet<QString> verified;
    for (const QJsonValue& sv : prior.value("stages").toArray()) {
        const QJsonObject so = sv.toObject();
        const QString sid = so.value("id").toString();
        if (so.value("status").toString() != "done") continue;
        if (so.value("assumed").toBool()) continue;   // 上次任务的"假定完成"不构成实际执行：resume 时必须重跑
        if (!stageIds.contains(sid)) {
            *error = QStringLiteral("QS-TL-SEL-UNKNOWNSTAGE: prior stage '%1' not in current pipeline '%2'")
                        .arg(sid, pipe.id);
            return false;
        }
        QStringList missing;
        for (const QJsonValue& o : so.value("outputs").toArray())
            if (!QFileInfo::exists(o.toString())) missing << o.toString();
        if (!missing.isEmpty()) {
            *error = QStringLiteral("QS-TL-SEL-DEP-MISSING: resume: outputs of stage '%1' missing on disk: %2")
                        .arg(sid, missing.join("; "));
            return false;
        }
        verified.insert(sid);
    }
    stagesToRun->clear();
    for (const PipelineStage& s : pipe.stages)
        if (!verified.contains(s.id)) stagesToRun->append(s.id);
    for (const QString& sid : verified) assumed->insert(sid);
    return true;
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
    if (!m_loader.loadFromFile(m_configDir + "/pipelines.json",
                                    m_configDir + "/pipelines.local.json", &err)) {
        if (detail) *detail = err;
        return false;
    }
    // P2：阶段 params 白名单（manifest allowedArgs，--key 形式），加载期 fail-fast；
    // 模块注册存在性仍留启动期（QS-TL-CFG-UNKNOWNMODULE，现状）
    for (const PipelineDef& pd : m_loader.allPipelines()) {
        for (const PipelineStage& st : pd.stages) {
            const ModuleDef* md = m_registry.module(st.moduleId);
            if (!md) continue;
            for (auto pit = st.params.constBegin(); pit != st.params.constEnd(); ++pit) {
                const QString key = pit.key().startsWith("--") ? pit.key()
                                                               : "--" + pit.key();
                if (!md->allowedArgs.contains(key)) {
                    if (detail) *detail = QString(
                        "pipeline '%1': stage '%2': param '%3' not in allowedArgs of module '%4'")
                        .arg(pd.id, st.id, pit.key(), st.moduleId);
                    return false;
                }
            }
        }
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
    const QMap<QString, QMap<QString, QString>> pov = publishOverridesOf(*pipe);
    QString rebindErr;
    if (!m_registry.rebindDataRoot(root, &ov, &pov, &rebindErr)) {
        QTextStream(stdout) << "config error: " << rebindErr << "\n";
        return 2;
    }
    // args/契约路径已展开（含 stage artifacts 路由 + 宿主 publishes），包含性守卫已通过
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
                               const QString& moduleId, const QJsonObject& inputs,
                               const QJsonObject& selection)
{
    const QString root = QDir::cleanPath(QFileInfo(dataRoot).absoluteFilePath());
    PathResolver r;
    r.setBase(root, m_moduleRoot, m_binRoot,
              QDir(root).filePath("Result/tasks/_validate"));
    r.setInputs(inputs);
    // P1 执行选择解析（与 task.run 同语义；入口集合与 rebind 共用）
    QSet<QString> validateSelSet;
    {
        const QString selMode = selection.value("mode").toString();
        if (selMode == "stages") {
            for (const QJsonValue& v : selection.value("stages").toArray())
                validateSelSet.insert(v.toString());
        } else if (selMode == "resume") {
            const PipelineDef* pp = m_loader.pipeline(pipelineId);
            if (pp) {
                QStringList toRun;
                QSet<QString> assumed;
                QString rerr;
                if (!selection.value("resumeTaskId").toString().isEmpty() &&
                    normalizeResumeSelection(*pp, root, selection.value("resumeTaskId").toString(),
                                              &toRun, &assumed, &rerr))
                    for (const QString& sid : toRun) validateSelSet.insert(sid);
            }
        }
    }
    // 校验用临时 rebind：把契约路径展开到本 dataRoot（不改活动任务状态）；
    // P1 带 selection 时按有效阶段集合（选中 + 上游闭包）计算路由，与 task.run 一致
    QMap<QString, QMap<QString, QString>> vov, vpov;
    if (!pipelineId.isEmpty() && m_loader.pipeline(pipelineId)) {
        QSet<QString> effective;
        if (!validateSelSet.isEmpty()) {
            QMap<QString, QStringList> depMap;
            for (const PipelineStage& s2 : m_loader.pipeline(pipelineId)->stages)
                depMap.insert(s2.id, s2.dependsOn);
            QSet<QString> closure;
            QStringList stack;
            for (const QString& sid : validateSelSet) stack.append(sid);
            while (!stack.isEmpty()) {
                const QString cur = stack.takeLast();
                if (closure.contains(cur)) continue;
                closure.insert(cur);
                for (const QString& d : depMap.value(cur))
                    if (!closure.contains(d)) stack.append(d);
            }
            effective = closure;
        }
        vov = artifactOverridesOf(*m_loader.pipeline(pipelineId),
                                  effective.isEmpty() ? nullptr : &effective);
        vpov = publishOverridesOf(*m_loader.pipeline(pipelineId),
                                  effective.isEmpty() ? nullptr : &effective);
    }
    QString rebindErr;
    if (!m_registry.rebindDataRoot(root, vov.isEmpty() ? nullptr : &vov,
                                   vpov.isEmpty() ? nullptr : &vpov,
                                   &rebindErr)) {
        // 包含性守卫触发（QS-TL-PATH-ESCAPE）：直接返回非 valid 体检报告，UI 原样展示 issue
        QJsonArray issueArr;
        issueArr.append(QJsonObject{{"path", rebindErr},
                                    {"reason", QStringLiteral("QS-TL-PATH-ESCAPE")},
                                    {"detail", QStringLiteral("resolved contract path outside current dataRoot")}});
        return QJsonObject{{"valid", false},
                           {"dataRoot", root},
                           {"checked", QJsonArray()},
                           {"issues", issueArr},
                           {"warnings", QJsonArray()}};
    }
    // 只校验"入口阶段"的输入契约：这是"这条流水线能否启动"的判据。
    // P1 带 selection 时：入口 = 选中且"依赖不都在选中集"的阶段（resume 已归一）；
    // 假定完成阶段的产出在 task.run 期由 resume 预检保证（validate 期不重复在盘核验）。
    QStringList entryModuleIds;
    QStringList entryRequires;   // P1：入口阶段的 requires（流水线声明的上游产出，dataRoot 相对）
    if (!moduleId.isEmpty()) {
        entryModuleIds << moduleId;
    } else if (!pipelineId.isEmpty()) {
        const PipelineDef* p = m_loader.pipeline(pipelineId);
        if (p) {
            if (!validateSelSet.isEmpty()) {
                for (const PipelineStage& s : p->stages) {
                    if (!validateSelSet.contains(s.id)) continue;
                    // 执行入口：无依赖，或至少一个依赖未选中（其输入来自集合外，
                    // 必须按契约/requires 校验在盘存在）
                    bool entry = s.dependsOn.isEmpty();
                    for (const QString& d : s.dependsOn)
                        if (!validateSelSet.contains(d)) { entry = true; break; }
                    if (!entry) continue;
                    entryModuleIds << s.moduleId;
                    for (const QString& rq : s.optionalInputs) entryRequires << rq;
                }
            } else {
                for (const PipelineStage& s : p->stages) {
                    if (s.dependsOn.isEmpty()) {
                        entryModuleIds << s.moduleId;
                        for (const QString& rq : s.optionalInputs) entryRequires << rq;
                    }
                }
            }
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

    // P1：入口阶段的 requires（子集入口守卫；全链模式入口阶段无 requires，行为不变）
    for (const QString& rq : entryRequires) {
        const QString ap = QDir(root).filePath(rq);
        if (seen.contains(ap)) continue;
        seen.insert(ap);
        required << ap;
    }

    bool ok = InputValidator::validate(required, {}, {}, &issues, &warnings);

    // 端口绑定预检：EM 链入口检查 Model/source_em.json（DATA_LAYOUT v1.2 D3/I3；
    // compact_em v1.2 起只实读新名；旧名 port_em.json 保留仅供绑定诊断，模块不读取）
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
    } else if (method == "task.stage.awaiting") {
        // P1.5 用户闸门：阶段进入/保持 WaitingInput（非终态；随后 provideInput 唤醒或取消收尾）
        StageRecord rec;
        rec.id = params.value("stageId").toString();
        rec.moduleId = params.value("module").toString();
        rec.name = params.value("name").toString();
        rec.status = StageStatus::WaitingInput;
        rec.startedAt = 0;
        m_store.setStage(rec);
        const QByteArray missJson = QJsonDocument(
                                        params.value("missing").toObject())
                                        .toJson(QJsonDocument::Compact);
        m_store.appendLog(QString("stage %1 waiting for input: %2")
                              .arg(rec.id, QString::fromUtf8(missJson)));
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
        rec.assumed = params.value("assumed").toBool();   // P1 假定完成
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
    else if (method == "task.provideInput") rpcProvideInput(reqId, params);
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
        o["source"] = p.source;   // P3：builtin / local（pipelines.local.json 覆盖）
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
            if (!s.ref.isEmpty()) so["ref"] = s.ref;   // P2 物化溯源（v2 形态）
            QJsonArray deps;
            for (const QString& d : s.dependsOn) deps.append(d);
            so["dependsOn"] = deps;
            if (!s.optionalInputs.isEmpty()) {
                QJsonArray req;
                for (const QString& r : s.optionalInputs) req.append(r);
                so["requires"] = req;
            }
            if (!s.params.isEmpty()) so["params"] = s.params;   // P2 物化（库+本地合并）
            if (!s.userGate.isEmpty()) so["userGate"] = s.userGate;
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
                                                params.value("inputs").toObject(),
                                                params.value("selection").toObject()));
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
    // P3（H2 路由修复）：在所有 pipeline（含 local）的物化阶段条目中找该模块的接线——
    //   恰好 1 个 -> 采用其 artifacts/requires/params/userGate 库级默认接线；
    //   0 个     -> 现行为（无路由，manifest 默认位置）；
    //   >=2 个   -> 要求 stageRef = "<pipelineId>/<stageId>" 消歧（候选列表可直接复制）。
    struct Candidate { QString pipelineId; const PipelineStage* st; };
    QVector<Candidate> cands;
    for (const PipelineDef& pd : m_loader.allPipelines())
        for (const PipelineStage& ps : pd.stages)
            if (ps.moduleId == moduleId)
                cands.append({pd.id, &ps});
    const QString stageRef = params.value("stageRef").toString();
    const PipelineStage* routed = nullptr;
    if (cands.size() == 1) {
        routed = cands.first().st;
    } else if (cands.size() >= 2) {
        for (const Candidate& c : cands) {
            if (c.pipelineId + "/" + c.st->id == stageRef) {
                routed = c.st;
                break;
            }
        }
        if (!routed) {
            QStringList listed;
            for (const Candidate& c : cands)
                listed << c.pipelineId + "/" + c.st->id;
            m_transport->sendError(id, rpc::InvalidParams,
                QString("module '%1' is wired into %2 pipeline stages; pass stageRef as one of: %3")
                    .arg(moduleId, QString::number(cands.size()), listed.join(", ")));
            return;
        }
    }
    PipelineDef single;
    single.id = QStringLiteral("single:%1").arg(moduleId);
    single.displayName = def->displayName;
    single.description = QStringLiteral("单模块执行（module.run）");
    single.maxParallel = 1;
    PipelineStage st;
    st.id = def->id;
    st.moduleId = def->id;
    st.name = def->displayName;
    if (routed) {   // H2：唯一/消歧后的接线默认（物化后的库级 + 本地覆盖结果）
        st.artifacts = routed->artifacts;
        st.optionalInputs = routed->optionalInputs;
        st.params = routed->params;
        st.userGate = routed->userGate;
    }
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
    const QString root = QDir::cleanPath(QFileInfo(dataRoot).absoluteFilePath());

    // P1 执行选择（selection）：解析 + 校验 + 归一。失败即拒绝、不建任务、不动活动状态。
    //   full（缺省）= 全链；stages = DAG 子集；resume = 基于上次任务 state.json 续跑（归一为 stages）。
    Selection sel;
    QJsonObject selRecord;   // 调用方原样 selection（state.json 留痕用，resume 保留原始形态）
    const QJsonObject selObj = params.value("selection").toObject();
    if (!selObj.isEmpty()) {
        sel.mode = selObj.value("mode").toString("full");
        for (const QJsonValue& v : selObj.value("stages").toArray()) sel.stages << v.toString();
        sel.resumeTaskId = selObj.value("resumeTaskId").toString();
        selRecord = selObj;
        if (sel.mode == "stages") {
            if (sel.stages.isEmpty()) {
                m_transport->sendError(id, rpc::InvalidParams,
                                       "selection.stages required for mode=stages");
                return false;
            }
            QSet<QString> stageIds;
            for (const PipelineStage& s : pipe.stages) stageIds.insert(s.id);
            for (const QString& sid : sel.stages)
                if (!stageIds.contains(sid)) {
                    m_transport->sendError(id, rpc::InvalidParams,
                        QStringLiteral("QS-TL-SEL-UNKNOWNSTAGE: stage '%1' not in pipeline '%2'")
                            .arg(sid, pipe.id));
                    return false;
                }
        } else if (sel.mode == "resume") {
            if (sel.resumeTaskId.isEmpty()) {
                m_transport->sendError(id, rpc::InvalidParams,
                                       "selection.resumeTaskId required for mode=resume");
                return false;
            }
            QSet<QString> assumed;
            QString rerr;
            if (!normalizeResumeSelection(pipe, root, sel.resumeTaskId,
                                           &sel.stages, &assumed, &rerr)) {
                m_transport->sendError(id, rpc::InvalidParams, rerr);
                return false;
            }
            sel.mode = "stages";   // 编排器只看归一后的 stages；原始形态在 selRecord 留痕
        } else if (sel.mode != "full") {
            m_transport->sendError(id, rpc::InvalidParams,
                "unknown selection.mode: " + sel.mode + " (full|stages|resume)");
            return false;
        }
    }

    const QString taskId = params.value("taskId").toString();
    const QString tid = taskId.isEmpty()
        ? QStringLiteral("t%1").arg(QDateTime::currentMSecsSinceEpoch()) : taskId;

    m_activeTaskId = tid;
    m_activeDataRoot = root;
    m_activeTaskDir = QDir(root).filePath("Result/tasks/" + tid);
    m_activeInputs = params.value("inputs").toObject();
    m_resolver.setBase(root, m_moduleRoot, m_binRoot, m_activeTaskDir);
    m_resolver.setInputs(m_activeInputs);
    // P1：工件路由按有效阶段集合计算（选中 + 上游闭包；全链保持现行为）
    QSet<QString> effSet;
    if (!sel.isFull()) {
        QMap<QString, QStringList> depMap;
        for (const PipelineStage& s : pipe.stages) depMap.insert(s.id, s.dependsOn);
        QStringList stack = sel.stages;
        while (!stack.isEmpty()) {
            const QString cur = stack.takeLast();
            if (effSet.contains(cur)) continue;
            effSet.insert(cur);
            for (const QString& d : depMap.value(cur))
                if (!effSet.contains(d)) stack.append(d);
        }
    }
    const QMap<QString, QMap<QString, QString>> ov =
        artifactOverridesOf(pipe, sel.isFull() ? nullptr : &effSet);
    const QMap<QString, QMap<QString, QString>> pov =
        publishOverridesOf(pipe, sel.isFull() ? nullptr : &effSet);
    QString rebindErr;
    if (!m_registry.rebindDataRoot(root, &ov, &pov, &rebindErr)) {
        m_activeTaskId.clear();
        m_activeDataRoot.clear();
        m_activeTaskDir.clear();
        m_activeInputs = QJsonObject();
        m_transport->sendError(id, rpc::ErrConfig, rebindErr);
        return false;
    }
    // P1 子集入口依赖预检：选中阶段若依赖未选中上游，其产出契约必须已在盘上满足
    //（复用 optional 阶段的 requiredInputs + optionalInputs 存在性检查）→ 缺失拒绝启动。
    if (!sel.isFull()) {
        QSet<QString> selectedSet;
        for (const QString& sid : sel.stages) selectedSet.insert(sid);
        QStringList problems;
        for (const PipelineStage& s : pipe.stages) {
            if (!selectedSet.contains(s.id)) continue;
            bool hasUnselectedDep = false;
            for (const QString& d : s.dependsOn)
                if (!selectedSet.contains(d)) { hasUnselectedDep = true; break; }
            if (!hasUnselectedDep) continue;
            const ModuleDef* def = m_registry.module(s.moduleId);
            if (!def) continue;   // 未注册模块由编排器启动期报错（现状行为）
            QStringList missing;
            const QStringList req = m_resolver.resolveList(def->contract.requiredInputs);
            for (const QString& r : req)
                if (!QFileInfo::exists(r)) missing << r;
            for (const QString& p : s.optionalInputs) {
                QString ap = root + "/" + p;
                ap.replace('/', QDir::separator());
                if (!QFileInfo::exists(QDir::cleanPath(ap))) missing << QDir::cleanPath(ap);
            }
            if (!missing.isEmpty())
                problems << QStringLiteral("stage '%1': %2").arg(s.id, missing.join("; "));
        }
        if (!problems.isEmpty()) {
            m_activeTaskId.clear();
            m_activeDataRoot.clear();
            m_activeTaskDir.clear();
            m_activeInputs = QJsonObject();
            m_transport->sendError(id, rpc::InvalidParams,
                QStringLiteral("QS-TL-SEL-DEP-MISSING: unselected upstream outputs not satisfied: %1")
                    .arg(problems.join(" | ")));
            return false;
        }
    }


    m_store.open(m_activeTaskDir, tid, pipe.id);
    m_store.setInputs(m_resolver.resolvedInputs());
    if (!selRecord.isEmpty()) m_store.setSelection(selRecord);

    // overrides 白名单校验（manifest allowedArgs）：非法键直接拒绝，不静默透传
    //（与 P1.5 gate 输入 task.provideInput 共用 checkAllowedArgs 同一校验路径）
    const QJsonObject overrides = params.value("overrides").toObject();
    for (auto oit = overrides.constBegin(); oit != overrides.constEnd(); ++oit) {
        const PipelineStage* st = nullptr;
        for (const PipelineStage& s : pipe.stages)
            if (s.id == oit.key()) { st = &s; break; }
        if (!st) continue;   // 未知 stageId 由编排器按依赖处理
        const QJsonObject vals = oit.value().toObject();
        const QString bad = checkAllowedArgs(*st, vals, QStringLiteral("override '%1.").arg(oit.key()));
        if (!bad.isEmpty()) {
            m_transport->sendError(id, rpc::ErrInputInvalid, bad);
            return false;
        }
    }

    if (!m_orchestrator->startTask(tid, pipe, m_registry, m_resolver, root, overrides, sel)) {
        m_activeTaskId.clear();
        m_activeTaskDir.clear();
        m_activeInputs = QJsonObject();
        m_transport->sendError(id, rpc::InternalError, "task failed to start");
        return false;
    }
    if (m_orchestrator->isBusy())
        m_store.setStatus("running");   // state.json 顶层状态：运行中（UI_API §4.9 语义）；
    // 全假定任务已在 startTask 内同步收尾（finish() 写终态），不得覆写
    QJsonObject resp{{"taskId", tid}, {"pipeline", pipe.id}};
    if (!selRecord.isEmpty()) resp["selection"] = selRecord;
    m_transport->sendResponse(id, resp);
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

QString App::checkAllowedArgs(const PipelineStage& st, const QJsonObject& vals,
                              const QString& displayPrefix) const
{
    const ModuleDef* def = m_registry.module(st.moduleId);
    if (!def) return QString();
    for (auto it = vals.constBegin(); it != vals.constEnd(); ++it) {
        const QString key = it.key().startsWith("--") ? it.key() : "--" + it.key();
        if (!def->allowedArgs.contains(key))
            return QString("%1%2' not in allowedArgs of module %3")
                .arg(displayPrefix, it.key(), def->id);
    }
    return QString();
}

void App::rpcProvideInput(const QJsonValue& id, const QJsonObject& params)
{
    const QString taskId = params.value("taskId").toString();
    const QString stageId = params.value("stageId").toString();
    if (taskId.isEmpty() || stageId.isEmpty()) {
        m_transport->sendError(id, rpc::InvalidParams, "missing taskId/stageId");
        return;
    }
    if (taskId != m_activeTaskId) {
        m_transport->sendError(id, rpc::ErrTaskNotFound, "task not running: " + taskId);
        return;
    }
    const PipelineStage* st = m_orchestrator->activeStage(stageId);
    if (!st) {
        m_transport->sendError(id, rpc::InvalidParams,
            QStringLiteral("QS-TL-INPUT-NOTWAITING: stage '%1' is not in an active task").arg(stageId));
        return;
    }
    const QJsonObject values = params.value("values").toObject();
    // 白名单：与 overrides 同一校验路径（manifest allowedArgs），非法键整体拒绝
    const QString bad = checkAllowedArgs(*st, values, QStringLiteral("input '"));
    if (!bad.isEmpty()) {
        m_transport->sendError(id, rpc::ErrInputInvalid, bad);
        return;
    }
    QString err;
    if (!m_orchestrator->provideInput(taskId, stageId, values, &err)) {
        if (err.startsWith("stage not waiting")) {
            m_transport->sendError(id, rpc::InvalidParams,
                QStringLiteral("QS-TL-INPUT-NOTWAITING: stage '%1' is not waiting for input").arg(stageId));
            return;
        }
        m_transport->sendError(id, rpc::InvalidParams, err);
        return;
    }
    // gate 输入留痕落盘（state.json providedInputs，审计）
    m_store.setProvidedInputs(m_orchestrator->providedInputs());
    m_transport->sendResponse(id, QJsonObject{
        {"taskId", taskId}, {"stageId", stageId}, {"accepted", true}});
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