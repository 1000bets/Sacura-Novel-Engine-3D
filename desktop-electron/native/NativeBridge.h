#pragma once
#include "Engine.h"
#include "Documents/SceneWorkspace.h"
#include "Project/ProjectSession.h"
#include "Documents/StoryGraphDocument.h"
#include <nlohmann/json.hpp>
#include <functional>

// Single owner on Game thread. The wire contains persistent IDs, never pointers.
class NativeBridge {
public:
    using Json = nlohmann::json;
    NativeBridge(Engine& Runtime, SceneWorkspace& Workspace, ProjectSession& Project,
        const std::filesystem::path& Contract, std::function<void()> ResetInput);
    Json Execute(const Json& Request);
    Json Poll(bool Force = false);
    Json Context() const;
    void ScanAssets();
private:
    Json Dispatch(const std::string& Method, const Json& Params);
    Json Describe();
    Json Hierarchy();
    Json Properties(Component* Instance);
    Json Components(GameObject* Object);
    Json ConvertValue(const TypeId& Type, const Json& Value, bool Decode);
    Scene* World();
    GameObject* Find(const std::string& Id);
    Component* FindComponent(const Json& Params);
    std::filesystem::path ScenePath(const std::string& Path);
    void ValidateParams(const std::string& Method, const Json& Params);
    void Commit(std::unique_ptr<EditorCommand> Command);
    void ObserveWorld();
    Json Assets();
    AssetRegistryEntry Asset(const std::string& Id);
    Json StorySnapshot();
    Json DispatchDocuments(const std::string& Method, const Json& Params);
    std::filesystem::path ContentPath(const std::string& Path);
    Engine& Runtime;
    SceneWorkspace& Workspace;
    ProjectSession& Project;
    ContentHash ProjectFingerprint;
    bool ProjectChanged() const;
    Json Contract;
    std::string Session, PreviousState;
    uint64_t Epoch = 1, Revision = 0;
    ObjectHandle LastWorld, LastEditWorld;
    StoryGraphDocument Story;
    uint64_t StoryRevision=0;
    std::string PreviousStory, PreviousAssets, AssetError, StoryError;
    std::function<void()> ResetInput;
};
