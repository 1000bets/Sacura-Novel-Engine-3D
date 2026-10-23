#pragma once

#include "Story/StoryTypes.h"

#include <string>

// Built-in starter content uses the native scene and story document formats.
// IDs match the launcher catalog; project names and paths are supplied by the caller.
bool IsBuiltInProjectTemplate(const std::string& TemplateId);
bool BuildProjectTemplateContent(
    const std::string& TemplateId,
    std::string& OutSceneJson,
    StoryDocument& OutStory,
    std::string& OutGuide);
