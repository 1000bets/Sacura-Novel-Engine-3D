#pragma once
#include "Story/StoryTypes.h"

// Finite native event profile. Validation is shared by document edits and Play.
bool ValidateStoryEvents(const StoryDocument& Document, std::string& Error);
const StoryEventDefinition* FindStoryEvent(const StoryDocument& Document, const std::string& Identity);
StoryEventDefinition ResolveStoryEvent(const StoryEventDefinition& Event, const StoryEventBinding& Binding);
