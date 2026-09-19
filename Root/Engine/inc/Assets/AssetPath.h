#pragma once

#include "Assets/AssetTypes.h"

#include <filesystem>
#include <string>

enum class AssetMount
{
    Game,
    Engine
};

class AssetPath
{
public:
    static constexpr const char* ContentFolderName = "Content";
    static constexpr const char* GameMountName = "Game";
    static constexpr const char* EngineMountName = "Engine";

    static std::string NormalizeRelative(const std::string& RelativePath);
    static std::string MakeVirtualPath(AssetMount Mount, const std::string& RelativeInsideContent);
    static bool TryParseVirtualPath(const std::string& VirtualPath, AssetMount& OutMount, std::string& OutRelativeInsideContent);
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
