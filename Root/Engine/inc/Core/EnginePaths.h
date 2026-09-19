#pragma once

#include <filesystem>
#include <string>

class EnginePaths
{
public:
    static void InitializeFromExecutable(const std::filesystem::path& ExecutableFile);
    static void InitializeFromRoot(const std::filesystem::path& EngineInstallationRoot);
    static bool IsInitialized();

    static const std::filesystem::path& Root();
    static std::filesystem::path Bin();
    static std::filesystem::path Content();
    static std::filesystem::path Shaders();
    static std::filesystem::path Config();

    static std::string EngineVersion();

private:
    static std::filesystem::path InstallationRoot;
    static bool bInitialized;
};
