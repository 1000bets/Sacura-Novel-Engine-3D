#include "UI/StoryWidget.h"
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
