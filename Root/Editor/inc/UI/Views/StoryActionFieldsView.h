#pragma once
#include "UI/Presentation/StoryGraphModel.h"
#include <QWidget>
#include <functional>
#include <vector>
namespace EditorUi
{
class StoryActionFieldsView final : public QWidget
{
  public:
    StoryActionFieldsView(StoryGraphAction Action, const StoryGraphModel& Model, QWidget* Parent = nullptr);
    void Commit();
    std::function<void(StoryGraphAction)> Changed;

  private:
    StoryGraphAction Value;
    std::vector<std::function<void()>> Collectors;
};
} // namespace EditorUi
