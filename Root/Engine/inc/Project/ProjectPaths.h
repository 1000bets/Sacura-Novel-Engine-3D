#pragma once

#include <filesystem>

class ProjectPaths
{
public:
    static void SetRoot(const std::filesystem::path& ProjectRoot);
    static void Clear();
    static bool IsOpen();

    static const std::filesystem::path& Root();
    static std::filesystem::path Content();
    static std::filesystem::path Scripts();
    static std::filesystem::path Config();

private:
    static std::filesystem::path ProjectRootPath;
};
