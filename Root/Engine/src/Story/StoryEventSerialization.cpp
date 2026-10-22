#include "StoryJson.h"
#include "Story/StoryEvents.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace
{
using Json = nlohmann::json;
const char* Phases[] = {"BEFORE", "ON_START", "AFTER"};
const char* Joins[] = {"NONE", "STARTED", "FLOW_END", "EVENT_END"};
std::string Identity(const Json& Value)
{
    auto Result = Value.at("id").get<std::string>();
    if (Result.empty())
    {
        throw std::runtime_error("Event identities must not be empty");
    }
    return Result;
}
bool ValidAction(const StoryActionDefinition& Action)
{
    return static_cast<int>(Action.Kind) >= 0 && static_cast<int>(Action.Kind) <= 3 &&
           std::isfinite(Action.DurationSeconds) && Action.DurationSeconds >= 0 &&
           std::isfinite(Action.TargetPosition.x) && std::isfinite(Action.TargetPosition.y) &&
           std::isfinite(Action.TargetPosition.z) && std::isfinite(Action.TargetRotation.x) &&
           std::isfinite(Action.TargetRotation.y) && std::isfinite(Action.TargetRotation.z) &&
           std::isfinite(Action.TargetRotation.w);
}
} // namespace

const StoryEventDefinition* FindStoryEvent(const StoryDocument& Document, const std::string& Identity)
{
    for (const auto& Event : Document.Events)
    {
        if (Event.Id == Identity)
        {
            return &Event;
        }
    }
    return nullptr;
}

StoryEventDefinition ResolveStoryEvent(const StoryEventDefinition& Event, const StoryEventBinding& Binding)
{
    auto Result = Event;
    for (auto& Group : Result.Groups)
    {
        for (auto& Action : Group.Actions)
        {
            for (const auto& Override : Binding.ActionOverrides)
            {
                if (Override.Id == Action.Id)
                {
                    Action = Override;
                    break;
                }
            }
        }
    }
    return Result;
}

bool ValidateStoryEvents(const StoryDocument& Document, std::string& Error)
{
    auto Fail = [&](const std::string& Text) {
        Error = Text;
        return false;
    };
    std::set<std::string> EventIds;
    for (const auto& Event : Document.Events)
    {
        if (Event.Id.empty() || !EventIds.insert(Event.Id).second)
        {
            return Fail("Duplicate or empty event ID");
        }
        std::set<std::string> Groups, Actions;
        for (const auto& Group : Event.Groups)
        {
            if (Group.Id.empty() || !Groups.insert(Group.Id).second)
            {
                return Fail("Duplicate or empty event group ID");
            }
            for (const auto& Action : Group.Actions)
            {
                if (Action.Id.empty() || !Actions.insert(Action.Id).second || !ValidAction(Action))
                {
                    return Fail("Invalid event action: " + Event.Id);
                }
            }
        }
    }
    for (const auto& Node : Document.Nodes)
    {
        std::set<std::string> Batches, Bindings;
        if (!Node.EventBatches.empty() && Node.Kind != StoryNodeKind::Line && Node.Kind != StoryNodeKind::Choice)
        {
            return Fail("Event phases are supported on dialogue and choice nodes");
        }
        for (const auto& Batch : Node.EventBatches)
        {
            if (Batch.Id.empty() || !Batches.insert(Batch.Id).second || static_cast<int>(Batch.Phase) < 0 ||
                static_cast<int>(Batch.Phase) > 2)
            {
                return Fail("Invalid event batch: " + Node.Id);
            }
            for (const auto& Binding : Batch.Bindings)
            {
                const auto* Event = FindStoryEvent(Document, Binding.EventId);
                if (Binding.Id.empty() || !Bindings.insert(Binding.Id).second || !Event ||
                    static_cast<int>(Binding.Join) < 0 || static_cast<int>(Binding.Join) > 3)
                {
                    return Fail("Invalid event binding: " + Node.Id);
                }
                std::set<std::string> Overrides;
                for (const auto& Action : Binding.ActionOverrides)
                {
                    bool Found = false;
                    for (const auto& Group : Event->Groups)
                    {
                        for (const auto& Original : Group.Actions)
                        {
                            Found |= Original.Id == Action.Id;
                        }
                    }
                    if (!Found || !Overrides.insert(Action.Id).second || !ValidAction(Action))
                    {
                        return Fail("Invalid placement action override: " + Binding.Id);
                    }
                }
            }
        }
    }
    Error.clear();
    return true;
}

bool StoryJson::ReadEvents(const Json& Root, StoryDocument& Document, std::string& Error)
{
    try
    {
        if (Root.contains("events"))
        {
            if (!Root.at("events").is_array())
            {
                throw std::runtime_error("events must be an array");
            }
            for (const auto& Value : Root.at("events"))
            {
                if (Value.value("retention", "AUTO_CLOSE_ON_FLOW_END") != "AUTO_CLOSE_ON_FLOW_END")
                {
                    throw std::runtime_error("Native finite events require AUTO_CLOSE_ON_FLOW_END retention");
                }
                StoryEventDefinition Event;
                Event.Id = Identity(Value);
                Event.Name = Value.at("name").get<std::string>();
                Event.Description = Value.value("description", std::string{});
                if (!Value.at("groups").is_array())
                {
                    throw std::runtime_error("Event groups must be an array");
                }
                for (const auto& GroupValue : Value.at("groups"))
                {
                    StoryEventGroup Group;
                    Group.Id = Identity(GroupValue);
                    Group.Name = GroupValue.value("name", std::string{});
                    if (!GroupValue.at("actions").is_array())
                    {
                        throw std::runtime_error("Group actions must be an array");
                    }
                    for (const auto& ActionValue : GroupValue.at("actions"))
                    {
                        StoryActionDefinition Action;
                        if (!ReadAction(ActionValue, Action, Error))
                        {
                            return false;
                        }
                        Group.Actions.push_back(std::move(Action));
                    }
                    Event.Groups.push_back(std::move(Group));
                }
                Document.Events.push_back(std::move(Event));
            }
        }
        for (size_t Index = 0; Index < Document.Nodes.size(); ++Index)
        {
            auto& Node = Document.Nodes[Index];
            const auto& Value = Root.at("nodes").at(Index);
            std::map<std::string, StoryEventBinding> Bindings;
            std::map<std::string, int> Hooks;
            if (Value.contains("bindings"))
            {
                if (!Value.at("bindings").is_array())
                {
                    throw std::runtime_error("bindings must be an array");
                }
                for (const auto& Placement : Value.at("bindings"))
                {
                    if (Placement.contains("condition") || Placement.contains("operationContext") ||
                        (Placement.contains("overrides") && !Placement.at("overrides").empty()))
                    {
                        throw std::runtime_error(
                            "This event placement uses unsupported conditions or execution context");
                    }
                    StoryEventBinding Binding;
                    Binding.Id = Identity(Placement);
                    Binding.EventId = Placement.at("eventId").get<std::string>();
                    const auto Join = Placement.value("join", std::string("EVENT_END"));
                    auto It = std::find(std::begin(Joins), std::end(Joins), Join);
                    if (It == std::end(Joins))
                    {
                        throw std::runtime_error("Unknown event join: " + Join);
                    }
                    Binding.Join = static_cast<StoryEventJoin>(It - std::begin(Joins));
                    const auto Hook = Placement.at("hook").get<std::string>();
                    auto PhaseIt = std::find(std::begin(Phases), std::end(Phases), Hook);
                    if (PhaseIt == std::end(Phases))
                    {
                        throw std::runtime_error("Unknown event phase: " + Hook);
                    }
                    Hooks[Binding.Id] = static_cast<int>(PhaseIt - std::begin(Phases));
                    if (Placement.contains("actionOverrides"))
                    {
                        if (!Placement.at("actionOverrides").is_object())
                        {
                            throw std::runtime_error("actionOverrides must be an object");
                        }
                        for (const auto& [Id, ActionValue] : Placement.at("actionOverrides").items())
                        {
                            StoryActionDefinition Action;
                            if (!ReadAction(ActionValue, Action, Error))
                            {
                                return false;
                            }
                            Action.Id = Id;
                            Binding.ActionOverrides.push_back(std::move(Action));
                        }
                    }
                    if (!Bindings.emplace(Binding.Id, std::move(Binding)).second)
                    {
                        throw std::runtime_error("Duplicate binding ID");
                    }
                }
            }
            if (Value.contains("batches"))
            {
                if (!Value.at("batches").is_object())
                {
                    throw std::runtime_error("batches must be an object");
                }
                for (const auto& [Key, Unused] : Value.at("batches").items())
                {
                    if (std::find(std::begin(Phases), std::end(Phases), Key) == std::end(Phases))
                    {
                        throw std::runtime_error("Unknown batch phase");
                    }
                }
                for (int Phase = 0; Phase < 3; ++Phase)
                {
                    if (!Value.at("batches").contains(Phases[Phase]))
                    {
                        continue;
                    }
                    const auto& Groups = Value.at("batches").at(Phases[Phase]);
                    if (!Groups.is_array())
                    {
                        throw std::runtime_error("Phase batches must be an array");
                    }
                    for (const auto& Group : Groups)
                    {
                        StoryEventBatch Batch;
                        Batch.Id = Identity(Group);
                        Batch.Phase = static_cast<StoryEventPhase>(Phase);
                        const auto Mode = Group.value("mode", std::string("SEQUENTIAL"));
                        if (Mode != "SEQUENTIAL" && Mode != "PARALLEL")
                        {
                            throw std::runtime_error("Unknown batch mode");
                        }
                        Batch.bParallel = Mode == "PARALLEL";
                        if (!Group.at("bindingIds").is_array())
                        {
                            throw std::runtime_error("bindingIds must be an array");
                        }
                        for (const auto& IdValue : Group.at("bindingIds"))
                        {
                            const auto Id = IdValue.get<std::string>();
                            if (!Bindings.count(Id) || Hooks.at(Id) != Phase)
                            {
                                throw std::runtime_error("Missing, repeated or misplaced event binding");
                            }
                            Batch.Bindings.push_back(std::move(Bindings.at(Id)));
                            Bindings.erase(Id);
                        }
                        Node.EventBatches.push_back(std::move(Batch));
                    }
                }
            }
            if (!Bindings.empty())
            {
                throw std::runtime_error("Event binding is not assigned to a phase batch");
            }
        }
        return ValidateStoryEvents(Document, Error);
    }
    catch (const std::exception& Exception)
    {
        Error = std::string("Invalid story events: ") + Exception.what();
        return false;
    }
}

void StoryJson::WriteEvents(Json& Root, const StoryDocument& Document)
{
    if (!Document.Events.empty())
    {
        Root["events"] = Json::array();
        for (const auto& Event : Document.Events)
        {
            Json Value = {{"id", Event.Id},
                          {"name", Event.Name},
                          {"description", Event.Description},
                          {"retention", "AUTO_CLOSE_ON_FLOW_END"},
                          {"groups", Json::array()}};
            for (const auto& Group : Event.Groups)
            {
                Json GroupValue = {{"id", Group.Id}, {"name", Group.Name}, {"actions", Json::array()}};
                for (const auto& Action : Group.Actions)
                {
                    GroupValue["actions"].push_back(WriteAction(Action));
                }
                Value["groups"].push_back(std::move(GroupValue));
            }
            Root["events"].push_back(std::move(Value));
        }
    }
    for (size_t Index = 0; Index < Document.Nodes.size(); ++Index)
    {
        const auto& Node = Document.Nodes[Index];
        if (Node.EventBatches.empty())
        {
            continue;
        }
        auto& Value = Root["nodes"][Index];
        Value["bindings"] = Json::array();
        Value["batches"] = Json::object();
        for (const auto& Batch : Node.EventBatches)
        {
            const auto* Phase = Phases[static_cast<int>(Batch.Phase)];
            Json Group = {
                {"id", Batch.Id}, {"mode", Batch.bParallel ? "PARALLEL" : "SEQUENTIAL"}, {"bindingIds", Json::array()}};
            for (const auto& Binding : Batch.Bindings)
            {
                Json Placement = {{"id", Binding.Id},
                                  {"eventId", Binding.EventId},
                                  {"hook", Phase},
                                  {"join", Joins[static_cast<int>(Binding.Join)]}};
                if (!Binding.ActionOverrides.empty())
                {
                    Placement["actionOverrides"] = Json::object();
                    for (const auto& Action : Binding.ActionOverrides)
                    {
                        Placement["actionOverrides"][Action.Id] = WriteAction(Action);
                    }
                }
                Value["bindings"].push_back(std::move(Placement));
                Group["bindingIds"].push_back(Binding.Id);
            }
            if (!Value["batches"].contains(Phase))
            {
                Value["batches"][Phase] = Json::array();
            }
            Value["batches"][Phase].push_back(std::move(Group));
        }
    }
}
