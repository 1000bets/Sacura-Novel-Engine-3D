#include "UI/Views/StudioHeaderView.h"
#include "UI/Styles/StudioIcons.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QPainter>
#include <QPushButton>
#include <algorithm>

namespace
{
class BrandMarkView final : public QWidget
{
public:
    explicit BrandMarkView(QWidget* Parent) : QWidget(Parent), Icon(EditorUi::MakeStudioIcon(EditorUi::StudioIcon::Flower, "--brand-mark"))
    {
        setObjectName("EditorBrandMark");
        setFixedSize(18, 23);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter Painter(this);
        Icon.paint(&Painter, rect());
    }
private:
    QIcon Icon;
};

// Keep both groups intact. Height-for-width lets QMainWindow reserve a second
// row when the available logical width (including DPI/font changes) is too small.
class HeaderLayout final : public QLayout
{
public:
    explicit HeaderLayout(QWidget* Parent) : QLayout(Parent)
    {
        setContentsMargins(12, 5, 12, 11);
        setSpacing(8);
    }

    ~HeaderLayout() override
    {
        while (auto* Item = takeAt(0)) { delete Item; }
    }

    void addItem(QLayoutItem* Item) override { Items.push_back(Item); }
    int count() const override { return static_cast<int>(Items.size()); }
    QLayoutItem* itemAt(int Index) const override
    {
        return Index >= 0 && Index < count() ? Items[Index] : nullptr;
    }
    QLayoutItem* takeAt(int Index) override
    {
        auto* Item = itemAt(Index);
        if (Item) { Items.removeAt(Index); }
        return Item;
    }
    bool hasHeightForWidth() const override { return true; }
    Qt::Orientations expandingDirections() const override { return Qt::Horizontal; }
    QSize minimumSize() const override
    {
        QSize Size;
        for (auto* Item : Items) { Size = Size.expandedTo(Item->minimumSize()); }
        return Size + MarginsSize();
    }
    QSize sizeHint() const override
    {
        QSize Size;
        for (auto* Item : Items)
        {
            Size.rwidth() += Item->sizeHint().width();
            Size.setHeight(std::max(Size.height(), Item->sizeHint().height()));
        }
        Size.rwidth() += std::max(0, count() - 1) * spacing();
        return Size + MarginsSize();
    }
    int heightForWidth(int Width) const override
    {
        return Arrange(QRect(0, 0, Width, 0), false);
    }
    void setGeometry(const QRect& Rect) override
    {
        QLayout::setGeometry(Rect);
        Arrange(Rect, true);
    }

private:
    QSize MarginsSize() const
    {
        const auto Margins = contentsMargins();
        return {Margins.left() + Margins.right(), Margins.top() + Margins.bottom()};
    }
    int Arrange(const QRect& Rect, bool Apply) const
    {
        const QRect Area = Rect.marginsRemoved(contentsMargins());
        int X = Area.x();
        int Y = Area.y();
        int RowHeight = 28;
        for (auto* Item : Items) { RowHeight = std::max(RowHeight, Item->sizeHint().height()); }
        for (int Index = 0; Index < count(); ++Index)
        {
            auto* Item = Items[Index];
            const QSize Size = Item->sizeHint().boundedTo(QSize(Area.width(), QWIDGETSIZE_MAX));
            if (Index > 0 && X + Size.width() > Area.x() + Area.width())
            {
                Y += RowHeight + spacing();
                X = Area.x();
            }
            // Align the command group to the right edge on either row.
            if (Index == count() - 1) { X = Area.x() + Area.width() - Size.width(); }
            if (Apply) { Item->setGeometry(QRect(X, Y, Size.width(), RowHeight)); }
            X += Size.width() + spacing();
        }
        return Y - Area.y() + RowHeight + MarginsSize().height();
    }
    QList<QLayoutItem*> Items;
};
}

namespace EditorUi
{
StudioHeaderView::StudioHeaderView(QMenuBar& Menus, QLabel& ProjectTitle, QWidget* Parent) : QWidget(Parent)
{
    setObjectName("StudioHeader");
    auto* Layout = new HeaderLayout(this);
    auto MakeGroup = [&]()
    {
        auto* Group = new QWidget(this);
        auto* Row = new QHBoxLayout(Group);
        Row->setContentsMargins(0, 0, 0, 0);
        Row->setSpacing(4);
        Row->setAlignment(Qt::AlignVCenter);
        Layout->addWidget(Group);
        return Row;
    };
    auto* Navigation = MakeGroup();
    auto* Mark = new BrandMarkView(this);
    Navigation->addWidget(Mark);
    auto* Brand = new QLabel(QStringLiteral("Sacura"), this);
    Brand->setObjectName("EditorBrand");
    Navigation->addWidget(Brand);
    Navigation->addSpacing(16);
    Menus.setNativeMenuBar(false);
    Menus.setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    Navigation->addWidget(&Menus, 0, Qt::AlignVCenter);
    auto* Commands = MakeGroup();
    ProjectTitle.setObjectName("ProjectTitle");
    ProjectTitle.setMaximumWidth(220);
    // Override QLabel's unbreakable-text minimum without discarding its size hint.
    ProjectTitle.setMinimumWidth(1);
    Commands->addWidget(&ProjectTitle);
    Commands->addSpacing(20);
    auto MakeButton = [&](const QString& Text, StudioIcon Icon, const QString& Hint)
    {
        auto* Button = new QPushButton(MakeStudioIcon(Icon), Text, this);
        Button->setProperty("uiRole", "chromeButton");
        Button->setToolTip(Hint);
        Button->setIconSize(QSize(14, 14));
        Commands->addWidget(Button);
        return Button;
    };
    Save = new QPushButton(MakeFrontendIcon("Save"), {}, this);
    Save->setObjectName("SaveDocument");
    Save->setProperty("uiRole", "chromeButton");
    Save->setFixedSize(32, 32);
    Save->setIconSize(QSize(20, 20));
    Save->setToolTip(tr("Сохранить (Ctrl+S)"));
    Save->setAccessibleName(tr("Сохранить"));
    Commands->addWidget(Save);
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
    Commands->addWidget(Mode);
    Commands->addSpacing(24);
    Home = MakeButton(tr("Главное меню"), StudioIcon::Folder, tr("Открыть окно проектов"));
}
}
