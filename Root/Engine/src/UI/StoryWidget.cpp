#include "UI/StoryWidget.h"
#include "UI/WidgetSurface.h"
#include "UI/WidgetLayout.h"
#include "Story/StoryRuntime.h"

#include <QEvent>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QRegion>
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
        if (OverlayViewport) SyncOverlay();
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
    AuthoredLayout = Layout;
    if (!Authored)
    {
        Speaker->hide(); Line->hide(); Choices->hide(); Advance->hide();
        Authored = new WidgetSurface(this);
        if (OverlayViewport) Authored->setMinimumHeight(0);
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
    if (OverlayViewport) SyncOverlay();
}

void StoryWidget::AttachToViewport(QWidget* Viewport)
{
    if (!Viewport || OverlayViewport == Viewport) return;
    if (OverlayViewport)
    {
        OverlayViewport->removeEventFilter(this);
        OverlayViewport->window()->removeEventFilter(this);
    }
    OverlayViewport = Viewport;
    setParent(Viewport->window(), Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint
        | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    layout()->setContentsMargins(0, 0, 0, 0);
    if (Authored) Authored->setMinimumHeight(0);
    Viewport->installEventFilter(this);
    Viewport->window()->installEventFilter(this);
    hide();
    SyncOverlay();
}

void StoryWidget::SetOverlayVisible(bool bVisible)
{
    bOverlayRequested = bVisible;
    SyncOverlay();
}

bool StoryWidget::eventFilter(QObject* Watched, QEvent* Event)
{
    if (OverlayViewport && (Watched == OverlayViewport || Watched == OverlayViewport->window()))
    {
        switch (Event->type())
        {
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::Hide:
        case QEvent::WindowStateChange:
            SyncOverlay();
            break;
        default: break;
        }
    }
    return QWidget::eventFilter(Watched, Event);
}

void StoryWidget::SyncOverlay()
{
    if (!OverlayViewport) return;
    const bool bShouldShow = bOverlayRequested && Runtime.GetPlaybackPhase() != StoryPlaybackPhase::Idle
        && OverlayViewport->isVisible() && OverlayViewport->window()->isVisible()
        && !OverlayViewport->window()->isMinimized();
    if (!bShouldShow) { hide(); return; }

    const QRect ViewportRect(OverlayViewport->mapToGlobal(QPoint()), OverlayViewport->size());
    if (geometry() != ViewportRect) setGeometry(ViewportRect);
    if (!AuthoredLayout.isEmpty())
    {
        QRegion PaintedRegion;
        const auto Rects = WidgetLayout::Rects(AuthoredLayout, size());
        for (const auto& Value : AuthoredLayout["elements"].toArray())
        {
            const auto Element = Value.toObject();
            if (!WidgetPainting::Visible(Element, AuthoredLayout)) continue;
            if (Element["type"] == "choices" && !Runtime.CanSelectChoice()) continue;
            if (Element["type"] == "button" && Element["action"] == "advance"
                && !Runtime.CanAdvanceDialogue()) continue;
            PaintedRegion += Rects.value(Element["id"].toString()).toAlignedRect();
        }
        if (mask() != PaintedRegion) setMask(PaintedRegion);
    }
    if (!isVisible()) show();
}
