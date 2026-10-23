#pragma once

#include "Documents/MaterialDocument.h"
#include "Materials/DynamicMaterialInstance.h"
#include "Materials/MaterialCompilation.h"
#include <atomic>

class AssetGpuUploader;

enum class MaterialEditOperation { Document, Source, Undo, Redo, Reload, Save, Compile };

class MaterialEditingSession
{
public:
    explicit MaterialEditingSession(AssetRegistry& Registry);
    AssetDiagnostic Open(const AssetKey& Asset);
    AssetDiagnostic Execute(MaterialEditOperation Operation, const nlohmann::json& Value = {},
        uint64_t ExpectedRevision = 0);
    void Poll(AssetGpuUploader& Uploader);
    MaterialDocument& GetDocument() { return *Document; }
    const MaterialDocument& GetDocument() const { return *Document; }
    uint64_t GetRevision() const { return Revision; }
    uint64_t GetPreviewRevision() const { return PreviewRevision; }
    bool IsInstance() const { return bInstance; }
    bool IsPreparing() const { return Preparation != nullptr || Compilation != nullptr; }
    const std::string& GetDiagnostic() const { return Diagnostic; }
    std::shared_ptr<DynamicMaterialInstance> GetPreview() const { return Preview; }

private:
    struct PreparedDraft
    {
        ResolvedMaterial Material;
        AssetDiagnostic Diagnostic;
        std::atomic<bool> bReady{false};
    };
    AssetDiagnostic ResolveCandidate(const nlohmann::json& Candidate, ResolvedMaterial& Material) const;
    AssetRegistry& Registry;
    std::unique_ptr<MaterialDocument> Document;
    std::shared_ptr<PreparedDraft> Preparation;
    std::shared_ptr<MaterialCompilation> Compilation;
    ResolvedMaterial CompiledMaterial;
    std::shared_ptr<DynamicMaterialInstance> Preview;
    uint64_t Revision = 1;
    uint64_t RequestedRevision = 0;
    uint64_t PreviewRevision = 0;
    bool bInstance = false;
    std::string Diagnostic;
};
