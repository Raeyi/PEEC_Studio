#include "config/ModuleRegistry.h"

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
                def.args = st->argsTemplate;
            }
        }

        // 4) 契约派生：consumed → requiredInputs（user/* 与跨模块对外 ref）
        //    produced → outputChecks + publishes
        if (mm) {
            for (const ManifestArtifact& a : mm->consumed) {
                const int slash = a.ref.indexOf('/');
                const QString owner = slash > 0 ? a.ref.left(slash) : QString();
                if (owner == mm->id) continue;          // 内部自产自用不进浅检
                // 只把本 stage 相关的当 required：无法精确区分时取全部对外 consumed
                // （保守策略：入口阶段校验由 App 按 pipeline 入口过滤）
                def.contract.requiredInputs << a.path;   // 暂存 POSIX 相对路径
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
        }
        m_modules.insert(moduleId, def);
    }

    if (!missing.isEmpty() && error && m_modules.isEmpty())
        *error = missing.first();
    return !m_modules.isEmpty();
}

void ModuleRegistry::rebindDataRoot(const QString& dataRoot)
{
    m_dataRoot = dataRoot;
    for (auto mit = m_modules.begin(); mit != m_modules.end(); ++mit) {
        ModuleDef& def = mit.value();
        if (def.command.isEmpty()) continue;

        // args 模板展开：{dataRoot} / {runtimeDir}；{artifact:<ref>} 暂不支持跨任务注入
        // （当前流水线内上游产物位置固定，直接由模板字面量表达）
        const QString runtimeDir = toNative(
            "Result/logs/" + def.provider, dataRoot);
        QStringList out;
        for (const QString& a : def.args) {
            QString v = a;
            v.replace("{dataRoot}", dataRoot);
            v.replace("{runtimeDir}", runtimeDir);
            v.replace("{moduleDir}", m_packageDirs.value(def.provider));
            v.replace('/', QDir::separator());
            out << v;
        }
        def.args = out;

        QStringList req, chk;
        for (const QString& p : def.contract.requiredInputs) {
            // Only convert relative (POSIX) paths; skip already-absolute ones (idempotent rebind)
            if (QFileInfo(p).isAbsolute()) req << p;
            else req << toNative(p, dataRoot);
        }
        for (const QString& p : def.contract.outputChecks) {
            if (QFileInfo(p).isAbsolute()) chk << p;
            else chk << toNative(p, dataRoot);
        }
        def.contract.requiredInputs = req;
        def.contract.outputChecks = chk;
        for (ModuleContract::PublishRule& pr : def.contract.publishes) {
            if (!QFileInfo(pr.src).isAbsolute()) pr.src = toNative(pr.src, dataRoot);
            if (!QFileInfo(pr.dst).isAbsolute()) pr.dst = toNative(pr.dst, dataRoot);
        }
    }
}

} // namespace task