#include "Documents/MaterialDocument.h"
#include "Core/IO/AtomicFileWriter.h"
#include <fstream>

MaterialDocument::MaterialDocument() : ExpectedType(MaterialAssetType) {}
MaterialDocument::MaterialDocument(AssetType Type) : ExpectedType(std::move(Type)) {}

AssetDiagnostic MaterialDocument::Open(AssetRegistry& Registry, const AssetKey& Key)
{
    AssetRegistryEntry Entry;
    if (!Registry.TryResolveKey(Key, Entry) || Entry.Metadata.Type != ExpectedType)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::TypeMismatch, "MaterialDocument", "Unexpected material document type", Key);
    }
    Revision Initial;
    auto Diagnostic = MaterialDocumentIO::Read(Entry.AbsolutePath, Initial.Document);
    if (Diagnostic.HasError())
    {
        return Diagnostic;
    }
    ResolvedMaterial Material;
    Diagnostic = MaterialDocumentIO::Resolve(Registry, Key, Material);
    AssetKey Shader = Material.Definition.Source;
    if (Diagnostic.HasError() && ExpectedType == MaterialAssetType && Initial.Document.value("schemaVersion", 0) == 2)
    {
        AssetDiagnostic SourceDiagnostic;
        AssetMetadataIO::TryAssetRefFromJson(Initial.Document.value("source", nlohmann::json()), Shader, SourceDiagnostic);
    }
    else if (Diagnostic.HasError() && ExpectedType == MaterialAssetType)
    {
        return Diagnostic;
    }
    AssetRegistryEntry Source;
    ContentHash SourceHash;
    if (Shader.IsValid() && Registry.TryResolveKey(Shader, Source))
    {
        std::ifstream Input(Source.AbsolutePath);
        Initial.Source.assign(std::istreambuf_iterator<char>(Input), std::istreambuf_iterator<char>());
        ContentHash::TryHashFile(Source.AbsolutePath, SourceHash, Diagnostic);
    }
    else
    {
        Initial.Source = Material.Definition.PreparedSource;
    }
    const bool bOldFormat = ExpectedType == MaterialAssetType && Initial.Document.at("schemaVersion") == 1;
    if (bOldFormat)
    {
        Initial.Document = MaterialDocumentIO::MakeMaterialDocument(Material);
    }
    ContentHash Hash;
    if (!ContentHash::TryHashFile(Entry.AbsolutePath, Hash, Diagnostic))
    {
        return Diagnostic;
    }
    BoundRegistry = &Registry;
    Asset = Key;
    SourceAsset = Shader;
    DocumentFingerprint = Hash;
    SourceFingerprint = SourceHash;
    SourceBaselines.clear();
    SourceBaselines[Shader.Asset.ToString()] = SourceHash;
    Revisions = {Initial};
    Saved = std::move(Initial);
    CurrentRevision = 0;
    bLegacy = bOldFormat;
    return AssetDiagnostic::Ok();
}

const nlohmann::json& MaterialDocument::GetDocument() const { return Revisions.at(CurrentRevision).Document; }
const std::string& MaterialDocument::GetSource() const { return Revisions.at(CurrentRevision).Source; }
bool MaterialDocument::IsDirty() const { return !Revisions.empty() && !(Revisions[CurrentRevision] == Saved); }

bool MaterialDocument::IsReadOnly() const
{
    AssetRegistryEntry Entry;
    return !BoundRegistry || !BoundRegistry->TryResolveKey(Asset, Entry) || Entry.Mount == AssetMount::Engine;
}

std::string MaterialDocument::GetSourcePath() const
{
    AssetRegistryEntry Entry;
    if (BoundRegistry && BoundRegistry->TryResolveKey(SourceAsset, Entry))
    {
        return Entry.AbsolutePath;
    }
    return {};
}

bool MaterialDocument::HasExternalChanges() const
{
    AssetRegistryEntry Entry;
    AssetDiagnostic Diagnostic;
    ContentHash Current;
    if (!BoundRegistry || !BoundRegistry->TryResolveKey(Asset, Entry)
        || !ContentHash::TryHashFile(Entry.AbsolutePath, Current, Diagnostic) || Current != DocumentFingerprint)
    {
        return true;
    }
    if (ExpectedType == MaterialAssetType && SourceAsset.IsValid())
    {
        if (!BoundRegistry->TryResolveKey(SourceAsset, Entry)
            || !ContentHash::TryHashFile(Entry.AbsolutePath, Current, Diagnostic) || Current != SourceFingerprint)
        {
            return true;
        }
    }
    return false;
}

void MaterialDocument::AppendRevision(Revision Value)
{
    if (IsReadOnly() || Value == Revisions[CurrentRevision])
    {
        return;
    }
    if (ExpectedType == MaterialAssetType)
    {
        AssetKey NextSource;
        AssetDiagnostic Diagnostic;
        AssetMetadataIO::TryAssetRefFromJson(Value.Document.value("source", nlohmann::json()), NextSource, Diagnostic);
        if (NextSource != SourceAsset)
        {
            AssetRegistryEntry Entry;
            if (BoundRegistry->TryResolveKey(NextSource, Entry) && Entry.Metadata.Type == ShaderSourceAssetType)
            {
                std::ifstream Input(Entry.AbsolutePath);
                Value.Source.assign(std::istreambuf_iterator<char>(Input), std::istreambuf_iterator<char>());
                ContentHash Hash;
                ContentHash::TryHashFile(Entry.AbsolutePath, Hash, Diagnostic);
                SourceBaselines[NextSource.Asset.ToString()] = Hash;
            }
        }
    }
    Revisions.resize(CurrentRevision + 1);
    Revisions.push_back(std::move(Value));
    ++CurrentRevision;
    UpdateSourceIdentity();
}

void MaterialDocument::SetDocument(nlohmann::json Document)
{
    Revision Next = Revisions.at(CurrentRevision);
    Next.Document = std::move(Document);
    AppendRevision(std::move(Next));
}

void MaterialDocument::SetSource(std::string Text)
{
    Revision Next = Revisions.at(CurrentRevision);
    Next.Source = std::move(Text);
    AppendRevision(std::move(Next));
}

bool MaterialDocument::Undo()
{
    if (CurrentRevision == 0 || IsReadOnly())
    {
        return false;
    }
    --CurrentRevision;
    UpdateSourceIdentity();
    return true;
}

bool MaterialDocument::Redo()
{
    if (CurrentRevision + 1 >= Revisions.size() || IsReadOnly())
    {
        return false;
    }
    ++CurrentRevision;
    UpdateSourceIdentity();
    return true;
}

AssetDiagnostic MaterialDocument::BuildResolved(ResolvedMaterial& OutMaterial) const
{
    std::map<std::string, nlohmann::json> Documents = {{Asset.Asset.ToString(), GetDocument()}};
    std::map<std::string, std::string> Sources;
    if (ExpectedType == MaterialAssetType)
    {
        if (!SourceAsset.IsValid())
        {
            Sources[Asset.Asset.ToString()] = GetSource();
        }
        else
        {
            Sources[SourceAsset.Asset.ToString()] = GetSource();
        }
    }
    return MaterialDocumentIO::Resolve(*BoundRegistry, Asset, OutMaterial, Documents, Sources);
}

AssetDiagnostic MaterialDocument::Save(bool bOverwriteExternalChanges)
{
    if (IsReadOnly())
    {
        return AssetDiagnostic::Fail(AssetErrorCode::UnsupportedFeature, "MaterialDocument", "Engine Content is read-only");
    }
    if (!bOverwriteExternalChanges && HasExternalChanges())
    {
        return AssetDiagnostic::Fail(AssetErrorCode::AssetChanged, "MaterialDocument", "Material or source changed externally; reload or explicitly overwrite");
    }
    ResolvedMaterial Material;
    auto Diagnostic = BuildResolved(Material);
    if (Diagnostic.HasError())
    {
        return Diagnostic;
    }
    AssetRegistryEntry Entry;
    BoundRegistry->TryResolveKey(Asset, Entry);
    Revision Next = Revisions.at(CurrentRevision);
    AssetRegistryEntry Source;
    bool bCreatedSource = false;
    if (bLegacy && !SourceAsset.IsValid())
    {
        const auto SourcePath = std::filesystem::path(Entry.AbsolutePath).parent_path()
            / (std::filesystem::path(Entry.AbsolutePath).stem().string() + ".Source.hlsl");
        if (std::filesystem::exists(SourcePath))
        {
            return AssetDiagnostic::Fail(AssetErrorCode::ImportConflict, "MaterialDocument", "Legacy shader destination already exists", Asset, SourcePath.string());
        }
        Source.AbsolutePath = SourcePath.string();
        Source.RelativePath = std::filesystem::relative(SourcePath, BoundRegistry->GetGameContentRoot()).generic_string();
        Source.Metadata.Guid = Guid::Generate();
        Source.Metadata.Type = ShaderSourceAssetType;
        SourceAsset = AssetKey{Source.Metadata.Guid, {}};
        Next.Document["source"] = AssetMetadataIO::AssetRefToJson(SourceAsset);
        bCreatedSource = true;
    }
    else if (SourceAsset.IsValid())
    {
        BoundRegistry->TryResolveKey(SourceAsset, Source);
    }
    std::string PreviousSource;
    if (!bCreatedSource && ExpectedType == MaterialAssetType && !Source.AbsolutePath.empty())
    {
        std::ifstream Input(Source.AbsolutePath);
        PreviousSource.assign(std::istreambuf_iterator<char>(Input), std::istreambuf_iterator<char>());
    }
    const bool bWriteSource = ExpectedType == MaterialAssetType && (bCreatedSource || Next.Source != PreviousSource);
    if (bWriteSource && Source.Mount == AssetMount::Engine)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::UnsupportedFeature, "MaterialDocument", "Engine shader source is read-only");
    }
    std::string WriteError;
    if (bWriteSource && !AtomicFileWriter::WriteText(Source.AbsolutePath, Next.Source, WriteError))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "MaterialDocument", WriteError);
    }
    if (bCreatedSource)
    {
        ContentHash::TryHashFile(Source.AbsolutePath, Source.Metadata.SourceFingerprint, Diagnostic);
        if (!AssetMetadataIO::TrySaveToFile(Source.AbsolutePath + ".meta", Source.Metadata, Diagnostic))
        {
            std::filesystem::remove(Source.AbsolutePath);
            SourceAsset = {};
            return Diagnostic;
        }
    }
    Diagnostic = MaterialDocumentIO::Save(Entry.AbsolutePath, Next.Document);
    if (Diagnostic.HasError())
    {
        if (bCreatedSource)
        {
            std::filesystem::remove(Source.AbsolutePath);
            std::filesystem::remove(Source.AbsolutePath + ".meta");
            SourceAsset = {};
        }
        else if (bWriteSource)
        {
            AtomicFileWriter::WriteText(Source.AbsolutePath, PreviousSource, WriteError);
        }
        return Diagnostic;
    }
    if (bCreatedSource)
    {
        BoundRegistry->RegisterExistingAsset(Source.RelativePath, Source.Metadata);
        bLegacy = false;
        for (auto& Revision : Revisions)
        {
            Revision.Document["source"] = Next.Document["source"];
        }
        Revisions[CurrentRevision] = Next;
    }
    ContentHash::TryHashFile(Entry.AbsolutePath, DocumentFingerprint, Diagnostic);
    if (SourceAsset.IsValid())
    {
        ContentHash::TryHashFile(Source.AbsolutePath, SourceFingerprint, Diagnostic);
        SourceBaselines[SourceAsset.Asset.ToString()] = SourceFingerprint;
    }
    if (SourceAsset.IsValid())
    {
        bLegacy = false;
    }
    Saved = std::move(Next);
    return AssetDiagnostic::Ok();
}

void MaterialDocument::UpdateSourceIdentity()
{
    if (ExpectedType != MaterialAssetType)
    {
        return;
    }
    AssetDiagnostic Diagnostic;
    AssetMetadataIO::TryAssetRefFromJson(GetDocument().value("source", nlohmann::json()), SourceAsset, Diagnostic);
    const auto Found = SourceBaselines.find(SourceAsset.Asset.ToString());
    SourceFingerprint = {};
    if (Found != SourceBaselines.end())
    {
        SourceFingerprint = Found->second;
    }
}

bool MaterialDocument::IsSourceReadOnly() const
{
    AssetRegistryEntry Entry;
    return IsReadOnly() || ExpectedType != MaterialAssetType
        || (BoundRegistry->TryResolveKey(SourceAsset, Entry) && Entry.Mount == AssetMount::Engine);
}
