#include "UI/Views/StoryWorkspaceView.h"
#include "UI/Views/StoryGraphicsScene.h"
#include "UI/Views/StudioActionMenu.h"
#include "UI/Adapters/StoryMenuBuilder.h"
#include <QKeyEvent>
#include <QScrollBar>
#include <QScrollArea>
#include <QMouseEvent>
#include <QShowEvent>
#include "UI/Styles/StoryGraphStyle.h"
#include "UI/Styles/StudioIcons.h"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QQueue>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace EditorUi
{
namespace
{
class StoryWireLegend final : public QWidget
{
  public:
    explicit StoryWireLegend(QWidget* Parent) : QWidget(Parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFixedSize(240, 28);
    }
  protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter Painter(this);
        Painter.setRenderHint(QPainter::Antialiasing);
        Painter.setFont(StoryGraphStyle::Font(13));
        StoryGraphStyle::PaintConnection(Painter, StoryGraphStyle::ConnectionPath({0, 14}, {36, 14}), false);
        Painter.setPen(EditorStyle::Color("--text-muted"));
        Painter.drawText(QRectF(45, 0, 76, 28), Qt::AlignVCenter, tr("Переход"));
        QPen Values(EditorStyle::Color("--info"), 2, Qt::DotLine, Qt::RoundCap);
        Painter.setPen(Values);
        Painter.drawLine(QPointF(120, 14), QPointF(156, 14));
        Painter.setPen(EditorStyle::Color("--text-muted"));
        Painter.drawText(QRectF(165, 0, 75, 28), Qt::AlignVCenter, tr("Значение"));
    }
};

class StoryCanvas final : public QGraphicsView
{
  public:
    explicit StoryCanvas(QGraphicsScene* Scene, StoryWorkspaceView* Parent)
        : QGraphicsView(Scene, Parent), Workspace(*Parent)
    {
        setObjectName("StoryCanvas");
        setFrameShape(QFrame::NoFrame);
        setRenderHint(QPainter::Antialiasing);
        setRenderHint(QPainter::TextAntialiasing);
        setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
        setDragMode(QGraphicsView::ScrollHandDrag);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        Legend = new StoryWireLegend(viewport());
        Navigation = new QWidget(viewport());
        Navigation->setObjectName("GraphNavigation");
        auto* Controls = new QVBoxLayout(Navigation);
        Controls->setContentsMargins(0, 0, 0, 0);
        Controls->setSpacing(1);
        auto* ZoomIn = new QPushButton(QStringLiteral("+"), Navigation);
        ZoomIn->setToolTip(tr("Приблизить"));
        auto* ZoomOut = new QPushButton(QString::fromUtf8("−"), Navigation);
        ZoomOut->setToolTip(tr("Отдалить"));
        auto* Fit = new QPushButton(MakeStudioIcon(StudioIcon::Fit), {}, Navigation);
        Fit->setToolTip(tr("Вся история"));
        for (auto* Button : {ZoomIn, ZoomOut, Fit})
        {
            Button->setFixedSize(32, 32);
            Controls->addWidget(Button);
        }
        connect(ZoomIn, &QPushButton::clicked, this, [this]() { Zoom(1.15); });
        connect(ZoomOut, &QPushButton::clicked, this, [this]() { Zoom(1.0 / 1.15); });
        connect(Fit, &QPushButton::clicked, this, [this]() { FitGraph(); });
        Navigation->adjustSize();
        FocusNavigation = new QWidget(viewport());
        FocusNavigation->setObjectName("GraphFocusNavigation");
        auto* FocusControls = new QHBoxLayout(FocusNavigation);
        FocusControls->setContentsMargins(4, 4, 4, 4);
        FocusControls->setSpacing(4);
        auto* Selected = new QPushButton(MakeStudioIcon(StudioIcon::Fit), {}, FocusNavigation);
        Selected->setToolTip(tr("К реплике"));
        auto* Overview = new QPushButton(MakeStudioIcon(StudioIcon::Expand), {}, FocusNavigation);
        Overview->setToolTip(tr("Вся история"));
        for (auto* Button : {Selected, Overview}) { Button->setFixedSize(32, 32); FocusControls->addWidget(Button); }
        connect(Selected, &QPushButton::clicked, this, [this]() { FocusSelection(); });
        connect(Overview, &QPushButton::clicked, this, [this]() { FitGraph(); });
        FocusNavigation->adjustSize();
    }

    void FitGraph()
    {
        bKeepOriginOnResize = false;
        fitInView(scene()->itemsBoundingRect().adjusted(-30, -30, 30, 30), Qt::KeepAspectRatio);
        const qreal Scale = std::clamp(transform().m11(), .25, 1.0);
        setTransform(QTransform::fromScale(Scale, Scale));
        centerOn(scene()->itemsBoundingRect().center());
    }
    void FocusSelection()
    {
        for (auto* Item : scene()->selectedItems())
        {
            if (!Item->data(0).toString().isEmpty()) { ResetNavigation(Item->pos()); return; }
        }
    }
    void ResetNavigation(QPointF NodePosition)
    {
        bKeepOriginOnResize = true;
        InitialNodePosition = NodePosition;
        resetTransform();
        RestoreInitialView();
    }
    void EnsureVisibleExtent(QPointF Center)
    {
        const QSizeF Size(viewport()->width() / transform().m11(), viewport()->height() / transform().m11());
        const QRectF Visible(Center - QPointF(Size.width() / 2, Size.height() / 2), Size);
        scene()->setSceneRect(scene()->sceneRect().united(Visible.adjusted(-2, -2, 2, 2)));
    }

  protected:
    bool event(QEvent* Event) override
    {
        if (Event->type() == QEvent::ShortcutOverride)
        {
            auto* Key = static_cast<QKeyEvent*>(Event);
            if (Key->key() == Qt::Key_Delete || Key->matches(QKeySequence::Undo) || Key->matches(QKeySequence::Redo) ||
                Key->matches(QKeySequence::Save))
            {
                Key->accept();
                return true;
            }
        }
        return QGraphicsView::event(Event);
    }
    void keyPressEvent(QKeyEvent* Event) override
    {
        if (Event->key() == Qt::Key_Escape)
        {
            Workspace.CancelInteraction();
        }
        else if (Event->key() == Qt::Key_Delete)
        {
            Workspace.DeleteSelection();
        }
        else if (Event->matches(QKeySequence::Undo) && Workspace.UndoRequested)
        {
            Workspace.CancelInteraction();
            Workspace.UndoRequested();
        }
        else if (Event->matches(QKeySequence::Redo) && Workspace.RedoRequested)
        {
            Workspace.CancelInteraction();
            Workspace.RedoRequested();
        }
        else if (Event->matches(QKeySequence::Save) && Workspace.SaveRequested)
        {
            Workspace.SaveRequested();
        }
        else
        {
            QGraphicsView::keyPressEvent(Event);
            return;
        }
        Event->accept();
    }
    void mousePressEvent(QMouseEvent* Event) override
    {
        bKeepOriginOnResize = false;
        QGraphicsView::mousePressEvent(Event);
    }
    void showEvent(QShowEvent* Event) override
    {
        QGraphicsView::showEvent(Event);
        if (bKeepOriginOnResize)
        {
            RestoreInitialView();
        }
    }
    void resizeEvent(QResizeEvent* Event) override
    {
        QGraphicsView::resizeEvent(Event);
        if (bKeepOriginOnResize)
        {
            RestoreInitialView();
        }
        PositionOverlays();
    }
    void scrollContentsBy(int Dx, int Dy) override
    {
        QGraphicsView::scrollContentsBy(Dx, Dy);
        // QWidget::scroll also moves child widgets. Keep chrome in viewport space.
        PositionOverlays();
    }
    void PositionOverlays()
    {
        if (Navigation != nullptr)
        {
            Navigation->move(8, std::max(8, viewport()->height() - Navigation->height() - 8));
        }
        if (FocusNavigation)
        {
            FocusNavigation->move(69, std::max(8, viewport()->height() - FocusNavigation->height() - 14));
        }
        if (Legend) { Legend->move(std::max(0, viewport()->width() - Legend->width() - 12), 10); }
    }
    void drawBackground(QPainter* Painter, const QRectF& Rect) override
    {
        StoryGraphStyle::PaintGrid(*Painter, Rect);
    }
    void wheelEvent(QWheelEvent* Event) override
    {
        bKeepOriginOnResize = false;
        if (Event->modifiers().testFlag(Qt::ControlModifier))
        {
            Zoom(Event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15);
        }
        else
        {
            QPoint Delta = Event->pixelDelta().isNull() ? Event->angleDelta() / 2 : Event->pixelDelta();
            if (Event->modifiers().testFlag(Qt::ShiftModifier))
            {
                Delta = QPoint(Delta.y(), Delta.x());
            }
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - Delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - Delta.y());
        }
        Event->accept();
    }

  private:
    void RestoreInitialView()
    {
        // Keep the complete header inside the canvas, including after dock resize.
        // Extend the
        // scrollable scene before setting offsets: tall/wide viewports otherwise
        // clamp a requested origin and center the cards in empty space.
        const qreal Scale = transform().m11();
        const QSizeF Size(viewport()->width() / Scale, viewport()->height() / Scale);
        const QPointF TopLeft = InitialNodePosition - QPointF(40, 30) / Scale;
        scene()->setSceneRect(scene()->sceneRect().united(QRectF(TopLeft, Size).adjusted(-2, -2, 2, 2)));
        horizontalScrollBar()->setValue(qRound(TopLeft.x() * Scale));
        verticalScrollBar()->setValue(qRound(TopLeft.y() * Scale));
    }
    void Zoom(qreal Factor)
    {
        bKeepOriginOnResize = false;
        const qreal CurrentScale = transform().m11();
        if (Factor > 1 && CurrentScale < 1.8)
        {
            const qreal LimitedFactor = std::min(Factor, 1.8 / CurrentScale);
            scale(LimitedFactor, LimitedFactor);
        }
        else if (Factor < 1 && CurrentScale > 0.25)
        {
            const qreal LimitedFactor = std::max(Factor, 0.25 / CurrentScale);
            scale(LimitedFactor, LimitedFactor);
        }
    }
    bool bKeepOriginOnResize = true;
    QPointF InitialNodePosition;
    QWidget* Navigation = nullptr;
    QWidget* FocusNavigation = nullptr;
    QWidget* Legend = nullptr;
    StoryWorkspaceView& Workspace;
};
} // namespace

StoryWorkspaceView::StoryWorkspaceView(QWidget* Parent) : QWidget(Parent)
{
    setObjectName("StoryWorkspace");
    auto* Layout = new QVBoxLayout(this);
    Layout->setContentsMargins(0, 0, 0, 0);
    Layout->setSpacing(0);
    auto* Toolbar = new QWidget(this);
    Toolbar->setObjectName("GraphToolbar");
    auto* Tools = new QHBoxLayout(Toolbar);
    Tools->setSizeConstraint(QLayout::SetMinimumSize);
    Tools->setContentsMargins(12, 0, 12, 0);
    Summary = new QLabel(Toolbar);
    Summary->setObjectName("MutedLabel");
    Summary->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    Summary->setMaximumWidth(265);
    Tools->addWidget(Summary, 1);
    auto* Focus = new QPushButton(MakeStudioIcon(StudioIcon::Fit), tr("К реплике"), Toolbar);
    Focus->setProperty("uiRole", "chromeButton");
    Tools->addWidget(Focus);
    auto* Fit = new QPushButton(MakeStudioIcon(StudioIcon::Fit), tr("Вся история"), Toolbar);
    Fit->setProperty("uiRole", "chromeButton");
    Tools->addWidget(Fit);
    Tools->addStretch(2);
    auto MakeTool = [&](const QString& Text, const QString& Name)
    {
        auto* Button = new QPushButton(Text, Toolbar);
        Button->setObjectName(Name);
        Button->setProperty("uiRole", "chromeButton");
        Tools->addWidget(Button);
        return Button;
    };
    UndoButton = MakeTool({}, "StoryUndo");
    UndoButton->setIcon(MakeFrontendIcon("Undo2"));
    UndoButton->setToolTip(tr("Отменить (Ctrl+Z)"));
    RedoButton = MakeTool({}, "StoryRedo");
    RedoButton->setIcon(MakeFrontendIcon("Redo2"));
    RedoButton->setToolTip(tr("Повторить (Ctrl+Shift+Z)"));
    OpenButton = MakeTool(tr("Открыть"), "StoryOpen");
    SaveButton = MakeTool(tr("Сохранить"), "StorySave");
    SaveButton->setToolTip(tr("Сохранить историю (Ctrl+S)"));
    AddButton = MakeTool(tr("+ Нода"), "StoryAddNode");
    connect(AddButton, &QPushButton::clicked, this, [this]()
    {
        ShowAddMenu(Canvas->mapToScene(Canvas->viewport()->rect().center()) -
                        QPointF(StoryGraphStyle::NodeWidth / 2, StoryGraphStyle::NodeHeight / 2),
                    AddButton->mapToGlobal(QPoint(0, AddButton->height())));
    });
    connect(UndoButton, &QPushButton::clicked, this,
            [this]()
            {
                if (UndoRequested)
                {
                    UndoRequested();
                }
            });
    connect(RedoButton, &QPushButton::clicked, this,
            [this]()
            {
                if (RedoRequested)
                {
                    RedoRequested();
                }
            });
    connect(SaveButton, &QPushButton::clicked, this,
            [this]()
            {
                if (SaveRequested)
                {
                    SaveRequested();
                }
            });
    connect(OpenButton, &QPushButton::clicked, this,
            [this]()
            {
                if (OpenRequested)
                {
                    OpenRequested();
                }
            });
    ToolbarScroll = new QScrollArea(this);
    ToolbarScroll->setObjectName("StoryToolbarScroll");
    ToolbarScroll->setFrameShape(QFrame::NoFrame);
    ToolbarScroll->setWidgetResizable(true);
    ToolbarScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ToolbarScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ToolbarScroll->setWidget(Toolbar);
    ToolbarScroll->setFixedHeight(43);
    connect(ToolbarScroll->horizontalScrollBar(), &QScrollBar::rangeChanged, this, [this](int Minimum, int Maximum)
    { ToolbarScroll->setFixedHeight(43 + (Maximum > Minimum ? ToolbarScroll->horizontalScrollBar()->sizeHint().height() : 0)); });
    Layout->addWidget(ToolbarScroll);
    auto* Body = new QHBoxLayout();
    Body->setContentsMargins(0, 0, 0, 0);
    Body->setSpacing(0);
    auto* Variables = new QWidget(this);
    VariablesPanel = Variables;
    Variables->setObjectName("BlueprintSidebar");
    Variables->setFixedWidth(302);
    auto* Sidebar = new QVBoxLayout(Variables);
    Sidebar->setContentsMargins(13, 8, 15, 12);
    Sidebar->setSpacing(16);
    auto* Heading = new QLabel(tr("Мой Blueprint"), Variables);
    Heading->setObjectName("BlueprintHeading");
    Sidebar->addWidget(Heading);
    auto* VariablesLabel = new QLabel(tr("Переменные"), Variables);
    VariablesLabel->setObjectName("BlueprintVariablesTitle");
    Sidebar->addWidget(VariablesLabel);
    auto* Search = new QLineEdit(Variables);
    Search->setPlaceholderText(tr("Поиск переменных…"));
    Search->setReadOnly(true);
    Search->setFixedHeight(42);
    Sidebar->addWidget(Search);
    auto* Note = new QLabel(tr("В этой истории пока нет переменных."), Variables);
    Note->setWordWrap(true);
    Note->setObjectName("MutedLabel");
    Sidebar->addWidget(Note);
    Sidebar->addStretch();
    auto* Hint = new QLabel(tr("Колесо — прокрутка\nCtrl+колесо — масштаб"), Variables);
    Hint->setObjectName("MutedLabel");
    Hint->setProperty("uiRole", "graphHint");
    Sidebar->addWidget(Hint);
    Body->addWidget(Variables);
    Graphics = new StoryGraphicsScene(this);
    Graphics->InteractionFinished = [this]()
    {
        // Let the controller publish the completed gesture before using a deferred model.
        QTimer::singleShot(0, this, [this]()
        {
            if (PendingModel && !Graphics->IsInteracting())
            {
                auto Latest = std::move(*PendingModel);
                PendingModel.reset();
                SetModel(std::move(Latest));
            }
        });
    };
    Graphics->AddEventRequested = [this](QString Node, int Phase, QString Batch, QString Source)
    { if (AddEventRequested) { AddEventRequested(Node, Phase, Batch, Source); } };
    Graphics->EventSelected = [this](QString Node, QString Binding) { if (EventSelected) { EventSelected(Node, Binding); } };
    Graphics->RemoveEventRequested = [this](QString Node, QString Binding) { if (RemoveEventRequested) { RemoveEventRequested(Node, Binding); } };
    Graphics->EventModeRequested = [this](QString Node, QString Batch, bool Parallel) { if (EventModeRequested) { EventModeRequested(Node, Batch, Parallel); } };
    Graphics->ImportEventRequested = [this](QString Node, int Phase) { if (ImportEventRequested) { ImportEventRequested(Node, Phase); } };
    Graphics->MoveEventRequested = [this](QString Node, QString Binding, int Phase, QString Batch, QString Before) { if (MoveEventRequested) { MoveEventRequested(Node, Binding, Phase, Batch, Before); } };
    Graphics->AddMenuRequested = [this](QPointF Position, QPoint ScreenPosition)
    { ShowAddMenu(Position, ScreenPosition); };
    Graphics->DeleteRequested = [this](QStringList Nodes)
    {
        if (DeleteNodesRequested)
        {
            DeleteNodesRequested(std::move(Nodes));
        }
    };
    Graphics->ConnectRequested = [this](QString Source, int Port, QString Target)
    {
        if (ConnectRequested)
        {
            ConnectRequested(Source, Port, Target);
        }
    };
    Graphics->MoveRequested = [this](QMap<QString, QPointF> Positions)
    {
        if (MoveNodesRequested)
        {
            MoveNodesRequested(std::move(Positions));
        }
    };
    Graphics->StartRequested = [this](QString Identity)
    {
        if (StartNodeRequested)
        {
            StartNodeRequested(Identity);
        }
    };
    Canvas = new StoryCanvas(Graphics, this);
    connect(Focus, &QPushButton::clicked, this, [this]() { static_cast<StoryCanvas*>(Canvas)->FocusSelection(); });
    Body->addWidget(Canvas, 1);
    Layout->addLayout(Body, 1);
    connect(Fit, &QPushButton::clicked, this,
            [this]()
            {
                if (!Model.Nodes.isEmpty())
                {
                    static_cast<StoryCanvas*>(Canvas)->FitGraph();
                }
            });
    connect(Graphics, &QGraphicsScene::selectionChanged, this,
            [this]()
            {
                const auto Selected = Graphics->SelectedNodes();
                if (Selected.isEmpty() || !NodeSelected)
                {
                    return;
                }
                const QString Identity = Selected.front();
                for (const auto& Node : Model.Nodes)
                {
                    if (Node.Identity == Identity)
                    {
                        NodeSelected(Node);
                        return;
                    }
                }
            });
}

void StoryWorkspaceView::resizeEvent(QResizeEvent* Event)
{
    QWidget::resizeEvent(Event);
    // A tool can now live in a side rail or a small floating window. Keep its
    // canvas usable; the native variables placeholder returns when space allows.
    VariablesPanel->setVisible(width() >= 720);
}

void StoryWorkspaceView::ShowAddMenu(QPointF Position, QPoint ScreenPosition)
{
    if (Model.bLocked || (Model.bOpen && Model.bReadOnly)) { return; }
    if (ActionMenu) { ActionMenu->close(); }
    QString SelectedLine;
    const auto Selected = Graphics->SelectedNodes();
    if (Selected.size() == 1)
    {
        for (const auto& Node : Model.Nodes)
        { if (Node.Identity == Selected.front() && Node.KindId == 0) { SelectedLine = Node.Identity; break; } }
    }
    auto Items = BuildStoryNodeMenu(
        [this, Position](int Kind) { if (AddNodeRequested) { AddNodeRequested(Kind, Position); } },
        [this, SelectedLine, ScreenPosition]() { Graphics->ShowEventMenu(SelectedLine, 1, {}, ScreenPosition); },
        !SelectedLine.isEmpty());
    ActionMenu = new StudioActionMenu(tr("Добавить ноду"), std::move(Items), this);
    ActionMenu->Open(ScreenPosition);
}

StoryWorkspaceView::~StoryWorkspaceView()
{
    // The scene emits selectionChanged while deleting its items. Disconnect
    // while Model and the derived workspace are still alive.
    Graphics->disconnect(this);
    Graphics->InteractionFinished = {};
    delete Graphics;
    Graphics = nullptr;
}

void StoryWorkspaceView::SetModel(StoryGraphModel InModel)
{
    const bool bReset = Model.DocumentIdentity != InModel.DocumentIdentity || (!Model.bOpen && InModel.bOpen);
    if (!bReset && InModel.bOpen && !InModel.bReadOnly && Graphics->IsInteracting())
    {
        PendingModel = std::move(InModel);
        return;
    }
    PendingModel.reset();
    if (ActionMenu && (bReset || !InModel.bOpen || InModel.bReadOnly != Model.bReadOnly ||
                       InModel.bLocked != Model.bLocked)) { ActionMenu->close(); }
    const auto Selected = Graphics->SelectedNodes();
    const QTransform Transform = Canvas->transform();
    const int Horizontal = Canvas->horizontalScrollBar()->value();
    const int Vertical = Canvas->verticalScrollBar()->value();
    const QPointF Center = Canvas->viewportTransform().inverted().map(
        QPointF(Canvas->viewport()->width() / 2.0, Canvas->viewport()->height() / 2.0));
    Model = std::move(InModel);
    const QSignalBlocker Blocker(Graphics);
    Graphics->SetGraph(Model);
    for (const auto& Identity : Selected)
    {
        for (auto* Item : Graphics->items())
        {
            if (Item->data(0).toString() == Identity)
            {
                Item->setSelected(true);
            }
        }
    }
    Summary->setText(Model.bOpen ? tr("Нод: %1 · единый поток%2")
                                       .arg(Model.Nodes.size())
                                       .arg(Model.bDirty ? QStringLiteral(" *") : QString())
                                 : Model.Diagnostic);
    Summary->setToolTip(Model.Title);
    if (Model.bReadOnly && Model.bOpen && !Model.bLocked)
    {
        Summary->setText(Summary->text() + tr(" · Только чтение"));
    }
    const bool bWritable = Model.bOpen && !Model.bReadOnly;
    AddButton->setEnabled(!Model.bLocked && (!Model.bOpen || !Model.bReadOnly));
    SaveButton->setEnabled(bWritable);
    UndoButton->setEnabled(bWritable && Model.bCanUndo);
    RedoButton->setEnabled(bWritable && Model.bCanRedo);
    OpenButton->setEnabled(!Model.bLocked);
    if (bReset)
    {
        QPointF InitialPosition;
        if (!Model.Nodes.isEmpty()) { InitialPosition = Model.Nodes.front().Position; }
        for (const auto& Node : Model.Nodes)
        {
            if (Node.Identity == Model.Start) { InitialPosition = Node.Position; break; }
        }
        static_cast<StoryCanvas*>(Canvas)->ResetNavigation(InitialPosition);
    }
    else
    {
        Canvas->setTransform(Transform);
        static_cast<StoryCanvas*>(Canvas)->EnsureVisibleExtent(Center);
        // Preserve the actual scroll offsets. QRect::center() rounds down;
        // round-tripping it through centerOn drifts by a pixel on each refresh.
        Canvas->horizontalScrollBar()->setValue(Horizontal);
        Canvas->verticalScrollBar()->setValue(Vertical);
    }
}

void StoryWorkspaceView::ClearSelection()
{
    const QSignalBlocker Blocker(Graphics);
    Graphics->clearSelection();
}

void StoryWorkspaceView::SelectNode(const QString& Identity)
{
    Graphics->SelectNode(Identity);
}

bool StoryWorkspaceView::DeleteSelection()
{
    CancelInteraction();
    return Graphics->DeleteSelection();
}

void StoryWorkspaceView::CancelInteraction()
{
    if (ActionMenu) { ActionMenu->close(); }
    Graphics->CancelInteraction();
    if (PendingModel)
    {
        auto Latest = std::move(*PendingModel);
        PendingModel.reset();
        SetModel(std::move(Latest));
    }
}

void StoryWorkspaceView::SelectEvent(const QString& Node, const QString& Binding)
{
    Graphics->SelectEvent(Node, Binding);
}
} // namespace EditorUi
