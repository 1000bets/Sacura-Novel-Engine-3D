#pragma once
#include "Engine.h"
#include "Documents/SceneWorkspace.h"
#include "Project/ProjectSession.h"
#include "Documents/StoryGraphDocument.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <map>
#include <mutex>
#include "Documents/StaticMeshDocument.h"
#include "Documents/MaterialEditingSession.h"

// Single owner on Game thread. The wire contains persistent IDs, never pointers.
class MeshRendererComponent;
class CameraComponent;
class NativeBridge {
public:
    using Json = nlohmann::json;
    NativeBridge(Engine& Runtime, SceneWorkspace& Workspace, ProjectSession& Project,
        const std::filesystem::path& Contract, std::function<void()> ResetInput);
    ~NativeBridge();
    void TickPreview();
    Scene* GetPreviewScene() const { return PreviewScene; }
    MeshRendererComponent* GetPreviewMesh() const { return PreviewMesh; }
    const StaticMeshDefaults* GetPreviewCollision() const;
    bool CanEditPreviewCollision() const;
    int GetSelectedCollision() const { return SelectedCollision; }
    void SelectCollision(int Index) { SelectedCollision = Index; }
    uint64_t GetPreviewRevision() const;
    bool CommitPreviewCollision(const Json& Value, uint64_t ExpectedRevision);
    Json Execute(const Json& Request);
    Json Poll(bool Force = false);
    Json Context() const;
    void ScanAssets();
    std::function<Json(float, float)> PickViewportMesh;
private:
    struct AssetDocumentState
    {
        std::unique_ptr<StaticMeshDocument> Mesh;
        std::unique_ptr<MaterialEditingSession> Material;
        uint64_t Revision = 1;
        uint64_t RequestedPreviewRevision = 0;
        uint64_t ImageRevision = 0;
        std::shared_ptr<MaterialImageRequest> Image;
        Json TextureSnapshot;
        std::string ImagePng;
        std::string ImageDiagnostic;
        float ImageTime = -1.f;
    };
    std::map<std::string, AssetDocumentState> AssetDocuments;
    std::string PreviewDocument;
    Scene* PreviewScene = nullptr;
    MeshRendererComponent* PreviewMesh = nullptr;
    CameraComponent* PreviewCamera = nullptr;
    int SelectedCollision = -1;
    bool bShowCollision = false;
    std::string PreviewShape = "Sphere";
    Json AssetDocumentTools(const Json& Params);
    bool HasDirtyAssetDocuments() const;
    struct PreviewResult
    {
        std::mutex Mutex;
        bool bReady = false;
        std::string Encoded;
    };
    std::map<std::string, std::shared_ptr<PreviewResult>> Previews;
    Json MeshTools(const std::string& Method, const Json& Params);
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
    void CancelTransformEdit();
    std::string TransformTransaction;
    std::string TransformObjectId;
    ObjectHandle TransformWorld;
    Transform TransformBefore;
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
