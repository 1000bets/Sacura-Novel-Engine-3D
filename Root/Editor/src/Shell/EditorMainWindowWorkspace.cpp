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

void EditorMainWindow::BindDocumentFromSession()
{
    if (BoundSession.GetProject() == nullptr || BoundEngine.GetEditScene() == nullptr)
    {
        return;
    }
    std::string Error;
    if (!Workspace.AdoptSessionScene(BoundSession, Error))
    {
        PrintString("EditorMainWindow: failed to adopt startup scene: " + Error);
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
        Title = QString("Sacura Novel Studio РІР‚вЂќ %1").arg(QString::fromStdString(Descriptor->Name));
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
        IssuesText += QString::fromUtf8(" Р’В· dirty");
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
    if (bPlayRequested && GetEditScene() != nullptr && BoundEngine.PrepareSceneMaterials(*GetEditScene()))
    {
        OnPlayToggled(true);
    }
    RefreshStoryPlaybackUi();
    UpdateStatus();
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
