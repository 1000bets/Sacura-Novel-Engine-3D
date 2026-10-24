#include "Localization/EditorLocalization.h"
#include "Core/IO/AtomicFileWriter.h"
#include "Core/EnginePaths.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <regex>

EditorLocalization& EditorLocalization::Get()
{
    static EditorLocalization Instance;
    return Instance;
}

EditorLocalization::EditorLocalization()
{
    std::ifstream Catalog(EnginePaths::Config() / "EditorLocalization.json", std::ios::binary);
    Entries = nlohmann::json::parse(Catalog).at("entries");
    const char* SettingsRoot = std::getenv("APPDATA");
    if (!SettingsRoot)
    {
        throw std::runtime_error("APPDATA is unavailable for editor preferences");
    }
    SettingsPath = std::filesystem::u8path(SettingsRoot) / "SakuraNovel" / "EditorLocalization.json";
    std::ifstream Input(SettingsPath, std::ios::binary);
    if (Input)
    {
        const auto Values = nlohmann::json::parse(Input, nullptr, false);
        if (Values.is_object())
        {
            const auto Language = Values.value("language", std::string("en"));
            if (Language == "ru")
            {
                CurrentLanguage = Language;
            }
            if (Values.contains("overrides") && Values["overrides"].is_object())
            {
                Overrides = Values["overrides"];
            }
        }
    }
}

nlohmann::json EditorLocalization::Preferences() const
{
    return {{"language", CurrentLanguage}, {"overrides", Overrides}};
}

bool EditorLocalization::ApplyPreferences(const nlohmann::json& Values, std::string& Error)
{
    if (!Values.is_object() || (Values.contains("language") && !Values["language"].is_string()))
    {
        Error = "Localization preferences must be an object with a string language";
        return false;
    }
    const auto Language = Values.value("language", CurrentLanguage);
    if (Language != "en" && Language != "ru")
    {
        Error = "Language must be en or ru";
        return false;
    }
    auto Updated = Values.value("overrides", Overrides);
    if (!Updated.is_object())
    {
        Error = "Translation overrides must be an object";
        return false;
    }
    const auto Slots = [](const std::string& Text)
    {
        static const std::regex Pattern(R"(\{[a-zA-Z_0-9]+\}|%[1-9][0-9]*)");
        std::vector<std::string> Result;
        for (std::sregex_iterator Match(Text.begin(), Text.end(), Pattern), End; Match != End; ++Match)
        {
            Result.push_back(Match->str());
        }
        std::sort(Result.begin(), Result.end());
        return Result;
    };
    for (const auto& [Key, Value] : Updated.items())
    {
        if (!Entries.contains(Key) || !Value.is_object() || !Value.contains("text") || !Value["text"].is_string()
            || Value.value("source", nlohmann::json()) != Entries[Key]["source"])
        {
            Error = "Invalid or outdated translation entry: " + Key;
            return false;
        }
        const auto Text = Value["text"].get<std::string>();
        if (Text.size() > 65536 || (!Text.empty() && Slots(Text) != Slots(Entries[Key]["source"])))
        {
            Error = "Invalid translation placeholders or size: " + Key;
            return false;
        }
    }
    if (!AtomicFileWriter::WriteText(SettingsPath, nlohmann::json{{"language", Language}, {"overrides", Updated}}.dump(), Error))
    {
        return false;
    }
    CurrentLanguage = Language;
    Overrides = std::move(Updated);
    return true;
}
