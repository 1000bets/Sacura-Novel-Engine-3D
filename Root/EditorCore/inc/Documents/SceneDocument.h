#pragma once

#include "Core/Object/ObjectHandle.h"
#include "Assets/Guid.h"
#include <filesystem>
#include <cstdint>
#include <string>

class Engine;
class Scene;

class SceneDocument
{
public:
    void BindEngine(Engine* InEngine);
    bool AdoptScene(Scene* ExistingScene, const std::filesystem::path& AbsolutePath, std::string& OutError);

    bool Open(const std::filesystem::path& AbsolutePath, std::string& OutError);
    bool Save(std::string& OutError);
    bool SaveAs(const std::filesystem::path& AbsolutePath, std::string& OutError);
    void Close();

    bool IsOpen() const { return GetScene() != nullptr; }
    bool IsDirty() const { return CurrentRevision != SavedRevision && IsOpen(); }
    void MarkDirty();
    uint64_t GetRevision() const { return CurrentRevision; }
    void RestoreRevision(uint64_t Revision) { CurrentRevision = Revision; }
    const AssetId& GetAssetId() const { return DocumentAsset; }
    void RefreshAssetPath();

    Scene* GetScene() const;
    const std::filesystem::path& GetDocumentPath() const { return DocumentPath; }
    const std::string& GetLastError() const { return LastError; }

private:
    bool ReplaceBoundScene(Scene* NewScene, const std::filesystem::path& Path, std::string& OutError);
    void BindDocumentAsset();

    Engine* BoundEngine = nullptr;
    ObjectHandle BoundScene;
    std::filesystem::path DocumentPath;
    AssetId DocumentAsset;
    uint64_t CurrentRevision = 0;
    uint64_t SavedRevision = 0;
    uint64_t NextRevision = 1;
    std::string LastError;
};
