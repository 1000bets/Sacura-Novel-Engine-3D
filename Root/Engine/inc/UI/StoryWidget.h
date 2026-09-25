#pragma once

#include <QWidget>
#include <QStringList>

class StoryRuntime;
class QLabel;
class QPushButton;
class QVBoxLayout;

class StoryWidget : public QWidget
{
public:
    explicit StoryWidget(StoryRuntime& Runtime, QWidget* Parent = nullptr);
    void Refresh(bool bAllowInput);

private:
    StoryRuntime& Runtime;
    QLabel* Speaker = nullptr;
    QLabel* Line = nullptr;
    QPushButton* Advance = nullptr;
    QWidget* Choices = nullptr;
    QVBoxLayout* ChoiceLayout = nullptr;
    QStringList DisplayedChoices;
};
