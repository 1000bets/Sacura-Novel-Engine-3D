#pragma once

class EditorToolset;
class Engine;
namespace EditorUi
{
class StoryGraphController;
}

void RegisterNarrativeTools(EditorToolset& Toolset, Engine& Runtime, EditorUi::StoryGraphController& Controller);
