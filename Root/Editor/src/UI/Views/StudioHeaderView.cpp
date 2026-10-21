#include "UI/Views/StudioHeaderView.h"
#include "UI/Styles/StudioIcons.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QPushButton>

namespace EditorUi
{
StudioHeaderView::StudioHeaderView(QMenuBar& Menus, QLabel& ProjectTitle, QWidget* Parent) : QWidget(Parent)
{
    setObjectName("StudioHeader");
    setFixedHeight(44);
    auto* Layout = new QHBoxLayout(this);
    Layout->setContentsMargins(12, 0, 12, 0);
    Layout->setSpacing(4);
    auto* Mark = new QLabel(this);
    Mark->setPixmap(MakeStudioIcon(StudioIcon::Flower, "--brand-mark").pixmap(18, 18));
    Layout->addWidget(Mark);
    auto* Brand = new QLabel(QStringLiteral("Sacura"), this);
    Brand->setObjectName("EditorBrand");
    Layout->addWidget(Brand);
    Layout->addSpacing(16);
    Menus.setNativeMenuBar(false);
    Menus.setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    Layout->addWidget(&Menus);
    Layout->addStretch(1);
    ProjectTitle.setObjectName("ProjectTitle");
    ProjectTitle.setMaximumWidth(220);
    Layout->addWidget(&ProjectTitle);
    Layout->addSpacing(20);
    auto MakeButton = [&](const QString& Text, StudioIcon Icon, const QString& Hint)
    {
        auto* Button = new QPushButton(MakeStudioIcon(Icon), Text, this);
        Button->setProperty("uiRole", "chromeButton");
        Button->setToolTip(Hint);
        Button->setIconSize(QSize(14, 14));
        Layout->addWidget(Button);
        return Button;
    };
    Play = MakeButton(tr("Запуск"), StudioIcon::Play, tr("Запустить / остановить предпросмотр"));
    Play->setObjectName("PlayButton");
    Play->setIcon(MakeStudioIcon(StudioIcon::Play, "--success"));
    Play->setCheckable(true);
    auto* FromLine = MakeButton(tr("С реплики"), StudioIcon::Play, tr("Запуск с выбранного блока пока недоступен"));
    FromLine->setEnabled(false);
    Pause = MakeButton({}, StudioIcon::Pause, tr("Пауза / продолжить"));
    Pause->setCheckable(true);
    Pause->setEnabled(false);
    Step = MakeButton({}, StudioIcon::Step, tr("Один кадр симуляции на паузе"));
    Step->setEnabled(false);
    Mode = new QLabel(this);
    Mode->setObjectName("EditorMode");
    Layout->addWidget(Mode);
    Layout->addSpacing(24);
    Home = MakeButton(tr("Главное меню"), StudioIcon::Folder, tr("Открыть окно проектов"));
}
}
