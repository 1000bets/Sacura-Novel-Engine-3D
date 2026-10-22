#include "UI/StoryWidget.h"
#include "UI/WidgetSurface.h"
#include "Story/StoryRuntime.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

StoryWidget::StoryWidget(StoryRuntime& InRuntime, QWidget* Parent)
    : QWidget(Parent), Runtime(InRuntime)
{
    auto* Layout = new QVBoxLayout(this);
    Speaker = new QLabel(this);
    Line = new QLabel(this);
    Speaker->setTextFormat(Qt::PlainText);
    Line->setTextFormat(Qt::PlainText);
    Line->setWordWrap(true);
    Choices = new QWidget(this);
    ChoiceLayout = new QVBoxLayout(Choices);
    ChoiceLayout->setContentsMargins(0, 0, 0, 0);
    Advance = new QPushButton(tr("Continue"), this);
    connect(Advance, &QPushButton::clicked, this, [this]()
    {
        Runtime.AdvanceDialogue();
        Refresh(isEnabled());
    });
    Layout->addWidget(Speaker);
    Layout->addWidget(Line);
    Layout->addWidget(Choices);
    Layout->addWidget(Advance);
    Refresh(false);
}

void StoryWidget::Refresh(bool bAllowInput)
{
    setEnabled(bAllowInput);
    if (Authored)
    {
        WidgetContext Context;
        Context.Speaker = QString::fromStdString(Runtime.GetDisplayedSpeaker());
        Context.Text = QString::fromStdString(Runtime.GetDisplayedLine());
        if (Runtime.IsFinished()) Context.Text = tr("История завершена.");
        else if (!Runtime.GetLastError().empty()) Context.Text = QString::fromStdString(Runtime.GetLastError());
        Context.CanAdvance = Runtime.CanAdvanceDialogue();
        Context.CanChoose = Runtime.CanSelectChoice();
        Context.Status = Context.CanAdvance ? tr("Нажмите, чтобы продолжить") : QString();
        for (size_t I=0; I<Runtime.GetChoiceCount(); ++I) Context.Choices.append(QString::fromStdString(Runtime.GetChoice(I).Label));
        Authored->SetContext(std::move(Context));
        return;
    }
    Speaker->setText(QString::fromStdString(Runtime.GetDisplayedSpeaker()));
    QString Text = QString::fromStdString(Runtime.GetDisplayedLine());
    if (Runtime.IsFinished())
    {
        Text = tr("The story has ended.");
    }
    else if (!Runtime.GetLastError().empty())
    {
        Text = QString::fromStdString(Runtime.GetLastError());
    }
    Line->setText(Text);
    Advance->setVisible(Runtime.CanAdvanceDialogue());
    Choices->setEnabled(Runtime.CanSelectChoice());
    QStringList ChoiceLabels;
    for (size_t ChoiceIndex = 0; ChoiceIndex < Runtime.GetChoiceCount(); ++ChoiceIndex)
    {
        ChoiceLabels.push_back(QString::fromStdString(Runtime.GetChoice(ChoiceIndex).Label));
    }
    if (ChoiceLabels != DisplayedChoices)
    {
        while (QLayoutItem* Item = ChoiceLayout->takeAt(0))
        {
            Item->widget()->hide();
            Item->widget()->deleteLater();
            delete Item;
        }
        DisplayedChoices = ChoiceLabels;
        for (int ChoiceIndex = 0; ChoiceIndex < ChoiceLabels.size(); ++ChoiceIndex)
        {
            auto* Choice = new QPushButton(ChoiceLabels[ChoiceIndex], Choices);
            connect(Choice, &QPushButton::clicked, this, [this, ChoiceIndex]()
            {
                Runtime.SelectChoice(static_cast<size_t>(ChoiceIndex));
                Refresh(isEnabled());
            });
            ChoiceLayout->addWidget(Choice);
        }
    }
}

void StoryWidget::SetWidgetLayout(const QJsonObject& Layout)
{
    if (Layout.isEmpty()) return;
    if (!Authored)
    {
        Speaker->hide(); Line->hide(); Choices->hide(); Advance->hide();
        Authored = new WidgetSurface(this);
        layout()->addWidget(Authored);
        Authored->Activated = [this](QString Action, int Index)
        {
            if (!isEnabled()) return;
            if (Action == "advance") Runtime.AdvanceDialogue();
            else if (Action == "choice") Runtime.SelectChoice(static_cast<size_t>(Index));
            Refresh(isEnabled());
        };
    }
    Authored->SetWidget(Layout);
}
