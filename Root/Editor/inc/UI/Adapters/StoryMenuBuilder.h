#pragma once
#include "UI/Presentation/StudioMenuItem.h"
#include "UI/Presentation/StoryGraphModel.h"

namespace EditorUi
{
// One catalog for toolbar and canvas; stable IDs are independent of translated labels.
QVector<StudioMenuItem> BuildStoryNodeMenu(std::function<void(int)> AddNode,
                                         std::function<void()> AddEvent, bool CanAddEvent, std::function<void(QString)> AddNarrative = {});
QVector<StudioMenuItem> BuildStoryEventMenu(const QVector<StoryGraphEvent>& Events,
                                          std::function<void(QString)> AddEvent,
                                          std::function<void()> ImportEvent = {});
}
