#include "EditorMainWindow.h"

#include "Actions/BuiltinEditorActions.h"
#include "EditorAction.h"
#include "EditorClipboard.h"
#include "EditorCommands.h"
#include "ContentBrowserWidget.h"
#include "EditorTheme.h"
#include "EditorViewportWidget.h"
#include "Engine.h"
#include "Game/PlaySession.h"
#include "Game/Scene.h"
#include "Game/SceneSerializer.h"
#include "Gameplay/CameraComponent.h"
#include "Gameplay/Component.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/LightComponent.h"
#include "Gameplay/MeshRendererComponent.h"
#include "Project/ProjectSession.h"
#include "ProjectBrowserDialog.h"
#include "ReflectionInspector.h"
#include "Core/Threading/RenderFrameData.h"
#include "Core/Threading/RenderThread.h"
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

namespace
{
constexpr int HierarchyObjectIdRole = Qt::UserRole;
constexpr int HierarchyObjectGenerationRole = Qt::UserRole + 1;
constexpr float DegreesToRadians = 0.01745329251994329577f;
constexpr float RadiansToDegrees = 57.295779513082320876f;

void StoreObjectHandle(QTreeWidgetItem* Item, ObjectHandle Handle)
{
    Item->setData(0, HierarchyObjectIdRole, QVariant::fromValue<qulonglong>(Handle.Id));
    Item->setData(0, HierarchyObjectGenerationRole, QVariant::fromValue<uint>(Handle.Generation));
}

ObjectHandle LoadObjectHandle(const QTreeWidgetItem* Item)
{
    ObjectHandle Handle;
    Handle.Id = static_cast<ObjectID>(Item->data(0, HierarchyObjectIdRole).toULongLong());
    Handle.Generation = Item->data(0, HierarchyObjectGenerationRole).toUInt();
    return Handle;
}

bool HierarchyItemIsDescendant(const QTreeWidgetItem* Ancestor, const QTreeWidgetItem* Candidate)
{
    for (const QTreeWidgetItem* Walk = Candidate; Walk != nullptr; Walk = Walk->parent())
    {
        if (Walk == Ancestor)
        {
            return true;
        }
    }
    return false;
}

class HierarchyTreeWidget : public QTreeWidget
{
public:
    using QTreeWidget::QTreeWidget;
    std::function<bool(ObjectHandle, ObjectHandle)> ReparentRequested;

protected:
    void dragEnterEvent(QDragEnterEvent* Event) override
    {
        if (Event->source() == this)
        {
            QTreeWidget::dragEnterEvent(Event);
            Event->setDropAction(Qt::CopyAction);
            Event->accept();
            return;
        }
        Event->ignore();
    }

    void dragMoveEvent(QDragMoveEvent* Event) override
    {
        if (Event->source() != this)
        {
            Event->ignore();
            return;
        }

        QTreeWidget::dragMoveEvent(Event);

        QTreeWidgetItem* Dragged = currentItem();
        QTreeWidgetItem* DropItem = itemAt(Event->position().toPoint());
        if (Dragged != nullptr
            && DropItem != nullptr
            && dropIndicatorPosition() == QAbstractItemView::OnItem
            && HierarchyItemIsDescendant(Dragged, DropItem))
        {
            Event->ignore();
            return;
        }

        Event->setDropAction(Qt::CopyAction);
        Event->accept();
    }

    void dropEvent(QDropEvent* Event) override
    {
        QTreeWidgetItem* Dragged = currentItem();
        if (Dragged == nullptr || !ReparentRequested || Event->source() != this)
        {
            Event->ignore();
            return;
        }

        QTreeWidgetItem* DropItem = itemAt(Event->position().toPoint());
        ObjectHandle NewParent;
        const QAbstractItemView::DropIndicatorPosition DropPosition = dropIndicatorPosition();
        if (DropItem != nullptr && DropPosition == QAbstractItemView::OnItem)
        {
            if (HierarchyItemIsDescendant(Dragged, DropItem) || DropItem == Dragged)
            {
                Event->setDropAction(Qt::IgnoreAction);
                Event->accept();
                return;
            }
            NewParent = LoadObjectHandle(DropItem);
        }
        else if (DropItem != nullptr
            && DropItem != Dragged
            && DropItem->parent() != nullptr
            && (DropPosition == QAbstractItemView::AboveItem
                || DropPosition == QAbstractItemView::BelowItem))
        {
            NewParent = LoadObjectHandle(DropItem->parent());
        }

        const ObjectHandle Target = LoadObjectHandle(Dragged);
        ReparentRequested(Target, NewParent);
        Event->setDropAction(Qt::IgnoreAction);
        Event->accept();
    }
};

Vector3 QuaternionToEulerDegrees(const Quaternion& Rotation)
{
    const float PitchSin = 2.0f * (Rotation.w * Rotation.y - Rotation.z * Rotation.x);
    return Vector3(
        std::atan2(
            2.0f * (Rotation.w * Rotation.x + Rotation.y * Rotation.z),
            1.0f - 2.0f * (Rotation.x * Rotation.x + Rotation.y * Rotation.y)) * RadiansToDegrees,
        std::asin(std::clamp(PitchSin, -1.0f, 1.0f)) * RadiansToDegrees,
        std::atan2(
            2.0f * (Rotation.w * Rotation.z + Rotation.x * Rotation.y),
            1.0f - 2.0f * (Rotation.y * Rotation.y + Rotation.z * Rotation.z)) * RadiansToDegrees);
}

bool RayIntersectsBounds(
    const Vector3& RayOrigin,
    const Vector3& RayDirection,
    const AxisAlignedBounds& Bounds,
    float& OutDistance)
{
    float MinimumDistance = 0.0f;
    float MaximumDistance = std::numeric_limits<float>::max();
    auto TestAxis = [&](float Origin, float Direction, float Minimum, float Maximum)
    {
        if (std::abs(Direction) < 0.000001f)
        {
            return Origin >= Minimum && Origin <= Maximum;
        }
        float FirstDistance = (Minimum - Origin) / Direction;
        float SecondDistance = (Maximum - Origin) / Direction;
        if (FirstDistance > SecondDistance)
        {
            std::swap(FirstDistance, SecondDistance);
        }
        MinimumDistance = std::max(MinimumDistance, FirstDistance);
        MaximumDistance = std::min(MaximumDistance, SecondDistance);
        return MinimumDistance <= MaximumDistance;
    };

    if (!TestAxis(RayOrigin.x, RayDirection.x, Bounds.Minimum.x, Bounds.Maximum.x)
        || !TestAxis(RayOrigin.y, RayDirection.y, Bounds.Minimum.y, Bounds.Maximum.y)
        || !TestAxis(RayOrigin.z, RayDirection.z, Bounds.Minimum.z, Bounds.Maximum.z))
    {
        return false;
    }
    OutDistance = MinimumDistance;
    return MaximumDistance >= 0.0f;
}
}

EditorMainWindow::EditorMainWindow(Engine& InEngine, ProjectSession& InSession, QWidget* Parent)
    : QMainWindow(Parent)
    , BoundEngine(InEngine)
    , BoundSession(InSession)
    , StoryPlayback(InEngine.GetStoryRuntime())
{
    setObjectName("EditorMainWindow");
    setWindowTitle("Sacura Novel Studio");
    resize(1440, 900);
    setStyleSheet(EditorTheme::Stylesheet());

    Document.BindEngine(&BoundEngine);
    CommandStack.BindDocument(&Document);
    BindDocumentFromSession();

    BuildMenus();
    BuildToolbar();
    BuildUi();
    PopulateHierarchy();
    PopulateDockPages();
    RegisterBuiltInAssetEditors();
    RefreshProjectTitle();
    UpdateStatus();

    MaintenanceClock = new QElapsedTimer();
    MaintenanceClock->start();
    MaintenanceTimer = new QTimer(this);
    connect(MaintenanceTimer, &QTimer::timeout, this, &EditorMainWindow::OnMaintenanceTick);
    MaintenanceTimer->start(16);
}

EditorMainWindow::~EditorMainWindow()
{
    MaintenanceTimer->stop();
    BoundEngine.StopPresenting();
    ClearSelection();
    CommandStack.Clear();
    CommandStack.BindDocument(nullptr);
    Document.Close();
    delete MaintenanceClock;
}

void EditorMainWindow::BindDocumentFromSession()
{
    const ProjectDescriptor* Descriptor = BoundSession.GetProject();
    Scene* ActiveScene = BoundEngine.GetActiveScene();
    if (Descriptor == nullptr || ActiveScene == nullptr || Descriptor->StartupScene.empty())
    {
        return;
    }

    std::string Error;
    if (!Document.AdoptScene(ActiveScene, Descriptor->StartupScene, Error))
    {
        PrintString(std::string("EditorMainWindow: failed to adopt startup scene: ") + Error);
    }
}

bool EditorMainWindow::CaptureScreenshot(const QString& OutputFile)
{
    show();
    raise();
    activateWindow();
    QApplication::processEvents();
    return grab().save(OutputFile, "PNG");
}

bool EditorMainWindow::RegisterAssetEditor(const AssetType& Type, AssetEditorHandler Handler)
{
    if (!Type.IsValid() || !Handler || AssetEditors.find(Type) != AssetEditors.end())
    {
        return false;
    }
    AssetEditors.emplace(Type, std::move(Handler));
    return true;
}

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
    ViewportModeLabel = new QLabel(QString::fromUtf8("Scene · Edit World"), ViewportHeader);
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

void EditorMainWindow::PopulateHierarchy()
{
    HierarchyTree->blockSignals(true);
    HierarchyTree->clear();

    Scene* HierarchyScene = GetHierarchyScene();
    if (HierarchyScene != nullptr)
    {
        for (GameObject* RootObject : HierarchyScene->GetRootObjects())
        {
            AddHierarchyItem(nullptr, RootObject);
        }
    }

    HierarchyTree->expandAll();
    HierarchyTree->blockSignals(false);

    if (SelectedObject.IsValid() && GetSelectedGameObject() != nullptr)
    {
        SelectObject(SelectedObject);
    }
    else
    {
        ClearSelection();
    }
}

bool EditorMainWindow::ReparentHierarchyObject(ObjectHandle Target, ObjectHandle NewParent)
{
    Scene* EditScene = GetEditScene();
    if (EditScene == nullptr || BoundEngine.GetPlaySession().IsSimulating() || !Target.IsValid())
    {
        PopulateHierarchy();
        return false;
    }

    GameObject* ObjectInstance = EditScene->FindByHandle(Target);
    if (ObjectInstance == nullptr)
    {
        PopulateHierarchy();
        return false;
    }

    ObjectHandle CurrentParent;
    if (ObjectInstance->GetParent() != nullptr)
    {
        CurrentParent = ObjectInstance->GetParent()->GetObjectHandle();
    }
    if (CurrentParent == NewParent)
    {
        PopulateHierarchy();
        return true;
    }

    if (!ExecuteCommand(MakeReparentObjectCommand(EditScene, Target, NewParent)))
    {
        PopulateHierarchy();
        return false;
    }
    return true;
}

void EditorMainWindow::AddHierarchyItem(QTreeWidgetItem* ParentItem, GameObject* ObjectInstance)
{
    if (ObjectInstance == nullptr)
    {
        return;
    }

    QTreeWidgetItem* Item = nullptr;
    if (ParentItem != nullptr)
    {
        Item = new QTreeWidgetItem(ParentItem, {QString::fromStdString(ObjectInstance->GetName())});
    }
    else
    {
        Item = new QTreeWidgetItem(HierarchyTree, {QString::fromStdString(ObjectInstance->GetName())});
    }
    StoreObjectHandle(Item, ObjectInstance->GetObjectHandle());
    if (!BoundEngine.GetPlaySession().IsSimulating())
    {
        Item->setFlags(Item->flags() | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    }

    for (GameObject* Child : ObjectInstance->GetChildren())
    {
        AddHierarchyItem(Item, Child);
    }
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
    RenderStatisticsLabel->setText(tr("Waiting for render statistics…"));
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

    Layout->addLayout(Form);
    Layout->addStretch(1);
    DockTabs->addTab(Page, QString::fromUtf8("\xD0\x9D\xD0\xB0\xD1\x81\xD1\x82\xD1\x80\xD0\xBE\xD0\xB9\xD0\xBA\xD0\xB8 \xD0\xBF\xD0\xBE\xD1\x81\xD1\x82\xD0\xBE\xD0\xB1\xD1\x80\xD0\xB0\xD0\xB1\xD0\xBE\xD1\x82\xD0\xBA\xD0\xB8"));
}

void EditorMainWindow::PopulateDockPages()
{
    ContentBrowser = new ContentBrowserWidget(DockTabs);
    ContentBrowser->SetEngine(&BoundEngine);
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
        {"\xD0\x9F\xD0\xBE\xD1\x81\xD1\x82\xD0\xB0\xD0\xBD\xD0\xBE\xD0\xB2\xD0\xBA\xD0\xB0", "Staging · before / during / after line"},
        {"\xD0\xA1\xD0\xBE\xD0\xB1\xD1\x8B\xD1\x82\xD0\xB8\xD0\xB5", "Event · action groups and actions"},
        {"\xD0\x9F\xD1\x80\xD0\xBE\xD0\xB5\xD0\xBA\xD1\x82", "Asset browser · scenes / events / audio"},
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

void EditorMainWindow::RegisterBuiltInAssetEditors()
{
    RegisterAssetEditor(SceneAssetType, [this](
        const AssetKey&,
        const AssetRegistryEntry& Entry,
        const QString& VirtualPath)
    {
        if (BoundEngine.GetPlaySession().IsSimulating())
        {
            statusBar()->showMessage(tr("Stop Play before opening another scene"), 2500);
            return;
        }
        if (!PromptSaveIfDirty())
        {
            return;
        }

        std::string Error;
        if (!Document.Open(Entry.AbsolutePath, Error))
        {
            QMessageBox::warning(
                this,
                tr("Open Scene"),
                QString::fromStdString(Error.empty() ? "Failed to open scene" : Error));
            return;
        }

        ClearSelection();
        CommandStack.Clear();
        StoryPlayback.BindScene(Document.GetScene());
        PopulateHierarchy();
        ConfigureRenderView();
        UpdateWindowTitleDirty();
        UpdateStatus();
        statusBar()->showMessage(tr("Opened scene %1").arg(VirtualPath), 2500);
    });

    RegisterAssetEditor(StoryAssetType, [this](
        const AssetKey&,
        const AssetRegistryEntry& Entry,
        const QString& VirtualPath)
    {
        StoryPlayback.BindScene(GetEditScene());
        if (!StoryPlayback.LoadFromFile(Entry.AbsolutePath))
        {
            QMessageBox::warning(
                this,
                tr("Open Story"),
                QString::fromStdString(StoryPlayback.GetLastError().empty()
                    ? "Failed to open story"
                    : StoryPlayback.GetLastError()));
            return;
        }
        for (int TabIndex = 0; TabIndex < DockTabs->count(); ++TabIndex)
        {
            if (DockTabs->widget(TabIndex) == StoryDockPage)
            {
                DockTabs->setCurrentIndex(TabIndex);
                break;
            }
        }
        RefreshStoryPlaybackUi();
        statusBar()->showMessage(tr("Opened story %1").arg(VirtualPath), 2500);
    });
}

void EditorMainWindow::RefreshProjectTitle()
{
    const ProjectDescriptor* Descriptor = BoundSession.GetProject();
    if (Descriptor == nullptr)
    {
        ProjectTitleLabel->setText(QString::fromUtf8("\xD0\x9D\xD0\xB5\xD1\x82 \xD0\xBF\xD1\x80\xD0\xBE\xD0\xB5\xD0\xBA\xD1\x82\xD0\xB0"));
        setWindowTitle("Sacura Novel Studio");
        return;
    }

    ProjectTitleLabel->setText(QString::fromUtf8("\xF0\x9F\x93\x82  ") + QString::fromStdString(Descriptor->Name));
    UpdateWindowTitleDirty();
}

void EditorMainWindow::UpdateWindowTitleDirty()
{
    const ProjectDescriptor* Descriptor = BoundSession.GetProject();
    QString Title = "Sacura Novel Studio";
    if (Descriptor != nullptr)
    {
        Title = QString("Sacura Novel Studio — %1").arg(QString::fromStdString(Descriptor->Name));
    }
    if (Document.IsDirty())
    {
        Title += " *";
    }
    setWindowTitle(Title);
}

void EditorMainWindow::UpdateStatus()
{
    const int IssueCount = BoundSession.GetIssueCount();
    QString IssuesText = QString::fromUtf8("\xD0\x9E\xD1\x88\xD0\xB8\xD0\xB1\xD0\xBA\xD0\xB8: %1").arg(IssueCount);
    if (BoundSession.GetHealth() == ProjectSessionHealth::Degraded)
    {
        IssuesText += QString::fromUtf8(" (degraded)");
    }
    else if (BoundSession.GetHealth() == ProjectSessionHealth::Failed)
    {
        IssuesText += QString::fromUtf8(" (failed)");
    }
    if (Document.IsDirty())
    {
        IssuesText += QString::fromUtf8(" · dirty");
    }
    StatusIssuesLabel->setText(IssuesText);

    if (GameObject* Selected = GetSelectedGameObject())
    {
        StatusSelectionLabel->setText(
            QString::fromUtf8("\xD0\x92\xD1\x8B\xD0\xB4\xD0\xB5\xD0\xBB\xD0\xB5\xD0\xBD\xD0\xBE: ")
            + QString::fromStdString(Selected->GetName()));
    }
    else
    {
        StatusSelectionLabel->setText(QString::fromUtf8("\xD0\x9D\xD0\xB5\xD1\x82 \xD0\xB2\xD1\x8B\xD0\xB4\xD0\xB5\xD0\xBB\xD0\xB5\xD0\xBD\xD0\xB8\xD1\x8F"));
    }
}

void EditorMainWindow::OnPlayToggled(bool bChecked)
{
    PlaySession& Session = BoundEngine.GetPlaySession();
    if (bChecked)
    {
        if (!Session.StartPlay())
        {
            PlayButton->blockSignals(true);
            PlayButton->setChecked(false);
            PlayButton->blockSignals(false);
            statusBar()->showMessage(QString::fromUtf8("Play failed — check log"), 2500);
            UpdateStatus();
            return;
        }
        StartStoryPlayback();
    }
    else
    {
        StopStoryPlayback();
        Session.StopPlay();
    }

    bPlaying = Session.IsSimulating();
    bPaused = Session.GetState() == PlaySessionState::Paused;
    PauseButton->setChecked(bPaused);
    PauseButton->setEnabled(bPlaying);
    StepButton->setEnabled(bPlaying && bPaused);
    ModeLabel->setText(bPlaying
        ? QString::fromUtf8("\xD0\x9F\xD1\x80\xD0\xB5\xD0\xB4\xD0\xBF\xD1\x80\xD0\xBE\xD1\x81\xD0\xBC\xD0\xBE\xD1\x82\xD1\x80")
        : QString::fromUtf8("\xD0\xA0\xD0\xB5\xD0\xB4\xD0\xB0\xD0\xBA\xD1\x82\xD0\xB8\xD1\x80\xD0\xBE\xD0\xB2\xD0\xB0\xD0\xBD\xD0\xB8\xD0\xB5"));
    PlayButton->setText(bPlaying ? QString::fromUtf8("\xE2\x96\xA0") : QString::fromUtf8("\xE2\x96\xB6"));
    PrimaryViewport->SetEditorToolsEnabled(!bPlaying);
    if (CameraSpeedSpin != nullptr)
    {
        CameraSpeedSpin->setEnabled(!bPlaying);
    }
    if (TranslateGizmoButton != nullptr)
    {
        TranslateGizmoButton->setEnabled(!bPlaying);
    }
    if (RotateGizmoButton != nullptr)
    {
        RotateGizmoButton->setEnabled(!bPlaying);
    }
    if (ScaleGizmoButton != nullptr)
    {
        ScaleGizmoButton->setEnabled(!bPlaying);
    }
    ViewportModeLabel->setText(bPlaying
        ? QString::fromUtf8("Game · Play World")
        : QString::fromUtf8("Scene · Edit World"));
    GameDialogue->setVisible(bPlaying);
    ClearSelection();
    PopulateHierarchy();
    ConfigureRenderView();
    if (bPlaying)
    {
        statusBar()->showMessage(QString::fromUtf8("Play mode — hierarchy edits disabled"), 3000);
    }
    RefreshStoryPlaybackUi();
    UpdateStatus();
}

void EditorMainWindow::OnPauseClicked()
{
    PlaySession& Session = BoundEngine.GetPlaySession();
    if (!Session.IsSimulating())
    {
        return;
    }
    if (Session.GetState() == PlaySessionState::Paused)
    {
        Session.Resume();
    }
    else
    {
        Session.Pause();
    }
    bPaused = Session.GetState() == PlaySessionState::Paused;
    PauseButton->setChecked(bPaused);
    StepButton->setEnabled(bPaused);
    RefreshStoryPlaybackUi();
    ModeLabel->setText(bPaused
        ? QString::fromUtf8("\xD0\x9D\xD0\xB0 \xD0\xBF\xD0\xB0\xD1\x83\xD0\xB7\xD0\xB5")
        : QString::fromUtf8("\xD0\x9F\xD1\x80\xD0\xB5\xD0\xB4\xD0\xBF\xD1\x80\xD0\xBE\xD1\x81\xD0\xBC\xD0\xBE\xD1\x82\xD1\x80"));
}

void EditorMainWindow::OnStepClicked()
{
    PlaySession& Session = BoundEngine.GetPlaySession();
    if (Session.GetState() != PlaySessionState::Paused)
    {
        return;
    }
    Session.StepFrame(1.0f / 60.0f);
    RefreshStoryPlaybackUi();
    statusBar()->showMessage(tr("Step frame"), 1500);
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

bool EditorMainWindow::LaunchEditorProcessForProject(const std::filesystem::path& ProjectFile)
{
    const QString Program = QCoreApplication::applicationFilePath();
    QStringList Arguments;
    Arguments << "--project" << QString::fromStdString(ProjectFile.generic_string());
    const bool bStarted = QProcess::startDetached(Program, Arguments);
    if (!bStarted)
    {
        QMessageBox::warning(
            this,
            "Open Project",
            QString::fromUtf8("\xD0\x9D\xD0\xB5 \xD1\x83\xD0\xB4\xD0\xB0\xD0\xBB\xD0\xBE\xD1\x81\xD1\x8C \xD0\xB7\xD0\xB0\xD0\xBF\xD1\x83\xD1\x81\xD1\x82\xD0\xB8\xD1\x82\xD1\x8C \xD0\xBD\xD0\xBE\xD0\xB2\xD1\x8B\xD0\xB9 \xD0\xBF\xD1\x80\xD0\xBE\xD1\x86\xD0\xB5\xD1\x81\xD1\x81 \xD1\x80\xD0\xB5\xD0\xB4\xD0\xB0\xD0\xBA\xD1\x82\xD0\xBE\xD1\x80\xD0\xB0"));
        PrintString("EditorMainWindow: failed to launch editor process for another project");
    }
    return bStarted;
}

void EditorMainWindow::OnNewProject()
{
    ProjectBrowserDialog Browser(this);
    if (Browser.exec() != QDialog::Accepted)
    {
        return;
    }

    const std::filesystem::path Selected = Browser.SelectedProjectFile();
    if (BoundSession.IsOpen())
    {
        LaunchEditorProcessForProject(Selected);
        return;
    }

    if (BoundSession.OpenProject(Selected))
    {
        RefreshProjectTitle();
        UpdateStatus();
        return;
    }

    QMessageBox::warning(
        this,
        "New Project",
        QString::fromStdString(BoundSession.GetLastError().empty()
            ? "Failed to open project"
            : BoundSession.GetLastError()));
}

void EditorMainWindow::OnOpenProject()
{
    const QString Selected = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8("\xD0\x9E\xD1\x82\xD0\xBA\xD1\x80\xD1\x8B\xD1\x82\xD1\x8C \xD0\xBF\xD1\x80\xD0\xBE\xD0\xB5\xD0\xBA\xD1\x82"),
        QString(),
        "Sakura Project (*.project)");
    if (Selected.isEmpty())
    {
        return;
    }

    const std::filesystem::path ProjectFile = Selected.toStdString();
    if (BoundSession.IsOpen())
    {
        LaunchEditorProcessForProject(ProjectFile);
        return;
    }

    if (BoundSession.OpenProject(ProjectFile))
    {
        RefreshProjectTitle();
        UpdateStatus();
        return;
    }

    QMessageBox::warning(
        this,
        "Open Project",
        QString::fromStdString(BoundSession.GetLastError().empty()
            ? "Failed to open project"
            : BoundSession.GetLastError()));
}

void EditorMainWindow::OnSaveScene()
{
    if (!Document.IsOpen())
    {
        QMessageBox::warning(this, "Save Scene", "No scene document is open");
        return;
    }

    std::string Error;
    if (!Document.Save(Error))
    {
        QMessageBox::warning(
            this,
            "Save Scene",
            QString::fromStdString(Error.empty() ? "Failed to save scene" : Error));
        UpdateWindowTitleDirty();
        return;
    }

    UpdateWindowTitleDirty();
    statusBar()->showMessage(QString::fromUtf8("Scene saved"), 2000);
}

void EditorMainWindow::OnMaintenanceTick()
{
    float DeltaTime = 0.016f;
    if (MaintenanceClock != nullptr)
    {
        DeltaTime = static_cast<float>(MaintenanceClock->restart()) / 1000.0f;
        if (DeltaTime <= 0.0f)
        {
            DeltaTime = 0.016f;
        }
        if (DeltaTime > 0.25f)
        {
            DeltaTime = 0.25f;
        }
    }

    ConfigureRenderView();
    BoundEngine.Tick(DeltaTime);
    RefreshStoryPlaybackUi();
    UpdateStatus();
}

void EditorMainWindow::OnViewportSurfaceChanged(RenderViewportWidget* Viewport)
{
    if (!Viewport->HasValidNativeHandle())
    {
        BoundEngine.UnregisterRenderSurface(RenderSurfaceId{Viewport->GetViewId().Value});
        return;
    }
    EnsurePresenting(Viewport);
}

void EditorMainWindow::OnViewportResized(RenderViewportWidget* Viewport)
{
    if (Viewport == nullptr || !BoundEngine.IsPresenting())
    {
        return;
    }

    const NativeWindowInfo Info = Viewport->BuildNativeWindowInfo();
    BoundEngine.ResizeRenderSurface(RenderSurfaceId{Viewport->GetViewId().Value}, Info.Width, Info.Height);
}

void EditorMainWindow::OnViewportSelectionRequested(QPoint Position)
{
    Scene* EditScene = GetEditScene();
    if (EditScene == nullptr
        || PrimaryViewport == nullptr
        || BoundEngine.GetPlaySession().IsSimulating())
    {
        return;
    }

    const RenderViewCamera& Camera = PrimaryViewport->GetViewCamera();
    Vector3 Forward = Camera.Target - Camera.Position;
    if (Forward.LengthSquared() <= 0.000001f)
    {
        return;
    }
    Forward.Normalize();
    Vector3 Right = Forward.Cross(Camera.Up);
    if (Right.LengthSquared() <= 0.000001f)
    {
        return;
    }
    Right.Normalize();
    Vector3 Up = Right.Cross(Forward);
    Up.Normalize();

    const float ViewportWidth = static_cast<float>(std::max(1, PrimaryViewport->width()));
    const float ViewportHeight = static_cast<float>(std::max(1, PrimaryViewport->height()));
    const float NormalizedX = static_cast<float>(Position.x()) / ViewportWidth * 2.0f - 1.0f;
    const float NormalizedY = 1.0f - static_cast<float>(Position.y()) / ViewportHeight * 2.0f;
    const float HalfVerticalField = std::tan(Camera.FieldOfViewDegrees * DegreesToRadians * 0.5f);
    Vector3 RayDirection = Forward
        + Right * (NormalizedX * HalfVerticalField * ViewportWidth / ViewportHeight)
        + Up * (NormalizedY * HalfVerticalField);
    RayDirection.Normalize();

    GameObject* ClosestObject = nullptr;
    float ClosestDistance = std::numeric_limits<float>::max();
    for (GameObject* Candidate : EditScene->GetAllObjects())
    {
        if (Candidate == nullptr || !Candidate->IsActiveInHierarchy())
        {
            continue;
        }
        MeshRendererComponent* MeshRenderer = Candidate->GetComponent<MeshRendererComponent>();
        if (MeshRenderer == nullptr || !MeshRenderer->bVisible)
        {
            continue;
        }

        float HitDistance = 0.0f;
        const AxisAlignedBounds WorldBounds = MeshRenderer->LocalBounds.TransformedBy(Candidate->GetWorldMatrix());
        if (RayIntersectsBounds(Camera.Position, RayDirection, WorldBounds, HitDistance)
            && HitDistance < ClosestDistance)
        {
            ClosestDistance = HitDistance;
            ClosestObject = Candidate;
        }
    }

    if (ClosestObject != nullptr)
    {
        SelectObject(ClosestObject->GetObjectHandle());
    }
    else
    {
        HierarchyTree->clearSelection();
        ClearSelection();
        UpdateStatus();
    }
}

void EditorMainWindow::EnsurePresenting(RenderViewportWidget* Viewport)
{
    if (Viewport == nullptr || !Viewport->HasValidNativeHandle() || !Viewport->isVisible())
    {
        return;
    }
    const NativeWindowInfo Info = Viewport->BuildNativeWindowInfo();
    if (!BoundEngine.RegisterRenderSurface(RenderSurfaceId{Viewport->GetViewId().Value}, Info))
    {
        PrintString("EditorMainWindow: failed to attach Qt render surface");
    }
}

void EditorMainWindow::ConfigureRenderView()
{
    PrimaryViewport->SyncGizmoOverlay();
    EnsurePresenting(PrimaryViewport);
    if (BoundEngine.GetPlaySession().IsSimulating())
    {
        BoundEngine.ConfigureRenderSurface(RenderSurfaceId{1}, false, RenderCamera{}, RenderConfiguration);
    }
    else
    {
        const NativeWindowInfo Info = PrimaryViewport->BuildNativeWindowInfo();
        RenderCamera Camera;
        PrimaryViewport->GetViewCamera().BuildRenderCamera(
            static_cast<float>(qMax(1u, Info.Width)) / static_cast<float>(qMax(1u, Info.Height)), Camera);
        BoundEngine.ConfigureRenderSurface(RenderSurfaceId{1}, true, Camera, RenderConfiguration);
    }
    const RenderStatistics Statistics = BoundEngine.GetRenderStatistics(RenderSurfaceId{1});
    QString Timings = tr("GPU timings pending or unsupported");
    if (Statistics.bGpuTimingsAvailable)
    {
        Timings = tr("GPU: shadow %1 / opaque %2 / transparency %3 / post %4 ms")
            .arg(Statistics.ShadowMilliseconds, 0, 'f', 2).arg(Statistics.OpaqueMilliseconds, 0, 'f', 2)
            .arg(Statistics.TransparencyMilliseconds, 0, 'f', 2).arg(Statistics.PostprocessMilliseconds, 0, 'f', 2);
    }
    if (RenderStatisticsLabel != nullptr)
    {
        RenderStatisticsLabel->setText(tr("%1: %2 visible, %3 culled, %4 draws, CPU %5 ms. %6")
            .arg(bPlaying ? tr("Game") : tr("Scene"))
            .arg(Statistics.VisibleObjects).arg(Statistics.CulledObjects).arg(Statistics.DrawCalls)
            .arg(Statistics.CpuMilliseconds, 0, 'f', 2).arg(Timings));
    }
}

void EditorMainWindow::OnFocusActionsAndEvents()
{
    const QString EventTabTitle = QString::fromUtf8("\xD0\xA1\xD0\xBE\xD0\xB1\xD1\x8B\xD1\x82\xD0\xB8\xD0\xB5");
    for (int TabIndex = 0; TabIndex < DockTabs->count(); ++TabIndex)
    {
        if (DockTabs->tabText(TabIndex) == EventTabTitle)
        {
            DockTabs->setCurrentIndex(TabIndex);
            break;
        }
    }
    LayoutCombo->setCurrentIndex(1);
}

Scene* EditorMainWindow::GetEditScene() const
{
    return Document.GetScene();
}

Scene* EditorMainWindow::GetHierarchyScene() const
{
    if (BoundEngine.GetPlaySession().IsSimulating())
    {
        return BoundEngine.GetPlaySession().GetPlayWorld();
    }
    return GetEditScene();
}

bool EditorMainWindow::ExecuteCommand(std::unique_ptr<EditorCommand> Command)
{
    if (BoundEngine.GetPlaySession().IsSimulating())
    {
        statusBar()->showMessage(QString::fromUtf8("Hierarchy edits disabled during play"), 2000);
        return false;
    }
    const bool bOk = CommandStack.Execute(std::move(Command));
    if (bOk)
    {
        PopulateHierarchy();
        RefreshSelectionUi();
        UpdateWindowTitleDirty();
        UpdateStatus();
    }
    return bOk;
}

void EditorMainWindow::ClearSelection()
{
    SelectedObject = ObjectHandle{};
    ComponentFilter.clear();
    RefreshSelectionUi();
}

void EditorMainWindow::SelectObject(ObjectHandle Target)
{
    SelectedObject = Target;
    QTreeWidgetItemIterator Iterator(HierarchyTree);
    while (*Iterator != nullptr)
    {
        if (LoadObjectHandle(*Iterator) == Target)
        {
            HierarchyTree->setCurrentItem(*Iterator);
            break;
        }
        ++Iterator;
    }
    RefreshSelectionUi();
    UpdateStatus();
}

ObjectHandle EditorMainWindow::GetSelectedObjectHandle() const
{
    return SelectedObject;
}

GameObject* EditorMainWindow::GetSelectedGameObject() const
{
    Scene* HierarchyScene = GetHierarchyScene();
    if (HierarchyScene == nullptr || !SelectedObject.IsValid())
    {
        return nullptr;
    }
    return HierarchyScene->FindByHandle(SelectedObject);
}

void EditorMainWindow::ConfigureComponentInspector(ReflectionInspector* ComponentInspector)
{
    ComponentInspector->SetAssetRegistry(&BoundEngine.GetAssetRegistry());
    ComponentInspector->SetTypeLabelVisible(false);
    ComponentInspector->SetPropertyCommitCallback([this](
        Object* Instance,
        const PropertyId& Property,
        const ReflectedValue& NewValue)
    {
        if (Instance == nullptr || GetEditScene() == nullptr || BoundEngine.GetPlaySession().IsSimulating())
        {
            return false;
        }
        if (!CommandStack.Execute(MakeSetPropertyCommand(
            GetEditScene(),
            Instance->GetObjectHandle(),
            Property,
            NewValue)))
        {
            return false;
        }
        UpdateWindowTitleDirty();
        UpdateStatus();
        return true;
    });
    ComponentInspector->SetPropertyResetCallback([this](Object* Instance, const PropertyId& Property)
    {
        if (Instance == nullptr || GetEditScene() == nullptr || BoundEngine.GetPlaySession().IsSimulating())
        {
            return false;
        }
        if (!CommandStack.Execute(MakeResetPropertyCommand(
            GetEditScene(),
            Instance->GetObjectHandle(),
            Property)))
        {
            return false;
        }
        UpdateWindowTitleDirty();
        UpdateStatus();
        return true;
    });
    ComponentInspector->SetAssetCommitCallback([this](
        Object* Instance,
        const PropertyId& Property,
        const PropertyId& CompanionProperty,
        const AssetKey& Key)
    {
        if (Instance == nullptr || GetEditScene() == nullptr)
        {
            return false;
        }
        CommandStack.BeginGroup("Assign Asset");
        bool bResult = CommandStack.Execute(MakeSetPropertyCommand(
            GetEditScene(),
            Instance->GetObjectHandle(),
            Property,
            ReflectedValue::MakeString(Key.IsValid() ? Key.Asset.ToString() : std::string{})));
        if (bResult && !CompanionProperty.Value.empty())
        {
            bResult = CommandStack.Execute(MakeSetPropertyCommand(
                GetEditScene(),
                Instance->GetObjectHandle(),
                CompanionProperty,
                ReflectedValue::MakeString(Key.HasSubAsset() ? Key.SubAsset->ToString() : std::string{})));
        }
        CommandStack.EndGroup();
        if (bResult)
        {
            RefreshSelectionUi();
            UpdateWindowTitleDirty();
            UpdateStatus();
        }
        return bResult;
    });
    connect(ComponentInspector, &ReflectionInspector::PropertyChanged, this, [this]()
    {
        UpdateWindowTitleDirty();
    });
}

void EditorMainWindow::RebuildComponentInspectors()
{
    if (ComponentCombo == nullptr || ComponentInspectorLayout == nullptr)
    {
        return;
    }

    while (QLayoutItem* Item = ComponentInspectorLayout->takeAt(0))
    {
        if (QWidget* ItemWidget = Item->widget())
        {
            ItemWidget->deleteLater();
        }
        delete Item;
    }
    ComponentInspectors.clear();

    GameObject* Selected = GetSelectedGameObject();
    QStringList ComponentTypes;
    if (Selected != nullptr)
    {
        for (Component* ComponentInstance : Selected->GetAllComponents())
        {
            if (ComponentInstance == nullptr)
            {
                continue;
            }
            const QString ComponentType = ComponentInstance->GetClass() != nullptr
                ? QString::fromStdString(ComponentInstance->GetClass()->GetTypeId().Value)
                : QString::fromStdString(ComponentInstance->GetName());
            if (!ComponentTypes.contains(ComponentType))
            {
                ComponentTypes.push_back(ComponentType);
            }

            auto* ComponentCard = new QGroupBox(ComponentType);
            ComponentCard->setObjectName("ComponentInspectorCard");
            ComponentCard->setProperty("componentType", ComponentType);
            auto* ComponentCardLayout = new QVBoxLayout(ComponentCard);
            ComponentCardLayout->setContentsMargins(6, 8, 6, 6);
            auto* ComponentInspector = new ReflectionInspector(ComponentCard);
            ConfigureComponentInspector(ComponentInspector);
            ComponentInspector->SetInspectedObject(ComponentInstance);
            ComponentInspector->setEnabled(!BoundEngine.GetPlaySession().IsSimulating());
            ComponentCardLayout->addWidget(ComponentInspector);
            ComponentInspectorLayout->addWidget(ComponentCard);
            ComponentInspectors.push_back(ComponentInspector);
        }
    }
    ComponentInspectorLayout->addStretch(1);

    QSignalBlocker ComboSignalBlocker(ComponentCombo);
    ComponentCombo->clear();
    ComponentCombo->addItem(QStringLiteral("All"), QString{});
    for (const QString& ComponentType : ComponentTypes)
    {
        ComponentCombo->addItem(ComponentType, ComponentType);
    }
    int FilterIndex = ComponentCombo->findData(ComponentFilter);
    if (FilterIndex < 0)
    {
        ComponentFilter.clear();
        FilterIndex = 0;
    }
    ComponentCombo->setCurrentIndex(FilterIndex);
    ComponentCombo->setEnabled(Selected != nullptr && !ComponentTypes.isEmpty());
    OnComponentFilterChanged(FilterIndex);
}

void EditorMainWindow::RefreshSelectionUi()
{
    bUpdatingSelectionUi = true;

    GameObject* Selected = GetSelectedGameObject();
    const bool bHasSelection = Selected != nullptr;
    const bool bEditableSelection = bHasSelection && !BoundEngine.GetPlaySession().IsSimulating();
    if (ObjectNameEdit != nullptr)
    {
        ObjectNameEdit->setEnabled(bEditableSelection);
        ObjectNameEdit->setText(bHasSelection ? QString::fromStdString(Selected->GetName()) : QString());
    }

    auto SetSpinEnabled = [&](QDoubleSpinBox* Spin, double Value)
    {
        if (Spin == nullptr)
        {
            return;
        }
        Spin->setEnabled(bEditableSelection);
        Spin->setValue(Value);
    };

    if (bHasSelection)
    {
        const Transform& ObjectTransform = Selected->GetTransform();
        if (PrimaryViewport != nullptr)
        {
            PrimaryViewport->SetSelectedTransform(ObjectTransform, Selected->GetWorldTransform());
        }
        SetSpinEnabled(PositionXSpin, ObjectTransform.Position.x);
        SetSpinEnabled(PositionYSpin, ObjectTransform.Position.y);
        SetSpinEnabled(PositionZSpin, ObjectTransform.Position.z);
        const Vector3 RotationDegrees = QuaternionToEulerDegrees(ObjectTransform.Rotation);
        SetSpinEnabled(RotationXSpin, RotationDegrees.x);
        SetSpinEnabled(RotationYSpin, RotationDegrees.y);
        SetSpinEnabled(RotationZSpin, RotationDegrees.z);
        SetSpinEnabled(ScaleXSpin, ObjectTransform.Scale.x);
        SetSpinEnabled(ScaleYSpin, ObjectTransform.Scale.y);
        SetSpinEnabled(ScaleZSpin, ObjectTransform.Scale.z);
    }
    else
    {
        if (PrimaryViewport != nullptr)
        {
            PrimaryViewport->ClearSelectedTransform();
        }
        SetSpinEnabled(PositionXSpin, 0.0);
        SetSpinEnabled(PositionYSpin, 0.0);
        SetSpinEnabled(PositionZSpin, 0.0);
        SetSpinEnabled(RotationXSpin, 0.0);
        SetSpinEnabled(RotationYSpin, 0.0);
        SetSpinEnabled(RotationZSpin, 0.0);
        SetSpinEnabled(ScaleXSpin, 1.0);
        SetSpinEnabled(ScaleYSpin, 1.0);
        SetSpinEnabled(ScaleZSpin, 1.0);
    }

    RebuildComponentInspectors();

    bUpdatingSelectionUi = false;
}

void EditorMainWindow::OnHierarchySelectionChanged()
{
    QTreeWidgetItem* Current = HierarchyTree->currentItem();
    if (Current == nullptr)
    {
        ClearSelection();
        UpdateStatus();
        return;
    }

    SelectedObject = LoadObjectHandle(Current);
    ComponentFilter.clear();
    if (GetSelectedGameObject() == nullptr)
    {
        ClearSelection();
    }
    else
    {
        RefreshSelectionUi();
    }
    UpdateStatus();
}

void EditorMainWindow::OnHierarchyContextMenu(const QPoint& Position)
{
    if (BoundEngine.GetPlaySession().IsSimulating())
    {
        return;
    }
    QMenu Menu(this);
    Menu.addAction(QString::fromUtf8("Copy"), this, &EditorMainWindow::OnCopy);
    QAction* PasteAction = Menu.addAction(QString::fromUtf8("Paste"), this, &EditorMainWindow::OnPaste);
    PasteAction->setEnabled(
        !BoundEngine.GetPlaySession().IsSimulating()
        && QApplication::clipboard()->mimeData()->hasFormat(SakuraSceneSubtreeMimeType));
    Menu.addSeparator();
    QMenu* CreateObjectsMenu = Menu.addMenu(QString::fromUtf8("\xD0\x9E\xD0\xB1\xD1\x8A\xD0\xB5\xD0\xBA\xD1\x82\xD1\x8B"));
    QMenu* CreateComponentsMenu = Menu.addMenu(QString::fromUtf8("\xD0\x9A\xD0\xBE\xD0\xBC\xD0\xBF\xD0\xBE\xD0\xBD\xD0\xB5\xD0\xBD\xD1\x82\xD1\x8B"));
    PopulateCreateMenus(CreateObjectsMenu, CreateComponentsMenu);
    Menu.addSeparator();
    Menu.addAction(QString::fromUtf8("Delete Object"), this, &EditorMainWindow::OnDeleteSelectedObject);
    Menu.exec(HierarchyTree->viewport()->mapToGlobal(Position));
}

void EditorMainWindow::OnUndo()
{
    if (!CommandStack.Undo())
    {
        return;
    }
    if (GetSelectedGameObject() == nullptr)
    {
        ClearSelection();
    }
    PopulateHierarchy();
    RefreshSelectionUi();
    UpdateWindowTitleDirty();
    UpdateStatus();
}

void EditorMainWindow::OnRedo()
{
    if (!CommandStack.Redo())
    {
        return;
    }
    PopulateHierarchy();
    RefreshSelectionUi();
    UpdateWindowTitleDirty();
    UpdateStatus();
}

bool EditorMainWindow::IsTextClipboardWidget(QWidget* Candidate)
{
    if (Candidate == nullptr)
    {
        return false;
    }
    if (qobject_cast<QLineEdit*>(Candidate) != nullptr
        || qobject_cast<QPlainTextEdit*>(Candidate) != nullptr
        || qobject_cast<QTextEdit*>(Candidate) != nullptr
        || qobject_cast<QAbstractSpinBox*>(Candidate) != nullptr)
    {
        return true;
    }
    if (auto* Combo = qobject_cast<QComboBox*>(Candidate))
    {
        return Combo->isEditable();
    }
    return qobject_cast<QAbstractSpinBox*>(Candidate->parentWidget()) != nullptr;
}

bool EditorMainWindow::CopyFocusedTextWidget()
{
    QWidget* FocusedWidget = QApplication::focusWidget();
    if (!IsTextClipboardWidget(FocusedWidget))
    {
        return false;
    }
    if (auto* LineEdit = qobject_cast<QLineEdit*>(FocusedWidget))
    {
        LineEdit->copy();
        return true;
    }
    if (auto* PlainTextEdit = qobject_cast<QPlainTextEdit*>(FocusedWidget))
    {
        PlainTextEdit->copy();
        return true;
    }
    if (auto* TextEdit = qobject_cast<QTextEdit*>(FocusedWidget))
    {
        TextEdit->copy();
        return true;
    }
    if (auto* NestedLineEdit = FocusedWidget->findChild<QLineEdit*>())
    {
        NestedLineEdit->copy();
        return true;
    }
    return false;
}

bool EditorMainWindow::PasteFocusedTextWidget()
{
    QWidget* FocusedWidget = QApplication::focusWidget();
    if (!IsTextClipboardWidget(FocusedWidget))
    {
        return false;
    }
    if (auto* LineEdit = qobject_cast<QLineEdit*>(FocusedWidget))
    {
        LineEdit->paste();
        return true;
    }
    if (auto* PlainTextEdit = qobject_cast<QPlainTextEdit*>(FocusedWidget))
    {
        PlainTextEdit->paste();
        return true;
    }
    if (auto* TextEdit = qobject_cast<QTextEdit*>(FocusedWidget))
    {
        TextEdit->paste();
        return true;
    }
    if (auto* NestedLineEdit = FocusedWidget->findChild<QLineEdit*>())
    {
        NestedLineEdit->paste();
        return true;
    }
    return false;
}

bool EditorMainWindow::HandleClipboardShortcut(QEvent* Event)
{
    if (Event == nullptr
        || (Event->type() != QEvent::ShortcutOverride && Event->type() != QEvent::KeyPress))
    {
        return false;
    }

    auto* KeyEvent = static_cast<QKeyEvent*>(Event);
    if (!KeyEvent->matches(QKeySequence::Copy) && !KeyEvent->matches(QKeySequence::Paste))
    {
        return false;
    }
    if (IsTextClipboardWidget(QApplication::focusWidget()))
    {
        return false;
    }
    if (Event->type() == QEvent::ShortcutOverride)
    {
        KeyEvent->accept();
        return true;
    }
    if (KeyEvent->matches(QKeySequence::Copy))
    {
        OnCopy();
    }
    else
    {
        OnPaste();
    }
    KeyEvent->accept();
    return true;
}

bool EditorMainWindow::HandleGizmoModeHotkey(QEvent* Event)
{
    if (Event == nullptr
        || PrimaryViewport == nullptr
        || BoundEngine.GetPlaySession().IsSimulating()
        || (Event->type() != QEvent::ShortcutOverride && Event->type() != QEvent::KeyPress))
    {
        return false;
    }
    if (IsTextClipboardWidget(QApplication::focusWidget()) || PrimaryViewport->IsCameraNavigationActive())
    {
        return false;
    }

    auto* KeyEvent = static_cast<QKeyEvent*>(Event);
    if (KeyEvent->modifiers() != Qt::NoModifier)
    {
        return false;
    }

    const quint32 ScanCode = KeyEvent->nativeScanCode();
    EditorViewportWidget::GizmoOperation Operation = PrimaryViewport->GetGizmoOperation();
    bool bMatched = false;
    if (ScanCode == 0x11)
    {
        Operation = EditorViewportWidget::GizmoOperation::Translate;
        bMatched = true;
    }
    else if (ScanCode == 0x12)
    {
        Operation = EditorViewportWidget::GizmoOperation::Rotate;
        bMatched = true;
    }
    else if (ScanCode == 0x13)
    {
        Operation = EditorViewportWidget::GizmoOperation::Scale;
        bMatched = true;
    }
    if (!bMatched)
    {
        return false;
    }
    if (Event->type() == QEvent::ShortcutOverride)
    {
        KeyEvent->accept();
        return true;
    }
    PrimaryViewport->SetGizmoOperation(Operation);
    KeyEvent->accept();
    return true;
}

void EditorMainWindow::SyncGizmoModeButtons()
{
    if (GizmoModeButtons == nullptr || PrimaryViewport == nullptr)
    {
        return;
    }
    QAbstractButton* Button = GizmoModeButtons->button(
        static_cast<int>(PrimaryViewport->GetGizmoOperation()));
    if (Button != nullptr)
    {
        const QSignalBlocker Blocker(GizmoModeButtons);
        Button->setChecked(true);
    }
}

bool EditorMainWindow::eventFilter(QObject* Watched, QEvent* Event)
{
    if ((Watched == HierarchyTree || Watched == PrimaryViewport) && HandleClipboardShortcut(Event))
    {
        return true;
    }
    if ((Watched == HierarchyTree || Watched == PrimaryViewport) && HandleGizmoModeHotkey(Event))
    {
        return true;
    }
    return QMainWindow::eventFilter(Watched, Event);
}

bool EditorMainWindow::CopySelectedObjectToClipboard()
{
    GameObject* Selected = GetSelectedGameObject();
    if (Selected == nullptr)
    {
        return false;
    }

    std::string SerializedSubtree;
    SceneSerializeResult Serialized = SceneSerializer::SerializeSubtreeToJson(
        *Selected,
        SerializedSubtree);
    if (!Serialized.bOk)
    {
        statusBar()->showMessage(
            tr("Copy failed: %1").arg(QString::fromStdString(Serialized.Error)),
            2500);
        return false;
    }

    auto* MimeData = new QMimeData();
    MimeData->setData(
        SakuraSceneSubtreeMimeType,
        QByteArray::fromStdString(SerializedSubtree));
    QApplication::clipboard()->setMimeData(MimeData);
    statusBar()->showMessage(tr("Copied object subtree"), 1500);
    return true;
}

bool EditorMainWindow::PasteObjectFromClipboard()
{
    Scene* EditScene = GetEditScene();
    const QMimeData* MimeData = QApplication::clipboard()->mimeData();
    if (EditScene == nullptr
        || BoundEngine.GetPlaySession().IsSimulating()
        || MimeData == nullptr
        || !MimeData->hasFormat(SakuraSceneSubtreeMimeType))
    {
        return false;
    }

    ObjectHandle Parent;
    if (GameObject* Selected = GetSelectedGameObject())
    {
        if (Selected->GetParent() != nullptr)
        {
            Parent = Selected->GetParent()->GetObjectHandle();
        }
    }

    ObjectHandle Created;
    std::unique_ptr<EditorCommand> Command = MakePasteSubtreeCommand(
        EditScene,
        MimeData->data(SakuraSceneSubtreeMimeType).toStdString(),
        Parent,
        &Created);
    if (!ExecuteCommand(std::move(Command)))
    {
        statusBar()->showMessage(tr("Paste failed"), 2500);
        return false;
    }
    SelectObject(Created);
    statusBar()->showMessage(tr("Pasted object subtree"), 1500);
    return true;
}

void EditorMainWindow::OnCopy()
{
    if (CopyFocusedTextWidget())
    {
        return;
    }
    QWidget* FocusedWidget = QApplication::focusWidget();
    if (ContentBrowser != nullptr
        && FocusedWidget != nullptr
        && (FocusedWidget == ContentBrowser || ContentBrowser->isAncestorOf(FocusedWidget)))
    {
        ContentBrowser->CopySelectionToClipboard();
        return;
    }
    CopySelectedObjectToClipboard();
}

void EditorMainWindow::OnPaste()
{
    if (PasteFocusedTextWidget())
    {
        return;
    }
    QWidget* FocusedWidget = QApplication::focusWidget();
    if (ContentBrowser != nullptr
        && FocusedWidget != nullptr
        && (FocusedWidget == ContentBrowser || ContentBrowser->isAncestorOf(FocusedWidget)))
    {
        ContentBrowser->PasteFromClipboard();
        return;
    }
    PasteObjectFromClipboard();
}

void EditorMainWindow::PopulateCreateMenus(QMenu* ObjectsMenu, QMenu* ComponentsMenu)
{
    if (ObjectsMenu == nullptr || ComponentsMenu == nullptr)
    {
        return;
    }

    ObjectsMenu->clear();
    ComponentsMenu->clear();

    EditorActionContext Context = MakeEditorActionContext();
    int PreviousObjectSortOrder = -1;
    int PreviousComponentSortOrder = -1;
    for (EditorAction* Action : EditorAction::CollectPublishedActions())
    {
        if (Action == nullptr)
        {
            continue;
        }

        QMenu* TargetMenu = nullptr;
        int* PreviousSortOrder = nullptr;
        if (std::strcmp(Action->GetMenuCategory(), EditorAction::ObjectsCategory) == 0)
        {
            TargetMenu = ObjectsMenu;
            PreviousSortOrder = &PreviousObjectSortOrder;
        }
        else if (std::strcmp(Action->GetMenuCategory(), EditorAction::ComponentsCategory) == 0)
        {
            TargetMenu = ComponentsMenu;
            PreviousSortOrder = &PreviousComponentSortOrder;
        }
        if (TargetMenu == nullptr || PreviousSortOrder == nullptr)
        {
            continue;
        }

        if (*PreviousSortOrder >= 0 && Action->GetSortOrder() - *PreviousSortOrder >= 50)
        {
            TargetMenu->addSeparator();
        }
        *PreviousSortOrder = Action->GetSortOrder();

        QAction* MenuAction = TargetMenu->addAction(QString::fromUtf8(Action->GetDisplayName()));
        MenuAction->setEnabled(Action->CanExecute(Context));
        connect(MenuAction, &QAction::triggered, this, [this, Action]()
        {
            RunEditorAction(Action);
        });
    }
}

EditorActionContext EditorMainWindow::MakeEditorActionContext()
{
    EditorActionContext Context;
    Context.EditScene = GetEditScene();
    Context.CommandStack = &CommandStack;
    Context.SelectedObject = GetSelectedObjectHandle();
    Context.ExecuteCommand = [this](std::unique_ptr<EditorCommand> Command)
    {
        return ExecuteCommand(std::move(Command));
    };
    Context.SelectObject = [this](ObjectHandle Target)
    {
        SelectObject(Target);
    };
    return Context;
}

void EditorMainWindow::RunEditorAction(EditorAction* Action)
{
    if (Action == nullptr)
    {
        return;
    }
    EditorActionContext Context = MakeEditorActionContext();
    if (!Action->CanExecute(Context))
    {
        return;
    }
    Action->Execute(Context);
}

void EditorMainWindow::RunEditorActionByTypeId(const char* TypeIdString)
{
    Class* ActionClass = ReflectionSubsystem::Get().FindClass(TypeIdString);
    if (ActionClass == nullptr || ActionClass->GetClassDefaultObject() == nullptr)
    {
        return;
    }
    RunEditorAction(static_cast<EditorAction*>(ActionClass->GetClassDefaultObject()));
}

void EditorMainWindow::OnCreateEmptyObject()
{
    RunEditorActionByTypeId(CreateEmptyObjectAction::StaticReflectionTypeId());
}

void EditorMainWindow::OnDeleteSelectedObject()
{
    Scene* EditScene = GetEditScene();
    GameObject* Selected = GetSelectedGameObject();
    if (EditScene == nullptr || Selected == nullptr)
    {
        return;
    }

    const ObjectHandle Target = Selected->GetObjectHandle();
    if (!ExecuteCommand(MakeDeleteObjectCommand(EditScene, Target)))
    {
        return;
    }
    ClearSelection();
}

void EditorMainWindow::OnObjectNameEdited()
{
    if (bUpdatingSelectionUi)
    {
        return;
    }

    Scene* EditScene = GetEditScene();
    GameObject* Selected = GetSelectedGameObject();
    if (EditScene == nullptr || Selected == nullptr || ObjectNameEdit == nullptr)
    {
        return;
    }

    const std::string NewName = ObjectNameEdit->text().toStdString();
    if (NewName == Selected->GetName())
    {
        return;
    }
    ExecuteCommand(MakeRenameObjectCommand(EditScene, Selected->GetObjectHandle(), NewName));
}

void EditorMainWindow::OnTransformEdited()
{
    if (bUpdatingSelectionUi)
    {
        return;
    }

    Scene* EditScene = GetEditScene();
    GameObject* Selected = GetSelectedGameObject();
    if (EditScene == nullptr || Selected == nullptr)
    {
        return;
    }

    Transform Updated = Selected->GetTransform();
    Updated.Position = Vector3(
        static_cast<float>(PositionXSpin->value()),
        static_cast<float>(PositionYSpin->value()),
        static_cast<float>(PositionZSpin->value()));
    Updated.Rotation = Quaternion::CreateFromYawPitchRoll(
        static_cast<float>(RotationYSpin->value()) * DegreesToRadians,
        static_cast<float>(RotationXSpin->value()) * DegreesToRadians,
        static_cast<float>(RotationZSpin->value()) * DegreesToRadians);
    Updated.Rotation.Normalize();
    Updated.Scale = Vector3(
        static_cast<float>(ScaleXSpin->value()),
        static_cast<float>(ScaleYSpin->value()),
        static_cast<float>(ScaleZSpin->value()));
    ExecuteCommand(MakeSetTransformCommand(EditScene, Selected->GetObjectHandle(), Updated));
}

void EditorMainWindow::OnAssetActivated(
    QString AssetId,
    QString SubAssetIdentifier,
    QString AssetTypeIdentifier,
    QString VirtualPath)
{
    AssetKey Key{};
    if (!Guid::TryParse(AssetId.toStdString(), Key.Asset))
    {
        return;
    }
    if (!SubAssetIdentifier.isEmpty())
    {
        SubAssetId ParsedSubAsset{};
        if (!Guid::TryParse(SubAssetIdentifier.toStdString(), ParsedSubAsset))
        {
            return;
        }
        Key.SubAsset = ParsedSubAsset;
    }

    AssetRegistryEntry Entry{};
    const SubAssetRecord* SubAsset = nullptr;
    if (!BoundEngine.GetAssetRegistry().TryResolveKey(Key, Entry, &SubAsset))
    {
        statusBar()->showMessage(tr("Asset is no longer registered"), 2500);
        return;
    }
    AssetType Type = UnknownAssetType;
    if (SubAsset != nullptr)
    {
        Type = SubAsset->Type;
    }
    else if (!TryParseAssetType(AssetTypeIdentifier.toStdString(), Type))
    {
        return;
    }

    auto FoundEditor = AssetEditors.find(Type);
    if (FoundEditor == AssetEditors.end())
    {
        statusBar()->showMessage(
            tr("No editor is registered for %1 assets").arg(QString::fromUtf8(AssetTypeToString(Type))),
            2500);
        return;
    }
    FoundEditor->second(Key, Entry, VirtualPath);
}

void EditorMainWindow::OnAssetDropped(
    QString AssetId,
    QString SubAssetIdentifier,
    QString AssetTypeIdentifier,
    QString VirtualPath,
    QPoint Position)
{
    if (BoundEngine.GetPlaySession().IsSimulating())
    {
        statusBar()->showMessage(tr("Scene asset drops are disabled during play"), 2000);
        return;
    }
    AssetKey Key{};
    if (!Guid::TryParse(AssetId.toStdString(), Key.Asset))
    {
        return;
    }
    if (!SubAssetIdentifier.isEmpty())
    {
        SubAssetId ParsedSubAsset{};
        if (Guid::TryParse(SubAssetIdentifier.toStdString(), ParsedSubAsset))
        {
            Key.SubAsset = ParsedSubAsset;
        }
    }
    AssetType Type = UnknownAssetType;
    if (!TryParseAssetType(AssetTypeIdentifier.toStdString(), Type))
    {
        return;
    }
    AssetRegistryEntry Entry{};
    if (!BoundEngine.GetAssetRegistry().TryGetById(Key.Asset, Entry))
    {
        return;
    }

    if (Type == MaterialAssetType)
    {
        GameObject* Selected = GetSelectedGameObject();
        MeshRendererComponent* MeshRenderer = Selected != nullptr
            ? Selected->GetComponent<MeshRendererComponent>()
            : nullptr;
        if (MeshRenderer == nullptr)
        {
            statusBar()->showMessage(tr("Select an object with MeshRenderer to assign a material"), 2500);
            return;
        }
        ExecuteCommand(MakeSetPropertyCommand(
            GetEditScene(),
            MeshRenderer->GetObjectHandle(),
            PropertyId{"material_asset_id"},
            ReflectedValue::MakeString(Key.Asset.ToString())));
        return;
    }

    if (Type == ModelAssetType && !Key.HasSubAsset())
    {
        for (const SubAssetRecord& SubAsset : Entry.Metadata.SubAssets)
        {
            if (SubAsset.Type == StaticMeshAssetType)
            {
                Key.SubAsset = SubAsset.Id;
                Type = StaticMeshAssetType;
                break;
            }
        }
    }
    if (Type != StaticMeshAssetType || !Key.HasSubAsset())
    {
        statusBar()->showMessage(tr("Only Model/StaticMesh assets can create scene objects"), 2500);
        return;
    }

    Scene* EditScene = GetEditScene();
    if (EditScene == nullptr || PrimaryViewport == nullptr)
    {
        return;
    }
    std::string ObjectName = std::filesystem::path(VirtualPath.toStdString()).stem().string();
    const SubAssetRecord* SubAsset = nullptr;
    AssetRegistryEntry ResolvedEntry{};
    if (BoundEngine.GetAssetRegistry().TryResolveKey(Key, ResolvedEntry, &SubAsset)
        && SubAsset != nullptr && !SubAsset->Name.empty())
    {
        ObjectName = SubAsset->Name;
    }

    ObjectHandle CreatedObject;
    ObjectHandle CreatedComponent;
    CommandStack.BeginGroup("Create Mesh Object");
    bool bCreated = ExecuteCommand(MakeCreateObjectCommand(EditScene, ObjectName, ObjectHandle{}, &CreatedObject));
    if (bCreated)
    {
        bCreated = ExecuteCommand(MakeAddComponentCommand(
            EditScene,
            CreatedObject,
            TypeId{MeshRendererComponent::StaticReflectionTypeId()},
            &CreatedComponent));
    }
    if (bCreated)
    {
        bCreated = ExecuteCommand(MakeSetPropertyCommand(
            EditScene,
            CreatedComponent,
            PropertyId{"mesh_asset_id"},
            ReflectedValue::MakeString(Key.Asset.ToString())));
    }
    if (bCreated)
    {
        bCreated = ExecuteCommand(MakeSetPropertyCommand(
            EditScene,
            CreatedComponent,
            PropertyId{"mesh_sub_asset_id"},
            ReflectedValue::MakeString(Key.SubAsset->ToString())));
    }
    if (bCreated)
    {
        const RenderViewCamera& Camera = PrimaryViewport->GetViewCamera();
        Vector3 Forward = Camera.Target - Camera.Position;
        const float Distance = std::max(1.f, Forward.Length());
        Forward.Normalize();
        Vector3 Right = Forward.Cross(Camera.Up);
        Right.Normalize();
        Vector3 Up = Right.Cross(Forward);
        Up.Normalize();
        const float NormalizedX = static_cast<float>(Position.x()) / std::max(1, PrimaryViewport->width()) * 2.f - 1.f;
        const float NormalizedY = 1.f - static_cast<float>(Position.y()) / std::max(1, PrimaryViewport->height()) * 2.f;
        const float HalfHeight = std::tan(Camera.FieldOfViewDegrees * 0.0087266463f) * Distance;
        Transform DroppedTransform;
        DroppedTransform.Position = Camera.Target
            + Right * (NormalizedX * HalfHeight * static_cast<float>(PrimaryViewport->width()) / std::max(1, PrimaryViewport->height()))
            + Up * (NormalizedY * HalfHeight);
        bCreated = ExecuteCommand(MakeSetTransformCommand(EditScene, CreatedObject, DroppedTransform));
    }
    CommandStack.EndGroup();
    if (bCreated)
    {
        SelectObject(CreatedObject);
        statusBar()->showMessage(tr("Created %1 from Content Browser").arg(QString::fromStdString(ObjectName)), 2000);
    }
}

void EditorMainWindow::OnComponentFilterChanged(int Index)
{
    if (ComponentCombo == nullptr || Index < 0 || Index >= ComponentCombo->count())
    {
        return;
    }

    ComponentFilter = ComponentCombo->itemData(Index).toString();
    for (ReflectionInspector* ComponentInspector : ComponentInspectors)
    {
        if (ComponentInspector == nullptr || ComponentInspector->GetInspectedObject() == nullptr)
        {
            continue;
        }
        Object* ComponentInstance = ComponentInspector->GetInspectedObject();
        const QString ComponentType = ComponentInstance->GetClass() != nullptr
            ? QString::fromStdString(ComponentInstance->GetClass()->GetTypeId().Value)
            : QString::fromStdString(ComponentInstance->GetName());
        ComponentInspector->parentWidget()->setVisible(ComponentFilter.isEmpty() || ComponentFilter == ComponentType);
    }
}

bool EditorMainWindow::PromptSaveIfDirty()
{
    if (!Document.IsDirty())
    {
        return true;
    }

    const QMessageBox::StandardButton Answer = QMessageBox::question(
        this,
        "Unsaved Scene",
        QString::fromUtf8("Scene has unsaved changes. Save before continue?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (Answer == QMessageBox::Cancel)
    {
        return false;
    }
    if (Answer == QMessageBox::Save)
    {
        std::string Error;
        if (!Document.Save(Error))
        {
            QMessageBox::warning(
                this,
                "Save Scene",
                QString::fromStdString(Error.empty() ? "Failed to save scene" : Error));
            return false;
        }
        UpdateWindowTitleDirty();
    }
    return true;
}

void EditorMainWindow::StartStoryPlayback()
{
    RefreshStoryPlaybackUi();
}

void EditorMainWindow::StopStoryPlayback()
{
    StoryPlayback.Stop();
}

void EditorMainWindow::RefreshStoryPlaybackUi()
{
    const bool bAllowInput = bPlaying && !bPaused;
    StoryPanel->Refresh(bAllowInput);
    GameDialogue->Refresh(bAllowInput);
}

void EditorMainWindow::closeEvent(QCloseEvent* Event)
{
    if (!PromptSaveIfDirty())
    {
        Event->ignore();
        return;
    }
    StopStoryPlayback();
    BoundEngine.GetPlaySession().StopPlay();
    ClearSelection();
    CommandStack.Clear();
    CommandStack.BindDocument(nullptr);
    Document.Close();
    MaintenanceTimer->stop();
    BoundEngine.StopPresenting();
    QMainWindow::closeEvent(Event);
}
