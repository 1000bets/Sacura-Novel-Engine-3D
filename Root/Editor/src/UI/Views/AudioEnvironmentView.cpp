#include "UI/Views/AudioEnvironmentView.h"
#include "UI/Styles/StudioIcons.h"
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

namespace EditorUi
{
namespace
{
QLabel* Label(const QString& Text, const char* Role, QWidget* Parent)
{
    auto* Result = new QLabel(Text, Parent);
    Result->setTextFormat(Qt::PlainText);
    Result->setWordWrap(true);
    Result->setProperty("uiRole", Role);
    return Result;
}
void Columns(QGridLayout& Layout)
{
    Layout.setHorizontalSpacing(15);
    const int Minimums[] = {170, 100, 110, 90};
    const int Weights[] = {120, 65, 60, 100};
    for (int Index = 0; Index < 4; ++Index)
    {
        Layout.setColumnMinimumWidth(Index, Minimums[Index]);
        Layout.setColumnStretch(Index, Weights[Index]);
    }
}
}

AudioEnvironmentView::AudioEnvironmentView(QWidget* Parent) : QWidget(Parent)
{
    setObjectName("AudioEnvironment");
    auto* Layout = new QVBoxLayout(this);
    Layout->setContentsMargins(0, 0, 0, 0);
    Scroll = new QScrollArea(this);
    Scroll->setWidgetResizable(true);
    Scroll->setFrameShape(QFrame::NoFrame);
    auto* Body = new QWidget(Scroll);
    Body->setObjectName("AudioEnvironmentBody");
    BodyLayout = new QVBoxLayout(Body);
    BodyLayout->setContentsMargins(14, 0, 14, 15);
    BodyLayout->setSpacing(0);

    auto* Guide = new QWidget(Body);
    Guide->setObjectName("AudioEnvironmentGuide");
    auto* GuideLayout = new QVBoxLayout(Guide);
    GuideLayout->setContentsMargins(18, 18, 18, 24);
    GuideLayout->setSpacing(12);
    GuideLayout->addWidget(Label(tr("Звук и окружение"), "audioTitle", Guide));
    GuideLayout->addWidget(Label(tr("Музыка, озвучка и состояние мира. Во время запуска здесь можно приостановить или завершить событие."), "audioDescription", Guide));
    auto* Buttons = new QGridLayout();
    Buttons->setSpacing(8);
    const QStringList Titles = {tr("Звуковые файлы и озвучка"), tr("Начальное окружение"), tr("Изменения в сценарии")};
    const QStringList Icons = {"Music2", "CloudSun", "Clapperboard"};
    for (int Index = 0; Index < Titles.size(); ++Index)
    {
        auto* Button = new QPushButton(MakeFrontendIcon(Icons[Index]), Titles[Index], Guide);
        Button->setProperty("uiRole", "audioShortcut");
        Button->setIconSize(QSize(18, 18));
        Buttons->addWidget(Button, 0, Index);
        if (Index < 2)
        {
            Button->setEnabled(false);
            Button->setObjectName(Index == 0 ? "AudioEnvironmentLibrary" : "AudioEnvironmentInitial");
            Button->setToolTip(Index == 0 ? tr("Аудиотека и воспроизведение звука пока не поддерживаются движком.")
                                         : tr("Погода и время суток пока не поддерживаются движком. Освещение задаётся компонентами света сцены."));
        }
        else
        {
            OpenStory = Button;
            OpenStory->setObjectName("AudioEnvironmentOpenStory");
            connect(Button, &QPushButton::clicked, this, [this]()
            {
                if (OpenStoryRequested) { OpenStoryRequested(); }
            });
        }
    }
    Buttons->setColumnStretch(3, 1);
    GuideLayout->addLayout(Buttons);
    Notice = Label({}, "audioDescription", Guide);
    Notice->setObjectName("AudioEnvironmentNotice");
    GuideLayout->addWidget(Notice);
    BodyLayout->addWidget(Guide);

    auto* Header = new QWidget(Body);
    Header->setProperty("uiRole", "audioTableHeader");
    auto* HeaderLayout = new QGridLayout(Header);
    HeaderLayout->setContentsMargins(0, 11, 0, 11);
    Columns(*HeaderLayout);
    const QStringList Headers = {tr("Событие / эффект"), tr("Состояние"), tr("Где действует"), tr("Управление")};
    for (int Index = 0; Index < Headers.size(); ++Index)
    {
        HeaderLayout->addWidget(Label(Headers[Index], "audioHeader", Header), 0, Index);
    }
    BodyLayout->addWidget(Header);
    Rows = new QWidget(Body);
    BodyLayout->addWidget(Rows);
    Footer = Label({}, "audioFooter", Body);
    Footer->setObjectName("AudioEnvironmentFooter");
    BodyLayout->addWidget(Footer);
    BodyLayout->addStretch(1);
    Scroll->setWidget(Body);
    Layout->addWidget(Scroll);
}

void AudioEnvironmentView::SetModel(const AudioEnvironmentModel& Model)
{
    if (bHasModel && CurrentModel == Model)
    {
        return;
    }
    bHasModel = true;
    CurrentModel = Model;
    Notice->setText(Model.Notice);
    Footer->setText(Model.Footer);
    OpenStory->setEnabled(Model.bCanOpenStory);
    OpenStory->setToolTip(Model.bCanOpenStory ? tr("Открыть поток истории и постановку событий")
                                            : tr("Сначала откройте историю в Content"));
    const int ScrollPosition = Scroll->verticalScrollBar()->value();
    auto* Replacement = new QWidget(Rows->parentWidget());
    auto* Layout = new QVBoxLayout(Replacement);
    Layout->setContentsMargins(0, 0, 0, 0);
    Layout->setSpacing(0);
    for (const auto& Row : Model.Rows)
    {
        auto* Item = new QWidget(Replacement);
        Item->setProperty("uiRole", "audioEventRow");
        auto* Grid = new QGridLayout(Item);
        Grid->setContentsMargins(0, 14, 0, 14);
        Columns(*Grid);
        auto* Identity = new QWidget(Item);
        auto* IdentityLayout = new QGridLayout(Identity);
        IdentityLayout->setContentsMargins(0, 0, 0, 0);
        IdentityLayout->setHorizontalSpacing(7);
        IdentityLayout->setVerticalSpacing(5);
        auto* Icon = new QLabel(Identity);
        Icon->setPixmap(MakeFrontendIcon(Row.Icon, "--accent").pixmap(QSize(17, 17)));
        Icon->setFixedWidth(20);
        IdentityLayout->addWidget(Icon, 0, 0);
        auto* Name = Label(Row.Name, "audioEventName", Identity);
        Name->setToolTip(Row.Name);
        IdentityLayout->addWidget(Name, 0, 1);
        auto* Origin = Label(Row.Origin, "audioOrigin", Identity);
        Origin->setToolTip(Row.Origin);
        IdentityLayout->addWidget(Origin, 1, 1);
        IdentityLayout->setColumnStretch(1, 1);
        Grid->addWidget(Identity, 0, 0);
        Grid->addWidget(Label(Row.Status, "audioStatus", Item), 0, 1);
        Grid->addWidget(Label(Row.Scope, "audioScope", Item), 0, 2);
        auto* Controls = new QWidget(Item);
        auto* ControlLayout = new QHBoxLayout(Controls);
        ControlLayout->setContentsMargins(0, 0, 0, 0);
        ControlLayout->setSpacing(6);
        auto* Pause = new QPushButton(MakeFrontendIcon(Row.bPaused ? "Play" : "Pause"), {}, Controls);
        Pause->setProperty("uiRole", "audioControl");
        Pause->setObjectName(QString("AudioEventPause_%1").arg(Row.InstanceId));
        Pause->setEnabled(Row.bCanPause);
        Pause->setToolTip(Row.PauseHint);
        Pause->setAccessibleName(Row.PauseHint);
        connect(Pause, &QPushButton::clicked, this, [this, Identity = Row.InstanceId, bPaused = Row.bPaused]()
        {
            if (PauseRequested) { PauseRequested(Identity, !bPaused); }
        });
        auto* Stop = new QPushButton(MakeFrontendIcon("Square"), {}, Controls);
        Stop->setProperty("uiRole", "audioControl");
        Stop->setObjectName(QString("AudioEventStop_%1").arg(Row.InstanceId));
        Stop->setEnabled(Row.bCanStop);
        Stop->setToolTip(Row.bCanStop ? tr("Остановить событие") : tr("Нет активного события"));
        Stop->setAccessibleName(tr("Остановить событие"));
        connect(Stop, &QPushButton::clicked, this, [this, Identity = Row.InstanceId]()
        {
            if (StopRequested) { StopRequested(Identity); }
        });
        ControlLayout->addWidget(Pause);
        ControlLayout->addWidget(Stop);
        ControlLayout->addStretch();
        Grid->addWidget(Controls, 0, 3);
        Layout->addWidget(Item);
    }
    delete BodyLayout->replaceWidget(Rows, Replacement);
    Rows->hide();
    Rows->deleteLater();
    Rows = Replacement;
    BodyLayout->activate();
    Scroll->verticalScrollBar()->setValue(ScrollPosition);
}
}
