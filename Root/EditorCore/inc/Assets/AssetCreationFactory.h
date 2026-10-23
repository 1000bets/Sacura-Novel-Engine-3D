#pragma once
#include "Assets/AssetRegistry.h"
#include "Core/Object/Object.h"
#include "Reflection/ReflectionMacros.h"

class AssetCreationFactory : public Object
{
    ENGINE_ABSTRACT_CLASS(AssetCreationFactory, Object, "editor.AssetCreationFactory")
public:
    virtual AssetType GetAssetType() const { return UnknownAssetType; }
    virtual const char* GetDisplayName() const { return "Asset"; }
    virtual const char* GetExtension() const { return ""; }
    virtual const char* GetCategory() const { return "Assets"; }
    virtual bool IsSettingRequired(const PropertyId& Property) const { return false; }
    virtual bool Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const
    { Error = "Asset factory has no creation implementation"; return false; }
    virtual nlohmann::json DescribeSettings() const;
    static std::vector<AssetCreationFactory*> CollectFactories();
    static nlohmann::json DescribeFactories();
    static bool CreateAsset(AssetRegistry& Registry, const TypeId& FactoryType, const std::string& VirtualPath,
        const nlohmann::json& Settings, AssetRegistryEntry& Created, std::string& Error);
};
ENGINE_CLASS_END(AssetCreationFactory)

void ForceTouchBuiltinAssetCreationFactories();
