#include "Core/EnginePaths.h"

#include "Core/Threading/ThreadContext.h"

std::filesystem::path EnginePaths::InstallationRoot{};
bool EnginePaths::bInitialized = false;

namespace
{
bool LooksLikeEngineRoot(const std::filesystem::path& Candidate)
{
    std::error_code Error;
    if (std::filesystem::exists(Candidate / "Engine" / "Shaders", Error))
    {
        return true;
    }
    if (std::filesystem::exists(Candidate / "Engine" / "Content", Error))
    {
        return true;
    }
    if (std::filesystem::exists(Candidate / "shaders", Error))
    {
        return true;
    }
    return false;
}
}

void EnginePaths::InitializeFromRoot(const std::filesystem::path& EngineInstallationRoot)
{
    std::error_code Error;
    InstallationRoot = std::filesystem::weakly_canonical(EngineInstallationRoot, Error);
    if (Error)
    {
        InstallationRoot = std::filesystem::absolute(EngineInstallationRoot).lexically_normal();
    }
    bInitialized = true;
    PrintString(std::string("EnginePaths: root = ") + InstallationRoot.generic_string());
}

void EnginePaths::InitializeFromExecutable(const std::filesystem::path& ExecutableFile)
{
    std::error_code Error;
    std::filesystem::path ExecutableAbsolute = std::filesystem::weakly_canonical(ExecutableFile, Error);
    if (Error)
    {
        ExecutableAbsolute = std::filesystem::absolute(ExecutableFile).lexically_normal();
    }

    const std::filesystem::path ExecutableDirectory = ExecutableAbsolute.parent_path();
    const std::filesystem::path ParentOfExecutable = ExecutableDirectory.parent_path();

    if (LooksLikeEngineRoot(ParentOfExecutable)
        && (ExecutableDirectory.filename() == "Bin" || ExecutableDirectory.filename() == "bin"))
    {
        InitializeFromRoot(ParentOfExecutable);
        return;
    }

    if (LooksLikeEngineRoot(ExecutableDirectory))
    {
        InitializeFromRoot(ExecutableDirectory);
        return;
    }

    if (LooksLikeEngineRoot(ParentOfExecutable))
    {
        InitializeFromRoot(ParentOfExecutable);
        return;
    }

    InitializeFromRoot(ExecutableDirectory);
}

bool EnginePaths::IsInitialized()
{
    return bInitialized;
}

const std::filesystem::path& EnginePaths::Root()
{
    return InstallationRoot;
}

std::filesystem::path EnginePaths::Bin()
{
    const std::filesystem::path StagedBin = InstallationRoot / "Bin";
    std::error_code Error;
    if (std::filesystem::exists(StagedBin, Error))
    {
        return StagedBin;
    }
    return InstallationRoot;
}

std::filesystem::path EnginePaths::Content()
{
    return InstallationRoot / "Engine" / "Content";
}

std::filesystem::path EnginePaths::Shaders()
{
    const std::filesystem::path Staged = InstallationRoot / "Engine" / "Shaders";
    std::error_code Error;
    if (std::filesystem::exists(Staged, Error))
    {
        return Staged;
    }

    const std::filesystem::path LegacyNextToRoot = InstallationRoot / "shaders";
    if (std::filesystem::exists(LegacyNextToRoot, Error))
    {
        return LegacyNextToRoot;
    }

    const std::filesystem::path SourceTree = InstallationRoot / "Root" / "Engine" / "shaders";
    if (std::filesystem::exists(SourceTree, Error))
    {
        return SourceTree;
    }

    return Staged;
}

std::filesystem::path EnginePaths::Config()
{
    return InstallationRoot / "Engine" / "Config";
}

std::string EnginePaths::EngineVersion()
{
    return "0.1";
}
