#pragma once
#include <QString>
#include <QVector>
#include <QPointF>
#include <array>

namespace EditorUi
{
struct StoryGraphChoice
{
    QString Label;
    QString Target;
    QString Identity;
};
struct StoryGraphAction
{
    int Kind = 0;
    double Duration = 0;
    QString Target;
    QString LegacyTarget;
    std::array<double, 3> Position{};
    std::array<double, 4> Rotation{0, 0, 0, 1};
    bool bHasRotation = false;
    bool bCopyTarget = false;
    std::array<double, 3> RotationDegrees{};
    bool bRotationEdited = false;
    QString Identity;
};
struct StoryGraphEventGroup
{
    QString Identity, Name;
    QVector<StoryGraphAction> Actions;
};
struct StoryGraphEvent
{
    QString Identity, Name, Description;
    QVector<StoryGraphEventGroup> Groups;
};
struct StoryGraphEventBinding
{
    QString Identity, EventId, Name, Summary;
    int Join = 3;
    QVector<StoryGraphAction> Overrides;
    bool bSelected = false;
};
struct StoryGraphEventBatch
{
    QString Identity;
    int Phase = 1;
    bool bParallel = false;
    QVector<StoryGraphEventBinding> Bindings;
};
struct StoryGraphNode
{
    QString Identity;
    QString Kind;
    QString Speaker;
    QString Text;
    QVector<QString> Targets;
    int KindId = 0;
    QVector<StoryGraphChoice> Choices;
    QVector<StoryGraphAction> Actions;
    QPointF Position;
    bool bHasPosition = false;
    bool bStart = false;
    QString Label;
    QVector<StoryGraphEventBatch> EventBatches;
};

// Owned presentation data; graphics items never retain a runtime/world pointer.
struct StoryGraphModel
{
    QString Title;
    QString Start;
    QString Diagnostic;
    QVector<StoryGraphNode> Nodes;
    QVector<StoryGraphEvent> Events;
    QVector<QPair<QString, QString>> Objects;
    bool bOpen = false;
    bool bReadOnly = true;
    bool bDirty = false;
    bool bCanUndo = false;
    bool bCanRedo = false;
    bool bLocked = false;
    QString DocumentIdentity;
};
} // namespace EditorUi
