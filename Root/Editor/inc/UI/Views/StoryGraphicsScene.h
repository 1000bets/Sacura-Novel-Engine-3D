#pragma once
#include "UI/Presentation/StoryGraphModel.h"
#include <QGraphicsScene>
#include <QHash>
#include <QMap>
#include <functional>

namespace EditorUi
{
class StoryCardItem;
class StoryEdgeItem;
class StoryGraphicsScene final : public QGraphicsScene
{
  public:
    explicit StoryGraphicsScene(QObject* Parent = nullptr);
    void SetGraph(const StoryGraphModel& Model);
    void UpdateConnections();
    QStringList SelectedNodes() const;
    bool DeleteSelection();
    void SelectNode(const QString& Identity);
    std::function<void(int, QPointF)> AddRequested;
    std::function<void(QStringList)> DeleteRequested;
    std::function<void(QString, int, QString)> ConnectRequested;
    std::function<void(QMap<QString, QPointF>)> MoveRequested;
    std::function<void(QString)> StartRequested;

  protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* Event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* Event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* Event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* Event) override;

  private:
    void CancelConnection();
    QHash<QString, StoryCardItem*> Cards;
    QVector<StoryEdgeItem*> Edges;
    QMap<QString, QPointF> BeforeMove;
    bool bEditable = false;
    QString ConnectionSource;
    int ConnectionPort = -1;
    QGraphicsPathItem* Preview = nullptr;
};
} // namespace EditorUi
