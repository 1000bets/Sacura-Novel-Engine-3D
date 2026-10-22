#include "UI/Adapters/StoryGraphController.h"
#include "UI/Adapters/StoryEventAdapter.h"
#include "UI/Adapters/StoryGraphAdapter.h"
#include "UI/Views/StoryInspectorView.h"
#include "UI/Views/StoryEventInspectorView.h"
#include "UI/Views/StoryWorkspaceView.h"
#include <algorithm>

namespace EditorUi
{
void StoryGraphController::ConfigureEvents()
{
    View.AddEventRequested = [this](QString Node, int Phase, QString Batch, QString Source) {
        if (bLocked)
        {
            return;
        }
        FlushEdits();
        std::string Created, Error;
        const bool Success = Document.AddEvent(Node.toStdString(), static_cast<StoryEventPhase>(Phase),
                                               Source.toStdString(), Created, Error, Batch.toStdString());
        if (Success)
        {
            Selected = Node;
            SelectedBinding = QString::fromStdString(Created);
            bForceInspector = true;
        }
        Finish(Success, Error);
        if (Success && SelectionActivated)
        {
            SelectionActivated();
        }
    };
    Inspector.AddEventRequested = [this](QString Node, int Phase, QString Source) {
        View.AddEventRequested(Node, Phase, {}, Source);
    };
    View.EventSelected = [this](QString Node, QString Binding) {
        FlushEdits();
        Selected = Node;
        SelectedBinding = Binding;
        bForceInspector = true;
        View.SelectEvent(Node, Binding);
        auto Model = StoryGraphAdapter::Build(Document);
        Model.Objects = Objects;
        Model.bReadOnly |= bLocked;
        Model.bLocked = bLocked;
        for (const auto& Item : Model.Nodes)
        {
            if (Item.Identity == Node)
            {
                ShowEventInspector(Item, Model);
                break;
            }
        }
        bForceInspector = false;
        if (SelectionActivated)
        {
            SelectionActivated();
        }
    };
    Inspector.EventSelected = View.EventSelected;
    View.MoveEventRequested = [this](QString Node, QString Identity, int Phase, QString Batch, QString Before) {
        if (bLocked)
        {
            return;
        }
        FlushEdits();
        const auto* Target = Document.FindNode(Node.toStdString());
        if (!Target)
        {
            return;
        }
        for (const auto& Current : Target->EventBatches)
        {
            for (const auto& Binding : Current.Bindings)
            {
                if (Binding.Id != Identity.toStdString())
                {
                    continue;
                }
                std::string Error;
                const bool Success = Document.SetEventPlacement(Node.toStdString(), Identity.toStdString(),
                                                                static_cast<StoryEventPhase>(Phase), Binding.Join,
                                                                Error, Batch.toStdString(), Before.toStdString());
                bForceInspector = true;
                Finish(Success, Error);
                return;
            }
        }
    };
    Inspector.ImportEventRequested = [this]() { ImportEvent(); };
    View.ImportEventRequested = [this](QString Node, int Phase) {
        if (!bLocked)
        {
            Select(Node);
            ImportEvent(Phase);
        }
    };
    View.RemoveEventRequested = [this](QString Node, QString Binding) {
        if (bLocked)
        {
            return;
        }
        FlushEdits();
        std::string Error;
        const bool Success = Document.RemoveEventBinding(Node.toStdString(), Binding.toStdString(), Error);
        if (Success)
        {
            SelectedBinding.clear();
            bForceInspector = true;
        }
        Finish(Success, Error);
    };
    View.EventModeRequested = [this](QString Node, QString Batch, bool Parallel) {
        if (bLocked)
        {
            return;
        }
        FlushEdits();
        std::string Error;
        const bool Success = Document.SetEventBatchMode(Node.toStdString(), Batch.toStdString(), Parallel, Error);
        bForceInspector = true;
        Finish(Success, Error);
    };
    auto& Events = Inspector.EventInspector();
    Events.EventChanged = [this](StoryGraphEvent Event) {
        if (bLocked || bForceInspector)
        {
            return;
        }
        std::string Error;
        const bool Success = Document.ReplaceEvent(StoryEventAdapter::ToDefinition(Event), Error);
        Finish(Success, Error);
    };
    Events.ActionChanged = [this](StoryGraphAction Action) {
        if (bLocked || bForceInspector)
        {
            return;
        }
        const auto* Node = Document.FindNode(Selected.toStdString());
        if (!Node)
        {
            return;
        }
        for (const auto& Batch : Node->EventBatches)
        {
            for (const auto& Binding : Batch.Bindings)
            {
                if (Binding.Id != SelectedBinding.toStdString())
                {
                    continue;
                }
                auto Overrides = Binding.ActionOverrides;
                Overrides.erase(
                    std::remove_if(Overrides.begin(), Overrides.end(),
                                   [&](const auto& Item) { return Item.Id == Action.Identity.toStdString(); }),
                    Overrides.end());
                Overrides.push_back(StoryEventAdapter::ToDefinition(Action));
                std::string Error;
                const bool Success = Document.SetEventOverrides(Node->Id, Binding.Id, Overrides, Error);
                Finish(Success, Error);
                return;
            }
        }
    };
    Events.PlacementChanged = [this](int Phase, int Join) {
        if (bLocked || bForceInspector)
        {
            return;
        }
        std::string Error;
        const bool Success =
            Document.SetEventPlacement(Selected.toStdString(), SelectedBinding.toStdString(),
                                       static_cast<StoryEventPhase>(Phase), static_cast<StoryEventJoin>(Join), Error);
        bForceInspector = true;
        Finish(Success, Error);
    };
    Events.RemoveRequested = [this]() { View.RemoveEventRequested(Selected, SelectedBinding); };
    Events.BackRequested = [this]() { Select(Selected); };
    Events.ResetRequested = [this]() {
        if (bLocked)
        {
            return;
        }
        std::string Error;
        const bool Success =
            Document.SetEventOverrides(Selected.toStdString(), SelectedBinding.toStdString(), {}, Error);
        bForceInspector = true;
        Finish(Success, Error);
    };
    Events.MoveRequested = [this](int Offset) {
        if (bLocked)
        {
            return;
        }
        FlushEdits();
        std::string Error;
        const bool Success =
            Document.MoveEventBinding(Selected.toStdString(), SelectedBinding.toStdString(), Offset, Error);
        bForceInspector = true;
        Finish(Success, Error);
    };
    Events.UndoRequested = View.UndoRequested;
    Events.RedoRequested = View.RedoRequested;
    Events.SaveRequested = View.SaveRequested;
    Events.ExportRequested = [this]() { ExportSelectedEvent(); };
    Events.ImportRequested = [this]() { ImportEvent(); };
}
void StoryGraphController::ClearEventCallbacks()
{
    View.AddEventRequested = {};
    Inspector.AddEventRequested = {};
    View.EventSelected = {};
    Inspector.EventSelected = {};
    View.RemoveEventRequested = {};
    View.EventModeRequested = {};
    View.ImportEventRequested = {};
    Inspector.ImportEventRequested = {};
    View.MoveEventRequested = {};
    auto& Events = Inspector.EventInspector();
    Events.EventChanged = {};
    Events.ActionChanged = {};
    Events.PlacementChanged = {};
    Events.RemoveRequested = {};
    Events.ResetRequested = {};
    Events.BackRequested = {};
    Events.MoveRequested = {};
    Events.UndoRequested = {};
    Events.RedoRequested = {};
    Events.SaveRequested = {};
    Events.ExportRequested = {};
    Events.ImportRequested = {};
}
bool StoryGraphController::ShowEventInspector(const StoryGraphNode& Node, const StoryGraphModel& Model)
{
    if (SelectedBinding.isEmpty())
    {
        return false;
    }
    for (const auto& Batch : Node.EventBatches)
    {
        for (const auto& Binding : Batch.Bindings)
        {
            if (Binding.Identity != SelectedBinding)
            {
                continue;
            }
            for (const auto& Event : Model.Events)
            {
                if (Event.Identity == Binding.EventId)
                {
                    Inspector.SetEvent(Node, Batch, Binding, Event, Model, bForceInspector);
                    return true;
                }
            }
        }
    }
    SelectedBinding.clear();
    return false;
}
} // namespace EditorUi
