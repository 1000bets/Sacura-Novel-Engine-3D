#include "UI/Adapters/StoryGraphController.h"
#include "UI/Adapters/StoryGraphAdapter.h"
#include "UI/Views/StoryInspectorView.h"
#include "UI/Views/StoryWorkspaceView.h"
#include <QTimer>

namespace EditorUi
{
StoryGraphController::StoryGraphController(StoryWorkspaceView& InView, StoryInspectorView& InInspector)
    : View(InView), Inspector(InInspector)
{
    View.AddNarrativeRequested = [this](QString Type, QPointF Position)
    {
        if (bLocked || !Document.IsOpen())
        {
            return;
        }
        FlushEdits();
        StoryNodeId Created;
        std::string Error;
        const bool Success = Document.AddNarrativeNode(Type.toStdString(), {Position.x(), Position.y()}, Created, Error);
        if (Success)
        {
            Selected = QString::fromStdString(Created);
            bSelectRequested = true;
            bForceInspector = true;
        }
        Finish(Success, Error);
    };
    View.ConnectDataRequested = [this](QString Source, QString Output, QString Target, QString Input)
    {
        if (bLocked)
        {
            return;
        }
        FlushEdits();
        std::string Error;
        const bool Success = Document.ConnectData(Source.toStdString(), Output.toStdString(), Target.toStdString(), Input.toStdString(), Error);
        Finish(Success, Error);
    };
    View.NodeSelected = [this](const StoryGraphNode& Node) { Select(Node.Identity); };
    View.AddNodeRequested = [this](int Kind, QPointF Position)
    {
        if (bLocked)
        {
            return;
        }
        if (!Document.IsOpen())
        {
            if (NewDocumentRequested)
            {
                NewDocumentRequested(Kind, Position);
            }
            return;
        }
        FlushEdits();
        StoryNodeId Created;
        std::string Error;
        const bool bSuccess =
            Document.AddNode(static_cast<StoryNodeKind>(Kind), {Position.x(), Position.y()}, Created, Error);
        if (bSuccess)
        {
            Selected = QString::fromStdString(Created);
            bSelectRequested = true;
            bForceInspector = true;
        }
        Finish(bSuccess, Error);
    };
    View.DeleteNodesRequested = [this](QStringList Identities)
    {
        if (bLocked)
        {
            return;
        }
        FlushEdits();
        std::vector<StoryNodeId> Removed;
        for (const auto& Identity : Identities)
        {
            Removed.push_back(Identity.toStdString());
        }
        std::string Error;
        const bool bSuccess = Document.DeleteNodes(Removed, Error);
        bForceInspector = true;
        Finish(bSuccess, Error);
    };
    View.ConnectRequested = [this](QString Source, int Port, QString Target)
    {
        if (bLocked)
        {
            return;
        }
        FlushEdits();
        std::string Error;
        const bool bSuccess = Document.Connect(Source.toStdString(), Port, Target.toStdString(), Error);
        bForceInspector = true;
        Finish(bSuccess, Error);
    };
    View.MoveNodesRequested = [this](QMap<QString, QPointF> Positions)
    {
        if (bLocked)
        {
            return;
        }
        FlushEdits();
        std::map<StoryNodeId, StoryGraphPosition> Moved;
        for (auto Iterator = Positions.begin(); Iterator != Positions.end(); ++Iterator)
        {
            Moved.emplace(Iterator.key().toStdString(), StoryGraphPosition{Iterator.value().x(), Iterator.value().y()});
        }
        std::string Error;
        const bool bSuccess = Document.MoveNodes(Moved, Error);
        Finish(bSuccess, Error);
    };
    View.StartNodeRequested = [this](QString Identity)
    {
        if (bLocked)
        {
            return;
        }
        FlushEdits();
        std::string Error;
        const bool bSuccess = Document.SetStart(Identity.toStdString(), Error);
        bForceInspector = true;
        Finish(bSuccess, Error);
    };
    View.UndoRequested = [this]() { Undo(); };
    View.RedoRequested = [this]() { Redo(); };
    View.SaveRequested = [this]()
    {
        std::string Error;
        if (!Save(Error) && ErrorReported)
        {
            ErrorReported(QString::fromStdString(Error));
        }
    };
    View.OpenRequested = [this]()
    {
        if (OpenRequested)
        {
            OpenRequested();
        }
    };
    Inspector.NodeChanged = [this](StoryGraphNode Node) { EditNode(Node); };
    Inspector.StartRequested = View.StartNodeRequested;
    Inspector.UndoRequested = View.UndoRequested;
    Inspector.RedoRequested = View.RedoRequested;
    Inspector.SaveRequested = View.SaveRequested;
    ConfigureEvents();
}

StoryGraphController::~StoryGraphController()
{
    ClearEventCallbacks();
    View.NodeSelected = {};
    View.AddNodeRequested = {};
    View.AddNarrativeRequested = {};
    View.ConnectDataRequested = {};
    View.DeleteNodesRequested = {};
    View.ConnectRequested = {};
    View.MoveNodesRequested = {};
    View.StartNodeRequested = {};
    View.UndoRequested = {};
    View.RedoRequested = {};
    View.SaveRequested = {};
    View.OpenRequested = {};
    Inspector.NodeChanged = {};
    Inspector.StartRequested = {};
    Inspector.UndoRequested = {};
    Inspector.RedoRequested = {};
    Inspector.SaveRequested = {};
}

bool StoryGraphController::Open(AssetRegistry& Registry, const AssetKey& Asset, std::string& Error)
{
    if (bLocked)
    {
        Error = "Stop playback before opening another story";
        return false;
    }
    FlushEdits();
    if (!Document.Open(Registry, Asset, Error))
    {
        return false;
    }
    auto Model = StoryGraphAdapter::Build(Document);
    std::map<StoryNodeId, StoryGraphPosition> Positions;
    for (const auto& Node : Model.Nodes)
    {
        Positions.emplace(Node.Identity.toStdString(), StoryGraphPosition{Node.Position.x(), Node.Position.y()});
    }
    Document.SetInitialPositions(std::move(Positions));
    Selected = Model.Start;
    SelectedBinding.clear();
    bSelectRequested = true;
    bForceInspector = true;
    RefreshNow();
    return true;
}

void StoryGraphController::FlushEdits()
{
    // Undo/reconnection may have changed the document before the queued repaint.
    // The inspector must never write its stale controls back over that revision.
    if (!bForceInspector)
    {
        Inspector.Commit();
    }
}

bool StoryGraphController::Save(std::string& Error)
{
    if (bLocked)
    {
        Error = "Stop playback before saving the story";
        return false;
    }
    FlushEdits();
    const bool bSuccess = Document.Save(Error);
    Refresh();
    return bSuccess;
}

bool StoryGraphController::Undo()
{
    if (bLocked)
    {
        return false;
    }
    FlushEdits();
    const bool bSuccess = Document.Undo();
    bForceInspector = true;
    Refresh();
    return bSuccess;
}
bool StoryGraphController::Redo()
{
    if (bLocked)
    {
        return false;
    }
    FlushEdits();
    const bool bSuccess = Document.Redo();
    bForceInspector = true;
    Refresh();
    return bSuccess;
}
bool StoryGraphController::DeleteSelection()
{
    return !bLocked && View.DeleteSelection();
}

bool StoryGraphController::ApplyDocumentEdit(
    const std::function<bool(StoryGraphDocument&, std::string&)>& Edit, std::string& Error)
{
    if (bLocked || !Document.IsOpen())
    {
        Error = "Open a story and stop playback before editing Narrative";
        return false;
    }
    FlushEdits();
    const bool bSuccess = Edit(Document, Error);
    bForceInspector = true;
    Finish(bSuccess, Error);
    return bSuccess;
}

void StoryGraphController::SetLocked(bool bValue)
{
    if (!bLocked)
    {
        FlushEdits();
    }
    bLocked = bValue;
    bForceInspector = true;
    RefreshNow();
}
void StoryGraphController::SetObjects(QVector<QPair<QString, QString>> Values, QVector<QPair<QString, QString>> NarrativeValues,
    QMap<QString, QVector<QPair<QString, QString>>> AnimationValues)
{
    if (Objects == Values && NarrativeObjects == NarrativeValues && CharacterAnimations == AnimationValues)
    {
        return;
    }
    CharacterAnimations = std::move(AnimationValues);
    NarrativeObjects = std::move(NarrativeValues);
    Objects = std::move(Values);
    Refresh();
}

void StoryGraphController::EditNode(const StoryGraphNode& Value)
{
    if (bLocked || bForceInspector)
    {
        return;
    }
    const auto* Previous = Document.FindNode(Value.Identity.toStdString());
    if (Previous == nullptr)
    {
        return;
    }
    StoryNode Node = *Previous;
    Node.Kind = static_cast<StoryNodeKind>(Value.KindId);
    if (Node.Kind != StoryNodeKind::Line && Node.Kind != StoryNodeKind::Choice) { Node.EventBatches.clear(); }
    Node.NarrativeType = Value.NarrativeType;
    Node.NarrativeInputs = Value.NarrativeInputs;
    Node.NarrativeSources = Value.NarrativeSources;
    Node.Speaker = Value.Speaker.toStdString();
    if (Node.Kind == StoryNodeKind::Choice)
    {
        Node.Prompt = Value.Text.toStdString();
    }
    else if (Node.Kind == StoryNodeKind::Line)
    {
        Node.Text = Value.Text.toStdString();
    }
    Node.NextNodeId = Node.Kind == StoryNodeKind::Line || Node.Kind == StoryNodeKind::Action
                          ? Value.Targets.value(0).toStdString()
                          : std::string{};
    Node.Choices.clear();
    for (const auto& Choice : Value.Choices)
    {
        Node.Choices.push_back(
            {Choice.Label.toStdString(), Choice.Target.toStdString(), Choice.Identity.toStdString()});
    }
    Node.Actions.clear();
    for (const auto& Action : Value.Actions)
    {
        StoryActionDefinition Definition;
        Definition.Id = Action.Identity.toStdString();
        Definition.Kind = static_cast<StoryActionKind>(Action.Kind);
        Definition.DurationSeconds = static_cast<float>(Action.Duration);
        Definition.TargetObjectId = Action.Target.toStdString();
        Definition.TargetObjectName = Action.LegacyTarget.toStdString();
        Definition.AnimationId = Action.AnimationId.toStdString();
        Definition.TargetPosition =
            Vector3(static_cast<float>(Action.Position[0]), static_cast<float>(Action.Position[1]),
                    static_cast<float>(Action.Position[2]));
        Definition.TargetRotation =
            Quaternion(static_cast<float>(Action.Rotation[0]), static_cast<float>(Action.Rotation[1]),
                       static_cast<float>(Action.Rotation[2]), static_cast<float>(Action.Rotation[3]));
        if (Action.bRotationEdited)
        {
            constexpr float DegreesToRadians = 3.14159265358979323846f / 180.f;
            Definition.TargetRotation =
                Quaternion::CreateFromYawPitchRoll(static_cast<float>(Action.RotationDegrees[1]) * DegreesToRadians,
                                                   static_cast<float>(Action.RotationDegrees[0]) * DegreesToRadians,
                                                   static_cast<float>(Action.RotationDegrees[2]) * DegreesToRadians);
        }
        Definition.bHasTargetRotation = Action.bHasRotation;
        Definition.bUseTargetObjectTransform = Action.bCopyTarget;
        Node.Actions.push_back(Definition);
    }
    std::string Error;
    const bool bSuccess = Document.ReplaceNode(Node, Error);
    Finish(bSuccess, Error);
}

void StoryGraphController::Select(const QString& Identity)
{
    FlushEdits();
    SelectedBinding.clear();
    Selected = Identity;
    auto Model = StoryGraphAdapter::Build(Document);
    Model.Objects = Objects;
    Model.NarrativeObjects = NarrativeObjects;
    Model.CharacterAnimations = CharacterAnimations;
    Model.bReadOnly = Model.bReadOnly || bLocked;
    Model.bLocked = bLocked;
    for (const auto& Node : Model.Nodes)
    {
        if (Node.Identity == Identity)
        {
            Inspector.SetNode(Node, Model, true);
            if (SelectionActivated)
            {
                SelectionActivated();
            }
            return;
        }
    }
}

void StoryGraphController::Finish(bool bSuccess, const std::string& Error)
{
    if (!bSuccess && ErrorReported)
    {
        ErrorReported(QString::fromStdString(Error));
    }
    Refresh();
}
void StoryGraphController::Refresh()
{
    if (bRefreshPending)
    {
        return;
    }
    bRefreshPending = true;
    QTimer::singleShot(0, this,
                       [this]()
                       {
                           bRefreshPending = false;
                           RefreshNow();
                       });
}
void StoryGraphController::RefreshNow()
{
    auto Model = StoryGraphAdapter::Build(Document);
    Model.Objects = Objects;
    Model.NarrativeObjects = NarrativeObjects;
    Model.CharacterAnimations = CharacterAnimations;
    Model.bReadOnly = Model.bReadOnly || bLocked;
    Model.bLocked = bLocked;
    View.SetModel(Model);
    if (Document.FindNode(Selected.toStdString()) == nullptr)
    {
        Selected = Model.Start;
        bSelectRequested = true;
    }
    if (bSelectRequested && !Selected.isEmpty())
    {
        View.SelectNode(Selected);
    }
    bSelectRequested = false;
    if (!SelectedBinding.isEmpty()) { View.SelectEvent(Selected, SelectedBinding); }
    bool bFound = false;
    for (const auto& Node : Model.Nodes)
    {
        if (Node.Identity == Selected)
        {
            if (!ShowEventInspector(Node, Model)) { Inspector.SetNode(Node, Model, bForceInspector); }
            bFound = true;
            break;
        }
    }
    if (!bFound)
    {
        Inspector.Clear();
    }
    bForceInspector = false;
    if (Changed)
    {
        Changed();
    }
}
} // namespace EditorUi
