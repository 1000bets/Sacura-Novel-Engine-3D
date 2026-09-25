#include "Story/StoryRuntime.h"
#include "UI/StoryWidget.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QPushButton>
#include <iostream>

int main(int ArgumentCount, char** Arguments)
{
    QApplication Application(ArgumentCount, Arguments);
    StoryDocument Document;
    Document.Name = "Interface test";
    Document.StartNodeId = "intro";
    StoryNode Intro;
    Intro.Id = "intro";
    Intro.Kind = StoryNodeKind::Line;
    Intro.Text = "Choose your route";
    Intro.NextNodeId = "choice";
    StoryNode Choice;
    Choice.Id = "choice";
    Choice.Kind = StoryNodeKind::Choice;
    Choice.Choices = {{"First route", "first"}, {"Second route", "second"}};
    StoryNode First;
    First.Id = "first";
    First.Kind = StoryNodeKind::Line;
    First.Text = "First outcome";
    StoryNode Second = First;
    Second.Id = "second";
    Second.Text = "Second outcome";
    Document.Nodes = {Intro, Choice, First, Second};
    StoryRuntime Runtime;
    if (!Runtime.LoadDocument(Document))
    {
        return 1;
    }
    StoryWidget Widget(Runtime);
    Widget.Refresh(true);
    QPushButton* Advance = Widget.findChild<QPushButton*>();
    if (Advance == nullptr)
    {
        return 1;
    }
    Advance->click();
    if (Runtime.GetPlaybackPhase() != StoryPlaybackPhase::Choice)
    {
        return 1;
    }
    const auto InitialButtons = Widget.findChildren<QPushButton*>();
    Widget.Refresh(true);
    if (InitialButtons != Widget.findChildren<QPushButton*>())
    {
        std::cout << "Unchanged choices recreated their widgets\n";
        return 1;
    }
    Widget.Refresh(false);
    for (QPushButton* Button : Widget.findChildren<QPushButton*>())
    {
        if (Button->text() == "Second route")
        {
            Button->click();
        }
    }
    if (Runtime.GetPlaybackPhase() != StoryPlaybackPhase::Choice)
    {
        std::cout << "Paused UI advanced the story\n";
        return 1;
    }
    Widget.Refresh(true);
    for (QPushButton* Button : Widget.findChildren<QPushButton*>())
    {
        if (Button->text() == "Second route")
        {
            Button->click();
            break;
        }
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    if (Runtime.GetDisplayedLine() != "Second outcome")
    {
        std::cout << "The chosen route did not reach its outcome\n";
        return 1;
    }
    std::cout << "Story widget interaction passed\n";
    return 0;
}
