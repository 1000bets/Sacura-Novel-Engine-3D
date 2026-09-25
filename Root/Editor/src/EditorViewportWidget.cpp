#include "EditorViewportWidget.h"

#include "EditorAssetMime.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>

#include <algorithm>
#include <array>
#include <cmath>

using namespace DirectX::SimpleMath;

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
        setFocusPolicy(Qt::ClickFocus);
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
        update();
    }

    void ClearSelectedTransform()
    {
        bHasSelection = false;
        bDragging = false;
        update();
    }

    void SetOperation(EditorGizmoOperation InOperation)
    {
        Operation = InOperation;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter Painter(this);
        Painter.setRenderHint(QPainter::Antialiasing, true);
        Painter.setPen(QColor(225, 215, 222));
        Painter.drawText(QRect(8, 6, 210, 22), Qt::AlignLeft | Qt::AlignVCenter,
            Operation == EditorGizmoOperation::Translate ? tr("W  Move")
            : Operation == EditorGizmoOperation::Rotate ? tr("E  Rotate")
            : tr("R  Scale"));
        if (!bHasSelection)
        {
            return;
        }
        QPointF Center;
        if (!Project(SelectedWorld.Position, Center))
        {
            return;
        }
        const std::array<Vector3, 3> Axes = {Vector3::Right, Vector3::Up, Vector3::Backward};
        const std::array<QColor, 3> Colors = {QColor(235, 72, 72), QColor(82, 220, 112), QColor(80, 145, 245)};
        for (int AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
        {
            Painter.setPen(QPen(Colors[AxisIndex], ActiveAxis == AxisIndex ? 5.0 : 3.0));
            if (Operation == EditorGizmoOperation::Rotate)
            {
                const double Radius = 35.0 + AxisIndex * 7.0;
                Painter.drawEllipse(Center, Radius, Radius);
                continue;
            }
            QPointF End;
            if (!Project(SelectedWorld.Position + Axes[AxisIndex], End))
            {
                continue;
            }
            QLineF Direction(Center, End);
            Direction.setLength(72.0);
            Painter.drawLine(Direction);
            if (Operation == EditorGizmoOperation::Translate)
            {
                Painter.drawEllipse(Direction.p2(), 4.0, 4.0);
            }
            else
            {
                Painter.drawRect(QRectF(Direction.p2().x() - 4.0, Direction.p2().y() - 4.0, 8.0, 8.0));
            }
        }
    }

    void keyPressEvent(QKeyEvent* Event) override
    {
        if (Event->key() == Qt::Key_W)
        {
            SetOperation(EditorGizmoOperation::Translate);
        }
        else if (Event->key() == Qt::Key_E)
        {
            SetOperation(EditorGizmoOperation::Rotate);
        }
        else if (Event->key() == Qt::Key_R)
        {
            SetOperation(EditorGizmoOperation::Scale);
        }
        else
        {
            QWidget::keyPressEvent(Event);
        }
    }

    void mousePressEvent(QMouseEvent* Event) override
    {
        setFocus();
        if (Event->button() != Qt::LeftButton || !bHasSelection)
        {
            return;
        }
        ActiveAxis = HitTest(Event->position());
        if (ActiveAxis < 0)
        {
            update();
            return;
        }
        DragStart = Event->position();
        DragStartLocal = SelectedLocal;
        bDragging = true;
        update();
    }

    void mouseMoveEvent(QMouseEvent* Event) override
    {
        if (!bDragging || ActiveAxis < 0)
        {
            return;
        }
        const QPointF Movement = Event->position() - DragStart;
        const float SignedPixels = static_cast<float>(Movement.x() - Movement.y());
        Transform Updated = DragStartLocal;
        const Vector3 Axis = ActiveAxis == 0 ? Vector3::Right : ActiveAxis == 1 ? Vector3::Up : Vector3::Backward;
        if (Operation == EditorGizmoOperation::Translate)
        {
            Updated.Position += Axis * (SignedPixels * 0.015f);
        }
        else if (Operation == EditorGizmoOperation::Rotate)
        {
            Updated.Rotation = Quaternion::CreateFromAxisAngle(Axis, SignedPixels * 0.01f) * DragStartLocal.Rotation;
            Updated.Rotation.Normalize();
        }
        else
        {
            const float Amount = SignedPixels * 0.01f;
            if (ActiveAxis == 0)
            {
                Updated.Scale.x = std::max(0.001f, DragStartLocal.Scale.x + Amount);
            }
            else if (ActiveAxis == 1)
            {
                Updated.Scale.y = std::max(0.001f, DragStartLocal.Scale.y + Amount);
            }
            else
            {
                Updated.Scale.z = std::max(0.001f, DragStartLocal.Scale.z + Amount);
            }
        }
        SelectedLocal = Updated;
        if (Owner->PreviewCallback)
        {
            Owner->PreviewCallback(Updated);
        }
        update();
    }

    void mouseReleaseEvent(QMouseEvent* Event) override
    {
        if (Event->button() == Qt::LeftButton && bDragging)
        {
            bDragging = false;
            if (Owner->CommitCallback)
            {
                Owner->CommitCallback(DragStartLocal, SelectedLocal);
            }
            ActiveAxis = -1;
            update();
        }
    }

    void dragEnterEvent(QDragEnterEvent* Event) override
    {
        EditorAssetPayload Payload;
        if (DecodeEditorAssetPayload(Event->mimeData(), Payload))
        {
            Event->acceptProposedAction();
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
            static_cast<int>(Payload.Type),
            Payload.VirtualPath,
            Event->position().toPoint());
        Event->acceptProposedAction();
    }

private:
    bool Project(const Vector3& Position, QPointF& OutPosition) const
    {
        const RenderViewCamera& Camera = Owner->GetViewCamera();
        const float AspectRatio = static_cast<float>(std::max(1, width())) / static_cast<float>(std::max(1, height()));
        const Matrix ViewProjection = Matrix::CreateLookAt(Camera.Position, Camera.Target, Camera.Up)
            * Matrix::CreatePerspectiveFieldOfView(
                Camera.FieldOfViewDegrees * 0.0174532925f,
                AspectRatio,
                Camera.NearPlane,
                Camera.FarPlane);
        const Vector4 Clip = Vector4::Transform(Vector4(Position.x, Position.y, Position.z, 1.f), ViewProjection);
        if (Clip.w <= 0.001f)
        {
            return false;
        }
        OutPosition = QPointF(
            (Clip.x / Clip.w * 0.5f + 0.5f) * width(),
            (0.5f - Clip.y / Clip.w * 0.5f) * height());
        return true;
    }

    int HitTest(const QPointF& Position) const
    {
        QPointF Center;
        if (!Project(SelectedWorld.Position, Center))
        {
            return -1;
        }
        if (Operation == EditorGizmoOperation::Rotate)
        {
            const double Distance = QLineF(Center, Position).length();
            for (int AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
            {
                if (std::abs(Distance - (35.0 + AxisIndex * 7.0)) <= 6.0)
                {
                    return AxisIndex;
                }
            }
            return -1;
        }
        const std::array<Vector3, 3> Axes = {Vector3::Right, Vector3::Up, Vector3::Backward};
        double BestDistance = 10.0;
        int BestAxis = -1;
        for (int AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
        {
            QPointF AxisPoint;
            if (!Project(SelectedWorld.Position + Axes[AxisIndex], AxisPoint))
            {
                continue;
            }
            QLineF AxisLine(Center, AxisPoint);
            AxisLine.setLength(72.0);
            const QPointF Direction = AxisLine.p2() - AxisLine.p1();
            const double LengthSquared = Direction.x() * Direction.x() + Direction.y() * Direction.y();
            const QPointF Offset = Position - Center;
            const double Alpha = std::clamp(
                (Offset.x() * Direction.x() + Offset.y() * Direction.y()) / LengthSquared,
                0.0,
                1.0);
            const QPointF Closest = Center + Direction * Alpha;
            const double Distance = QLineF(Closest, Position).length();
            if (Distance < BestDistance)
            {
                BestDistance = Distance;
                BestAxis = AxisIndex;
            }
        }
        return BestAxis;
    }

    EditorViewportWidget* Owner = nullptr;
    Transform SelectedLocal{};
    Transform SelectedWorld{};
    Transform DragStartLocal{};
    QPointF DragStart;
    EditorGizmoOperation Operation = EditorGizmoOperation::Translate;
    int ActiveAxis = -1;
    bool bHasSelection = false;
    bool bDragging = false;
};

EditorViewportWidget::EditorViewportWidget(QWidget* Parent)
    : RenderViewportWidget(Parent)
{
    Gizmo = new EditorGizmoOverlay(this);
    Gizmo->SyncGeometry();
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

void EditorViewportWidget::SetGizmoOperation(EditorGizmoOperation Operation)
{
    Gizmo->SetOperation(Operation);
}

void EditorViewportWidget::SetEditorToolsEnabled(bool bEnabled)
{
    bEditorToolsEnabled = bEnabled;
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
    Gizmo->hide();
    RenderViewportWidget::hideEvent(Event);
}
