#include "Shell/EditorMainWindow.h"

#include "Actions/BuiltinEditorActions.h"
#include "Actions/EditorAction.h"
#include "Shell/EditorClipboard.h"
#include "Commands/EditorCommands.h"
#include "Panels/ContentBrowserWidget.h"
#include "Shell/EditorTheme.h"
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
#include "UI/StoryWidget.h"

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
    CreateMenu->addSeparator();
    CreateMenu->addAction(QString::fromUtf8("\xD0\x94\xD0\xB5\xD0\xB9\xD1\x81\xD1\x82\xD0\xB2\xD0\xB8\xD1\x8F \xD0\xB8 \xD1\x81\xD0\xBE\xD0\xB1\xD1\x8B\xD1\x82\xD0\xB8\xD1\x8F"), this, &EditorMainWindow::OnFocusActionsAndEvents);

    QMenu* WindowMenu = menuBar()->addMenu(QString::fromUtf8("\xD0\x9E\xD0\xBA\xD0\xBD\xD0\xBE"));
    WindowMenu->addAction(QString::fromUtf8("\xD0\x92\xD0\xBE\xD1\x81\xD1\x81\xD1\x82\xD0\xB0\xD0\xBD\xD0\xBE\xD0\xB2\xD0\xB8\xD1\x82\xD1\x8C \xD1\x80\xD0\xB0\xD1\x81\xD0\xBA\xD0\xBB\xD0\xB0\xD0\xB4\xD0\xBA\xD1\x83"), this, [this]()
    {
        if (LayoutCombo != nullptr)
        {
            LayoutCombo->setCurrentIndex(0);
        }
        if (WorkspaceSplitter != nullptr)
        {
            WorkspaceSplitter->setSizes({320, 900, 240});
        }
        if (CenterSplitter != nullptr)
        {
            CenterSplitter->setSizes({520, 280});
        }
    });

    auto* Brand = new QLabel(menuBar());
    Brand->setText(QString::fromUtf8("SakuraNovel Studio"));
    Brand->setStyleSheet("padding: 0 12px; color: #c496ad; font-weight: 700;");
    menuBar()->setCornerWidget(Brand, Qt::TopLeftCorner);

    ProjectTitleLabel = new QLabel(menuBar());
    ProjectTitleLabel->setObjectName("MutedLabel");
    ProjectTitleLabel->setStyleSheet("color: #95909e; padding-right: 12px;");
    menuBar()->setCornerWidget(ProjectTitleLabel, Qt::TopRightCorner);
}

void EditorMainWindow::BuildToolbar()
{
    auto* Toolbar = addToolBar("Editor");
    Toolbar->setObjectName("EditorToolbar");
    Toolbar->setMovable(false);
    Toolbar->setFloatable(false);
    Toolbar->setIconSize(QSize(16, 16));

    PlayButton = new QPushButton(QString::fromUtf8("\xE2\x96\xB6"), Toolbar);
    PlayButton->setObjectName("PlayButton");
    PlayButton->setCheckable(true);
    PlayButton->setToolTip(QString::fromUtf8("\xD0\x97\xD0\xB0\xD0\xBF\xD1\x83\xD1\x81\xD1\x82\xD0\xB8\xD1\x82\xD1\x8C \xD0\xBF\xD1\x80\xD0\xB5\xD0\xB4\xD0\xBF\xD1\x80\xD0\xBE\xD1\x81\xD0\xBC\xD0\xBE\xD1\x82\xD1\x80"));
    PauseButton = new QPushButton(QString::fromUtf8("\xE2\x8F\xB8"), Toolbar);
    PauseButton->setProperty("class", "EditorToolButton");
    PauseButton->setCheckable(true);
    PauseButton->setEnabled(false);
    StepButton = new QPushButton(QString::fromUtf8("\xE2\x8F\xA9"), Toolbar);
    StepButton->setProperty("class", "EditorToolButton");
    StepButton->setEnabled(false);
    Toolbar->addWidget(PlayButton);
    Toolbar->addWidget(PauseButton);
    Toolbar->addWidget(StepButton);
    Toolbar->addSeparator();

    ModeLabel = new QLabel(QString::fromUtf8("\xD0\xA0\xD0\xB5\xD0\xB4\xD0\xB0\xD0\xBA\xD1\x82\xD0\xB8\xD1\x80\xD0\xBE\xD0\xB2\xD0\xB0\xD0\xBD\xD0\xB8\xD0\xB5"), Toolbar);
    ModeLabel->setObjectName("RoseLabel");
    ModeLabel->setStyleSheet("color: #c496ad; padding: 0 8px;");
    Toolbar->addWidget(ModeLabel);

    auto* Spacer = new QWidget(Toolbar);
    Spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    Toolbar->addWidget(Spacer);

    LayoutCombo = new QComboBox(Toolbar);
    LayoutCombo->addItem(QString::fromUtf8("\xD0\x9F\xD0\xBE\xD1\x81\xD1\x82\xD0\xB0\xD0\xBD\xD0\xBE\xD0\xB2\xD0\xBA\xD0\xB0"), "balanced");
    LayoutCombo->addItem(QString::fromUtf8("\xD0\xA1\xD1\x86\xD0\xB5\xD0\xBD\xD0\xB0\xD1\x80\xD0\xB8\xD0\xB9"), "graph");
    LayoutCombo->addItem(QString::fromUtf8("\xD0\xA2\xD0\xBE\xD0\xBB\xD1\x8C\xD0\xBA\xD0\xBE \xD1\x81\xD1\x86\xD0\xB5\xD0\xBD\xD0\xB0"), "scene");
    LayoutCombo->addItem(QString::fromUtf8("\xD0\xA2\xD0\xBE\xD0\xBB\xD1\x8C\xD0\xBA\xD0\xBE \xD0\xB8\xD0\xB5\xD1\x80\xD0\xB0\xD1\x80\xD1\x85\xD0\xB8\xD1\x8F"), "hierarchy");
    LayoutCombo->addItem(QString::fromUtf8("\xD0\xA2\xD0\xBE\xD0\xBB\xD1\x8C\xD0\xBA\xD0\xBE \xD0\xB8\xD0\xBD\xD1\x81\xD0\xBF\xD0\xB5\xD0\xBA\xD1\x82\xD0\xBE\xD1\x80"), "inspector");
    Toolbar->addWidget(LayoutCombo);

    auto* ActionsButton = new QPushButton(QString::fromUtf8("+\xC2\xA0\xD0\x94\xD0\xB5\xD0\xB9\xD1\x81\xD1\x82\xD0\xB2\xD0\xB8\xD1\x8F \xD0\xB8 \xD1\x81\xD0\xBE\xD0\xB1\xD1\x8B\xD1\x82\xD0\xB8\xD1\x8F"), Toolbar);
    ActionsButton->setStyleSheet(
        "QPushButton { background:#40323e; color:#e4b9cd; border:1px solid #67505f; border-radius:3px; padding:5px 10px; }"
        "QPushButton:hover { background:#493b48; }");
    Toolbar->addWidget(ActionsButton);

    connect(PlayButton, &QPushButton::toggled, this, &EditorMainWindow::OnPlayToggled);
    connect(PauseButton, &QPushButton::clicked, this, &EditorMainWindow::OnPauseClicked);
    connect(StepButton, &QPushButton::clicked, this, &EditorMainWindow::OnStepClicked);
    connect(LayoutCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EditorMainWindow::OnLayoutModeChanged);
    connect(ActionsButton, &QPushButton::clicked, this, &EditorMainWindow::OnFocusActionsAndEvents);
}

void EditorMainWindow::BuildUi()
{
    auto* Root = new QWidget(this);
    Root->setObjectName("EditorRoot");
    auto* RootLayout = new QVBoxLayout(Root);
    RootLayout->setContentsMargins(0, 0, 0, 0);
    RootLayout->setSpacing(0);

    WorkspaceSplitter = new QSplitter(Qt::Horizontal, Root);
    WorkspaceSplitter->setHandleWidth(5);

    auto* HierarchyPanel = new QWidget(WorkspaceSplitter);
    HierarchyPanel->setObjectName("HierarchyPanel");
    HierarchyPanel->setMinimumWidth(180);
    auto* HierarchyLayout = new QVBoxLayout(HierarchyPanel);
    HierarchyLayout->setContentsMargins(0, 0, 0, 0);
    HierarchyLayout->setSpacing(0);

    HierarchyTabs = new QTabWidget(HierarchyPanel);
    auto* Tree = new HierarchyTreeWidget(HierarchyTabs);
    HierarchyTree = Tree;
    HierarchyTree->setHeaderHidden(true);
    HierarchyTree->setContextMenuPolicy(Qt::CustomContextMenu);
    HierarchyTree->setDragEnabled(true);
    HierarchyTree->setAcceptDrops(true);
    HierarchyTree->setDropIndicatorShown(true);
    HierarchyTree->setDefaultDropAction(Qt::CopyAction);
    HierarchyTree->setDragDropMode(QAbstractItemView::DragDrop);
    HierarchyTree->installEventFilter(this);
    Tree->ReparentRequested = [this](ObjectHandle Target, ObjectHandle NewParent)
    {
        return ReparentHierarchyObject(Target, NewParent);
    };
    HierarchyTabs->addTab(HierarchyTree, QString::fromUtf8("\xD0\x98\xD0\xB5\xD1\x80\xD0\xB0\xD1\x80\xD1\x85\xD0\xB8\xD1\x8F"));
    HierarchyLayout->addWidget(HierarchyTabs, 1);

    auto* CenterPanel = new QWidget(WorkspaceSplitter);
    auto* CenterLayout = new QVBoxLayout(CenterPanel);
    CenterLayout->setContentsMargins(0, 0, 0, 0);
    CenterLayout->setSpacing(0);

    CenterSplitter = new QSplitter(Qt::Vertical, CenterPanel);
    CenterSplitter->setHandleWidth(5);

    auto* ViewportPanel = new QWidget(CenterSplitter);
    ViewportPanel->setObjectName("ViewportPanel");
    auto* ViewportLayout = new QVBoxLayout(ViewportPanel);
    ViewportLayout->setContentsMargins(0, 0, 0, 0);
    ViewportLayout->setSpacing(0);

    PrimaryViewport = new EditorViewportWidget(ViewportPanel);
    PrimaryViewport->SetViewId(RenderViewId{1});
    PrimaryViewport->SetEngine(&BoundEngine);
    PrimaryViewport->SetEditorToolsEnabled(true);
    PrimaryViewport->installEventFilter(this);
    connect(PrimaryViewport, &EditorViewportWidget::NativeSurfaceChanged, this, &EditorMainWindow::OnViewportSurfaceChanged);
    connect(PrimaryViewport, &EditorViewportWidget::ViewportResized, this, &EditorMainWindow::OnViewportResized);
    connect(PrimaryViewport, &EditorViewportWidget::ObjectSelectionRequested, this, &EditorMainWindow::OnViewportSelectionRequested);
    connect(PrimaryViewport, &EditorViewportWidget::AssetDropped, this, &EditorMainWindow::OnAssetDropped);
    connect(PrimaryViewport, &EditorViewportWidget::CameraChanged, this, &EditorMainWindow::ConfigureRenderView);
    PrimaryViewport->SetTransformCallbacks(
        [this](const Transform& Value)
        {
            if (GameObject* Selected = GetSelectedGameObject())
            {
                Selected->SetTransform(Value);
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
            ExecuteCommand(MakeSetTransformCommand(EditScene, Selected->GetObjectHandle(), NewValue));
        });

    auto* ViewportHeader = new QFrame(ViewportPanel);
    ViewportHeader->setObjectName("ViewportHeader");
    auto* ViewportHeaderLayout = new QHBoxLayout(ViewportHeader);
    ViewportHeaderLayout->setContentsMargins(10, 4, 10, 4);
    ViewportModeLabel = new QLabel(QString::fromUtf8("Scene В· Edit World"), ViewportHeader);
    ViewportModeLabel->setObjectName("RoseLabel");
    ViewportHeaderLayout->addWidget(ViewportModeLabel);
    ViewportHeaderLayout->addStretch(1);

    GizmoModeButtons = new QButtonGroup(ViewportHeader);
    GizmoModeButtons->setExclusive(true);
    auto MakeGizmoModeButton = [&](const QString& Label, const QString& ToolTip, int ModeId)
    {
        auto* Button = new QPushButton(Label, ViewportHeader);
        Button->setObjectName("ViewportGizmoButton");
        Button->setCheckable(true);
        Button->setFixedHeight(24);
        Button->setMinimumWidth(28);
        Button->setToolTip(ToolTip);
        GizmoModeButtons->addButton(Button, ModeId);
        ViewportHeaderLayout->addWidget(Button);
        return Button;
    };
    TranslateGizmoButton = MakeGizmoModeButton(
        QStringLiteral("W"),
        tr("Translate gizmo (W)"),
        static_cast<int>(EditorViewportWidget::GizmoOperation::Translate));
    RotateGizmoButton = MakeGizmoModeButton(
        QStringLiteral("E"),
        tr("Rotate gizmo (E)"),
        static_cast<int>(EditorViewportWidget::GizmoOperation::Rotate));
    ScaleGizmoButton = MakeGizmoModeButton(
        QStringLiteral("R"),
        tr("Scale gizmo (R)"),
        static_cast<int>(EditorViewportWidget::GizmoOperation::Scale));
    TranslateGizmoButton->setChecked(true);
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

    ViewportHeaderLayout->addSpacing(8);
    ViewportHeaderLayout->addWidget(new QLabel(tr("Camera Speed"), ViewportHeader));
    CameraSpeedSpin = new QDoubleSpinBox(ViewportHeader);
    CameraSpeedSpin->setRange(0.25, 250.0);
    CameraSpeedSpin->setDecimals(2);
    CameraSpeedSpin->setSingleStep(0.5);
    CameraSpeedSpin->setValue(PrimaryViewport->GetCameraMoveSpeed());
    CameraSpeedSpin->setSuffix(tr(" u/s"));
    CameraSpeedSpin->setFixedWidth(110);
    CameraSpeedSpin->setToolTip(tr("Editor camera fly speed. Shift multiplies, Ctrl divides, RMB+wheel adjusts."));
    ViewportHeaderLayout->addWidget(CameraSpeedSpin);
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
    GameDialogue = new StoryWidget(StoryPlayback, ViewportPanel);
    ViewportLayout->addWidget(GameDialogue);
    GameDialogue->hide();

    auto* GraphDock = new QWidget(CenterSplitter);
    GraphDock->setObjectName("GraphDock");
    auto* GraphLayout = new QVBoxLayout(GraphDock);
    GraphLayout->setContentsMargins(0, 0, 0, 0);
    GraphLayout->setSpacing(0);
    DockTabs = new QTabWidget(GraphDock);
    GraphLayout->addWidget(DockTabs);

    CenterSplitter->addWidget(ViewportPanel);
    CenterSplitter->addWidget(GraphDock);
    CenterSplitter->setStretchFactor(0, 3);
    CenterSplitter->setStretchFactor(1, 2);
    CenterSplitter->setSizes({520, 280});
    CenterLayout->addWidget(CenterSplitter);

    auto* InspectorPanel = new QWidget(WorkspaceSplitter);
    InspectorPanel->setObjectName("InspectorPanel");
    InspectorPanel->setMinimumWidth(240);
    auto* InspectorLayout = new QVBoxLayout(InspectorPanel);
    InspectorLayout->setContentsMargins(0, 0, 0, 0);
    InspectorLayout->setSpacing(0);
    InspectorTabs = new QTabWidget(InspectorPanel);

    auto* ObjectInspectorPage = new QWidget(InspectorTabs);
    ObjectInspectorPage->setObjectName("ObjectInspectorPage");
    ObjectInspectorPage->setAttribute(Qt::WA_StyledBackground, true);
    auto* ObjectInspectorLayout = new QVBoxLayout(ObjectInspectorPage);
    ObjectInspectorLayout->setContentsMargins(8, 8, 8, 8);
    ObjectInspectorLayout->setSpacing(6);

    ObjectNameEdit = new QLineEdit(ObjectInspectorPage);
    ObjectNameEdit->setPlaceholderText(QString::fromUtf8("Object name"));
    ObjectInspectorLayout->addWidget(ObjectNameEdit);

    auto* TransformForm = new QFormLayout();
    auto MakeAxisRow = [&](QDoubleSpinBox*& XSpin, QDoubleSpinBox*& YSpin, QDoubleSpinBox*& ZSpin)
    {
        auto* Row = new QWidget(ObjectInspectorPage);
        auto* RowLayout = new QHBoxLayout(Row);
        RowLayout->setContentsMargins(0, 0, 0, 0);
        XSpin = new QDoubleSpinBox(Row);
        YSpin = new QDoubleSpinBox(Row);
        ZSpin = new QDoubleSpinBox(Row);
        for (QDoubleSpinBox* Spin : {XSpin, YSpin, ZSpin})
        {
            Spin->setDecimals(3);
            Spin->setRange(-1.0e6, 1.0e6);
            Spin->setSingleStep(0.1);
            RowLayout->addWidget(Spin);
        }
        return Row;
    };
    TransformForm->addRow(QString::fromUtf8("Position"), MakeAxisRow(PositionXSpin, PositionYSpin, PositionZSpin));
    TransformForm->addRow(QString::fromUtf8("Rotation"), MakeAxisRow(RotationXSpin, RotationYSpin, RotationZSpin));
    TransformForm->addRow(QString::fromUtf8("Scale"), MakeAxisRow(ScaleXSpin, ScaleYSpin, ScaleZSpin));
    ObjectInspectorLayout->addLayout(TransformForm);

    ComponentCombo = new QComboBox(ObjectInspectorPage);
    ObjectInspectorLayout->addWidget(ComponentCombo);

    auto* ComponentScrollArea = new QScrollArea(ObjectInspectorPage);
    ComponentScrollArea->setObjectName("ComponentInspectorScroll");
    ComponentScrollArea->setWidgetResizable(true);
    ComponentScrollArea->setFrameShape(QFrame::NoFrame);
    auto* ComponentInspectorContainer = new QWidget(ComponentScrollArea);
    ComponentInspectorContainer->setObjectName("ComponentInspectorList");
    ComponentInspectorContainer->setAttribute(Qt::WA_StyledBackground, true);
    ComponentInspectorLayout = new QVBoxLayout(ComponentInspectorContainer);
    ComponentInspectorLayout->setContentsMargins(0, 0, 0, 0);
    ComponentInspectorLayout->setSpacing(6);
    ComponentScrollArea->setWidget(ComponentInspectorContainer);
    ObjectInspectorLayout->addWidget(ComponentScrollArea, 1);

    InspectorTabs->addTab(ObjectInspectorPage, QString::fromUtf8("\xD0\x98\xD0\xBD\xD1\x81\xD0\xBF\xD0\xB5\xD0\xBA\xD1\x82\xD0\xBE\xD1\x80"));
    InspectorLayout->addWidget(InspectorTabs);

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

    WorkspaceSplitter->addWidget(InspectorPanel);
    WorkspaceSplitter->addWidget(CenterPanel);
    WorkspaceSplitter->addWidget(HierarchyPanel);
    WorkspaceSplitter->setStretchFactor(0, 0);
    WorkspaceSplitter->setStretchFactor(1, 1);
    WorkspaceSplitter->setStretchFactor(2, 0);
    WorkspaceSplitter->setSizes({320, 900, 240});

    RootLayout->addWidget(WorkspaceSplitter, 1);
    setCentralWidget(Root);

    StatusIssuesLabel = new QLabel(statusBar());
    StatusSelectionLabel = new QLabel(statusBar());
    statusBar()->addWidget(StatusIssuesLabel);
    statusBar()->addPermanentWidget(StatusSelectionLabel);

    connect(DockTabs, &QTabWidget::currentChanged, this, &EditorMainWindow::OnDockTabChanged);
    connect(HierarchyTree, &QTreeWidget::itemSelectionChanged, this, &EditorMainWindow::OnHierarchySelectionChanged);
    connect(HierarchyTree, &QTreeWidget::customContextMenuRequested, this, &EditorMainWindow::OnHierarchyContextMenu);
}

QWidget* EditorMainWindow::MakePlaceholderPage(const QString& Title, const QString& Description)
{
    auto* Page = new QWidget();
    auto* Layout = new QVBoxLayout(Page);
    Layout->setContentsMargins(16, 16, 16, 16);
    auto* TitleLabel = new QLabel(Title, Page);
    TitleLabel->setStyleSheet("color: #ead4df; font-size: 16px; font-weight: 600;");
    auto* Body = new QLabel(Description, Page);
    Body->setObjectName("MutedLabel");
    Body->setWordWrap(true);
    Body->setStyleSheet("color: #95909e;");
    Layout->addWidget(TitleLabel);
    Layout->addWidget(Body);
    Layout->addStretch(1);
    return Page;
}

void EditorMainWindow::BuildStoryDockPage()
{
    StoryPanel = new StoryWidget(StoryPlayback, DockTabs);
    StoryDockPage = StoryPanel;
    DockTabs->addTab(StoryDockPage, tr("Story"));
}

void EditorMainWindow::BuildRenderStatisticsDockPage()
{
    auto* Page = new QWidget(DockTabs);
    auto* Layout = new QVBoxLayout(Page);
    Layout->setContentsMargins(16, 16, 16, 16);
    Layout->setSpacing(8);
    auto* TitleLabel = new QLabel(QString::fromUtf8("\xD0\x9D\xD0\xB0\xD1\x81\xD1\x82\xD1\x80\xD0\xBE\xD0\xB9\xD0\xBA\xD0\xB8 \xD1\x81\xD1\x82\xD0\xB0\xD1\x82\xD0\xB8\xD1\x81\xD1\x82\xD0\xB8\xD0\xBA\xD0\xB8"), Page);
    TitleLabel->setStyleSheet("color: #ead4df; font-size: 16px; font-weight: 600;");
    Layout->addWidget(TitleLabel);
    RenderStatisticsLabel = new QLabel(Page);
    RenderStatisticsLabel->setWordWrap(true);
    RenderStatisticsLabel->setObjectName("MutedLabel");
    RenderStatisticsLabel->setStyleSheet("color: #95909e;");
    RenderStatisticsLabel->setText(tr("Waiting for render statisticsвЂ¦"));
    Layout->addWidget(RenderStatisticsLabel);
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
    TitleLabel->setStyleSheet("color: #ead4df; font-size: 16px; font-weight: 600;");
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

    auto* Antialiasing = new QCheckBox(tr("FXAA"), Page);
    Antialiasing->setChecked(RenderConfiguration.bAntialiasing);
    Form->addRow(QString(), Antialiasing);
    connect(Antialiasing, &QCheckBox::toggled, this, [this](bool bEnabled)
    {
        RenderConfiguration.bAntialiasing = bEnabled;
    });

    auto* MaterialEffects = new QCheckBox(tr("Preview Camera Material Effects"), Page);
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
    ContentBrowser = new ContentBrowserWidget(DockTabs);
    ContentBrowser->SetEngine(&BoundEngine);
    ContentBrowser->SetProjectContext(&BoundSession, &Document);
    connect(ContentBrowser, &ContentBrowserWidget::AssetsChanged, this, [this]()
    {
        UpdateWindowTitleDirty();
        UpdateStatus();
    });
    connect(ContentBrowser, &ContentBrowserWidget::AssetActivated, this, &EditorMainWindow::OnAssetActivated);
    DockTabs->addTab(ContentBrowser, tr("Content"));

    BuildStoryDockPage();

    struct DockPage
    {
        const char* TitleUtf8;
        const char* BodyUtf8;
    };

    const DockPage Pages[] = {
        {"\xD0\xA2\xD0\xB0\xD0\xB9\xD0\xBC\xD0\xBB\xD0\xB0\xD0\xB9\xD0\xBD", "Global story timeline / route map"},
        {"\xD0\xA1\xD0\xB0\xD0\xB1\xD1\x81\xD1\x86\xD0\xB5\xD0\xBD\xD1\x8B", "Subscene workspace and map"},
        {"\xD0\x9A\xD0\xB0\xD0\xBC\xD0\xB5\xD1\x80\xD1\x8B", "Camera library and framing"},
        {"\xD0\x9F\xD0\xBE\xD1\x81\xD1\x82\xD0\xB0\xD0\xBD\xD0\xBE\xD0\xB2\xD0\xBA\xD0\xB0", "Staging В· before / during / after line"},
        {"\xD0\xA1\xD0\xBE\xD0\xB1\xD1\x8B\xD1\x82\xD0\xB8\xD0\xB5", "Event В· action groups and actions"},
        {"\xD0\x9F\xD1\x80\xD0\xBE\xD0\xB5\xD0\xBA\xD1\x82", "Asset browser В· scenes / events / audio"},
        {"\xD0\x97\xD0\xB2\xD1\x83\xD0\xBA", "Sound workspace and meters"},
        {"\xD0\xAD\xD1\x84\xD1\x84\xD0\xB5\xD0\xBA\xD1\x82\xD1\x8B", "Effect samples playground"},
        {"\xD0\x9E\xD1\x88\xD0\xB8\xD0\xB1\xD0\xBA\xD0\xB8", "Validation issues and failure lab"},
    };

    for (const DockPage& Page : Pages)
    {
        DockTabs->addTab(
            MakePlaceholderPage(QString::fromUtf8(Page.TitleUtf8), QString::fromUtf8(Page.BodyUtf8)),
            QString::fromUtf8(Page.TitleUtf8));
    }
    BuildRenderStatisticsDockPage();
    BuildPostprocessDockPage();
}

void EditorMainWindow::OnDockTabChanged(int Index)
{
    if (Index >= 0)
    {
        statusBar()->showMessage(QString::fromUtf8("Dock: ") + DockTabs->tabText(Index), 1200);
    }
}

void EditorMainWindow::OnLayoutModeChanged(int Index)
{
    if (WorkspaceSplitter == nullptr || CenterSplitter == nullptr)
    {
        return;
    }

    const QString Mode = LayoutCombo->itemData(Index).toString();
    if (Mode == "graph")
    {
        CenterSplitter->setSizes({120, 680});
        for (int TabIndex = 0; TabIndex < DockTabs->count(); ++TabIndex)
        {
            if (DockTabs->tabText(TabIndex) == tr("Story"))
            {
                DockTabs->setCurrentIndex(TabIndex);
                break;
            }
        }
    }
    else if (Mode == "scene")
    {
        CenterSplitter->setSizes({700, 80});
    }
    else if (Mode == "hierarchy")
    {
        WorkspaceSplitter->setSizes({200, 500, 520});
    }
    else if (Mode == "inspector")
    {
        WorkspaceSplitter->setSizes({540, 500, 180});
    }
    else
    {
        WorkspaceSplitter->setSizes({320, 900, 240});
        CenterSplitter->setSizes({520, 280});
    }
}

