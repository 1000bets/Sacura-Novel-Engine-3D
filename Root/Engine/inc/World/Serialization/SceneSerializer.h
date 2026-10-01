#pragma once

#include <filesystem>
#include <string>

class Component;
class GameObject;
class Scene;

struct SceneSerializeResult
{
    bool bOk = false;
    std::string Error;
};

class SceneSerializer
{
public:
    static constexpr const char* FormatId = "sakura.scene";
    static constexpr uint32_t FormatVersion = 1;

    static SceneSerializeResult SerializeToJson(const Scene& InScene, std::string& OutJsonText);
    static SceneSerializeResult SerializeToFile(const Scene& InScene, const std::filesystem::path& AbsolutePath);

    static SceneSerializeResult DeserializeFromJson(const std::string& JsonText, Scene*& OutScene);
    static SceneSerializeResult DeserializeFromFile(const std::filesystem::path& AbsolutePath, Scene*& OutScene);

    static SceneSerializeResult SerializeSubtreeToJson(const GameObject& RootObject, std::string& OutJsonText);
    static SceneSerializeResult RemapSubtreePersistentIds(
        const std::string& JsonText,
        std::string& OutRemappedJsonText);
    static SceneSerializeResult DeserializeSubtreeFromJson(
        Scene& TargetScene,
        const std::string& JsonText,
        GameObject* OptionalParent,
        GameObject*& OutRestoredRoot);

    static SceneSerializeResult SerializeComponentToJson(const Component& ComponentInstance, std::string& OutJsonText);
    static SceneSerializeResult DeserializeComponentFromJson(
        GameObject& Owner,
        const std::string& JsonText,
        Component*& OutComponent);

    static SceneSerializeResult WriteTextFileAtomically(const std::filesystem::path& AbsolutePath, const std::string& Contents);
};
