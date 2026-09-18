#pragma once

#include "Assets/AssetTypes.h"

#include <filesystem>
#include <string>

class AssetPath
{
public:
    static constexpr const char* ContentFolderName = "Content";

    static std::string NormalizeRelative(const std::string& RelativePath);
    static bool IsInsideContent(const std::filesystem::path& AbsolutePath, const std::filesystem::path& ContentRoot);
    static bool TryMakeRelative(
        const std::filesystem::path& AbsolutePath,
        const std::filesystem::path& ContentRoot,
        std::string& OutRelative,
        AssetDiagnostic& OutError);
    static std::filesystem::path CombineContent(
        const std::filesystem::path& ContentRoot,
        const std::string& RelativePath);
    static std::string MetaPathForAsset(const std::string& AssetRelativePath);
    static bool HasMetaSuffix(const std::string& Path);
};
