#pragma once
#include "UI/Presentation/StoryGraphModel.h"
#include <QWidget>
#include <QMap>
#include <functional>

class AssetRegistry;
class AssetReferenceField;
class QLabel;
class QLineEdit;
class QTextEdit;
class QComboBox;
class QCheckBox;
class QTimer;
class QVBoxLayout;
class QScrollArea;

namespace EditorUi
{
class StoryEventInspectorView;
class StoryInspectorView final : public QWidget
{
  public:
    explicit StoryInspectorView(QWidget* Parent = nullptr);
    void SetNode(const StoryGraphNode& Node, const StoryGraphModel& Model, bool bForce = false);
    void SetAssetRegistry(const AssetRegistry* Value);
    void Commit();
    void Clear();
    void SetEvent(const StoryGraphNode& Node, const StoryGraphEventBatch& Batch, const StoryGraphEventBinding& Binding,
                  const StoryGraphEvent& Event, const StoryGraphModel& Model, bool bForce = false);
    StoryEventInspectorView& EventInspector() { return *EventView; }
    std::function<void(QString, int, QString)> AddEventRequested;
    std::function<void(QString, QString)> EventSelected;
    std::function<void()> ImportEventRequested;
    std::function<void(StoryGraphNode)> NodeChanged;
    std::function<void(QString)> StartRequested;
    std::function<void()> UndoRequested;
    std::function<void()> RedoRequested;
    std::function<void()> SaveRequested;

  protected:
    bool eventFilter(QObject* Watched, QEvent* Event) override;
    void resizeEvent(QResizeEvent* Event) override;

  private:
    void RebuildDetails();
    void SetPendingStructure();
    QWidget* MakeSection(const QString& Key, const QString& Caption, bool bOpen = true);
    QComboBox* MakeTargetPicker(QWidget* Parent, const QString& Target);
    const AssetRegistry* Registry = nullptr;
    StoryGraphNode Current;
    StoryGraphModel Context;
    bool bUpdating = false;
    bool bStructurePending = false;
    bool bCollecting = false;
    QLabel* Title = nullptr;
    QLabel* ContextTitle = nullptr;
    QWidget* Options = nullptr;
    QMap<QString, bool> SectionStates;
    QComboBox* Kind = nullptr;
    QWidget* KindCaption = nullptr;
    QComboBox* Speaker = nullptr;
    AssetReferenceField* SpeakerAsset = nullptr;
    QWidget* SpeakerCaption = nullptr;
    QTextEdit* Text = nullptr;
    QWidget* Details = nullptr;
    QVBoxLayout* DetailsLayout = nullptr;
    QCheckBox* Start = nullptr;
    QTimer* TextTimer = nullptr;
    QScrollArea* NodeScroll = nullptr;
    QScrollArea* EventScroll = nullptr;
    StoryEventInspectorView* EventView = nullptr;
};
} // namespace EditorUi
