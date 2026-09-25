#include "EditorViewportWidget.h"

#include "EditorAssetMime.h"
#include "Engine.h"
#include "Rendering/ImGuiOverlaySnapshot.h"

#include <imgui.h>
#include <ImGuizmo.h>

#include <QApplication>
#include <QCursor>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace DirectX::SimpleMath;

namespace
{
constexpr float DegreesToRadians = 0.01745329251994329577f;
constexpr float RadiansToDegrees = 57.2957795130823208768f;
constexpr float MinFocusDistance = 0.25f;
constexpr float MaxPitchDegrees = 89.0f;
constexpr float LookSensitivity = 0.18f;
constexpr float OrbitSensitivity = 0.25f;
constexpr float PanSensitivity = 0.01f;
constexpr float DollySensitivity = 0.0025f;
constexpr float WheelDollyFactor = 0.12f;
constexpr float MinMoveSpeed = 0.25f;
constexpr float MaxMoveSpeed = 250.0f;

constexpr quint32 ForwardKeyScanCode = 0x11;
constexpr quint32 BackwardKeyScanCode = 0x1f;
constexpr quint32 LeftKeyScanCode = 0x1e;
constexpr quint32 RightKeyScanCode = 0x20;
constexpr quint32 DownKeyScanCode = 0x10;
constexpr quint32 UpKeyScanCode = 0x12;
constexpr quint32 SpaceKeyScanCode = 0x39;
constexpr quint32 RotateGizmoKeyScanCode = 0x12;
constexpr quint32 ScaleGizmoKeyScanCode = 0x13;

float ClampPitch(float PitchDegrees)
{
    return std::clamp(PitchDegrees, -MaxPitchDegrees, MaxPitchDegrees);
}

void ExtractYawPitch(const Vector3& Forward, float& OutYawDegrees, float& OutPitchDegrees)
{
    const Vector3 SafeForward = Forward.LengthSquared() > 0.000001f ? Forward : Vector3::Forward;
    OutYawDegrees = std::atan2(SafeForward.x, SafeForward.z) * RadiansToDegrees;
    OutPitchDegrees = std::asin(std::clamp(SafeForward.y, -1.0f, 1.0f)) * RadiansToDegrees;
}

Vector3 DirectionFromYawPitch(float YawDegrees, float PitchDegrees)
{
    const float Yaw = YawDegrees * DegreesToRadians;
    const float Pitch = PitchDegrees * DegreesToRadians;
    const float CosPitch = std::cos(Pitch);
    Vector3 Direction(std::sin(Yaw) * CosPitch, std::sin(Pitch), std::cos(Yaw) * CosPitch);
    Direction.Normalize();
    return Direction;
}

ImGuiOverlaySnapshot CaptureImGuiOverlay(const ImDrawData* DrawData)
{
    ImGuiOverlaySnapshot Snapshot;
    if (DrawData == nullptr || !DrawData->Valid || DrawData->CmdListsCount <= 0)
    {
        return Snapshot;
    }

    static_assert(sizeof(ImGuiOverlayVertex) == sizeof(ImDrawVert), "ImGui overlay vertex layout must match ImDrawVert");
    static_assert(sizeof(uint16_t) == sizeof(ImDrawIdx), "ImGui overlay index type must match ImDrawIdx");

    Snapshot.DisplayWidth = DrawData->DisplaySize.x;
    Snapshot.DisplayHeight = DrawData->DisplaySize.y;
    Snapshot.FramebufferScaleX = DrawData->FramebufferScale.x;
    Snapshot.FramebufferScaleY = DrawData->FramebufferScale.y;
    Snapshot.Lists.reserve(static_cast<size_t>(DrawData->CmdListsCount));

    for (int ListIndex = 0; ListIndex < DrawData->CmdListsCount; ++ListIndex)
    {
        const ImDrawList* SourceList = DrawData->CmdLists[ListIndex];
        ImGuiOverlayDrawList& TargetList = Snapshot.Lists.emplace_back();
        TargetList.Vertices.resize(static_cast<size_t>(SourceList->VtxBuffer.Size));
        if (SourceList->VtxBuffer.Size > 0)
        {
            std::memcpy(
                TargetList.Vertices.data(),
                SourceList->VtxBuffer.Data,
                static_cast<size_t>(SourceList->VtxBuffer.Size) * sizeof(ImDrawVert));
        }
        TargetList.Indices.resize(static_cast<size_t>(SourceList->IdxBuffer.Size));
        if (SourceList->IdxBuffer.Size > 0)
        {
            std::memcpy(
                TargetList.Indices.data(),
                SourceList->IdxBuffer.Data,
                static_cast<size_t>(SourceList->IdxBuffer.Size) * sizeof(ImDrawIdx));
        }
        TargetList.Commands.reserve(static_cast<size_t>(SourceList->CmdBuffer.Size));
        for (int CommandIndex = 0; CommandIndex < SourceList->CmdBuffer.Size; ++CommandIndex)
        {
            const ImDrawCmd& SourceCommand = SourceList->CmdBuffer[CommandIndex];
            if (SourceCommand.UserCallback != nullptr || SourceCommand.ElemCount == 0)
            {
                continue;
            }
            ImGuiOverlayDrawCommand TargetCommand;
            TargetCommand.IndexOffset = SourceCommand.IdxOffset;
            TargetCommand.IndexCount = SourceCommand.ElemCount;
            TargetCommand.VertexOffset = SourceCommand.VtxOffset;
            TargetCommand.ClipMinX = SourceCommand.ClipRect.x;
            TargetCommand.ClipMinY = SourceCommand.ClipRect.y;
            TargetCommand.ClipMaxX = SourceCommand.ClipRect.z;
            TargetCommand.ClipMaxY = SourceCommand.ClipRect.w;
            TargetList.Commands.push_back(TargetCommand);
        }
    }

    Snapshot.bValid = !Snapshot.Lists.empty();
    return Snapshot;
}
}

class EditorGizmoOverlay : public QWidget
{
    friend class EditorViewportWidget;

public:
    explicit EditorGizmoOverlay(EditorViewportWidget* InOwner)
        : QWidget(InOwner->window(), Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint
            | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus)
        , Owner(InOwner)
    {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_ShowWithoutActivating, true);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setMouseTracking(true);
        setAcceptDrops(true);
        setFocusPolicy(Qt::NoFocus);
        ImGuiContextInstance = ImGui::CreateContext();
        ImGui::SetCurrentContext(ImGuiContextInstance);
        ImGui::GetIO().IniFilename = nullptr;
        unsigned char* FontPixels = nullptr;
        int FontWidth = 0;
        int FontHeight = 0;
        ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&FontPixels, &FontWidth, &FontHeight);
    }

    ~EditorGizmoOverlay() override
    {
        ImGui::DestroyContext(ImGuiContextInstance);
    }

    void SyncGeometry()
    {
        setGeometry(QRect(Owner->mapToGlobal(QPoint(0, 0)), Owner->size()));
        if (Owner->isVisible())
        {
            show();
            raise();
        }
    }

    void SetSelectedTransform(const Transform& LocalTransform, const Transform& WorldTransform)
    {
        SelectedLocal = LocalTransform;
        SelectedWorld = WorldTransform;
        bHasSelection = true;
    }

    void ClearSelectedTransform()
    {
        bHasSelection = false;
        bManipulating = false;
    }

    bool IsManipulating() const
    {
        return bManipulating;
    }

    void BuildAndSubmit()
    {
        ImGui::SetCurrentContext(ImGuiContextInstance);
        ImGuiIO& InputOutput = ImGui::GetIO();
        const qreal PixelRatio = Owner->devicePixelRatioF();
        const float DisplayWidth = static_cast<float>(width()) * static_cast<float>(PixelRatio);
        const float DisplayHeight = static_cast<float>(height()) * static_cast<float>(PixelRatio);
        InputOutput.DisplaySize = ImVec2(DisplayWidth, DisplayHeight);
        InputOutput.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
        InputOutput.DeltaTime = 1.0f / 60.0f;
        InputOutput.MousePos = ImVec2(
            static_cast<float>(MousePosition.x()) * static_cast<float>(PixelRatio),
            static_cast<float>(MousePosition.y()) * static_cast<float>(PixelRatio));
        const bool bAllowGizmoMouse =
            bLeftMouseDown
            && !Owner->IsCameraNavigationActive()
            && (QApplication::keyboardModifiers() & Qt::AltModifier) == Qt::NoModifier;
        InputOutput.MouseDown[0] = bAllowGizmoMouse;
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
        ImGuizmo::SetRect(0.0f, 0.0f, DisplayWidth, DisplayHeight);

        if (bHasSelection && !Owner->IsCameraNavigationActive() && DisplayWidth > 0.0f && DisplayHeight > 0.0f)
        {
            Matrix ViewMatrix = Matrix::CreateLookAt(
                Owner->GetViewCamera().Position,
                Owner->GetViewCamera().Target,
                Owner->GetViewCamera().Up);
            Matrix ProjectionMatrix = Matrix::CreatePerspectiveFieldOfView(
                Owner->GetViewCamera().FieldOfViewDegrees * DegreesToRadians,
                DisplayWidth / DisplayHeight,
                Owner->GetViewCamera().NearPlane,
                Owner->GetViewCamera().FarPlane);
            Matrix WorldMatrix = SelectedWorld.GetMatrix();
            ImGuizmo::OPERATION OperationMask = ImGuizmo::TRANSLATE;
            if (Owner->GetGizmoOperation() == EditorViewportWidget::GizmoOperation::Rotate)
            {
                OperationMask = ImGuizmo::ROTATE;
            }
            else if (Owner->GetGizmoOperation() == EditorViewportWidget::GizmoOperation::Scale)
            {
                OperationMask = ImGuizmo::SCALE;
            }
            const bool bChanged = ImGuizmo::Manipulate(
                &ViewMatrix._11,
                &ProjectionMatrix._11,
                OperationMask,
                ImGuizmo::LOCAL,
                &WorldMatrix._11);
            if (ImGuizmo::IsUsing() && !bManipulating)
            {
                DragStartLocal = SelectedLocal;
                bManipulating = true;
            }
            if (bChanged)
            {
                ApplyManipulatedWorld(WorldMatrix);
            }
        }

        ImGui::Render();
        if (Owner->BoundEngine != nullptr)
        {
            Owner->BoundEngine->SetRenderSurfaceImGuiOverlay(
                RenderSurfaceId{Owner->GetViewId().Value},
                CaptureImGuiOverlay(ImGui::GetDrawData()));
        }
    }

    void mousePressEvent(QMouseEvent* Event) override
    {
        Owner->setFocus(Qt::MouseFocusReason);
        MousePosition = Event->position();
        if (Event->button() == Qt::LeftButton)
        {
            ImGui::SetCurrentContext(ImGuiContextInstance);
            bLeftMouseDown = true;
            bPressedOverGizmo = bHasSelection && ImGuizmo::IsOver();
            bSelectionClickCandidate =
                !bPressedOverGizmo
                && Owner->bEditorToolsEnabled
                && !Owner->IsCameraNavigationActive()
                && (Event->modifiers() & Qt::AltModifier) == Qt::NoModifier;
            PressPosition = Event->position();
        }
        Owner->HandleCameraMousePress(Event);
    }

    void mouseMoveEvent(QMouseEvent* Event) override
    {
        MousePosition = Event->position();
        Owner->HandleCameraMouseMove(Event);
    }

    void mouseReleaseEvent(QMouseEvent* Event) override
    {
        MousePosition = Event->position();
        Owner->HandleCameraMouseRelease(Event);
        if (Event->button() == Qt::LeftButton)
        {
            bLeftMouseDown = false;
            const bool bWasManipulating = bManipulating;
            if (bManipulating && Owner->CommitCallback)
            {
                Owner->CommitCallback(DragStartLocal, SelectedLocal);
            }
            bManipulating = false;
            const QPointF ClickDelta = Event->position() - PressPosition;
            if (!bWasManipulating
                && bSelectionClickCandidate
                && ClickDelta.manhattanLength() <= 4.0
                && Owner->bEditorToolsEnabled)
            {
                emit Owner->ObjectSelectionRequested(Event->position().toPoint());
            }
            bPressedOverGizmo = false;
            bSelectionClickCandidate = false;
        }
    }

    void wheelEvent(QWheelEvent* Event) override
    {
        Owner->HandleCameraWheel(Event);
    }

    void keyPressEvent(QKeyEvent* Event) override
    {
        if (Event->matches(QKeySequence::Copy) || Event->matches(QKeySequence::Paste))
        {
            Event->ignore();
            return;
        }
        Owner->HandleCameraKeyPress(Event);
    }

    void keyReleaseEvent(QKeyEvent* Event) override
    {
        Owner->HandleCameraKeyRelease(Event);
    }

    void focusOutEvent(QFocusEvent* Event) override
    {
        Owner->ResetCameraNavigation();
        bLeftMouseDown = false;
        bManipulating = false;
        QWidget::focusOutEvent(Event);
    }

    void dragEnterEvent(QDragEnterEvent* Event) override
    {
        EditorAssetPayload Payload;
        if (DecodeEditorAssetPayload(Event->mimeData(), Payload))
        {
            Event->setDropAction(Qt::CopyAction);
            Event->accept();
        }
    }

    void dragMoveEvent(QDragMoveEvent* Event) override
    {
        EditorAssetPayload Payload;
        if (DecodeEditorAssetPayload(Event->mimeData(), Payload))
        {
            Event->setDropAction(Qt::CopyAction);
            Event->accept();
        }
    }

    void dropEvent(QDropEvent* Event) override
    {
        EditorAssetPayload Payload;
        if (!DecodeEditorAssetPayload(Event->mimeData(), Payload))
        {
            return;
        }
        emit Owner->AssetDropped(
            QString::fromStdString(Payload.Key.Asset.ToString()),
            Payload.Key.HasSubAsset() ? QString::fromStdString(Payload.Key.SubAsset->ToString()) : QString{},
            QString::fromStdString(Payload.Type.GetIdentifier()),
            Payload.VirtualPath,
            Event->position().toPoint());
        Event->setDropAction(Qt::CopyAction);
        Event->accept();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
    }

private:
    void ApplyManipulatedWorld(const Matrix& WorldMatrix)
    {
        Matrix ParentWorld = SelectedLocal.GetMatrix().Invert() * SelectedWorld.GetMatrix();
        Matrix LocalMatrix = WorldMatrix * ParentWorld.Invert();
        Transform Updated;
        if (!LocalMatrix.Decompose(Updated.Scale, Updated.Rotation, Updated.Position))
        {
            return;
        }
        Updated.Rotation.Normalize();
        SelectedLocal = Updated;
        Matrix UpdatedWorld = WorldMatrix;
        UpdatedWorld.Decompose(SelectedWorld.Scale, SelectedWorld.Rotation, SelectedWorld.Position);
        SelectedWorld.Rotation.Normalize();
        if (Owner->PreviewCallback)
        {
            Owner->PreviewCallback(Updated);
        }
    }

    EditorViewportWidget* Owner = nullptr;
    ImGuiContext* ImGuiContextInstance = nullptr;
    Transform SelectedLocal{};
    Transform SelectedWorld{};
    Transform DragStartLocal{};
    QPointF MousePosition;
    QPointF PressPosition;
    bool bHasSelection = false;
    bool bLeftMouseDown = false;
    bool bManipulating = false;
    bool bPressedOverGizmo = false;
    bool bSelectionClickCandidate = false;
};

EditorViewportWidget::EditorViewportWidget(QWidget* Parent)
    : RenderViewportWidget(Parent)
{
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    Gizmo = new EditorGizmoOverlay(this);
    Gizmo->SyncGeometry();
    NavigationTimer = new QTimer(this);
    NavigationTimer->setInterval(16);
    connect(NavigationTimer, &QTimer::timeout, this, &EditorViewportWidget::TickCameraNavigation);
}

void EditorViewportWidget::SetEngine(Engine* EngineInstance)
{
    BoundEngine = EngineInstance;
}

void EditorViewportWidget::SetSelectedTransform(const Transform& LocalTransform, const Transform& WorldTransform)
{
    Gizmo->SetSelectedTransform(LocalTransform, WorldTransform);
}

void EditorViewportWidget::ClearSelectedTransform()
{
    Gizmo->ClearSelectedTransform();
}

void EditorViewportWidget::SetTransformCallbacks(TransformPreviewCallback Preview, TransformCommitCallback Commit)
{
    PreviewCallback = std::move(Preview);
    CommitCallback = std::move(Commit);
}

void EditorViewportWidget::SetEditorToolsEnabled(bool bEnabled)
{
    bEditorToolsEnabled = bEnabled;
    if (!bEnabled)
    {
        ResetCameraNavigation();
        if (BoundEngine != nullptr)
        {
            BoundEngine->SetRenderSurfaceImGuiOverlay(RenderSurfaceId{GetViewId().Value}, {});
        }
    }
    SyncGizmoOverlay();
}

void EditorViewportWidget::SyncGizmoOverlay()
{
    if (!bEditorToolsEnabled)
    {
        Gizmo->hide();
        return;
    }
    Gizmo->SyncGeometry();
    Gizmo->BuildAndSubmit();
}

bool EditorViewportWidget::IsCameraNavigationActive() const
{
    return ActiveCameraMode != CameraMode::None;
}

void EditorViewportWidget::SetCameraMoveSpeed(float Speed)
{
    MoveSpeed = std::clamp(Speed, MinMoveSpeed, MaxMoveSpeed);
}

float EditorViewportWidget::GetCameraMoveSpeed() const
{
    return MoveSpeed;
}

void EditorViewportWidget::SetGizmoOperation(GizmoOperation Operation)
{
    if (ActiveGizmoOperation == Operation)
    {
        return;
    }
    ActiveGizmoOperation = Operation;
    emit GizmoOperationChanged(ActiveGizmoOperation);
}

EditorViewportWidget::GizmoOperation EditorViewportWidget::GetGizmoOperation() const
{
    return ActiveGizmoOperation;
}

void EditorViewportWidget::dragEnterEvent(QDragEnterEvent* Event)
{
    EditorAssetPayload Payload;
    if (DecodeEditorAssetPayload(Event->mimeData(), Payload))
    {
        Event->setDropAction(Qt::CopyAction);
        Event->accept();
    }
}

void EditorViewportWidget::dragMoveEvent(QDragMoveEvent* Event)
{
    EditorAssetPayload Payload;
    if (DecodeEditorAssetPayload(Event->mimeData(), Payload))
    {
        Event->setDropAction(Qt::CopyAction);
        Event->accept();
    }
}

void EditorViewportWidget::dropEvent(QDropEvent* Event)
{
    EditorAssetPayload Payload;
    if (!DecodeEditorAssetPayload(Event->mimeData(), Payload))
    {
        return;
    }
    emit AssetDropped(
        QString::fromStdString(Payload.Key.Asset.ToString()),
        Payload.Key.HasSubAsset() ? QString::fromStdString(Payload.Key.SubAsset->ToString()) : QString{},
        QString::fromStdString(Payload.Type.GetIdentifier()),
        Payload.VirtualPath,
        Event->position().toPoint());
    Event->setDropAction(Qt::CopyAction);
    Event->accept();
}

void EditorViewportWidget::mousePressEvent(QMouseEvent* Event)
{
    Gizmo->mousePressEvent(Event);
    Event->accept();
}

void EditorViewportWidget::mouseMoveEvent(QMouseEvent* Event)
{
    Gizmo->mouseMoveEvent(Event);
    Event->accept();
}

void EditorViewportWidget::mouseReleaseEvent(QMouseEvent* Event)
{
    Gizmo->mouseReleaseEvent(Event);
    Event->accept();
}

void EditorViewportWidget::wheelEvent(QWheelEvent* Event)
{
    HandleCameraWheel(Event);
}

void EditorViewportWidget::keyPressEvent(QKeyEvent* Event)
{
    if (Event->matches(QKeySequence::Copy) || Event->matches(QKeySequence::Paste))
    {
        Event->ignore();
        return;
    }
    HandleCameraKeyPress(Event);
}

void EditorViewportWidget::keyReleaseEvent(QKeyEvent* Event)
{
    HandleCameraKeyRelease(Event);
}

void EditorViewportWidget::focusOutEvent(QFocusEvent* Event)
{
    Gizmo->focusOutEvent(Event);
    RenderViewportWidget::focusOutEvent(Event);
}

void EditorViewportWidget::resizeEvent(QResizeEvent* Event)
{
    RenderViewportWidget::resizeEvent(Event);
    SyncGizmoOverlay();
}

void EditorViewportWidget::showEvent(QShowEvent* Event)
{
    RenderViewportWidget::showEvent(Event);
    SyncGizmoOverlay();
}

void EditorViewportWidget::hideEvent(QHideEvent* Event)
{
    ResetCameraNavigation();
    Gizmo->hide();
    RenderViewportWidget::hideEvent(Event);
}

void EditorViewportWidget::HandleCameraMousePress(QMouseEvent* Event)
{
    if (!bEditorToolsEnabled || Event == nullptr)
    {
        return;
    }

    LastMousePosition = Event->position();
    const bool bAltHeld = (Event->modifiers() & Qt::AltModifier) != 0;
    if (Event->button() == Qt::RightButton)
    {
        ActiveCameraMode = bAltHeld ? CameraMode::Dolly : CameraMode::FlyLook;
        setCursor(Qt::BlankCursor);
        if (ActiveCameraMode == CameraMode::FlyLook)
        {
            grabKeyboard();
            bKeyboardCaptured = true;
            if (!NavigationTimer->isActive())
            {
                NavigationTimer->start();
            }
        }
        Event->accept();
        return;
    }
    if (Event->button() == Qt::MiddleButton)
    {
        ActiveCameraMode = bAltHeld ? CameraMode::Pan : CameraMode::Pan;
        setCursor(Qt::ClosedHandCursor);
        Event->accept();
        return;
    }
    if (Event->button() == Qt::LeftButton && bAltHeld && !Gizmo->IsManipulating())
    {
        ActiveCameraMode = CameraMode::Orbit;
        setCursor(Qt::SizeAllCursor);
        Event->accept();
    }
}

void EditorViewportWidget::HandleCameraMouseMove(QMouseEvent* Event)
{
    if (!bEditorToolsEnabled || Event == nullptr || ActiveCameraMode == CameraMode::None)
    {
        return;
    }

    const QPointF Delta = Event->position() - LastMousePosition;
    LastMousePosition = Event->position();
    const float DeltaX = static_cast<float>(Delta.x());
    const float DeltaY = static_cast<float>(Delta.y());
    if (ActiveCameraMode == CameraMode::FlyLook)
    {
        ApplyFlyLook(DeltaX, DeltaY);
    }
    else if (ActiveCameraMode == CameraMode::Pan)
    {
        ApplyPan(DeltaX, DeltaY);
    }
    else if (ActiveCameraMode == CameraMode::Orbit)
    {
        ApplyOrbit(DeltaX, DeltaY);
    }
    else if (ActiveCameraMode == CameraMode::Dolly)
    {
        ApplyDolly(-(DeltaY + DeltaX) * DollySensitivity * GetFocusDistance());
    }
    Event->accept();
}

void EditorViewportWidget::HandleCameraMouseRelease(QMouseEvent* Event)
{
    if (Event == nullptr)
    {
        return;
    }
    if (Event->button() == Qt::RightButton
        || Event->button() == Qt::MiddleButton
        || (Event->button() == Qt::LeftButton && ActiveCameraMode == CameraMode::Orbit))
    {
        if (ActiveCameraMode == CameraMode::FlyLook)
        {
            HeldKeys.clear();
            NavigationTimer->stop();
            if (bKeyboardCaptured)
            {
                releaseKeyboard();
                bKeyboardCaptured = false;
            }
        }
        ActiveCameraMode = CameraMode::None;
        unsetCursor();
        Event->accept();
    }
}

void EditorViewportWidget::HandleCameraWheel(QWheelEvent* Event)
{
    if (!bEditorToolsEnabled || Event == nullptr)
    {
        return;
    }

    const float WheelSteps = static_cast<float>(Event->angleDelta().y()) / 120.0f;
    if (ActiveCameraMode == CameraMode::FlyLook)
    {
        const float PreviousSpeed = MoveSpeed;
        MoveSpeed = std::clamp(MoveSpeed * (WheelSteps > 0.0f ? 1.25f : 0.8f), MinMoveSpeed, MaxMoveSpeed);
        if (std::abs(PreviousSpeed - MoveSpeed) > 0.0001f)
        {
            emit CameraMoveSpeedChanged(MoveSpeed);
        }
        Event->accept();
        return;
    }

    ApplyDolly(-WheelSteps * GetFocusDistance() * WheelDollyFactor);
    Event->accept();
}

void EditorViewportWidget::HandleCameraKeyPress(QKeyEvent* Event)
{
    if (!bEditorToolsEnabled || Event == nullptr || Event->isAutoRepeat())
    {
        return;
    }
    if (Event->matches(QKeySequence::Copy) || Event->matches(QKeySequence::Paste))
    {
        return;
    }

    const quint32 ScanCode = Event->nativeScanCode();
    if (ActiveCameraMode != CameraMode::FlyLook)
    {
        if (ScanCode == ForwardKeyScanCode)
        {
            SetGizmoOperation(GizmoOperation::Translate);
            Event->accept();
            return;
        }
        if (ScanCode == RotateGizmoKeyScanCode)
        {
            SetGizmoOperation(GizmoOperation::Rotate);
            Event->accept();
            return;
        }
        if (ScanCode == ScaleGizmoKeyScanCode)
        {
            SetGizmoOperation(GizmoOperation::Scale);
            Event->accept();
            return;
        }
    }

    HeldKeys.insert(ScanCode);
    if (ActiveCameraMode == CameraMode::FlyLook)
    {
        Event->accept();
    }
}

void EditorViewportWidget::HandleCameraKeyRelease(QKeyEvent* Event)
{
    if (Event == nullptr || Event->isAutoRepeat())
    {
        return;
    }
    HeldKeys.remove(Event->nativeScanCode());
    if (ActiveCameraMode == CameraMode::FlyLook)
    {
        Event->accept();
    }
}

void EditorViewportWidget::ResetCameraNavigation()
{
    ActiveCameraMode = CameraMode::None;
    HeldKeys.clear();
    NavigationTimer->stop();
    if (bKeyboardCaptured)
    {
        releaseKeyboard();
        bKeyboardCaptured = false;
    }
    unsetCursor();
}

void EditorViewportWidget::TickCameraNavigation()
{
    if (!bEditorToolsEnabled || ActiveCameraMode != CameraMode::FlyLook)
    {
        return;
    }

    Vector3 MoveDirection = Vector3::Zero;
    const Vector3 Forward = GetCameraForward();
    const Vector3 Right = GetCameraRight();
    if (HeldKeys.contains(ForwardKeyScanCode))
    {
        MoveDirection += Forward;
    }
    if (HeldKeys.contains(BackwardKeyScanCode))
    {
        MoveDirection -= Forward;
    }
    if (HeldKeys.contains(RightKeyScanCode))
    {
        MoveDirection += Right;
    }
    if (HeldKeys.contains(LeftKeyScanCode))
    {
        MoveDirection -= Right;
    }
    if (HeldKeys.contains(UpKeyScanCode) || HeldKeys.contains(SpaceKeyScanCode))
    {
        MoveDirection += Vector3::Up;
    }
    if (HeldKeys.contains(DownKeyScanCode))
    {
        MoveDirection -= Vector3::Up;
    }
    if (MoveDirection.LengthSquared() <= 0.000001f)
    {
        return;
    }
    MoveDirection.Normalize();
    MoveCamera(MoveDirection * GetCurrentMoveSpeed() * (16.0f / 1000.0f));
}

void EditorViewportWidget::ApplyFlyLook(float DeltaX, float DeltaY)
{
    float YawDegrees = 0.0f;
    float PitchDegrees = 0.0f;
    ExtractYawPitch(GetCameraForward(), YawDegrees, PitchDegrees);
    YawDegrees -= DeltaX * LookSensitivity;
    PitchDegrees = ClampPitch(PitchDegrees - DeltaY * LookSensitivity);
    const Vector3 Forward = DirectionFromYawPitch(YawDegrees, PitchDegrees);
    RenderViewCamera Updated = GetViewCamera();
    Updated.Target = Updated.Position + Forward * GetFocusDistance();
    CommitCamera(Updated);
}

void EditorViewportWidget::ApplyPan(float DeltaX, float DeltaY)
{
    const float Distance = GetFocusDistance();
    const Vector3 Right = GetCameraRight();
    const Vector3 Up = Right.Cross(GetCameraForward());
    MoveCamera((-Right * DeltaX + Up * DeltaY) * Distance * PanSensitivity);
}

void EditorViewportWidget::ApplyOrbit(float DeltaX, float DeltaY)
{
    RenderViewCamera Updated = GetViewCamera();
    Vector3 Offset = Updated.Position - Updated.Target;
    float Distance = Offset.Length();
    if (Distance < MinFocusDistance)
    {
        Distance = MinFocusDistance;
    }
    float YawDegrees = 0.0f;
    float PitchDegrees = 0.0f;
    ExtractYawPitch(Offset, YawDegrees, PitchDegrees);
    YawDegrees -= DeltaX * OrbitSensitivity;
    PitchDegrees = ClampPitch(PitchDegrees - DeltaY * OrbitSensitivity);
    Updated.Position = Updated.Target + DirectionFromYawPitch(YawDegrees, PitchDegrees) * Distance;
    CommitCamera(Updated);
}

void EditorViewportWidget::ApplyDolly(float DistanceDelta)
{
    RenderViewCamera Updated = GetViewCamera();
    const Vector3 Forward = GetCameraForward();
    float Distance = GetFocusDistance() + DistanceDelta;
    if (Distance < MinFocusDistance)
    {
        Distance = MinFocusDistance;
    }
    Updated.Position = Updated.Target - Forward * Distance;
    CommitCamera(Updated);
}

void EditorViewportWidget::MoveCamera(const Vector3& WorldDelta)
{
    RenderViewCamera Updated = GetViewCamera();
    Updated.Position += WorldDelta;
    Updated.Target += WorldDelta;
    CommitCamera(Updated);
}

void EditorViewportWidget::CommitCamera(const RenderViewCamera& Updated)
{
    SetViewCamera(Updated);
    emit CameraChanged();
}

Vector3 EditorViewportWidget::GetCameraForward() const
{
    Vector3 Forward = GetViewCamera().Target - GetViewCamera().Position;
    if (Forward.LengthSquared() <= 0.000001f)
    {
        return Vector3::Forward;
    }
    Forward.Normalize();
    return Forward;
}

Vector3 EditorViewportWidget::GetCameraRight() const
{
    Vector3 Right = GetCameraForward().Cross(GetViewCamera().Up);
    if (Right.LengthSquared() <= 0.000001f)
    {
        Right = Vector3::Right;
    }
    Right.Normalize();
    return Right;
}

float EditorViewportWidget::GetFocusDistance() const
{
    return std::max(MinFocusDistance, (GetViewCamera().Target - GetViewCamera().Position).Length());
}

float EditorViewportWidget::GetCurrentMoveSpeed() const
{
    float Speed = MoveSpeed;
    if ((QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0)
    {
        Speed *= 4.0f;
    }
    if ((QApplication::keyboardModifiers() & Qt::ControlModifier) != 0)
    {
        Speed *= 0.25f;
    }
    return Speed;
}
