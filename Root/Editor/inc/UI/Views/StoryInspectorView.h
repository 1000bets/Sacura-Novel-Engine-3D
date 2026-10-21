#pragma once
#include "UI/Presentation/StoryGraphModel.h"
#include <QWidget>
#include <functional>

class QLabel;
class QLineEdit;
class QTextEdit;
class QComboBox;
class QCheckBox;
class QTimer;
class QVBoxLayout;

namespace EditorUi
{
class StoryInspectorView final : public QWidget
{
  public:
    explicit StoryInspectorView(QWidget* Parent = nullptr);
    void SetNode(const StoryGraphNode& Node, const StoryGraphModel& Model, bool bForce = false);
    void Commit();
    void Clear();
    std::function<void(StoryGraphNode)> NodeChanged;
    std::function<void(QString)> StartRequested;
    std::function<void()> UndoRequested;
    std::function<void()> RedoRequested;
    std::function<void()> SaveRequested;

  protected:
    bool eventFilter(QObject* Watched, QEvent* Event) override;

  private:
    void RebuildDetails();
    void SetPendingStructure();
    QComboBox* MakeTargetPicker(QWidget* Parent, const QString& Target);
    StoryGraphNode Current;
    StoryGraphModel Context;
    bool bUpdating = false;
    bool bStructurePending = false;
    bool bCollecting = false;
    QLabel* Title = nullptr;
    QComboBox* Kind = nullptr;
    QWidget* KindCaption = nullptr;
    QComboBox* Speaker = nullptr;
    QWidget* SpeakerCaption = nullptr;
    QTextEdit* Text = nullptr;
    QWidget* Details = nullptr;
    QVBoxLayout* DetailsLayout = nullptr;
    QCheckBox* Start = nullptr;
    QTimer* TextTimer = nullptr;
};
} // namespace EditorUi
