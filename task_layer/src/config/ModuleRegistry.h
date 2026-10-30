// ModuleRegistry.h — 模块注册表（v2 路由 × 各包 manifest → 合成调度模型）
// modules.json v2 只含路由（provider/stage/searchDirs），契约来自 module.manifest.json。
#pragma once

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include "config/ModuleManifest.h"

namespace task {

enum class ModuleKind {
    External,   // 外部独立 exe
    DllWorker   // 功能 DLL + worker_host 隔离运行
};

struct ModuleContract {
    QStringList requiredInputs;   // 执行前必须存在的文件（绝对路径，由 manifest 派生）
    QStringList optionalInputs;
    QStringList outputChecks;     // 执行后应存在的文件
    bool allOutputsRequired = true;
    // publisher：阶段成功后把 produced 工件复制到正式件落点（Model//Setting/）
    struct PublishRule { QString src; QString dst; };
    QVector<PublishRule> publishes;
};

struct ModuleDef {
    QString id;                   // 路由 id（modules.json 键，如 peec.solver）
    QString displayName;
    ModuleKind kind = ModuleKind::External;
    QString provider;             // manifest id = 模块包目录名
    QString stageId;              // manifest stages[].id
    QString command;              // 可执行文件名（manifest stage.executable）
    QStringList searchDirs;       // 路径变量
    QString workingDir = "{DATA}";
    QStringList args;             // 已展开的完整命令参数（含 dataRoot 绑定）
    QString workerHost;
    int timeoutSec = 0;
    QStringList allowedArgs;      // UI overrides 白名单（--key 形式）
    ModuleContract contract;
    QString errorType = "none";   // none | qs-json-stderr
    QString traceLog;
};

class ModuleRegistry
{
public:
    // 加载 modules.json（v2 路由）。schemaVersion 1 的旧格式不再支持。
    bool loadRouting(const QString& jsonPath, QString* error);

    // 在模块根下发现并加载各 provider 包的 manifest；随后合成 ModuleDef。
    // missingProviders：有路由但包/manifest 缺失的 provider（不阻断，标记不可用）
    bool loadManifests(const QString& moduleRoot, const QStringList& extraPackageDirs,
                       QStringList* missingProviders, QString* error);

    bool hasModule(const QString& id) const { return m_modules.contains(id); }
    const ModuleDef* module(const QString& id) const
    {
        const auto it = m_modules.find(id);
        return it == m_modules.end() ? nullptr : &it.value();
    }
    QVector<ModuleDef> allModules() const { return m_modules.values().toVector(); }
    const QMap<QString, ModuleManifest>& manifests() const { return m_manifests; }

    // 用给定 dataRoot 重新展开所有模块的 args / contract 路径（每个任务开始时调用）
    void rebindDataRoot(const QString& dataRoot);

private:
    struct RoutingEntry {
        QString displayName;
        ModuleKind kind = ModuleKind::External;
        QString provider;
        QString stageId;
        QStringList searchDirs;
        QString workerHost;
    };

    bool parseRouting(const QJsonObject& obj, QString* error);
    void synthesize(const QString& moduleId, const RoutingEntry& r);

    QMap<QString, RoutingEntry> m_routing;
    QMap<QString, ModuleManifest> m_manifests;      // providerId -> manifest
    QMap<QString, QString> m_packageDirs;           // providerId -> 包目录
    QMap<QString, ModuleDef> m_modules;
    QString m_dataRoot;                             // 最近一次 rebind 的 dataRoot
};

} // namespace task
