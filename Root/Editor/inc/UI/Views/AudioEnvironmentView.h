#pragma once

#include "UI/Presentation/AudioEnvironmentModel.h"
#include <QWidget>
#include <functional>

class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

namespace EditorUi
{
class AudioEnvironmentView final : public QWidget
{
public:
    explicit AudioEnvironmentView(QWidget* Parent = nullptr);
    void SetModel(const AudioEnvironmentModel& Model);
    std::function<void()> OpenStoryRequested;
    std::function<void(uint64_t, bool)> PauseRequested;
    std::function<void(uint64_t)> StopRequested;

private:
    QLabel* Notice = nullptr;
    QLabel* Footer = nullptr;
    QPushButton* OpenStory = nullptr;
    QScrollArea* Scroll = nullptr;
    QWidget* Rows = nullptr;
    QVBoxLayout* BodyLayout = nullptr;
    AudioEnvironmentModel CurrentModel;
    bool bHasModel = false;
};
}
