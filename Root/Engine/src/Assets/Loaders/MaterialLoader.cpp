#include "Assets/Loaders/MaterialLoader.h"
#include "Assets/AssetMetadata.h"
#include "Assets/Resources/MaterialResource.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace
{
bool TryReadMaterialDocument(const std::string& AbsolutePath, nlohmann::json& OutDocument, AssetDiagnostic& OutDiagnostic, const AssetKey& Key)
{
    std::ifstream Input(AbsolutePath);
    if (!Input)
    {
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::NotFound,
            "MaterialLoader",
            "Failed to open .material",
            Key,
            AbsolutePath,
            "MaterialLoader");
        return false;
    }

    try
    {
        Input >> OutDocument;
    }
    catch (const std::exception& Exception)
    {
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "MaterialLoader",
            Exception.what(),
            Key,
            AbsolutePath,
            "MaterialLoader");
        return false;
    }

    return true;
}

bool TryParseBaseColorTexture(const nlohmann::json& Document, AssetKey& OutKey, AssetDiagnostic& OutDiagnostic)
{
    if (!Document.contains("baseColorTexture"))
    {
        return true;
    }

    return AssetMetadataIO::TryAssetRefFromJson(Document["baseColorTexture"], OutKey, OutDiagnostic);
}
}

void MaterialLoader::CollectDependencies(const AssetLoadContext& Context, std::vector<AssetKey>& OutDependencies) const
{
    nlohmann::json Document;
    AssetDiagnostic Diagnostic{};
    if (!TryReadMaterialDocument(Context.Entry.AbsolutePath, Document, Diagnostic, Context.Key))
    {
        return;
    }

    AssetKey TextureKey{};
    AssetDiagnostic RefError{};
    if (!TryParseBaseColorTexture(Document, TextureKey, RefError))
    {
        return;
    }

    if (TextureKey.IsValid())
    {
        OutDependencies.push_back(TextureKey);
    }
}

std::shared_ptr<const void> MaterialLoader::Load(const AssetLoadContext& Context, AssetDiagnostic& OutDiagnostic)
{
    nlohmann::json Document;
    if (!TryReadMaterialDocument(Context.Entry.AbsolutePath, Document, OutDiagnostic, Context.Key))
    {
        return nullptr;
    }

    if (!Document.is_object() || !Document.contains("schemaVersion") || !Document["schemaVersion"].is_number_integer())
    {
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "MaterialLoader",
            "Missing schemaVersion",
            Context.Key,
            Context.Entry.RelativePath,
            "MaterialLoader");
        return nullptr;
    }

    if (Document["schemaVersion"].get<int32_t>() != 1)
    {
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "MaterialLoader",
            "Unsupported material schemaVersion",
            Context.Key,
            Context.Entry.RelativePath,
            "MaterialLoader");
        return nullptr;
    }

    auto Resource = std::make_shared<MaterialResource>();

    if (Document.contains("baseColor") && Document["baseColor"].is_array() && Document["baseColor"].size() == 4)
    {
        Resource->BaseColor = DirectX::SimpleMath::Vector4(
            Document["baseColor"][0].get<float>(),
            Document["baseColor"][1].get<float>(),
            Document["baseColor"][2].get<float>(),
            Document["baseColor"][3].get<float>());
    }

    if (Document.contains("metallic") && Document["metallic"].is_number())
    {
        Resource->Metallic = Document["metallic"].get<float>();
    }

    if (Document.contains("roughness") && Document["roughness"].is_number())
    {
        Resource->Roughness = Document["roughness"].get<float>();
    }

    AssetKey TextureKey{};
    if (!TryParseBaseColorTexture(Document, TextureKey, OutDiagnostic))
    {
        OutDiagnostic.Key = Context.Key;
        OutDiagnostic.Loader = "MaterialLoader";
        return nullptr;
    }

    Resource->BaseColorTexture.Key = TextureKey;
    return Resource;
}
