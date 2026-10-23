#pragma once
#include "Input/InputDocument.h"
#include "Assets/AssetRegistry.h"

class InputPackage
{
public:
    static bool Export(AssetRegistry& Registry, const AssetRegistryEntry& Root,
        const std::filesystem::path& Destination, std::string& Error);
    static bool Validate(const nlohmann::json& Package, std::string& Error);
    static bool Import(AssetRegistry& Registry, const nlohmann::json& Package,
        std::vector<AssetRegistryEntry>& Imported, std::string& Error);
};
