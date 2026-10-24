#include "Project/GameExporter.h"
#include "Project/ProjectDescriptor.h"

#include "Assets/Guid.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <fstream>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
#include <stdexcept>

class GameExportFiles
{
public:
    static bool IsInside(const std::filesystem::path& Candidate, const std::filesystem::path& Root)
    {
        const auto CandidatePath = std::filesystem::weakly_canonical(Candidate);
        const auto RootPath = std::filesystem::weakly_canonical(Root);
        auto CandidatePart = CandidatePath.begin();
        for (const auto& RootPart : RootPath)
        {
            if (CandidatePart == CandidatePath.end())
            {
                return false;
            }
#ifdef _WIN32
            if (CompareStringOrdinal(CandidatePart->c_str(), -1, RootPart.c_str(), -1, TRUE) != CSTR_EQUAL)
#else
            if (*CandidatePart != RootPart)
#endif
            {
                return false;
            }
            ++CandidatePart;
        }
        return true;
    }

    static void Copy(const std::filesystem::path& Source, const std::filesystem::path& Destination)
    {
        if (std::filesystem::is_symlink(std::filesystem::symlink_status(Source)))
        {
            throw std::runtime_error("Export does not support symbolic links: " + Source.u8string());
        }
        if (std::filesystem::is_directory(Source))
        {
            std::filesystem::create_directories(Destination);
            for (const auto& Entry : std::filesystem::directory_iterator(Source))
            {
                const auto Name = Entry.path().filename();
                if (Name == "__pycache__")
                {
                    continue;
                }
                Copy(Entry.path(), Destination / Name);
            }
        }
        else
        {
            std::filesystem::create_directories(Destination.parent_path());
            std::filesystem::copy_file(Source, Destination);
        }
    }
};

bool GameExporter::Export(const std::filesystem::path& ProjectFile,
    const std::filesystem::path& RuntimeDirectory,
    const std::filesystem::path& EngineRoot,
    const std::filesystem::path& Destination,
    const std::string& ExecutableName,
    std::string& OutError)
{
    OutError.clear();
    std::filesystem::path StagingDirectory;
    try
    {
        ProjectDescriptor Project;
        if (!ProjectDescriptor::TryLoadFromFile(ProjectFile, Project, OutError))
        {
            return false;
        }
        if (Project.StartupScene.empty() || !std::filesystem::is_regular_file(Project.StartupScene))
        {
            OutError = "Save a startup scene and select it in project settings before exporting.";
            return false;
        }
        if (!Project.StartupStory.empty() && !std::filesystem::is_regular_file(Project.StartupStory))
        {
            OutError = "The startup story does not exist.";
            return false;
        }
        if (!GameExportFiles::IsInside(Project.StartupScene, Project.ProjectRoot / "Content") ||
            (!Project.StartupStory.empty() && !GameExportFiles::IsInside(Project.StartupStory, Project.ProjectRoot / "Content")))
        {
            OutError = "The startup scene and story must be stored in the project's Content folder.";
            return false;
        }
        const auto OutputDirectory = std::filesystem::absolute(Destination).lexically_normal();
        if (Destination.empty() || std::filesystem::exists(OutputDirectory) ||
            GameExportFiles::IsInside(OutputDirectory, Project.ProjectRoot) ||
            GameExportFiles::IsInside(OutputDirectory, RuntimeDirectory) ||
            GameExportFiles::IsInside(OutputDirectory, EngineRoot / "Engine"))
        {
            OutError = "Choose a new output folder outside the project and runtime folders. Existing folders are never overwritten.";
            return false;
        }
        if (!std::filesystem::is_regular_file(RuntimeDirectory / ExecutableName)
            || std::filesystem::u8path(ExecutableName).filename().u8string() != ExecutableName ||
            !std::filesystem::is_directory(EngineRoot / "Engine" / "Content") ||
            !std::filesystem::is_directory(EngineRoot / "Engine" / "Shaders"))
        {
            OutError = "The installed Player runtime is incomplete. Install a complete Editor distribution.";
            return false;
        }
        if (!std::filesystem::is_directory(OutputDirectory.parent_path()))
        {
            OutError = "The output parent folder does not exist.";
            return false;
        }
        StagingDirectory = OutputDirectory.parent_path() / (".sakura-export-" + Guid::Generate().ToString());
        std::filesystem::create_directory(StagingDirectory);
        for (const auto& Entry : std::filesystem::directory_iterator(RuntimeDirectory))
        {
            if (Entry.path().filename() == "resources") { continue; }
            if (Entry.path().filename().u8string() == ExecutableName)
            {
                GameExportFiles::Copy(Entry.path(), StagingDirectory / "Game.exe");
            }
            else
            {
                GameExportFiles::Copy(Entry.path(), StagingDirectory / Entry.path().filename());
            }
        }
        GameExportFiles::Copy(Project.ProjectFile, StagingDirectory / "resources" / "Game" / "Game.project");
        nlohmann::json DescriptorJson;
        std::ifstream DescriptorInput(Project.ProjectFile);
        DescriptorInput >> DescriptorJson;
        DescriptorJson["startupScene"] = Project.StartupScene.lexically_relative(Project.ProjectRoot).generic_u8string();
        if (!Project.StartupStory.empty())
        {
            DescriptorJson["startupStory"] = Project.StartupStory.lexically_relative(Project.ProjectRoot).generic_u8string();
        }
        std::ofstream DescriptorOutput(StagingDirectory / "resources" / "Game" / "Game.project", std::ios::trunc);
        DescriptorOutput.exceptions(std::ios::badbit | std::ios::failbit);
        DescriptorOutput << DescriptorJson.dump(2);
        DescriptorOutput.close();
        for (const auto& Folder : {"Content", "Config"})
        {
            if (std::filesystem::exists(Project.ProjectRoot / Folder))
            {
                GameExportFiles::Copy(Project.ProjectRoot / Folder, StagingDirectory / "resources" / "Game" / Folder);
            }
        }
        for (int Attempt = 0; ; ++Attempt)
        {
            std::error_code PublishError;
            std::filesystem::rename(StagingDirectory, OutputDirectory, PublishError);
            if (!PublishError)
            {
                break;
            }
            if (Attempt >= 10 || PublishError != std::errc::permission_denied)
            {
                throw std::filesystem::filesystem_error("Publish exported game", StagingDirectory, OutputDirectory, PublishError);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        return true;
    }
    catch (const std::exception& Error)
    {
        OutError = std::string("Game export failed: ") + Error.what();
        if (!StagingDirectory.empty())
        {
            std::error_code CleanupError;
            std::filesystem::remove_all(StagingDirectory, CleanupError);
        }
        return false;
    }
}
