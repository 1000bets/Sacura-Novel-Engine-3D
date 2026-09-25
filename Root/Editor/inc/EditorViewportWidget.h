#pragma once

#include "Core/Transform.h"
#include "UI/RenderViewportWidget.h"

#include <QPointF>
#include <QSet>
#include <functional>

class EditorGizmoOverlay;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QFocusEvent;
class QKeyEvent;
class QMouseEvent;
class QTimer;
class QWheelEvent;

class EditorViewportWidget : public RenderViewportWidget
{
    Q_OBJECT

public:
    using TransformPreviewCallback = std::function<void(const Transform& Value)>;
    using TransformCommitCallback = std::function<void(const Transform& OldValue, const Transform& NewValue)>;

    explicit EditorViewportWidget(QWidget* Parent = nullptr);

    void SetSelectedTransform(const Transform& LocalTransform, const Transform& WorldTransform);
    void ClearSelectedTransform();
    void SetTransformCallbacks(TransformPreviewCallback Preview, TransformCommitCallback Commit);
    void SetEditorToolsEnabled(bool bEnabled);
    void SyncGizmoOverlay();
    bool IsCameraNavigationActive() const;
    void SetCameraMoveSpeed(float Speed);
    float GetCameraMoveSpeed() const;

signals:
    void AssetDropped(QString AssetId, QString SubAssetId, QString AssetTypeIdentifier, QString VirtualPath, QPoint Position);
    void ObjectSelectionRequested(QPoint Position);
    void CameraChanged();
    void CameraMoveSpeedChanged(float Speed);

protected:
    void dragEnterEvent(QDragEnterEvent* Event) override;
    void dragMoveEvent(QDragMoveEvent* Event) override;
    void dropEvent(QDropEvent* Event) override;
    void mousePressEvent(QMouseEvent* Event) override;
    void mouseMoveEvent(QMouseEvent* Event) override;
    void mouseReleaseEvent(QMouseEvent* Event) override;
    void wheelEvent(QWheelEvent* Event) override;
    void keyPressEvent(QKeyEvent* Event) override;
    void keyReleaseEvent(QKeyEvent* Event) override;
    void focusOutEvent(QFocusEvent* Event) override;
    void resizeEvent(QResizeEvent* Event) override;
    void showEvent(QShowEvent* Event) override;
    void hideEvent(QHideEvent* Event) override;

private:
    friend class EditorGizmoOverlay;

    enum class CameraMode
    {
        None,
        FlyLook,
        Pan,
        Orbit,
        Dolly
    };

    void HandleCameraMousePress(QMouseEvent* Event);
    void HandleCameraMouseMove(QMouseEvent* Event);
    void HandleCameraMouseRelease(QMouseEvent* Event);
    void HandleCameraWheel(QWheelEvent* Event);
    void HandleCameraKeyPress(QKeyEvent* Event);
    void HandleCameraKeyRelease(QKeyEvent* Event);
    void ResetCameraNavigation();
    void TickCameraNavigation();
    void ApplyFlyLook(float DeltaX, float DeltaY);
    void ApplyPan(float DeltaX, float DeltaY);
    void ApplyOrbit(float DeltaX, float DeltaY);
    void ApplyDolly(float DistanceDelta);
    void MoveCamera(const Vector3& WorldDelta);
    void CommitCamera(const RenderViewCamera& Updated);
    Vector3 GetCameraForward() const;
    Vector3 GetCameraRight() const;
    float GetFocusDistance() const;
    float GetCurrentMoveSpeed() const;

    EditorGizmoOverlay* Gizmo = nullptr;
    TransformPreviewCallback PreviewCallback;
    TransformCommitCallback CommitCallback;
    QTimer* NavigationTimer = nullptr;
    QPointF LastMousePosition;
    QSet<int> HeldKeys;
    CameraMode ActiveCameraMode = CameraMode::None;
    float MoveSpeed = 8.0f;
    bool bEditorToolsEnabled = false;
    bool bKeyboardCaptured = false;
};
