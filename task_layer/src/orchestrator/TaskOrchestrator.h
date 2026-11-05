// TaskOrchestrator.h — 任务编排核心（DAG 调度：就绪优先 + 最大并行）
#pragma once

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVector>

#include "config/ModuleRegistry.h"
#include "config/PipelineLoader.h"
#include "io/ResultStore.h"
#include "modules/ModuleRunner.h"

namespace task {

// 执行选择（P1）：运行时决定"本次跑什么"，与静态 DAG 定义正交（H1 根因）。
//   full   = 全链（缺省，等价旧行为）；
//   stages = 跑 DAG 子集（按 pipeline 内 stage 本地 id 选择）；
//   resume = 基于上次任务 state.json 续跑（App 层归一为 stages 后传入编排器）。
struct Selection {
    QString mode = "full";
    QStringList stages;        // mode=stages 的有效阶段集
    QString resumeTaskId;      // mode=resume 留痕（审计）

    bool isFull() const { return mode == "full"; }   // stages 模式允许空集（resume 归一后全部假定完成 = 无操作任务）
    bool contains(const QString& stageId) const { return stages.contains(stageId); }
    QJsonObject toJson() const {
        QJsonObject o{{"mode", mode}};
        if (!stages.isEmpty()) {
            QJsonArray a;
            for (const QString& st : stages) a.append(st);
            o["stages"] = a;
        }
        if (!resumeTaskId.isEmpty()) o["resumeTaskId"] = resumeTaskId;
        return o;
    }
};

class TaskOrchestrator : public QObject
{
    Q_OBJECT
public:
    explicit TaskOrchestrator(QObject* parent = nullptr) : QObject(parent) {}

    // 运行一个任务。dataRoot：案例目录；overrides：阶段参数覆盖（stageId -> {key:value}）
    // sel：执行选择（P1，缺省 full = 全链）
    bool startTask(const QString& taskId, const PipelineDef& pipeline,
                   const ModuleRegistry& registry, const PathResolver& resolver,
                   const QString& dataRoot, const QJsonObject& overrides,
                   const Selection& sel = Selection());

    bool cancel(const QString& taskId);
    bool isBusy() const { return m_busy; }
    QString currentTaskId() const { return m_busy ? m_taskId : QString(); }

    // P1.5 用户闸门：UI 为等待中的阶段提供输入（文件出现/CLI 值/确认）。
    // 返回 false 时 *error 说明原因（任务非活动 / 阶段不在等待 / 值非法）。
    // 白名单校验在 App 层（与 overrides 同一校验路径）；本处做 gate.inputs 的
    // required/类型校验、留痕与唤醒。
    bool provideInput(const QString& taskId, const QString& stageId,
                      const QJsonObject& values, QString* error);
    // 活动任务的阶段定义（任务非活动/未知阶段 -> nullptr）；供 App 白名单校验用
    const PipelineStage* activeStage(const QString& stageId) const;
    // gate 输入留痕快照（stageId -> {values, ts, confirmed?}）；供 App 落盘 state.json
    QJsonObject providedInputs() const { return m_providedInputs; }

signals:
    void notify(const QString& method, const QJsonObject& params);  // 转发给 UI 的通知

private slots:
    void onStageOutput(const QString& stageId, const QString& line);
    void onStageFinished(const QString& stageId, const RunResult& result);

private:
    void pump();                                  // 从就绪队列启动可用阶段
    // 启动单个阶段。返回 false = 阶段进入 WaitingInput（P1.5 用户闸门未满足，留在队列等待）
    bool launchStage(const PipelineStage& s);      // 启动单个阶段（闸门未满足时先出队）
    bool isReady(const PipelineStage& s) const;    // 依赖全部 done
    bool propagateBlocked();                      // 上游失败/跳过 -> 下游标记 blocked
    void finishTask(const QString& status, const QString& summary);
    void clearRunners();
    void setStageStatus(const PipelineStage& s, StageStatus st, const QString& errorCode,
                        const QString& errorMessage);
    // 阶段磁盘输入契约检查：manifest requiredInputs（已解析绝对路径）+ 阶段 optionalInputs
    // （dataRoot 相对）；返回缺失文件列表（空 = 满足）。optional 跳过与 P1 入口复检共用。
    QStringList missingFileInputs(const ModuleDef* def, const PipelineStage& s) const;

    // P1.5 闸门检查：返回缺失项（"file:<相对路径>" / "confirm" / "input:<name>"；空 = 满足）
    QStringList gateMissing(const PipelineStage& s) const;
    // 组装 task.stage.awaiting 通知参数（gate 原样 + 当前缺失明细）
    QJsonObject buildAwaiting(const PipelineStage& s, const QStringList& missing) const;
    bool hasWaiting() const { return !m_waiting.isEmpty(); }

    QString m_taskId;
    const PipelineDef* m_pipeline = nullptr;
    const ModuleRegistry* m_registry = nullptr;
    PathResolver m_resolver;
    QString m_dataRoot;
    QJsonObject m_overrides;
    Selection m_selection;                // P1 执行选择
    QSet<QString> m_assumed;              // P1 假定完成的阶段 id（合成记录）
    QSet<QString> m_waiting;              // P1.5 处于 WaitingInput 的阶段 id（留在 m_pending，等待 task.provideInput）
    QJsonObject m_providedInputs;         // P1.5 gate 输入留痕（stageId -> {values, ts, confirmed?}）

    QMap<QString, StageRecord> m_records;      // stageId -> 运行记录
    QMap<QString, ModuleRunner*> m_running;    // stageId -> 正在运行的 runner
    QVector<PipelineStage> m_pending;          // 尚未就绪的阶段
    int m_maxParallel = 1;
    bool m_cancelled = false;
    bool m_pumping = false;                    // 防重入：launch 可能同步回调 stageFinished
    bool m_busy = false;
    qint64 m_startMs = 0;
};

} // namespace task
