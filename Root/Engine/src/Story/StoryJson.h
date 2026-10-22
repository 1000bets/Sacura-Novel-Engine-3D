#pragma once
#include "Story/StoryTypes.h"
#include <nlohmann/json.hpp>

// Shared by the legacy node codec and the event codec; not a public Engine API.
namespace StoryJson
{
bool ReadAction(const nlohmann::json& Value, StoryActionDefinition& Action, std::string& Error);
nlohmann::json WriteAction(const StoryActionDefinition& Action);
bool ReadEvents(const nlohmann::json& Root, StoryDocument& Document, std::string& Error);
void WriteEvents(nlohmann::json& Root, const StoryDocument& Document);
} // namespace StoryJson
