#pragma once
#include "Assets/AssetRegistry.h"

struct MaterialSourceEdit
{
    std::string Path;
    std::string Original;
    std::string Updated;
};

class MaterialSourceRelocation
{
public:
    static AssetDiagnostic Prepare(const AssetRegistry& Registry, const std::filesystem::path& Source,
        const std::filesystem::path& Destination, std::vector<MaterialSourceEdit>& OutEdits);
    static AssetDiagnostic Apply(const std::vector<MaterialSourceEdit>& Edits);
    static void Restore(const std::vector<MaterialSourceEdit>& Edits);
};
