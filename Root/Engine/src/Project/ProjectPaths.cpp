#include "Project/ProjectPaths.h"

std::filesystem::path ProjectPaths::ProjectRootPath{};

void ProjectPaths::SetRoot(const std::filesystem::path& ProjectRoot)
{
    std::error_code Error;
    ProjectRootPath = std::filesystem::weakly_canonical(ProjectRoot, Error);
    if (Error)
    {
        ProjectRootPath = std::filesystem::absolute(ProjectRoot).lexically_normal();
    }
}

void ProjectPaths::Clear()
{
    ProjectRootPath.clear();
}

bool ProjectPaths::IsOpen()
{
    return !ProjectRootPath.empty();
}

const std::filesystem::path& ProjectPaths::Root()
{
    return ProjectRootPath;
}

std::filesystem::path ProjectPaths::Content()
{
    return ProjectRootPath / "Content";
}

std::filesystem::path ProjectPaths::Scripts()
{
    return ProjectRootPath / "Scripts";
}

std::filesystem::path ProjectPaths::Config()
{
    return ProjectRootPath / "Config";
}
