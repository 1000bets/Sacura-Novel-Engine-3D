#pragma once

#include "Assets/Loaders/IAssetLoader.h"
#include "Assets/Resources/ModelResource.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

class ModelLoader : public IAssetLoader
{
public:
    AssetType GetAssetType() const override { return AssetType::Model; }
    const char* GetLoaderName() const override { return "ModelLoader"; }

    std::shared_ptr<const void> Load(const AssetLoadContext& Context, AssetDiagnostic& OutDiagnostic) override;

    std::shared_ptr<const ModelDocument> LoadOrGetSharedDocument(
        const std::filesystem::path& AbsolutePath,
        const ContentHash& ExpectedFingerprint,
        AssetDiagnostic& OutDiagnostic);

    void ClearSharedDocuments();

private:
    struct SharedDocumentEntry
    {
        ContentHash Fingerprint{};
        std::shared_ptr<const ModelDocument> Document;
    };

    std::mutex SharedDocumentsMutex;
    std::unordered_map<std::string, SharedDocumentEntry> SharedDocuments;
};
