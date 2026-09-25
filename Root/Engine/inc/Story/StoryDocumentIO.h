#pragma once

#include "Story/StoryTypes.h"

#include <filesystem>
#include <string>

class StoryDocumentIO
{
public:
    static constexpr const char* FormatId = "sakura.story";
    static constexpr uint32_t FormatVersion = 1;

    static StorySerializeResult LoadFromFile(const std::filesystem::path& AbsolutePath, StoryDocument& OutDocument);
    static StorySerializeResult LoadFromJson(const std::string& JsonText, StoryDocument& OutDocument);
    static StorySerializeResult SaveToFile(const StoryDocument& Document, const std::filesystem::path& AbsolutePath);
    static StorySerializeResult SaveToJson(const StoryDocument& Document, std::string& OutJsonText);
};
