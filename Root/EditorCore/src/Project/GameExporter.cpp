#include "Project/GameExporter.h"
#include "Project/ProjectDescriptor.h"

#include <QString>
#include <QUuid>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

class GameExportFiles
{
public:
    static bool IsInside(const std::filesystem::path& Candidate, const std::filesystem::path& Root)
    {
        const QString CandidateText = QString::fromStdWString(std::filesystem::weakly_canonical(Candidate).generic_wstring());
        const QString RootText = QString::fromStdWString(std::filesystem::weakly_canonical(Root).generic_wstring());
        Qt::CaseSensitivity Sensitivity = Qt::CaseSensitive;
#ifdef _WIN32
        Sensitivity = Qt::CaseInsensitive;
#endif
        return CandidateText.compare(RootText, Sensitivity) == 0 || CandidateText.startsWith(RootText + "/", Sensitivity);
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
        if (!std::filesystem::is_regular_file(RuntimeDirectory / "SakuraPlayer.exe") ||
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
        StagingDirectory = OutputDirectory.parent_path() / (".sakura-export-" + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString());
        std::filesystem::create_directory(StagingDirectory);
        GameExportFiles::Copy(RuntimeDirectory / "SakuraPlayer.exe", StagingDirectory / "Game.exe");
        for (const auto& Entry : std::filesystem::directory_iterator(RuntimeDirectory))
        {
            const auto Extension = Entry.path().extension();
            if (Extension == ".dll" || Extension == "._pth")
            {
                GameExportFiles::Copy(Entry.path(), StagingDirectory / Entry.path().filename());
            }
        }
        for (const auto& Folder : {"platforms", "imageformats", "Python"})
        {
            if (std::filesystem::exists(RuntimeDirectory / Folder))
            {
                GameExportFiles::Copy(RuntimeDirectory / Folder, StagingDirectory / Folder);
            }
        }
        GameExportFiles::Copy(EngineRoot / "Engine", StagingDirectory / "Engine");
        GameExportFiles::Copy(Project.ProjectFile, StagingDirectory / "Game" / "Game.project");
        nlohmann::json DescriptorJson;
        std::ifstream DescriptorInput(Project.ProjectFile);
        DescriptorInput >> DescriptorJson;
        DescriptorJson["startupScene"] = Project.StartupScene.lexically_relative(Project.ProjectRoot).generic_u8string();
        if (!Project.StartupStory.empty())
        {
            DescriptorJson["startupStory"] = Project.StartupStory.lexically_relative(Project.ProjectRoot).generic_u8string();
        }
        std::ofstream DescriptorOutput(StagingDirectory / "Game" / "Game.project", std::ios::trunc);
        DescriptorOutput.exceptions(std::ios::badbit | std::ios::failbit);
        DescriptorOutput << DescriptorJson.dump(2);
        DescriptorOutput.close();
        for (const auto& Folder : {"Content", "Config"})
        {
            if (std::filesystem::exists(Project.ProjectRoot / Folder))
            {
                GameExportFiles::Copy(Project.ProjectRoot / Folder, StagingDirectory / "Game" / Folder);
            }
        }
        std::filesystem::rename(StagingDirectory, OutputDirectory);
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
