#include <QtGlobal>
#include "config/PipelineLoader.h"

#include <functional>

#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

namespace task {

void PathResolver::setBase(const QString& dataRoot, const QString& moduleRoot,
                           const QString& binRoot, const QString& taskDir)
{
    m_baseVars.clear();
    m_baseVars["{DATA}"] = dataRoot;
    m_baseVars["{Model}"] = dataRoot + "/Model";
    m_baseVars["{Setting}"] = dataRoot + "/Setting";
    m_baseVars["{Result}"] = dataRoot + "/Result";
    m_baseVars["{BIN}"] = binRoot;
    m_baseVars["{MODULE_ROOT}"] = moduleRoot;
    m_baseVars["{CASE_ROOT}"] = dataRoot;
    m_baseVars["{TASK_DIR}"] = taskDir;
    m_baseVars["{TASK_JSON}"] = taskDir + "/task.json";
}

QString PathResolver::varNameFromKey(const QString& key)
{
    // 驼峰转大写下划线：portJson -> PORT_JSON，setFile -> SET_FILE
    QString out;
    for (int i = 0; i < key.size(); ++i) {
        const QChar c = key.at(i);
        if (i > 0 && c.isUpper() && !key.at(i - 1).isUpper()) out += '_';
        out += c.toUpper();
    }
    return out;
}

void PathResolver::setInputs(const QJsonObject& inputs)
{
    m_inputs = inputs;
    m_extraVars.clear();
    for (auto it = inputs.begin(); it != inputs.end(); ++it) {
        const QString raw = it.value().toString().trimmed();
        if (raw.isEmpty()) continue;
        const QString name = varNameFromKey(it.key());
        m_extraVars["{" + name + "}"] = raw;
        m_extraVars["{" + name.toUpper() + "}"] = raw;   // 兼容直接写全大写键
    }
}

bool PathResolver::hasVar(const QString& name) const
{
    return m_baseVars.contains(name) || m_extraVars.contains(name);
}

QJsonObject PathResolver::resolvedInputs() const
{
    QJsonObject out;
    for (auto it = m_inputs.begin(); it != m_inputs.end(); ++it)
        out[it.key()] = resolve(it.value().toString().trimmed());
    return out;
}

QString PathResolver::resolve(const QString& expr) const
{
    QString out = expr;
    // 先展开动态变量（其值可能本身含 {DATA} 等固定变量），再展开固定变量
    for (auto it = m_extraVars.cbegin(); it != m_extraVars.cend(); ++it)
        out.replace(it.key(), it.value());
    for (auto it = m_baseVars.cbegin(); it != m_baseVars.cend(); ++it)
        out.replace(it.key(), it.value());
    out.replace('/', QDir::separator());
    return out;
}

QStringList PathResolver::resolveList(const QStringList& exprs) const
{
    QStringList out;
    for (const QString& e : exprs) out << resolve(e);
    return out;
}

// userGate 结构校验（阶段库定义与内联阶段共用）：
//   waitForFiles 非空字符串数组 / confirm 布尔 / inputs 唯一非空 name
bool validateUserGate(const QJsonObject& gate, const QString& ctx, QString* error)
{
    const QJsonArray wf = gate.value("waitForFiles").toArray();
    for (const QJsonValue& fv : wf) {
        if (!fv.isString() || fv.toString().isEmpty()) {
            if (error) *error = ctx + ": userGate.waitForFiles must be non-empty strings";
            return false;
        }
    }
    if (!gate.value("confirm").isUndefined() && !gate.value("confirm").isBool()) {
        if (error) *error = ctx + ": userGate.confirm must be a boolean";
        return false;
    }
    const QJsonArray ins = gate.value("inputs").toArray();
    QSet<QString> inames;
    for (const QJsonValue& iv : ins) {
        const QJsonObject io = iv.toObject();
        const QString name = io.value("name").toString();
        if (name.isEmpty() || inames.contains(name)) {
            if (error) *error = ctx + ": userGate.inputs need unique non-empty 'name'";
            return false;
        }
        inames.insert(name);
    }
    return true;
}

bool PipelineLoader::parseStageLib(const QString& ref, const QJsonObject& obj, QString* error)
{
    // 阶段库条目：module 必填（库定义接线）；其余键可选，结构非法 -> 加载期拒绝
    const QString ctx = QString("stage library ref '%1'").arg(ref);
    const QString moduleId = obj.value("module").toString();
    if (moduleId.isEmpty()) {
        if (error) *error = ctx + ": 'module' (non-empty string) is required";
        return false;
    }
    StageLibDef lib;
    lib.moduleId = moduleId;
    lib.name = obj.value("name").toString();
    if (obj.contains("params") && !obj.value("params").isObject()) {
        if (error) *error = ctx + ": 'params' must be an object";
        return false;
    }
    lib.params = obj.value("params").toObject();
    if (obj.contains("artifacts") && !obj.value("artifacts").isObject()) {
        if (error) *error = ctx + ": 'artifacts' must be an object";
        return false;
    }
    const QJsonObject arts = obj.value("artifacts").toObject();
    for (auto ait = arts.constBegin(); ait != arts.constEnd(); ++ait)
        lib.artifacts.insert(ait.key(), ait.value().toString());
    if (obj.contains("requires") && !obj.value("requires").isArray()) {
        if (error) *error = ctx + ": 'requires' must be an array";
        return false;
    }
    for (const QJsonValue& rv : obj.value("requires").toArray())
        lib.requires << rv.toString();
    if (obj.contains("optional") && !obj.value("optional").isBool()) {
        if (error) *error = ctx + ": 'optional' must be a boolean";
        return false;
    }
    lib.optional = obj.value("optional").toBool(false);
    const QJsonValue gv = obj.value("userGate");
    if (!gv.isUndefined() && !gv.isNull()) {
        if (!gv.isObject()) {
            if (error) *error = ctx + ": userGate must be an object";
            return false;
        }
        lib.userGate = gv.toObject();
        if (!validateUserGate(lib.userGate, ctx, error)) return false;
    }
    m_stageLib.insert(ref, lib);
    return true;
}

bool PipelineLoader::parsePipeline(const QString& id, const QJsonObject& obj, QString* error,
                                         const QString& source)
{
    PipelineDef def;
    def.source = source;
    def.id = id;
    def.displayName = obj.value("displayName").toString(id);
    def.description = obj.value("description").toString();
    def.maxParallel = obj.value("maxParallel").toInt(1);

    const QJsonArray arr = obj.value("stages").toArray();
    for (const QJsonValue& v : arr) {
        const QJsonObject so = v.toObject();
        PipelineStage s;
        s.id = so.value("id").toString();

        // P2：阶段条目 ref（阶段库引用）与内联 module（v1 形态）二选一，必居其一
        const bool hasRef = so.value("ref").isString() && !so.value("ref").toString().isEmpty();
        const bool hasModule = so.value("module").isString() && !so.value("module").toString().isEmpty();
        if (hasRef == hasModule) {
            *error = QString("pipeline '%1': stage '%2': exactly one of 'ref' / 'module' required")
                         .arg(id, s.id);
            return false;
        }

        if (hasRef) {
            // 物化（P2）：库定义 + 本地覆盖 -> 与 v1 解析完全相同的 PipelineStage（编排器无感）
            const QString ref = so.value("ref").toString();
            const StageLibDef* lib = stageLib(ref);
            if (!lib) {
                *error = QString("pipeline '%1': stage '%2': unknown ref '%3' (stage library, fail-fast)")
                             .arg(id, s.id, ref);
                return false;
            }
            s.ref = ref;
            s.moduleId = lib->moduleId;
            // name：本地有则本地，否则库定义，否则 stage id（现有兜底）
            const QString lname = so.value("name").toString();
            s.name = !lname.isEmpty() ? lname : (!lib->name.isEmpty() ? lib->name : s.id);
            // optional：本地有则本地，否则库定义
            s.optional = so.contains("optional") ? so.value("optional").toBool(false)
                                                 : lib->optional;
            // requires：本地有则用本地，否则用库定义
            if (so.contains("requires")) {
                for (const QJsonValue& rv : so.value("requires").toArray())
                    s.optionalInputs << rv.toString();
            } else {
                s.optionalInputs = lib->requires;
            }
            // params / artifacts：对象按键合并，本地键赢
            for (auto pit = lib->params.constBegin(); pit != lib->params.constEnd(); ++pit)
                s.params[pit.key()] = pit.value();
            const QJsonObject lparams = so.value("params").toObject();
            for (auto pit = lparams.constBegin(); pit != lparams.constEnd(); ++pit)
                s.params[pit.key()] = pit.value();
            for (auto kvit = lib->artifacts.constBegin(); kvit != lib->artifacts.constEnd(); ++kvit)
                s.artifacts.insert(kvit.key(), kvit.value());
            const QJsonObject larts = so.value("artifacts").toObject();
            for (auto ait = larts.constBegin(); ait != larts.constEnd(); ++ait)
                s.artifacts.insert(ait.key(), ait.value().toString());
            // userGate：库定义为默认，本地可 extends（新增键）；库级闸门键不可被本地覆盖/取消
            s.userGate = lib->userGate;
            const QJsonValue lg = so.value("userGate");
            if (!lg.isUndefined() && !lg.isNull()) {
                if (!lg.isObject()) {
                    *error = QString("pipeline '%1': stage '%2': userGate must be an object").arg(id, s.id);
                    return false;
                }
                const QJsonObject lgate = lg.toObject();
                if (!validateUserGate(lgate,
                                      QString("pipeline '%1': stage '%2'").arg(id, s.id), error))
                    return false;
                for (auto git = lgate.constBegin(); git != lgate.constEnd(); ++git)
                    if (!s.userGate.contains(git.key()))
                        s.userGate[git.key()] = git.value();   // 仅新增键
            }
        } else {
            // v1 内联形态（module）：全部字段来自本地
            s.moduleId = so.value("module").toString();
            s.name = so.value("name").toString(s.id);
            s.optional = so.value("optional").toBool(false);
            s.params = so.value("params").toObject();
            const QJsonObject arts = so.value("artifacts").toObject();
            for (auto ait = arts.constBegin(); ait != arts.constEnd(); ++ait)
                s.artifacts.insert(ait.key(), ait.value().toString());
            for (const QJsonValue& rv : so.value("requires").toArray())
                s.optionalInputs << rv.toString();
            // P1.5 用户闸门（可选键）：结构非法 → 加载期拒绝
            const QJsonValue gv = so.value("userGate");
            if (!gv.isUndefined() && !gv.isNull()) {
                if (!gv.isObject()) {
                    *error = QString("pipeline '%1': stage '%2': userGate must be an object").arg(id, s.id);
                    return false;
                }
                s.userGate = gv.toObject();
                if (!validateUserGate(s.userGate,
                                      QString("pipeline '%1': stage '%2'").arg(id, s.id), error))
                    return false;
            }
        }

        for (const QJsonValue& d : so.value("dependsOn").toArray())
            s.dependsOn << d.toString();
        if (s.id.isEmpty() || s.moduleId.isEmpty()) {
            *error = QString("pipeline '%1': stage requires 'id' and ('ref' or 'module')").arg(id);
            return false;
        }
        def.stages << s;
    }
    if (def.stages.isEmpty()) {
        *error = QString("pipeline '%1': no stages").arg(id);
        return false;
    }

    // ---- DAG 校验：阶段 id 唯一、依赖必须存在、不能成环 ----
    // 没有这一步校验，错误配置会在调度时表现为“任务永远不结束”。
    QSet<QString> ids;
    for (const PipelineStage& s : def.stages) {
        if (ids.contains(s.id)) {
            *error = QString("pipeline '%1': duplicate stage id '%2'").arg(id, s.id);
            return false;
        }
        ids.insert(s.id);
    }
    QHash<QString, QStringList> deps;
    for (const PipelineStage& s : def.stages) {
        for (const QString& d : s.dependsOn) {
            if (!ids.contains(d)) {
                *error = QString("pipeline '%1': stage '%2' depends on unknown stage '%3'")
                             .arg(id, s.id, d);
                return false;
            }
            if (d == s.id) {
                *error = QString("pipeline '%1': stage '%2' depends on itself").arg(id, s.id);
                return false;
            }
            deps[s.id] << d;
        }
    }
    QHash<QString, int> color;   // 0=未访问 1=访问中 2=完成
    std::function<bool(const QString&)> visit = [&](const QString& n) -> bool {
        color[n] = 1;
        for (const QString& d : deps.value(n)) {
            if (color.value(d) == 1) return false;             // 回边 = 环
            if (color.value(d) == 0 && !visit(d)) return false;
        }
        color[n] = 2;
        return true;
    };
    for (const PipelineStage& s : def.stages) {
        if (color.value(s.id) == 0 && !visit(s.id)) {
            *error = QString("pipeline '%1': dependsOn cycle detected at stage '%2'")
                         .arg(id, s.id);
            return false;
        }
    }

    m_pipelines.insert(id, def);
    return true;
}

bool PipelineLoader::loadFromFile(const QString& mainPath, const QString& localPath, QString* error)
{
    QFile f(mainPath);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = QString("cannot open %1").arg(mainPath);
        return false;
    }
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = QString("pipelines.json parse error: %1").arg(perr.errorString());
        return false;
    }
    const QJsonObject root = doc.object();
    const int schema = root.value("schemaVersion").toInt(1);
    if (schema < 1 || schema > 2) {
        *error = QString("pipelines.json: unsupported schemaVersion %1 (1 = all-inline, 2 = stage library)")
                     .arg(schema);
        return false;
    }
    m_schemaVersion = schema;
    if (schema == 2) {
        // 先解析阶段库（顶层 stages 对象）：ref 存在性在此 fail-fast
        const QJsonObject libs = root.value("stages").toObject();
        for (auto it = libs.begin(); it != libs.end(); ++it)
            if (!parseStageLib(it.key(), it.value().toObject(), error)) return false;
    }
    const QJsonObject pipes = root.value("pipelines").toObject();
    for (auto it = pipes.begin(); it != pipes.end(); ++it) {
        if (!parsePipeline(it.key(), it.value().toObject(), error)) return false;
    }

    // P3 L1 部署级覆盖：pipelines.local.json（可选文件）
    //   - 同名 pipeline id -> local 赢；同名阶段库 ref -> local 赢（均启动日志留痕）
    //   - 与主文件同一套加载期校验；任何错误整文件拒载（fail-fast，不静默忽略）
    if (!localPath.isEmpty() && QFile::exists(localPath)) {
        QFile lf(localPath);
        if (!lf.open(QIODevice::ReadOnly)) {
            *error = QString("pipelines.local.json: cannot open %1").arg(localPath);
            return false;
        }
        QJsonParseError lperr;
        const QJsonDocument ldoc = QJsonDocument::fromJson(lf.readAll(), &lperr);
        if (lperr.error != QJsonParseError::NoError || !ldoc.isObject()) {
            *error = QString("pipelines.local.json parse error: %1").arg(lperr.errorString());
            return false;
        }
        const QJsonObject lroot = ldoc.object();
        const int lschema = lroot.value("schemaVersion").toInt(2);
        if (lschema < 1 || lschema > 2) {
            *error = QString("pipelines.local.json: unsupported schemaVersion %1").arg(lschema);
            return false;
        }
        // local 阶段库先行并入（同名 ref local 赢），其 pipeline 的 ref 才能跨主/local 库解析
        if (lschema == 2) {
            const QJsonObject llibs = lroot.value("stages").toObject();
            for (auto it = llibs.begin(); it != llibs.end(); ++it) {
                const bool shadow = m_stageLib.contains(it.key());
                QString sub;
                if (!parseStageLib(it.key(), it.value().toObject(), &sub)) {
                    *error = "pipelines.local.json: " + sub;
                    return false;
                }
                if (shadow)
                    qWarning("[config] pipelines.local.json: stage library ref '%s' overrides builtin",
                             qPrintable(it.key()));
            }
        }
        const QJsonObject lpipes = lroot.value("pipelines").toObject();
        for (auto it = lpipes.begin(); it != lpipes.end(); ++it) {
            if (m_pipelines.contains(it.key()))
                qWarning("[config] pipelines.local.json: pipeline '%s' overrides builtin",
                         qPrintable(it.key()));
            QString sub;
            if (!parsePipeline(it.key(), it.value().toObject(), &sub, "local")) {
                *error = "pipelines.local.json: " + sub;
                return false;
            }
        }
    }
    return true;
}

} // namespace task
