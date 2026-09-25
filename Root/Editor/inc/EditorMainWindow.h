#pragma once

#include "Rendering/RenderSettings.h"

#include "EditorCommandStack.h"
#include "EditorViewportWidget.h"
#include "Gameplay/ObjectHandle.h"
#include "SceneDocument.h"
#include "Story/StoryRuntime.h"

#include <QMainWindow>
#include <QString>

#include <filesystem>

class Engine;
class StoryWidget;
class GameObject;
class ProjectSession;
class ReflectionInspector;
class ContentBrowserWidget;
class QComboBox;
class QDoubleSpinBox;
class QElapsedTimer;
class QLabel;
class QLineEdit;
class QPushButton;
class QSplitter;
class QTabWidget;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class QListWidget;
class QCloseEvent;

class EditorMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    EditorMainWindow(Engine& InEngine, ProjectSession& InSession, QWidget* Parent = nullptr);
    ~EditorMainWindow() override;

    void RefreshProjectTitle();
    bool CaptureScreenshot(const QString& OutputFile);

protected:
    void closeEvent(QCloseEvent* Event) override;

private slots:
    void OnPlayToggled(bool bChecked);
    void OnPauseClicked();
    void OnStepClicked();
    void OnViewportModeChanged(int Index);
    void OnDockTabChanged(int Index);
    void OnHierarchyTabChanged(int Index);
    void OnLayoutModeChanged(int Index);
    void OnOpenProject();
    void OnNewProject();
    void OnSaveScene();
    void OnFocusActionsAndEvents();
    void OnMaintenanceTick();
    void OnViewportSurfaceChanged(RenderViewportWidget* Viewport);
    void OnViewportResized(RenderViewportWidget* Viewport);
    void OnHierarchySelectionChanged();
    void OnHierarchyContextMenu(const QPoint& Position);
    void OnUndo();
    void OnRedo();
    void OnCreateObject();
    void OnDeleteSelectedObject();
    void OnAddCameraComponent();
    void OnAddLightComponent();
    void OnObjectNameEdited();
    void OnTransformEdited();
    void OnInspectedComponentChanged(int Index);
    void OnAssetDropped(QString AssetId, QString SubAssetIdentifier, int AssetTypeValue, QString VirtualPath, QPoint Position);

private:
    void BuildUi();
    void BindDocumentFromSession();
    void BuildMenus();
    void BuildToolbar();
    void PopulateHierarchy();
    void PopulateStoryTree();
    void PopulateDockPages();
    void BuildStoryDockPage();
    void RefreshStoryPlaybackUi();
    void StartStoryPlayback();
    void StopStoryPlayback();
    void RefreshSelectionUi();
    void ClearSelection();
    void SelectObject(ObjectHandle Target);
    ObjectHandle GetSelectedObjectHandle() const;
    GameObject* GetSelectedGameObject() const;
    void AddHierarchyItem(QTreeWidgetItem* ParentItem, GameObject* ObjectInstance);
    QWidget* MakePlaceholderPage(const QString& Title, const QString& Description);
    void UpdateStatus();
    void UpdateWindowTitleDirty();
    bool PromptSaveIfDirty();
    bool LaunchEditorProcessForProject(const std::filesystem::path& ProjectFile);
    void EnsurePresenting(RenderViewportWidget* Viewport);
    void ConfigureRenderView();
    bool ExecuteCommand(std::unique_ptr<EditorCommand> Command);
    Scene* GetEditScene() const;

    Engine& BoundEngine;
    ProjectSession& BoundSession;
    SceneDocument Document;
    EditorCommandStack CommandStack;
    ObjectHandle SelectedObject;
    ObjectHandle InspectedComponent;

    QLabel* ProjectTitleLabel = nullptr;
    QLabel* ModeLabel = nullptr;
    QLabel* StatusSelectionLabel = nullptr;
    QLabel* StatusIssuesLabel = nullptr;
    QComboBox* SubsceneCombo = nullptr;
    QComboBox* LayoutCombo = nullptr;
    QPushButton* PlayButton = nullptr;
    QPushButton* PauseButton = nullptr;
    QPushButton* StepButton = nullptr;

    QSplitter* WorkspaceSplitter = nullptr;
    QSplitter* CenterSplitter = nullptr;
    QTabWidget* HierarchyTabs = nullptr;
    QTreeWidget* HierarchyTree = nullptr;
    QTreeWidget* StoryTree = nullptr;
    QListWidget* SubsceneList = nullptr;
    QTabWidget* ViewportTabs = nullptr;
    EditorViewportWidget* PrimaryViewport = nullptr;
    EditorViewportWidget* GameViewport = nullptr;
    QLabel* RenderStatisticsLabel = nullptr;
    RenderSettings RenderConfiguration;
    QTabWidget* DockTabs = nullptr;
    QWidget* StoryDockPage = nullptr;
    StoryWidget* StoryPanel = nullptr;
    StoryWidget* GameDialogue = nullptr;
    QLabel* StoryOverlayLabel = nullptr;
    StoryRuntime& StoryPlayback;
    QTabWidget* InspectorTabs = nullptr;
    QLineEdit* ObjectNameEdit = nullptr;
    QDoubleSpinBox* PositionXSpin = nullptr;
    QDoubleSpinBox* PositionYSpin = nullptr;
    QDoubleSpinBox* PositionZSpin = nullptr;
    QDoubleSpinBox* ScaleXSpin = nullptr;
    QDoubleSpinBox* ScaleYSpin = nullptr;
    QDoubleSpinBox* ScaleZSpin = nullptr;
    QComboBox* ComponentCombo = nullptr;
    ReflectionInspector* Inspector = nullptr;
    ContentBrowserWidget* ContentBrowser = nullptr;

    QTimer* MaintenanceTimer = nullptr;
    QElapsedTimer* MaintenanceClock = nullptr;

    bool bPlaying = false;
    bool bPaused = false;
    bool bUpdatingSelectionUi = false;
};
