#pragma once
#include "UI/Presentation/StoryGraphModel.h"
#include <QWidget>
#include <QMap>
#include <functional>
class QVBoxLayout;
class QLineEdit;
class QTextEdit;
namespace EditorUi
{
class StoryActionFieldsView;
class StoryEventInspectorView final : public QWidget
{
  public:
    explicit StoryEventInspectorView(QWidget* Parent = nullptr);
    void SetEvent(const StoryGraphNode& Node, const StoryGraphEventBatch& Batch, const StoryGraphEventBinding& Binding,
                  const StoryGraphEvent& Event, const StoryGraphModel& Model, bool bForce);
    void Commit();
    bool IsEditingSource() const
    {
        return bEditingSource;
    }
    std::function<void(StoryGraphEvent)> EventChanged;
    std::function<void(StoryGraphAction)> ActionChanged;
    std::function<void(int, int)> PlacementChanged;
    std::function<void()> RemoveRequested, ResetRequested, BackRequested, UndoRequested, RedoRequested, SaveRequested;
    std::function<void(int)> MoveRequested;
    std::function<void()> ExportRequested, ImportRequested;

  protected:
    bool eventFilter(QObject* Watched, QEvent* Event) override;

  private:
    void Rebuild();
    void RebuildLater();
    QWidget* Section(const QString& Key, const QString& Caption, bool bOpen = true);
    void PublishStructure();
    StoryGraphNode Node;
    StoryGraphEventBatch Batch;
    StoryGraphEventBinding Binding;
    StoryGraphEvent Source, Effective;
    StoryGraphModel Context;
    QString SelectedAction;
    QMap<QString, bool> Sections;
    QVBoxLayout* Layout = nullptr;
    QLineEdit* Name = nullptr;
    QTextEdit* Description = nullptr;
    StoryActionFieldsView* ActionFields = nullptr;
    bool bPending = false, bUpdating = false, bCollecting = false;
    bool bEditingSource = false;
};
} // namespace EditorUi
