#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

class EditorLocalization
{
public:
    static EditorLocalization& Get();
    nlohmann::json Preferences() const;
    bool ApplyPreferences(const nlohmann::json& Values, std::string& Error);
private:
    EditorLocalization();
    nlohmann::json Entries;
    nlohmann::json Overrides = nlohmann::json::object();
    std::string CurrentLanguage = "en";
    std::filesystem::path SettingsPath;
};
