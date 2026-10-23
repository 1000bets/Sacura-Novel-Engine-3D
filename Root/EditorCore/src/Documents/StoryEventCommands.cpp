#include "Documents/StoryGraphDocument.h"
#include "StoryDocumentSchema.h"
#include "Story/StoryDocumentIO.h"
#include "Story/StoryEvents.h"
#include <algorithm>
#include <set>

namespace
{
using Json = nlohmann::json;
StoryNode* NodeIn(StoryDocument& Story, const std::string& Identity)
{
    for (auto& Node : Story.Nodes)
    {
        if (Node.Id == Identity)
        {
            return &Node;
        }
    }
    return nullptr;
}
} // namespace
namespace StoryDocumentSchema
{
using Json = nlohmann::json;
void Merge(Json& Original, const Json& Before, const Json& After)
{
    if (Before == After)
    {
        return;
    }
    if (Before.is_object() && After.is_object() && Original.is_object())
    {
        for (const auto& [Key, Value] : Before.items())
        {
            if (!After.contains(Key))
            {
                Original.erase(Key);
            }
        }
        for (const auto& [Key, Value] : After.items())
        {
            if (Before.contains(Key) && Original.contains(Key))
            {
                Merge(Original[Key], Before.at(Key), Value);
            }
            else
            {
                Original[Key] = Value;
            }
        }
    }
    else if (Before.is_array() && After.is_array() && Original.is_array())
    {
        Json Result = Json::array();
        for (const auto& Value : After)
        {
            Json Preserved = Value;
            if (Value.is_object() && Value.contains("id"))
            {
                const auto Same = [&](const Json& Item) {
                    return Item.is_object() && Item.value("id", Json{}) == Value.at("id");
                };
                const auto Old = std::find_if(Before.begin(), Before.end(), Same);
                const auto Raw = std::find_if(Original.begin(), Original.end(), Same);
                if (Old != Before.end() && Raw != Original.end())
                {
                    Preserved = *Raw;
                    Merge(Preserved, *Old, Value);
                }
            }
            Result.push_back(std::move(Preserved));
        }
        Original = std::move(Result);
    }
    else
    {
        Original = After;
    }
}
} // namespace StoryDocumentSchema
namespace
{
std::string NewId()
{
    return Guid::Generate().ToString();
}
} // namespace

bool StoryGraphDocument::EditStaging(const char* Label, const std::function<bool(StoryDocument&, std::string&)>& Edit,
                                     std::string& Error)
{
    return Apply(
        Label,
        [&](Json& Candidate, std::string& Failure) {
            StoryDocument Draft;
            if (!Decode(Candidate, Draft, Failure))
            {
                return false;
            }
            std::string Before, After;
            StoryDocumentIO::SaveToJson(Draft, Before);
            if (!Edit(Draft, Failure) || !ValidateStoryEvents(Draft, Failure))
            {
                return false;
            }
            StoryDocumentIO::SaveToJson(Draft, After);
            StoryDocumentSchema::Merge(Candidate, Json::parse(Before), Json::parse(After));
            return true;
        },
        Error);
}

bool StoryGraphDocument::AddEvent(const StoryNodeId& Node, StoryEventPhase Phase, const std::string& SourceEvent,
                                  std::string& Binding, std::string& Error, const std::string& BatchId, StoryActionKind InitialKind)
{
    const auto Created = NewId();
    const bool Applied = EditStaging(
        "Add event",
        [&](StoryDocument& Draft, std::string& Failure) {
            auto* Target = NodeIn(Draft, Node);
            if (!Target)
            {
                Failure = "Story node no longer exists";
                return false;
            }
            std::string EventId = SourceEvent;
            if (EventId.empty())
            {
                StoryEventDefinition Event;
                Event.Id = EventId = NewId();
                Event.Name = "Новое событие";
                StoryActionDefinition Action;
                Action.Kind = InitialKind;
                if (InitialKind == StoryActionKind::MoveTo) Event.Name = "Перемещение объекта";
                if (InitialKind == StoryActionKind::CameraCut) Event.Name = "Переход камеры";
                if (InitialKind == StoryActionKind::NavigateTo) Event.Name = "Движение по навигации";
                Action.Id = NewId();
                Action.DurationSeconds = 1;
                Event.Groups.push_back({NewId(), "Основное действие", {Action}});
                Draft.Events.push_back(std::move(Event));
            }
            else if (!FindStoryEvent(Draft, EventId))
            {
                Failure = "Source event no longer exists";
                return false;
            }
            StoryEventBatch* Batch = nullptr;
            for (auto& Item : Target->EventBatches)
            {
                if (!BatchId.empty() && Item.Id == BatchId && Item.Phase == Phase)
                {
                    Batch = &Item;
                    break;
                }
            }
            if (!Batch && !BatchId.empty())
            {
                Failure = "Event batch no longer exists";
                return false;
            }
            if (!Batch)
            {
                Target->EventBatches.push_back({NewId(), Phase});
                Batch = &Target->EventBatches.back();
            }
            Batch->Bindings.push_back({Created, EventId});
            return true;
        },
        Error);
    if (Applied)
    {
        Binding = Created;
    }
    return Applied;
}

bool StoryGraphDocument::RemoveEventBinding(const StoryNodeId& Node, const std::string& Binding, std::string& Error)
{
    return EditStaging(
        "Remove event placement",
        [&](StoryDocument& Draft, std::string& Failure) {
            auto* Target = NodeIn(Draft, Node);
            if (!Target)
            {
                Failure = "Story node no longer exists";
                return false;
            }
            bool Removed = false;
            for (auto& Batch : Target->EventBatches)
            {
                auto& Bindings = Batch.Bindings;
                const auto Size = Bindings.size();
                Bindings.erase(std::remove_if(Bindings.begin(), Bindings.end(),
                                              [&](const auto& Item) { return Item.Id == Binding; }),
                               Bindings.end());
                Removed |= Size != Bindings.size();
            }
            auto& Batches = Target->EventBatches;
            Batches.erase(
                std::remove_if(Batches.begin(), Batches.end(), [](const auto& Item) { return Item.Bindings.empty(); }),
                Batches.end());
            if (!Removed)
            {
                Failure = "Event placement no longer exists";
            }
            return Removed;
        },
        Error);
}

bool StoryGraphDocument::SetEventPlacement(const StoryNodeId& Node, const std::string& Binding, StoryEventPhase Phase,
                                           StoryEventJoin Join, std::string& Error, const std::string& BatchId,
                                           const std::string& BeforeBinding)
{
    return EditStaging(
        "Edit event placement",
        [&](StoryDocument& Draft, std::string& Failure) {
            auto* Target = NodeIn(Draft, Node);
            if (Target)
            {
                for (auto& Batch : Target->EventBatches)
                {
                    for (auto Iterator = Batch.Bindings.begin(); Iterator != Batch.Bindings.end(); ++Iterator)
                    {
                        if (Iterator->Id != Binding)
                        {
                            continue;
                        }
                        Iterator->Join = Join;
                        if (BeforeBinding == Binding)
                        {
                            return true;
                        }
                        if (Batch.Phase != Phase || !BatchId.empty())
                        {
                            auto Placement = *Iterator;
                            Batch.Bindings.erase(Iterator);
                            StoryEventBatch* Destination = nullptr;
                            if (!BatchId.empty())
                            {
                                for (auto& Item : Target->EventBatches)
                                {
                                    if (Item.Id == BatchId && Item.Phase == Phase)
                                    {
                                        Destination = &Item;
                                        break;
                                    }
                                }
                                if (!Destination)
                                {
                                    Failure = "Event batch no longer exists";
                                    return false;
                                }
                            }
                            if (!Destination)
                            {
                                Target->EventBatches.push_back({NewId(), Phase});
                                Destination = &Target->EventBatches.back();
                            }
                            auto Position = Destination->Bindings.end();
                            if (!BeforeBinding.empty())
                            {
                                Position = std::find_if(Destination->Bindings.begin(), Destination->Bindings.end(),
                                                        [&](const auto& Item) { return Item.Id == BeforeBinding; });
                                if (Position == Destination->Bindings.end())
                                {
                                    Failure = "Event insertion point no longer exists";
                                    return false;
                                }
                            }
                            Destination->Bindings.insert(Position, std::move(Placement));
                            auto& Batches = Target->EventBatches;
                            Batches.erase(std::remove_if(Batches.begin(), Batches.end(),
                                                         [](const auto& Item) { return Item.Bindings.empty(); }),
                                          Batches.end());
                        }
                        return true;
                    }
                }
            }
            Failure = "Event placement no longer exists";
            return false;
        },
        Error);
}

bool StoryGraphDocument::SetEventBatchMode(const StoryNodeId& Node, const std::string& BatchId, bool bParallel,
                                           std::string& Error)
{
    return EditStaging(
        "Set event order",
        [&](StoryDocument& Draft, std::string& Failure) {
            if (auto* Target = NodeIn(Draft, Node))
            {
                for (auto& Batch : Target->EventBatches)
                {
                    if (Batch.Id == BatchId)
                    {
                        Batch.bParallel = bParallel;
                        return true;
                    }
                }
            }
            Failure = "Event batch no longer exists";
            return false;
        },
        Error);
}

bool StoryGraphDocument::MoveEventBinding(const StoryNodeId& Node, const std::string& Binding, int Offset,
                                          std::string& Error)
{
    return EditStaging(
        "Move event",
        [&](StoryDocument& Draft, std::string& Failure) {
            if (auto* Target = NodeIn(Draft, Node))
            {
                for (auto& Batch : Target->EventBatches)
                {
                    auto& Bindings = Batch.Bindings;
                    for (size_t Index = 0; Index < Bindings.size(); ++Index)
                    {
                        if (Bindings[Index].Id != Binding)
                        {
                            continue;
                        }
                        const auto Next = static_cast<int64_t>(Index) + Offset;
                        if (Next < 0 || Next >= static_cast<int64_t>(Bindings.size()))
                        {
                            return true;
                        }
                        std::swap(Bindings[Index], Bindings[static_cast<size_t>(Next)]);
                        return true;
                    }
                }
            }
            Failure = "Event placement no longer exists";
            return false;
        },
        Error);
}

bool StoryGraphDocument::ReplaceEvent(StoryEventDefinition Event, std::string& Error)
{
    for (auto& Group : Event.Groups)
    {
        if (Group.Id.empty())
        {
            Group.Id = NewId();
        }
        for (auto& Action : Group.Actions)
        {
            if (Action.Id.empty())
            {
                Action.Id = NewId();
            }
        }
    }
    return EditStaging(
        "Edit event",
        [&](StoryDocument& Draft, std::string& Failure) {
            for (auto& Existing : Draft.Events)
            {
                if (Existing.Id != Event.Id)
                {
                    continue;
                }
                Existing = Event;
                std::set<std::string> Actions;
                for (const auto& Group : Event.Groups)
                {
                    for (const auto& Action : Group.Actions)
                    {
                        Actions.insert(Action.Id);
                    }
                }
                for (auto& Node : Draft.Nodes)
                {
                    for (auto& Batch : Node.EventBatches)
                    {
                        for (auto& Binding : Batch.Bindings)
                        {
                            if (Binding.EventId == Event.Id)
                            {
                                auto& Overrides = Binding.ActionOverrides;
                                Overrides.erase(
                                    std::remove_if(Overrides.begin(), Overrides.end(),
                                                   [&](const auto& Action) { return !Actions.count(Action.Id); }),
                                    Overrides.end());
                            }
                        }
                    }
                }
                return true;
            }
            Failure = "Event no longer exists";
            return false;
        },
        Error);
}

bool StoryGraphDocument::SetEventOverrides(const StoryNodeId& Node, const std::string& Binding,
                                           const std::vector<StoryActionDefinition>& Actions, std::string& Error)
{
    return EditStaging(
        "Edit event placement actions",
        [&](StoryDocument& Draft, std::string& Failure) {
            if (auto* Target = NodeIn(Draft, Node))
            {
                for (auto& Batch : Target->EventBatches)
                {
                    for (auto& Placement : Batch.Bindings)
                    {
                        if (Placement.Id == Binding)
                        {
                            Placement.ActionOverrides = Actions;
                            return true;
                        }
                    }
                }
            }
            Failure = "Event placement no longer exists";
            return false;
        },
        Error);
}
