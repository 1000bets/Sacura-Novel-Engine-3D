#include "UI/Views/StoryGraphicsScene.h"
#include "UI/Styles/StoryGraphStyle.h"
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
    QPainterPath Path(Start);
    const qreal Reach = std::max(55.0, std::abs(Finish.x() - Start.x()) * 0.45);
    Path.cubicTo(Start + QPointF(Reach, 0), Finish - QPointF(Reach, 0), Finish);
    return Path;
}

class StoryPortItem final : public QGraphicsItem
{
  public:
    StoryPortItem(QString InIdentity, int InPort, bool bInOutput, QGraphicsItem* Parent)
        : QGraphicsItem(Parent), Identity(std::move(InIdentity)), Port(InPort), bOutput(bInOutput)
    {
        setZValue(3);
        setCursor(Qt::CrossCursor);
        setToolTip(bOutput ? QObject::tr("Перетащите к входу другой ноды") : QObject::tr("Вход перехода"));
    }
    int type() const override
    {
        return PortItemType;
    }
    QRectF boundingRect() const override
    {
        return {-12, -12, 24, 24};
    }
    void paint(QPainter* Painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        StoryGraphStyle::PaintPort(*Painter, bOutput);
    }
    QString Identity;
    int Port = -1;
    bool bOutput = false;
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
        setToolTip(Node.Text);
        auto* Input = new StoryPortItem(Node.Identity, -2, false, this);
        Input->setPos(StoryGraphStyle::InputAnchor());
        for (int Index = 0; Index < StoryGraphStyle::OutputCount(Node); ++Index)
        {
            const int Port = Node.KindId == 1 ? Index : -1;
            auto* Output = new StoryPortItem(Node.Identity, Port, true, this);
            Output->setPos(StoryGraphStyle::OutputAnchor(Node, Port));
        }
    }
    int type() const override
    {
        return CardItemType;
    }
    QRectF boundingRect() const override
    {
        return {0, 0, StoryGraphStyle::NodeWidth, StoryGraphStyle::Height(Node)};
    }
    void paint(QPainter* Painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        StoryGraphStyle::PaintCard(*Painter, Node, Title, isSelected());
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
};

class StoryEdgeItem final : public QGraphicsPathItem
{
  public:
    StoryEdgeItem(QString InSource, int InPort, QString InTarget)
        : Source(std::move(InSource)), Port(InPort), Target(std::move(InTarget))
    {
        setFlag(ItemIsSelectable);
        setZValue(-1);
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
        StoryGraphStyle::PaintConnection(*Painter, path(), isSelected());
    }
    QString Source;
    int Port = -1;
    QString Target;
};

StoryGraphicsScene::StoryGraphicsScene(QObject* Parent) : QGraphicsScene(Parent) {}

void StoryGraphicsScene::SetGraph(const StoryGraphModel& Model)
{
    const QSignalBlocker Blocker(this);
    CancelConnection();
    Cards.clear();
    Edges.clear();
    BeforeMove.clear();
    clear();
    bEditable = Model.bOpen && !Model.bReadOnly;
    int FallbackIndex = 0;
    for (const auto& Node : Model.Nodes)
    {
        auto* Card = new StoryCardItem(Node, Model.Title, bEditable);
        addItem(Card);
        Card->setPos(Node.bHasPosition ? Node.Position
                                       : QPointF(40 + FallbackIndex++ * StoryGraphStyle::ColumnSpacing, 30));
        Cards.insert(Node.Identity, Card);
    }
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
            Edge->setPath(ConnectionPath(Source->pos() + StoryGraphStyle::OutputAnchor(Source->Node, Edge->Port),
                                         Target->pos() + StoryGraphStyle::InputAnchor()));
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

void StoryGraphicsScene::mouseMoveEvent(QGraphicsSceneMouseEvent* Event)
{
    if (Preview != nullptr)
    {
        auto* Source = Cards.value(ConnectionSource);
        Preview->setPath(ConnectionPath(Source->pos() + StoryGraphStyle::OutputAnchor(Source->Node, ConnectionPort),
                                        Event->scenePos()));
        Event->accept();
        return;
    }
    QGraphicsScene::mouseMoveEvent(Event);
}

void StoryGraphicsScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* Event)
{
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
