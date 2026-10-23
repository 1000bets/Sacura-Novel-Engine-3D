#include "Documents/StaticMeshDocument.h"
#include "Assets/AssetManager.h"
#include "Materials/MaterialDocumentIO.h"

StaticMeshDocument::StaticMeshDocument(AssetRegistry& InRegistry) : Registry(InRegistry) {}

bool StaticMeshDocument::Open(const AssetKey& InAsset, std::string& Error)
{
    AssetRegistryEntry Resolved;
    const SubAssetRecord* Subasset = nullptr;
    if (!Registry.TryResolveKey(InAsset, Resolved, &Subasset) || !Subasset || Subasset->Type != StaticMeshAssetType)
    {
        Error = "StaticMesh subasset not found";
        return false;
    }
    StaticMeshDefaults Parsed;
    if (!StaticMeshDefaults::Parse(StaticMeshDefaults::Read(Resolved.Metadata, InAsset), Parsed, Error))
    {
        return false;
    }
    Asset = InAsset;
    Entry = std::move(Resolved);
    Defaults = std::move(Parsed);
    Saved = Defaults.ToJson();
    History = {Saved};
    Position = 0;
    return true;
}

bool StaticMeshDocument::Edit(const nlohmann::json& Value, std::string& Error)
{
    if (Entry.Mount != AssetMount::Game)
    {
        Error = "Engine mesh defaults are read-only";
        return false;
    }
    StaticMeshDefaults Parsed;
    if (!StaticMeshDefaults::Parse(Value, Parsed, Error))
    {
        return false;
    }
    for (const auto& Slot : Parsed.Materials)
    {
        AssetRegistryEntry Material;
        if (!Registry.TryResolveKey(Slot.second, Material) || Slot.second.HasSubAsset()
            || (Material.Metadata.Type != MaterialAssetType && Material.Metadata.Type != MaterialInstanceAssetType))
        {
            Error = "Material slot requires a registered Material or MaterialInstance";
            return false;
        }
    }
    for (const auto& Slot : Parsed.Materials)
    {
        ResolvedMaterial Material;
        const auto Diagnostic = MaterialDocumentIO::Resolve(Registry, Slot.second, Material);
        if (Diagnostic.HasError() || Material.Definition.Domain != MaterialDomain::Surface)
        {
            Error = "Mesh slot requires a valid Surface material: " + Diagnostic.Message;
            return false;
        }
    }
    const auto Next = Parsed.ToJson();
    if (Next != Defaults.ToJson())
    {
        History.resize(Position + 1);
        History.push_back(Next);
        ++Position;
        Defaults = std::move(Parsed);
    }
    return true;
}

bool StaticMeshDocument::Undo()
{
    if (Position == 0)
    {
        return false;
    }
    std::string Error;
    return StaticMeshDefaults::Parse(History[--Position], Defaults, Error);
}

bool StaticMeshDocument::Redo()
{
    if (Position + 1 >= History.size())
    {
        return false;
    }
    std::string Error;
    return StaticMeshDefaults::Parse(History[++Position], Defaults, Error);
}

bool StaticMeshDocument::Save(AssetManager& Manager, std::string& Error)
{
    if (!Edit(Defaults.ToJson(), Error))
    {
        return false;
    }
    auto Settings = Entry.Metadata.LoadSettings;
    Settings["staticMeshDefaults"][Asset.SubAsset->ToString()] = Defaults.ToJson();
    const auto Result = Registry.UpdateLoadSettings(Asset.Asset, Settings, Entry.Metadata.LoadSettings);
    if (Result.HasError())
    {
        Error = Result.Message;
        return false;
    }
    Entry.Metadata.LoadSettings = std::move(Settings);
    Saved = Defaults.ToJson();
    Manager.InvalidateAsset(Asset.Asset);
    return true;
}

bool StaticMeshDocument::IsDirty() const
{
    return Defaults.ToJson() != Saved;
}
