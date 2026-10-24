#include "UI/Presentation/LocalizedEditorUi.h"
#include "UI/Adapters/WorkspaceDockController.h"
#include "UI/Styles/EditorStyle.h"
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QScreen>
#include <QSettings>
#include <QRubberBand>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <algorithm>

namespace EditorUi
{
namespace
{
constexpr auto MimeType = "application/x-sacura-workspace-page";
constexpr auto SettingsKey = "workspace/layout-v1";
}

WorkspaceDockController::WorkspaceDockController(QWidget& InOwner, const QString& SettingsFile)
    : QObject(&InOwner), Owner(InOwner), DragToken(QUuid::createUuid().toString())
{
    setObjectName("WorkspaceDockController");
    Settings = SettingsFile.isEmpty()
        ? std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope, "Sacura", "Editor")
        : std::make_unique<QSettings>(SettingsFile, QSettings::IniFormat);
    SaveTimer = new QTimer(this);
    SaveTimer->setSingleShot(true);
    SaveTimer->setInterval(250);
    connect(SaveTimer, &QTimer::timeout, this, &WorkspaceDockController::Save);
}

WorkspaceDockController::~WorkspaceDockController() = default;

void WorkspaceDockController::AddArea(const QString& Id, const QString& Title, QTabWidget& Tabs, QWidget& Region)
{
    Areas.insert(Id, {Title, &Tabs, &Region});
    Tabs.setProperty("workspaceArea", Id);
    Tabs.tabBar()->setProperty("workspaceArea", Id);
    Tabs.setAcceptDrops(true);
    Tabs.tabBar()->setAcceptDrops(true);
    Tabs.installEventFilter(this);
    Tabs.tabBar()->installEventFilter(this);
    Region.installEventFilter(this);
    connect(&Tabs, &QTabWidget::currentChanged, this, [this]() { ScheduleSave(); });
    connect(&Tabs, &QTabWidget::tabCloseRequested, this, [this, &Tabs](int Index)
    {
        if (auto* Page = Tabs.widget(Index); !PageId(Page).isEmpty()) { SetOpen(*Page, false); }
    });
}

void WorkspaceDockController::AddPage(const QString& Id, QWidget& Page, const QString& Area, QAction& Visibility,
                                      bool bPreviouslyFixed)
{
    auto* Tabs = Areas.value(Area).Tabs.data();
    if (!Tabs || Tabs->indexOf(&Page) < 0) { return; }
    const int Index = Tabs->indexOf(&Page);
    PageState State;
    State.Page = &Page;
    State.Visibility = &Visibility;
    State.Title = Tabs->tabText(Index);
    State.Area = State.DefaultArea = Area;
    State.DefaultIndex = Index;
    State.Open = State.DefaultOpen = Tabs->isTabVisible(Index);
    State.bPreviouslyFixed = bPreviouslyFixed;
    Pages.insert(Id, State);
    Tabs->setTabToolTip(Index, tr("Перетащите вкладку в другую область или за её пределы. Двойной щелчок — отдельное окно."));
    connect(&Visibility, &QAction::toggled, this, [this, &Page](bool Open) { SetOpen(Page, Open); });
}

void WorkspaceDockController::SetSplitters(QSplitter& InWorkspace, QSplitter& InCenter)
{
    Workspace = &InWorkspace; Center = &InCenter;
    for (auto* Splitter : {&InWorkspace, &InCenter})
    { connect(Splitter, &QSplitter::splitterMoved, this, [this]() { ScheduleSave(); }); }
}

void WorkspaceDockController::SetWorkspacePosition(const QString& NewPosition)
{
    if (!Center || !Areas.contains("scene") || !Areas.contains("workspace") ||
        (NewPosition != "bottom" && NewPosition != "left" && NewPosition != "right")) { return; }
    if (Position == NewPosition) { return; }
    auto* Scene = Areas["scene"].Region.data();
    auto* Tools = Areas["workspace"].Region.data();
    Position = NewPosition;
    Center->setOrientation(Position == "bottom" ? Qt::Vertical : Qt::Horizontal);
    Center->insertWidget(0, Position == "left" ? Tools : Scene);
    const int Extent = Position == "bottom" ? Center->height() : Center->width();
    const int ToolsExtent = Position == "bottom" ? std::clamp(Extent * 2 / 5, 180, 480)
                                                 : std::clamp(Extent * 2 / 5, 260, 520);
    Center->setSizes(Position == "left" ? QList<int>{ToolsExtent, std::max(200, Extent - ToolsExtent)}
                                         : QList<int>{std::max(200, Extent - ToolsExtent), ToolsExtent});
    ScheduleSave();
}

QString WorkspaceDockController::PageId(QWidget* Page) const
{
    if (!Page) { return {}; }
    for (auto It = Pages.cbegin(); It != Pages.cend(); ++It)
    { if (It->Page == Page) { return It.key(); } }
    return {};
}

QTabWidget* WorkspaceDockController::Host(QWidget* Page) const
{
    for (const auto& Area : Areas)
    { if (Area.Tabs && Area.Tabs->indexOf(Page) >= 0) { return Area.Tabs; } }
    return nullptr;
}

void WorkspaceDockController::Open(QWidget& Page) { SetOpen(Page, true); }

void WorkspaceDockController::UpdateArea(QTabWidget& Tabs)
{
    bool HasBase = false, HasVisible = false;
    Tabs.setTabsClosable(true);
    for (int I = 0; I < Tabs.count(); ++I)
    {
        HasVisible = HasVisible || Tabs.isTabVisible(I);
        if (PageId(Tabs.widget(I)).isEmpty())
        {
            HasBase = true;
            Tabs.tabBar()->setTabButton(I, QTabBar::LeftSide, nullptr);
            Tabs.tabBar()->setTabButton(I, QTabBar::RightSide, nullptr);
        }
    }
    if (!HasBase && !HasVisible) { Areas[Tabs.property("workspaceArea").toString()].Region->hide(); }
}

void WorkspaceDockController::SetOpen(QWidget& Page, bool Open)
{
    const QString Id = PageId(&Page);
    if (Id.isEmpty()) { return; }
    auto& State = Pages[Id];
    State.Open = Open;
    if (State.Visibility)
    {
        const QSignalBlocker Block(State.Visibility);
        State.Visibility->setChecked(Open);
    }
    if (State.Floating)
    {
        State.Floating->setVisible(Open);
        if (Open) { State.Floating->raise(); State.Floating->activateWindow(); }
    }
    else if (auto* Tabs = Host(&Page))
    {
        Tabs->setTabVisible(Tabs->indexOf(&Page), Open);
        UpdateArea(*Tabs);
        if (Open)
        {
            Areas[State.Area].Region->show();
            Tabs->setCurrentWidget(&Page);
            // A collapsed splitter must reveal a newly opened destination.
            for (QWidget* Child = Areas[State.Area].Region; Child; Child = Child->parentWidget())
            {
                if (auto* Splitter = qobject_cast<QSplitter*>(Child->parentWidget()))
                {
                    auto Sizes = Splitter->sizes();
                    const int Index = Splitter->indexOf(Child);
                    if (Index >= 0 && Sizes.value(Index) == 0)
                    { Sizes[Index] = 300; Splitter->setSizes(Sizes); }
                }
            }
        }
    }
    ScheduleSave();
}

void WorkspaceDockController::Detach(PageState& State)
{
    if (auto* Tabs = Host(State.Page)) { Tabs->removeTab(Tabs->indexOf(State.Page)); UpdateArea(*Tabs); }
    State.Page->hide();
    State.Page->setParent(&Owner);
    if (State.Floating)
    {
        State.Floating->removeEventFilter(this);
        State.Floating->hide();
        State.Floating->deleteLater();
        State.Floating = nullptr;
    }
}

void WorkspaceDockController::Dock(QWidget& Page, const QString& Area, int Index)
{
    const QString Id = PageId(&Page);
    if (Id.isEmpty() || !Areas.contains(Area)) { return; }
    auto& State = Pages[Id];
    auto* Tabs = Areas[Area].Tabs.data();
    if (Host(&Page) == Tabs && Index > Tabs->indexOf(&Page)) { --Index; }
    Detach(State);
    State.Area = Area;
    const int Inserted = Tabs->insertTab(Index < 0 ? Tabs->count() : Index, &Page, State.Title);
    Tabs->setTabToolTip(Inserted, tr("Перетащите вкладку или откройте меню правой кнопкой мыши"));
    Open(Page);
}

void WorkspaceDockController::RestoreDefault(QWidget& Page)
{
    const QString Id = PageId(&Page);
    if (Id.isEmpty()) { return; }
    const PageState State = Pages[Id];
    Dock(Page, State.DefaultArea);
    if (auto* Tabs = Host(&Page))
    {
        Tabs->tabBar()->moveTab(Tabs->indexOf(&Page), std::min(State.DefaultIndex, Tabs->count() - 1));
    }
}

QRect WorkspaceDockController::VisibleGeometry(QRect Geometry) const
{
    if (!Geometry.isValid()) { Geometry = QRect(Owner.mapToGlobal(QPoint(80, 80)), QSize(800, 520)); }
    QScreen* Screen = QGuiApplication::screenAt(Geometry.center());
    if (!Screen) { Screen = Owner.screen(); }
    const QRect Available = Screen->availableGeometry().adjusted(12, 32, -12, -12);
    Geometry.setSize(Geometry.size().boundedTo(Available.size()).expandedTo(QSize(240, 160).boundedTo(Available.size())));
    Geometry.moveLeft(std::clamp(Geometry.x(), Available.left(), Available.right() - Geometry.width() + 1));
    Geometry.moveTop(std::clamp(Geometry.y(), Available.top(), Available.bottom() - Geometry.height() + 1));
    return Geometry;
}

void WorkspaceDockController::Float(QWidget& Page, QRect Geometry)
{
    const QString Id = PageId(&Page);
    if (Id.isEmpty()) { return; }
    auto& State = Pages[Id];
    if (State.Floating) { Open(Page); return; }
    Detach(State);
    auto* Window = new QWidget(&Owner, Qt::Tool);
    State.Floating = Window;
    Window->setObjectName("WorkspaceFloating_" + Id);
    Window->setProperty("uiRole", "workspaceFloating");
    Window->setWindowTitle(State.Title);
    Window->setProperty("workspacePage", Id);
    Window->setMinimumSize(240, 160);
    Window->setStyleSheet(EditorStyle::LoadStylesheet());
    auto* Layout = new QVBoxLayout(Window);
    Layout->setContentsMargins(4, 4, 4, 4);
    auto* Attach = new QToolButton(Window);
    Attach->setText(tr("Закрепить…"));
    BindEditorProperty(Attach, "toolTip", QString::fromUtf8("Вернуть окно в одну из областей редактора"));
    connect(Attach, &QToolButton::clicked, this, [this, Id, Attach]()
    { ShowMenu(Id, Attach->mapToGlobal(QPoint(0, Attach->height()))); });
    Layout->addWidget(Attach, 0, Qt::AlignRight);
    Layout->addWidget(&Page, 1);
    Page.show();
    Window->setGeometry(VisibleGeometry(Geometry));
    Window->installEventFilter(this);
    Open(Page);
}

void WorkspaceDockController::ShowMenu(const QString& Id, QPoint Position)
{
    if (!Pages.contains(Id)) { return; }
    auto* Menu = new QMenu(&Owner);
    Menu->setAttribute(Qt::WA_DeleteOnClose);
    QPointer<QWidget> Page = Pages[Id].Page;
    auto* Restore = BindEditorCaption(Menu->addAction(tr("Вернуть на исходное место"), this, [this, Page]()
    {
        if (Page) { RestoreDefault(*Page); }
    }), QString::fromUtf8("Вернуть на исходное место"));
    Restore->setObjectName("RestoreWorkspacePage");
    Menu->addSeparator();
    if (!Pages[Id].Floating)
    { BindEditorCaption(Menu->addAction(tr("Отдельное окно"), this, [this, Page]() { if (Page) { Float(*Page); } }), QString::fromUtf8("Отдельное окно")); }
    for (auto It = Areas.cbegin(); It != Areas.cend(); ++It)
    {
        BindEditorCaption(Menu->addAction(tr("Закрепить: %1").arg(It->Title), this,
            [this, Page, Area = It.key()]() { if (Page) { Dock(*Page, Area); } }), QString::fromUtf8("Закрепить: %1"));
    }
    Menu->addSeparator();
    BindEditorCaption(Menu->addAction(tr("Закрыть"), this, [this, Page]() { if (Page) { SetOpen(*Page, false); } }), QString::fromUtf8("Закрыть"));
    Menu->popup(Position);
}

void WorkspaceDockController::ScheduleSave()
{
    if (!bRestoring) { SaveTimer->start(); }
}

void WorkspaceDockController::Save()
{
    if (bRestoring) { return; }
    SaveTimer->stop();
    QJsonObject Root, Windows, Regions;
    for (auto It = Pages.cbegin(); It != Pages.cend(); ++It)
    {
        QJsonObject Value{{"area", It->Area}, {"open", It->Open}, {"floating", bool(It->Floating)}};
        if (It->Floating)
        {
            const QRect R = It->Floating->geometry();
            Value["rect"] = QJsonArray{R.x(), R.y(), R.width(), R.height()};
        }
        Windows[It.key()] = Value;
    }
    for (auto It = Areas.cbegin(); It != Areas.cend(); ++It)
    {
        QJsonArray Order;
        for (int I = 0; I < It->Tabs->count(); ++I) { Order.append(PageId(It->Tabs->widget(I))); }
        Regions[It.key()] = QJsonObject{{"order", Order}, {"active", PageId(It->Tabs->currentWidget())},
                                       {"visible", !It->Region->isHidden()}};
    }
    Root["version"] = 1; Root["pages"] = Windows; Root["areas"] = Regions;
    Root["position"] = Position;
    if (Workspace) { Root["workspace"] = QString::fromLatin1(Workspace->saveState().toBase64()); }
    if (Center) { Root["center"] = QString::fromLatin1(Center->saveState().toBase64()); }
    Settings->setValue(SettingsKey, QJsonDocument(Root).toJson(QJsonDocument::Compact));
    Settings->sync();
}

bool WorkspaceDockController::Restore()
{
    bRestoring = true;
    const QJsonObject Root = QJsonDocument::fromJson(Settings->value(SettingsKey).toByteArray()).object();
    if (Root["version"].toInt() != 1 || !Root["pages"].isObject() || !Root["areas"].isObject())
    { bRestoring = false; return false; }
    const auto Windows = Root["pages"].toObject();
    const auto Regions = Root["areas"].toObject();
    SetWorkspacePosition(Root["position"].toString("bottom"));
    // Older layouts represented fixed base tabs with an empty ID and stored
    // their visibility on the area. Resolve those entries without resetting
    // the user's other tool placements or activating a neighbouring tab.
    auto ResolveSavedId = [&](const QString& Area, const QString& Id)
    {
        if (!Id.isEmpty()) { return Id; }
        for (auto It = Pages.cbegin(); It != Pages.cend(); ++It)
        {
            if (It->bPreviouslyFixed && It->DefaultArea == Area && !Windows.contains(It.key()))
            { return It.key(); }
        }
        return QString{};
    };
    for (auto It = Pages.begin(); It != Pages.end(); ++It)
    {
        const auto Value = Windows[It.key()].toObject();
        if (Value.isEmpty())
        {
            if (It->bPreviouslyFixed)
            { SetOpen(*It->Page, Regions[It->DefaultArea].toObject()["visible"].toBool(true)); }
            continue;
        }
        const QString Area = Value["area"].toString();
        if (!Areas.contains(Area)) { continue; }
        Dock(*It->Page, Area);
        if (Value["floating"].toBool())
        {
            const auto R = Value["rect"].toArray();
            Float(*It->Page, R.size() == 4 ? QRect(R[0].toInt(), R[1].toInt(), R[2].toInt(), R[3].toInt()) : QRect{});
        }
        SetOpen(*It->Page, Value["open"].toBool(true));
    }
    for (auto It = Areas.begin(); It != Areas.end(); ++It)
    {
        const auto Region = Regions[It.key()].toObject();
        int Position = 0;
        for (const auto& Id : Region["order"].toArray())
        {
            auto* Page = Pages.value(ResolveSavedId(It.key(), Id.toString())).Page.data();
            int Index = Page ? It->Tabs->indexOf(Page) : -1;
            if (Index >= 0) { It->Tabs->tabBar()->moveTab(Index, Position++); }
            else if (Id.toString().isEmpty()) { ++Position; }
        }
        auto* Active = Pages.value(ResolveSavedId(It.key(), Region["active"].toString())).Page.data();
        if (Active && It->Tabs->indexOf(Active) >= 0 && It->Tabs->isTabVisible(It->Tabs->indexOf(Active)))
        { It->Tabs->setCurrentWidget(Active); }
        else if (Region["active"].toString().isEmpty())
        {
            for (int I = 0; I < It->Tabs->count(); ++I)
            { if (PageId(It->Tabs->widget(I)).isEmpty()) { It->Tabs->setCurrentIndex(I); break; } }
        }
        It->Region->setVisible(Region["visible"].toBool(true));
    }
    if (Workspace) { Workspace->restoreState(QByteArray::fromBase64(Root["workspace"].toString().toLatin1())); }
    if (Center) { Center->restoreState(QByteArray::fromBase64(Root["center"].toString().toLatin1())); }
    bRestoring = false;
    return true;
}

void WorkspaceDockController::Reset()
{
    bRestoring = true;
    SetWorkspacePosition("bottom");
    // Sort by the original index; map-key order is unrelated to the authored default tabs.
    auto Values = Pages.values();
    std::sort(Values.begin(), Values.end(), [](const auto& A, const auto& B) { return A.DefaultIndex < B.DefaultIndex; });
    for (const auto& State : Values)
    { Dock(*State.Page, State.DefaultArea, State.DefaultIndex); SetOpen(*State.Page, State.DefaultOpen); }
    for (const auto& Area : Areas) { Area.Region->show(); }
    bRestoring = false;
    ScheduleSave();
}

bool WorkspaceDockController::eventFilter(QObject* Watched, QEvent* Event)
{
    const QString FloatingId = Watched->property("workspacePage").toString();
    if (!FloatingId.isEmpty() && Pages.contains(FloatingId))
    {
        if (Event->type() == QEvent::Close)
        {
            static_cast<QCloseEvent*>(Event)->ignore();
            SetOpen(*Pages[FloatingId].Page, false);
            return true;
        }
        if (Event->type() == QEvent::Move || Event->type() == QEvent::Resize) { ScheduleSave(); }
    }
    if (Event->type() == QEvent::Show || Event->type() == QEvent::Hide)
    {
        ScheduleSave();
    }
    const QString Area = Watched->property("workspaceArea").toString();
    if (Area.isEmpty()) { return QObject::eventFilter(Watched, Event); }
    auto* Tabs = Areas[Area].Tabs.data();
    auto* Bar = qobject_cast<QTabBar*>(Watched);
    if (Bar && Event->type() == QEvent::ContextMenu)
    {
        auto* Context = static_cast<QContextMenuEvent*>(Event);
        const QString Id = PageId(Tabs->widget(Bar->tabAt(Context->pos())));
        if (!Id.isEmpty()) { ShowMenu(Id, Context->globalPos()); return true; }
    }
    if (Bar && Event->type() == QEvent::MouseButtonDblClick)
    {
        auto* Mouse = static_cast<QMouseEvent*>(Event);
        const QString Id = PageId(Tabs->widget(Bar->tabAt(Mouse->position().toPoint())));
        if (Mouse->button() == Qt::LeftButton && !Id.isEmpty()) { Float(*Pages[Id].Page); return true; }
    }
    if (Bar && Event->type() == QEvent::MouseButtonPress)
    {
        auto* Mouse = static_cast<QMouseEvent*>(Event);
        DragId = Mouse->button() == Qt::LeftButton ? PageId(Tabs->widget(Bar->tabAt(Mouse->position().toPoint()))) : QString{};
        DragStart = Mouse->position().toPoint();
    }
    if (Bar && Event->type() == QEvent::MouseMove && !DragId.isEmpty())
    {
        auto* Mouse = static_cast<QMouseEvent*>(Event);
        if (Mouse->buttons().testFlag(Qt::LeftButton) &&
            (Mouse->position().toPoint() - DragStart).manhattanLength() >= QApplication::startDragDistance())
        {
            const QString Id = DragId; DragId.clear();
            auto* Drag = new QDrag(Bar);
            auto* Mime = new QMimeData;
            Mime->setData(MimeType, (DragToken + "/" + Id).toUtf8());
            Drag->setMimeData(Mime);
            Drag->setPixmap(Bar->grab(Bar->tabRect(Tabs->indexOf(Pages[Id].Page))));
            const auto Result = Drag->exec(Qt::MoveAction);
            if (DropIndicator) { DropIndicator->hide(); }
            // Escape cancels with the mouse still held; only an actual outside release floats.
            if (Result == Qt::IgnoreAction && !QApplication::mouseButtons().testFlag(Qt::LeftButton))
            { Float(*Pages[Id].Page, QRect(QCursor::pos(), QSize(800, 520))); }
            Drag->deleteLater();
            return true;
        }
    }
    if (Event->type() == QEvent::MouseButtonRelease) { DragId.clear(); }
    if (Event->type() == QEvent::DragLeave && DropIndicator) { DropIndicator->hide(); }
    if (Event->type() == QEvent::DragEnter || Event->type() == QEvent::DragMove || Event->type() == QEvent::Drop)
    {
        auto* Drop = static_cast<QDropEvent*>(Event);
        const QString Payload = QString::fromUtf8(Drop->mimeData()->data(MimeType));
        const QString Prefix = DragToken + "/";
        const QString Id = Payload.startsWith(Prefix) ? Payload.mid(Prefix.size()) : QString{};
        if (Pages.contains(Id))
        {
            // Empty side/bottom regions are hidden. Their neighbours' edges
            // remain reachable drop targets that reopen the original split.
            QString Destination = Area;
            QRect Edge;
            if (!Bar && (Area == "scene" || Area == "workspace"))
            {
                const int EdgeWidth = std::min(Tabs->width() / 2, std::clamp(Tabs->width() / 4, 48, 96));
                const int EdgeHeight = std::min(Tabs->height(), std::clamp(Tabs->height() / 4, 48, 96));
                const QRect LeftEdge(0, 0, EdgeWidth, Tabs->height());
                const QRect RightEdge(Tabs->width() - EdgeWidth, 0, EdgeWidth, Tabs->height());
                const QRect BottomEdge(0, Tabs->height() - EdgeHeight, Tabs->width(), EdgeHeight);
                const QPoint Position = Drop->position().toPoint();
                if (Areas.contains("hierarchy") && LeftEdge.contains(Position))
                {
                    Destination = "hierarchy";
                    Edge = LeftEdge;
                }
                else if (Areas.contains("inspector") && RightEdge.contains(Position))
                {
                    Destination = "inspector";
                    Edge = RightEdge;
                }
                else if (Area == "scene" && Areas.contains("workspace") && BottomEdge.contains(Position))
                {
                    Destination = "workspace";
                    Edge = BottomEdge;
                }
            }
            if (!Edge.isEmpty())
            {
                if (!DropIndicator)
                {
                    DropIndicator = new QRubberBand(QRubberBand::Rectangle, Tabs);
                    DropIndicator->setObjectName("WorkspaceDropIndicator");
                    DropIndicator->setAttribute(Qt::WA_TransparentForMouseEvents);
                }
                if (DropIndicator->parentWidget() != Tabs) { DropIndicator->setParent(Tabs); }
                DropIndicator->setGeometry(Edge);
                DropIndicator->show();
                DropIndicator->raise();
            }
            else if (DropIndicator) { DropIndicator->hide(); }
            Drop->setDropAction(Qt::MoveAction); Drop->accept();
            if (Event->type() == QEvent::Drop)
            {
                if (DropIndicator) { DropIndicator->hide(); }
                Dock(*Pages[Id].Page, Destination,
                     Bar ? Bar->tabAt(Drop->position().toPoint()) : -1);
            }
            return true;
        }
    }
    return QObject::eventFilter(Watched, Event);
}
}
