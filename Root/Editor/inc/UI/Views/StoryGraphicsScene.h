#pragma once
#include "UI/Presentation/StoryGraphModel.h"
#include <QGraphicsScene>
#include <QHash>
#include <QMap>
#include <QPointer>
#include <functional>

namespace EditorUi
{
class StudioActionMenu;
class StoryCardItem;
class StoryEdgeItem;
class StoryGraphicsScene final : public QGraphicsScene
{
  public:
    explicit StoryGraphicsScene(QObject* Parent = nullptr);
    void SetGraph(const StoryGraphModel& Model);
    void UpdateConnections(const QString& MovedNode = {});
    bool IsInteracting() const { return bPointerInteraction; }
    void CancelInteraction();
    std::function<void()> InteractionFinished;
    QStringList SelectedNodes() const;
    bool DeleteSelection();
    void SelectNode(const QString& Identity);
    void SelectEvent(const QString& Node, const QString& Binding);
    std::function<void(QPointF, QPoint)> AddMenuRequested;
    void ShowEventMenu(const QString& Node, int Phase, const QString& Batch, QPoint Screen);
    std::function<void(QStringList)> DeleteRequested;
    std::function<void(QString, int, QString)> ConnectRequested;
    std::function<void(QMap<QString, QPointF>)> MoveRequested;
    std::function<void(QString)> StartRequested;
    std::function<void(QString, int, QString, QString)> AddEventRequested;
    std::function<void(QString, QString)> EventSelected, RemoveEventRequested;
    std::function<void(QString, QString, bool)> EventModeRequested;
    std::function<void(QString, int)> ImportEventRequested;
    std::function<void(QString, QString, int, QString, QString)> MoveEventRequested;

  protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* Event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* Event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* Event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* Event) override;

  private:
    void CancelConnection();
    QHash<QString, StoryCardItem*> Cards;
    QVector<StoryEdgeItem*> Edges;
    QHash<QString, QVector<StoryEdgeItem*>> IncidentEdges;
    QMap<QString, QPointF> BeforeMove;
    bool bEditable = false;
    bool bPointerInteraction = false;
    QString ConnectionSource;
    int ConnectionPort = -1;
    QGraphicsPathItem* Preview = nullptr;
    QVector<StoryGraphEvent> EventLibrary;
    QPointer<StudioActionMenu> ActionMenu;
    QString MenuDocumentIdentity;
    QString DragEventNode, DragBinding;
    QPointF DragOrigin;
    bool bEventDragging = false;
    QGraphicsItem* EventDragPreview = nullptr;
    QString SelectedEventNode, SelectedEventBinding;
};
} // namespace EditorUi
