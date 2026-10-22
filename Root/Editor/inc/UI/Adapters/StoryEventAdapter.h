#pragma once
#include "Story/StoryTypes.h"
#include "UI/Presentation/StoryGraphModel.h"

namespace EditorUi
{
struct StoryEventAdapter
{
    static StoryGraphAction ToView(const StoryActionDefinition& Action);
    static StoryActionDefinition ToDefinition(const StoryGraphAction& Action);
    static StoryGraphEvent ToView(const StoryEventDefinition& Event);
    static StoryEventDefinition ToDefinition(const StoryGraphEvent& Event);
};
} // namespace EditorUi
