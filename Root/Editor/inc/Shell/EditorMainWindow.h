class PrefabOverridesPanel;
class QVBoxLayout;
#pragma once

#include "Rendering/RenderSettings.h"

#include "Assets/AssetRegistry.h"
#include "Actions/EditorAction.h"
#include "Actions/EditorActionContext.h"
#include "Commands/EditorCommandStack.h"
#include "Viewport/EditorViewportWidget.h"
#include "Core/Object/ObjectHandle.h"
#include "Documents/SceneDocument.h"
#include "Documents/SceneWorkspace.h"
#include "Documents/CharacterDocument.h"
#include "Story/StoryRuntime.h"

#include <QMainWindow>
#include <QHash>
#include <QPointer>
#include <QString>
#include <QJsonObject>

#include <filesystem>
#include <functional>
#include <unordered_map>
#include <vector>

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
class QMenu;
class QPushButton;
class QSplitter;
class QTabWidget;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class QCloseEvent;
class QEvent;
class QButtonGroup;
class QVBoxLayout;
class QStackedWidget;
class EditorToolset;
class MaterialEditorWindow;
class StaticMeshEditorWindow;
class CharacterWorkspaceView;
class EditorMcpServer;
class QPlainTextEdit;

namespace EditorUi
{
class StoryWorkspaceView;
class AudioEnvironmentView;
class WidgetWorkspaceView;
class StoryInspectorView;
class HierarchyPanelView;
class StoryGraphController;
class WorkspaceDockController;
class StudioActionMenu;
class StudioNumberEdit;
}

class EditorMainWindow : public QMainWindow
{
    Q_OBJECT
    friend struct EditorCompatibilityProbe;

public:
    using AssetEditorHandler = std::function<void(const AssetKey&, const AssetRegistryEntry&, const QString&)>;

    EditorMainWindow(Engine& InEngine, ProjectSession& InSession, QWidget* Parent = nullptr);
    ~EditorMainWindow() override;

    void RefreshProjectTitle();
    bool CaptureScreenshot(const QString& OutputFile);
    bool RegisterAssetEditor(const AssetType& Type, AssetEditorHandler Handler);
    EditorToolset& GetToolset();
    EditorMcpServer& GetMcpServer();
    void ShowSettings(int Page = 0);

protected:
    void closeEvent(QCloseEvent* Event) override;
    bool eventFilter(QObject* Watched, QEvent* Event) override;

private slots:
    void OnPlayToggled(bool bChecked);
    void OnPauseClicked();
    void OnStepClicked();
    void OnLayoutModeChanged(int Index);
    void OnOpenProject();
    void OnNewProject();
    void OnSaveScene();
    void OnExportGame();
    void OnFocusActionsAndEvents();
    void OnMaintenanceTick();
    void OnViewportSurfaceChanged(RenderViewportWidget* Viewport);
    void OnViewportResized(RenderViewportWidget* Viewport);
    void OnViewportSelectionRequested(QPoint Position);
    void OnViewportContextMenu(QPoint Position);
    void OnHierarchySelectionChanged();
    void OnHierarchyContextMenu(const QPoint& Position);
    void OnUndo();
    void OnRedo();
    void OnCopy();
    void OnPaste();
    void OnCreateEmptyObject();
    void OnDeleteSelectedObject();
    void OnObjectNameEdited();
    void OnTransformEdited();
    void OnComponentFilterChanged(int Index);
    void OnAssetActivated(QString AssetId, QString SubAssetIdentifier, QString AssetTypeIdentifier, QString VirtualPath);
    void OnAssetDropped(QString AssetId, QString SubAssetIdentifier, QString AssetTypeIdentifier, QString VirtualPath, QPoint Position);

private:
    void OpenPrefabEditor(const AssetId& Prefab);
    void CreatePrefabFromSelection();
    void InitializePrefabPanel(QVBoxLayout* Layout);
    void RegisterPrefabTools();
    PrefabOverridesPanel* PrefabPanel = nullptr;
    uint64_t PrefabListener = 0;

    void InitializeTools();
    void BuildConsoleDockPage();
    QJsonObject ReadProjectSettings() const;
    QJsonObject ApplyProjectSettings(const QJsonObject& Settings);
    QJsonObject ReadEditorSettings() const;
    QJsonObject ApplyEditorSettings(const QJsonObject& Settings);
    std::unique_ptr<EditorToolset> Toolset;
    std::unique_ptr<EditorMcpServer> McpServer;
    QWidget* ConsolePage = nullptr;
    QPlainTextEdit* ConsoleOutput = nullptr;
    uint64_t ConsoleSequence = 0;
    void BindTransformScrubbing(EditorUi::StudioNumberEdit* Field, int Index);
    QPointer<EditorUi::StudioNumberEdit> ScrubbingTransformField;
    ObjectHandle ScrubbingTransformObject;
    void FrameSelectedObject();
    void DuplicateSelectedObject();
    void CloseSceneContextMenu();
    QPointer<EditorUi::StudioActionMenu> SceneContextMenu;
    void BuildUi();
    void BindDocumentFromSession();
    void BuildMenus();
    void BuildWindowsMenuView();
    void BuildToolbar();
    void PopulateCreateMenus(QMenu* ObjectsMenu, QMenu* ComponentsMenu);
    void PopulateHierarchy();
    void PopulateDockPages();
    void RegisterBuiltInAssetEditors();
    MaterialEditorWindow* OpenMaterialEditor(const AssetKey& Asset);
    void RegisterMaterialTools();
    StaticMeshEditorWindow* OpenStaticMeshEditor(const AssetKey& Asset);
    void RegisterStaticMeshTools();
    CharacterWorkspaceView* OpenCharacterWorkspace();
    void RegisterCharacterTools();
    bool PlaceCharacter(const AssetRegistryEntry& Entry, ObjectHandle& Created, std::string& Error);
    CharacterDocuments Characters;
    QPointer<CharacterWorkspaceView> CharacterWorkspace;
    void BuildStoryDockPage();
    void BuildAudioEnvironmentDockPage();
    void RefreshAudioEnvironment();
    void InitializeStoryAuthoring();
    void InitializeWidgetAuthoring();
    bool IsWidgetEditingContext() const;
    void RefreshStoryTargets();
    bool OpenStoryAsset(const AssetKey& Asset);
    void OpenStoryFromDialog();
    bool CreateStoryFromDialog();
    bool SaveStory();
    void SaveActiveDocument();
    bool PromptSaveStoryIfDirty();
    bool IsStoryEditingContext() const;
    void BuildRenderStatisticsDockPage();
    void BuildPostprocessDockPage();
    void RefreshStoryPlaybackUi();
    void StartStoryPlayback();
    void StopStoryPlayback();
    void RefreshSelectionUi();
    void RefreshTransformUi();
    bool ExecuteSelectedTransform(const Transform& Updated);
    void ClearSelection();
    void SelectObject(ObjectHandle Target);
    ObjectHandle GetSelectedObjectHandle() const;
    GameObject* GetSelectedGameObject() const;
    void AddHierarchyItem(QTreeWidgetItem* ParentItem, GameObject* ObjectInstance);
    bool ReparentHierarchyObject(ObjectHandle Target, ObjectHandle NewParent);
    void UpdateStatus();
    void UpdateWindowTitleDirty();
    void ConfigureComponentInspector(ReflectionInspector* ComponentInspector);
    void RebuildComponentInspectors();
    EditorActionContext MakeEditorActionContext();
    void RunEditorAction(EditorAction* Action);
    void RunEditorActionByTypeId(const char* TypeIdString);
    bool CopySelectedObjectToClipboard();
    bool PasteObjectFromClipboard();
    bool CopyFocusedTextWidget();
    bool PasteFocusedTextWidget();
    bool HandleClipboardShortcut(QEvent* Event);
    bool HandleGizmoModeHotkey(QEvent* Event);
    void SyncGizmoModeButtons();
    static bool IsTextClipboardWidget(QWidget* Candidate);
    bool PromptSaveIfDirty();
    bool LaunchEditorProcessForProject(const std::filesystem::path& ProjectFile);
    void EnsurePresenting(RenderViewportWidget* Viewport);
    void ConfigureRenderView();
    void SetViewportVisualization(RenderVisualization Mode, float DepthRange);
    bool ExecuteCommand(std::unique_ptr<EditorCommand> Command);
    Scene* GetEditScene() const;
    Scene* GetHierarchyScene() const;

    Engine& BoundEngine;
    ProjectSession& BoundSession;
    SceneWorkspace Workspace;
    SceneDocument& Document;
    EditorCommandStack& CommandStack;
    QString ComponentFilter;

    QLabel* ProjectTitleLabel = nullptr;
    QLabel* ModeLabel = nullptr;
    QLabel* StatusSelectionLabel = nullptr;
    QLabel* StatusIssuesLabel = nullptr;
    QComboBox* LayoutCombo = nullptr;
    QPushButton* PlayButton = nullptr;
    QPushButton* PauseButton = nullptr;
    QPushButton* StepButton = nullptr;

    QSplitter* WorkspaceSplitter = nullptr;
    EditorUi::WorkspaceDockController* Docking = nullptr;
    QSplitter* CenterSplitter = nullptr;
    QTabWidget* HierarchyTabs = nullptr;
    QTreeWidget* HierarchyTree = nullptr;
    EditorUi::HierarchyPanelView* HierarchyContent = nullptr;
    EditorViewportWidget* PrimaryViewport = nullptr;
    QWidget* CameraPreviewPanel = nullptr;
    RenderViewportWidget* CameraPreviewViewport = nullptr;
    QLabel* CameraPreviewUnavailableLabel = nullptr;
    bool bCameraPreviewAttached = false;
    bool bCameraPreviewUnavailable = false;
    QPushButton* LocationTab = nullptr;
    QPushButton* GameTab = nullptr;
    QDoubleSpinBox* CameraSpeedSpin = nullptr;
    QButtonGroup* GizmoModeButtons = nullptr;
    QPushButton* TranslateGizmoButton = nullptr;
    QPushButton* RotateGizmoButton = nullptr;
    QPushButton* ScaleGizmoButton = nullptr;
    QLabel* RenderStatisticsLabel = nullptr;
    QLabel* NavigationStatisticsLabel = nullptr;
    RenderSettings RenderConfiguration;
    QTabWidget* DockTabs = nullptr;
    QWidget* StoryDockPage = nullptr;
    StoryWidget* StoryPanel = nullptr;
    StoryWidget* GameDialogue = nullptr;
    StoryRuntime& StoryPlayback;
    QTabWidget* InspectorTabs = nullptr;
    QStackedWidget* InspectorPages = nullptr;
    QWidget* ObjectInspectorContent = nullptr;
    QWidget* EmptyInspectorPage = nullptr;
    EditorUi::StoryWorkspaceView* StoryGraph = nullptr;
    EditorUi::AudioEnvironmentView* AudioEnvironment = nullptr;
    EditorUi::StoryInspectorView* StoryInspector = nullptr;
    std::unique_ptr<EditorUi::StoryGraphController> StoryAuthoring;
    bool bStoryEditingContext = false;
    bool bWidgetEditingContext = false;
    EditorUi::WidgetWorkspaceView* WidgetWorkspace = nullptr;
    QJsonObject PendingWidgetPreview;
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
    QComboBox* ViewportVisualization = nullptr;
    QDoubleSpinBox* ViewportDepthRange = nullptr;
    QComboBox* ComponentCombo = nullptr;
    QVBoxLayout* ComponentInspectorLayout = nullptr;
    std::vector<ReflectionInspector*> ComponentInspectors;
    ObjectHandle InspectorSelection;
    QHash<QString, bool> ComponentExpansion;
    ContentBrowserWidget* ContentBrowser = nullptr;
    std::unordered_map<AssetType, AssetEditorHandler, AssetTypeHash> AssetEditors;

    QTimer* MaintenanceTimer = nullptr;
    QElapsedTimer* MaintenanceClock = nullptr;

    bool bPlayRequested = false;
    bool bPlaying = false;
    bool bPaused = false;
    bool bUpdatingSelectionUi = false;
};
