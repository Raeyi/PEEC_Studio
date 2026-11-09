#include "config/ModuleRegistry.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

namespace task {

namespace {
QString toNative(const QString& posixPath, const QString& dataRoot)
{
    // manifest 的 POSIX 相对路径 → 系统绝对路径
    QString p = dataRoot + "/" + posixPath;
    p.replace('/', QDir::separator());
    return QDir::cleanPath(p);
}
} // namespace

bool ModuleRegistry::parseRouting(const QJsonObject& obj, QString* error)
{
    const int sv = obj.value("schemaVersion").toInt();
    if (sv != 2) {
        *error = QString("modules.json: schemaVersion must be 2 (got %1); "
                         "v1 格式已废弃，契约移入各包 module.manifest.json")
                     .arg(sv);
        return false;
    }
    m_routing.clear();
    const QJsonObject mods = obj.value("modules").toObject();
    for (auto it = mods.begin(); it != mods.end(); ++it) {
        const QJsonObject o = it.value().toObject();
        RoutingEntry r;
        r.displayName = o.value("displayName").toString(it.key());
        r.kind = o.value("kind").toString() == "dll" ? ModuleKind::DllWorker
                                                     : ModuleKind::External;
        r.provider = o.value("provider").toString();
        r.stageId = o.value("stage").toString();
        for (const QJsonValue& v : o.value("searchDirs").toArray())
            r.searchDirs << v.toString();
        r.workerHost = o.value("workerHost").toString();
        const QJsonObject envObj = o.value("env").toObject();
        for (auto eit = envObj.constBegin(); eit != envObj.constEnd(); ++eit)
            r.env.insert(eit.key(), eit.value().toString());
        if (r.provider.isEmpty() || r.stageId.isEmpty()) {
            *error = QString("module '%1': v2 routing requires 'provider' and 'stage'")
                         .arg(it.key());
            return false;
        }
        m_routing.insert(it.key(), r);
    }
    if (m_routing.isEmpty()) {
        *error = "modules.json: no modules";
        return false;
    }
    return true;
}

bool ModuleRegistry::loadRouting(const QString& jsonPath, QString* error)
{
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = QString("cannot open %1").arg(jsonPath);
        return false;
    }
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = QString("modules.json parse error: %1").arg(perr.errorString());
        return false;
    }
    return parseRouting(doc.object(), error);
}

bool ModuleRegistry::loadManifests(const QString& moduleRoot,
                                   const QStringList& extraPackageDirs,
                                   QStringList* missingProviders, QString* error)
{
    m_manifests.clear();
    m_packageDirs.clear();
    m_modules.clear();

    // 1) 发现 provider 包：优先 searchDirs 命中的目录，其次 <moduleRoot>/<provider>
    QMap<QString, QString> pkgOf;   // provider -> 包目录
    for (auto rit = m_routing.constBegin(); rit != m_routing.constEnd(); ++rit) {
        const RoutingEntry& r = rit.value();
        if (pkgOf.contains(r.provider)) continue;
        QString dir;
        for (const QString& sd : r.searchDirs) {
            // searchDirs 是路径变量形式（{MODULE_ROOT}/gds2geo），这里做简单替换
            QString cand = sd;
            cand.replace("{MODULE_ROOT}", moduleRoot);
            if (QFileInfo::exists(cand + "/module.manifest.json")) { dir = cand; break; }
        }
        if (dir.isEmpty()) {
            const QString guess = QDir(moduleRoot).filePath(r.provider);
            if (QFileInfo::exists(guess + "/module.manifest.json")) dir = guess;
        }
        for (const QString& extra : extraPackageDirs) {
            if (dir.isEmpty() && QFileInfo::exists(extra + "/module.manifest.json"))
                dir = extra;
        }
        if (!dir.isEmpty()) pkgOf.insert(r.provider, dir);
    }

    // 2) 加载 manifest；缺失的 provider 记为不可用（不阻断启动）
    QStringList missing;
    for (auto pit = pkgOf.constBegin(); pit != pkgOf.constEnd(); ++pit) {
        ModuleManifest m;
        QString err;
        if (ManifestLoader::loadFromDir(pit.value(), &m, &err)) {
            m_manifests.insert(m.id, m);
            m_packageDirs.insert(m.id, pit.value());
        } else {
            missing << QString("%1: %2").arg(pit.key(), err);
        }
    }
    for (const QString& provider : pkgOf.keys())
        if (!m_manifests.contains(provider))
            missing << QString("%1: package found but manifest load failed").arg(provider);
    // 路由了但完全没找到包的 provider
    QSet<QString> allProviders;
    for (const RoutingEntry& re : m_routing) allProviders.insert(re.provider);
    for (const QString& provider : allProviders)
        if (!pkgOf.contains(provider))
            missing << QString("%1: package directory not found").arg(provider);
    if (missingProviders) *missingProviders = missing;

    // 3) 合成 ModuleDef
    for (auto rit = m_routing.constBegin(); rit != m_routing.constEnd(); ++rit) {
        const QString moduleId = rit.key();
        const RoutingEntry& r = rit.value();
        ModuleDef def;
        def.id = moduleId;
        def.displayName = r.displayName;
        def.kind = r.kind;
        def.provider = r.provider;
        def.stageId = r.stageId;
        def.searchDirs = r.searchDirs;
        def.workerHost = r.workerHost;
        def.env = r.env;

        const ModuleManifest* mm = m_manifests.contains(r.provider) ? &m_manifests[r.provider] : nullptr;
        if (!mm) {
            def.command = QString();      // 不可用：无 manifest
        } else {
            const ManifestStage* st = mm->stage(r.stageId);
            if (!st) {
                missing << QString("%1: stage '%2' not in manifest of %3")
                            .arg(moduleId, r.stageId, r.provider);
                if (missingProviders) *missingProviders = missing;
                def.command = QString();
            } else {
                def.command = st->executable;
                def.allowedArgs = st->allowedArgs;
                def.errorType = st->errorType;
                def.traceLog = st->traceLog;
                def.timeoutSec = 0;
                // argsTemplate 保留模板形态，rebindDataRoot 时展开
                def.argsTemplate = st->argsTemplate;
                def.args = st->argsTemplate;
            }
        }

        // 4) 契约派生：consumed → requiredInputs（user/* 与跨模块对外 ref）
        //    produced → outputChecks + publishes
        if (mm) {
            // 内部件集合：各 stage 的 consumesInternal / producesInternal。
            // required inputs = manifest 级 consumed 中不属于内部件的 ref。
            // （旧版按 owner==id 过滤，会漏掉 owner 恰为模块 id 但由 UI/上游维护的件，
            //   如 thermal/source.def：owner=thermal 但 maintainedBy=ui，必须进浅检）
            QSet<QString> internal;
            for (const ManifestStage& ms : mm->stages) {
                for (const QString& x : ms.consumesInternal) internal.insert(x);
                for (const QString& x : ms.producesInternal) internal.insert(x);
            }
            for (const ManifestArtifact& a : mm->consumed) {
                if (internal.contains(a.ref)) continue;  // 内部自产自用不进浅检
                // 全部对外 consumed 挂到本 stage（保守策略：入口阶段校验由 App 按
                // pipeline 入口过滤；stage 内深检由模块自身完成）
                def.contract.requiredInputs << a.path;   // 暂存 POSIX 相对路径
                def.consumedRefs.insert(a.ref, a.path);
            }
            // produced → 本 stage 的 outputChecks + publishes：
            // 按 DATA_LAYOUT D2（Result/<providerId>/<stageId>/ 与 stage 一一对应）
            // 用前缀归属，其他 stage 的产物不进本 stage 浅检（修复"全 stage 全量查
            // 全 manifest 产物"导致的 QS-TL-OUTPUT-MISSING 误报）
            const QString stagePrefix = "Result/" + mm->id + "/" + r.stageId + "/";
            for (const ManifestArtifact& a : mm->produced) {
                if (!a.path.startsWith(stagePrefix)) continue;
                def.contract.outputChecks << a.path;
                if (!a.publishTo.isEmpty())
                    def.contract.publishes.append({a.path, a.publishTo});
            }
            def.basePublishes = def.contract.publishes;   // manifest 基底；rebind 先重置再叠加宿主 publishes
        }
        m_modules.insert(moduleId, def);
    }

    if (!missing.isEmpty() && error && m_modules.isEmpty())
        *error = missing.first();
    return !m_modules.isEmpty();
}

void ModuleRegistry::rebindDataRoot(const QString& dataRoot,
                                    const QMap<QString, QMap<QString, QString>>* artifactOverrides,
                                    const QMap<QString, QMap<QString, QString>>* publishOverrides)
{
    m_dataRoot = dataRoot;
    for (auto mit = m_modules.begin(); mit != m_modules.end(); ++mit) {
        ModuleDef& def = mit.value();
        if (def.command.isEmpty()) continue;

        const QMap<QString, QString> ov = artifactOverrides
            ? artifactOverrides->value(def.id)
            : QMap<QString, QString>();
        const QString runtimeDir = toNative("Result/logs/" + def.provider, dataRoot);
        const ModuleManifest* mm = m_manifests.contains(def.provider)
            ? &m_manifests[def.provider] : nullptr;

        // args 模板展开（每次从原始模板展开）：
        //   {dataRoot} / {runtimeDir} / {moduleDir} / {artifact:<ref>}
        // {artifact:<ref>} 路由优先级：
        //   ① 流水线 stage 的 artifacts 覆盖（跨模块工件路由，如热网格指向
        //      Result/gds2geo/thermal/ 而不是 Model/，保持 Model/ 单主网格不变式 I1）
        //   ② 本模块 manifest consumed 声明（UI/用户维护件的规范位置）
        //   ③ 本模块 produced ④ 其他模块 produced（跨模块 ref，如 gds2geo/mesh.thermal）
        QStringList out;
        for (const QString& a : def.argsTemplate) {
            QString v = a;
            const int ab = v.indexOf("{artifact:");
            if (ab >= 0 && v.endsWith("}")) {
                const QString ref = v.mid(ab + 10, v.size() - ab - 11);
                QString raw;
                if (ov.contains(ref)) {
                    raw = ov.value(ref);
                } else if (def.consumedRefs.contains(ref)) {
                    raw = def.consumedRefs.value(ref);
                } else if (mm && mm->artifact(ref)) {
                    raw = mm->artifact(ref)->path;
                }
                if (raw.isEmpty()) {
                    for (const ModuleManifest& m2 : m_manifests)
                        if (const ManifestArtifact* pa = m2.artifact(ref)) {
                            raw = pa->path;
                            break;
                        }
                }
                if (!raw.isEmpty()) {
                    v = raw;
                } else {
                    // 保留字面量让模块以清晰报错失败，任务层不静默改写
                    qWarning("[config] module %s: {artifact:%s} unresolved",
                             qPrintable(def.id), qPrintable(ref));
                }
            }
            v.replace("{dataRoot}", dataRoot);
            v.replace("{runtimeDir}", runtimeDir);
            v.replace("{moduleDir}", m_packageDirs.value(def.provider));
            v.replace('/', QDir::separator());
            out << v;
        }
        def.args = out;

        // requiredInputs：从 consumedRefs 重建并应用路由覆盖，
        // 保证执行前浅检的路径与 args 实际传给模块的路径一致
        QStringList req, chk;
        for (auto it = def.consumedRefs.constBegin(); it != def.consumedRefs.constEnd(); ++it)
            req << toNative(ov.contains(it.key()) ? ov.value(it.key()) : it.value(), dataRoot);
        for (const QString& p : def.contract.outputChecks) {
            if (QFileInfo(p).isAbsolute()) chk << p;
            else chk << toNative(p, dataRoot);
        }
        def.contract.requiredInputs = req;
        def.contract.outputChecks = chk;
        // 宿主侧发布声明（流水线 stage publishes）：重置为 manifest 基底后叠加
        // （ref -> Model/|Setting/ 正式件落点）；ref 解析于 manifest produced，
        // 与 manifest publishTo 保持同形（POSIX 相对路径），下方统一展开为绝对路径。
        def.contract.publishes = def.basePublishes;
        if (publishOverrides) {
            const QMap<QString, QString> pv = publishOverrides->value(def.id);
            for (auto pjit = pv.constBegin(); pjit != pv.constEnd(); ++pjit) {
                const QString dest = pjit.value();
                if (!dest.startsWith("Model/") && !dest.startsWith("Setting/")) {
                    qWarning("[config] module %s: publishes dest '%s' rejected (Model/|Setting/ only)",
                             qPrintable(def.id), qPrintable(dest));
                    continue;
                }
                QString raw;
                if (mm && mm->artifact(pjit.key()))
                    raw = mm->artifact(pjit.key())->path;
                if (raw.isEmpty()) {
                    for (const ModuleManifest& m2 : m_manifests)
                        if (const ManifestArtifact* pa = m2.artifact(pjit.key())) {
                            raw = pa->path;
                            break;
                        }
                }
                if (raw.isEmpty()) {
                    qWarning("[config] module %s: publishes ref '%s' not in manifest produced, skipped",
                             qPrintable(def.id), qPrintable(pjit.key()));
                    continue;
                }
                def.contract.publishes.append({raw, dest});
            }
        }
        for (ModuleContract::PublishRule& pr : def.contract.publishes) {
            if (!QFileInfo(pr.src).isAbsolute()) pr.src = toNative(pr.src, dataRoot);
            if (!QFileInfo(pr.dst).isAbsolute()) pr.dst = toNative(pr.dst, dataRoot);
        }
    }
}

} // namespace task