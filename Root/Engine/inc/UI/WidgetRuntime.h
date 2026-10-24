#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>

class Engine;

class WidgetRuntime
{
public:
    static bool Start(Engine& Runtime, const std::filesystem::path& ProjectFile, std::string& Error);
    static void Stop();
    static void NarrativeAction(const std::string& Action);
    static void Update(Engine& Runtime);
    static nlohmann::json Snapshot();
    static nlohmann::json Slots();
    static nlohmann::json GameViews();
    static nlohmann::json Controllers();
    static bool ValidateController(const nlohmann::json& Document, std::string& Error);
    static bool Dispatch(const std::string& Instance, const std::string& Element, const nlohmann::json& Value, std::string& Error);
};
