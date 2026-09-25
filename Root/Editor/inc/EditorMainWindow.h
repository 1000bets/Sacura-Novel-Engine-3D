#pragma once

#include "Rendering/RenderSettings.h"

#include "Assets/AssetRegistry.h"
#include "EditorCommandStack.h"
#include "EditorViewportWidget.h"
#include "Gameplay/ObjectHandle.h"
#include "SceneDocument.h"
#include "Story/StoryRuntime.h"

#include <QMainWindow>
#include <QString>

#include <filesystem>
#include <functional>
#include <unordered_map>

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
class QCloseEvent;

class EditorMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    using AssetEditorHandler = std::function<void(const AssetKey&, const AssetRegistryEntry&, const QString&)>;

    EditorMainWindow(Engine& InEngine, ProjectSession& InSession, QWidget* Parent = nullptr);
    ~EditorMainWindow() override;

    void RefreshProjectTitle();
    bool CaptureScreenshot(const QString& OutputFile);
    bool RegisterAssetEditor(const AssetType& Type, AssetEditorHandler Handler);

protected:
    void closeEvent(QCloseEvent* Event) override;

private slots:
    void OnPlayToggled(bool bChecked);
    void OnPauseClicked();
    void OnStepClicked();
    void OnDockTabChanged(int Index);
    void OnLayoutModeChanged(int Index);
    void OnOpenProject();
    void OnNewProject();
    void OnSaveScene();
    void OnFocusActionsAndEvents();
    void OnMaintenanceTick();
    void OnViewportSurfaceChanged(RenderViewportWidget* Viewport);
    void OnViewportResized(RenderViewportWidget* Viewport);
    void OnViewportSelectionRequested(QPoint Position);
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
    void OnAssetActivated(QString AssetId, QString SubAssetIdentifier, QString AssetTypeIdentifier, QString VirtualPath);
    void OnAssetDropped(QString AssetId, QString SubAssetIdentifier, QString AssetTypeIdentifier, QString VirtualPath, QPoint Position);

private:
    void BuildUi();
    void BindDocumentFromSession();
    void BuildMenus();
    void BuildToolbar();
    void PopulateHierarchy();
    void PopulateDockPages();
    void RegisterBuiltInAssetEditors();
    void BuildStoryDockPage();
    void BuildRenderStatisticsDockPage();
    void BuildPostprocessDockPage();
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
    Scene* GetHierarchyScene() const;

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
    QComboBox* LayoutCombo = nullptr;
    QPushButton* PlayButton = nullptr;
    QPushButton* PauseButton = nullptr;
    QPushButton* StepButton = nullptr;

    QSplitter* WorkspaceSplitter = nullptr;
    QSplitter* CenterSplitter = nullptr;
    QTabWidget* HierarchyTabs = nullptr;
    QTreeWidget* HierarchyTree = nullptr;
    EditorViewportWidget* PrimaryViewport = nullptr;
    QLabel* ViewportModeLabel = nullptr;
    QDoubleSpinBox* CameraSpeedSpin = nullptr;
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
    QDoubleSpinBox* RotationXSpin = nullptr;
    QDoubleSpinBox* RotationYSpin = nullptr;
    QDoubleSpinBox* RotationZSpin = nullptr;
    QDoubleSpinBox* ScaleXSpin = nullptr;
    QDoubleSpinBox* ScaleYSpin = nullptr;
    QDoubleSpinBox* ScaleZSpin = nullptr;
    QComboBox* ComponentCombo = nullptr;
    ReflectionInspector* Inspector = nullptr;
    ContentBrowserWidget* ContentBrowser = nullptr;
    std::unordered_map<AssetType, AssetEditorHandler, AssetTypeHash> AssetEditors;

    QTimer* MaintenanceTimer = nullptr;
    QElapsedTimer* MaintenanceClock = nullptr;

    bool bPlaying = false;
    bool bPaused = false;
    bool bUpdatingSelectionUi = false;
};
