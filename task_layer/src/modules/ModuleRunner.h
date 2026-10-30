// ModuleRunner.h — 模块执行（external exe / DLL worker 两种形态）+ 通用 publisher
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QProcess>
#include <QStringList>
#include <QVector>

#include "config/ModuleRegistry.h"
#include "config/PipelineLoader.h"

namespace task {

struct RunResult {
    bool ok = false;
    int exitCode = -1;
    QString errorCode;      // 机器错误码（QS-TL-* 或模块自身 QS-<域>-*）
    QString errorMessage;   // 用户可见
    QStringList outputs;    // 输出校验通过的文件
};

// 单阶段执行器：拼装命令、启动进程、浅检输出、publisher 落位正式件
class ModuleRunner : public QObject
{
    Q_OBJECT
public:
    explicit ModuleRunner(QObject* parent = nullptr) : QObject(parent) {}
    ~ModuleRunner() override;

    void launch(const ModuleDef& def, const QStringList& args,
                const PathResolver& resolver, const QString& dataRoot,
                const QString& stageId, const QString& taskId, int timeoutSec);

    void cancel();

    static QString locateCommand(const ModuleDef& def, const PathResolver& resolver);

signals:
    void stageOutput(const QString& stageId, const QString& line);
    void stageFinished(const QString& stageId, const RunResult& result);

private slots:
    void onProcessOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);

private:
    QString writeJobJson(const ModuleDef& def, const QStringList& args,
                         const PathResolver& resolver, const QString& dataRoot,
                         const QString& stageId, const QString& taskId);
    void finishFailure(const QString& code, const QString& message);
    void finishSuccess(const QStringList& outputs);
    // 通用 publisher：按 manifest publishTo 把 produced 工件复制到正式件落点（模块无关）
    bool runPublishers(const ModuleDef& def);

    QProcess* m_proc = nullptr;
    ModuleDef m_def;
    PathResolver m_resolver;
    QString m_stageId;
    QString m_taskId;
    QString m_dataRoot;
    RunResult m_result;
    QByteArray m_tail;          // 滚动保留最近一段原始输出（失败时提取模块错误码）
    int m_timeoutSec = 0;
    bool m_cancelled = false;
    bool m_timedOut = false;
    bool m_finished = false;
};

} // namespace task
