#pragma once
#include "Materials/MaterialDocumentIO.h"
#include "Assets/AssetRegistry.h"
#include "Assets/ContentHash.h"

class MaterialDocument
{
public:
    MaterialDocument();
    virtual ~MaterialDocument() = default;
    AssetDiagnostic Open(AssetRegistry& Registry, const AssetKey& Key);
    AssetDiagnostic Save(bool bOverwriteExternalChanges = false);
    AssetDiagnostic BuildResolved(ResolvedMaterial& OutMaterial) const;
    void SetDocument(nlohmann::json Document);
    void SetSource(std::string Text);
    void SetDocumentAndSource(nlohmann::json Document, std::string Text);
    bool Undo();
    bool Redo();
    bool IsDirty() const;
    bool IsReadOnly() const;
    bool IsSourceReadOnly() const;
    bool HasExternalChanges() const;
    const nlohmann::json& GetDocument() const;
    const std::string& GetSource() const;
    const AssetKey& GetAsset() const { return Asset; }
    std::string GetSourcePath() const;

protected:
    explicit MaterialDocument(AssetType Type);

private:
    struct Revision
    {
        nlohmann::json Document;
        std::string Source;
        bool operator==(const Revision& Other) const { return Document == Other.Document && Source == Other.Source; }
    };
    void AppendRevision(Revision Value);
    void UpdateSourceIdentity();
    AssetRegistry* BoundRegistry = nullptr;
    AssetType ExpectedType;
    AssetKey Asset;
    AssetKey SourceAsset;
    ContentHash DocumentFingerprint;
    ContentHash SourceFingerprint;
    std::map<std::string, ContentHash> SourceBaselines;
    std::vector<Revision> Revisions;
    size_t CurrentRevision = 0;
    Revision Saved;
    bool bLegacy = false;
};

class MaterialInstanceDocument : public MaterialDocument
{
public:
    MaterialInstanceDocument() : MaterialDocument(MaterialInstanceAssetType) {}
};
