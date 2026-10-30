// ModuleManifest.h — 模块自描述契约（module.manifest.json）加载与聚合校验
// 规范：docs/MODULE_MANIFEST_SPEC.md v1.0
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

namespace task {

struct ManifestArtifact {
    QString ref;              // <owner>/<name>
    QString path;             // 相对 dataRoot（POSIX 风格）
    QString schemaVersion;    // 对外工件必填
    QJsonObject units;
    QString publishTo;        // 正式件落点（仅允许 Model/ 或 Setting/ 前缀），空 = 不发布
};

struct ManifestStage {
    QString id;
    QString executable;
    QStringList argsTemplate; // 含 {dataRoot}/{runtimeDir}/{artifact:<ref>}/{override:<key>}
    QStringList allowedArgs;  // UI 可覆盖参数白名单
    bool optional = false;
    QMap<QString, QString> exitCodes;   // "0" -> "ok" 等语义登记
    QString errorType;        // none | qs-json-stderr
    QString traceLog;
    QStringList consumesInternal;
    QStringList producesInternal;
};

struct ModuleManifest {
    int manifestVersion = 0;
    QString id;               // providerId = 模块包目录名
    QString name;
    QString version;          // 交付批次
    int apiVersion = 0;
    int minApiVersion = 1;
    QStringList executables;
    QVector<ManifestStage> stages;
    QVector<ManifestArtifact> produced;
    QVector<ManifestArtifact> consumed;
    QStringList dependsOnModules;
    QStringList dependsOnArtifacts;
    QJsonObject environment;
    QString dataLayout;
    QJsonArray breakingChanges;

    const ManifestStage* stage(const QString& id) const
    {
        for (const ManifestStage& s : stages)
            if (s.id == id) return &s;
        return nullptr;
    }
    const ManifestArtifact* artifact(const QString& ref) const
    {
        for (const ManifestArtifact& a : produced)
            if (a.ref == ref) return &a;
        return nullptr;
    }
};

// 聚合后的跨模块路由表：ref -> 生产者 + 磁盘路径
struct RefRoute {
    QString producerId;       // 生产者 providerId；user/* 为 "user"
    QString path;             // 相对 dataRoot
    QString schemaVersion;
};

class ManifestLoader
{
public:
    // 从模块包目录加载 module.manifest.json；失败返回 false 并设置 error
    static bool loadFromDir(const QString& packageDir, ModuleManifest* out, QString* error);

    // 解析单个 manifest JSON 对象（供测试/复用）
    static bool parse(const QJsonObject& obj, ModuleManifest* out, QString* error);

    // ── 聚合校验（注册期，全部通过才可部署）──
    // routes: 输出的 ref 路由表（含 user/* 条目）
    // errors: 阻断性错误明细（每条一行，定位到字段）
    // warnings: 非阻断提示（如包内残留 DATA/）
    static bool validateAggregate(const QMap<QString, ModuleManifest>& manifests,
                                  const QStringList& packageDirs,
                                  QMap<QString, RefRoute>* routes,
                                  QStringList* errors, QStringList* warnings);

    // 单 manifest 自检（模块团队提交前自查用，规则同聚合校验的单包部分）
    static void selfCheck(const ModuleManifest& m, const QString& packageDir,
                          QStringList* errors, QStringList* warnings);

private:
    static bool parseArtifact(const QJsonObject& o, ManifestArtifact* out);
};

} // namespace task
