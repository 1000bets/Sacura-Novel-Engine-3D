#pragma once

#include <QMainWindow>
#include <QString>

class Engine;
class ProjectSession;
class ReflectionInspector;
class QComboBox;
class QLabel;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTabWidget;
class QTreeWidget;
class QListWidget;

class EditorMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    EditorMainWindow(Engine& InEngine, ProjectSession& InSession, QWidget* Parent = nullptr);

    void RefreshProjectTitle();
    bool CaptureScreenshot(const QString& OutputFile);

private slots:
    void OnPlayToggled(bool bChecked);
    void OnPauseClicked();
    void OnStepClicked();
    void OnViewportModeChanged(int Index);
    void OnDockTabChanged(int Index);
    void OnHierarchyTabChanged(int Index);
    void OnLayoutModeChanged(int Index);
    void OnOpenProject();
    void OnSaveProject();
    void OnFocusActionsAndEvents();

private:
    void BuildUi();
    void BuildMenus();
    void BuildToolbar();
    void PopulateHierarchy();
    void PopulateStoryTree();
    void PopulateDockPages();
    QWidget* MakePlaceholderPage(const QString& Title, const QString& Description);
    void UpdateStatus();

    Engine& BoundEngine;
    ProjectSession& BoundSession;

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
    QLabel* ViewportPlaceholder = nullptr;
    QTabWidget* DockTabs = nullptr;
    QTabWidget* InspectorTabs = nullptr;
    ReflectionInspector* Inspector = nullptr;

    bool bPlaying = false;
    bool bPaused = false;
};
