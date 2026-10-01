#pragma once

#include "Materials/MaterialDefinition.h"
#include <nlohmann/json.hpp>

class AssetRegistry;

class MaterialDocumentIO
{
public:
    static AssetDiagnostic Resolve(const AssetRegistry& Registry, const AssetKey& Key, ResolvedMaterial& OutMaterial,
        const std::map<std::string, nlohmann::json>& Documents = {},
        const std::map<std::string, std::string>& Sources = {});
    static AssetDiagnostic Read(const std::string& Path, nlohmann::json& OutDocument);
    static AssetDiagnostic Save(const std::string& Path, const nlohmann::json& Document);
    static MaterialParameterValue ReadValue(MaterialParameterType Type, const nlohmann::json& Value);
    static nlohmann::json WriteValue(const MaterialParameterValue& Value);
    static nlohmann::json MakeMaterialDocument(const ResolvedMaterial& Material);
};
