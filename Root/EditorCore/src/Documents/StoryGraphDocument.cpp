#include "Documents/StoryGraphDocument.h"
#include "Story/Narrative.h"
#include "Reflection/ReflectionValueCodec.h"
#include "Reflection/Json/ReflectionJson.h"
#include "Reflection/PropertyAccess.h"
#include "Core/IO/AtomicFileWriter.h"
#include "Story/StoryDocumentIO.h"
#include "Story/StoryEvents.h"
#include "StoryDocumentSchema.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>

namespace
{
using Json = nlohmann::json;
Json* FindNodeJson(Json& Data, const StoryNodeId& Identity)
{
    for (auto& Node : Data["nodes"])
    {
        if (Node.at("id") == Identity)
        {
            return &Node;
        }
    }
    return nullptr;
}

Json EncodeNode(const StoryNode& Node, const std::vector<StoryEventDefinition>& Events = {})
{
    StoryDocument Container;
    Container.StartNodeId = Node.Id;
    Container.Nodes = {Node};
    Container.Events = Events;
    std::string Text;
    StoryDocumentIO::SaveToJson(Container, Text);
    return Json::parse(Text).at("nodes").at(0);
}

// Preserve extension fields while replacing the runtime-owned schema fields.
void MergeNode(Json& Original, const Json& Replacement, const Json& Before)
{
    for (const char* Array : {"choices", "actions"})
    {
        if (!Replacement.contains(Array))
        {
            Original.erase(Array);
            continue;
        }
        const auto Previous = Original.value(Array, Json::array());
        auto Updated = Replacement.at(Array);
        for (size_t Index = 0; Index < Updated.size(); ++Index)
        {
            Json Preserved = Json::object();
            const std::string Identity = Updated.at(Index).value("id", std::string{});
            for (size_t PreviousIndex = 0; PreviousIndex < Previous.size(); ++PreviousIndex)
            {
                const auto& Item = Previous.at(PreviousIndex);
                const auto PreviousIdentity = Item.value("id", "legacy:" + Original.at("id").get<std::string>() + ":" +
                                                                   Array + ":" + std::to_string(PreviousIndex));
                if (PreviousIdentity == Identity)
                {
                    Preserved = Item;
                    break;
                }
            }
            if (std::string(Array) == "choices")
            {
                Preserved.erase("label");
                Preserved.erase("nextNodeId");
            }
            else
            {
                for (const char* Field : {"kind", "durationSeconds", "targetObjectId", "targetObjectName", "position",
                                          "rotation", "useTargetObjectTransform"})
                {
                    Preserved.erase(Field);
                }
            }
            Preserved.update(Updated.at(Index));
            Updated.at(Index) = std::move(Preserved);
        }
        Original[Array] = std::move(Updated);
    }
    for (const char* Field : {"bindings", "batches"})
    {
        if (Before.value(Field, Json{}) == Replacement.value(Field, Json{})) { continue; }
        if (!Replacement.contains(Field)) { Original.erase(Field); }
        else if (Original.contains(Field) && Before.contains(Field))
        { StoryDocumentSchema::Merge(Original[Field], Before.at(Field), Replacement.at(Field)); }
        else { Original[Field] = Replacement.at(Field); }
    }
    for (const char* Field : {"id", "kind", "speaker", "text", "prompt", "nextNodeId", "narrativeType", "inputs", "sources"})
    {
        Original.erase(Field);
        if (Replacement.contains(Field))
        {
            Original[Field] = Replacement.at(Field);
        }
    }
}
} // namespace

bool StoryGraphDocument::Decode(const Json& Data, StoryDocument& Result, std::string& Error)
{
    const auto Loaded = StoryDocumentIO::LoadFromJson(Data.dump(), Result);
    Error = Loaded.Error;
    if (!Loaded.bOk)
    {
        return false;
    }
    for (auto& Node : Result.Nodes)
    {
        for (size_t Index = 0; Index < Node.Choices.size(); ++Index)
        {
            if (Node.Choices[Index].Id.empty())
            {
                Node.Choices[Index].Id = "legacy:" + Node.Id + ":choices:" + std::to_string(Index);
            }
        }
        for (size_t Index = 0; Index < Node.Actions.size(); ++Index)
        {
            if (Node.Actions[Index].Id.empty())
            {
                Node.Actions[Index].Id = "legacy:" + Node.Id + ":actions:" + std::to_string(Index);
            }
        }
        if (Node.Id.empty())
        {
            Error = "Story nodes require a nonempty identity";
            return false;
        }
        for (const auto& Action : Node.Actions)
        {
            if (!std::isfinite(Action.DurationSeconds) || Action.DurationSeconds < 0 ||
                !std::isfinite(Action.TargetPosition.x) || !std::isfinite(Action.TargetPosition.y) ||
                !std::isfinite(Action.TargetPosition.z) || !std::isfinite(Action.TargetRotation.x) ||
                !std::isfinite(Action.TargetRotation.y) || !std::isfinite(Action.TargetRotation.z) ||
                !std::isfinite(Action.TargetRotation.w))
            {
                Error = "Story action values must be finite and duration must be nonnegative";
                return false;
            }
        }
    }
    if (Data.contains("editor"))
    {
        const auto& Editor = Data.at("editor");
        if (!Editor.is_object() || Editor.value("version", 1) != 1)
        {
            Error = "Unsupported story editor metadata";
            return false;
        }
        if (Editor.contains("positions"))
        {
            if (!Editor.at("positions").is_object())
            {
                Error = "Story graph positions must be an object";
                return false;
            }
            for (const auto& Position : Editor.at("positions"))
            {
                if (!Position.is_object() || !Position.contains("x") || !Position.contains("y") ||
                    !Position.at("x").is_number() || !Position.at("y").is_number() ||
                    !std::isfinite(Position.at("x").get<double>()) || !std::isfinite(Position.at("y").get<double>()))
                {
                    Error = "Invalid story graph position";
                    return false;
                }
            }
        }
    }
    return true;
}

bool StoryGraphDocument::Open(const std::filesystem::path& DocumentPath, std::string& Error)
{
    try
    {
        std::ifstream Input(DocumentPath, std::ios::binary);
        if (!Input)
        {
            Error = "Cannot open story document";
            return false;
        }
        const std::string Text((std::istreambuf_iterator<char>(Input)), std::istreambuf_iterator<char>());
        auto Initial = Json::parse(Text);
        StoryDocument Loaded;
        if (!Decode(Initial, Loaded, Error))
        {
            return false;
        }
        Close();
        Path = DocumentPath;
        Fingerprint = ContentHash::FromBytes(Text.data(), Text.size());
        Saved = Initial;
        History.push_back({std::move(Initial), {}});
        Story = std::move(Loaded);
        return true;
    }
    catch (const std::exception& Exception)
    {
        Error = Exception.what();
        return false;
    }
}

bool StoryGraphDocument::Open(AssetRegistry& InRegistry, const AssetKey& Key, std::string& Error)
{
    AssetRegistryEntry Entry;
    if (!InRegistry.TryResolveKey(Key, Entry) || Entry.Metadata.Type != StoryAssetType || Key.HasSubAsset())
    {
        Error = "The selected asset is not a story document";
        return false;
    }
    if (!Open(Entry.AbsolutePath, Error))
    {
        return false;
    }
    Registry = &InRegistry;
    Asset = Key;
    return true;
}

bool StoryGraphDocument::IsReadOnly() const
{
    if (Registry == nullptr)
    {
        return false;
    }
    AssetRegistryEntry Entry;
    return !Registry->TryResolveKey(Asset, Entry) || Entry.Mount == AssetMount::Engine;
}

bool StoryGraphDocument::HasExternalChanges() const
{
    if (!IsOpen())
    {
        return false;
    }
    auto CurrentPath = Path;
    if (Registry != nullptr)
    {
        AssetRegistryEntry Entry;
        if (!Registry->TryResolveKey(Asset, Entry))
        {
            return true;
        }
        CurrentPath = Entry.AbsolutePath;
    }
    ContentHash CurrentFingerprint;
    AssetDiagnostic Diagnostic;
    return !ContentHash::TryHashFile(CurrentPath.string(), CurrentFingerprint, Diagnostic) ||
           CurrentFingerprint != Fingerprint;
}

bool StoryGraphDocument::Save(std::string& Error)
{
    if (!IsOpen() || IsReadOnly())
    {
        Error = "This story document is not writable";
        return false;
    }
    std::filesystem::path Destination = Path;
    if (Registry != nullptr)
    {
        AssetRegistryEntry Entry;
        if (!Registry->TryResolveKey(Asset, Entry))
        {
            Error = "The story asset was removed";
            return false;
        }
        Destination = Entry.AbsolutePath;
    }
    ContentHash CurrentFingerprint;
    AssetDiagnostic Diagnostic;
    if (!ContentHash::TryHashFile(Destination.string(), CurrentFingerprint, Diagnostic) ||
        CurrentFingerprint != Fingerprint)
    {
        Error = "The story changed on disk. Reopen it before saving; your draft is retained.";
        return false;
    }
    StoryDocument Verified;
    if (!Decode(Data(), Verified, Error))
    {
        return false;
    }
    const std::string Text = Data().dump(2) + "\n";
    if (!AtomicFileWriter::WriteText(Destination, Text, Error))
    {
        return false;
    }
    Fingerprint = ContentHash::FromBytes(Text.data(), Text.size());
    Saved = Data();
    Path = Destination;
    if (Registry != nullptr)
    {
        Registry->UpdateSourceFingerprint(Asset.Asset, Fingerprint);
    }
    return true;
}

void StoryGraphDocument::Close()
{
    History.clear();
    Current = 0;
    Story = {};
    Saved = {};
    Path.clear();
    Registry = nullptr;
    Asset = {};
    Fingerprint = {};
    InitialPositions.clear();
}

bool StoryGraphDocument::IsDirty() const
{
    return IsOpen() && Data() != Saved;
}
const StoryNode* StoryGraphDocument::FindNode(const StoryNodeId& Identity) const
{
    const auto Found = std::find_if(Story.Nodes.begin(), Story.Nodes.end(),
                                    [&](const StoryNode& Node) { return Node.Id == Identity; });
    return Found == Story.Nodes.end() ? nullptr : &*Found;
}

std::map<StoryNodeId, StoryGraphPosition> StoryGraphDocument::GetPositions() const
{
    std::map<StoryNodeId, StoryGraphPosition> Positions;
    if (IsOpen() && Data().contains("editor") && Data().at("editor").contains("positions"))
    {
        for (const auto& Entry : Data().at("editor").at("positions").items())
        {
            Positions.emplace(Entry.key(), StoryGraphPosition{Entry.value().at("x"), Entry.value().at("y")});
        }
    }
    return Positions;
}

bool StoryGraphDocument::Apply(const char* Label, const std::function<bool(Json&, std::string&)>& Edit,
                               std::string& Error)
{
    Error.clear();
    if (!IsOpen() || IsReadOnly())
    {
        Error = "This story document is not writable";
        return false;
    }
    try
    {
        auto Candidate = Data();
        if (!Edit(Candidate, Error))
        {
            return false;
        }
        if (Candidate == Data())
        {
            return true;
        }
        for (const auto& Position : InitialPositions)
        {
            if (FindNodeJson(Candidate, Position.first) != nullptr &&
                (!Candidate.contains("editor") || !Candidate["editor"].contains("positions") ||
                 !Candidate["editor"]["positions"].contains(Position.first)))
            {
                Candidate["editor"]["version"] = 1;
                Candidate["editor"]["positions"][Position.first] = {{"x", Position.second.X}, {"y", Position.second.Y}};
            }
        }
        StoryDocument Decoded;
        if (!Decode(Candidate, Decoded, Error))
        {
            return false;
        }
        History.erase(History.begin() + Current + 1, History.end());
        History.push_back({std::move(Candidate), Label});
        ++Current;
        Story = std::move(Decoded);
        return true;
    }
    catch (const std::exception& Exception)
    {
        Error = Exception.what();
        return false;
    }
}

bool StoryGraphDocument::Undo()
{
    if (!CanUndo())
    {
        return false;
    }
    StoryDocument Previous;
    std::string Error;
    if (!Decode(History[Current - 1].Data, Previous, Error))
    {
        return false;
    }
    --Current;
    Story = std::move(Previous);
    return true;
}
bool StoryGraphDocument::Redo()
{
    if (!CanRedo())
    {
        return false;
    }
    StoryDocument Next;
    std::string Error;
    if (!Decode(History[Current + 1].Data, Next, Error))
    {
        return false;
    }
    ++Current;
    Story = std::move(Next);
    return true;
}
const std::string& StoryGraphDocument::GetUndoLabel() const
{
    static const std::string Empty;
    return CanUndo() ? History[Current].Label : Empty;
}
const std::string& StoryGraphDocument::GetRedoLabel() const
{
    static const std::string Empty;
    return CanRedo() ? History[Current + 1].Label : Empty;
}

bool StoryGraphDocument::AddNode(StoryNodeKind Kind, StoryGraphPosition Position, StoryNodeId& Created,
                                 std::string& Error)
{
    StoryNode Node;
    Node.Id = Guid::Generate().ToString();
    Node.Kind = Kind;
    if (Kind == StoryNodeKind::Line)
    {
        Node.Speaker = "Рассказчик";
    }
    if (Kind == StoryNodeKind::Choice)
    {
        Node.Choices = {{"Вариант 1", {}}, {"Вариант 2", {}}};
    }
    if (Kind == StoryNodeKind::Action)
    {
        Node.Actions.push_back({});
    }
    for (auto& Choice : Node.Choices)
    {
        Choice.Id = Guid::Generate().ToString();
    }
    for (auto& Action : Node.Actions)
    {
        Action.Id = Guid::Generate().ToString();
    }
    const bool bApplied = Apply(
        "Add node",
        [&](Json& Candidate, std::string&)
        {
            Candidate["nodes"].push_back(EncodeNode(Node));
            if (Candidate.at("startNodeId").get<std::string>().empty())
            {
                Candidate["startNodeId"] = Node.Id;
            }
            Candidate["editor"]["version"] = 1;
            Candidate["editor"]["positions"][Node.Id] = {{"x", Position.X}, {"y", Position.Y}};
            return true;
        },
        Error);
    if (bApplied)
    {
        Created = Node.Id;
    }
    return bApplied;
}

bool StoryGraphDocument::DeleteNodes(const std::vector<StoryNodeId>& Identities, std::string& Error)
{
    return Apply(
        "Delete nodes",
        [&](Json& Candidate, std::string&)
        {
            const std::set<StoryNodeId> Removed(Identities.begin(), Identities.end());
            auto& Nodes = Candidate["nodes"];
            Nodes.erase(std::remove_if(Nodes.begin(), Nodes.end(),
                                       [&](const Json& Node) { return Removed.count(Node.at("id")) != 0; }),
                        Nodes.end());
            for (auto& Node : Nodes)
            {
                if (Removed.count(Node.value("nextNodeId", std::string{})))
                {
                    Node.erase("nextNodeId");
                }
                if (Node.contains("sources"))
                {
                    auto& Sources = Node["sources"];
                    for (auto Source = Sources.begin(); Source != Sources.end();)
                    {
                        if (Removed.count(Source.value()[0].get<std::string>()))
                        {
                            Source = Sources.erase(Source);
                        }
                        else
                        {
                            ++Source;
                        }
                    }
                }
                if (Node.contains("choices"))
                {
                    for (auto& Choice : Node["choices"])
                    {
                        if (Removed.count(Choice.value("nextNodeId", std::string{})))
                        {
                            Choice["nextNodeId"] = "";
                        }
                    }
                }
            }
            if (Removed.count(Candidate.at("startNodeId")))
            {
                Candidate["startNodeId"] = "";
                for (const auto& Remaining : Nodes)
                {
                    NarrativeNodeDefinition Definition;
                    if (Remaining.value("kind", std::string{}) == "Narrative"
                        && FindNarrativeDefinition(Remaining.value("narrativeType", std::string{}), Definition)
                        && Definition.bPure)
                    {
                        continue;
                    }
                    Candidate["startNodeId"] = Remaining.at("id");
                    break;
                }
            }
            if (Candidate.contains("editor") && Candidate["editor"].contains("positions"))
            {
                for (const auto& Identity : Removed)
                {
                    Candidate["editor"]["positions"].erase(Identity);
                }
            }
            return true;
        },
        Error);
}

bool StoryGraphDocument::ReplaceNode(const StoryNode& Node, std::string& Error)
{
    StoryNode Normalized = Node;
    for (auto& Choice : Normalized.Choices)
    {
        if (Choice.Id.empty())
        {
            Choice.Id = Guid::Generate().ToString();
        }
    }
    for (auto& Action : Normalized.Actions)
    {
        if (Action.Id.empty())
        {
            Action.Id = Guid::Generate().ToString();
        }
    }
    for (const auto& Action : Normalized.Actions)
    {
        if (!std::isfinite(Action.DurationSeconds) || !std::isfinite(Action.TargetPosition.x) ||
            !std::isfinite(Action.TargetPosition.y) || !std::isfinite(Action.TargetPosition.z) ||
            !std::isfinite(Action.TargetRotation.x) || !std::isfinite(Action.TargetRotation.y) ||
            !std::isfinite(Action.TargetRotation.z) || !std::isfinite(Action.TargetRotation.w))
        {
            Error = "Story action values must be finite";
            return false;
        }
    }
    StoryDocument Validation;
    Validation.Nodes = {Normalized};
    Validation.Events = Story.Events;
    if (!ValidateStoryEvents(Validation, Error)) { return false; }
    if (const auto* Previous = FindNode(Node.Id);
        Previous != nullptr && EncodeNode(*Previous, Story.Events) == EncodeNode(Normalized, Story.Events))
    {
        Error.clear();
        return true;
    }
    return Apply(
        "Edit node",
        [&](Json& Candidate, std::string& Failure)
        {
            auto* Existing = FindNodeJson(Candidate, Node.Id);
            if (Existing == nullptr)
            {
                Failure = "The edited story node no longer exists";
                return false;
            }
            for (const auto& Choice : Node.Choices)
            {
                if (!Choice.NextNodeId.empty() && FindNodeJson(Candidate, Choice.NextNodeId) == nullptr)
                {
                    Failure = "A choice target no longer exists";
                    return false;
                }
            }
            if (!Node.NextNodeId.empty() && FindNodeJson(Candidate, Node.NextNodeId) == nullptr)
            {
                Failure = "The continuation target no longer exists";
                return false;
            }
            MergeNode(*Existing, EncodeNode(Normalized, Story.Events), EncodeNode(*FindNode(Node.Id), Story.Events));
            return true;
        },
        Error);
}

bool StoryGraphDocument::Connect(const StoryNodeId& Source, int Port, const StoryNodeId& Target, std::string& Error)
{
    return Apply(
        Target.empty() ? "Disconnect" : "Connect",
        [&](Json& Candidate, std::string& Failure)
        {
            auto* Node = FindNodeJson(Candidate, Source);
            if (Node == nullptr || (!Target.empty() && FindNodeJson(Candidate, Target) == nullptr))
            {
                Failure = "The connection endpoint no longer exists";
                return false;
            }
            if (const auto* Destination = FindNode(Target))
            {
                NarrativeNodeDefinition Definition;
                if (FindNarrativeDefinition(Destination->NarrativeType, Definition) && Definition.bPure)
                {
                    Failure = "Pure nodes cannot receive execution";
                    return false;
                }
            }
            if ((Node->at("kind") == "Choice" || Node->at("kind") == "Narrative") && Port >= 0 && Node->contains("choices") &&
                Port < static_cast<int>((*Node)["choices"].size()))
            {
                (*Node)["choices"][Port]["nextNodeId"] = Target;
            }
            else if (Port == -1 && (Node->at("kind") == "Line" || Node->at("kind") == "Action"))
            {
                (*Node)["nextNodeId"] = Target;
            }
            else
            {
                Failure = "This output port is not available";
                return false;
            }
            return true;
        },
        Error);
}

bool StoryGraphDocument::MoveNodes(const std::map<StoryNodeId, StoryGraphPosition>& Positions, std::string& Error)
{
    return Apply(
        "Move nodes",
        [&](Json& Candidate, std::string& Failure)
        {
            for (const auto& Entry : Positions)
            {
                if (FindNodeJson(Candidate, Entry.first) == nullptr || !std::isfinite(Entry.second.X) ||
                    !std::isfinite(Entry.second.Y))
                {
                    Failure = "Invalid node position";
                    return false;
                }
                Candidate["editor"]["version"] = 1;
                Candidate["editor"]["positions"][Entry.first] = {{"x", Entry.second.X}, {"y", Entry.second.Y}};
            }
            return true;
        },
        Error);
}

bool StoryGraphDocument::SetStart(const StoryNodeId& Identity, std::string& Error)
{
    if (const auto* Node = FindNode(Identity))
    {
        NarrativeNodeDefinition Definition;
        if (FindNarrativeDefinition(Node->NarrativeType, Definition) && Definition.bPure)
        {
            Error = "Pure nodes cannot start execution";
            return false;
        }
    }
    return Apply(
        "Set story start",
        [&](Json& Candidate, std::string& Failure)
        {
            if (FindNodeJson(Candidate, Identity) == nullptr)
            {
                Failure = "The start node no longer exists";
                return false;
            }
            Candidate["startNodeId"] = Identity;
            return true;
        },
        Error);
}

bool StoryGraphDocument::ValidateForPlayback(std::string& Error) const
{
    if (!ValidateNarrativeDocument(Story, Error))
    {
        return false;
    }
    if (!IsOpen())
    {
        Error = "No story document is open";
        return false;
    }
    if (Story.Nodes.empty())
    {
        return true;
    }
    std::vector<StoryNodeId> Pending{Story.StartNodeId};
    std::set<StoryNodeId> Visited;
    while (!Pending.empty())
    {
        const StoryNodeId Identity = std::move(Pending.back());
        Pending.pop_back();
        if (!Visited.insert(Identity).second)
        {
            continue;
        }
        const auto* Node = FindNode(Identity);
        if (Node == nullptr)
        {
            Error = "A continuation points to a missing node";
            return false;
        }
        if (Node->Kind == StoryNodeKind::Line || Node->Kind == StoryNodeKind::Action)
        {
            if (!Node->NextNodeId.empty())
            {
                Pending.push_back(Node->NextNodeId);
            }
        }
        if (Node->Kind == StoryNodeKind::Choice)
        {
            if (Node->Choices.empty())
            {
                Error = "Add and connect choices before starting playback";
                return false;
            }
            for (const auto& Choice : Node->Choices)
            {
                if (Choice.NextNodeId.empty())
                {
                    Error = "Connect every choice before starting playback";
                    return false;
                }
                Pending.push_back(Choice.NextNodeId);
            }
        }
    }
    return true;
}


bool StoryGraphDocument::AddNarrativeNode(const std::string& Type, StoryGraphPosition Position,
    StoryNodeId& Created, std::string& Error)
{
    NarrativeNodeDefinition Definition;
    if (!FindNarrativeDefinition(Type, Definition))
    {
        Error = "Narrative type not found: " + Type;
        return false;
    }
    StoryNode Node;
    Node.Id = Guid::Generate().ToString();
    Node.Kind = StoryNodeKind::Narrative;
    Node.NarrativeType = Type;
    for (const auto& Pin : Definition.Inputs)
    {
        if (Pin.bHasDefault)
        {
            Node.NarrativeInputs[Pin.Property.Id.Value] = Pin.Default;
        }
        else
        {
            ReflectedValue Value;
            nlohmann::json Encoded;
            if (MakeDefaultReflectionValue(Pin.Property.ValueTypeId, Value).bOk
                && ReflectionJson::ReflectedValueToJson(Pin.Property.ValueTypeId, Value, Encoded).bOk)
            {
                Node.NarrativeInputs[Pin.Property.Id.Value] = std::move(Encoded);
            }
        }
    }
    for (const auto& Exit : Definition.Exits)
    {
        Node.Choices.push_back({Exit, {}, Exit});
    }
    const bool Applied = Apply("Add Narrative node", [&](Json& Candidate, std::string&)
    {
        Candidate["nodes"].push_back(EncodeNode(Node));
        if (!Definition.bPure && Candidate.at("startNodeId").get<std::string>().empty())
        {
            Candidate["startNodeId"] = Node.Id;
        }
        Candidate["editor"]["version"] = 1;
        Candidate["editor"]["positions"][Node.Id] = {{"x", Position.X}, {"y", Position.Y}};
        return true;
    }, Error);
    if (Applied)
    {
        Created = Node.Id;
    }
    return Applied;
}

bool StoryGraphDocument::ConnectData(const StoryNodeId& Source, const std::string& Output,
    const StoryNodeId& Target, const std::string& Input, std::string& Error)
{
    const StoryNode* Consumer = FindNode(Target);
    const StoryNode* Producer = FindNode(Source);
    NarrativeNodeDefinition ConsumerType;
    NarrativeNodeDefinition ProducerType;
    if (!Consumer || !FindNarrativeDefinition(Consumer->NarrativeType, ConsumerType))
    {
        Error = "Narrative input node is unavailable";
        return false;
    }
    const auto InputPin = std::find_if(ConsumerType.Inputs.begin(), ConsumerType.Inputs.end(), [&](const NarrativePin& Pin)
    {
        return Pin.Property.Id.Value == Input;
    });
    if (InputPin == ConsumerType.Inputs.end())
    {
        Error = "Narrative input is unavailable";
        return false;
    }
    if (!Source.empty())
    {
        if (!Producer || !FindNarrativeDefinition(Producer->NarrativeType, ProducerType))
        {
            Error = "Narrative output node is unavailable";
            return false;
        }
        const auto OutputPin = std::find_if(ProducerType.Outputs.begin(), ProducerType.Outputs.end(), [&](const NarrativePin& Pin)
        {
            return Pin.Property.Id.Value == Output;
        });
        if (OutputPin == ProducerType.Outputs.end() || OutputPin->Property.ValueTypeId != InputPin->Property.ValueTypeId)
        {
            Error = "Narrative pin types do not match";
            return false;
        }
        std::set<std::string> Visited;
        std::vector<std::string> Pending{Source};
        while (!Pending.empty())
        {
            const auto Identity = Pending.back();
            Pending.pop_back();
            if (Identity == Target)
            {
                Error = "Narrative data connections cannot form cycles";
                return false;
            }
            if (!Visited.insert(Identity).second)
            {
                continue;
            }
            if (const auto* Node = FindNode(Identity))
            {
                for (const auto& Dependency : Node->NarrativeSources)
                {
                    Pending.push_back(Dependency.second.first);
                }
            }
        }
    }
    StoryNode Updated = *Consumer;
    if (Source.empty())
    {
        Updated.NarrativeSources.erase(Input);
    }
    else
    {
        Updated.NarrativeSources[Input] = {Source, Output};
    }
    return ReplaceNode(Updated, Error);
}

bool StoryGraphDocument::SetNarrativeInput(const StoryNodeId& Identity, const std::string& Input,
    const nlohmann::json& Value, std::string& Error, const ReflectionJson::ObjectResolver& ResolveObject)
{
    const StoryNode* Node = FindNode(Identity);
    NarrativeNodeDefinition Definition;
    if (Node == nullptr || Node->Kind != StoryNodeKind::Narrative || !FindNarrativeDefinition(Node->NarrativeType, Definition))
    {
        Error = "Narrative node definition is unavailable";
        return false;
    }
    const auto Pin = std::find_if(Definition.Inputs.begin(), Definition.Inputs.end(), [&](const NarrativePin& Candidate)
    {
        return Candidate.Property.Id.Value == Input;
    });
    if (Pin == Definition.Inputs.end())
    {
        Error = "Narrative input does not exist";
        return false;
    }
    ReflectedValue Converted;
    const auto Conversion = ReflectionJson::JsonToReflectedValue(Pin->Property.ValueTypeId, Value, Converted, ResolveObject);
    if (!Conversion.bOk)
    {
        Error = Conversion.Message;
        return false;
    }
    const auto Validation = PropertyAccess::ValidatePropertyValue(Pin->Property, Converted);
    if (!Validation.bOk)
    {
        Error = Validation.Message;
        return false;
    }
    StoryNode Updated = *Node;
    Updated.NarrativeInputs[Input] = Value;
    Updated.NarrativeSources.erase(Input);
    return ReplaceNode(Updated, Error);
}
