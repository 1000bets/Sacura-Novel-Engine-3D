#pragma once
#include "UI/Presentation/AudioEnvironmentModel.h"
#include "Story/StoryTypes.h"

namespace EditorUi
{
AudioEnvironmentModel BuildAudioEnvironmentModel(const StoryDocument* Story,
    const std::vector<StoryEventSnapshot>& Events, bool bPlaying, bool bPaused, const std::string& Error);
}
