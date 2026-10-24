#include "Documents/StoryGraphDocument.h"
#include "Core/IO/AtomicFileWriter.h"
#include "Story/StoryDocumentIO.h"
#include "Story/StoryEvents.h"
#include <SDL3/SDL_stdinc.h>
#include <algorithm>
#include <set>
#include <regex>

namespace
{
using Json = nlohmann::json;
constexpr const char* Capability = "native.story.events";
Json StoryContainer(const Json& Event)
{
    return {{"format", "sakura.story"}, {"formatVersion", 1},     {"name", "Package"},
            {"startNodeId", ""},        {"nodes", Json::array()}, {"events", Json::array({Event})}};
}
void ReadPackage(const std::string& Text, Json& Package, StoryEventPackagePreview& Preview)
{
    if (Text.size() > 4 * 1024 * 1024)
    {
        throw std::runtime_error("Event package exceeds 4 MiB");
    }
    Package = Json::parse(Text);
    if (Package.at("format") != "sacura-authoring-package" || Package.at("formatVersion") != 1 ||
        Package.at("compatibility").at("projectSchemaVersion") != 1 ||
        Package.at("compatibility").at("requiredCapabilities") != Json::array({{{"id", Capability}, {"version", 1}}}) ||
        Package.at("compatibility").at("requiredEngineModules") != Json::array() ||
        Package.at("roots") != Json{{"kind", "event"}, {"entries", Json::array({"/content/events/0"})}} ||
        Package.at("assets") != Json::array() || !Package.at("content").is_object() ||
        Package.at("content").size() != 1 || !Package.at("content").at("events").is_array() ||
        Package.at("content").at("events").size() != 1)
    {
        throw std::runtime_error("This package requires features outside the native finite event profile");
    }
    const auto& Meta = Package.at("metadata");
    const auto RequireText = [&](const char* Key) {
        auto Value = Meta.at(Key).get<std::string>();
        if (Value.empty())
        {
            throw std::runtime_error(std::string("Missing package metadata: ") + Key);
        }
        return Value;
    };
    RequireText("id");
    if (!std::regex_match(RequireText("version"), std::regex("(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)")) ||
        !Meta.at("sources").is_array())
    {
        throw std::runtime_error("Unsupported event package metadata");
    }
    Preview = {};
    Preview.Metadata = {RequireText("name"), Meta.value("description", std::string{}), RequireText("author"),
                        RequireText("license")};
    std::set<std::string> Inputs, Referenced, Places;
    if (!Package.at("inputs").is_array() || !Package.at("references").is_array())
    {
        throw std::runtime_error("Invalid package inputs or references");
    }
    for (const auto& Input : Package.at("inputs"))
    {
        const auto Id = Input.at("id").get<std::string>();
        if (Id.empty() || !Inputs.insert(Id).second || Input.at("type") != "object" || Input.at("required") != true ||
            Input.contains("default"))
        {
            throw std::runtime_error("Unsupported event package input");
        }
        Preview.Inputs.push_back({Id, Input.at("label").get<std::string>()});
    }
    auto Data = Package.at("content").at("events").at(0);
    const std::set<std::string> EventFields = {"id", "name", "description", "retention", "groups", "sources"};
    for (const auto& [Key, Value] : Data.items())
    {
        if (!EventFields.count(Key))
        {
            throw std::runtime_error("Unsupported event dependency field: " + Key);
        }
    }
    std::set<std::string> Allowed;
    size_t GroupIndex = 0;
    for (const auto& Group : Data.at("groups"))
    {
        for (const auto& [Key, Value] : Group.items())
        {
            if (Key != "id" && Key != "name" && Key != "actions")
            {
                throw std::runtime_error("Unsupported event group dependency");
            }
        }
        size_t ActionIndex = 0;
        for (const auto& Action : Group.at("actions"))
        {
            const std::set<std::string> Fields = {"id",       "kind",     "durationSeconds",         "targetObjectId",
                                                  "position", "rotation", "useTargetObjectTransform"};
            for (const auto& [Key, Value] : Action.items())
            {
                if (!Fields.count(Key))
                {
                    throw std::runtime_error("Unsupported action dependency: " + Key);
                }
            }
            const auto Kind = Action.at("kind").get<std::string>();
            const bool NeedsTarget = Kind == "MoveTo" || Kind == "NavigateTo" ||
                                     (Kind == "CameraCut" && Action.value("useTargetObjectTransform", false));
            const std::string At = "/content/events/0/groups/" + std::to_string(GroupIndex) + "/actions/" +
                                   std::to_string(ActionIndex) + "/targetObjectId";
            if (NeedsTarget)
            {
                if (!Action.contains("targetObjectId") || !Action.at("targetObjectId").is_null())
                {
                    throw std::runtime_error("Scene object must be an external package input");
                }
                Allowed.insert(At);
            }
            else if (Action.contains("targetObjectId"))
            {
                throw std::runtime_error("Unexpected scene reference");
            }
            ++ActionIndex;
            ++Preview.Actions;
        }
        ++GroupIndex;
        ++Preview.Groups;
    }
    for (const auto& Reference : Package.at("references"))
    {
        const auto At = Reference.at("at").get<std::string>();
        const auto Input = Reference.at("inputId").get<std::string>();
        if (!Allowed.count(At) || !Places.insert(At).second || !Inputs.count(Input))
        {
            throw std::runtime_error("Invalid or repeated event package reference");
        }
        Referenced.insert(Input);
        Package[Json::json_pointer(At)] = "preview-target";
    }
    if (Places != Allowed || Referenced != Inputs)
    {
        throw std::runtime_error("Incomplete event package dependencies");
    }
    StoryDocument Document;
    const auto Result =
        StoryDocumentIO::LoadFromJson(StoryContainer(Package.at("content").at("events")[0]).dump(), Document);
    if (!Result.bOk)
    {
        throw std::runtime_error(Result.Error);
    }
    // Keep the transport copy, not the preview placeholder, for revalidation/apply.
    Package = Json::parse(Text);
}
} // namespace
bool PreviewStoryEventPackage(const std::string& Text, StoryEventPackagePreview& Preview, std::string& Error)
{
    try
    {
        Json Package;
        ReadPackage(Text, Package, Preview);
        Error.clear();
        return true;
    }
    catch (const std::exception& Exception)
    {
        Error = Exception.what();
        Preview = {};
        return false;
    }
}

bool StoryGraphDocument::ExportEventPackage(const std::string& EventId,
                                            const std::vector<StoryActionDefinition>& Overrides,
                                            const StoryEventPackageMetadata& Metadata,
                                            const std::map<std::string, std::string>& ObjectNames, std::string& Text,
                                            std::string& Error) const
{
    try
    {
        const auto* Source = FindStoryEvent(Story, EventId);
        if (!Source || !IsOpen())
        {
            throw std::runtime_error("Event no longer exists");
        }
        StoryEventBinding Placement;
        Placement.ActionOverrides = Overrides;
        StoryDocument Container;
        Container.Events = {ResolveStoryEvent(*Source, Placement)};
        std::string Encoded;
        StoryDocumentIO::SaveToJson(Container, Encoded);
        auto Event = Json::parse(Encoded).at("events")[0];
        Json Sources = Json::array();
        if (Data().contains("events"))
        {
            for (const auto& Raw : Data().at("events"))
            {
                if (Raw.at("id") == EventId)
                {
                    Sources = Raw.value("sources", Json::array());
                    const std::set<std::string> EventFields = {"id",        "name",   "description",
                                                               "retention", "groups", "sources"};
                    const std::set<std::string> ActionFields = {
                        "id",       "kind",     "durationSeconds",         "targetObjectId", "targetObjectName",
                        "position", "rotation", "useTargetObjectTransform"};
                    for (const auto& [Key, Value] : Raw.items())
                    {
                        if (!EventFields.count(Key))
                        {
                            throw std::runtime_error("Export cannot discard unsupported event data: " + Key);
                        }
                    }
                    for (const auto& Group : Raw.at("groups"))
                    {
                        for (const auto& [Key, Value] : Group.items())
                        {
                            if (Key != "id" && Key != "name" && Key != "actions")
                            {
                                throw std::runtime_error("Export cannot discard unsupported event group data: " + Key);
                            }
                        }
                        for (const auto& Action : Group.at("actions"))
                        {
                            for (const auto& [Key, Value] : Action.items())
                            {
                                if (!ActionFields.count(Key))
                                {
                                    throw std::runtime_error("Export cannot discard unsupported action data: " + Key);
                                }
                            }
                        }
                    }
                }
            }
        }
        Json Package = {{"format", "sacura-authoring-package"},
                        {"formatVersion", 1},
                        {"metadata",
                         {{"id", EventId},
                          {"version", "1.0.0"},
                          {"name", Metadata.Name},
                          {"description", Metadata.Description},
                          {"author", Metadata.Author},
                          {"license", Metadata.License},
                          {"sources", Sources}}},
                        {"compatibility",
                         {{"projectSchemaVersion", 1},
                          {"requiredCapabilities", Json::array({{{"id", Capability}, {"version", 1}}})},
                          {"requiredEngineModules", Json::array()}}},
                        {"roots", {{"kind", "event"}, {"entries", Json::array({"/content/events/0"})}}},
                        {"inputs", Json::array()},
                        {"references", Json::array()},
                        {"assets", Json::array()},
                        {"content", {{"events", Json::array({Event})}}}};
        std::map<std::string, std::string> Inputs;
        for (size_t Group = 0; Group < Event.at("groups").size(); ++Group)
        {
            for (size_t Index = 0; Index < Event["groups"][Group]["actions"].size(); ++Index)
            {
                auto& Action = Package["content"]["events"][0]["groups"][Group]["actions"][Index];
                if (Action.contains("targetObjectName"))
                {
                    throw std::runtime_error("Choose a scene object before exporting a legacy target");
                }
                const auto Kind = Action.at("kind").get<std::string>();
                const bool Needed = Kind == "MoveTo" || Kind == "NavigateTo" ||
                                    (Kind == "CameraCut" && Action.value("useTargetObjectTransform", false));
                if (!Needed)
                {
                    Action.erase("targetObjectId");
                    continue;
                }
                const auto Target = Action.value("targetObjectId", std::string{});
                if (Target.empty() || !ObjectNames.count(Target))
                {
                    throw std::runtime_error("Choose an available object for every event action before exporting");
                }
                auto& InputId = Inputs[Target];
                if (InputId.empty())
                {
                    InputId = "object" + std::to_string(Inputs.size());
                    Package["inputs"].push_back({{"id", InputId},
                                                 {"type", "object"},
                                                 {"label", ObjectNames.at(Target)},
                                                 {"description", "Объект действия"},
                                                 {"required", true}});
                }
                Action["targetObjectId"] = nullptr;
                Package["references"].push_back({{"at", "/content/events/0/groups/" + std::to_string(Group) +
                                                            "/actions/" + std::to_string(Index) + "/targetObjectId"},
                                                 {"inputId", InputId}});
            }
        }
        const auto Result = Package.dump(2);
        StoryEventPackagePreview Preview;
        if (!PreviewStoryEventPackage(Result, Preview, Error))
        {
            return false;
        }
        Text = Result;
        return true;
    }
    catch (const std::exception& Exception)
    {
        Error = Exception.what();
        return false;
    }
}

bool StoryGraphDocument::ImportEventPackage(const std::string& Text, const std::map<std::string, std::string>& Bindings,
                                            const std::vector<std::string>& AvailableObjects, const StoryNodeId& NodeId,
                                            StoryEventPhase Phase, std::string& CreatedBinding, std::string& Error)
{
    try
    {
        Json Package;
        StoryEventPackagePreview Preview;
        ReadPackage(Text, Package, Preview);
        if (Bindings.size() != Preview.Inputs.size())
        {
            throw std::runtime_error("Choose every required object binding");
        }
        for (const auto& Input : Preview.Inputs)
        {
            const auto It = Bindings.find(Input.Id);
            if (It == Bindings.end() || It->second.empty() ||
                std::find(AvailableObjects.begin(), AvailableObjects.end(), It->second) == AvailableObjects.end())
            {
                throw std::runtime_error("Choose an available object for: " + Input.Label);
            }
        }
        for (const auto& Reference : Package.at("references"))
        {
            Package[Json::json_pointer(Reference.at("at").get<std::string>())] =
                Bindings.at(Reference.at("inputId").get<std::string>());
        }
        auto Event = Package.at("content").at("events")[0];
        Event["id"] = Guid::Generate().ToString();
        for (auto& Group : Event["groups"])
        {
            Group["id"] = Guid::Generate().ToString();
            for (auto& Action : Group["actions"])
            {
                Action["id"] = Guid::Generate().ToString();
            }
        }
        Event["name"] = Preview.Metadata.Name;
        auto Sources = Package.at("metadata").at("sources");
        auto Origin = Package.at("metadata");
        Origin.erase("sources");
        Sources.push_back(Origin);
        Event["sources"] = Sources;
        const std::string Name = Preview.Metadata.Name;
        std::string Unique = Name;
        auto Exists = [&](const std::string& Candidate) {
            for (const auto& Item : Story.Events)
            {
                if (SDL_strcasecmp(Item.Name.c_str(), Candidate.c_str()) == 0)
                {
                    return true;
                }
            }
            return false;
        };
        int Suffix = 2;
        while (Exists(Unique))
        {
            Unique = Name + " (" + std::to_string(Suffix++) + ")";
        }
        Event["name"] = Unique;
        const auto PlacementId = Guid::Generate().ToString();
        const bool Applied = Apply(
            "Import event package",
            [&](Json& Candidate, std::string& Failure) {
                Json* Node = nullptr;
                for (auto& Item : Candidate["nodes"])
                {
                    if (Item.at("id") == NodeId)
                    {
                        Node = &Item;
                        break;
                    }
                }
                if (!Node || (Node->at("kind") != "Line" && Node->at("kind") != "Choice") ||
                    static_cast<int>(Phase) < 0 || static_cast<int>(Phase) > 2)
                {
                    Failure = "Choose a dialogue or choice node for the imported event";
                    return false;
                }
                if (!Candidate.contains("events"))
                {
                    Candidate["events"] = Json::array();
                }
                Candidate["events"].push_back(Event);
                const char* Phases[] = {"BEFORE", "ON_START", "AFTER"};
                const auto Hook = Phases[static_cast<int>(Phase)];
                if (!Node->contains("bindings"))
                {
                    (*Node)["bindings"] = Json::array();
                }
                (*Node)["bindings"].push_back(
                    {{"id", PlacementId}, {"eventId", Event.at("id")}, {"hook", Hook}, {"join", "EVENT_END"}});
                if (!Node->contains("batches"))
                {
                    (*Node)["batches"] = Json::object();
                }
                if (!(*Node)["batches"].contains(Hook))
                {
                    (*Node)["batches"][Hook] = Json::array();
                }
                (*Node)["batches"][Hook].push_back({{"id", Guid::Generate().ToString()},
                                                    {"mode", "SEQUENTIAL"},
                                                    {"bindingIds", Json::array({PlacementId})}});
                return true;
            },
            Error);
        if (Applied)
        {
            CreatedBinding = PlacementId;
        }
        return Applied;
    }
    catch (const std::exception& Exception)
    {
        Error = Exception.what();
        return false;
    }
}
