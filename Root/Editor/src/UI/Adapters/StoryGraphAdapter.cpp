#include "UI/Adapters/StoryGraphAdapter.h"
#include "UI/Adapters/StoryEventAdapter.h"
#include "UI/Styles/StoryEventStyle.h"
#include "Story/StoryEvents.h"
#include "Documents/StoryGraphDocument.h"
#include "UI/Styles/StoryGraphStyle.h"
#include <QHash>
#include <QQueue>
#include <algorithm>

namespace EditorUi
{
StoryGraphModel StoryGraphAdapter::Load(const std::filesystem::path& Path)
{
    StoryGraphDocument Document;
    std::string Error;
    if (!Document.Open(Path, Error))
    {
        StoryGraphModel Model;
        Model.Diagnostic = QString::fromStdString(Error);
        return Model;
    }
    return Build(Document);
}

StoryGraphModel StoryGraphAdapter::Build(const StoryGraphDocument& Document)
{
    StoryGraphModel Model;
    Model.bOpen = Document.IsOpen();
    Model.bReadOnly = Document.IsReadOnly();
    Model.bDirty = Document.IsDirty();
    Model.bCanUndo = Document.CanUndo();
    Model.bCanRedo = Document.CanRedo();
    Model.DocumentIdentity = QString::fromStdString(
        Document.GetAsset().IsValid() ? Document.GetAsset().Asset.ToString() : Document.GetPath().generic_string());
    if (!Document.IsOpen())
    {
        Model.Diagnostic = QString::fromUtf8("Откройте историю, чтобы добавить ноды.");
        return Model;
    }
    Model.Title = QString::fromStdString(Document.GetStory().Name);
    Model.Start = QString::fromStdString(Document.GetStory().StartNodeId);
    const auto Positions = Document.GetPositions();
    for (const auto& Event : Document.GetStory().Events) { Model.Events.push_back(StoryEventAdapter::ToView(Event)); }
    for (const auto& Node : Document.GetStory().Nodes)
    {
        StoryGraphNode Item;
        Item.Identity = QString::fromStdString(Node.Id);
        Item.KindId = static_cast<int>(Node.Kind);
        Item.Speaker = QString::fromStdString(Node.Speaker);
        Item.Text = QString::fromStdString(Node.Kind == StoryNodeKind::Choice ? Node.Prompt : Node.Text);
        Item.bStart = Node.Id == Document.GetStory().StartNodeId;
        for (const auto& Batch : Node.EventBatches)
        {
            StoryGraphEventBatch Group{QString::fromStdString(Batch.Id), static_cast<int>(Batch.Phase), Batch.bParallel};
            for (const auto& Binding : Batch.Bindings)
            {
                StoryGraphEventBinding Placement;
                Placement.Identity = QString::fromStdString(Binding.Id);
                Placement.EventId = QString::fromStdString(Binding.EventId);
                Placement.Join = static_cast<int>(Binding.Join);
                for (const auto& Action : Binding.ActionOverrides) { Placement.Overrides.push_back(StoryEventAdapter::ToView(Action)); }
                if (const auto* Event = FindStoryEvent(Document.GetStory(), Binding.EventId))
                {
                    auto Effective = StoryEventAdapter::ToView(ResolveStoryEvent(*Event, Binding));
                    Placement.Name = Effective.Name;
                    Placement.Summary = StoryEventStyle::Summary(Effective);
                }
                Group.Bindings.push_back(std::move(Placement));
            }
            Item.EventBatches.push_back(std::move(Group));
        }
        switch (Node.Kind)
        {
        case StoryNodeKind::Line:
            Item.Kind = QString::fromUtf8("Реплика");
            break;
        case StoryNodeKind::Choice:
            Item.Kind = QString::fromUtf8("Выбор");
            break;
        case StoryNodeKind::Action:
            Item.Kind = QString::fromUtf8("Действие");
            break;
        case StoryNodeKind::End:
            Item.Kind = QString::fromUtf8("Концовка");
            break;
        }
        for (const auto& Choice : Node.Choices)
        {
            Item.Choices.push_back({QString::fromStdString(Choice.Label), QString::fromStdString(Choice.NextNodeId),
                                    QString::fromStdString(Choice.Id)});
            if (Node.Kind == StoryNodeKind::Choice)
            {
                Item.Targets.push_back(QString::fromStdString(Choice.NextNodeId));
            }
        }
        for (const auto& Action : Node.Actions)
        {
            StoryGraphAction Value;
            Value.Identity = QString::fromStdString(Action.Id);
            Value.Kind = static_cast<int>(Action.Kind);
            Value.Duration = Action.DurationSeconds;
            Value.Target = QString::fromStdString(Action.TargetObjectId);
            Value.LegacyTarget = QString::fromStdString(Action.TargetObjectName);
            Value.Position = {Action.TargetPosition.x, Action.TargetPosition.y, Action.TargetPosition.z};
            Value.Rotation = {Action.TargetRotation.x, Action.TargetRotation.y, Action.TargetRotation.z,
                              Action.TargetRotation.w};
            Value.bHasRotation = Action.bHasTargetRotation;
            Value.bCopyTarget = Action.bUseTargetObjectTransform;
            const Vector3 Angles = Action.TargetRotation.ToEuler() * (180.0f / 3.14159265358979323846f);
            Value.RotationDegrees = {Angles.x, Angles.y, Angles.z};
            Item.Actions.push_back(Value);
        }
        if (Node.Kind == StoryNodeKind::Line || Node.Kind == StoryNodeKind::Action)
        {
            Item.Targets.push_back(QString::fromStdString(Node.NextNodeId));
        }
        const auto Found = Positions.find(Node.Id);
        if (Found != Positions.end())
        {
            Item.Position = QPointF(Found->second.X, Found->second.Y);
            Item.bHasPosition = true;
        }
        Item.Label = Item.Text.isEmpty() ? Item.Kind + QStringLiteral(" %1").arg(Model.Nodes.size() + 1)
                                         : Item.Text.simplified().left(42);
        Model.Nodes.push_back(std::move(Item));
    }
    QHash<QString, int> Depths;
    QHash<QString, int> Indices;
    for (int Index = 0; Index < Model.Nodes.size(); ++Index)
    {
        Indices.insert(Model.Nodes[Index].Identity, Index);
    }
    QQueue<QString> Pending;
    if (Indices.contains(Model.Start))
    {
        Pending.enqueue(Model.Start);
        Depths.insert(Model.Start, 0);
    }
    while (!Pending.isEmpty())
    {
        const auto Identity = Pending.dequeue();
        for (const auto& Target : Model.Nodes[Indices.value(Identity)].Targets)
        {
            if (Indices.contains(Target) && !Depths.contains(Target))
            {
                Depths.insert(Target, Depths.value(Identity) + 1);
                Pending.enqueue(Target);
            }
        }
    }
    QHash<int, qreal> NextRow;
    for (auto& Node : Model.Nodes)
    {
        const int Depth = Depths.value(Node.Identity, 0);
        if (!Node.bHasPosition)
        {
            Node.Position = QPointF(40 + Depth * StoryGraphStyle::ColumnSpacing, NextRow.value(Depth, 30));
            Node.bHasPosition = true;
        }
        NextRow[Depth] = std::max(NextRow.value(Depth, 30), Node.Position.y() + StoryGraphStyle::Height(Node) + 90);
    }
    return Model;
}
} // namespace EditorUi
