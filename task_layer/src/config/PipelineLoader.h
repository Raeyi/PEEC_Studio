// PipelineLoader.h — 流水线定义加载 + 路径变量解析
#pragma once

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

namespace task {

struct PipelineStage {
    QString id;
    QString moduleId;
    QString name;
    QString ref;             // P2 阶段库引用（v2 形态；内联 module 形态为空）
    QStringList dependsOn;   // 依赖的阶段 id（空 = 无依赖）
    bool optional = false;   // 可选阶段：输入缺失或失败时不阻断主链
    QJsonObject params;      // 传给模块的参数（覆盖模块 args 中的变量）
    QMap<QString, QString> artifacts;  // <ref> -> dataRoot 相对路径（跨模块工件路由）
    QStringList optionalInputs;        // 可选阶段的存在性前置（dataRoot 相对路径）：缺失则跳过
    QJsonObject userGate;                // P1.5 用户闸门（可选）：{waitForFiles:[dataRoot 相对路径], confirm:bool,
                                          // inputs:[{name,type,required,description}]}；缺省 = 无闸门
};

struct PipelineDef {
    QString id;
    QString displayName;
    QString description;
    QVector<PipelineStage> stages;
    int maxParallel = 1;     // 最大并行阶段数
    QString source = "builtin";   // P3：来源标记（builtin / local，pipelines.local.json 覆盖）
};

// 路径变量解析器：
//   固定变量 {DATA} {Model} {Setting} {Result} {BIN} {MODULE_ROOT} {CASE_ROOT} {TASK_DIR}
//   动态变量 {PORT_JSON} 等，由 UI 通过 task.run/inputs.validate 的 inputs 传入
//   （键名驼峰转大写下划线：portJson -> {PORT_JSON}）
// P2 阶段库定义（pipelines.json schemaVersion 2 顶层 stages 对象）：
// 同一模块接线的"库级默认"（模块/参数/工件路由/前置/可选/闸门），pipeline 阶段经 ref 引用
// 并做本地覆盖（params/artifacts 键级合并本地赢；requires/optional 本地有则本地赢；
// userGate 本地只能 extends 新增键，不能取消库级闸门）。
struct StageLibDef {
    QString moduleId;
    QString name;
    QJsonObject params;
    QMap<QString, QString> artifacts;
    QStringList requires;         // 存在性前置（dataRoot 相对路径）
    bool optional = false;
    QJsonObject userGate;
};

// Path variable resolver:
class PathResolver
{
public:
    void setBase(const QString& dataRoot, const QString& moduleRoot,
                 const QString& binRoot, const QString& taskDir);

    // UI 动态输入（如 source_em.json 路径）。值本身可含固定变量，会一并展开。
    void setInputs(const QJsonObject& inputs);
    void clearInputs() { m_inputs = QJsonObject(); m_extraVars.clear(); }
    QJsonObject inputs() const { return m_inputs; }
    // 展开后的 inputs（供 job.json / state.json 记录）
    QJsonObject resolvedInputs() const;

    bool hasVar(const QString& name) const;       // name 含花括号，如 {PORT_JSON}
    QString resolve(const QString& expr) const;   // 展开全部变量
    QStringList resolveList(const QStringList& exprs) const;

private:
    static QString varNameFromKey(const QString& key);

    QMap<QString, QString> m_baseVars;    // setBase 建立
    QMap<QString, QString> m_extraVars;   // setInputs 建立
    QJsonObject m_inputs;
};

class PipelineLoader
{
public:
    // schemaVersion 1（全内联）与 2（阶段库 + ref 混合）均可；>=3 拒绝。
    // P3：localPath（pipelines.local.json，可选）= 部署级覆盖——同名 pipeline id / 阶段库
    // ref 均为 local 赢（启动日志留痕）；local 文件任何错误整文件拒载（fail-fast）。
    bool loadFromFile(const QString& mainPath, const QString& localPath, QString* error);
    const PipelineDef* pipeline(const QString& id) const
    {
        const auto it = m_pipelines.find(id);
        return it == m_pipelines.end() ? nullptr : &it.value();
    }
    QVector<PipelineDef> allPipelines() const { return m_pipelines.values().toVector(); }
    int schemaVersion() const { return m_schemaVersion; }
    const StageLibDef* stageLib(const QString& ref) const
    {
        const auto it = m_stageLib.find(ref);
        return it == m_stageLib.end() ? nullptr : &it.value();
    }

private:
    bool parseStageLib(const QString& ref, const QJsonObject& obj, QString* error);
    bool parsePipeline(const QString& id, const QJsonObject& obj, QString* error,
                         const QString& source = "builtin");
    int m_schemaVersion = 1;
    QMap<QString, StageLibDef> m_stageLib;
    QMap<QString, PipelineDef> m_pipelines;
};

} // namespace task
