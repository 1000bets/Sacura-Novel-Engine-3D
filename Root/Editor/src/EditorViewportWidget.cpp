#include "EditorViewportWidget.h"

#include "EditorAssetMime.h"

#include <imgui.h>
#include <ImGuizmo.h>

#include <QApplication>
#include <QCursor>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

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
}

class EditorGizmoOverlay : public QWidget
{
public:
    explicit EditorGizmoOverlay(EditorViewportWidget* InOwner)
        : QWidget(InOwner->window(), Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
        , Owner(InOwner)
    {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_ShowWithoutActivating, true);
        setMouseTracking(true);
        setAcceptDrops(true);
        setFocusPolicy(Qt::StrongFocus);
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
            update();
        }
    }

    void SetSelectedTransform(const Transform& LocalTransform, const Transform& WorldTransform)
    {
        SelectedLocal = LocalTransform;
        SelectedWorld = WorldTransform;
        bHasSelection = true;
        update();
    }

    void ClearSelectedTransform()
    {
        bHasSelection = false;
        bManipulating = false;
        update();
    }

    bool IsManipulating() const
    {
        return bManipulating;
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        ImGui::SetCurrentContext(ImGuiContextInstance);
        ImGuiIO& InputOutput = ImGui::GetIO();
        InputOutput.DisplaySize = ImVec2(static_cast<float>(width()), static_cast<float>(height()));
        InputOutput.DeltaTime = 1.0f / 60.0f;
        InputOutput.MousePos = ImVec2(static_cast<float>(MousePosition.x()), static_cast<float>(MousePosition.y()));
        const bool bAllowGizmoMouse =
            bLeftMouseDown
            && !Owner->IsCameraNavigationActive()
            && (QApplication::keyboardModifiers() & Qt::AltModifier) == Qt::NoModifier;
        InputOutput.MouseDown[0] = bAllowGizmoMouse;
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
        ImGuizmo::SetRect(0.0f, 0.0f, static_cast<float>(width()), static_cast<float>(height()));

        if (bHasSelection && !Owner->IsCameraNavigationActive())
        {
            Matrix ViewMatrix = Matrix::CreateLookAt(
                Owner->GetViewCamera().Position,
                Owner->GetViewCamera().Target,
                Owner->GetViewCamera().Up);
            Matrix ProjectionMatrix = Matrix::CreatePerspectiveFieldOfView(
                Owner->GetViewCamera().FieldOfViewDegrees * DegreesToRadians,
                static_cast<float>(std::max(1, width())) / static_cast<float>(std::max(1, height())),
                Owner->GetViewCamera().NearPlane,
                Owner->GetViewCamera().FarPlane);
            Matrix WorldMatrix = SelectedWorld.GetMatrix();
            Matrix ImGuizmoView = ViewMatrix.Transpose();
            Matrix ImGuizmoProjection = ProjectionMatrix.Transpose();
            Matrix ImGuizmoWorld = WorldMatrix.Transpose();
            const ImGuizmo::OPERATION OperationMask =
                ImGuizmo::TRANSLATE | ImGuizmo::ROTATE | ImGuizmo::SCALE;
            const bool bChanged = ImGuizmo::Manipulate(
                &ImGuizmoView._11,
                &ImGuizmoProjection._11,
                OperationMask,
                ImGuizmo::LOCAL,
                &ImGuizmoWorld._11);
            if (bChanged)
            {
                ApplyManipulatedWorld(ImGuizmoWorld.Transpose());
            }
            if (ImGuizmo::IsUsing() && !bManipulating)
            {
                DragStartLocal = SelectedLocal;
                bManipulating = true;
            }
        }

        ImGui::Render();
        QPainter Painter(this);
        Painter.setRenderHint(QPainter::Antialiasing, true);
        DrawImGui(Painter, ImGui::GetDrawData());
    }

    void mousePressEvent(QMouseEvent* Event) override
    {
        Owner->setFocus(Qt::MouseFocusReason);
        MousePosition = Event->position();
        if (Event->button() == Qt::LeftButton)
        {
            ImGui::SetCurrentContext(ImGuiContextInstance);
            bLeftMouseDown = true;
            bPressedOverGizmo = ImGuizmo::IsOver();
            bSelectionClickCandidate =
                !bPressedOverGizmo
                && Owner->bEditorToolsEnabled
                && !Owner->IsCameraNavigationActive()
                && (Event->modifiers() & Qt::AltModifier) == Qt::NoModifier;
            PressPosition = Event->position();
        }
        Owner->HandleCameraMousePress(Event);
        update();
    }

    void mouseMoveEvent(QMouseEvent* Event) override
    {
        MousePosition = Event->position();
        Owner->HandleCameraMouseMove(Event);
        update();
    }

    void mouseReleaseEvent(QMouseEvent* Event) override
    {
        MousePosition = Event->position();
        Owner->HandleCameraMouseRelease(Event);
        if (Event->button() == Qt::LeftButton)
        {
            bLeftMouseDown = false;
            update();
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

    void DrawImGui(QPainter& Painter, const ImDrawData* DrawData)
    {
        if (DrawData == nullptr)
        {
            return;
        }
        for (int ListIndex = 0; ListIndex < DrawData->CmdListsCount; ++ListIndex)
        {
            const ImDrawList* DrawList = DrawData->CmdLists[ListIndex];
            for (int CommandIndex = 0; CommandIndex < DrawList->CmdBuffer.Size; ++CommandIndex)
            {
                const ImDrawCmd& Command = DrawList->CmdBuffer[CommandIndex];
                Painter.save();
                Painter.setClipRect(QRectF(
                    Command.ClipRect.x,
                    Command.ClipRect.y,
                    Command.ClipRect.z - Command.ClipRect.x,
                    Command.ClipRect.w - Command.ClipRect.y));
                for (unsigned int ElementIndex = 0; ElementIndex + 2 < Command.ElemCount; ElementIndex += 3)
                {
                    QPolygonF Triangle;
                    QColor TriangleColor;
                    ImVec2 FirstTextureCoordinate;
                    bool bSolidTriangle = true;
                    for (unsigned int VertexOffset = 0; VertexOffset < 3; ++VertexOffset)
                    {
                        const ImDrawIdx VertexIndex = DrawList->IdxBuffer[
                            static_cast<int>(Command.IdxOffset + ElementIndex + VertexOffset)];
                        const ImDrawVert& Vertex = DrawList->VtxBuffer[
                            static_cast<int>(Command.VtxOffset + VertexIndex)];
                        Triangle << QPointF(Vertex.pos.x, Vertex.pos.y);
                        if (VertexOffset == 0)
                        {
                            FirstTextureCoordinate = Vertex.uv;
                            TriangleColor = QColor(
                                static_cast<int>((Vertex.col >> IM_COL32_R_SHIFT) & 0xff),
                                static_cast<int>((Vertex.col >> IM_COL32_G_SHIFT) & 0xff),
                                static_cast<int>((Vertex.col >> IM_COL32_B_SHIFT) & 0xff),
                                static_cast<int>((Vertex.col >> IM_COL32_A_SHIFT) & 0xff));
                        }
                        else if (std::abs(Vertex.uv.x - FirstTextureCoordinate.x) > 0.000001f
                            || std::abs(Vertex.uv.y - FirstTextureCoordinate.y) > 0.000001f)
                        {
                            bSolidTriangle = false;
                        }
                    }
                    if (!bSolidTriangle)
                    {
                        continue;
                    }
                    Painter.setPen(Qt::NoPen);
                    Painter.setBrush(TriangleColor);
                    Painter.drawPolygon(Triangle);
                }
                Painter.restore();
            }
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
    setFocus();
    HandleCameraMousePress(Event);
    RenderViewportWidget::mousePressEvent(Event);
}

void EditorViewportWidget::mouseMoveEvent(QMouseEvent* Event)
{
    HandleCameraMouseMove(Event);
    RenderViewportWidget::mouseMoveEvent(Event);
}

void EditorViewportWidget::mouseReleaseEvent(QMouseEvent* Event)
{
    HandleCameraMouseRelease(Event);
    RenderViewportWidget::mouseReleaseEvent(Event);
}

void EditorViewportWidget::wheelEvent(QWheelEvent* Event)
{
    HandleCameraWheel(Event);
}

void EditorViewportWidget::keyPressEvent(QKeyEvent* Event)
{
    HandleCameraKeyPress(Event);
}

void EditorViewportWidget::keyReleaseEvent(QKeyEvent* Event)
{
    HandleCameraKeyRelease(Event);
}

void EditorViewportWidget::focusOutEvent(QFocusEvent* Event)
{
    ResetCameraNavigation();
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
    HeldKeys.insert(Event->key());
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
    HeldKeys.remove(Event->key());
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
    if (HeldKeys.contains(Qt::Key_W))
    {
        MoveDirection += Forward;
    }
    if (HeldKeys.contains(Qt::Key_S))
    {
        MoveDirection -= Forward;
    }
    if (HeldKeys.contains(Qt::Key_D))
    {
        MoveDirection += Right;
    }
    if (HeldKeys.contains(Qt::Key_A))
    {
        MoveDirection -= Right;
    }
    if (HeldKeys.contains(Qt::Key_E) || HeldKeys.contains(Qt::Key_Space))
    {
        MoveDirection += Vector3::Up;
    }
    if (HeldKeys.contains(Qt::Key_Q))
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
