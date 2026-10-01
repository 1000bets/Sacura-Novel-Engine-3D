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



void StoreObjectHandle(QTreeWidgetItem* Item, ObjectHandle Handle)
{
    Item->setData(0, HierarchyObjectIdRole, QVariant::fromValue<qulonglong>(Handle.Id));
    Item->setData(0, HierarchyObjectGenerationRole, QVariant::fromValue<uint>(Handle.Generation));
}

ObjectHandle LoadObjectHandle(const QTreeWidgetItem* Item)
{
    ObjectHandle Handle;
    Handle.Id = static_cast<ObjectID>(Item->data(0, HierarchyObjectIdRole).toULongLong());
    Handle.Generation = Item->data(0, HierarchyObjectGenerationRole).toUInt();
    return Handle;
}

bool HierarchyItemIsDescendant(const QTreeWidgetItem* Ancestor, const QTreeWidgetItem* Candidate)
{
    for (const QTreeWidgetItem* Walk = Candidate; Walk != nullptr; Walk = Walk->parent())
    {
        if (Walk == Ancestor)
        {
            return true;
        }
    }
    return false;
}

Vector3 QuaternionToEulerDegrees(const Quaternion& Rotation)
{
    const float PitchSin = 2.0f * (Rotation.w * Rotation.y - Rotation.z * Rotation.x);
    return Vector3(
        std::atan2(
            2.0f * (Rotation.w * Rotation.x + Rotation.y * Rotation.z),
            1.0f - 2.0f * (Rotation.x * Rotation.x + Rotation.y * Rotation.y)) * RadiansToDegrees,
        std::asin(std::clamp(PitchSin, -1.0f, 1.0f)) * RadiansToDegrees,
        std::atan2(
            2.0f * (Rotation.w * Rotation.z + Rotation.x * Rotation.y),
            1.0f - 2.0f * (Rotation.y * Rotation.y + Rotation.z * Rotation.z)) * RadiansToDegrees);
}

bool RayIntersectsBounds(
    const Vector3& RayOrigin,
    const Vector3& RayDirection,
    const AxisAlignedBounds& Bounds,
    float& OutDistance)
{
    float MinimumDistance = 0.0f;
    float MaximumDistance = std::numeric_limits<float>::max();
    auto TestAxis = [&](float Origin, float Direction, float Minimum, float Maximum)
    {
        if (std::abs(Direction) < 0.000001f)
        {
            return Origin >= Minimum && Origin <= Maximum;
        }
        float FirstDistance = (Minimum - Origin) / Direction;
        float SecondDistance = (Maximum - Origin) / Direction;
        if (FirstDistance > SecondDistance)
        {
            std::swap(FirstDistance, SecondDistance);
        }
        MinimumDistance = std::max(MinimumDistance, FirstDistance);
        MaximumDistance = std::min(MaximumDistance, SecondDistance);
        return MinimumDistance <= MaximumDistance;
    };

    if (!TestAxis(RayOrigin.x, RayDirection.x, Bounds.Minimum.x, Bounds.Maximum.x)
        || !TestAxis(RayOrigin.y, RayDirection.y, Bounds.Minimum.y, Bounds.Maximum.y)
        || !TestAxis(RayOrigin.z, RayDirection.z, Bounds.Minimum.z, Bounds.Maximum.z))
    {
        return false;
    }
    OutDistance = MinimumDistance;
    return MaximumDistance >= 0.0f;
}
