#include "UI/Views/StoryGraphicsScene.h"
#include "UI/Styles/StoryGraphStyle.h"
#include "UI/Styles/StoryEventStyle.h"
#include <QGraphicsPathItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include "UI/Views/StudioActionMenu.h"
#include "UI/Adapters/StoryMenuBuilder.h"
#include "UI/Styles/StudioIcons.h"
#include <QGraphicsView>
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
        if (Port >= 1000)
        {
            Painter->setRenderHint(QPainter::Antialiasing);
            Painter->setPen(QPen(EditorStyle::Color("--accent"), 2));
            Painter->setBrush(EditorStyle::Color("--bg-panel"));
            Painter->drawEllipse(QPointF(0, 0), 6, 6);
            return;
        }
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
        setCacheMode(DeviceCoordinateCache);
        setCursor(bEditable ? Qt::OpenHandCursor : Qt::ArrowCursor);
        setData(0, Node.Identity);
        Geometry = StoryGraphStyle::Layout(Node);
        setToolTip(Node.Text);
        if (!Node.NarrativeDefinition.bPure)
        {
            auto* Input = new StoryPortItem(Node.Identity, -2, false, this);
            Input->setPos(StoryGraphStyle::InputAnchor());
        }
        for (int Index = 0; Index < Geometry.DataInputs.size(); ++Index)
        {
            auto* Input = new StoryPortItem(Node.Identity, 1000 + Index, false, this);
            Input->setPos(-4, Geometry.DataInputs[Index].center().y());
            Input->setToolTip(QString::fromStdString(Node.NarrativeDefinition.Inputs[Index].Property.Id.Value));
        }
        for (int Index = 0; Index < Geometry.DataOutputs.size(); ++Index)
        {
            auto* Output = new StoryPortItem(Node.Identity, 1000 + Index, true, this);
            Output->setPos(StoryGraphStyle::NodeWidth + 4, Geometry.DataOutputs[Index].center().y());
            Output->setToolTip(QString::fromStdString(Node.NarrativeDefinition.Outputs[Index].Property.Id.Value));
        }
        for (int Index = 0; Index < StoryGraphStyle::OutputCount(Node); ++Index)
        {
            const int Port = (Node.KindId == 1 || Node.KindId == 4) ? Index : -1;
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
            static_cast<StoryGraphicsScene*>(scene())->UpdateConnections(Node.Identity);
        }
        return QGraphicsItem::itemChange(Change, Value);
    }
    void mousePressEvent(QGraphicsSceneMouseEvent* Event) override
    {
        if (flags().testFlag(ItemIsMovable)) { setCursor(Qt::ClosedHandCursor); setZValue(2); }
        QGraphicsItem::mousePressEvent(Event);
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* Event) override
    {
        QGraphicsItem::mouseReleaseEvent(Event);
        setCursor(flags().testFlag(ItemIsMovable) ? Qt::OpenHandCursor : Qt::ArrowCursor);
        setZValue(0);
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
    QRectF boundingRect() const override
    {
        // Include the halo, arrow and pick stroke in Qt's dirty region.
        return path().controlPointRect().adjusted(-16, -16, 16, 16);
    }
    QPainterPath shape() const override
    {
        QPainterPathStroker Stroker;
        Stroker.setWidth(14);
        return Stroker.createStroke(path());
    }
    void paint(QPainter* Painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        if (InputIndex >= 0)
        {
            Painter->setRenderHint(QPainter::Antialiasing);
            QPen Pen(EditorStyle::Color("--accent"), 2);
            if (isSelected() || bHovered)
            {
                Pen.setWidth(3);
            }
            Painter->setPen(Pen);
            Painter->setBrush(Qt::NoBrush);
            Painter->drawPath(path());
            return;
        }
        StoryGraphStyle::PaintConnection(*Painter, path(), isSelected() || bHovered);
    }
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override { bHovered = true; update(); }
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override { bHovered = false; update(); }
    QString Source;
    int Port = -1;
    QString Target;
    QString InputName;
    QString OutputName;
    int InputIndex = -1;
    bool bHovered = false;
};

StoryGraphicsScene::StoryGraphicsScene(QObject* Parent) : QGraphicsScene(Parent) {}

void StoryGraphicsScene::SetGraph(const StoryGraphModel& Model)
{
    if (ActionMenu && (MenuDocumentIdentity != Model.DocumentIdentity || !Model.bOpen || Model.bReadOnly || Model.bLocked))
    { ActionMenu->close(); }
    MenuDocumentIdentity = Model.DocumentIdentity;
    const QSignalBlocker Blocker(this);
    CancelConnection();
    bPointerInteraction = false;
    Cards.clear();
    Edges.clear();
    IncidentEdges.clear();
    BeforeMove.clear();
    bEventDragging = false; EventDragPreview = nullptr;
    bool KeepEventDrag = !Model.bReadOnly;
    bool HasDraggedNode = false;
    for (const auto& Node : Model.Nodes) { HasDraggedNode |= Node.Identity == DragEventNode; }
    if (!KeepEventDrag || !HasDraggedNode) { DragEventNode.clear(); DragBinding.clear(); }
    clear();
    bEditable = Model.bOpen && !Model.bReadOnly && !Model.bLocked;
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
            auto* Edge = new StoryEdgeItem(Node.Identity, (Node.KindId == 1 || Node.KindId == 4) ? Index : -1, Target);
            addItem(Edge);
            Edges.push_back(Edge);
            IncidentEdges[Node.Identity].push_back(Edge);
            if (Target != Node.Identity) { IncidentEdges[Target].push_back(Edge); }
        }
    }
    for (const auto& Node : Model.Nodes)
    {
        for (size_t InputIndex = 0; InputIndex < Node.NarrativeDefinition.Inputs.size(); ++InputIndex)
        {
            const auto& InputName = Node.NarrativeDefinition.Inputs[InputIndex].Property.Id.Value;
            const auto Connection = Node.NarrativeSources.find(InputName);
            if (Connection == Node.NarrativeSources.end())
            {
                continue;
            }
            const QString Producer = QString::fromStdString(Connection->second.first);
            const auto* Source = Cards.value(Producer);
            if (!Source)
            {
                continue;
            }
            for (size_t OutputIndex = 0; OutputIndex < Source->Node.NarrativeDefinition.Outputs.size(); ++OutputIndex)
            {
                const auto& OutputName = Source->Node.NarrativeDefinition.Outputs[OutputIndex].Property.Id.Value;
                if (OutputName != Connection->second.second)
                {
                    continue;
                }
                auto* Edge = new StoryEdgeItem(Producer, 1000 + static_cast<int>(OutputIndex), Node.Identity);
                Edge->InputName = QString::fromStdString(InputName);
                Edge->OutputName = QString::fromStdString(OutputName);
                Edge->InputIndex = static_cast<int>(InputIndex);
                Edge->setToolTip(tr("Данные. Delete — удалить связь"));
                addItem(Edge);
                Edges.push_back(Edge);
                IncidentEdges[Producer].push_back(Edge);
                IncidentEdges[Node.Identity].push_back(Edge);
            }
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

void StoryGraphicsScene::UpdateConnections(const QString& MovedNode)
{
    const auto& Changed = MovedNode.isEmpty() ? Edges : IncidentEdges[MovedNode];
    for (auto* Edge : Changed)
    {
        const auto* Source = Cards.value(Edge->Source);
        const auto* Target = Cards.value(Edge->Target);
        if (Source != nullptr && Target != nullptr)
        {
            const QPointF From = Source->pos() + StoryGraphStyle::OutputAnchor(Source->Node, Edge->Port, &Source->Geometry)
                                 + QPointF(StoryGraphStyle::PortSize / 2, 0);
            // End at the socket border so its square does not cover the arrowhead.
            QPointF Anchor = StoryGraphStyle::InputAnchor();
            if (Edge->InputIndex >= 0)
            {
                Anchor = QPointF(-4, Target->Geometry.DataInputs[Edge->InputIndex].center().y());
            }
            const QPointF To = Target->pos() + Anchor - QPointF(StoryGraphStyle::PortSize / 2, 0);
            Edge->setPath(ConnectionPath(From, To));
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
            if (Edge->InputIndex >= 0 && ConnectDataRequested)
            {
                ConnectDataRequested({}, {}, Edge->Target, Edge->InputName);
            }
            else
            {
                ConnectRequested(Edge->Source, Edge->Port, {});
            }
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
                        bPointerInteraction = bEditable;
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
                    Preview = addPath(ConnectionPath(Port->scenePos() + QPointF(StoryGraphStyle::PortSize / 2, 0), Event->scenePos()),
                                      StoryGraphStyle::ConnectionPen(true));
                    Preview->setAcceptedMouseButtons(Qt::NoButton);
                    Preview->setZValue(5);
                    bPointerInteraction = true;
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
        bPointerInteraction = true;
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
    if (!bEditable || !Cards.contains(Node)) { return; }
    if (ActionMenu) { ActionMenu->close(); }
    const QPointer<StoryGraphicsScene> Guard(this);
    auto Items = BuildStoryEventMenu(EventLibrary,
        [Guard, Node, Phase, Batch](QString Source)
        { if (Guard && Guard->bEditable && Guard->Cards.contains(Node) && Guard->AddEventRequested)
            { Guard->AddEventRequested(Node, Phase, Batch, Source); } },
        [Guard, Node, Phase]()
        { if (Guard && Guard->bEditable && Guard->Cards.contains(Node) && Guard->ImportEventRequested)
            { Guard->ImportEventRequested(Node, Phase); } });
    ActionMenu = new StudioActionMenu(tr("Добавить событие"), std::move(Items), views().isEmpty() ? nullptr : views().front());
    connect(this, &QObject::destroyed, ActionMenu, &QWidget::close);
    ActionMenu->Open(Screen);
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
        const QPointF From = Source->pos() + StoryGraphStyle::OutputAnchor(Source->Node, ConnectionPort, &Source->Geometry)
                             + QPointF(StoryGraphStyle::PortSize / 2, 0);
        QPointF To = Event->scenePos();
        qreal Closest = 25;
        for (auto* Card : Cards)
        {
            const QPointF Anchor = Card->pos() + StoryGraphStyle::InputAnchor();
            const qreal Distance = QLineF(Event->scenePos(), Anchor).length();
            if (Distance < Closest) { Closest = Distance; To = Anchor - QPointF(StoryGraphStyle::PortSize / 2, 0); }
        }
        Preview->setPath(ConnectionPath(From, To));
        Event->accept();
        return;
    }
    QGraphicsScene::mouseMoveEvent(Event);
}

void StoryGraphicsScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* Event)
{
    if (Event->button() != Qt::LeftButton) { QGraphicsScene::mouseReleaseEvent(Event); return; }
    bPointerInteraction = false;
    // Run after release callbacks: the controller's latest revision supersedes
    // any inspector/object refresh deferred while the mouse owned a card.
    QTimer::singleShot(0, this, [this]() { if (InteractionFinished) { InteractionFinished(); } });
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
        if (ConnectionPort >= 1000)
        {
            const auto* Source = Cards.value(ConnectionSource);
            const QString Output = QString::fromStdString(Source->Node.NarrativeDefinition.Outputs[ConnectionPort - 1000].Property.Id.Value);
            const auto SourceIdentity = ConnectionSource;
            QString TargetIdentity;
            QString Input;
            for (auto* Item : items(Event->scenePos()))
            {
                if (Item->type() == PortItemType)
                {
                    const auto* Port = static_cast<StoryPortItem*>(Item);
                    if (!Port->bOutput && Port->Port >= 1000)
                    {
                        TargetIdentity = Port->Identity;
                        Input = QString::fromStdString(Cards.value(TargetIdentity)->Node.NarrativeDefinition.Inputs[Port->Port - 1000].Property.Id.Value);
                        break;
                    }
                }
            }
            CancelConnection();
            if (!TargetIdentity.isEmpty() && ConnectDataRequested)
            {
                ConnectDataRequested(SourceIdentity, Output, TargetIdentity, Input);
            }
            Event->accept();
            return;
        }
        QString Target;
        qreal Closest = 25;
        for (auto* Card : Cards)
        {
            if (Card->Node.NarrativeDefinition.bPure)
            {
                continue;
            }
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

void StoryGraphicsScene::CancelInteraction()
{
    if (ActionMenu) { ActionMenu->close(); }
    bPointerInteraction = false;
    if (auto* Grabber = mouseGrabberItem()) { Grabber->ungrabMouse(); }
    CancelConnection();
    delete EventDragPreview;
    EventDragPreview = nullptr;
    DragBinding.clear(); DragEventNode.clear(); bEventDragging = false;
    for (auto* Card : Cards)
    {
        if (BeforeMove.contains(Card->Node.Identity)) { Card->setPos(BeforeMove.value(Card->Node.Identity)); }
        Card->setCursor(bEditable ? Qt::OpenHandCursor : Qt::ArrowCursor);
        Card->setZValue(0);
    }
    BeforeMove.clear();
}

void StoryGraphicsScene::contextMenuEvent(QGraphicsSceneContextMenuEvent* Event)
{
    Event->accept();
    if (!bEditable) { return; }
    QGraphicsItem* Hit = itemAt(Event->scenePos(), QTransform());
    while (Hit != nullptr && Hit->type() == PortItemType) { Hit = Hit->parentItem(); }
    QVector<StudioMenuItem> Items;
    const QPointer<StoryGraphicsScene> Guard(this);
    auto Add = [&](const char* Identity, const QString& Label, StudioIcon Icon, std::function<void()> Invoke)
    {
        StudioMenuItem Item;
        Item.Identity = QString::fromLatin1(Identity);
        Item.Label = Label;
        Item.Icon = MakeStudioIcon(Icon, "--text-primary");
        Item.Invoke = std::move(Invoke);
        Items.push_back(std::move(Item));
    };
    if (Hit != nullptr && Hit->type() == CardItemType)
    {
        const QString Identity = Hit->data(0).toString();
        SelectNode(Identity);
        Add("story-start", tr("Сделать началом истории"), StudioIcon::Play,
            [Guard, Identity]() { if (Guard && Guard->bEditable && Guard->Cards.contains(Identity) && Guard->StartRequested)
                { Guard->StartRequested(Identity); } });
        Add("delete-node", tr("Удалить ноду"), StudioIcon::Trash,
            [Guard, Identity]() { if (Guard && Guard->bEditable && Guard->Cards.contains(Identity) && Guard->DeleteRequested)
                { Guard->DeleteRequested({Identity}); } });
    }
    else if (Hit != nullptr && Hit->type() == EdgeItemType)
    {
        clearSelection();
        Hit->setSelected(true);
        const auto* Edge = static_cast<StoryEdgeItem*>(Hit);
        Add("delete-edge", tr("Удалить связь"), StudioIcon::Trash,
            [Guard, Source = Edge->Source, Port = Edge->Port]()
            { if (Guard && Guard->bEditable && Guard->ConnectRequested) { Guard->ConnectRequested(Source, Port, {}); } });
    }
    else
    {
        if (AddMenuRequested) { AddMenuRequested(Event->scenePos(), Event->screenPos()); }
        return;
    }
    if (ActionMenu) { ActionMenu->close(); }
    ActionMenu = new StudioActionMenu(tr("Поток истории"), std::move(Items), Event->widget());
    connect(this, &QObject::destroyed, ActionMenu, &QWidget::close);
    ActionMenu->Open(Event->screenPos());
}
} // namespace EditorUi
