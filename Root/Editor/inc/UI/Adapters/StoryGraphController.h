#pragma once
#include "Documents/StoryGraphDocument.h"
#include "UI/Presentation/StoryGraphModel.h"
#include <QObject>
#include <functional>

namespace EditorUi
{
class StoryWorkspaceView;
class StoryInspectorView;

// Translates widget intents to document commands. The document owns all authored state.
class StoryGraphController final : public QObject
{
  public:
    StoryGraphController(StoryWorkspaceView& View, StoryInspectorView& Inspector);
    ~StoryGraphController() override;
    bool Open(AssetRegistry& Registry, const AssetKey& Asset, std::string& Error);
    bool Save(std::string& Error);
    void FlushEdits();
    bool Undo();
    bool Redo();
    bool DeleteSelection();
    void SetLocked(bool bValue);
    void SetObjects(QVector<QPair<QString, QString>> Objects);
    const StoryGraphDocument& GetDocument() const
    {
        return Document;
    }
    void Refresh();
    std::function<void()> Changed;
    std::function<void()> SelectionActivated;
    std::function<void()> OpenRequested;
    std::function<void(int, QPointF)> NewDocumentRequested;
    std::function<void(QString)> ErrorReported;

  private:
    void EditNode(const StoryGraphNode& Node);
    void Select(const QString& Identity);
    void Finish(bool bSuccess, const std::string& Error);
    void RefreshNow();
    void ConfigureEvents();
    void ClearEventCallbacks();
    bool ShowEventInspector(const StoryGraphNode& Node, const StoryGraphModel& Model);
    void ExportSelectedEvent();
    void ImportEvent(int Phase = -1);
    StoryGraphDocument Document;
    StoryWorkspaceView& View;
    StoryInspectorView& Inspector;
    QVector<QPair<QString, QString>> Objects;
    QString Selected;
    QString SelectedBinding;
    bool bLocked = false;
    bool bRefreshPending = false;
    bool bForceInspector = false;
    bool bSelectRequested = false;
};
} // namespace EditorUi
