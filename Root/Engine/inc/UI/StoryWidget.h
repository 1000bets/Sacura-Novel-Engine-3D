#pragma once

#include <QWidget>
#include <QStringList>
#include <QJsonObject>

class StoryRuntime;
class QLabel;
class QPushButton;
class QVBoxLayout;
class WidgetSurface;

class StoryWidget : public QWidget
{
public:
    explicit StoryWidget(StoryRuntime& Runtime, QWidget* Parent = nullptr);
    void Refresh(bool bAllowInput);
    void SetWidgetLayout(const QJsonObject& Layout);

private:
    StoryRuntime& Runtime;
    QLabel* Speaker = nullptr;
    QLabel* Line = nullptr;
    QPushButton* Advance = nullptr;
    QWidget* Choices = nullptr;
    QVBoxLayout* ChoiceLayout = nullptr;
    QStringList DisplayedChoices;
    WidgetSurface* Authored = nullptr;
};
