#include "Assets/MaterialAssetCreation.h"
#include "Materials/MaterialDocumentIO.h"
#include "Core/IO/AtomicFileWriter.h"
#include <fstream>

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
    AssetMetadata SourceMetadata;
    std::filesystem::path SourcePath;
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
        SourcePath = Destination.parent_path() / (Relative.stem().string() + ".Source.hlsl");
        SourceMetadata.Guid = Guid::Generate();
        SourceMetadata.Type = ShaderSourceAssetType;
        AssetRegistryEntry TemplateSource;
        Registry.TryResolveKey(Resolved.Definition.Source, TemplateSource);
        std::ifstream SourceInput(TemplateSource.AbsolutePath);
        const std::string ShaderText(std::istreambuf_iterator<char>(SourceInput), {});
        Resolved.Definition.Source = AssetKey{SourceMetadata.Guid, {}};
        Document = MaterialDocumentIO::MakeMaterialDocument(Resolved);
        if (std::filesystem::exists(SourcePath) || std::filesystem::exists(SourcePath.string() + ".meta"))
        {
            return AssetDiagnostic::Fail(AssetErrorCode::ImportConflict, "CreateMaterial", "Shader destination already exists");
        }
        std::string Error;
        if (!AtomicFileWriter::WriteText(SourcePath.string(), ShaderText, Error))
        {
            return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "CreateMaterial", Error);
        }
        Created.push_back(SourcePath);
        ContentHash::TryHashFile(SourcePath.string(), SourceMetadata.SourceFingerprint, Diagnostic);
        if (!AssetMetadataIO::TrySaveToFile(SourcePath.string() + ".meta", SourceMetadata, Diagnostic))
        {
            return Failure(Diagnostic);
        }
        Created.emplace_back(SourcePath.string() + ".meta");
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
    if (!SourcePath.empty())
    {
        Registry.RegisterExistingAsset(std::filesystem::relative(SourcePath, Registry.GetGameContentRoot()).generic_string(), SourceMetadata);
    }
    Diagnostic = Registry.RegisterExistingAsset(VirtualPath, Metadata);
    if (Diagnostic.HasError())
    {
        return Failure(Diagnostic);
    }
    OutAsset = AssetKey{Metadata.Guid, {}};
    return AssetDiagnostic::Ok();
}
