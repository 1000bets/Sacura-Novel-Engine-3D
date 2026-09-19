#include "Project/ProjectGenerator.h"

#include <fstream>

bool ProjectGenerator::CreateProject(
    const ProjectGeneratorRequest& Request,
    ProjectDescriptor& OutDescriptor,
    std::string& OutError)
{
    if (Request.ProjectName.empty())
    {
        OutError = "Project name is empty";
        return false;
    }
    if (Request.ParentDirectory.empty())
    {
        OutError = "Parent directory is empty";
        return false;
    }

    const std::filesystem::path ProjectRoot = Request.ParentDirectory / Request.ProjectName;
    std::error_code Error;
    if (std::filesystem::exists(ProjectRoot, Error))
    {
        OutError = "Project directory already exists";
        return false;
    }

    const std::filesystem::path Folders[] = {
        ProjectRoot / "Content" / "Scenes",
        ProjectRoot / "Content" / "Materials",
        ProjectRoot / "Content" / "Textures",
        ProjectRoot / "Content" / "Models",
        ProjectRoot / "Content" / "Audio",
        ProjectRoot / "Scripts",
        ProjectRoot / "Config",
    };

    for (const std::filesystem::path& Folder : Folders)
    {
        std::filesystem::create_directories(Folder, Error);
        if (Error)
        {
            OutError = "Failed to create project folders: " + Error.message();
            return false;
        }
    }

    OutDescriptor = ProjectDescriptor{};
    OutDescriptor.Name = Request.ProjectName;
    OutDescriptor.EngineVersion = Request.EngineVersion.empty() ? "0.1" : Request.EngineVersion;
    OutDescriptor.TemplateId = Request.TemplateId.empty() ? "Empty" : Request.TemplateId;
    OutDescriptor.ProjectRoot = ProjectRoot;
    OutDescriptor.ProjectFile = ProjectRoot / (Request.ProjectName + ".project");
    if (!Request.StartupScene.empty())
    {
        OutDescriptor.StartupScene = ProjectRoot / Request.StartupScene;
    }

    if (!OutDescriptor.TrySaveToFile(OutError))
    {
        return false;
    }

    const std::filesystem::path GitKeep = ProjectRoot / "Content" / "Scenes" / ".gitkeep";
    std::ofstream KeepFile(GitKeep);
    KeepFile << '\n';

    if (OutDescriptor.TemplateId == "VisualNovel" || OutDescriptor.TemplateId == "KineticNovel")
    {
        const std::filesystem::path StarterScript = ProjectRoot / "Scripts" / "story_controller.py";
        std::ofstream ScriptFile(StarterScript);
        ScriptFile <<
            "from engine import ScriptComponent, register_class, field\n"
            "\n"
            "\n"
            "@register_class(type_id=\"game.StoryController\", schema_version=1)\n"
            "class StoryController(ScriptComponent):\n"
            "    pace = field(\n"
            "        float,\n"
            "        property_id=\"pace\",\n"
            "        default=1.0,\n"
            "        serializable=True,\n"
            "        editable=True,\n"
            "        display_name=\"Pace\",\n"
            "    )\n"
            "\n"
            "    def on_create(self):\n"
            "        pass\n"
            "\n"
            "    def on_update(self, delta_time):\n"
            "        pass\n"
            "\n"
            "    def on_destroy(self):\n"
            "        pass\n";
    }

    return true;
}
