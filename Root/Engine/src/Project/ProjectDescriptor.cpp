#include "Project/ProjectDescriptor.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace
{
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

bool TryReadRequiredString(
    const nlohmann::json& Document,
    const char* FieldName,
    std::string& OutValue,
    std::string& OutError)
{
    if (!Document.contains(FieldName))
    {
        OutValue.clear();
        return true;
    }

    const nlohmann::json& Field = Document.at(FieldName);
    if (!Field.is_string())
    {
        OutError = std::string("Project field '") + FieldName + "' must be a string";
        return false;
    }

    OutValue = Field.get<std::string>();
    return true;
}
}

bool ProjectDescriptor::TryLoadFromFile(
    const std::filesystem::path& InProjectFile,
    ProjectDescriptor& OutDescriptor,
    std::string& OutError)
{
    std::error_code Error;
    const std::filesystem::path AbsoluteProjectFile = std::filesystem::weakly_canonical(InProjectFile, Error);
    if (Error || !std::filesystem::exists(AbsoluteProjectFile))
    {
        OutError = "Project file not found";
        return false;
    }

    std::ifstream Input(AbsoluteProjectFile);
    if (!Input)
    {
        OutError = "Failed to open project file";
        return false;
    }

    nlohmann::json Document;
    try
    {
        Input >> Document;
    }
    catch (const std::exception& Exception)
    {
        OutError = std::string("Invalid project JSON: ") + Exception.what();
        return false;
    }

    if (!Document.is_object())
    {
        OutError = "Project file root must be a JSON object";
        return false;
    }

    ProjectDescriptor Parsed{};
    Parsed.ProjectFile = AbsoluteProjectFile;
    Parsed.ProjectRoot = AbsoluteProjectFile.parent_path();

    if (!TryReadRequiredString(Document, "name", Parsed.Name, OutError))
    {
        return false;
    }
    if (Parsed.Name.empty())
    {
        Parsed.Name = AbsoluteProjectFile.stem().string();
    }

    if (!TryReadRequiredString(Document, "engineVersion", Parsed.EngineVersion, OutError))
    {
        return false;
    }
    if (Parsed.EngineVersion.empty())
    {
        Parsed.EngineVersion = "0.1";
    }

    if (!TryReadRequiredString(Document, "template", Parsed.TemplateId, OutError))
    {
        return false;
    }
    if (Parsed.TemplateId.empty())
    {
        Parsed.TemplateId = "Empty";
    }

    if (Document.contains("startupScene"))
    {
        const nlohmann::json& StartupField = Document.at("startupScene");
        if (!StartupField.is_string())
        {
            OutError = "Project field 'startupScene' must be a string";
            return false;
        }

        const std::string StartupRelative = StartupField.get<std::string>();
        if (!StartupRelative.empty())
        {
            const std::filesystem::path StartupAbsolute =
                (Parsed.ProjectRoot / StartupRelative).lexically_normal();
            if (!IsPathInsideRoot(Parsed.ProjectRoot, StartupAbsolute))
            {
                OutError = "startupScene must stay inside the project root";
                return false;
            }
            Parsed.StartupScene = StartupAbsolute;
        }
    }

    if (Document.contains("startupStory"))
    {
        const nlohmann::json& StartupField = Document.at("startupStory");
        if (!StartupField.is_string())
        {
            OutError = "Project field 'startupStory' must be a string";
            return false;
        }

        const std::string StartupRelative = StartupField.get<std::string>();
        if (!StartupRelative.empty())
        {
            const std::filesystem::path StartupAbsolute =
                (Parsed.ProjectRoot / StartupRelative).lexically_normal();
            if (!IsPathInsideRoot(Parsed.ProjectRoot, StartupAbsolute))
            {
                OutError = "startupStory must stay inside the project root";
                return false;
            }
            Parsed.StartupStory = StartupAbsolute;
        }
    }

    OutDescriptor = std::move(Parsed);
    return true;
}

bool ProjectDescriptor::TrySaveToFile(std::string& OutError) const
{
    if (ProjectFile.empty())
    {
        OutError = "Project file path is empty";
        return false;
    }

    if (!StartupScene.empty() && !IsPathInsideRoot(ProjectRoot, StartupScene))
    {
        OutError = "startupScene must stay inside the project root";
        return false;
    }

    if (!StartupStory.empty() && !IsPathInsideRoot(ProjectRoot, StartupStory))
    {
        OutError = "startupStory must stay inside the project root";
        return false;
    }

    nlohmann::json Document;
    Document["name"] = Name;
    Document["engineVersion"] = EngineVersion.empty() ? "0.1" : EngineVersion;
    Document["template"] = TemplateId.empty() ? "Empty" : TemplateId;

    if (!StartupScene.empty())
    {
        std::error_code RelativeError;
        std::filesystem::path RelativeStartup = std::filesystem::relative(StartupScene, ProjectRoot, RelativeError);
        if (RelativeError)
        {
            OutError = "Failed to relativize startupScene";
            return false;
        }
        Document["startupScene"] = RelativeStartup.generic_string();
    }

    if (!StartupStory.empty())
    {
        std::error_code RelativeError;
        std::filesystem::path RelativeStartup = std::filesystem::relative(StartupStory, ProjectRoot, RelativeError);
        if (RelativeError)
        {
            OutError = "Failed to relativize startupStory";
            return false;
        }
        Document["startupStory"] = RelativeStartup.generic_string();
    }

    std::error_code DirectoryError;
    std::filesystem::create_directories(ProjectFile.parent_path(), DirectoryError);

    const std::filesystem::path TemporaryFile = ProjectFile.string() + ".tmp";
    {
        std::ofstream Output(TemporaryFile, std::ios::binary | std::ios::trunc);
        if (!Output)
        {
            OutError = "Failed to write temporary project file";
            return false;
        }

        Output << Document.dump(2);
        if (!Output)
        {
            OutError = "Failed while writing project JSON";
            return false;
        }
    }

    std::error_code RenameError;
    std::filesystem::rename(TemporaryFile, ProjectFile, RenameError);
    if (RenameError)
    {
        std::filesystem::remove(TemporaryFile);
        OutError = "Failed to replace project file";
        return false;
    }

    return true;
}
