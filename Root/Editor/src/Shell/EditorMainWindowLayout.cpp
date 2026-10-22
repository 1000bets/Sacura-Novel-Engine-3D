#include "UI/Views/StudioControls.h"
#include "UI/Views/AudioEnvironmentView.h"
#include "UI/Views/WidgetWorkspaceView.h"
#include "UI/Views/ObjectInspectorView.h"
#include "Shell/EditorMainWindow.h"

#include "Actions/BuiltinEditorActions.h"
#include "Actions/EditorAction.h"
#include "Shell/EditorClipboard.h"
#include "Commands/EditorCommands.h"
#include "Panels/ContentBrowserWidget.h"
#include "Shell/EditorTheme.h"
#include "UI/Adapters/WorkspaceLayoutAdapter.h"
#include "UI/Adapters/WorkspaceDockController.h"
#include "UI/Views/WorkspaceView.h"
#include "UI/Views/HierarchyPanelView.h"
#include "UI/Views/StudioHeaderView.h"
#include "UI/Views/StoryWorkspaceView.h"
#include "UI/Views/StoryInspectorView.h"
#include "UI/Adapters/StoryGraphAdapter.h"
#include "UI/Styles/StudioIcons.h"
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>
#include <QWidgetAction>
#include "Viewport/EditorViewportWidget.h"
#include "Engine.h"
#include "World/Simulation/PlaySession.h"
#include "World/Scene.h"
#include "World/Serialization/SceneSerializer.h"
#include "World/Components/CameraComponent.h"
#include "World/Components/Component.h"
#include "World/GameObject.h"
#include "World/Components/LightComponent.h"
#include "World/Components/MeshRendererComponent.h"
#include "Project/ProjectSession.h"
#include "Launcher/ProjectBrowserDialog.h"
#include "Panels/ReflectionInspector.h"
#include "Rendering/Frame/RenderFrameData.h"
#include "Rendering/Threading/RenderThread.h"
#include "Core/Threading/ThreadContext.h"
#include "Project/ProjectPaths.h"
#include "Rendering/SceneExtractor.h"
#include "Story/StoryRuntime.h"
#include "Navigation/NavigationWorld.h"
#include "UI/StoryWidget.h"
#include "UI/WidgetLayout.h"

#include <QAbstractItemView>
#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QLayout>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QGuiApplication>
#include <QScreen>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStringList>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <QWidget>

#include "Reflection/Class.h"
#include "Reflection/ReflectionSubsystem.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>

#include "Shell/EditorMainWindowSupport.h"
#include "Panels/HierarchyTreeWidget.h"

void EditorMainWindow::BuildMenus()
{
    QMenu* FileMenu = menuBar()->addMenu(QString::fromUtf8("\xD0\xA4\xD0\xB0\xD0\xB9\xD0\xBB"));
    FileMenu->addAction(QString::fromUtf8("\xD0\x9D\xD0\xBE\xD0\xB2\xD1\x8B\xD0\xB9 \xD0\xBF\xD1\x80\xD0\xBE\xD0\xB5\xD0\xBA\xD1\x82..."), this, &EditorMainWindow::OnNewProject);
    FileMenu->addAction(QString::fromUtf8("\xD0\x9E\xD1\x82\xD0\xBA\xD1\x80\xD1\x8B\xD1\x82\xD1\x8C..."), this, &EditorMainWindow::OnOpenProject);
    FileMenu->addAction(QString::fromUtf8("\xD0\xA1\xD0\xBE\xD1\x85\xD1\x80\xD0\xB0\xD0\xBD\xD0\xB8\xD1\x82\xD1\x8C \xD1\x81\xD1\x86\xD0\xB5\xD0\xBD\xD1\x83"), this, &EditorMainWindow::OnSaveScene);
    FileMenu->addAction(tr("Сохранить"), this, &EditorMainWindow::SaveActiveDocument, QKeySequence::Save);
    FileMenu->addAction(tr("Сохранить историю"), this, [this]() { SaveStory(); });
    FileMenu->addSeparator();
    FileMenu->addAction(QString::fromUtf8("\xD0\x92\xD1\x8B\xD1\x85\xD0\xBE\xD0\xB4"), this, &QWidget::close);

    QMenu* EditMenu = menuBar()->addMenu(QString::fromUtf8("\xD0\x9F\xD1\x80\xD0\xB0\xD0\xB2\xD0\xBA\xD0\xB0"));
    EditMenu->addAction(QString::fromUtf8("Undo"), this, &EditorMainWindow::OnUndo, QKeySequence::Undo);
    EditMenu->addAction(QString::fromUtf8("Redo"), this, &EditorMainWindow::OnRedo, QKeySequence::Redo);
    EditMenu->addSeparator();
    EditMenu->addAction(QString::fromUtf8("Copy"), this, &EditorMainWindow::OnCopy, QKeySequence::Copy);
    EditMenu->addAction(QString::fromUtf8("Paste"), this, &EditorMainWindow::OnPaste, QKeySequence::Paste);
    EditMenu->addSeparator();
    EditMenu->addAction(QString::fromUtf8("Create Object"), this, &EditorMainWindow::OnCreateEmptyObject);
    EditMenu->addAction(QString::fromUtf8("Delete Object"), this, &EditorMainWindow::OnDeleteSelectedObject, QKeySequence::Delete);

    QMenu* CreateMenu = menuBar()->addMenu(QString::fromUtf8("\xD0\xA1\xD0\xBE\xD0\xB7\xD0\xB4\xD0\xB0\xD1\x82\xD1\x8C"));
    QMenu* CreateObjectsMenu = CreateMenu->addMenu(QString::fromUtf8("\xD0\x9E\xD0\xB1\xD1\x8A\xD0\xB5\xD0\xBA\xD1\x82\xD1\x8B"));
    QMenu* CreateComponentsMenu = CreateMenu->addMenu(QString::fromUtf8("\xD0\x9A\xD0\xBE\xD0\xBC\xD0\xBF\xD0\xBE\xD0\xBD\xD0\xB5\xD0\xBD\xD1\x82\xD1\x8B"));
    PopulateCreateMenus(CreateObjectsMenu, CreateComponentsMenu);
    CreateMenu->addAction(tr("Историю…"), this, [this]() { CreateStoryFromDialog(); });
    QMenu* NavigationMenu = CreateMenu->addMenu(tr("Навигация"));
    QAction* BuildNavigation = NavigationMenu->addAction(QString::fromUtf8("Перестроить навигацию"), this, [this]()
    {
        Scene* World = GetEditScene();
        if (World != nullptr && !BoundEngine.GetPlaySession().IsSimulating())
        {
            World->GetNavigation().RequestBuild();
            Document.MarkDirty();
            statusBar()->showMessage(QString::fromUtf8("Запущено построение навигации"), 3000);
        }
    });
    BuildNavigation->setObjectName("BuildNavigationAction");
    QAction* ShowNavigation = NavigationMenu->addAction(QString::fromUtf8("Показать NavMesh (P)"));
    ShowNavigation->setObjectName("ShowNavigationAction");
    ShowNavigation->setCheckable(true);
    connect(ShowNavigation, &QAction::toggled, this, [this](bool bVisible)
    {
        if (PrimaryViewport != nullptr)
        {
            PrimaryViewport->SetNavigationVisible(bVisible);
        }
    });
    QAction* ShowCollision = NavigationMenu->addAction(QString::fromUtf8("Показать коллизии"));
    ShowCollision->setObjectName("ShowCollisionAction");
    ShowCollision->setCheckable(true);
    connect(ShowCollision, &QAction::toggled, this, [this](bool bVisible)
    {
        if (PrimaryViewport != nullptr)
        {
            PrimaryViewport->SetCollisionVisible(bVisible);
        }
    });
    CreateMenu->addSeparator();
    CreateMenu->addAction(QString::fromUtf8("\xD0\x94\xD0\xB5\xD0\xB9\xD1\x81\xD1\x82\xD0\xB2\xD0\xB8\xD1\x8F \xD0\xB8 \xD1\x81\xD0\xBE\xD0\xB1\xD1\x8B\xD1\x82\xD0\xB8\xD1\x8F"), this, &EditorMainWindow::OnFocusActionsAndEvents);

    QMenu* WindowMenu = menuBar()->addMenu(QString::fromUtf8("\xD0\x9E\xD0\xBA\xD0\xBD\xD0\xBE"));
    WindowMenu->setObjectName("WindowsMenu");
    WindowMenu->setTitle(tr("Окна"));
    auto* RestoreLayout = WindowMenu->addAction(tr("Восстановить раскладку"), this, [this]()
    {
        if (LayoutCombo != nullptr)
        {
            const QSignalBlocker Blocker(LayoutCombo);
            LayoutCombo->setCurrentIndex(0);
        }
        if (WorkspaceSplitter != nullptr && CenterSplitter != nullptr)
        {
            for (auto* Button : findChildren<QPushButton*>())
            {
                if (Button->property("workspaceExpand").toBool())
                {
                    const QSignalBlocker Blocker(Button);
                    Button->setChecked(false);
                }
            }
            Docking->Reset();
            EditorUi::WorkspaceLayoutAdapter::Reset(*WorkspaceSplitter, *CenterSplitter, height());
            Docking->Save();
        }
    });
    RestoreLayout->setObjectName("RestoreWorkspaceLayout");

    ProjectTitleLabel = new QLabel(this);
}

void EditorMainWindow::BuildToolbar()
{
    auto* Header = new EditorUi::StudioHeaderView(*menuBar(), *ProjectTitleLabel, this);
    setMenuWidget(Header);
    PlayButton = Header->Play;
    PauseButton = Header->Pause;
    StepButton = Header->Step;
    ModeLabel = Header->Mode;
    LayoutCombo = new QComboBox(Header);
    LayoutCombo->hide();
    LayoutCombo->addItem(tr("Постановка"), "balanced");
    LayoutCombo->addItem(tr("Сценарий"), "graph");
    LayoutCombo->addItem(tr("Только сцена"), "scene");
    LayoutCombo->addItem(tr("Иерархия"), "hierarchy");
    LayoutCombo->addItem(tr("Инспектор"), "inspector");
    auto* LayoutMenu = findChild<QMenu*>("WindowsMenu")->addMenu(tr("Раскладка"));
    for (int Index = 0; Index < LayoutCombo->count(); ++Index)
    {
        LayoutMenu->addAction(LayoutCombo->itemText(Index), this, [this, Index]()
        {
            LayoutCombo->setCurrentIndex(Index);
            OnLayoutModeChanged(Index);
        });
    }
    connect(PlayButton, &QPushButton::toggled, this, &EditorMainWindow::OnPlayToggled);
    connect(PauseButton, &QPushButton::clicked, this, &EditorMainWindow::OnPauseClicked);
    connect(StepButton, &QPushButton::clicked, this, &EditorMainWindow::OnStepClicked);
    connect(Header->Home, &QPushButton::clicked, this, &EditorMainWindow::OnNewProject);
    connect(LayoutCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &EditorMainWindow::OnLayoutModeChanged);
}

void EditorMainWindow::BuildUi()
{
    auto* Root = new EditorUi::WorkspaceView(this);
    Docking = new EditorUi::WorkspaceDockController(*this, qEnvironmentVariable("SAKURA_EDITOR_LAYOUT_FILE"));
    WorkspaceSplitter = &Root->GetWorkspaceSplitter();
    CenterSplitter = &Root->GetCenterSplitter();
    Docking->SetSplitters(*WorkspaceSplitter, *CenterSplitter);
    QWidget* HierarchyPanel = &Root->GetHierarchyPanel();
    QVBoxLayout* HierarchyLayout = &Root->GetHierarchyLayout();

    HierarchyTabs = new QTabWidget(HierarchyPanel);
    Docking->AddArea("hierarchy", tr("Иерархия"), *HierarchyTabs, *HierarchyPanel);
    auto* Tree = new HierarchyTreeWidget();
    Tree->setObjectName("SceneHierarchy");
    HierarchyContent = new EditorUi::HierarchyPanelView(*Tree, HierarchyTabs);
    HierarchyTree = Tree;
    HierarchyTree->setHeaderHidden(true);
    HierarchyTree->setContextMenuPolicy(Qt::CustomContextMenu);
    HierarchyTree->setDragEnabled(true);
    HierarchyTree->setAcceptDrops(true);
    HierarchyTree->setDropIndicatorShown(true);
    HierarchyTree->setDefaultDropAction(Qt::CopyAction);
    HierarchyTree->setDragDropMode(QAbstractItemView::DragDrop);
    HierarchyTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    HierarchyTree->installEventFilter(this);
    Tree->ReparentRequested = [this](ObjectHandle Target, ObjectHandle NewParent)
    {
        return ReparentHierarchyObject(Target, NewParent);
    };
    HierarchyTabs->addTab(HierarchyContent, tr("Иерархия"));
    HierarchyLayout->addWidget(HierarchyTabs, 1);
    connect(&HierarchyContent->GetAddButton(), &QPushButton::clicked, this, &EditorMainWindow::OnCreateEmptyObject);

    QWidget* ViewportPanel = &Root->GetViewportPanel();
    auto* SceneTabs = new QTabWidget(ViewportPanel);
    SceneTabs->setObjectName("SceneWorkspaceTabs");
    SceneTabs->setTabBarAutoHide(true);
    auto* ScenePage = new QWidget(SceneTabs);
    auto* ViewportLayout = new QVBoxLayout(ScenePage);
    ViewportLayout->setContentsMargins(0, 0, 0, 0);
    ViewportLayout->setSpacing(0);
    SceneTabs->addTab(ScenePage, tr("Локация и игра"));
    Root->GetViewportLayout().addWidget(SceneTabs);
    Docking->AddArea("scene", tr("Локация и игра"), *SceneTabs, *ViewportPanel);

    PrimaryViewport = new EditorViewportWidget(ViewportPanel);
    connect(PrimaryViewport, &EditorViewportWidget::NavigationVisibilityChanged, this, [this](bool bVisible)
    {
        if (QAction* Action = findChild<QAction*>("ShowNavigationAction"))
        {
            Action->setChecked(bVisible);
        }
    });
    PrimaryViewport->SetViewId(RenderViewId{1});
    PrimaryViewport->SetEngine(&BoundEngine);
    PrimaryViewport->SetEditorToolsEnabled(true);
    PrimaryViewport->installEventFilter(this);
    connect(PrimaryViewport, &EditorViewportWidget::NativeSurfaceChanged, this, &EditorMainWindow::OnViewportSurfaceChanged);
    connect(PrimaryViewport, &EditorViewportWidget::ViewportResized, this, &EditorMainWindow::OnViewportResized);
    connect(PrimaryViewport, &EditorViewportWidget::SceneContextMenuRequested, this, &EditorMainWindow::OnViewportContextMenu);
    connect(PrimaryViewport, &EditorViewportWidget::EditorToolsDisabled, this, &EditorMainWindow::CloseSceneContextMenu);
    connect(PrimaryViewport, &EditorViewportWidget::ObjectSelectionRequested, this, &EditorMainWindow::OnViewportSelectionRequested);
    connect(PrimaryViewport, &EditorViewportWidget::AssetDropped, this, &EditorMainWindow::OnAssetDropped);
    connect(PrimaryViewport, &EditorViewportWidget::CameraChanged, this, &EditorMainWindow::ConfigureRenderView);
    PrimaryViewport->SetTransformCallbacks(
        [this](const Transform& Value)
        {
            if (GameObject* Selected = GetSelectedGameObject())
            {
                Selected->SetTransform(Value);
                RefreshTransformUi();
            }
        },
        [this](const Transform& OldValue, const Transform& NewValue)
        {
            GameObject* Selected = GetSelectedGameObject();
            Scene* EditScene = GetEditScene();
            if (Selected == nullptr || EditScene == nullptr)
            {
                return;
            }
            Selected->SetTransform(OldValue);
            ExecuteSelectedTransform(NewValue);
        });

    auto* ViewportTabs = new QWidget(ViewportPanel);
    ViewportTabs->setObjectName("ViewportTabs");
    auto* TabsLayout = new QHBoxLayout(ViewportTabs);
    TabsLayout->setContentsMargins(8, 0, 8, 0);
    TabsLayout->setSpacing(22);
    LocationTab = new QPushButton(tr("Локация"), ViewportTabs);
    LocationTab->setObjectName("ViewportLocationTab");
    LocationTab->setProperty("uiRole", "viewportTab");
    LocationTab->setCheckable(true);
    LocationTab->setChecked(true);
    LocationTab->setAutoExclusive(true);
    connect(LocationTab, &QPushButton::clicked, this, [this]() { PlayButton->setChecked(false); });
    TabsLayout->addWidget(LocationTab);
    GameTab = new QPushButton(tr("Игра"), ViewportTabs);
    GameTab->setObjectName("ViewportGameTab");
    GameTab->setProperty("uiRole", "viewportTab");
    GameTab->setCheckable(true);
    GameTab->setAutoExclusive(true);
    GameTab->setToolTip(tr("Запустить игровую сцену"));
    connect(GameTab, &QPushButton::clicked, this, [this]() { PlayButton->setChecked(true); });
    TabsLayout->addWidget(GameTab);
    TabsLayout->addStretch();
    auto* MaximizeViewport = new QPushButton(EditorUi::MakeStudioIcon(EditorUi::StudioIcon::Expand), {}, ViewportTabs);
    MaximizeViewport->setProperty("uiRole", "chromeButton");
    MaximizeViewport->setProperty("workspaceExpand", true);
    MaximizeViewport->setToolTip(tr("Развернуть / восстановить локацию"));
    MaximizeViewport->setCheckable(true);
    connect(MaximizeViewport, &QPushButton::toggled, this, [this](bool bMaximized)
    {
        if (bMaximized)
        {
            const bool bToolsFirst = CenterSplitter->widget(0)->objectName() == "GraphDock";
            CenterSplitter->setSizes(bToolsFirst ? QList<int>{0, height()} : QList<int>{height(), 0});
        }
        else
        {
            EditorUi::WorkspaceLayoutAdapter::Reset(*WorkspaceSplitter, *CenterSplitter, height());
        }
    });
    TabsLayout->addWidget(MaximizeViewport);
    ViewportLayout->addWidget(ViewportTabs);
    auto* ViewportHeader = new QFrame(ViewportPanel);
    ViewportHeader->setObjectName("ViewportHeader");
    auto* ViewportHeaderLayout = new QHBoxLayout(ViewportHeader);
    ViewportHeaderLayout->setContentsMargins(4, 3, 8, 3);
    ViewportHeaderLayout->setSpacing(2);
    GizmoModeButtons = new QButtonGroup(ViewportHeader);
    GizmoModeButtons->setExclusive(true);
    auto MakeGizmoModeButton = [&](EditorUi::StudioIcon Icon, const char* Name, const QString& ToolTip,
                                    EditorViewportWidget::GizmoOperation Operation)
    {
        auto* Button = new QPushButton(EditorUi::MakeStudioIcon(Icon, "--text-secondary", "--text-primary"), {}, ViewportHeader);
        Button->setObjectName(Name);
        Button->setProperty("uiRole", "viewportTool");
        Button->setCheckable(true);
        Button->setFixedSize(28, 26);
        Button->setIconSize(QSize(14, 14));
        Button->setToolTip(ToolTip);
        Button->setAccessibleName(ToolTip);
        GizmoModeButtons->addButton(Button, static_cast<int>(Operation));
        ViewportHeaderLayout->addWidget(Button);
        return Button;
    };
    MakeGizmoModeButton(EditorUi::StudioIcon::Select, "ViewportSelect", tr("Выбор · Q"),
        EditorViewportWidget::GizmoOperation::Select);
    TranslateGizmoButton = MakeGizmoModeButton(EditorUi::StudioIcon::Move, "ViewportTranslate", tr("Сдвиг · W"),
        EditorViewportWidget::GizmoOperation::Translate);
    RotateGizmoButton = MakeGizmoModeButton(EditorUi::StudioIcon::Rotate, "ViewportRotate", tr("Поворот · E"),
        EditorViewportWidget::GizmoOperation::Rotate);
    ScaleGizmoButton = MakeGizmoModeButton(EditorUi::StudioIcon::Scale, "ViewportScale", tr("Масштаб · R"),
        EditorViewportWidget::GizmoOperation::Scale);
    SyncGizmoModeButtons();
    connect(GizmoModeButtons, &QButtonGroup::idClicked, this, [this](int ModeId)
    {
        if (PrimaryViewport != nullptr)
        {
            PrimaryViewport->SetGizmoOperation(
                static_cast<EditorViewportWidget::GizmoOperation>(ModeId));
        }
    });
    connect(PrimaryViewport, &EditorViewportWidget::GizmoOperationChanged, this, [this](EditorViewportWidget::GizmoOperation)
    {
        SyncGizmoModeButtons();
    });

    auto MakeToggle = [&](EditorUi::StudioIcon Icon, const char* Name, const QString& Tip, bool Checked)
    {
        auto* Button = new QPushButton(EditorUi::MakeStudioIcon(Icon, "--text-secondary"), {}, ViewportHeader);
        Button->setObjectName(Name);
        Button->setProperty("uiRole", "viewportToggle");
        Button->setFixedSize(28, 26);
        Button->setIconSize(QSize(14, 14));
        Button->setToolTip(Tip);
        Button->setAccessibleName(Tip);
        Button->setCheckable(true);
        Button->setChecked(Checked);
        ViewportHeaderLayout->addWidget(Button);
        return Button;
    };
    auto* Snap = MakeToggle(EditorUi::StudioIcon::Magnet, "ViewportSnap",
        tr("Привязка: 0,25 м / 15° / 0,1×"), PrimaryViewport->IsSnapEnabled());
    connect(Snap, &QPushButton::toggled, PrimaryViewport, &EditorViewportWidget::SetSnapEnabled);
    auto* Grid = MakeToggle(EditorUi::StudioIcon::Grid, "ViewportGrid", tr("Сетка сцены"), PrimaryViewport->IsGridVisible());
    connect(Grid, &QPushButton::toggled, PrimaryViewport, &EditorViewportWidget::SetGridVisible);
    connect(PrimaryViewport, &EditorViewportWidget::GridVisibilityChanged, Grid, &QPushButton::setChecked);
    ViewportHeaderLayout->addSpacing(6);
    auto* Help = new QToolButton(ViewportHeader);
    Help->setObjectName("ViewportHelp");
    Help->setProperty("uiRole", "viewportUtility");
    Help->setIcon(EditorUi::MakeStudioIcon(EditorUi::StudioIcon::Help));
    Help->setIconSize(QSize(15, 15));
    Help->setFixedSize(26, 26);
    Help->setToolTip(tr("Управление сценой"));
    Help->setAccessibleName(Help->toolTip());
    Help->setPopupMode(QToolButton::InstantPopup);
    auto* HelpMenu = new QMenu(Help);
    auto* HelpAction = new QWidgetAction(HelpMenu);
    auto* HelpText = new QLabel(tr("W — перемещение · E — вращение\n"
        "R — масштаб · Q — выбор\n"
        "Работают и в русской раскладке\n"
        "Alt + ЛКМ — орбита · СКМ — панорама\n"
        "ПКМ + WASD / QE — полёт\n"
        "Колесо — приближение\n"
        "Delete — удалить выделенный объект\n"
        "Ctrl+Z — отменить изменение"), HelpMenu);
    HelpText->setObjectName("ViewportHelpText");
    HelpText->setMargin(12);
    HelpAction->setDefaultWidget(HelpText);
    HelpMenu->addAction(HelpAction);
    Help->setMenu(HelpMenu);
    ViewportHeaderLayout->addWidget(Help);
    ViewportHeaderLayout->addStretch();
    CameraSpeedSpin = new QDoubleSpinBox(ViewportHeader);
    CameraSpeedSpin->setRange(0.25, 250.0);
    CameraSpeedSpin->setDecimals(2);
    CameraSpeedSpin->setSingleStep(0.5);
    CameraSpeedSpin->setValue(PrimaryViewport->GetCameraMoveSpeed());
    CameraSpeedSpin->setSuffix(tr(" u/s"));
    CameraSpeedSpin->setFixedWidth(88);
    CameraSpeedSpin->setToolTip(tr("Editor camera fly speed. Shift multiplies, Ctrl divides, RMB+wheel adjusts."));
    auto* CameraOptions = new QToolButton(ViewportHeader);
    CameraOptions->setProperty("uiRole", "viewportUtility");
    CameraOptions->setFixedSize(26, 26);
    CameraOptions->setIconSize(QSize(14, 14));
    CameraOptions->setIcon(EditorUi::MakeStudioIcon(EditorUi::StudioIcon::Camera));
    CameraOptions->setToolTip(tr("Настройки камеры и отображения"));
    CameraOptions->setPopupMode(QToolButton::InstantPopup);
    auto* Options = new QMenu(CameraOptions);
    auto* SpeedAction = new QWidgetAction(Options);
    auto* CameraSettings = new QWidget(Options);
    auto* CameraLayout = new QHBoxLayout(CameraSettings);
    CameraLayout->addWidget(new QLabel(tr("Скорость камеры"), CameraSettings));
    CameraLayout->addWidget(CameraSpeedSpin);
    SpeedAction->setDefaultWidget(CameraSettings);
    Options->addAction(SpeedAction);
    Options->addAction(findChild<QAction*>("ShowNavigationAction"));
    Options->addAction(findChild<QAction*>("ShowCollisionAction"));
    CameraOptions->setMenu(Options);
    ViewportHeaderLayout->addWidget(CameraOptions);
    connect(CameraSpeedSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double Value)
    {
        if (PrimaryViewport != nullptr)
        {
            PrimaryViewport->SetCameraMoveSpeed(static_cast<float>(Value));
        }
    });
    connect(PrimaryViewport, &EditorViewportWidget::CameraMoveSpeedChanged, this, [this](float Speed)
    {
        if (CameraSpeedSpin == nullptr)
        {
            return;
        }
        const QSignalBlocker Blocker(CameraSpeedSpin);
        CameraSpeedSpin->setValue(Speed);
    });
    ViewportLayout->addWidget(ViewportHeader);
    ViewportLayout->addWidget(PrimaryViewport, 1);
    GameDialogue = new StoryWidget(StoryPlayback, this);
    GameDialogue->SetWidgetLayout(WidgetLayout::Create("dialogue"));
    GameDialogue->AttachToViewport(PrimaryViewport);

    QWidget* GraphDock = &Root->GetToolsPanel();
    QVBoxLayout* GraphLayout = &Root->GetToolsLayout();
    DockTabs = new QTabWidget(GraphDock);
    Docking->AddArea("workspace", tr("Рабочая область"), *DockTabs, *GraphDock);
    GraphLayout->addWidget(DockTabs);

    QWidget* InspectorPanel = &Root->GetInspectorPanel();
    QVBoxLayout* InspectorLayout = &Root->GetInspectorLayout();
    InspectorTabs = new QTabWidget(InspectorPanel);
    Docking->AddArea("inspector", tr("Инспектор"), *InspectorTabs, *InspectorPanel);
    InspectorPages = new QStackedWidget(InspectorTabs);
    EmptyInspectorPage = new QWidget(InspectorPages);
    auto* EmptyLayout = new QVBoxLayout(EmptyInspectorPage);
    auto* EmptyHint = new QLabel(tr("Выберите объект или блок истории"), EmptyInspectorPage);
    EmptyHint->setObjectName("MutedLabel");
    EmptyHint->setWordWrap(true);
    EmptyLayout->addWidget(EmptyHint);
    EmptyLayout->addStretch();
    InspectorPages->addWidget(EmptyInspectorPage);

    auto* ObjectInspectorPage = new EditorUi::ObjectInspectorView(InspectorPages);
    ObjectInspectorContent = ObjectInspectorPage;
    CameraPreviewPanel = new QWidget(this, Qt::Tool | Qt::FramelessWindowHint
        | Qt::NoDropShadowWindowHint | Qt::WindowDoesNotAcceptFocus);
    CameraPreviewPanel->setObjectName("CameraPreviewPanel");
    CameraPreviewPanel->setAttribute(Qt::WA_ShowWithoutActivating);
    auto* CameraPreviewLayout = new QVBoxLayout(CameraPreviewPanel);
    CameraPreviewLayout->setContentsMargins(12, 10, 12, 10);
    CameraPreviewLayout->setSpacing(7);
    auto* CameraPreviewTitle = new QLabel(tr("Вид камеры"), CameraPreviewPanel);
    CameraPreviewTitle->setProperty("uiRole", "objectSectionTitle");
    CameraPreviewLayout->addWidget(CameraPreviewTitle);
    CameraPreviewViewport = new RenderViewportWidget(CameraPreviewPanel);
    CameraPreviewViewport->setObjectName("CameraPreviewViewport");
    CameraPreviewViewport->SetViewId(RenderViewId{2});
    CameraPreviewViewport->setMinimumSize(160, 90);
    CameraPreviewViewport->setFixedHeight(158);
    CameraPreviewViewport->setFocusPolicy(Qt::NoFocus);
    CameraPreviewLayout->addWidget(CameraPreviewViewport);
    CameraPreviewUnavailableLabel = new QLabel(tr("Предпросмотр требует D3D12 или Vulkan"), CameraPreviewPanel);
    CameraPreviewUnavailableLabel->setWordWrap(true);
    CameraPreviewUnavailableLabel->hide();
    CameraPreviewLayout->addWidget(CameraPreviewUnavailableLabel);
    CameraPreviewPanel->hide();
    connect(CameraPreviewViewport, &RenderViewportWidget::NativeSurfaceChanged,
        this, &EditorMainWindow::OnViewportSurfaceChanged);
    connect(CameraPreviewViewport, &RenderViewportWidget::ViewportResized,
        this, &EditorMainWindow::OnViewportResized);
    ObjectNameEdit = ObjectInspectorPage->Name;
    const auto& Fields = ObjectInspectorPage->TransformFields;
    for (int Index = 0; Index < static_cast<int>(Fields.size()); ++Index)
    {
        BindTransformScrubbing(static_cast<EditorUi::StudioNumberEdit*>(Fields[Index]), Index);
    }
    PositionXSpin = Fields[0]; PositionYSpin = Fields[1]; PositionZSpin = Fields[2];
    RotationXSpin = Fields[3]; RotationYSpin = Fields[4]; RotationZSpin = Fields[5];
    ScaleXSpin = Fields[6]; ScaleYSpin = Fields[7]; ScaleZSpin = Fields[8];
    ComponentCombo = ObjectInspectorPage->ComponentFilter;
    ComponentInspectorLayout = ObjectInspectorPage->Components;
    connect(ObjectInspectorPage->Active, &QCheckBox::toggled, this, [this](bool Active) {
        auto* Selected = GetSelectedGameObject();
        if (!bUpdatingSelectionUi && Selected && !BoundEngine.GetPlaySession().IsSimulating())
        { ExecuteCommand(MakeSetObjectActiveCommand(GetEditScene(), Selected->GetObjectHandle(), Active)); }
    });
    ObjectInspectorPage->AddComponentRequested = [this, ObjectInspectorPage](int Index) {
        if (BoundEngine.GetPlaySession().IsSimulating() || Index < 0) { return; }
        const auto Type = ObjectInspectorPage->AddComponentType->itemData(Index).toString().toStdString();
        if (!Type.empty()) { RunEditorActionByTypeId(Type.c_str()); }
    };
    connect(ObjectInspectorPage->ParentObject, &QComboBox::activated, this, [this, ObjectInspectorPage](int Index) {
        auto* Selected = GetSelectedGameObject();
        if (!Selected || !GetEditScene() || BoundEngine.GetPlaySession().IsSimulating()) { return; }
        const auto Identity = ObjectInspectorPage->ParentObject->itemData(Index).toString().toStdString();
        auto* Parent = GetEditScene()->FindByPersistentId(Identity);
        if (!Identity.empty() && !Parent) { RefreshSelectionUi(); return; }
        ReparentHierarchyObject(Selected->GetObjectHandle(), Parent ? Parent->GetObjectHandle() : ObjectHandle{});
    });
    connect(ObjectInspectorPage->AnchorObject, &QComboBox::activated, this, [this, ObjectInspectorPage](int Index) {
        auto* Selected = GetSelectedGameObject();
        if (!Selected || !GetEditScene() || BoundEngine.GetPlaySession().IsSimulating()) { return; }
        auto* Anchor = GetEditScene()->FindByPersistentId(ObjectInspectorPage->AnchorObject->itemData(Index).toString().toStdString());
        if (!Anchor) { return; }
        auto Updated = Selected->GetTransform();
        Updated.Position = Anchor->GetWorldTransform().Position;
        if (auto* Parent = Selected->GetParent())
        {
            const auto Matrix = Parent->GetWorldMatrix();
            if (std::abs(Matrix.Determinant()) < 1.e-8f) { RefreshSelectionUi(); return; }
            Updated.Position = Vector3::Transform(Updated.Position, Matrix.Invert());
        }
        ExecuteSelectedTransform(Updated);
        const QSignalBlocker Blocker(ObjectInspectorPage->AnchorObject);
        ObjectInspectorPage->AnchorObject->setCurrentIndex(0);
    });
    ObjectInspectorPage->ResetRequested = [this]() {
        if (auto* Selected = GetSelectedGameObject(); Selected && !BoundEngine.GetPlaySession().IsSimulating())
        { ExecuteSelectedTransform(Transform{}); }
    };
    ObjectInspectorPage->DeleteRequested = [this]() { OnDeleteSelectedObject(); };
    ObjectInspectorPage->DuplicateRequested = [this]() { DuplicateSelectedObject(); };
    ObjectInspectorPage->FrameRequested = [this]() { FrameSelectedObject(); };

    InspectorPages->addWidget(ObjectInspectorPage);
    StoryInspector = new EditorUi::StoryInspectorView(InspectorPages);
    InspectorPages->addWidget(StoryInspector);
    InspectorTabs->addTab(InspectorPages, QString::fromUtf8("\xD0\x98\xD0\xBD\xD1\x81\xD0\xBF\xD0\xB5\xD0\xBA\xD1\x82\xD0\xBE\xD1\x80"));
    InspectorLayout->addWidget(InspectorTabs);

    auto AddPanelControls = [this](QTabWidget* Tabs, QWidget* Panel, const QString& Title)
    {
        auto* Visibility = findChild<QMenu*>("WindowsMenu")->addAction(Title);
        Visibility->setObjectName(Panel->objectName() + "Visibility");
        Visibility->setCheckable(true);
        Visibility->setChecked(true);
        const QString Area = Tabs->property("workspaceArea").toString();
        Docking->AddPage(Area, *Tabs->widget(0), Area, *Visibility, true);
        Tabs->setTabsClosable(true);
        auto* Controls = new QWidget(Tabs);
        auto* ControlLayout = new QHBoxLayout(Controls);
        ControlLayout->setContentsMargins(0, 0, 4, 0);
        ControlLayout->setSpacing(0);
        auto* Expand = new QPushButton(EditorUi::MakeStudioIcon(EditorUi::StudioIcon::Expand), {}, Controls);
        Expand->setProperty("uiRole", "chromeButton");
        Expand->setProperty("workspaceExpand", true);
        Expand->setToolTip(tr("Развернуть / восстановить панель"));
        Expand->setCheckable(true);
        ControlLayout->addWidget(Expand);
        connect(Expand, &QPushButton::toggled, this, [this, Panel](bool bExpanded)
        {
            for (int Index = 0; Index < WorkspaceSplitter->count(); ++Index)
            {
                QWidget* Region = WorkspaceSplitter->widget(Index);
                Region->setVisible(!bExpanded || Region == Panel);
            }
            if (!bExpanded)
            {
                EditorUi::WorkspaceLayoutAdapter::Reset(*WorkspaceSplitter, *CenterSplitter, height());
            }
        });
        auto* Collapse = new QPushButton(EditorUi::MakeStudioIcon(EditorUi::StudioIcon::Panel), {}, Controls);
        Collapse->setProperty("uiRole", "chromeButton");
        Collapse->setToolTip(tr("Закрыть текущую вкладку; вернуть через меню «Окна»"));
        ControlLayout->addWidget(Collapse);
        connect(Collapse, &QPushButton::clicked, this, [this, Tabs]()
        { if (auto* Page = Tabs->currentWidget()) { Docking->SetOpen(*Page, false); } });
        Tabs->setCornerWidget(Controls);
    };
    AddPanelControls(HierarchyTabs, HierarchyPanel, tr("Иерархия"));
    AddPanelControls(InspectorTabs, InspectorPanel, tr("Инспектор"));
    auto* DockControls = new QWidget(DockTabs);
    auto* DockControlsLayout = new QHBoxLayout(DockControls);
    DockControlsLayout->setContentsMargins(0, 0, 8, 0);
    auto* ExpandGraph = new QPushButton(EditorUi::MakeStudioIcon(EditorUi::StudioIcon::Expand), {}, DockControls);
    ExpandGraph->setProperty("uiRole", "chromeButton");
    ExpandGraph->setProperty("workspaceExpand", true);
    ExpandGraph->setToolTip(tr("Развернуть / восстановить поток истории"));
    ExpandGraph->setCheckable(true);
    DockControlsLayout->addWidget(ExpandGraph);
    connect(ExpandGraph, &QPushButton::toggled, this, [this](bool bExpanded)
    {
        if (bExpanded)
        {
            const bool bToolsFirst = CenterSplitter->widget(0)->objectName() == "GraphDock";
            CenterSplitter->setSizes(bToolsFirst ? QList<int>{height(), 0} : QList<int>{0, height()});
        }
        else
        {
            EditorUi::WorkspaceLayoutAdapter::Reset(*WorkspaceSplitter, *CenterSplitter, height());
        }
    });
    DockTabs->setCornerWidget(DockControls);

    connect(ObjectNameEdit, &QLineEdit::editingFinished, this, &EditorMainWindow::OnObjectNameEdited);
    connect(PositionXSpin, &QDoubleSpinBox::editingFinished, this, &EditorMainWindow::OnTransformEdited);
    connect(PositionYSpin, &QDoubleSpinBox::editingFinished, this, &EditorMainWindow::OnTransformEdited);
    connect(PositionZSpin, &QDoubleSpinBox::editingFinished, this, &EditorMainWindow::OnTransformEdited);
    connect(RotationXSpin, &QDoubleSpinBox::editingFinished, this, &EditorMainWindow::OnTransformEdited);
    connect(RotationYSpin, &QDoubleSpinBox::editingFinished, this, &EditorMainWindow::OnTransformEdited);
    connect(RotationZSpin, &QDoubleSpinBox::editingFinished, this, &EditorMainWindow::OnTransformEdited);
    connect(ScaleXSpin, &QDoubleSpinBox::editingFinished, this, &EditorMainWindow::OnTransformEdited);
    connect(ScaleYSpin, &QDoubleSpinBox::editingFinished, this, &EditorMainWindow::OnTransformEdited);
    connect(ScaleZSpin, &QDoubleSpinBox::editingFinished, this, &EditorMainWindow::OnTransformEdited);
    connect(ComponentCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &EditorMainWindow::OnComponentFilterChanged);

    EditorUi::WorkspaceLayoutAdapter::Reset(*WorkspaceSplitter, *CenterSplitter, height());
    setCentralWidget(Root);

    StatusIssuesLabel = new QLabel(statusBar());
    StatusSelectionLabel = new QLabel(statusBar());
    statusBar()->addWidget(StatusIssuesLabel);
    statusBar()->addPermanentWidget(StatusSelectionLabel);

    connect(HierarchyTree, &QTreeWidget::itemSelectionChanged, this, &EditorMainWindow::OnHierarchySelectionChanged);
    connect(HierarchyTree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* Item, int)
    {
        if (Item == nullptr) { return; }
        const ObjectHandle Target = LoadObjectHandle(Item);
        if (Target != GetSelectedObjectHandle()) { SelectObject(Target); }
        FrameSelectedObject();
    });
    connect(HierarchyTree, &QTreeWidget::customContextMenuRequested, this, &EditorMainWindow::OnHierarchyContextMenu);
}

void EditorMainWindow::BuildStoryDockPage()
{
    StoryPanel = new StoryWidget(StoryPlayback, DockTabs);
    StoryDockPage = StoryPanel;
    DockTabs->addTab(StoryDockPage, tr("Проигрыватель"));
}

void EditorMainWindow::BuildRenderStatisticsDockPage()
{
    auto* Page = new QWidget(DockTabs);
    auto* Layout = new QVBoxLayout(Page);
    Layout->setContentsMargins(16, 16, 16, 16);
    Layout->setSpacing(8);
    auto* TitleLabel = new QLabel(QString::fromUtf8("\xD0\x9D\xD0\xB0\xD1\x81\xD1\x82\xD1\x80\xD0\xBE\xD0\xB9\xD0\xBA\xD0\xB8 \xD1\x81\xD1\x82\xD0\xB0\xD1\x82\xD0\xB8\xD1\x81\xD1\x82\xD0\xB8\xD0\xBA\xD0\xB8"), Page);
    TitleLabel->setProperty("uiRole", "sectionTitle");
    Layout->addWidget(TitleLabel);
    RenderStatisticsLabel = new QLabel(Page);
    RenderStatisticsLabel->setWordWrap(true);
    RenderStatisticsLabel->setObjectName("MutedLabel");
    RenderStatisticsLabel->setProperty("uiRole", "description");
    RenderStatisticsLabel->setText(tr("Ожидание статистики рендера…"));
    Layout->addWidget(RenderStatisticsLabel);
    NavigationStatisticsLabel = new QLabel(Page);
    NavigationStatisticsLabel->setObjectName("NavigationStatisticsLabel");
    NavigationStatisticsLabel->setWordWrap(true);
    Layout->addWidget(NavigationStatisticsLabel);
    Layout->addStretch(1);
    DockTabs->addTab(Page, QString::fromUtf8("\xD0\x9D\xD0\xB0\xD1\x81\xD1\x82\xD1\x80\xD0\xBE\xD0\xB9\xD0\xBA\xD0\xB8 \xD1\x81\xD1\x82\xD0\xB0\xD1\x82\xD0\xB8\xD1\x81\xD1\x82\xD0\xB8\xD0\xBA\xD0\xB8"));
}

void EditorMainWindow::BuildPostprocessDockPage()
{
    auto* Page = new QWidget(DockTabs);
    auto* Layout = new QVBoxLayout(Page);
    Layout->setContentsMargins(16, 16, 16, 16);
    Layout->setSpacing(8);
    auto* TitleLabel = new QLabel(QString::fromUtf8("\xD0\x9D\xD0\xB0\xD1\x81\xD1\x82\xD1\x80\xD0\xBE\xD0\xB9\xD0\xBA\xD0\xB8 \xD0\xBF\xD0\xBE\xD1\x81\xD1\x82\xD0\xBE\xD0\xB1\xD1\x80\xD0\xB0\xD0\xB1\xD0\xBE\xD1\x82\xD0\xBA\xD0\xB8"), Page);
    TitleLabel->setProperty("uiRole", "sectionTitle");
    Layout->addWidget(TitleLabel);

    auto* Form = new QFormLayout();
    Form->setContentsMargins(0, 0, 0, 0);
    Form->setSpacing(8);

    auto* Exposure = new QDoubleSpinBox(Page);
    Exposure->setRange(0.01, 20.0);
    Exposure->setSingleStep(0.1);
    Exposure->setDecimals(2);
    Exposure->setValue(RenderConfiguration.Exposure);
    Form->addRow(tr("Exposure"), Exposure);
    connect(Exposure, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double Value)
    {
        RenderConfiguration.Exposure = static_cast<float>(Value);
    });

    auto* Environment = new QDoubleSpinBox(Page);
    Environment->setRange(0.0, 10.0);
    Environment->setSingleStep(0.1);
    Environment->setDecimals(2);
    Environment->setValue(RenderConfiguration.EnvironmentIntensity);
    Form->addRow(tr("IBL"), Environment);
    connect(Environment, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double Value)
    {
        RenderConfiguration.EnvironmentIntensity = static_cast<float>(Value);
    });

    auto* Bloom = new QDoubleSpinBox(Page);
    Bloom->setRange(0.0, 1.0);
    Bloom->setSingleStep(0.01);
    Bloom->setDecimals(2);
    Bloom->setValue(RenderConfiguration.BloomIntensity);
    Form->addRow(tr("Bloom"), Bloom);
    connect(Bloom, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double Value)
    {
        RenderConfiguration.BloomIntensity = static_cast<float>(Value);
    });

    auto* Antialiasing = new EditorUi::StudioCheckBox(tr("FXAA"), Page);
    Antialiasing->setChecked(RenderConfiguration.bAntialiasing);
    Form->addRow(QString(), Antialiasing);
    connect(Antialiasing, &QCheckBox::toggled, this, [this](bool bEnabled)
    {
        RenderConfiguration.bAntialiasing = bEnabled;
    });

    auto* MaterialEffects = new EditorUi::StudioCheckBox(tr("Preview Camera Material Effects"), Page);
    MaterialEffects->setChecked(RenderConfiguration.bMaterialPostProcess);
    Form->addRow(QString(), MaterialEffects);
    connect(MaterialEffects, &QCheckBox::toggled, this, [this](bool bEnabled)
    {
        RenderConfiguration.bMaterialPostProcess = bEnabled;
    });
    Layout->addLayout(Form);
    Layout->addStretch(1);
    DockTabs->addTab(Page, QString::fromUtf8("\xD0\x9D\xD0\xB0\xD1\x81\xD1\x82\xD1\x80\xD0\xBE\xD0\xB9\xD0\xBA\xD0\xB8 \xD0\xBF\xD0\xBE\xD1\x81\xD1\x82\xD0\xBE\xD0\xB1\xD1\x80\xD0\xB0\xD0\xB1\xD0\xBE\xD1\x82\xD0\xBA\xD0\xB8"));
}

void EditorMainWindow::PopulateDockPages()
{
    StoryGraph = new EditorUi::StoryWorkspaceView(DockTabs);
    DockTabs->addTab(StoryGraph, tr("Поток истории"));
    InitializeStoryAuthoring();
    InitializeWidgetAuthoring();
    BuildAudioEnvironmentDockPage();

    ContentBrowser = new ContentBrowserWidget(DockTabs);
    ContentBrowser->SetEngine(&BoundEngine);
    ContentBrowser->SetProjectContext(&BoundSession, &Document);
    connect(ContentBrowser, &ContentBrowserWidget::AssetsChanged, this, [this]()
    {
        RefreshStoryTargets();
        UpdateWindowTitleDirty();
        UpdateStatus();
    });
    connect(ContentBrowser, &ContentBrowserWidget::AssetActivated, this, &EditorMainWindow::OnAssetActivated);
    DockTabs->addTab(ContentBrowser, tr("Проводник"));
    BuildStoryDockPage();
    BuildRenderStatisticsDockPage();
    BuildPostprocessDockPage();

    auto* WindowMenu = findChild<QMenu*>("WindowsMenu");
    WindowMenu->addSeparator();
    const QStringList PageIds = {"story", "widgets", "active", "content", "playback", "statistics", "postprocess"};
    for (int Index = 0; Index < DockTabs->count(); ++Index)
    {
        QWidget* Page = DockTabs->widget(Index);
        // QStackedLayout otherwise includes every hidden page's minimum width.
        // Hidden dock pages must not squeeze the shell's side rails.
        Page->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        QAction* Action = new QAction(DockTabs->tabText(Index), Page);
        Action->setObjectName("WorkspaceVisibility");
        WindowMenu->addAction(Action);
        if (Page==WidgetWorkspace || Page==StoryGraph)
        {
            connect(Action,&QAction::triggered,this,[this,Page](bool Open)
            {
                if (!Open) return;
                bWidgetEditingContext=Page==WidgetWorkspace;
                bStoryEditingContext=Page==StoryGraph;
            });
        }
        Action->setCheckable(true);
        const bool InitiallyVisible = Index <= 1 || Page == ContentBrowser;
        Action->setChecked(InitiallyVisible);
        DockTabs->setTabVisible(Index, InitiallyVisible);
        Docking->AddPage(PageIds.value(Index), *Page, "workspace", *Action);
    }
    DockTabs->setTabsClosable(true);
    DockTabs->setCurrentWidget(StoryGraph);
}

void EditorMainWindow::BuildWindowsMenuView()
{
    auto* Menu = findChild<QMenu*>("WindowsMenu");
    for (auto* Action : Menu->actions()) { Action->setVisible(false); }

    auto* Scroll = new QScrollArea(Menu);
    Scroll->setObjectName("WindowsMenuScroll");
    Scroll->setFrameShape(QFrame::NoFrame);
    Scroll->setWidgetResizable(true);
    Scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    Scroll->setFixedWidth(275);
    auto* Body = new QWidget(Scroll);
    Body->setObjectName("WindowsMenuBody");
    auto* Layout = new QVBoxLayout(Body);
    Layout->setContentsMargins(6, 4, 6, 4);
    Layout->setSpacing(0);
    auto* SearchBox = new QWidget(Body);
    SearchBox->setObjectName("WindowsMenuSearchBox");
    auto* SearchLayout = new QHBoxLayout(SearchBox);
    SearchLayout->setContentsMargins(8, 3, 8, 5);
    SearchLayout->setSpacing(7);
    auto* SearchIcon = new QLabel(SearchBox);
    SearchIcon->setPixmap(EditorUi::MakeFrontendIcon("Search").pixmap(15, 15));
    SearchLayout->addWidget(SearchIcon);
    auto* Search = new QLineEdit(SearchBox);
    Search->setObjectName("WindowsMenuSearch");
    Search->setPlaceholderText(tr("Найти окно…"));
    Search->setAccessibleName(tr("Найти окно"));
    SearchLayout->addWidget(Search, 1);
    Layout->addWidget(SearchBox);

    auto* FixedGroup = new QWidget(Body);
    FixedGroup->setObjectName("WindowsMenuFixedGroup");
    auto* FixedLayout = new QVBoxLayout(FixedGroup);
    FixedLayout->setContentsMargins(0, 4, 0, 5);
    FixedLayout->setSpacing(0);
    Layout->addWidget(FixedGroup);
    auto* WindowGroup = new QWidget(Body);
    WindowGroup->setObjectName("WindowsMenuWindowGroup");
    auto* WindowLayout = new QVBoxLayout(WindowGroup);
    WindowLayout->setContentsMargins(0, 4, 0, 5);
    WindowLayout->setSpacing(0);
    Layout->addWidget(WindowGroup);

    struct MenuRow { QWidget* Container; QPushButton* Check; QPushButton* Reveal; QAction* Visibility; QString Label; bool SearchOnly; bool HasReveal; };
    auto Rows = std::make_shared<QVector<MenuRow>>();
    auto AddRow = [this, Body, Rows, Menu](QVBoxLayout* Group, const QString& Id, const QString& Label,
                                            const QString& Icon, QAction* Visibility, QWidget* Page,
                                            bool SearchOnly = false)
    {
        auto* Row = new QWidget(Body);
        Row->setObjectName("WindowsMenuRow_" + Id);
        auto* RowLayout = new QHBoxLayout(Row);
        RowLayout->setContentsMargins(0, 0, 0, 0);
        RowLayout->setSpacing(0);
        auto* Check = new QPushButton(Row);
        Check->setObjectName("WindowsMenuCheck_" + Id);
        Check->setProperty("uiRole", "windowsMenuCheck");
        Check->setFixedWidth(24);
        Check->setEnabled(Visibility != nullptr);
        RowLayout->addWidget(Check);
        QIcon RowIcon = EditorUi::MakeFrontendIcon(Icon);
        if (!Visibility && Id != "event")
        {
            // Keep unavailable future windows visually aligned with the React list.
            QIcon PassiveIcon;
            PassiveIcon.addPixmap(RowIcon.pixmap(15, 15), QIcon::Disabled);
            RowIcon = PassiveIcon;
        }
        auto* Toggle = new QPushButton(RowIcon, Label, Row);
        Toggle->setObjectName("WindowsMenuToggle_" + Id);
        Toggle->setProperty("uiRole", "windowsMenuRow");
        Toggle->setFlat(true);
        Toggle->setEnabled(Visibility != nullptr);
        Toggle->setAccessibleName(Label);
        Toggle->setIconSize(QSize(15, 15));
        if (!Visibility) Toggle->setToolTip(tr("Это окно пока недоступно в нативном редакторе"));
        RowLayout->addWidget(Toggle, 1);
        auto* Reveal = new QPushButton(Row);
        Reveal->setObjectName("WindowsMenuReveal_" + Id);
        Reveal->setProperty("uiRole", "windowsMenuReveal");
        Reveal->setIcon(EditorUi::MakeFrontendIcon("ArrowUpRight"));
        Reveal->setIconSize(QSize(15, 15));
        Reveal->setFixedWidth(24);
        Reveal->setToolTip(tr("Перейти к окну «%1»").arg(Label));
        Reveal->setVisible(Visibility && Visibility->isChecked() && Page);
        RowLayout->addWidget(Reveal);
        Group->addWidget(Row);
        Rows->append({Row, Check, Reveal, Visibility, Label, SearchOnly, Page != nullptr});
        if (Visibility)
        {
            connect(Check, &QPushButton::clicked, this, [Visibility]() { Visibility->trigger(); });
            connect(Toggle, &QPushButton::clicked, this, [Visibility]() { Visibility->trigger(); });
            if (Page)
            {
                connect(Reveal, &QPushButton::clicked, this, [this, Page, Menu]()
                { Docking->Open(*Page); Menu->hide(); });
            }
        }
    };
    auto* HierarchyAction = findChild<QAction*>("HierarchyPanelVisibility");
    auto* InspectorAction = findChild<QAction*>("InspectorPanelVisibility");
    auto PageAction = [](QWidget* Page) -> QAction*
    {
        return Page ? Page->findChild<QAction*>("WorkspaceVisibility", Qt::FindDirectChildrenOnly) : nullptr;
    };
    AddRow(FixedLayout, "hierarchy", tr("Иерархия"), "PanelLeft", HierarchyAction, nullptr);
    AddRow(FixedLayout, "inspector", tr("Инспектор"), "PanelRight", InspectorAction, nullptr);
    AddRow(FixedLayout, "history", tr("История · отдельное окно"), "BookOpen", nullptr, nullptr);
    struct WindowSpec { const char* Id; const char* Label; const char* Icon; QWidget* Page; };
    const WindowSpec Windows[] = {
        {"story", "Поток истории", "Workflow", StoryGraph},
        {"subscenes", "Сабсцены", "PanelsTopLeft", nullptr},
        {"characters", "Персонажи", "Users", nullptr},
        {"widgets", "Виджеты UI", "PanelsTopLeft", WidgetWorkspace},
        {"gameplay", "Игровые механики", "Gamepad2", nullptr},
        {"scripting", "Скриптинг", "Code2", nullptr},
        {"cameras", "Камеры", "Video", nullptr},
        {"input", "Ввод", "Keyboard", nullptr},
        {"files", "Проводник", "FolderOpen", ContentBrowser},
        {"active", "Звук и окружение", "CloudSun", AudioEnvironment},
        {"event", "Событие", "Layers", nullptr},
        {"create", "Создание", "Plus", nullptr},
        {"assets", "Проект", "Library", nullptr},
        {"sound", "Звук", "Music2", nullptr},
        {"samples", "Эффекты", "Sparkles", nullptr},
        {"issues", "Проблемы", "TriangleAlert", nullptr}
    };
    for (const auto& Window : Windows)
    {
        AddRow(WindowLayout, QString::fromLatin1(Window.Id), tr(Window.Label),
               QString::fromLatin1(Window.Icon), PageAction(Window.Page), Window.Page);
    }
    // Older native tools remain searchable so a closed tab can always be reopened.
    for (int Index = 4; Index < DockTabs->count(); ++Index)
    {
        QWidget* Page = DockTabs->widget(Index);
        AddRow(WindowLayout, "native_" + QString::number(Index), DockTabs->tabText(Index),
               "PanelsTopLeft", PageAction(Page), Page, true);
    }

    auto* Footer = new QWidget(Body);
    Footer->setObjectName("WindowsMenuFooter");
    auto* FooterLayout = new QVBoxLayout(Footer);
    FooterLayout->setContentsMargins(8, 8, 8, 4);
    FooterLayout->setSpacing(5);
    FooterLayout->addWidget(new QLabel(tr("Рабочая область"), Footer));
    auto* Position = new QComboBox(Footer);
    Position->setObjectName("WindowsMenuPosition");
    Position->setAccessibleName(tr("Расположение рабочей области"));
    Position->addItem(tr("Снизу от сцены"), "bottom");
    Position->addItem(tr("Слева от сцены"), "left");
    Position->addItem(tr("Справа от сцены"), "right");
    FooterLayout->addWidget(Position);
    connect(Position, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, Position](int Index)
    { Docking->SetWorkspacePosition(Position->itemData(Index).toString()); });
    auto* Restore = new QPushButton(EditorUi::MakeFrontendIcon("RotateCcw"), tr("Восстановить раскладку"), Footer);
    Restore->setObjectName("WindowsMenuRestore");
    Restore->setProperty("uiRole", "windowsMenuRestore");
    FooterLayout->addWidget(Restore);
    connect(Restore, &QPushButton::clicked, this, [this, Menu]()
    { findChild<QAction*>("RestoreWorkspaceLayout")->trigger(); Menu->hide(); });
    Layout->addWidget(Footer);
    Scroll->setWidget(Body);
    auto* WidgetAction = new QWidgetAction(Menu);
    WidgetAction->setDefaultWidget(Scroll);
    Menu->addAction(WidgetAction);
    Menu->setObjectName("WindowsMenu");
    Menu->setStyleSheet(EditorUi::EditorStyle::LoadStylesheet());
    auto Update = [this, Rows, FixedGroup, Footer, Position, Search, Scroll, Body, Layout]()
    {
        const QString Query = Search->text().trimmed();
        FixedGroup->setVisible(Query.isEmpty());
        Footer->setVisible(Query.isEmpty());
        for (auto& Row : *Rows)
        {
            Row.Container->setVisible((!Row.SearchOnly || !Query.isEmpty()) &&
                (Row.Container->parentWidget() == FixedGroup ||
                 Row.Label.contains(Query, Qt::CaseInsensitive)));
            if (Row.Visibility)
            {
                Row.Check->setIcon(Row.Visibility->isChecked()
                    ? EditorUi::MakeFrontendIcon("Check", "--accent") : QIcon());
                Row.Check->setIconSize(QSize(14, 14));
                Row.Reveal->setVisible(Row.Visibility->isChecked() && Row.HasReveal);
            }
        }
        const QSignalBlocker Block(Position);
        Position->setCurrentIndex(std::max(0, Position->findData(Docking->WorkspacePosition())));
        Layout->invalidate();
        Layout->activate();
        Body->updateGeometry();
        const int Available = QGuiApplication::primaryScreen()->availableGeometry().height() - 80;
        Scroll->setFixedHeight(std::min(Layout->sizeHint().height(), Available));
    };
    connect(Search, &QLineEdit::textChanged, this, [Update](const QString&) { Update(); });
    connect(Menu, &QMenu::aboutToShow, this, [Search, Scroll, Update]()
    {
        Search->clear();
        Update();
        QTimer::singleShot(0, Search, [Search, Scroll]()
        { Scroll->verticalScrollBar()->setValue(0); Search->setFocus(); });
    });
    for (const auto& Row : *Rows)
    {
        if (Row.Visibility)
        { connect(Row.Visibility, &QAction::toggled, this, [Update](bool) { Update(); }); }
    }
}

void EditorMainWindow::OnLayoutModeChanged(int Index)
{
    if (WorkspaceSplitter == nullptr || CenterSplitter == nullptr)
    {
        return;
    }

    const QString Mode = LayoutCombo->itemData(Index).toString();
    const bool bToolsFirst = CenterSplitter->widget(0)->objectName() == "GraphDock";
    if (Mode == "graph")
    {
        CenterSplitter->setSizes(bToolsFirst ? QList<int>{680, 120} : QList<int>{120, 680});
        Docking->Open(*StoryGraph);
    }
    else if (Mode == "scene")
    {
        CenterSplitter->setSizes(bToolsFirst ? QList<int>{80, 700} : QList<int>{700, 80});
    }
    else if (Mode == "hierarchy")
    {
        WorkspaceSplitter->setSizes({520, 500, 200});
    }
    else if (Mode == "inspector")
    {
        WorkspaceSplitter->setSizes({180, 500, 540});
    }
    else
    {
        EditorUi::WorkspaceLayoutAdapter::Reset(*WorkspaceSplitter, *CenterSplitter, height());
    }
    Docking->Save();
}

