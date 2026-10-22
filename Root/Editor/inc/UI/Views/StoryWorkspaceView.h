#pragma once
#include "UI/Presentation/StoryGraphModel.h"
#include <QWidget>
#include <QPointer>
#include <functional>
#include <QMap>
#include <optional>

class QGraphicsView;
class QGraphicsScene;
class QLabel;
class QPushButton;
class QScrollArea;

namespace EditorUi
{
class StoryGraphicsScene;
class StudioActionMenu;
class StoryWorkspaceView final : public QWidget
{
  public:
    explicit StoryWorkspaceView(QWidget* Parent = nullptr);
    ~StoryWorkspaceView() override;
    void SetModel(StoryGraphModel Model);
    const StoryGraphModel& GetModel() const
    {
        return Model;
    }
    void ClearSelection();
    void SelectNode(const QString& Identity);
    void SelectEvent(const QString& Node, const QString& Binding);
    bool DeleteSelection();
    void CancelInteraction();
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
    std::function<void(QString, int, QString, QString)> AddEventRequested;
    std::function<void(QString, QString)> EventSelected, RemoveEventRequested;
    std::function<void(QString, QString, bool)> EventModeRequested;
    std::function<void(QString, int)> ImportEventRequested;
    std::function<void(QString, QString, int, QString, QString)> MoveEventRequested;

  protected:
    void resizeEvent(QResizeEvent* Event) override;

  private:
    void ShowAddMenu(QPointF Position, QPoint ScreenPosition);
    QPointer<StudioActionMenu> ActionMenu;
    StoryGraphModel Model;
    std::optional<StoryGraphModel> PendingModel;
    QGraphicsView* Canvas = nullptr;
    StoryGraphicsScene* Graphics = nullptr;
    QLabel* Summary = nullptr;
    QPushButton* AddButton = nullptr;
    QPushButton* UndoButton = nullptr;
    QPushButton* RedoButton = nullptr;
    QPushButton* SaveButton = nullptr;
    QPushButton* OpenButton = nullptr;
    QWidget* VariablesPanel = nullptr;
    QScrollArea* ToolbarScroll = nullptr;
};
} // namespace EditorUi
