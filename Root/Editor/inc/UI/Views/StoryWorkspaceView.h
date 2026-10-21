#pragma once
#include "UI/Presentation/StoryGraphModel.h"
#include <QWidget>
#include <functional>
#include <QMap>

class QGraphicsView;
class QGraphicsScene;
class QLabel;
class QPushButton;

namespace EditorUi
{
class StoryGraphicsScene;
class StoryWorkspaceView final : public QWidget
{
  public:
    explicit StoryWorkspaceView(QWidget* Parent = nullptr);
    void SetModel(StoryGraphModel Model);
    const StoryGraphModel& GetModel() const
    {
        return Model;
    }
    void ClearSelection();
    void SelectNode(const QString& Identity);
    bool DeleteSelection();
    std::function<void(const StoryGraphNode&)> NodeSelected;
    std::function<void(int, QPointF)> AddNodeRequested;
    std::function<void(QStringList)> DeleteNodesRequested;
    std::function<void(QString, int, QString)> ConnectRequested;
    std::function<void(QMap<QString, QPointF>)> MoveNodesRequested;
    std::function<void(QString)> StartNodeRequested;
    std::function<void()> UndoRequested;
    std::function<void()> RedoRequested;
    std::function<void()> SaveRequested;
    std::function<void()> OpenRequested;

  private:
    StoryGraphModel Model;
    QGraphicsView* Canvas = nullptr;
    StoryGraphicsScene* Graphics = nullptr;
    QLabel* Summary = nullptr;
    QPushButton* AddButton = nullptr;
    QPushButton* UndoButton = nullptr;
    QPushButton* RedoButton = nullptr;
    QPushButton* SaveButton = nullptr;
    QPushButton* OpenButton = nullptr;
};
} // namespace EditorUi
