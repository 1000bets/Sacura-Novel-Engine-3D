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

    const ObjectHandle IconObject = PrimaryViewport->PickComponentIcon(Position);
    if (EditScene->FindByHandle(IconObject) != nullptr)
    {
        SelectObject(IconObject);
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

