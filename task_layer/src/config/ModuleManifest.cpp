#include "config/ModuleManifest.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

namespace task {

namespace {
QStringList jsonStrList(const QJsonObject& o, const QString& key)
{
    QStringList out;
    for (const QJsonValue& v : o.value(key).toArray())
        out << v.toString();
    return out;
}
} // namespace

bool ManifestLoader::parseArtifact(const QJsonObject& o, ManifestArtifact* out)
{
    out->ref = o.value("ref").toString();
    out->path = o.value("path").toString();
    out->schemaVersion = o.value("schemaVersion").toString();
    out->units = o.value("units").toObject();
    out->publishTo = o.value("publishTo").toString();
    return !out->ref.isEmpty() && !out->path.isEmpty();
}

bool ManifestLoader::parse(const QJsonObject& obj, ModuleManifest* m, QString* error)
{
    m->manifestVersion = obj.value("manifestVersion").toInt();
    if (m->manifestVersion != 1) {
        *error = QString("manifestVersion must be 1 (got %1)").arg(m->manifestVersion);
        return false;
    }
    m->id = obj.value("id").toString();
    m->name = obj.value("name").toString(m->id);
    m->version = obj.value("version").toString();
    m->apiVersion = obj.value("apiVersion").toInt();
    m->minApiVersion = obj.value("minApiVersion").toInt(1);
    if (m->id.isEmpty() || m->version.isEmpty() || m->apiVersion < 1) {
        *error = "missing id / version / apiVersion";
        return false;
    }

    const QJsonObject cli = obj.value("cli").toObject();
    m->executables = jsonStrList(cli, "executables");

    for (const QJsonValue& sv : obj.value("stages").toArray()) {
        const QJsonObject so = sv.toObject();
        ManifestStage s;
        s.id = so.value("id").toString();
        s.executable = so.value("executable").toString();
        s.argsTemplate = jsonStrList(so, "argsTemplate");
        s.allowedArgs = jsonStrList(so, "allowedArgs");
        s.optional = so.value("optional").toBool(false);
        const QJsonObject ec = so.value("exitCodes").toObject();
        for (auto it = ec.constBegin(); it != ec.constEnd(); ++it)
            s.exitCodes.insert(it.key(), it.value().toString());
        const QJsonObject epc = so.value("errorContract").toObject();
        s.errorType = epc.value("type").toString("none");
        s.traceLog = epc.value("traceLog").toString();
        s.consumesInternal = jsonStrList(so, "consumesInternal");
        s.producesInternal = jsonStrList(so, "producesInternal");
        if (s.id.isEmpty() || s.executable.isEmpty()) {
            *error = QString("stage requires 'id' and 'executable' (got id='%1')").arg(s.id);
            return false;
        }
        m->stages.append(s);
    }
    if (m->stages.isEmpty()) {
        *error = "no stages";
        return false;
    }

    const QJsonObject arts = obj.value("artifacts").toObject();
    for (const QJsonValue& av : arts.value("produced").toArray()) {
        ManifestArtifact a;
        if (!parseArtifact(av.toObject(), &a)) {
            *error = "produced artifact requires 'ref' and 'path'";
            return false;
        }
        m->produced.append(a);
    }
    for (const QJsonValue& cv : arts.value("consumed").toArray()) {
        ManifestArtifact a;
        if (!parseArtifact(cv.toObject(), &a)) {
            *error = "consumed artifact requires 'ref' and 'path'";
            return false;
        }
        m->consumed.append(a);
    }

    const QJsonObject dep = obj.value("dependsOn").toObject();
    m->dependsOnModules = jsonStrList(dep, "modules");
    m->dependsOnArtifacts = jsonStrList(dep, "artifacts");
    m->environment = dep.value("environment").toObject();

    const QJsonObject caps = obj.value("capabilities").toObject();
    m->dataLayout = caps.value("dataLayout").toString();
    m->breakingChanges = caps.value("breakingChanges").toArray();
    return true;
}

bool ManifestLoader::loadFromDir(const QString& packageDir, ModuleManifest* out, QString* error)
{
    const QString path = QDir(packageDir).filePath("module.manifest.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = QString("cannot open %1").arg(path);
        return false;
    }
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = QString("manifest parse error: %1").arg(perr.errorString());
        return false;
    }
    return parse(doc.object(), out, error);
}

void ManifestLoader::selfCheck(const ModuleManifest& m, const QString& packageDir,
                               QStringList* errors, QStringList* warnings)
{
    const QString prefix = m.id + ":";
    auto err = [&](const QString& msg) { if (errors) *errors << prefix + msg; };
    auto warn = [&](const QString& msg) { if (warnings) *warnings << prefix + msg; };

    // stage id 唯一
    QSet<QString> stageIds;
    for (const ManifestStage& s : m.stages) {
        if (stageIds.contains(s.id)) err(QString("duplicate stage id '%1'").arg(s.id));
        stageIds.insert(s.id);
        if (!m.executables.contains(s.executable))
            err(QString("stage '%1': executable '%2' not in cli.executables")
                    .arg(s.id, s.executable));
        if (!s.argsTemplate.contains("{dataRoot}"))
            err(QString("stage '%1': argsTemplate missing {dataRoot} binding (R1)").arg(s.id));
        // 禁止他模块目录名字面量（Result/<其他providerId>/）
        for (const QString& a : s.argsTemplate) {
            const int idx = a.indexOf(QStringLiteral("Result/"));
            if (idx < 0 || idx + 8 >= a.size()) continue;
            const QString rest = a.mid(idx + 7);
            const QString firstSeg = rest.section(QLatin1Char('/'), 0, 0);
            if (!firstSeg.isEmpty() && firstSeg != m.id
                    && firstSeg != QLatin1String("logs") && firstSeg != QLatin1String("tasks")) {
                err(QString("stage '%1': argsTemplate references foreign dir 'Result/%2/'")
                        .arg(s.id, firstSeg));
            }
        }
    }

    // produced 路径必须在本模块 Result/<id>/ 内；publishTo 仅 Model//Setting/
    for (const ManifestArtifact& a : m.produced) {
        const QString expect = "Result/" + m.id + "/";
        if (!a.path.startsWith(expect))
            err(QString("produced '%1': path '%2' outside Result/%3/")
                    .arg(a.ref, a.path, m.id));
        if (!a.publishTo.isEmpty()
                && !a.publishTo.startsWith("Model/")
                && !a.publishTo.startsWith("Setting/"))
            err(QString("produced '%1': publishTo '%2' must be under Model/ or Setting/")
                    .arg(a.ref, a.publishTo));
    }

    // consumed 的 user/* 与对外 ref：user 工件必须在 Model//Setting/ 下
    for (const ManifestArtifact& a : m.consumed) {
        const int slash = a.ref.indexOf('/');
        const QString owner = slash > 0 ? a.ref.left(slash) : QString();
        if (owner == "user"
                && !a.path.startsWith("Model/") && !a.path.startsWith("Setting/"))
            err(QString("consumed '%1': user artifact path must be under Model/ or Setting/")
                    .arg(a.ref));
    }

    // dependsOn.artifacts ⊇ consumed 对外 ref
    QSet<QString> declared;
    for (const QString& r : m.dependsOnArtifacts) declared.insert(r);
    for (const ManifestArtifact& a : m.consumed) {
        const int slash = a.ref.indexOf('/');
        const QString owner = slash > 0 ? a.ref.left(slash) : QString();
        if (owner != m.id && !declared.contains(a.ref))
            err(QString("consumed '%1' missing from dependsOn.artifacts").arg(a.ref));
    }

    // 包内残留可读写 DATA/（警告级）
    const QDir dataDir(QDir(packageDir).filePath("DATA"));
    if (dataDir.exists())
        warn("package contains DATA/ directory (should be examples/ only, see ONBOARDING §三)");
}

bool ManifestLoader::validateAggregate(const QMap<QString, ModuleManifest>& manifests,
                                       const QStringList& packageDirs,
                                       QMap<QString, RefRoute>* routes,
                                       QStringList* errors, QStringList* warnings)
{
    routes->clear();
    bool ok = true;
    auto err = [&](const QString& msg) { if (errors) *errors << msg; ok = false; };

    // 1) 单包自检
    for (auto mit = manifests.constBegin(); mit != manifests.constEnd(); ++mit) {
        const int pidx = packageDirs.indexOf(mit.key());
        const QString pkgDir = pidx >= 0 ? packageDirs.at(pidx) : QString();
        selfCheck(mit.value(), pkgDir, errors, warnings);
    }

    // 2) 建立生产者路由表
    for (const ModuleManifest& m : manifests) {
        for (const ManifestArtifact& a : m.produced) {
            if (routes->contains(a.ref)) {
                err(QString("ref '%1' has multiple producers (%2 and %3)")
                        .arg(a.ref, (*routes)[a.ref].producerId, m.id));
                continue;
            }
            RefRoute r;
            r.producerId = m.id;
            r.path = a.path;
            r.schemaVersion = a.schemaVersion;
            routes->insert(a.ref, r);
        }
    }
    // user/* 工件：由消费方声明的路径登记（UI 负责落位）
    for (const ModuleManifest& m : manifests) {
        for (const ManifestArtifact& a : m.consumed) {
            const int slash = a.ref.indexOf('/');
            const QString owner = slash > 0 ? a.ref.left(slash) : QString();
            if (owner == "user" && !routes->contains(a.ref)) {
                RefRoute r;
                r.producerId = "user";
                r.path = a.path;
                routes->insert(a.ref, r);
            }
        }
    }

    // 3) 消费 ref 可达性 + 上游模块存在性
    for (const ModuleManifest& m : manifests) {
        for (const ManifestArtifact& a : m.consumed) {
            const int slash = a.ref.indexOf('/');
            const QString owner = slash > 0 ? a.ref.left(slash) : QString();
            if (owner == m.id) continue;          // 内部自产自用
            if (owner == "user") continue;        // UI 提供
            if (!manifests.contains(owner)) {
                err(QString("%1: consumed ref '%2' producer '%3' not registered")
                        .arg(m.id, a.ref, owner));
                continue;
            }
            if (!routes->contains(a.ref))
                err(QString("%1: consumed ref '%2' has no route (producer does not declare it)")
                        .arg(m.id, a.ref));
        }
        for (const QString& up : m.dependsOnModules) {
            if (!manifests.contains(up))
                err(QString("%1: dependsOn.modules '%2' not registered").arg(m.id, up));
        }
    }

    // 4) apiVersion 兼容窗口（消费者 minApiVersion ≤ 生产者 apiVersion）
    for (const ModuleManifest& m : manifests) {
        for (const QString& up : m.dependsOnModules) {
            auto pit = manifests.constFind(up);
            if (pit == manifests.constEnd()) continue;
            const ModuleManifest& pm = pit.value();
            if (m.minApiVersion > pm.apiVersion) {
                err(QString("%1 requires apiVersion >= %2 but %3 provides %4")
                        .arg(m.id).arg(m.minApiVersion).arg(up).arg(pm.apiVersion));
            }
        }
    }

    // 5) 布局版本一致性
    QSet<QString> layouts;
    for (const ModuleManifest& m : manifests)
        if (!m.dataLayout.isEmpty()) layouts.insert(m.dataLayout);
    if (layouts.size() > 1)
        err(QString("mixed dataLayout versions: %1").arg(layouts.values().join(", ")));

    return ok;
}

} // namespace task
