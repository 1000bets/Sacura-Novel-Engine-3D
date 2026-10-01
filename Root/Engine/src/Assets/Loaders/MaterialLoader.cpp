#include "Assets/Loaders/MaterialLoader.h"
#include "Assets/Resources/MaterialResource.h"
#include "Materials/MaterialDocumentIO.h"

void MaterialLoader::CollectDependencies(const AssetLoadContext& Context, std::vector<AssetKey>& OutDependencies) const
{
    nlohmann::json Document;
    if (MaterialDocumentIO::Read(Context.Entry.AbsolutePath, Document).HasError())
    {
        return;
    }
    if (Document.value("schemaVersion", 0) == 1 && Context.Entry.Metadata.Type == MaterialAssetType
        && Document.contains("baseColorTexture"))
    {
        AssetKey Texture;
        AssetDiagnostic Diagnostic;
        if (AssetMetadataIO::TryAssetRefFromJson(Document["baseColorTexture"], Texture, Diagnostic) && Texture.IsValid())
        {
            OutDependencies.push_back(Texture);
        }
    }
}

static bool LoadMaterialValues(const AssetLoadContext& Context, MaterialResource& Resource, AssetDiagnostic& OutDiagnostic)
{
    OutDiagnostic = MaterialDocumentIO::Resolve(*Context.Registry, Context.Key, Resource.Resolved);
    if (OutDiagnostic.HasError())
    {
        return false;
    }
    const auto& Definition = Resource.Resolved.Definition;
    Resource.AlphaCutoff = Definition.AlphaCutoff;
    Resource.bDoubleSided = Definition.bDoubleSided;
    Resource.bCastShadows = Definition.bCastShadows;
    if (Definition.BlendMode == MaterialBlendMode::Masked)
    {
        Resource.AlphaMode = MaterialAlphaMode::Mask;
    }
    else if (Definition.BlendMode == MaterialBlendMode::Translucent)
    {
        Resource.AlphaMode = MaterialAlphaMode::Blend;
    }
    for (const auto& Parameter : Resource.Resolved.Values)
    {
        const auto& Value = Parameter.second;
        if (Parameter.first == "BaseColor" && Value.Type == MaterialParameterType::Color)
        {
            Resource.BaseColor = Value.Numbers;
        }
        else if (Parameter.first == "Metallic" && Value.Type == MaterialParameterType::Float)
        {
            Resource.Metallic = Value.Numbers.x;
        }
        else if (Parameter.first == "Roughness" && Value.Type == MaterialParameterType::Float)
        {
            Resource.Roughness = Value.Numbers.x;
        }
        else if (Parameter.first == "Emissive" && Value.Type == MaterialParameterType::Vector3)
        {
            Resource.Emissive = {Value.Numbers.x, Value.Numbers.y, Value.Numbers.z};
        }
        else if (Parameter.first == "BaseColorTexture" && Value.Type == MaterialParameterType::Texture2D)
        {
            Resource.BaseColorTexture.Key = Value.Texture;
        }
    }
    return true;
}

std::shared_ptr<const void> MaterialLoader::Load(const AssetLoadContext& Context, AssetDiagnostic& OutDiagnostic)
{
    auto Resource = std::make_shared<MaterialResource>();
    if (!LoadMaterialValues(Context, *Resource, OutDiagnostic))
    {
        return nullptr;
    }
    return Resource;
}

std::shared_ptr<const void> MaterialInstanceLoader::Load(const AssetLoadContext& Context, AssetDiagnostic& OutDiagnostic)
{
    auto Resource = std::make_shared<MaterialInstanceResource>();
    if (!LoadMaterialValues(Context, *Resource, OutDiagnostic))
    {
        return nullptr;
    }
    return Resource;
}
