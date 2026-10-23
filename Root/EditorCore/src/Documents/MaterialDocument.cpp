#include "Documents/MaterialDocument.h"

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
    if (Initial.Document.contains("source"))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::UnsupportedFeature, "MaterialDocument", "External shader sources are no longer supported", Key);
    }
    if (ExpectedType == MaterialAssetType && Initial.Document.value("schemaVersion", 0) == 1)
    {
        if (Diagnostic.HasError())
        {
            return Diagnostic;
        }
        Initial.Document = MaterialDocumentIO::MakeMaterialDocument(Material);
    }
    Initial.Source = Material.Definition.ShaderSource;
    if (ExpectedType == MaterialAssetType)
    {
        if (!Initial.Document.contains("shader") || !Initial.Document.at("shader").is_string())
        {
            return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "MaterialDocument", "Material requires embedded shader code", Key);
        }
        Initial.Source = Initial.Document.at("shader").get<std::string>();
    }
    ContentHash Hash;
    if (!ContentHash::TryHashFile(Entry.AbsolutePath, Hash, Diagnostic))
    {
        return Diagnostic;
    }
    BoundRegistry = &Registry;
    Asset = Key;
    DocumentFingerprint = Hash;
    Revisions = {Initial};
    Saved = std::move(Initial);
    CurrentRevision = 0;
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
        Value.Document["shader"] = Value.Source;
    }
    Revisions.resize(CurrentRevision + 1);
    Revisions.push_back(std::move(Value));
    ++CurrentRevision;
}

void MaterialDocument::SetDocument(nlohmann::json Document)
{
    Revision Next = Revisions.at(CurrentRevision);
    Next.Document = std::move(Document);
    if (ExpectedType == MaterialAssetType)
    {
        Next.Source = Next.Document.at("shader").get<std::string>();
    }
    AppendRevision(std::move(Next));
}

void MaterialDocument::SetSource(std::string Text)
{
    Revision Next = Revisions.at(CurrentRevision);
    Next.Source = std::move(Text);
    AppendRevision(std::move(Next));
}

void MaterialDocument::SetDocumentAndSource(nlohmann::json Document, std::string Text)
{
    Revision Next;
    Next.Document = std::move(Document);
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
    return true;
}

bool MaterialDocument::Redo()
{
    if (CurrentRevision + 1 >= Revisions.size() || IsReadOnly())
    {
        return false;
    }
    ++CurrentRevision;
    return true;
}

AssetDiagnostic MaterialDocument::BuildResolved(ResolvedMaterial& OutMaterial) const
{
    return MaterialDocumentIO::Resolve(*BoundRegistry, Asset, OutMaterial,
        {{Asset.Asset.ToString(), GetDocument()}});
}

AssetDiagnostic MaterialDocument::Save(bool bOverwriteExternalChanges)
{
    if (IsReadOnly())
    {
        return AssetDiagnostic::Fail(AssetErrorCode::UnsupportedFeature, "MaterialDocument", "Engine Content is read-only");
    }
    if (!bOverwriteExternalChanges && HasExternalChanges())
    {
        return AssetDiagnostic::Fail(AssetErrorCode::AssetChanged, "MaterialDocument", "Material changed externally; reload or explicitly overwrite");
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
    Diagnostic = MaterialDocumentIO::Save(Entry.AbsolutePath, Next.Document);
    if (Diagnostic.HasError())
    {
        return Diagnostic;
    }
    ContentHash::TryHashFile(Entry.AbsolutePath, DocumentFingerprint, Diagnostic);
    Saved = std::move(Next);
    return AssetDiagnostic::Ok();
}

bool MaterialDocument::IsSourceReadOnly() const
{
    return IsReadOnly() || ExpectedType != MaterialAssetType;
}
