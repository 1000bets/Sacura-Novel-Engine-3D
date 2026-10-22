#include "UI/Views/StoryWorkspaceView.h"
#include "UI/Views/StoryGraphicsScene.h"
#include <QMenu>
#include <QKeyEvent>
#include <QScrollBar>
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
#include <algorithm>
#include <cmath>

namespace EditorUi
{
namespace
{
class StoryCanvas final : public QGraphicsView
{
  public:
    explicit StoryCanvas(QGraphicsScene* Scene, StoryWorkspaceView* Parent)
        : QGraphicsView(Scene, Parent), Workspace(*Parent)
    {
        setObjectName("StoryCanvas");
        setFrameShape(QFrame::NoFrame);
        setRenderHint(QPainter::Antialiasing);
        setDragMode(QGraphicsView::ScrollHandDrag);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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
            Button->setFixedSize(26, 26);
            Controls->addWidget(Button);
        }
        connect(ZoomIn, &QPushButton::clicked, this, [this]() { Zoom(1.15); });
        connect(ZoomOut, &QPushButton::clicked, this, [this]() { Zoom(1.0 / 1.15); });
        connect(Fit, &QPushButton::clicked, this, [this]() { FitGraph(); });
        Navigation->adjustSize();
    }

    void FitGraph()
    {
        bKeepOriginOnResize = false;
        fitInView(scene()->itemsBoundingRect().adjusted(-30, -30, 30, 30), Qt::KeepAspectRatio);
    }
    void ResetNavigation(QPointF NodePosition)
    {
        bKeepOriginOnResize = true;
        InitialNodePosition = NodePosition;
        resetTransform();
        scale(0.8, 0.8);
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
            // Rebuilding the scene cancels a pending connection without writing history.
            Workspace.SetModel(Workspace.GetModel());
        }
        else if (Event->key() == Qt::Key_Delete)
        {
            Workspace.DeleteSelection();
        }
        else if (Event->matches(QKeySequence::Undo) && Workspace.UndoRequested)
        {
            Workspace.UndoRequested();
        }
        else if (Event->matches(QKeySequence::Redo) && Workspace.RedoRequested)
        {
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
        if (Navigation != nullptr)
        {
            Navigation->move(8, std::max(8, viewport()->height() - Navigation->height() - 8));
        }
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
        // StoryFlow focuses the selected entry at (40,30), zoom .8. Extend the
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
    Tools->setContentsMargins(12, 0, 12, 0);
    Summary = new QLabel(Toolbar);
    Summary->setObjectName("MutedLabel");
    Summary->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    Tools->addWidget(Summary, 1);
    auto* Fit = new QPushButton(MakeStudioIcon(StudioIcon::Fit), tr("Вся история"), Toolbar);
    Fit->setProperty("uiRole", "chromeButton");
    Tools->addWidget(Fit);
    auto MakeTool = [&](const QString& Text, const QString& Name)
    {
        auto* Button = new QPushButton(Text, Toolbar);
        Button->setObjectName(Name);
        Button->setProperty("uiRole", "chromeButton");
        Tools->addWidget(Button);
        return Button;
    };
    UndoButton = MakeTool(QString::fromUtf8("↶"), "StoryUndo");
    UndoButton->setToolTip(tr("Отменить (Ctrl+Z)"));
    RedoButton = MakeTool(QString::fromUtf8("↷"), "StoryRedo");
    RedoButton->setToolTip(tr("Повторить (Ctrl+Shift+Z)"));
    OpenButton = MakeTool(tr("Открыть"), "StoryOpen");
    SaveButton = MakeTool(tr("Сохранить"), "StorySave");
    SaveButton->setToolTip(tr("Сохранить историю (Ctrl+S)"));
    AddButton = MakeTool(tr("+ Нода"), "StoryAddNode");
    auto* Menu = new QMenu(AddButton);
    const QStringList Kinds = {tr("Реплика"), tr("Выбор"), tr("Действие"), tr("Концовка")};
    for (int Index = 0; Index < Kinds.size(); ++Index)
    {
        Menu->addAction(Kinds[Index], this,
                        [this, Index]()
                        {
                            if (AddNodeRequested)
                            {
                                AddNodeRequested(Index, Canvas->mapToScene(Canvas->viewport()->rect().center()) -
                                                            QPointF(StoryGraphStyle::NodeWidth / 2,
                                                                    StoryGraphStyle::NodeHeight / 2));
                            }
                        });
    }
    AddButton->setMenu(Menu);
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
    Layout->addWidget(Toolbar);
    auto* Body = new QHBoxLayout();
    Body->setContentsMargins(0, 0, 0, 0);
    Body->setSpacing(0);
    auto* Variables = new QWidget(this);
    Variables->setObjectName("BlueprintSidebar");
    Variables->setFixedWidth(244);
    auto* Sidebar = new QVBoxLayout(Variables);
    Sidebar->setContentsMargins(12, 12, 12, 12);
    Sidebar->setSpacing(10);
    auto* Heading = new QLabel(tr("Мой Blueprint"), Variables);
    Heading->setObjectName("BlueprintHeading");
    Sidebar->addWidget(Heading);
    Sidebar->addWidget(new QLabel(tr("Переменные"), Variables));
    auto* Search = new QLineEdit(Variables);
    Search->setPlaceholderText(tr("Поиск переменных…"));
    Search->setReadOnly(true);
    Sidebar->addWidget(Search);
    auto* Note = new QLabel(tr("В этой истории пока нет переменных."), Variables);
    Note->setWordWrap(true);
    Note->setObjectName("MutedLabel");
    Sidebar->addWidget(Note);
    Sidebar->addStretch();
    auto* Hint = new QLabel(tr("Колесо — прокрутка\nCtrl+колесо — масштаб"), Variables);
    Hint->setObjectName("MutedLabel");
    Sidebar->addWidget(Hint);
    Body->addWidget(Variables);
    Graphics = new StoryGraphicsScene(this);
    Graphics->AddEventRequested = [this](QString Node, int Phase, QString Batch, QString Source)
    { if (AddEventRequested) { AddEventRequested(Node, Phase, Batch, Source); } };
    Graphics->EventSelected = [this](QString Node, QString Binding) { if (EventSelected) { EventSelected(Node, Binding); } };
    Graphics->RemoveEventRequested = [this](QString Node, QString Binding) { if (RemoveEventRequested) { RemoveEventRequested(Node, Binding); } };
    Graphics->EventModeRequested = [this](QString Node, QString Batch, bool Parallel) { if (EventModeRequested) { EventModeRequested(Node, Batch, Parallel); } };
    Graphics->ImportEventRequested = [this](QString Node, int Phase) { if (ImportEventRequested) { ImportEventRequested(Node, Phase); } };
    Graphics->MoveEventRequested = [this](QString Node, QString Binding, int Phase, QString Batch, QString Before) { if (MoveEventRequested) { MoveEventRequested(Node, Binding, Phase, Batch, Before); } };
    Graphics->AddRequested = [this](int Kind, QPointF Position)
    {
        if (AddNodeRequested)
        {
            AddNodeRequested(Kind, Position);
        }
    };
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

void StoryWorkspaceView::SetModel(StoryGraphModel InModel)
{
    const bool bReset = Model.DocumentIdentity != InModel.DocumentIdentity || (!Model.bOpen && InModel.bOpen);
    const auto Selected = Graphics->SelectedNodes();
    const QTransform Transform = Canvas->transform();
    const QPointF Center = Canvas->mapToScene(Canvas->viewport()->rect().center());
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
    Summary->setText(Model.bOpen ? tr("%1 · %2 блоков · единый поток%3")
                                       .arg(Model.Title)
                                       .arg(Model.Nodes.size())
                                       .arg(Model.bDirty ? QStringLiteral(" *") : QString())
                                 : Model.Diagnostic);
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
        Canvas->centerOn(Center);
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
    return Graphics->DeleteSelection();
}

void StoryWorkspaceView::SelectEvent(const QString& Node, const QString& Binding)
{
    Graphics->SelectEvent(Node, Binding);
}
} // namespace EditorUi
