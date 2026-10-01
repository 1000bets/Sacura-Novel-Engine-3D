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

void EditorMainWindow::OnPlayToggled(bool bChecked)
{
    PlaySession& Session = BoundEngine.GetPlaySession();
    bPlayRequested = bChecked;
    if (bChecked)
    {
        Scene* World = GetEditScene();
        if (World != nullptr && !BoundEngine.PrepareSceneMaterials(*World))
        {
            statusBar()->showMessage(tr("Preparing material variants…"));
            return;
        }
        bPlayRequested = false;
        if (!Session.StartPlay())
        {
            PlayButton->blockSignals(true);
            PlayButton->setChecked(false);
            PlayButton->blockSignals(false);
            statusBar()->showMessage(QString::fromUtf8("Play failed вЂ” check log"), 2500);
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
        ? QString::fromUtf8("Game В· Play World")
        : QString::fromUtf8("Scene В· Edit World"));
    GameDialogue->setVisible(bPlaying);
    ClearSelection();
    PopulateHierarchy();
    ConfigureRenderView();
    if (bPlaying)
    {
        statusBar()->showMessage(QString::fromUtf8("Play mode вЂ” hierarchy edits disabled"), 3000);
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

