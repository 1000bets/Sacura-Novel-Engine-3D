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

EditorMainWindow::EditorMainWindow(Engine& InEngine, ProjectSession& InSession, QWidget* Parent)
    : QMainWindow(Parent)
    , BoundEngine(InEngine)
    , BoundSession(InSession)
    , Workspace(InEngine)
    , Document(Workspace.GetDocument())
    , CommandStack(Workspace.GetCommands())
    , StoryPlayback(InEngine.GetStoryRuntime())
{
    setObjectName("EditorMainWindow");
    setWindowTitle("Sacura Novel Studio");
    resize(1440, 900);
    setStyleSheet(EditorTheme::Stylesheet());

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
    disconnect(PrimaryViewport, nullptr, this, nullptr);
    BoundEngine.StopPresenting();
    ClearSelection();
    CommandStack.Clear();
    CommandStack.BindDocument(nullptr);
    Document.Close();
    delete MaintenanceClock;
}

