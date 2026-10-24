#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>

class WidgetDefinition
{
public:
    static bool Validate(const nlohmann::json& Document, std::string& Error);
    static bool Read(const std::filesystem::path& Path, nlohmann::json& Document, std::string& Error);
};
