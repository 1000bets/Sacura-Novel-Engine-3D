#include "UI/Views/StoryGraphicsScene.h"
#include "UI/Styles/StoryGraphStyle.h"
#include "UI/Styles/StoryEventStyle.h"
#include <QGraphicsPathItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPathStroker>
#include <QSignalBlocker>
#include <QTimer>
#include <QLineF>
#include <algorithm>
#include <cmath>

namespace EditorUi
{
namespace
{
constexpr int PortItemType = QGraphicsItem::UserType + 1;
constexpr int CardItemType = QGraphicsItem::UserType + 2;
constexpr int EdgeItemType = QGraphicsItem::UserType + 3;

QPainterPath ConnectionPath(QPointF Start, QPointF Finish)
{
    return StoryGraphStyle::ConnectionPath(Start, Finish);
}

class StoryPortItem final : public QGraphicsItem
{
  public:
    StoryPortItem(QString InIdentity, int InPort, bool bInOutput, QGraphicsItem* Parent)
        : QGraphicsItem(Parent), Identity(std::move(InIdentity)), Port(InPort), bOutput(bInOutput)
    {
        setZValue(3);
        setCursor(Qt::CrossCursor);
        setAcceptHoverEvents(true);
        setToolTip(bOutput ? QObject::tr("Перетащите к входу другой ноды") : QObject::tr("Вход перехода"));
    }
    int type() const override
    {
        return PortItemType;
    }
    QRectF boundingRect() const override
    {
        return {-14, -14, 28, 28};
    }
    void paint(QPainter* Painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        StoryGraphStyle::PaintPort(*Painter, bOutput, bHovered);
    }
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override { bHovered = true; update(); }
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override { bHovered = false; update(); }
    QString Identity;
    int Port = -1;
    bool bOutput = false;
    bool bHovered = false;
};
} // namespace

class StoryCardItem final : public QGraphicsItem
{
  public:
    StoryCardItem(StoryGraphNode InNode, QString InTitle, bool bEditable)
        : Node(std::move(InNode)), Title(std::move(InTitle))
    {
        setFlag(ItemIsSelectable);
        setFlag(ItemIsMovable, bEditable);
        setFlag(ItemSendsGeometryChanges);
        setData(0, Node.Identity);
        Geometry = StoryGraphStyle::Layout(Node);
        setToolTip(Node.Text);
        auto* Input = new StoryPortItem(Node.Identity, -2, false, this);
        Input->setPos(StoryGraphStyle::InputAnchor());
        for (int Index = 0; Index < StoryGraphStyle::OutputCount(Node); ++Index)
        {
            const int Port = Node.KindId == 1 ? Index : -1;
            auto* Output = new StoryPortItem(Node.Identity, Port, true, this);
            Output->setPos(StoryGraphStyle::OutputAnchor(Node, Port, &Geometry));
        }
    }
    int type() const override
    {
        return CardItemType;
    }
    QRectF boundingRect() const override
    {
        return {0, 0, StoryGraphStyle::NodeWidth, Geometry.Height};
    }
    void paint(QPainter* Painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        StoryGraphStyle::PaintCard(*Painter, Node, Title, isSelected(), &Geometry);
    }
    QVariant itemChange(GraphicsItemChange Change, const QVariant& Value) override
    {
        if (Change == ItemPositionHasChanged && scene() != nullptr)
        {
            static_cast<StoryGraphicsScene*>(scene())->UpdateConnections();
        }
        return QGraphicsItem::itemChange(Change, Value);
    }
    StoryGraphNode Node;
    QString Title;
    StoryGraphStyle::CardLayout Geometry;
};

class StoryEdgeItem final : public QGraphicsPathItem
{
  public:
    StoryEdgeItem(QString InSource, int InPort, QString InTarget)
        : Source(std::move(InSource)), Port(InPort), Target(std::move(InTarget))
    {
        setFlag(ItemIsSelectable);
        setZValue(-1);
        setAcceptHoverEvents(true);
        setToolTip(QObject::tr("Переход. Delete — удалить связь"));
    }
    int type() const override
    {
        return EdgeItemType;
    }
    QPainterPath shape() const override
    {
        QPainterPathStroker Stroker;
        Stroker.setWidth(14);
        return Stroker.createStroke(path());
    }
    void paint(QPainter* Painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        StoryGraphStyle::PaintConnection(*Painter, path(), isSelected() || bHovered);
    }
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override { bHovered = true; update(); }
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override { bHovered = false; update(); }
    QString Source;
    int Port = -1;
    QString Target;
    bool bHovered = false;
};

StoryGraphicsScene::StoryGraphicsScene(QObject* Parent) : QGraphicsScene(Parent) {}

void StoryGraphicsScene::SetGraph(const StoryGraphModel& Model)
{
    const QSignalBlocker Blocker(this);
    CancelConnection();
    Cards.clear();
    Edges.clear();
    BeforeMove.clear();
    bEventDragging = false; EventDragPreview = nullptr;
    bool KeepEventDrag = !Model.bReadOnly;
    bool HasDraggedNode = false;
    for (const auto& Node : Model.Nodes) { HasDraggedNode |= Node.Identity == DragEventNode; }
    if (!KeepEventDrag || !HasDraggedNode) { DragEventNode.clear(); DragBinding.clear(); }
    clear();
    bEditable = Model.bOpen && !Model.bReadOnly;
    EventLibrary = Model.Events;
    int FallbackIndex = 0;
    for (const auto& Node : Model.Nodes)
    {
        auto* Card = new StoryCardItem(Node, Model.Title, bEditable);
        addItem(Card);
        Card->setPos(Node.bHasPosition ? Node.Position
                                       : QPointF(40 + FallbackIndex++ * StoryGraphStyle::ColumnSpacing, 30));
        Cards.insert(Node.Identity, Card);
    }
    SelectEvent(SelectedEventNode, SelectedEventBinding);
    for (const auto& Node : Model.Nodes)
    {
        for (int Index = 0; Index < Node.Targets.size(); ++Index)
        {
            const auto& Target = Node.Targets[Index];
            if (!Cards.contains(Target))
            {
                continue;
            }
            auto* Edge = new StoryEdgeItem(Node.Identity, Node.KindId == 1 ? Index : -1, Target);
            addItem(Edge);
            Edges.push_back(Edge);
        }
    }
    UpdateConnections();
    if (Model.Nodes.isEmpty())
    {
        auto* Message =
            addText(Model.bOpen ? tr("Добавьте ноду через «+ Нода» или контекстное меню.") : Model.Diagnostic,
                    StoryGraphStyle::Font(13));
        Message->setDefaultTextColor(EditorStyle::Color("--text-muted"));
        Message->setTextWidth(380);
        Message->setPos(40, 40);
    }
    setSceneRect(itemsBoundingRect().adjusted(-300, -200, 300, 200).united(QRectF(0, 0, 1200, 800)));
}

void StoryGraphicsScene::UpdateConnections()
{
    for (auto* Edge : Edges)
    {
        const auto* Source = Cards.value(Edge->Source);
        const auto* Target = Cards.value(Edge->Target);
        if (Source != nullptr && Target != nullptr)
        {
            const QPointF From = Source->pos() + StoryGraphStyle::OutputAnchor(Source->Node, Edge->Port, &Source->Geometry);
            Edge->setPath(ConnectionPath(From, Target->pos() + StoryGraphStyle::InputAnchor()));
        }
    }
}

QStringList StoryGraphicsScene::SelectedNodes() const
{
    QStringList Result;
    for (auto* Item : selectedItems())
    {
        if (Item->type() == CardItemType)
        {
            Result.push_back(Item->data(0).toString());
        }
    }
    return Result;
}

void StoryGraphicsScene::SelectNode(const QString& Identity)
{
    SelectEvent({}, {});
    clearSelection();
    if (auto* Card = Cards.value(Identity))
    {
        Card->setSelected(true);
    }
}

bool StoryGraphicsScene::DeleteSelection()
{
    if (!bEditable)
    {
        return false;
    }
    if (!SelectedEventBinding.isEmpty() && RemoveEventRequested)
    { RemoveEventRequested(SelectedEventNode, SelectedEventBinding); return true; }
    const auto Nodes = SelectedNodes();
    if (!Nodes.isEmpty() && DeleteRequested)
    {
        DeleteRequested(Nodes);
        return true;
    }
    for (auto* Item : selectedItems())
    {
        if (Item->type() == EdgeItemType && ConnectRequested)
        {
            const auto* Edge = static_cast<StoryEdgeItem*>(Item);
            ConnectRequested(Edge->Source, Edge->Port, {});
            return true;
        }
    }
    return false;
}

void StoryGraphicsScene::CancelConnection()
{
    delete Preview;
    Preview = nullptr;
    ConnectionSource.clear();
}

void StoryGraphicsScene::mousePressEvent(QGraphicsSceneMouseEvent* Event)
{
    BeforeMove.clear();
    if (Event->button() == Qt::LeftButton)
    {
        for (auto* Item : items(Event->scenePos()))
        {
            if (Item->type() != CardItemType) { continue; }
            auto* Card = static_cast<StoryCardItem*>(Item);
            if (Card->Node.KindId != 0 && Card->Node.KindId != 1) { break; }
            const auto Local = Card->mapFromScene(Event->scenePos());
            const auto& G = Card->Geometry;
            const QRectF Phases[] = {G.Before, G.During, G.After};
            for (int Phase = 0; Phase < 3; ++Phase)
            {
                if (bEditable && QRectF(StoryGraphStyle::NodeWidth - 35, Phases[Phase].top() + 5, 28, 30).contains(Local))
                { ShowEventMenu(Card->Node.Identity, Phase, {}, Event->screenPos()); Event->accept(); return; }
            }
            for (const auto& Batch : G.Batches)
            {
                if (bEditable && Batch.Order.contains(Local))
                {
                    if (Local.x() > Batch.Order.right() - 28) { ShowEventMenu(Card->Node.Identity, Batch.Phase, Batch.Identity, Event->screenPos()); }
                    else if (EventModeRequested) { EventModeRequested(Card->Node.Identity, Batch.Identity, !Batch.bParallel); }
                    Event->accept(); return;
                }
                for (const auto& Row : Batch.Events)
                {
                    if (!Row.Rect.contains(Local)) { continue; }
                    const auto Node = Card->Node.Identity;
                    const auto Binding = Row.Binding;
                    clearSelection(); Card->setSelected(true);
                    SelectEvent(Node, Binding);
                    if (bEditable && Local.x() > Row.Rect.right() - 27 && Local.y() < Row.Rect.top() + 28)
                    { if (RemoveEventRequested) { RemoveEventRequested(Node, Binding); } }
                    else
                    {
                        if (EventSelected) { EventSelected(Node, Binding); }
                        if (bEditable) { DragEventNode = Node; DragBinding = Binding; DragOrigin = Event->scenePos(); }
                    }
                    Event->accept(); return;
                }
            }
            break;
        }
    }
    SelectEvent({}, {});
    if (bEditable && Event->button() == Qt::LeftButton)
    {
        for (auto* Item : items(Event->scenePos()))
        {
            if (Item->type() == PortItemType)
            {
                const auto* Port = static_cast<StoryPortItem*>(Item);
                if (Port->bOutput)
                {
                    CancelConnection();
                    ConnectionSource = Port->Identity;
                    ConnectionPort = Port->Port;
                    Preview = addPath(ConnectionPath(Port->scenePos(), Event->scenePos()),
                                      StoryGraphStyle::ConnectionPen(true));
                    Preview->setAcceptedMouseButtons(Qt::NoButton);
                    Preview->setZValue(5);
                    Event->accept();
                    return;
                }
            }
        }
        BeforeMove.clear();
        for (auto* Card : Cards)
        {
            BeforeMove.insert(Card->Node.Identity, Card->pos());
        }
    }
    QGraphicsScene::mousePressEvent(Event);
}

void StoryGraphicsScene::SelectEvent(const QString& Node, const QString& Binding)
{
    const QString RequestedNode = Node, RequestedBinding = Binding;
    SelectedEventNode.clear(); SelectedEventBinding.clear();
    for (auto* Card : Cards)
    {
        for (auto& Batch : Card->Node.EventBatches) { for (auto& Placement : Batch.Bindings)
        {
            Placement.bSelected = Card->Node.Identity == RequestedNode && Placement.Identity == RequestedBinding;
            if (Placement.bSelected) { SelectedEventNode = RequestedNode; SelectedEventBinding = RequestedBinding; }
        } }
        Card->update();
    }
}

void StoryGraphicsScene::ShowEventMenu(const QString& Node, int Phase, const QString& Batch, QPoint Screen)
{
    QMenu Menu;
    auto* New = Menu.addAction(tr("Новое событие"));
    auto* Import = Menu.addAction(tr("Импорт события…"));
    QMap<QAction*, QString> Sources;
    if (!EventLibrary.isEmpty()) { Menu.addSeparator(); }
    for (const auto& Event : EventLibrary) { Sources.insert(Menu.addAction(Event.Name), Event.Identity); }
    auto* Selected = Menu.exec(Screen);
    if (Selected == Import && ImportEventRequested) { ImportEventRequested(Node, Phase); }
    else if (Selected && AddEventRequested) { AddEventRequested(Node, Phase, Batch, Selected == New ? QString{} : Sources.value(Selected)); }
}

void StoryGraphicsScene::mouseMoveEvent(QGraphicsSceneMouseEvent* Event)
{
    if (!DragBinding.isEmpty())
    {
        if (!bEventDragging && QLineF(DragOrigin, Event->scenePos()).length() > 8)
        {
            bEventDragging = true;
            auto* Label = addText(tr("Переместить событие"), StoryGraphStyle::Font(12));
            Label->setDefaultTextColor(EditorStyle::Color("--accent"));
            Label->setZValue(10); Label->setAcceptedMouseButtons(Qt::NoButton); EventDragPreview = Label;
        }
        if (EventDragPreview) { EventDragPreview->setPos(Event->scenePos() + QPointF(12, 12)); }
        Event->accept(); return;
    }
    if (Preview != nullptr)
    {
        auto* Source = Cards.value(ConnectionSource);
        const QPointF From = Source->pos() + StoryGraphStyle::OutputAnchor(Source->Node, ConnectionPort, &Source->Geometry);
        Preview->setPath(ConnectionPath(From, Event->scenePos()));
        Event->accept();
        return;
    }
    QGraphicsScene::mouseMoveEvent(Event);
}

void StoryGraphicsScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* Event)
{
    if (!DragBinding.isEmpty())
    {
        const auto Node = DragEventNode, Binding = DragBinding;
        auto* Card = Cards.value(Node);
        const bool Dragged = bEventDragging;
        delete EventDragPreview; EventDragPreview = nullptr; DragBinding.clear(); DragEventNode.clear(); bEventDragging = false;
        if (Dragged && Card && MoveEventRequested)
        {
            const auto Local = Card->mapFromScene(Event->scenePos());
            const QRectF Phases[] = {Card->Geometry.Before, Card->Geometry.During, Card->Geometry.After};
            for (int Phase = 0; Phase < 3; ++Phase)
            {
                if (!Phases[Phase].contains(Local)) { continue; }
                QString Destination, Before;
                for (const auto& Batch : Card->Geometry.Batches)
                {
                    if (Batch.Phase != Phase) { continue; }
                    if (Batch.Order.contains(Local)) { Destination = Batch.Identity; break; }
                    for (const auto& Row : Batch.Events)
                    {
                        if (Row.Rect.contains(Local)) { Destination = Batch.Identity; Before = Row.Binding; break; }
                    }
                    if (!Destination.isEmpty()) { break; }
                }
                MoveEventRequested(Node, Binding, Phase, Destination, Before); break;
            }
        }
        Event->accept(); return;
    }
    if (Preview != nullptr)
    {
        QString Target;
        qreal Closest = 25;
        for (auto* Card : Cards)
        {
            const qreal Distance = QLineF(Event->scenePos(), Card->pos() + StoryGraphStyle::InputAnchor()).length();
            if (Distance < Closest)
            {
                Closest = Distance;
                Target = Card->Node.Identity;
            }
        }
        const auto Source = ConnectionSource;
        const auto Port = ConnectionPort;
        CancelConnection();
        if (!Target.isEmpty() && ConnectRequested)
        {
            ConnectRequested(Source, Port, Target);
        }
        Event->accept();
        return;
    }
    QGraphicsScene::mouseReleaseEvent(Event);
    QMap<QString, QPointF> Moved;
    for (auto* Card : Cards)
    {
        if (BeforeMove.contains(Card->Node.Identity) && BeforeMove.value(Card->Node.Identity) != Card->pos())
        {
            Moved.insert(Card->Node.Identity, Card->pos());
        }
    }
    BeforeMove.clear();
    if (bEditable && !Moved.isEmpty() && MoveRequested)
    {
        MoveRequested(Moved);
    }
}

void StoryGraphicsScene::contextMenuEvent(QGraphicsSceneContextMenuEvent* Event)
{
    if (!bEditable)
    {
        Event->accept();
        return;
    }
    QMenu Menu(Event->widget());
    QGraphicsItem* Hit = itemAt(Event->scenePos(), QTransform());
    while (Hit != nullptr && Hit->type() == PortItemType)
    {
        Hit = Hit->parentItem();
    }
    if (Hit != nullptr && Hit->type() == CardItemType)
    {
        const QString Identity = Hit->data(0).toString();
        SelectNode(Identity);
        Menu.addAction(tr("Сделать началом истории"),
                       [&]()
                       {
                           if (StartRequested)
                           {
                               StartRequested(Identity);
                           }
                       });
        Menu.addSeparator();
        Menu.addAction(tr("Удалить ноду"), [&]() { DeleteSelection(); });
    }
    else if (Hit != nullptr && Hit->type() == EdgeItemType)
    {
        clearSelection();
        Hit->setSelected(true);
        Menu.addAction(tr("Удалить связь"), [&]() { DeleteSelection(); });
    }
    else
    {
        const QStringList Kinds = {tr("Реплика"), tr("Выбор"), tr("Действие"), tr("Концовка")};
        for (int Kind = 0; Kind < Kinds.size(); ++Kind)
        {
            Menu.addAction(Kinds[Kind],
                           [&, Kind]()
                           {
                               if (AddRequested)
                               {
                                   AddRequested(Kind, Event->scenePos());
                               }
                           });
        }
    }
    Menu.exec(Event->screenPos());
    Event->accept();
}
} // namespace EditorUi
