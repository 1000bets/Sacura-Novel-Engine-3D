#include "EditorMainWindow.h"

#include "EditorTheme.h"
#include "Engine.h"
#include "Project/ProjectSession.h"
#include "ProjectBrowserDialog.h"
#include "ReflectionInspector.h"

#include <QApplication>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

EditorMainWindow::EditorMainWindow(Engine& InEngine, ProjectSession& InSession, QWidget* Parent)
    : QMainWindow(Parent)
    , BoundEngine(InEngine)
    , BoundSession(InSession)
{
    setObjectName("EditorMainWindow");
    setWindowTitle("Sacura Novel Studio");
    resize(1440, 900);
    setStyleSheet(EditorTheme::Stylesheet());

    BuildMenus();
    BuildToolbar();
    BuildUi();
    PopulateHierarchy();
    PopulateStoryTree();
    PopulateDockPages();
    RefreshProjectTitle();
    UpdateStatus();
}

bool EditorMainWindow::CaptureScreenshot(const QString& OutputFile)
{
    show();
    raise();
    activateWindow();
    QApplication::processEvents();
    return grab().save(OutputFile, "PNG");
}

void EditorMainWindow::BuildMenus()
{
    QMenu* FileMenu = menuBar()->addMenu(QString::fromUtf8("\xD0\xA4\xD0\xB0\xD0\xB9\xD0\xBB"));
    FileMenu->addAction(QString::fromUtf8("\xD0\x9D\xD0\xBE\xD0\xB2\xD1\x8B\xD0\xB9 \xD0\xBF\xD1\x80\xD0\xBE\xD0\xB5\xD0\xBA\xD1\x82..."), this, [this]()
    {
        ProjectBrowserDialog Browser(this);
        if (Browser.exec() == QDialog::Accepted)
        {
            if (BoundSession.OpenProject(Browser.SelectedProjectFile()))
            {
                RefreshProjectTitle();
                UpdateStatus();
            }
        }
    });
    FileMenu->addAction(QString::fromUtf8("\xD0\x9E\xD1\x82\xD0\xBA\xD1\x80\xD1\x8B\xD1\x82\xD1\x8C..."), this, &EditorMainWindow::OnOpenProject);
    FileMenu->addAction(QString::fromUtf8("\xD0\xA1\xD0\xBE\xD1\x85\xD1\x80\xD0\xB0\xD0\xBD\xD0\xB8\xD1\x82\xD1\x8C"), this, &EditorMainWindow::OnSaveProject);
    FileMenu->addSeparator();
    FileMenu->addAction(QString::fromUtf8("\xD0\x92\xD1\x8B\xD1\x85\xD0\xBE\xD0\xB4"), this, &QWidget::close);

    menuBar()->addMenu(QString::fromUtf8("\xD0\x9F\xD1\x80\xD0\xB0\xD0\xB2\xD0\xBA\xD0\xB0"));
    QMenu* CreateMenu = menuBar()->addMenu(QString::fromUtf8("\xD0\xA1\xD0\xBE\xD0\xB7\xD0\xB4\xD0\xB0\xD1\x82\xD1\x8C"));
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
            WorkspaceSplitter->setSizes({240, 900, 320});
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

    auto* SubsceneLabel = new QLabel(QString::fromUtf8("\xD0\xA1\xD0\xB0\xD0\xB1\xD1\x81\xD1\x86\xD0\xB5\xD0\xBD\xD0\xB0"), Toolbar);
    SubsceneLabel->setObjectName("MutedLabel");
    Toolbar->addWidget(SubsceneLabel);

    SubsceneCombo = new QComboBox(Toolbar);
    SubsceneCombo->setMinimumWidth(180);
    SubsceneCombo->addItem(QString::fromUtf8("Prologue Room"), "prologue");
    SubsceneCombo->addItem(QString::fromUtf8("School Yard"), "school");
    SubsceneCombo->addItem(QString::fromUtf8("Classroom"), "class");
    Toolbar->addWidget(SubsceneCombo);
    Toolbar->addSeparator();

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
    HierarchyTree = new QTreeWidget(HierarchyTabs);
    HierarchyTree->setHeaderHidden(true);
    StoryTree = new QTreeWidget(HierarchyTabs);
    StoryTree->setHeaderHidden(true);
    HierarchyTabs->addTab(HierarchyTree, QString::fromUtf8("\xD0\x98\xD0\xB5\xD1\x80\xD0\xB0\xD1\x80\xD1\x85\xD0\xB8\xD1\x8F"));
    HierarchyTabs->addTab(StoryTree, QString::fromUtf8("\xD0\x98\xD1\x81\xD1\x82\xD0\xBE\xD1\x80\xD0\xB8\xD1\x8F"));
    HierarchyLayout->addWidget(HierarchyTabs, 1);

    auto* SubsceneHeader = new QLabel(QString::fromUtf8("\xD0\xA1\xD0\xB0\xD0\xB1\xD1\x81\xD1\x86\xD0\xB5\xD0\xBD\xD1\x8B"), HierarchyPanel);
    SubsceneHeader->setObjectName("PanelHeader");
    HierarchyLayout->addWidget(SubsceneHeader);
    SubsceneList = new QListWidget(HierarchyPanel);
    SubsceneList->setMaximumHeight(120);
    HierarchyLayout->addWidget(SubsceneList);

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

    ViewportTabs = new QTabWidget(ViewportPanel);
    auto* ScenePage = new QWidget(ViewportTabs);
    auto* SceneLayout = new QVBoxLayout(ScenePage);
    SceneLayout->setContentsMargins(8, 8, 8, 8);
    ViewportPlaceholder = new QLabel(
        QString::fromUtf8(
            "<div style='text-align:center;'>"
            "<div style='font-size:18px;color:#c496ad;margin-bottom:8px;'>\xD0\xA0\xD0\xB5\xD0\xB4\xD0\xB0\xD0\xBA\xD1\x82\xD0\xBE\xD1\x80 \xD1\x81\xD1\x86\xD0\xB5\xD0\xBD\xD1\x8B</div>"
            "<div>3D Viewport · Q W E R · Scene / Game</div>"
            "<div style='margin-top:10px;color:#95909e;font-size:11px;'>UI shell по эталону React Editor.jsx</div>"
            "</div>"),
        ScenePage);
    ViewportPlaceholder->setObjectName("ViewportPlaceholder");
    ViewportPlaceholder->setAlignment(Qt::AlignCenter);
    ViewportPlaceholder->setMinimumHeight(280);
    SceneLayout->addWidget(ViewportPlaceholder, 1);

    auto* LiveStrip = new QFrame(ScenePage);
    LiveStrip->setObjectName("LiveStrip");
    auto* LiveLayout = new QHBoxLayout(LiveStrip);
    LiveLayout->setContentsMargins(10, 4, 10, 4);
    LiveLayout->addWidget(new QLabel(QString::fromUtf8("\xF0\x9F\x94\x8A  \xD0\x90\xD1\x83\xD0\xB4\xD0\xB8\xD0\xBE · \xD0\xBD\xD0\xB0\xD0\xB6\xD0\xBC\xD0\xB8\xD1\x82\xD0\xB5 Play"), LiveStrip));
    LiveLayout->addStretch(1);
    LiveLayout->addWidget(new QLabel(QString::fromUtf8("Active: 0"), LiveStrip));
    SceneLayout->addWidget(LiveStrip);

    auto* GamePage = MakePlaceholderPage(
        QString::fromUtf8("\xD0\x98\xD0\xB3\xD1\x80\xD0\xB0"),
        QString::fromUtf8("Playtest viewport + dialogue HUD"));
    auto* CamerasPage = MakePlaceholderPage(
        QString::fromUtf8("\xD0\x9A\xD0\xB0\xD0\xBC\xD0\xB5\xD1\x80\xD1\x8B"),
        QString::fromUtf8("Camera library preview"));
    ViewportTabs->addTab(ScenePage, QString::fromUtf8("\xD0\xA0\xD0\xB5\xD0\xB4\xD0\xB0\xD0\xBA\xD1\x82\xD0\xBE\xD1\x80 \xD1\x81\xD1\x86\xD0\xB5\xD0\xBD\xD1\x8B"));
    ViewportTabs->addTab(GamePage, QString::fromUtf8("\xD0\x98\xD0\xB3\xD1\x80\xD0\xB0"));
    ViewportTabs->addTab(CamerasPage, QString::fromUtf8("\xD0\x9A\xD0\xB0\xD0\xBC\xD0\xB5\xD1\x80\xD1\x8B"));
    ViewportLayout->addWidget(ViewportTabs);

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
    Inspector = new ReflectionInspector(InspectorTabs);
    auto* SubsceneInspector = MakePlaceholderPage(
        QString::fromUtf8("\xD0\xA1\xD0\xB0\xD0\xB1\xD1\x81\xD1\x86\xD0\xB5\xD0\xBD\xD0\xB0"),
        QString::fromUtf8("Subscene settings, entry, cast, transitions"));
    InspectorTabs->addTab(Inspector, QString::fromUtf8("\xD0\x98\xD0\xBD\xD1\x81\xD0\xBF\xD0\xB5\xD0\xBA\xD1\x82\xD0\xBE\xD1\x80"));
    InspectorTabs->addTab(SubsceneInspector, QString::fromUtf8("\xD0\xA1\xD0\xB0\xD0\xB1\xD1\x81\xD1\x86\xD0\xB5\xD0\xBD\xD0\xB0"));
    InspectorLayout->addWidget(InspectorTabs);

    WorkspaceSplitter->addWidget(HierarchyPanel);
    WorkspaceSplitter->addWidget(CenterPanel);
    WorkspaceSplitter->addWidget(InspectorPanel);
    WorkspaceSplitter->setStretchFactor(0, 0);
    WorkspaceSplitter->setStretchFactor(1, 1);
    WorkspaceSplitter->setStretchFactor(2, 0);
    WorkspaceSplitter->setSizes({240, 900, 320});

    RootLayout->addWidget(WorkspaceSplitter, 1);
    setCentralWidget(Root);

    StatusIssuesLabel = new QLabel(statusBar());
    StatusSelectionLabel = new QLabel(statusBar());
    statusBar()->addWidget(StatusIssuesLabel);
    statusBar()->addPermanentWidget(StatusSelectionLabel);

    connect(HierarchyTabs, &QTabWidget::currentChanged, this, &EditorMainWindow::OnHierarchyTabChanged);
    connect(ViewportTabs, &QTabWidget::currentChanged, this, &EditorMainWindow::OnViewportModeChanged);
    connect(DockTabs, &QTabWidget::currentChanged, this, &EditorMainWindow::OnDockTabChanged);
    connect(HierarchyTree, &QTreeWidget::itemSelectionChanged, this, &EditorMainWindow::UpdateStatus);
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
    HierarchyTree->clear();
    auto* SceneRoot = new QTreeWidgetItem(HierarchyTree, {QString::fromUtf8("Prologue Room")});
    SceneRoot->setExpanded(true);
    auto* Characters = new QTreeWidgetItem(SceneRoot, {QString::fromUtf8("Characters")});
    new QTreeWidgetItem(Characters, {"Alice"});
    new QTreeWidgetItem(Characters, {"Narrator"});
    Characters->setExpanded(true);
    auto* Props = new QTreeWidgetItem(SceneRoot, {QString::fromUtf8("Props")});
    new QTreeWidgetItem(Props, {"Window"});
    new QTreeWidgetItem(Props, {"Door"});
    new QTreeWidgetItem(Props, {"Desk"});
    Props->setExpanded(true);
    auto* Lights = new QTreeWidgetItem(SceneRoot, {QString::fromUtf8("Lights")});
    new QTreeWidgetItem(Lights, {"KeyLight"});
    new QTreeWidgetItem(Lights, {"FillLight"});
    auto* Cameras = new QTreeWidgetItem(SceneRoot, {QString::fromUtf8("Cameras")});
    new QTreeWidgetItem(Cameras, {QString::fromUtf8("Main Camera")});
    new QTreeWidgetItem(Cameras, {QString::fromUtf8("Close-up Alice")});

    SubsceneList->clear();
    SubsceneList->addItem(QString::fromUtf8("Prologue Room"));
    SubsceneList->addItem(QString::fromUtf8("School Yard"));
    SubsceneList->addItem(QString::fromUtf8("Classroom"));
    SubsceneList->setCurrentRow(0);
}

void EditorMainWindow::PopulateStoryTree()
{
    StoryTree->clear();
    auto* Chapter = new QTreeWidgetItem(StoryTree, {QString::fromUtf8("Chapter 1 · Arrival")});
    Chapter->setExpanded(true);
    new QTreeWidgetItem(Chapter, {QString::fromUtf8("b1 · Alice · Hello")});
    new QTreeWidgetItem(Chapter, {QString::fromUtf8("b2 · Choice · Go outside")});
    new QTreeWidgetItem(Chapter, {QString::fromUtf8("b3 · Narrator · Rain starts")});
}

void EditorMainWindow::PopulateDockPages()
{
    struct DockPage
    {
        const char* TitleUtf8;
        const char* BodyUtf8;
    };

    const DockPage Pages[] = {
        {"\xD0\xA1\xD1\x86\xD0\xB5\xD0\xBD\xD0\xB0\xD1\x80\xD0\xB8\xD0\xB9", "Story graph · replies, answers, transitions"},
        {"\xD0\xA2\xD0\xB0\xD0\xB9\xD0\xBC\xD0\xBB\xD0\xB0\xD0\xB9\xD0\xBD", "Global story timeline / route map"},
        {"\xD0\xA1\xD0\xB0\xD0\xB1\xD1\x81\xD1\x86\xD0\xB5\xD0\xBD\xD1\x8B", "Subscene workspace and map"},
        {"\xD0\x9A\xD0\xB0\xD0\xBC\xD0\xB5\xD1\x80\xD1\x8B", "Camera library and framing"},
        {"\xD0\x9F\xD0\xBE\xD1\x81\xD1\x82\xD0\xB0\xD0\xBD\xD0\xBE\xD0\xB2\xD0\xBA\xD0\xB0", "Staging · before / during / after line"},
        {"\xD0\xA1\xD0\xBE\xD0\xB1\xD1\x8B\xD1\x82\xD0\xB8\xD0\xB5", "Event · action groups and actions"},
        {"\xD0\xA1\xD0\xBE\xD0\xB7\xD0\xB4\xD0\xB0\xD0\xBD\xD0\xB8\xD0\xB5", "Authoring library for actions and groups"},
        {"\xD0\x9F\xD1\x80\xD0\xBE\xD0\xB5\xD0\xBA\xD1\x82", "Asset browser · scenes / events / audio"},
        {"\xD0\x97\xD0\xB2\xD1\x83\xD0\xBA", "Sound workspace and meters"},
        {"\xD0\xAD\xD1\x84\xD1\x84\xD0\xB5\xD0\xBA\xD1\x82\xD1\x8B", "Effect samples playground"},
        {"\xD0\x90\xD0\xBA\xD1\x82\xD0\xB8\xD0\xB2\xD0\xBD\xD1\x8B\xD0\xB5", "Active effects and event instances"},
        {"\xD0\x9E\xD1\x88\xD0\xB8\xD0\xB1\xD0\xBA\xD0\xB8", "Validation issues and failure lab"},
    };

    for (const DockPage& Page : Pages)
    {
        DockTabs->addTab(
            MakePlaceholderPage(QString::fromUtf8(Page.TitleUtf8), QString::fromUtf8(Page.BodyUtf8)),
            QString::fromUtf8(Page.TitleUtf8));
    }
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
    setWindowTitle(QString("Sacura Novel Studio — %1").arg(QString::fromStdString(Descriptor->Name)));
}

void EditorMainWindow::UpdateStatus()
{
    StatusIssuesLabel->setText(QString::fromUtf8("\xD0\x9E\xD1\x88\xD0\xB8\xD0\xB1\xD0\xBA\xD0\xB8: 0"));
    QTreeWidgetItem* Current = HierarchyTree->currentItem();
    if (Current != nullptr)
    {
        StatusSelectionLabel->setText(QString::fromUtf8("\xD0\x92\xD1\x8B\xD0\xB4\xD0\xB5\xD0\xBB\xD0\xB5\xD0\xBD\xD0\xBE: ") + Current->text(0));
    }
    else
    {
        StatusSelectionLabel->setText(QString::fromUtf8("\xD0\x9D\xD0\xB5\xD1\x82 \xD0\xB2\xD1\x8B\xD0\xB4\xD0\xB5\xD0\xBB\xD0\xB5\xD0\xBD\xD0\xB8\xD1\x8F"));
    }
}

void EditorMainWindow::OnPlayToggled(bool bChecked)
{
    bPlaying = bChecked;
    bPaused = false;
    PauseButton->setChecked(false);
    PauseButton->setEnabled(bChecked);
    StepButton->setEnabled(bChecked);
    ModeLabel->setText(bChecked
        ? QString::fromUtf8("\xD0\x9F\xD1\x80\xD0\xB5\xD0\xB4\xD0\xBF\xD1\x80\xD0\xBE\xD1\x81\xD0\xBC\xD0\xBE\xD1\x82\xD1\x80")
        : QString::fromUtf8("\xD0\xA0\xD0\xB5\xD0\xB4\xD0\xB0\xD0\xBA\xD1\x82\xD0\xB8\xD1\x80\xD0\xBE\xD0\xB2\xD0\xB0\xD0\xBD\xD0\xB8\xD0\xB5"));
    PlayButton->setText(bChecked ? QString::fromUtf8("\xE2\x96\xA0") : QString::fromUtf8("\xE2\x96\xB6"));
    UpdateStatus();
}

void EditorMainWindow::OnPauseClicked()
{
    if (!bPlaying)
    {
        return;
    }
    bPaused = !bPaused;
    PauseButton->setChecked(bPaused);
    ModeLabel->setText(bPaused
        ? QString::fromUtf8("\xD0\x9D\xD0\xB0 \xD0\xBF\xD0\xB0\xD1\x83\xD0\xB7\xD0\xB5")
        : QString::fromUtf8("\xD0\x9F\xD1\x80\xD0\xB5\xD0\xB4\xD0\xBF\xD1\x80\xD0\xBE\xD1\x81\xD0\xBC\xD0\xBE\xD1\x82\xD1\x80"));
}

void EditorMainWindow::OnStepClicked()
{
    statusBar()->showMessage(QString::fromUtf8("Step · next line"), 1500);
}

void EditorMainWindow::OnViewportModeChanged(int Index)
{
    if (Index == 1 && !bPlaying)
    {
        statusBar()->showMessage(QString::fromUtf8("Game viewport · press Play for playtest"), 2000);
    }
}

void EditorMainWindow::OnDockTabChanged(int Index)
{
    if (Index >= 0)
    {
        statusBar()->showMessage(QString::fromUtf8("Dock: ") + DockTabs->tabText(Index), 1200);
    }
}

void EditorMainWindow::OnHierarchyTabChanged(int)
{
    UpdateStatus();
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
        DockTabs->setCurrentIndex(0);
    }
    else if (Mode == "scene")
    {
        CenterSplitter->setSizes({700, 80});
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
        WorkspaceSplitter->setSizes({240, 900, 320});
        CenterSplitter->setSizes({520, 280});
    }
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
    if (BoundSession.OpenProject(Selected.toStdString()))
    {
        RefreshProjectTitle();
        UpdateStatus();
    }
    else
    {
        QMessageBox::warning(this, "Open Project", QString::fromUtf8("\xD0\x9D\xD0\xB5 \xD1\x83\xD0\xB4\xD0\xB0\xD0\xBB\xD0\xBE\xD1\x81\xD1\x8C \xD0\xBE\xD1\x82\xD0\xBA\xD1\x80\xD1\x8B\xD1\x82\xD1\x8C \xD0\xBF\xD1\x80\xD0\xBE\xD0\xB5\xD0\xBA\xD1\x82"));
    }
}

void EditorMainWindow::OnSaveProject()
{
    statusBar()->showMessage(QString::fromUtf8("\xD0\x9F\xD1\x80\xD0\xBE\xD0\xB5\xD0\xBA\xD1\x82 \xD1\x81\xD0\xBE\xD1\x85\xD1\x80\xD0\xB0\xD0\xBD\xD1\x91\xD0\xBD (\xD0\xB7\xD0\xB0\xD0\xB3\xD0\xBB\xD1\x83\xD1\x88\xD0\xBA\xD0\xB0 UI)"), 2000);
}

void EditorMainWindow::OnFocusActionsAndEvents()
{
    DockTabs->setCurrentIndex(6);
    LayoutCombo->setCurrentIndex(1);
}
