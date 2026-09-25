#include "Story/StoryDocumentIO.h"

#include "Game/SceneSerializer.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace
{
nlohmann::json Vector3ToJson(const Vector3& Value)
{
    return nlohmann::json::array({Value.x, Value.y, Value.z});
}

nlohmann::json QuaternionToJson(const Quaternion& Value)
{
    return nlohmann::json::array({Value.x, Value.y, Value.z, Value.w});
}

bool TryReadVector3(const nlohmann::json& JsonValue, Vector3& OutValue, std::string& OutError)
{
    if (!JsonValue.is_array() || JsonValue.size() != 3)
    {
        OutError = "Vector3 must be an array of 3 numbers";
        return false;
    }
    if (!JsonValue[0].is_number() || !JsonValue[1].is_number() || !JsonValue[2].is_number())
    {
        OutError = "Vector3 elements must be numbers";
        return false;
    }
    OutValue = Vector3(
        JsonValue[0].get<float>(),
        JsonValue[1].get<float>(),
        JsonValue[2].get<float>());
    return true;
}

bool TryReadQuaternion(const nlohmann::json& JsonValue, Quaternion& OutValue, std::string& OutError)
{
    if (!JsonValue.is_array() || JsonValue.size() != 4)
    {
        OutError = "Quaternion must be an array of 4 numbers";
        return false;
    }
    if (!JsonValue[0].is_number() || !JsonValue[1].is_number()
        || !JsonValue[2].is_number() || !JsonValue[3].is_number())
    {
        OutError = "Quaternion elements must be numbers";
        return false;
    }
    OutValue = Quaternion(
        JsonValue[0].get<float>(),
        JsonValue[1].get<float>(),
        JsonValue[2].get<float>(),
        JsonValue[3].get<float>());
    return true;
}

bool TryParseNodeKind(const std::string& KindText, StoryNodeKind& OutKind, std::string& OutError)
{
    if (KindText == "Line")
    {
        OutKind = StoryNodeKind::Line;
        return true;
    }
    if (KindText == "Choice")
    {
        OutKind = StoryNodeKind::Choice;
        return true;
    }
    if (KindText == "Action")
    {
        OutKind = StoryNodeKind::Action;
        return true;
    }
    if (KindText == "End")
    {
        OutKind = StoryNodeKind::End;
        return true;
    }
    OutError = "Unknown story node kind: " + KindText;
    return false;
}

bool TryParseActionKind(const std::string& KindText, StoryActionKind& OutKind, std::string& OutError)
{
    if (KindText == "Wait")
    {
        OutKind = StoryActionKind::Wait;
        return true;
    }
    if (KindText == "MoveTo")
    {
        OutKind = StoryActionKind::MoveTo;
        return true;
    }
    if (KindText == "CameraCut")
    {
        OutKind = StoryActionKind::CameraCut;
        return true;
    }
    OutError = "Unknown story action kind: " + KindText;
    return false;
}

bool TryReadAction(const nlohmann::json& JsonValue, StoryActionDefinition& OutAction, std::string& OutError)
{
    if (!JsonValue.is_object())
    {
        OutError = "Action must be an object";
        return false;
    }
    if (!JsonValue.contains("kind") || !JsonValue.at("kind").is_string())
    {
        OutError = "Action missing kind";
        return false;
    }
    if (!TryParseActionKind(JsonValue.at("kind").get<std::string>(), OutAction.Kind, OutError))
    {
        return false;
    }

    if (JsonValue.contains("durationSeconds") && JsonValue.at("durationSeconds").is_number())
    {
        OutAction.DurationSeconds = JsonValue.at("durationSeconds").get<float>();
    }

    if (JsonValue.contains("targetObjectId"))
    {
        if (!JsonValue.at("targetObjectId").is_string())
        {
            OutError = "targetObjectId must be a string";
            return false;
        }
        OutAction.TargetObjectId = JsonValue.at("targetObjectId").get<std::string>();
    }

    if (JsonValue.contains("targetObjectName") && JsonValue.at("targetObjectName").is_string())
    {
        OutAction.TargetObjectName = JsonValue.at("targetObjectName").get<std::string>();
    }

    if (JsonValue.contains("position"))
    {
        if (!TryReadVector3(JsonValue.at("position"), OutAction.TargetPosition, OutError))
        {
            return false;
        }
    }

    if (JsonValue.contains("rotation"))
    {
        if (!TryReadQuaternion(JsonValue.at("rotation"), OutAction.TargetRotation, OutError))
        {
            return false;
        }
        OutAction.bHasTargetRotation = true;
    }

    if (JsonValue.contains("useTargetObjectTransform") && JsonValue.at("useTargetObjectTransform").is_boolean())
    {
        OutAction.bUseTargetObjectTransform = JsonValue.at("useTargetObjectTransform").get<bool>();
    }

    return true;
}

bool TryReadNode(const nlohmann::json& JsonValue, StoryNode& OutNode, std::string& OutError)
{
    if (!JsonValue.is_object())
    {
        OutError = "Node must be an object";
        return false;
    }
    if (!JsonValue.contains("id") || !JsonValue.at("id").is_string())
    {
        OutError = "Node missing id";
        return false;
    }
    if (!JsonValue.contains("kind") || !JsonValue.at("kind").is_string())
    {
        OutError = "Node missing kind";
        return false;
    }

    OutNode.Id = JsonValue.at("id").get<std::string>();
    if (!TryParseNodeKind(JsonValue.at("kind").get<std::string>(), OutNode.Kind, OutError))
    {
        return false;
    }

    if (JsonValue.contains("speaker") && JsonValue.at("speaker").is_string())
    {
        OutNode.Speaker = JsonValue.at("speaker").get<std::string>();
    }
    if (JsonValue.contains("text") && JsonValue.at("text").is_string())
    {
        OutNode.Text = JsonValue.at("text").get<std::string>();
    }
    if (JsonValue.contains("prompt") && JsonValue.at("prompt").is_string())
    {
        OutNode.Prompt = JsonValue.at("prompt").get<std::string>();
    }
    if (JsonValue.contains("nextNodeId") && JsonValue.at("nextNodeId").is_string())
    {
        OutNode.NextNodeId = JsonValue.at("nextNodeId").get<std::string>();
    }

    if (JsonValue.contains("choices") && JsonValue.at("choices").is_array())
    {
        for (const nlohmann::json& ChoiceJson : JsonValue.at("choices"))
        {
            if (!ChoiceJson.is_object())
            {
                OutError = "Choice entry must be an object";
                return false;
            }
            if (!ChoiceJson.contains("label") || !ChoiceJson.at("label").is_string())
            {
                OutError = "Choice missing label";
                return false;
            }
            if (!ChoiceJson.contains("nextNodeId") || !ChoiceJson.at("nextNodeId").is_string())
            {
                OutError = "Choice missing nextNodeId";
                return false;
            }
            StoryChoice Choice{};
            Choice.Label = ChoiceJson.at("label").get<std::string>();
            Choice.NextNodeId = ChoiceJson.at("nextNodeId").get<std::string>();
            OutNode.Choices.push_back(std::move(Choice));
        }
    }

    if (JsonValue.contains("actions") && JsonValue.at("actions").is_array())
    {
        for (const nlohmann::json& ActionJson : JsonValue.at("actions"))
        {
            StoryActionDefinition Action{};
            if (!TryReadAction(ActionJson, Action, OutError))
            {
                return false;
            }
            OutNode.Actions.push_back(std::move(Action));
        }
    }

    return true;
}

nlohmann::json ActionToJson(const StoryActionDefinition& Action)
{
    nlohmann::json Document = nlohmann::json::object();
    switch (Action.Kind)
    {
    case StoryActionKind::Wait:
        Document["kind"] = "Wait";
        break;
    case StoryActionKind::MoveTo:
        Document["kind"] = "MoveTo";
        break;
    case StoryActionKind::CameraCut:
        Document["kind"] = "CameraCut";
        break;
    }
    Document["durationSeconds"] = Action.DurationSeconds;
    if (!Action.TargetObjectId.empty())
    {
        Document["targetObjectId"] = Action.TargetObjectId;
    }
    else if (!Action.TargetObjectName.empty())
    {
        Document["targetObjectName"] = Action.TargetObjectName;
    }
    if (Action.Kind == StoryActionKind::MoveTo || Action.Kind == StoryActionKind::CameraCut)
    {
        Document["position"] = Vector3ToJson(Action.TargetPosition);
    }
    if (Action.bHasTargetRotation)
    {
        Document["rotation"] = QuaternionToJson(Action.TargetRotation);
    }
    if (Action.bUseTargetObjectTransform)
    {
        Document["useTargetObjectTransform"] = true;
    }
    return Document;
}

nlohmann::json NodeToJson(const StoryNode& Node)
{
    nlohmann::json Document = nlohmann::json::object();
    Document["id"] = Node.Id;
    switch (Node.Kind)
    {
    case StoryNodeKind::Line:
        Document["kind"] = "Line";
        break;
    case StoryNodeKind::Choice:
        Document["kind"] = "Choice";
        break;
    case StoryNodeKind::Action:
        Document["kind"] = "Action";
        break;
    case StoryNodeKind::End:
        Document["kind"] = "End";
        break;
    }
    if (!Node.Speaker.empty())
    {
        Document["speaker"] = Node.Speaker;
    }
    if (!Node.Text.empty())
    {
        Document["text"] = Node.Text;
    }
    if (!Node.Prompt.empty())
    {
        Document["prompt"] = Node.Prompt;
    }
    if (!Node.NextNodeId.empty())
    {
        Document["nextNodeId"] = Node.NextNodeId;
    }
    if (!Node.Choices.empty())
    {
        nlohmann::json ChoicesJson = nlohmann::json::array();
        for (const StoryChoice& Choice : Node.Choices)
        {
            nlohmann::json ChoiceJson = nlohmann::json::object();
            ChoiceJson["label"] = Choice.Label;
            ChoiceJson["nextNodeId"] = Choice.NextNodeId;
            ChoicesJson.push_back(std::move(ChoiceJson));
        }
        Document["choices"] = std::move(ChoicesJson);
    }
    if (!Node.Actions.empty())
    {
        nlohmann::json ActionsJson = nlohmann::json::array();
        for (const StoryActionDefinition& Action : Node.Actions)
        {
            ActionsJson.push_back(ActionToJson(Action));
        }
        Document["actions"] = std::move(ActionsJson);
    }
    return Document;
}
}

StorySerializeResult StoryDocumentIO::LoadFromJson(const std::string& JsonText, StoryDocument& OutDocument)
{
    StorySerializeResult Result{};
    OutDocument = StoryDocument{};

    nlohmann::json Root;
    try
    {
        Root = nlohmann::json::parse(JsonText);
    }
    catch (const std::exception& Exception)
    {
        Result.Error = std::string("Invalid story JSON: ") + Exception.what();
        return Result;
    }

    if (!Root.is_object())
    {
        Result.Error = "Story root must be a JSON object";
        return Result;
    }
    if (!Root.contains("format") || !Root.at("format").is_string()
        || Root.at("format").get<std::string>() != FormatId)
    {
        Result.Error = "Unsupported or missing story format";
        return Result;
    }
    if (!Root.contains("formatVersion") || !Root.at("formatVersion").is_number_unsigned()
        || Root.at("formatVersion").get<uint32_t>() != FormatVersion)
    {
        Result.Error = "Unsupported story format version";
        return Result;
    }
    if (!Root.contains("startNodeId") || !Root.at("startNodeId").is_string())
    {
        Result.Error = "Story missing startNodeId";
        return Result;
    }
    if (!Root.contains("nodes") || !Root.at("nodes").is_array())
    {
        Result.Error = "Story missing nodes array";
        return Result;
    }

    OutDocument.StartNodeId = Root.at("startNodeId").get<std::string>();
    if (Root.contains("name") && Root.at("name").is_string())
    {
        OutDocument.Name = Root.at("name").get<std::string>();
    }

    std::unordered_map<std::string, size_t> SeenIds;
    for (const nlohmann::json& NodeJson : Root.at("nodes"))
    {
        StoryNode Node{};
        if (!TryReadNode(NodeJson, Node, Result.Error))
        {
            return Result;
        }
        if (SeenIds.find(Node.Id) != SeenIds.end())
        {
            Result.Error = "Duplicate story node id: " + Node.Id;
            return Result;
        }
        SeenIds[Node.Id] = OutDocument.Nodes.size();
        OutDocument.Nodes.push_back(std::move(Node));
    }

    if (SeenIds.find(OutDocument.StartNodeId) == SeenIds.end())
    {
        Result.Error = "startNodeId not found in nodes";
        return Result;
    }

    Result.bOk = true;
    return Result;
}

StorySerializeResult StoryDocumentIO::LoadFromFile(const std::filesystem::path& AbsolutePath, StoryDocument& OutDocument)
{
    StorySerializeResult Result{};
    if (AbsolutePath.empty())
    {
        Result.Error = "Story path is empty";
        return Result;
    }

    std::ifstream Input(AbsolutePath, std::ios::binary);
    if (!Input)
    {
        Result.Error = "Failed to open story file";
        return Result;
    }

    std::string JsonText((std::istreambuf_iterator<char>(Input)), std::istreambuf_iterator<char>());
    return LoadFromJson(JsonText, OutDocument);
}

StorySerializeResult StoryDocumentIO::SaveToJson(const StoryDocument& Document, std::string& OutJsonText)
{
    StorySerializeResult Result{};
    nlohmann::json Root = nlohmann::json::object();
    Root["format"] = FormatId;
    Root["formatVersion"] = FormatVersion;
    Root["name"] = Document.Name;
    Root["startNodeId"] = Document.StartNodeId;

    nlohmann::json NodesJson = nlohmann::json::array();
    for (const StoryNode& Node : Document.Nodes)
    {
        NodesJson.push_back(NodeToJson(Node));
    }
    Root["nodes"] = std::move(NodesJson);

    OutJsonText = Root.dump(2);
    Result.bOk = true;
    return Result;
}

StorySerializeResult StoryDocumentIO::SaveToFile(const StoryDocument& Document, const std::filesystem::path& AbsolutePath)
{
    StorySerializeResult Result{};
    std::string JsonText;
    Result = SaveToJson(Document, JsonText);
    if (!Result.bOk)
    {
        return Result;
    }

    const SceneSerializeResult WriteResult = SceneSerializer::WriteTextFileAtomically(AbsolutePath, JsonText);
    Result.bOk = WriteResult.bOk;
    Result.Error = WriteResult.Error;
    return Result;
}
