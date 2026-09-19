#include "Assets/AssetPath.h"

#include <algorithm>

namespace
{
bool ContainsDotDot(const std::filesystem::path& Path)
{
    for (const auto& Part : Path)
    {
        if (Part == "..")
        {
            return true;
        }
    }
    return false;
}
}

std::string AssetPath::NormalizeRelative(const std::string& RelativePath)
{
    std::filesystem::path Path = std::filesystem::path(RelativePath).lexically_normal();
    std::string Normalized = Path.generic_string();
    while (!Normalized.empty() && (Normalized.front() == '/' || Normalized.front() == '\\'))
    {
        Normalized.erase(Normalized.begin());
    }
    return Normalized;
}

std::string AssetPath::MakeVirtualPath(AssetMount Mount, const std::string& RelativeInsideContent)
{
    const std::string Relative = NormalizeRelative(RelativeInsideContent);
    const char* MountName = (Mount == AssetMount::Engine) ? EngineMountName : GameMountName;
    if (Relative.empty())
    {
        return std::string("/") + MountName;
    }
    return std::string("/") + MountName + "/" + Relative;
}

bool AssetPath::TryParseVirtualPath(
    const std::string& VirtualPath,
    AssetMount& OutMount,
    std::string& OutRelativeInsideContent)
{
    std::string Normalized = VirtualPath;
    std::replace(Normalized.begin(), Normalized.end(), '\\', '/');
    while (!Normalized.empty() && Normalized.front() == '/')
    {
        Normalized.erase(Normalized.begin());
    }

    const std::string EnginePrefix = std::string(EngineMountName) + "/";
    const std::string GamePrefix = std::string(GameMountName) + "/";

    if (Normalized == EngineMountName || Normalized.rfind(EnginePrefix, 0) == 0)
    {
        OutMount = AssetMount::Engine;
        OutRelativeInsideContent = (Normalized == EngineMountName)
            ? std::string{}
            : NormalizeRelative(Normalized.substr(EnginePrefix.size()));
        return true;
    }

    if (Normalized == GameMountName || Normalized.rfind(GamePrefix, 0) == 0)
    {
        OutMount = AssetMount::Game;
        OutRelativeInsideContent = (Normalized == GameMountName)
            ? std::string{}
            : NormalizeRelative(Normalized.substr(GamePrefix.size()));
        return true;
    }

    OutMount = AssetMount::Game;
    OutRelativeInsideContent = NormalizeRelative(Normalized);
    return true;
}

bool AssetPath::IsInsideContent(const std::filesystem::path& AbsolutePath, const std::filesystem::path& ContentRoot)
{
    std::error_code Error;
    const std::filesystem::path CanonicalRoot = std::filesystem::weakly_canonical(ContentRoot, Error);
    if (Error)
    {
        return false;
    }
    const std::filesystem::path CanonicalPath = std::filesystem::weakly_canonical(AbsolutePath, Error);
    if (Error)
    {
        return false;
    }
    auto RootIt = CanonicalRoot.begin();
    auto PathIt = CanonicalPath.begin();
    for (; RootIt != CanonicalRoot.end(); ++RootIt, ++PathIt)
    {
        if (PathIt == CanonicalPath.end() || *RootIt != *PathIt)
        {
            return false;
        }
    }
    return true;
}

bool AssetPath::TryMakeRelative(
    const std::filesystem::path& AbsolutePath,
    const std::filesystem::path& ContentRoot,
    std::string& OutRelative,
    AssetDiagnostic& OutError)
{
    if (ContainsDotDot(AbsolutePath))
    {
        OutError = AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "AssetPath", "Path traversal is not allowed", {}, AbsolutePath.string());
        return false;
    }
    if (!IsInsideContent(AbsolutePath, ContentRoot))
    {
        OutError = AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "AssetPath", "Path is outside Content root", {}, AbsolutePath.string());
        return false;
    }
    OutRelative = NormalizeRelative(std::filesystem::relative(AbsolutePath, ContentRoot).generic_string());
    OutError = AssetDiagnostic::Ok();
    return true;
}

std::filesystem::path AssetPath::CombineContent(
    const std::filesystem::path& ContentRoot,
    const std::string& RelativePath)
{
    const std::string Normalized = NormalizeRelative(RelativePath);
    std::filesystem::path Combined = ContentRoot / std::filesystem::path(Normalized);
    return Combined.lexically_normal();
}

std::string AssetPath::MetaPathForAsset(const std::string& AssetRelativePath)
{
    return NormalizeRelative(AssetRelativePath) + ".meta";
}

bool AssetPath::HasMetaSuffix(const std::string& Path)
{
    const std::string Normalized = NormalizeRelative(Path);
    return Normalized.size() >= 5 && Normalized.compare(Normalized.size() - 5, 5, ".meta") == 0;
}
