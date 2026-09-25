#include "Game/SceneSerializer.h"

#include "Core/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Core/Transform.h"
#include "Game/Scene.h"
#include "Gameplay/Component.h"
#include "Gameplay/GameObject.h"
#include "Reflection/Json/ReflectionJson.h"
#include "Reflection/ReflectionSubsystem.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
void DestroyPartialScene(Scene* CreatedScene, MemorySubsystem* Memory)
{
    if (CreatedScene == nullptr)
    {
        return;
    }
    while (!CreatedScene->GetAllObjects().empty())
    {
        CreatedScene->DestroyGameObject(CreatedScene->GetAllObjects().back());
    }
    Memory->DestroyObject(CreatedScene);
}

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

nlohmann::json TransformToJson(const Transform& Value)
{
    nlohmann::json Document = nlohmann::json::object();
    Document["position"] = Vector3ToJson(Value.Position);
    Document["rotation"] = QuaternionToJson(Value.Rotation);
    Document["scale"] = Vector3ToJson(Value.Scale);
    return Document;
}

bool TryReadTransform(const nlohmann::json& JsonValue, Transform& OutValue, std::string& OutError)
{
    if (!JsonValue.is_object())
    {
        OutError = "transform must be an object";
        return false;
    }
    if (JsonValue.contains("position") && !TryReadVector3(JsonValue.at("position"), OutValue.Position, OutError))
    {
        return false;
    }
    if (JsonValue.contains("rotation") && !TryReadQuaternion(JsonValue.at("rotation"), OutValue.Rotation, OutError))
    {
        return false;
    }
    if (JsonValue.contains("scale") && !TryReadVector3(JsonValue.at("scale"), OutValue.Scale, OutError))
    {
        return false;
    }
    return true;
}



bool DetectParentCycles(
    const std::unordered_map<std::string, std::string>& ParentById,
    std::string& OutError)
{
    for (const auto& Pair : ParentById)
    {
        const std::string& StartId = Pair.first;
        std::unordered_set<std::string> Visited;
        std::string Current = Pair.second;
        while (!Current.empty())
        {
            if (Current == StartId)
            {
                OutError = "Parent cycle detected involving object '" + StartId + "'";
                return true;
            }
            if (!Visited.insert(Current).second)
            {
                OutError = "Parent cycle detected near object '" + Current + "'";
                return true;
            }
            const auto Iterator = ParentById.find(Current);
            if (Iterator == ParentById.end())
            {
                break;
            }
            Current = Iterator->second;
        }
    }
    return false;
}
}

SceneSerializeResult SceneSerializer::WriteTextFileAtomically(
    const std::filesystem::path& AbsolutePath,
    const std::string& Contents)
{
    SceneSerializeResult Result{};
    if (AbsolutePath.empty())
    {
        Result.Error = "Output path is empty";
        return Result;
    }

    std::error_code Error;
    std::filesystem::create_directories(AbsolutePath.parent_path(), Error);
    if (Error)
    {
        Result.Error = "Failed to create directories: " + Error.message();
        return Result;
    }

    const std::filesystem::path TemporaryPath = AbsolutePath.string() + ".tmp";
    {
        std::ofstream Output(TemporaryPath, std::ios::binary | std::ios::trunc);
        if (!Output)
        {
            Result.Error = "Failed to open temporary file for writing";
            return Result;
        }
        Output.write(Contents.data(), static_cast<std::streamsize>(Contents.size()));
        if (!Output)
        {
            Result.Error = "Failed while writing temporary file";
            Output.close();
            std::filesystem::remove(TemporaryPath);
            return Result;
        }
        Output.flush();
        if (!Output)
        {
            Result.Error = "Failed to flush temporary file";
            Output.close();
            std::filesystem::remove(TemporaryPath);
            return Result;
        }
    }

    {
        std::ifstream Verify(TemporaryPath, std::ios::binary);
        if (!Verify)
        {
            std::filesystem::remove(TemporaryPath);
            Result.Error = "Failed to re-open temporary file for verification";
            return Result;
        }
        std::string Written((std::istreambuf_iterator<char>(Verify)), std::istreambuf_iterator<char>());
        if (Written != Contents)
        {
            std::filesystem::remove(TemporaryPath);
            Result.Error = "Temporary file contents do not match written payload";
            return Result;
        }
    }

    std::filesystem::rename(TemporaryPath, AbsolutePath, Error);
    if (Error)
    {
        std::filesystem::remove(AbsolutePath, Error);
        std::filesystem::rename(TemporaryPath, AbsolutePath, Error);
        if (Error)
        {
            std::filesystem::remove(TemporaryPath);
            Result.Error = "Failed to replace destination file: " + Error.message();
            return Result;
        }
    }

    Result.bOk = true;
    return Result;
}

SceneSerializeResult SceneSerializer::SerializeToJson(const Scene& InScene, std::string& OutJsonText)
{
    SceneSerializeResult Result{};
    nlohmann::json Root = nlohmann::json::object();
    Root["format"] = FormatId;
    Root["formatVersion"] = FormatVersion;
    Root["name"] = InScene.GetName();
    Root["objects"] = nlohmann::json::array();

    const std::vector<GameObject*>& Objects = InScene.GetAllObjects();
    std::unordered_map<const GameObject*, std::string> IdByObject;
    IdByObject.reserve(Objects.size());
    for (size_t Index = 0; Index < Objects.size(); ++Index)
    {
        const GameObject* ObjectInstance = Objects[Index];
        if (ObjectInstance == nullptr)
        {
            Result.Error = "Scene contains a null GameObject";
            return Result;
        }
        IdByObject.emplace(ObjectInstance, ObjectInstance->GetPersistentId());
    }

    for (size_t ObjectIndex = 0; ObjectIndex < Objects.size(); ++ObjectIndex)
    {
        const GameObject* ObjectInstance = Objects[ObjectIndex];
        nlohmann::json ObjectJson = nlohmann::json::object();
        ObjectJson["id"] = IdByObject.at(ObjectInstance);
        ObjectJson["name"] = ObjectInstance->GetName();
        ObjectJson["bActive"] = ObjectInstance->IsActive();
        ObjectJson["bVisual"] = ObjectInstance->IsVisual();
        ObjectJson["transform"] = TransformToJson(ObjectInstance->GetTransform());

        if (ObjectInstance->GetParent() != nullptr)
        {
            const auto ParentIterator = IdByObject.find(ObjectInstance->GetParent());
            if (ParentIterator == IdByObject.end())
            {
                Result.Error = "Parent object is not part of the same Scene";
                return Result;
            }
            ObjectJson["parent"] = ParentIterator->second;
        }
        else
        {
            ObjectJson["parent"] = nullptr;
        }

        ObjectJson["components"] = nlohmann::json::array();
        const std::vector<Component*>& Components = ObjectInstance->GetAllComponents();
        for (size_t ComponentIndex = 0; ComponentIndex < Components.size(); ++ComponentIndex)
        {
            Component* ComponentInstance = Components[ComponentIndex];
            if (ComponentInstance == nullptr)
            {
                Result.Error = "GameObject contains a null Component";
                return Result;
            }
            if (ComponentInstance->GetClass() == nullptr)
            {
                Result.Error = "Component '" + ComponentInstance->GetName() + "' has no Class (not reflected)";
                return Result;
            }

            ReflectionJsonDocument ComponentDocument;
            ReflectionDiagnostic Serialized = ReflectionJson::SerializeObject(ComponentInstance, ComponentDocument);
            if (!Serialized.bOk)
            {
                Result.Error = Serialized.Message;
                return Result;
            }

            nlohmann::json ComponentJson = nlohmann::json::object();
            ComponentJson["id"] = ComponentInstance->GetPersistentId();
            ComponentJson["type"] = ComponentDocument.Type.Value;
            ComponentJson["typeVersion"] = ComponentDocument.TypeVersion;
            ComponentJson["bEnabled"] = ComponentInstance->IsEnabled();
            ComponentJson["properties"] = std::move(ComponentDocument.Properties);
            ObjectJson["components"].push_back(std::move(ComponentJson));
        }

        Root["objects"].push_back(std::move(ObjectJson));
    }

    OutJsonText = Root.dump(2);
    Result.bOk = true;
    return Result;
}

SceneSerializeResult SceneSerializer::SerializeToFile(
    const Scene& InScene,
    const std::filesystem::path& AbsolutePath)
{
    std::string JsonText;
    SceneSerializeResult Serialized = SerializeToJson(InScene, JsonText);
    if (!Serialized.bOk)
    {
        return Serialized;
    }
    return WriteTextFileAtomically(AbsolutePath, JsonText);
}

SceneSerializeResult SceneSerializer::DeserializeFromJson(const std::string& JsonText, Scene*& OutScene)
{
    SceneSerializeResult Result{};
    OutScene = nullptr;

    nlohmann::json Root;
    try
    {
        Root = nlohmann::json::parse(JsonText);
    }
    catch (const std::exception& Exception)
    {
        Result.Error = std::string("Invalid scene JSON: ") + Exception.what();
        return Result;
    }

    if (!Root.is_object())
    {
        Result.Error = "Scene root must be a JSON object";
        return Result;
    }
    if (!Root.contains("format") || !Root.at("format").is_string() || Root.at("format").get<std::string>() != FormatId)
    {
        Result.Error = "Unsupported or missing scene format id";
        return Result;
    }
    if (!Root.contains("formatVersion") || !Root.at("formatVersion").is_number_unsigned())
    {
        Result.Error = "Missing or invalid formatVersion";
        return Result;
    }
    const uint32_t Version = Root.at("formatVersion").get<uint32_t>();
    if (Version != FormatVersion)
    {
        Result.Error = "Unsupported scene formatVersion " + std::to_string(Version);
        return Result;
    }
    if (!Root.contains("objects") || !Root.at("objects").is_array())
    {
        Result.Error = "Scene objects array is required";
        return Result;
    }

    MemorySubsystem* Memory = MemorySubsystem::Get();
    if (Memory == nullptr)
    {
        Result.Error = "MemorySubsystem is not initialized";
        return Result;
    }
    ReflectionSubsystem& Reflection = ReflectionSubsystem::Get();
    if (!Reflection.IsInitialized())
    {
        Result.Error = "ReflectionSubsystem is not initialized";
        return Result;
    }

    std::string SceneName = "Scene";
    if (Root.contains("name") && Root.at("name").is_string())
    {
        SceneName = Root.at("name").get<std::string>();
    }

    Scene* CreatedScene = Memory->NewObject<Scene>(SceneName);
    if (CreatedScene == nullptr)
    {
        Result.Error = "Failed to allocate Scene";
        return Result;
    }

    std::unordered_map<std::string, GameObject*> ObjectById;
    std::unordered_map<std::string, std::string> ParentById;
    std::unordered_set<std::string> SeenObjectIds;
    std::unordered_set<std::string> SeenComponentIds;
    std::vector<Component*> DeferredCreate;

    const nlohmann::json& ObjectsJson = Root.at("objects");
    for (const nlohmann::json& ObjectJson : ObjectsJson)
    {
        if (!ObjectJson.is_object())
        {
            Result.Error = "Scene object entry must be an object";
            DestroyPartialScene(CreatedScene, Memory);
            return Result;
        }
        if (!ObjectJson.contains("id") || !ObjectJson.at("id").is_string())
        {
            Result.Error = "Scene object requires string id";
            DestroyPartialScene(CreatedScene, Memory);
            return Result;
        }
        const std::string ObjectId = ObjectJson.at("id").get<std::string>();
        if (ObjectId.empty() || !SeenObjectIds.insert(ObjectId).second)
        {
            Result.Error = "Duplicate or empty object id '" + ObjectId + "'";
            DestroyPartialScene(CreatedScene, Memory);
            return Result;
        }

        std::string ObjectName = ObjectId;
        if (ObjectJson.contains("name") && ObjectJson.at("name").is_string())
        {
            ObjectName = ObjectJson.at("name").get<std::string>();
        }

        GameObject* ObjectInstance = CreatedScene->CreateGameObject(ObjectName);
        if (ObjectInstance == nullptr)
        {
            Result.Error = "Failed to create GameObject";
            DestroyPartialScene(CreatedScene, Memory);
            return Result;
        }
        ObjectInstance->PersistentId = ObjectId;
        ObjectById.emplace(ObjectId, ObjectInstance);

        if (ObjectJson.contains("bActive") && ObjectJson.at("bActive").is_boolean())
        {
            ObjectInstance->SetActive(ObjectJson.at("bActive").get<bool>());
        }
        if (ObjectJson.contains("bVisual") && ObjectJson.at("bVisual").is_boolean())
        {
            ObjectInstance->SetVisual(ObjectJson.at("bVisual").get<bool>());
        }
        if (ObjectJson.contains("transform"))
        {
            Transform LoadedTransform = ObjectInstance->GetTransform();
            if (!TryReadTransform(ObjectJson.at("transform"), LoadedTransform, Result.Error))
            {
                DestroyPartialScene(CreatedScene, Memory);
                return Result;
            }
            ObjectInstance->SetTransform(LoadedTransform);
        }

        if (ObjectJson.contains("parent") && !ObjectJson.at("parent").is_null())
        {
            if (!ObjectJson.at("parent").is_string())
            {
                Result.Error = "parent must be a string id or null";
                DestroyPartialScene(CreatedScene, Memory);
                return Result;
            }
            ParentById.emplace(ObjectId, ObjectJson.at("parent").get<std::string>());
        }

        if (ObjectJson.contains("components"))
        {
            if (!ObjectJson.at("components").is_array())
            {
                Result.Error = "components must be an array";
                DestroyPartialScene(CreatedScene, Memory);
                return Result;
            }

            for (const nlohmann::json& ComponentJson : ObjectJson.at("components"))
            {
                if (!ComponentJson.is_object())
                {
                    Result.Error = "component entry must be an object";
                    DestroyPartialScene(CreatedScene, Memory);
                    return Result;
                }
                if (!ComponentJson.contains("id") || !ComponentJson.at("id").is_string())
                {
                    Result.Error = "component requires string id";
                    DestroyPartialScene(CreatedScene, Memory);
                    return Result;
                }
                const std::string ComponentId = ComponentJson.at("id").get<std::string>();
                if (ComponentId.empty() || !SeenComponentIds.insert(ComponentId).second)
                {
                    Result.Error = "Duplicate or empty component id '" + ComponentId + "'";
                    DestroyPartialScene(CreatedScene, Memory);
                    return Result;
                }
                if (!ComponentJson.contains("type") || !ComponentJson.at("type").is_string())
                {
                    Result.Error = "component requires string type";
                    DestroyPartialScene(CreatedScene, Memory);
                    return Result;
                }

                const TypeId ComponentType{ComponentJson.at("type").get<std::string>()};
                Object* CreatedObject = Reflection.CreateInstance(ComponentType);
                if (CreatedObject == nullptr)
                {
                    Result.Error = "Unknown or unsupported component type '" + ComponentType.Value + "'";
                    DestroyPartialScene(CreatedScene, Memory);
                    return Result;
                }

                Component* ComponentInstance = dynamic_cast<Component*>(CreatedObject);
                if (ComponentInstance == nullptr)
                {
                    Memory->DestroyObject(CreatedObject);
                    Result.Error = "Type '" + ComponentType.Value + "' is not a Component";
                    DestroyPartialScene(CreatedScene, Memory);
                    return Result;
                }

                ComponentInstance->PersistentId = ComponentId;
                ReflectionJsonDocument ComponentDocument;
                ComponentDocument.Type = ComponentType;
                ComponentDocument.TypeVersion = 1;
                if (ComponentJson.contains("typeVersion") && ComponentJson.at("typeVersion").is_number_unsigned())
                {
                    ComponentDocument.TypeVersion = ComponentJson.at("typeVersion").get<uint32_t>();
                }
                if (ComponentJson.contains("properties") && ComponentJson.at("properties").is_object())
                {
                    ComponentDocument.Properties = ComponentJson.at("properties");
                }
                else
                {
                    ComponentDocument.Properties = nlohmann::json::object();
                }

                ReflectionDiagnostic Applied = ReflectionJson::DeserializeInto(ComponentInstance, ComponentDocument, true);
                if (!Applied.bOk)
                {
                    Memory->DestroyObject(ComponentInstance);
                    Result.Error = Applied.Message;
                    DestroyPartialScene(CreatedScene, Memory);
                    return Result;
                }

                if (ComponentJson.contains("bEnabled") && ComponentJson.at("bEnabled").is_boolean())
                {
                    ComponentInstance->SetEnabled(ComponentJson.at("bEnabled").get<bool>());
                }

                if (ObjectInstance->AddExistingComponent(ComponentInstance, false) == nullptr)
                {
                    Memory->DestroyObject(ComponentInstance);
                    Result.Error = "Failed to attach component '" + ComponentId + "'";
                    DestroyPartialScene(CreatedScene, Memory);
                    return Result;
                }
                DeferredCreate.push_back(ComponentInstance);
            }
        }
    }

    if (DetectParentCycles(ParentById, Result.Error))
    {
        DestroyPartialScene(CreatedScene, Memory);
        return Result;
    }

    for (const auto& Pair : ParentById)
    {
        const auto ChildIterator = ObjectById.find(Pair.first);
        const auto ParentIterator = ObjectById.find(Pair.second);
        if (ChildIterator == ObjectById.end())
        {
            Result.Error = "Missing child object for parent link '" + Pair.first + "'";
            DestroyPartialScene(CreatedScene, Memory);
            return Result;
        }
        if (ParentIterator == ObjectById.end())
        {
            Result.Error = "Missing parent reference '" + Pair.second + "' for object '" + Pair.first + "'";
            DestroyPartialScene(CreatedScene, Memory);
            return Result;
        }
        if (!ChildIterator->second->SetParent(ParentIterator->second))
        {
            Result.Error = "Failed to apply parent '" + Pair.second + "' to '" + Pair.first + "'";
            DestroyPartialScene(CreatedScene, Memory);
            return Result;
        }
    }

    for (Component* ComponentInstance : DeferredCreate)
    {
        ComponentInstance->OnCreate();
    }

    CreatedScene->SetLoaded(true);
    OutScene = CreatedScene;
    Result.bOk = true;
    return Result;
}

SceneSerializeResult SceneSerializer::DeserializeFromFile(
    const std::filesystem::path& AbsolutePath,
    Scene*& OutScene)
{
    SceneSerializeResult Result{};
    OutScene = nullptr;

    std::error_code Error;
    if (!std::filesystem::exists(AbsolutePath, Error))
    {
        Result.Error = "Scene file not found";
        return Result;
    }

    std::ifstream Input(AbsolutePath, std::ios::binary);
    if (!Input)
    {
        Result.Error = "Failed to open scene file";
        return Result;
    }

    const std::string JsonText((std::istreambuf_iterator<char>(Input)), std::istreambuf_iterator<char>());
    return DeserializeFromJson(JsonText, OutScene);
}

namespace
{
void CollectSubtreeDepthFirst(GameObject* RootObject, std::vector<GameObject*>& OutObjects)
{
    if (RootObject == nullptr)
    {
        return;
    }
    OutObjects.push_back(RootObject);
    for (GameObject* Child : RootObject->GetChildren())
    {
        CollectSubtreeDepthFirst(Child, OutObjects);
    }
}

void DestroyCreatedSubtreeObjects(Scene& TargetScene, const std::vector<GameObject*>& CreatedObjects)
{
    std::vector<GameObject*> RootsToDestroy;
    for (GameObject* Created : CreatedObjects)
    {
        if (Created == nullptr)
        {
            continue;
        }
        bool bParentAlsoCreated = false;
        if (Created->GetParent() != nullptr)
        {
            for (GameObject* Candidate : CreatedObjects)
            {
                if (Candidate == Created->GetParent())
                {
                    bParentAlsoCreated = true;
                    break;
                }
            }
        }
        if (!bParentAlsoCreated)
        {
            RootsToDestroy.push_back(Created);
        }
    }

    for (GameObject* RootObject : RootsToDestroy)
    {
        if (TargetScene.FindByHandle(RootObject->GetObjectHandle()) == RootObject)
        {
            TargetScene.DestroyGameObject(RootObject);
        }
    }
}

bool TrySerializeObjectEntry(
    const GameObject* ObjectInstance,
    const std::unordered_map<const GameObject*, std::string>& IdByObject,
    nlohmann::json& OutObjectJson,
    std::string& OutError)
{
    OutObjectJson = nlohmann::json::object();
    OutObjectJson["id"] = IdByObject.at(ObjectInstance);
    OutObjectJson["name"] = ObjectInstance->GetName();
    OutObjectJson["bActive"] = ObjectInstance->IsActive();
    OutObjectJson["bVisual"] = ObjectInstance->IsVisual();
    OutObjectJson["transform"] = TransformToJson(ObjectInstance->GetTransform());

    if (ObjectInstance->GetParent() != nullptr)
    {
        const auto ParentIterator = IdByObject.find(ObjectInstance->GetParent());
        if (ParentIterator == IdByObject.end())
        {
            OutObjectJson["parent"] = nullptr;
        }
        else
        {
            OutObjectJson["parent"] = ParentIterator->second;
        }
    }
    else
    {
        OutObjectJson["parent"] = nullptr;
    }

    OutObjectJson["components"] = nlohmann::json::array();
    const std::vector<Component*>& Components = ObjectInstance->GetAllComponents();
    for (size_t ComponentIndex = 0; ComponentIndex < Components.size(); ++ComponentIndex)
    {
        Component* ComponentInstance = Components[ComponentIndex];
        if (ComponentInstance == nullptr)
        {
            OutError = "GameObject contains a null Component";
            return false;
        }
        if (ComponentInstance->GetClass() == nullptr)
        {
            OutError = "Component '" + ComponentInstance->GetName() + "' has no Class (not reflected)";
            return false;
        }

        ReflectionJsonDocument ComponentDocument;
        ReflectionDiagnostic Serialized = ReflectionJson::SerializeObject(ComponentInstance, ComponentDocument);
        if (!Serialized.bOk)
        {
            OutError = Serialized.Message;
            return false;
        }

        nlohmann::json ComponentJson = nlohmann::json::object();
        ComponentJson["id"] = ComponentInstance->GetPersistentId();
        ComponentJson["type"] = ComponentDocument.Type.Value;
        ComponentJson["typeVersion"] = ComponentDocument.TypeVersion;
        ComponentJson["bEnabled"] = ComponentInstance->IsEnabled();
        ComponentJson["properties"] = std::move(ComponentDocument.Properties);
        OutObjectJson["components"].push_back(std::move(ComponentJson));
    }

    return true;
}
}

SceneSerializeResult SceneSerializer::SerializeSubtreeToJson(const GameObject& RootObject, std::string& OutJsonText)
{
    SceneSerializeResult Result{};
    std::vector<GameObject*> Subtree;
    CollectSubtreeDepthFirst(const_cast<GameObject*>(&RootObject), Subtree);
    if (Subtree.empty())
    {
        Result.Error = "Subtree is empty";
        return Result;
    }

    std::unordered_map<const GameObject*, std::string> IdByObject;
    IdByObject.reserve(Subtree.size());
    for (size_t Index = 0; Index < Subtree.size(); ++Index)
    {
        IdByObject.emplace(Subtree[Index], Subtree[Index]->GetPersistentId());
    }

    nlohmann::json Root = nlohmann::json::object();
    Root["objects"] = nlohmann::json::array();
    for (size_t ObjectIndex = 0; ObjectIndex < Subtree.size(); ++ObjectIndex)
    {
        nlohmann::json ObjectJson;
        if (!TrySerializeObjectEntry(Subtree[ObjectIndex], IdByObject, ObjectJson, Result.Error))
        {
            return Result;
        }
        Root["objects"].push_back(std::move(ObjectJson));
    }

    OutJsonText = Root.dump(2);
    Result.bOk = true;
    return Result;
}

SceneSerializeResult SceneSerializer::DeserializeSubtreeFromJson(
    Scene& TargetScene,
    const std::string& JsonText,
    GameObject* OptionalParent,
    GameObject*& OutRestoredRoot)
{
    SceneSerializeResult Result{};
    OutRestoredRoot = nullptr;

    if (OptionalParent != nullptr && OptionalParent->GetScene() != &TargetScene)
    {
        Result.Error = "Optional parent does not belong to target Scene";
        return Result;
    }

    nlohmann::json Root;
    try
    {
        Root = nlohmann::json::parse(JsonText);
    }
    catch (const std::exception& Exception)
    {
        Result.Error = std::string("Failed to parse subtree JSON: ") + Exception.what();
        return Result;
    }

    if (!Root.is_object() || !Root.contains("objects") || !Root.at("objects").is_array())
    {
        Result.Error = "Subtree JSON requires objects array";
        return Result;
    }

    MemorySubsystem* Memory = MemorySubsystem::Get();
    ReflectionSubsystem& Reflection = ReflectionSubsystem::Get();
    if (Memory == nullptr || !Reflection.IsInitialized())
    {
        Result.Error = "Memory or Reflection is not ready";
        return Result;
    }

    std::unordered_map<std::string, GameObject*> ObjectById;
    std::unordered_map<std::string, std::string> ParentById;
    std::unordered_set<std::string> SeenObjectIds;
    std::unordered_set<std::string> SeenComponentIds;
    for (GameObject* ExistingObject : TargetScene.GetAllObjects())
    {
        for (Component* ExistingComponent : ExistingObject->GetAllComponents())
        {
            SeenComponentIds.insert(ExistingComponent->GetPersistentId());
        }
    }
    std::vector<Component*> DeferredCreate;
    std::vector<GameObject*> CreatedObjects;
    std::string RootDocumentId;

    for (const nlohmann::json& ObjectJson : Root.at("objects"))
    {
        if (!ObjectJson.is_object()
            || !ObjectJson.contains("id")
            || !ObjectJson.at("id").is_string())
        {
            Result.Error = "Subtree object requires string id";
            DestroyCreatedSubtreeObjects(TargetScene, CreatedObjects);
            return Result;
        }

        const std::string ObjectId = ObjectJson.at("id").get<std::string>();
        if (ObjectId.empty() || TargetScene.FindByPersistentId(ObjectId) != nullptr || !SeenObjectIds.insert(ObjectId).second)
        {
            Result.Error = "Duplicate or empty subtree object id";
            DestroyCreatedSubtreeObjects(TargetScene, CreatedObjects);
            return Result;
        }
        if (RootDocumentId.empty())
        {
            RootDocumentId = ObjectId;
        }

        std::string ObjectName = ObjectId;
        if (ObjectJson.contains("name") && ObjectJson.at("name").is_string())
        {
            ObjectName = ObjectJson.at("name").get<std::string>();
        }

        GameObject* ObjectInstance = TargetScene.CreateGameObject(ObjectName);
        if (ObjectInstance == nullptr)
        {
            Result.Error = "Failed to create GameObject for subtree restore";
            DestroyCreatedSubtreeObjects(TargetScene, CreatedObjects);
            return Result;
        }
        CreatedObjects.push_back(ObjectInstance);
        ObjectInstance->PersistentId = ObjectId;
        ObjectById.emplace(ObjectId, ObjectInstance);

        if (ObjectJson.contains("bActive") && ObjectJson.at("bActive").is_boolean())
        {
            ObjectInstance->SetActive(ObjectJson.at("bActive").get<bool>());
        }
        if (ObjectJson.contains("bVisual") && ObjectJson.at("bVisual").is_boolean())
        {
            ObjectInstance->SetVisual(ObjectJson.at("bVisual").get<bool>());
        }
        if (ObjectJson.contains("transform"))
        {
            Transform LoadedTransform = ObjectInstance->GetTransform();
            if (!TryReadTransform(ObjectJson.at("transform"), LoadedTransform, Result.Error))
            {
                DestroyCreatedSubtreeObjects(TargetScene, CreatedObjects);
                return Result;
            }
            ObjectInstance->SetTransform(LoadedTransform);
        }

        if (ObjectJson.contains("parent") && !ObjectJson.at("parent").is_null())
        {
            if (!ObjectJson.at("parent").is_string())
            {
                Result.Error = "parent must be a string id or null";
                DestroyCreatedSubtreeObjects(TargetScene, CreatedObjects);
                return Result;
            }
            ParentById.emplace(ObjectId, ObjectJson.at("parent").get<std::string>());
        }

        if (!ObjectJson.contains("components") || !ObjectJson.at("components").is_array())
        {
            continue;
        }

        for (const nlohmann::json& ComponentJson : ObjectJson.at("components"))
        {
            if (!ComponentJson.is_object()
                || !ComponentJson.contains("id")
                || !ComponentJson.at("id").is_string()
                || !ComponentJson.contains("type")
                || !ComponentJson.at("type").is_string())
            {
                Result.Error = "Invalid component entry in subtree";
                DestroyCreatedSubtreeObjects(TargetScene, CreatedObjects);
                return Result;
            }

            const std::string ComponentId = ComponentJson.at("id").get<std::string>();
            if (ComponentId.empty() || !SeenComponentIds.insert(ComponentId).second)
            {
                Result.Error = "Duplicate or empty component id in subtree";
                DestroyCreatedSubtreeObjects(TargetScene, CreatedObjects);
                return Result;
            }

            const TypeId ComponentType{ComponentJson.at("type").get<std::string>()};
            Object* CreatedObject = Reflection.CreateInstance(ComponentType);
            Component* ComponentInstance = dynamic_cast<Component*>(CreatedObject);
            if (ComponentInstance == nullptr)
            {
                if (CreatedObject != nullptr)
                {
                    Memory->DestroyObject(CreatedObject);
                }
                Result.Error = "Unknown or unsupported component type '" + ComponentType.Value + "'";
                DestroyCreatedSubtreeObjects(TargetScene, CreatedObjects);
                return Result;
            }

            ComponentInstance->PersistentId = ComponentId;
            ReflectionJsonDocument ComponentDocument;
            ComponentDocument.Type = ComponentType;
            ComponentDocument.TypeVersion = 1;
            if (ComponentJson.contains("typeVersion") && ComponentJson.at("typeVersion").is_number_unsigned())
            {
                ComponentDocument.TypeVersion = ComponentJson.at("typeVersion").get<uint32_t>();
            }
            if (ComponentJson.contains("properties") && ComponentJson.at("properties").is_object())
            {
                ComponentDocument.Properties = ComponentJson.at("properties");
            }
            else
            {
                ComponentDocument.Properties = nlohmann::json::object();
            }

            ReflectionDiagnostic Applied = ReflectionJson::DeserializeInto(ComponentInstance, ComponentDocument, true);
            if (!Applied.bOk)
            {
                Memory->DestroyObject(ComponentInstance);
                Result.Error = Applied.Message;
                DestroyCreatedSubtreeObjects(TargetScene, CreatedObjects);
                return Result;
            }

            if (ComponentJson.contains("bEnabled") && ComponentJson.at("bEnabled").is_boolean())
            {
                ComponentInstance->SetEnabled(ComponentJson.at("bEnabled").get<bool>());
            }

            if (ObjectInstance->AddExistingComponent(ComponentInstance, false) == nullptr)
            {
                Memory->DestroyObject(ComponentInstance);
                Result.Error = "Failed to attach restored component";
                DestroyCreatedSubtreeObjects(TargetScene, CreatedObjects);
                return Result;
            }
            DeferredCreate.push_back(ComponentInstance);
        }
    }

    if (DetectParentCycles(ParentById, Result.Error))
    {
        DestroyCreatedSubtreeObjects(TargetScene, CreatedObjects);
        return Result;
    }

    for (const auto& Pair : ParentById)
    {
        const auto ChildIterator = ObjectById.find(Pair.first);
        const auto ParentIterator = ObjectById.find(Pair.second);
        if (ChildIterator == ObjectById.end() || ParentIterator == ObjectById.end())
        {
            Result.Error = "Missing parent link while restoring subtree";
            DestroyCreatedSubtreeObjects(TargetScene, CreatedObjects);
            return Result;
        }
        if (!ChildIterator->second->SetParent(ParentIterator->second))
        {
            Result.Error = "Failed to restore parent link in subtree";
            DestroyCreatedSubtreeObjects(TargetScene, CreatedObjects);
            return Result;
        }
    }

    GameObject* RestoredRoot = ObjectById[RootDocumentId];
    if (OptionalParent != nullptr)
    {
        if (!RestoredRoot->SetParent(OptionalParent))
        {
            Result.Error = "Failed to attach restored subtree to parent";
            DestroyCreatedSubtreeObjects(TargetScene, CreatedObjects);
            return Result;
        }
    }

    for (Component* ComponentInstance : DeferredCreate)
    {
        ComponentInstance->OnCreate();
    }

    OutRestoredRoot = RestoredRoot;
    Result.bOk = true;
    return Result;
}

SceneSerializeResult SceneSerializer::SerializeComponentToJson(
    const Component& ComponentInstance,
    std::string& OutJsonText)
{
    SceneSerializeResult Result{};
    if (ComponentInstance.GetClass() == nullptr)
    {
        Result.Error = "Component has no Class";
        return Result;
    }

    ReflectionJsonDocument ComponentDocument;
    ReflectionDiagnostic Serialized = ReflectionJson::SerializeObject(
        const_cast<Component*>(&ComponentInstance),
        ComponentDocument);
    if (!Serialized.bOk)
    {
        Result.Error = Serialized.Message;
        return Result;
    }

    nlohmann::json ComponentJson = nlohmann::json::object();
    ComponentJson["id"] = ComponentInstance.GetPersistentId();
    ComponentJson["type"] = ComponentDocument.Type.Value;
    ComponentJson["typeVersion"] = ComponentDocument.TypeVersion;
    ComponentJson["bEnabled"] = ComponentInstance.IsEnabled();
    ComponentJson["properties"] = std::move(ComponentDocument.Properties);
    OutJsonText = ComponentJson.dump(2);
    Result.bOk = true;
    return Result;
}

SceneSerializeResult SceneSerializer::DeserializeComponentFromJson(
    GameObject& Owner,
    const std::string& JsonText,
    Component*& OutComponent)
{
    SceneSerializeResult Result{};
    OutComponent = nullptr;

    nlohmann::json ComponentJson;
    try
    {
        ComponentJson = nlohmann::json::parse(JsonText);
    }
    catch (const std::exception& Exception)
    {
        Result.Error = std::string("Failed to parse component JSON: ") + Exception.what();
        return Result;
    }

    if (!ComponentJson.is_object()
        || !ComponentJson.contains("type")
        || !ComponentJson.at("type").is_string())
    {
        Result.Error = "Component JSON requires string type";
        return Result;
    }

    MemorySubsystem* Memory = MemorySubsystem::Get();
    ReflectionSubsystem& Reflection = ReflectionSubsystem::Get();
    if (Memory == nullptr || !Reflection.IsInitialized())
    {
        Result.Error = "Memory or Reflection is not ready";
        return Result;
    }

    const TypeId ComponentType{ComponentJson.at("type").get<std::string>()};
    Object* CreatedObject = Reflection.CreateInstance(ComponentType);
    Component* ComponentInstance = dynamic_cast<Component*>(CreatedObject);
    if (ComponentInstance == nullptr)
    {
        if (CreatedObject != nullptr)
        {
            Memory->DestroyObject(CreatedObject);
        }
        Result.Error = "Unknown or unsupported component type '" + ComponentType.Value + "'";
        return Result;
    }

    if (ComponentJson.contains("id") && ComponentJson.at("id").is_string())
    {
        ComponentInstance->PersistentId = ComponentJson.at("id").get<std::string>();
        for (GameObject* ExistingObject : Owner.GetScene()->GetAllObjects())
        {
            for (Component* ExistingComponent : ExistingObject->GetAllComponents())
            {
                if (ExistingComponent->GetPersistentId() == ComponentInstance->GetPersistentId())
                {
                    Memory->DestroyObject(ComponentInstance);
                    Result.Error = "Component persistent id already exists";
                    return Result;
                }
            }
        }
    }
    ReflectionJsonDocument ComponentDocument;
    ComponentDocument.Type = ComponentType;
    ComponentDocument.TypeVersion = 1;
    if (ComponentJson.contains("typeVersion") && ComponentJson.at("typeVersion").is_number_unsigned())
    {
        ComponentDocument.TypeVersion = ComponentJson.at("typeVersion").get<uint32_t>();
    }
    if (ComponentJson.contains("properties") && ComponentJson.at("properties").is_object())
    {
        ComponentDocument.Properties = ComponentJson.at("properties");
    }
    else
    {
        ComponentDocument.Properties = nlohmann::json::object();
    }

    ReflectionDiagnostic Applied = ReflectionJson::DeserializeInto(ComponentInstance, ComponentDocument, true);
    if (!Applied.bOk)
    {
        Memory->DestroyObject(ComponentInstance);
        Result.Error = Applied.Message;
        return Result;
    }

    if (ComponentJson.contains("bEnabled") && ComponentJson.at("bEnabled").is_boolean())
    {
        ComponentInstance->SetEnabled(ComponentJson.at("bEnabled").get<bool>());
    }

    if (Owner.AddExistingComponent(ComponentInstance, true) == nullptr)
    {
        Memory->DestroyObject(ComponentInstance);
        Result.Error = "Failed to attach component";
        return Result;
    }

    OutComponent = ComponentInstance;
    Result.bOk = true;
    return Result;
}
