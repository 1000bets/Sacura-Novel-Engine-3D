#include "Project/ProjectGenerator.h"
#include "ProjectTemplateContent.h"

#include "Story/StoryDocumentIO.h"
#include "World/Serialization/SceneSerializer.h"

#include <fstream>
#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>

namespace
{
bool ContainsPathSeparator(const std::string& Value)
{
    return Value.find('/') != std::string::npos || Value.find('\\') != std::string::npos;
}

bool IsPathInsideRoot(const std::filesystem::path& Root, const std::filesystem::path& Candidate)
{
    std::error_code Error;
    const std::filesystem::path CanonicalRoot = std::filesystem::weakly_canonical(Root, Error);
    if (Error)
    {
        return false;
    }

    const std::filesystem::path CanonicalCandidate = std::filesystem::weakly_canonical(Candidate, Error);
    if (Error)
    {
        return false;
    }

    const std::filesystem::path Relative = CanonicalCandidate.lexically_relative(CanonicalRoot);
    if (Relative.empty())
    {
        return false;
    }

    for (const std::filesystem::path& Part : Relative)
    {
        if (Part == "..")
        {
            return false;
        }
    }

    return true;
}

bool IsSafeProjectName(const std::string& ProjectName)
{
    if (ProjectName.empty())
    {
        return false;
    }
    if (ContainsPathSeparator(ProjectName))
    {
        return false;
    }
    if (ProjectName == "." || ProjectName == "..")
    {
        return false;
    }
    if (ProjectName.find("..") != std::string::npos)
    {
        return false;
    }

    if(ProjectName.find_first_of("<>:\"|?*")!=std::string::npos || ProjectName.back()=='.' || ProjectName.back()==' ') return false;
    for(unsigned char C:ProjectName) if(C<32) return false;
    const std::filesystem::path AsPath=std::filesystem::u8path(ProjectName);
    if (AsPath.is_absolute() || AsPath.has_root_name() || AsPath.has_root_directory())
    {
        return false;
    }

    return AsPath.filename() == AsPath;
}

std::string MakeEmptySceneJson(const std::string& SceneName)
{
    return nlohmann::json{{"format",SceneSerializer::FormatId},{"formatVersion",SceneSerializer::FormatVersion},
        {"name",SceneName},{"objects",nlohmann::json::array()}}.dump(2);
}
}

bool ProjectGenerator::CreateProject(
    const ProjectGeneratorRequest& Request,
    ProjectDescriptor& OutDescriptor,
    std::string& OutError)
{
    if (!IsSafeProjectName(Request.ProjectName))
    {
        OutError = "Project name must be a single path segment inside the parent directory";
        return false;
    }
    if (Request.ParentDirectory.empty())
    {
        OutError = "Parent directory is empty";
        return false;
    }

    std::error_code Error;
    const std::filesystem::path ParentAbsolute = std::filesystem::weakly_canonical(Request.ParentDirectory, Error);
    if (Error)
    {
        OutError = "Parent directory is invalid";
        return false;
    }

    const std::filesystem::path ProjectRoot = (ParentAbsolute / std::filesystem::u8path(Request.ProjectName)).lexically_normal();
    if (!IsPathInsideRoot(ParentAbsolute, ProjectRoot))
    {
        OutError = "Project directory must stay inside the parent directory";
        return false;
    }

    if (std::filesystem::exists(ProjectRoot, Error))
    {
        OutError = "Project directory already exists";
        return false;
    }
    if(!Request.StartupScene.empty() && !IsPathInsideRoot(ProjectRoot,ProjectRoot/std::filesystem::u8path(Request.StartupScene))) {
        OutError="startupScene must stay inside the project root";return false;
    }
    if(!Request.EngineVersion.empty() && Request.EngineVersion!="0.1") {OutError="Unsupported project engineVersion";return false;}
    const std::string TemplateId = Request.TemplateId.empty() ? "Empty" : Request.TemplateId;
    if (!IsBuiltInProjectTemplate(TemplateId))
    {
        OutError = "Unknown project template: " + TemplateId;
        return false;
    }
    if (TemplateId != "Empty" && Request.StartupScene.empty())
    {
        OutError = "Project templates require a startup scene";
        return false;
    }

    const std::filesystem::path Folders[] = {
        ProjectRoot / "Content" / "Scenes",
        ProjectRoot / "Content" / "Materials",
        ProjectRoot / "Content" / "Textures",
        ProjectRoot / "Content" / "Models",
        ProjectRoot / "Content" / "Audio",
        ProjectRoot / "Content" / "Stories",
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

    ProjectDescriptor Parsed{};
    Parsed.Name = Request.ProjectName;
    Parsed.EngineVersion = Request.EngineVersion.empty() ? "0.1" : Request.EngineVersion;
    Parsed.TemplateId = TemplateId;
    Parsed.ProjectRoot = ProjectRoot;
    Parsed.ProjectFile = ProjectRoot / std::filesystem::u8path(Request.ProjectName + ".project");
    if (!Request.StartupScene.empty())
    {
        const std::filesystem::path StartupAbsolute = (ProjectRoot / Request.StartupScene).lexically_normal();
        if (!IsPathInsideRoot(ProjectRoot, StartupAbsolute))
        {
            OutError = "startupScene must stay inside the project root";
            return false;
        }
        Parsed.StartupScene = StartupAbsolute;

        std::string SceneJson = MakeEmptySceneJson(StartupAbsolute.stem().string());
        StoryDocument StarterStory;
        std::string Guide;
        if (TemplateId != "Empty"
            && !BuildProjectTemplateContent(TemplateId, SceneJson, StarterStory, Guide))
        {
            OutError = "Could not build project template";
            return false;
        }
        const SceneSerializeResult Written = SceneSerializer::WriteTextFileAtomically(StartupAbsolute, SceneJson);
        if (!Written.bOk)
        {
            OutError = Written.Error;
            return false;
        }
        if (TemplateId != "Empty")
        {
            std::string SceneStem = StartupAbsolute.stem().u8string();
            std::transform(SceneStem.begin(), SceneStem.end(), SceneStem.begin(),
                [](unsigned char Character) { return static_cast<char>(std::tolower(Character)); });
            const std::string StoryName = SceneStem == "opening" ? "OpeningStory.story" : "Opening.story";
            Parsed.StartupStory = ProjectRoot / "Content" / "Stories" / StoryName;
            if (StoryName != "Opening.story")
            {
                const std::string Original = "Opening.story";
                const size_t Position = Guide.find(Original);
                if (Position != std::string::npos)
                {
                    Guide.replace(Position, Original.size(), StoryName);
                }
            }
            const StorySerializeResult StoryWritten = StoryDocumentIO::SaveToFile(StarterStory, Parsed.StartupStory);
            if (!StoryWritten.bOk)
            {
                OutError = StoryWritten.Error;
                return false;
            }
            const SceneSerializeResult GuideWritten = SceneSerializer::WriteTextFileAtomically(ProjectRoot / "README.md", Guide);
            if (!GuideWritten.bOk)
            {
                OutError = GuideWritten.Error;
                return false;
            }
        }
    }

    if (!Parsed.TrySaveToFile(OutError))
    {
        return false;
    }

    if (Parsed.TemplateId == "VisualNovel" || Parsed.TemplateId == "KineticNovel")
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

    OutDescriptor = std::move(Parsed);
    return true;
}
