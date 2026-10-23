#include "Assets/MaterialAssetCreation.h"
#include "Materials/MaterialDocumentIO.h"

AssetDiagnostic MaterialAssetCreation::Create(AssetRegistry& Registry, const std::string& VirtualPath,
    const AssetKey& Parent, MaterialDomain Domain, AssetKey& OutAsset)
{
    if (VirtualPath.rfind("/Game/", 0) != 0 || VirtualPath.find("..") != std::string::npos)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "CreateMaterial", "A Game Content destination is required");
    }
    const auto Relative = std::filesystem::path(VirtualPath.substr(6));
    if (Registry.LeafNameExists(Relative.stem().string()))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::ImportConflict, "CreateMaterial", "An asset with this name already exists");
    }
    const auto Destination = Registry.GetGameContentRoot() / Relative;
    std::vector<std::filesystem::path> Created;
    auto Failure = [&](const AssetDiagnostic& Diagnostic)
    {
        for (const auto& Path : Created)
        {
            std::error_code Error;
            std::filesystem::remove(Path, Error);
        }
        return Diagnostic;
    };
    AssetMetadata Metadata;
    Metadata.Guid = Guid::Generate();
    nlohmann::json Document;
    AssetDiagnostic Diagnostic;
    if (Parent.IsValid())
    {
        ResolvedMaterial Resolved;
        Diagnostic = MaterialDocumentIO::Resolve(Registry, Parent, Resolved);
        if (Diagnostic.HasError())
        {
            return Diagnostic;
        }
        Metadata.Type = MaterialInstanceAssetType;
        Document = {{"schemaVersion", 1}, {"parent", AssetMetadataIO::AssetRefToJson(Parent)},
            {"overrides", nlohmann::json::object()}};
    }
    else
    {
        Metadata.Type = MaterialAssetType;
        const char* TemplatePath = "/Engine/Materials/StandardPBR.material";
        if (Domain == MaterialDomain::PostProcess)
        {
            TemplatePath = "/Engine/Materials/HDRVignette.material";
        }
        else if (Domain == MaterialDomain::UserInterface)
        {
            TemplatePath = "/Engine/Materials/UIPanel.material";
        }
        AssetRegistryEntry Template;
        if (!Registry.TryGetByPath(TemplatePath, Template))
        {
            return AssetDiagnostic::Fail(AssetErrorCode::NotFound, "CreateMaterial",
                std::string("Builtin material template is unavailable: ") + TemplatePath
                + "\nEngine Content: " + Registry.GetEngineContentRoot().generic_string());
        }
        ResolvedMaterial Resolved;
        Diagnostic = MaterialDocumentIO::Resolve(Registry, AssetKey{Template.Metadata.Guid, {}}, Resolved);
        if (Diagnostic.HasError())
        {
            return Diagnostic;
        }
        Document = MaterialDocumentIO::MakeMaterialDocument(Resolved);
    }
    if (std::filesystem::exists(Destination) || std::filesystem::exists(Destination.string() + ".meta"))
    {
        return Failure(AssetDiagnostic::Fail(AssetErrorCode::ImportConflict, "CreateMaterial", "Material destination already exists"));
    }
    Diagnostic = MaterialDocumentIO::Save(Destination.string(), Document);
    if (Diagnostic.HasError())
    {
        return Failure(Diagnostic);
    }
    Created.push_back(Destination);
    ContentHash::TryHashFile(Destination.string(), Metadata.SourceFingerprint, Diagnostic);
    if (!AssetMetadataIO::TrySaveToFile(Destination.string() + ".meta", Metadata, Diagnostic))
    {
        return Failure(Diagnostic);
    }
    Created.emplace_back(Destination.string() + ".meta");
    Diagnostic = Registry.RegisterExistingAsset(VirtualPath, Metadata);
    if (Diagnostic.HasError())
    {
        return Failure(Diagnostic);
    }
    OutAsset = AssetKey{Metadata.Guid, {}};
    return AssetDiagnostic::Ok();
}
