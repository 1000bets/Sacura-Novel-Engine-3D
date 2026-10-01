#pragma once

#include "Assets/AssetRegistry.h"

class Engine;
class ProjectSession;
class SceneDocument;

class AssetOperations
{
public:
    explicit AssetOperations(Engine& InEngine);
    void SetProjectContext(ProjectSession* Session, SceneDocument* Document);
    AssetDiagnostic RenameAsset(const std::string& Source, const std::string& Destination);
    AssetDiagnostic RenameSubAsset(const AssetKey& Key, const std::string& Name);
    AssetDiagnostic DeleteAsset(const AssetKey& Key);
    AssetDiagnostic DuplicateAsset(const AssetKey& Source, const std::string& DestinationFolder);
    AssetDiagnostic CreateFolder(const std::string& Folder);
    AssetDiagnostic RenameFolder(const std::string& Source, const std::string& Destination);
    AssetDiagnostic DeleteFolder(const std::string& Folder);
    bool ResolveGameFolder(const std::string& Folder, std::filesystem::path& OutAbsolute) const;

private:
    AssetDiagnostic RelocateProjectPaths(const std::filesystem::path& Source, const std::filesystem::path& Destination);
    void RefreshDocumentPath();
    void InvalidateAsset(const AssetId& Identity);

    Engine& BoundEngine;
    ProjectSession* BoundSession = nullptr;
    SceneDocument* BoundDocument = nullptr;
};
