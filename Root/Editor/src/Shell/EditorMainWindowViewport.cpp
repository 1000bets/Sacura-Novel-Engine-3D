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
#include "Navigation/NavigationWorld.h"
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
    if (Viewport == CameraPreviewViewport)
    {
        if (!Viewport->HasValidNativeHandle())
        {
            BoundEngine.UnregisterRenderSurface(RenderSurfaceId{2});
            bCameraPreviewAttached = false;
        }
        return;
    }
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
    if (Viewport == CameraPreviewViewport && !bCameraPreviewAttached)
    {
        return;
    }

    const NativeWindowInfo Info = Viewport->BuildNativeWindowInfo();
    if (Viewport == PrimaryViewport && Info.Width != 0 && Info.Height != 0)
    {
        // Resize publishes immediately, so refresh the overlay before the
        // Engine consumes it rather than waiting for the maintenance timer.
        PrimaryViewport->SyncGizmoOverlay();
    }
    BoundEngine.ResizeRenderSurface(RenderSurfaceId{Viewport->GetViewId().Value}, Info.Width, Info.Height, Viewport->IsInteractiveResize());
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
        RenderSettings EditSettings = RenderConfiguration;
        EditSettings.bGroundGrid = PrimaryViewport->IsGridVisible();
        BoundEngine.ConfigureRenderSurface(RenderSurfaceId{1}, true, Camera, EditSettings);
    }
    const GameObject* Selected = BoundEngine.GetPlaySession().IsSimulating() ? nullptr : GetSelectedGameObject();
    const CameraComponent* SelectedCamera = Selected ? Selected->GetComponent<CameraComponent>() : nullptr;
    const int PreviewWidth = std::min(300, PrimaryViewport->width() - 24);
    const int PreviewHeight = std::min(202, PrimaryViewport->height() - 24);
    const bool bShowCameraPreview = SelectedCamera && SelectedCamera->IsEnabled() && GetEditScene() != nullptr
        && PrimaryViewport->isVisible() && !isMinimized() && PreviewWidth >= 180 && PreviewHeight >= 140;
    if (!bShowCameraPreview)
    {
        if (bCameraPreviewAttached)
        {
            BoundEngine.UnregisterRenderSurface(RenderSurfaceId{2});
            bCameraPreviewAttached = false;
        }
        CameraPreviewPanel->hide();
    }
    else
    {
        if (CameraPreviewPanel->size() != QSize(PreviewWidth, PreviewHeight))
        {
            CameraPreviewViewport->setFixedHeight(PreviewHeight - 44);
            CameraPreviewPanel->setFixedSize(PreviewWidth, PreviewHeight);
        }
        const QPoint PreviewPosition = PrimaryViewport->mapToGlobal(QPoint(
            PrimaryViewport->width() - PreviewWidth - 12,
            PrimaryViewport->height() - PreviewHeight - 12));
        if (CameraPreviewPanel->pos() != PreviewPosition) CameraPreviewPanel->move(PreviewPosition);
        if (!CameraPreviewPanel->isVisible())
        {
            CameraPreviewPanel->show();
            CameraPreviewPanel->raise();
        }
        if (!bCameraPreviewUnavailable && !bCameraPreviewAttached && BoundEngine.IsPresenting()
            && CameraPreviewViewport->isVisible() && CameraPreviewViewport->HasValidNativeHandle())
        {
            bCameraPreviewAttached = BoundEngine.RegisterRenderSurface(RenderSurfaceId{2},
                CameraPreviewViewport->BuildNativeWindowInfo());
            if (!bCameraPreviewAttached)
            {
                bCameraPreviewUnavailable = true;
                CameraPreviewViewport->hide();
                CameraPreviewUnavailableLabel->show();
            }
        }
        if (bCameraPreviewAttached)
        {
            RenderViewCamera View;
            View.Position = Selected->GetWorldPosition();
            View.Target = View.Position + Selected->GetWorldForward();
            View.Up = Selected->GetWorldUp();
            View.FieldOfViewDegrees = std::clamp(SelectedCamera->FieldOfViewDegrees, 1.f, 179.f);
            View.NearPlane = std::max(.01f, SelectedCamera->NearPlane);
            View.FarPlane = std::max(View.NearPlane + .01f, SelectedCamera->FarPlane);
            const NativeWindowInfo Info = CameraPreviewViewport->BuildNativeWindowInfo();
            RenderCamera Camera;
            View.BuildRenderCamera(static_cast<float>(qMax(1u, Info.Width)) / static_cast<float>(qMax(1u, Info.Height)), Camera);
            RenderSettings PreviewSettings = RenderConfiguration;
            PreviewSettings.bGroundGrid = false;
            BoundEngine.ConfigureRenderSurface(RenderSurfaceId{2}, true, Camera, PreviewSettings);
        }
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
    if (NavigationStatisticsLabel != nullptr)
    {
        QStringList Lines;
        if (const Scene* World = BoundEngine.GetActiveScene())
        {
            if (const NavigationWorld* Navigation = World->GetNavigationIfCreated())
            {
                for (const std::string& Profile : Navigation->GetProfiles())
                {
                    QString State = tr("Empty");
                    switch (Navigation->GetBuildState(Profile))
                    {
                    case NavigationBuildState::Building: State = tr("Building"); break;
                    case NavigationBuildState::Ready: State = tr("Ready"); break;
                    case NavigationBuildState::Stale: State = tr("Stale"); break;
                    case NavigationBuildState::Failed: State = tr("Failed"); break;
                    case NavigationBuildState::Empty: break;
                    }
                    QString Detail = QString::fromStdString(Profile) + ": " + State;
                    if (const NavigationMesh* Mesh = Navigation->GetMesh(Profile))
                    {
                        const NavigationStatistics& NavigationMetrics = Mesh->GetStatistics();
                        Detail += tr(", %1 tiles, %2 polygons, %3 KiB, build %4 ms")
                            .arg(NavigationMetrics.TileCount).arg(NavigationMetrics.PolygonCount)
                            .arg(NavigationMetrics.DataBytes / 1024).arg(NavigationMetrics.BuildMilliseconds, 0, 'f', 2);
                    }
                    Lines.append(Detail);
                }
                if (!Navigation->GetDiagnostic().empty())
                {
                    Lines.append(QString::fromStdString(Navigation->GetDiagnostic()));
                }
            }
        }
        NavigationStatisticsLabel->setText(Lines.join('\n'));
        NavigationStatisticsLabel->setVisible(!Lines.isEmpty());
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

    if (KeyEvent->nativeScanCode() == 0x21 || (KeyEvent->nativeScanCode() == 0 && KeyEvent->key() == Qt::Key_F))
    {
        if (Event->type() == QEvent::KeyPress) { FrameSelectedObject(); }
        KeyEvent->accept();
        return true;
    }
    const quint32 ScanCode = KeyEvent->nativeScanCode();
    EditorViewportWidget::GizmoOperation Operation = PrimaryViewport->GetGizmoOperation();
    bool bMatched = false;
    if (ScanCode == 0x10)
    {
        Operation = EditorViewportWidget::GizmoOperation::Select;
        bMatched = true;
    }
    else if (ScanCode == 0x11)
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

