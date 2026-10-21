#pragma once
#include "UI/Presentation/StoryGraphModel.h"
#include <filesystem>
class StoryGraphDocument;

namespace EditorUi
{
class StoryGraphAdapter
{
public:
    static StoryGraphModel Load(const std::filesystem::path& DocumentPath);
    static StoryGraphModel Build(const StoryGraphDocument& Document);
};
}
