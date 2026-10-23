#include "Prefab/PrefabEditorWindow.h"
#include "Commands/StaticMeshCommands.h"

#include "Actions/EditorAction.h"
#include "Actions/EditorActionContext.h"
#include "Panels/EditorAssetMime.h"
#include "Shell/EditorClipboard.h"
#include "Commands/EditorCommands.h"
#include "Shell/EditorTheme.h"
#include "Viewport/EditorViewportWidget.h"
#include "Engine.h"
#include "World/Scene.h"
#include "World/Serialization/SceneSerializer.h"
#include "World/Components/Component.h"
#include "World/GameObject.h"
#include "World/Components/MeshRendererComponent.h"
#include "Core/Object/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Prefab/PrefabLibrary.h"
#include "Prefab/PrefabUtility.h"
#include "Commands/PrefabCommands.h"
#include "Prefab/PrefabOverridesPanel.h"
#include "Reflection/Class.h"
#include "Panels/ReflectionInspector.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
constexpr int ObjectIdRole = Qt::UserRole;
constexpr int ObjectGenerationRole = Qt::UserRole + 1;
constexpr float DegreesToRadians = 0.01745329251994329577f;
constexpr float RadiansToDegrees = 57.295779513082320876f;

// Prefab editor surfaces live far from the main viewport id (1) so several windows can coexist.
uint32_t AllocateViewId()
{
    static uint32_t NextViewId = 1000;
    return NextViewId++;
}

void StoreHandle(QTreeWidgetItem* Item, ObjectHandle Handle)
{
    Item->setData(0, ObjectIdRole, QVariant::fromValue<qulonglong>(Handle.Id));
    Item->setData(0, ObjectGenerationRole, QVariant::fromValue<uint>(Handle.Generation));
}

ObjectHandle LoadHandle(const QTreeWidgetItem* Item)
{
    ObjectHandle Handle;
    Handle.Id = static_cast<ObjectID>(Item->data(0, ObjectIdRole).toULongLong());
    Handle.Generation = Item->data(0, ObjectGenerationRole).toUInt();
    return Handle;
}

bool IsDescendantItem(const QTreeWidgetItem* Ancestor, const QTreeWidgetItem* Candidate)
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

bool RayHitsBounds(const Vector3& Origin, const Vector3& Direction, const AxisAlignedBounds& Bounds, float& OutDistance)
{
    float Near = 0.0f;
    float Far = std::numeric_limits<float>::max();
    auto Axis = [&](float O, float D, float Minimum, float Maximum)
    {
        if (std::abs(D) < 0.000001f)
        {
            return O >= Minimum && O <= Maximum;
        }
        float First = (Minimum - O) / D;
        float Second = (Maximum - O) / D;
        if (First > Second)
        {
            std::swap(First, Second);
        }
        Near = std::max(Near, First);
        Far = std::min(Far, Second);
        return Near <= Far;
    };
    if (!Axis(Origin.x, Direction.x, Bounds.Minimum.x, Bounds.Maximum.x)
        || !Axis(Origin.y, Direction.y, Bounds.Minimum.y, Bounds.Maximum.y)
        || !Axis(Origin.z, Direction.z, Bounds.Minimum.z, Bounds.Maximum.z))
    {
        return false;
    }
    OutDistance = Near;
    return Far >= 0.0f;
}

// Hierarchy for the preview scene: internal drag = reparent, Content Browser drop = add asset.
class PrefabHierarchyTree : public QTreeWidget
{
public:
    using QTreeWidget::QTreeWidget;
    std::function<bool(ObjectHandle, ObjectHandle)> ReparentRequested;
    std::function<void(const EditorAssetPayload&, ObjectHandle)> AssetDropped;

protected:
    void dragEnterEvent(QDragEnterEvent* Event) override
    {
        if (Event->source() == this || Event->mimeData()->hasFormat(SakuraAssetMimeType))
        {
            if (Event->source() == this)
            {
                QTreeWidget::dragEnterEvent(Event);
            }
            Event->setDropAction(Qt::CopyAction);
            Event->accept();
            return;
        }
        Event->ignore();
    }

    void dragMoveEvent(QDragMoveEvent* Event) override
    {
        if (Event->mimeData()->hasFormat(SakuraAssetMimeType))
        {
            Event->setDropAction(Qt::CopyAction);
            Event->accept();
            return;
        }
        if (Event->source() != this)
        {
            Event->ignore();
            return;
        }
        QTreeWidget::dragMoveEvent(Event);
        QTreeWidgetItem* Dragged = currentItem();
        QTreeWidgetItem* Target = itemAt(Event->position().toPoint());
        if (Dragged != nullptr && Target != nullptr && dropIndicatorPosition() == QAbstractItemView::OnItem
            && IsDescendantItem(Dragged, Target))
        {
            Event->ignore();
            return;
        }
        Event->setDropAction(Qt::CopyAction);
        Event->accept();
    }

    void dropEvent(QDropEvent* Event) override
    {
        QTreeWidgetItem* Target = itemAt(Event->position().toPoint());
        if (Event->mimeData()->hasFormat(SakuraAssetMimeType))
        {
            EditorAssetPayload Payload;
            if (AssetDropped && DecodeEditorAssetPayload(Event->mimeData(), Payload))
            {
                AssetDropped(Payload, Target != nullptr ? LoadHandle(Target) : ObjectHandle{});
            }
            Event->setDropAction(Qt::IgnoreAction);
            Event->accept();
            return;
        }
        QTreeWidgetItem* Dragged = currentItem();
        if (Dragged == nullptr || !ReparentRequested || Event->source() != this)
        {
            Event->ignore();
            return;
        }
        ObjectHandle NewParent;
        const QAbstractItemView::DropIndicatorPosition Position = dropIndicatorPosition();
        if (Target != nullptr && Position == QAbstractItemView::OnItem)
        {
            if (Target == Dragged || IsDescendantItem(Dragged, Target))
            {
                Event->setDropAction(Qt::IgnoreAction);
                Event->accept();
                return;
            }
            NewParent = LoadHandle(Target);
        }
        else if (Target != nullptr && Target->parent() != nullptr)
        {
            NewParent = LoadHandle(Target->parent());
        }
        ReparentRequested(LoadHandle(Dragged), NewParent);
        Event->setDropAction(Qt::IgnoreAction);
        Event->accept();
    }
};
}

// ============================================================================ lifetime

PrefabEditorWindow::PrefabEditorWindow(Engine& InEngine, const AssetId& InPrefab, QWidget* Parent)
    : QMainWindow(Parent)
    , BoundEngine(InEngine)
    , Prefab(InPrefab)
    , ViewId(RenderViewId{AllocateViewId()})
{
    setObjectName("PrefabEditorWindow");
    setAttribute(Qt::WA_DeleteOnClose, true);
    EditorUi::EditorStyle::Apply(*this);
    resize(1280, 800);

    CommandStack.SetChangedCallback([this]()
    {
        bDirty = true;
        UpdateTitle();
    });

    BuildToolbar();
    BuildUi();

    LibraryListener = PrefabLibrary::Get().AddChangeListener([this](const AssetId& Changed)
    {
        OnPrefabChanged(Changed);
    });

    TickTimer = new QTimer(this);
    connect(TickTimer, &QTimer::timeout, this, &PrefabEditorWindow::OnTick);
    TickTimer->start(16);
}

PrefabEditorWindow::~PrefabEditorWindow()
{
    if (TickTimer != nullptr)
    {
        TickTimer->stop();
    }
    PrefabLibrary::Get().RemoveChangeListener(LibraryListener);
    BoundEngine.UnregisterRenderSurface(RenderSurfaceId{ViewId.Value});
    CommandStack.Clear();
    if (PreviewScene != nullptr)
    {
        MemorySubsystem::Get()->DestroyObject(PreviewScene);
        PreviewScene = nullptr;
    }
}

bool PrefabEditorWindow::LoadPrefab(std::string& OutError)
{
    Scene* Loaded = MemorySubsystem::Get()->NewObject<Scene>("PrefabPreview");
    GameObject* Root = nullptr;
    if (!PrefabUtility::LoadPrefabIntoScene(Prefab, *Loaded, Root, OutError))
    {
        MemorySubsystem::Get()->DestroyObject(Loaded);
        return false;
    }
    CommandStack.Clear();
    Selected = ObjectHandle{};
    if (PreviewScene != nullptr)
    {
        BoundEngine.UnregisterRenderSurface(RenderSurfaceId{ViewId.Value});
        MemorySubsystem::Get()->DestroyObject(PreviewScene);
    }
    PreviewScene = Loaded;
    Viewport->SetPreviewScene(PreviewScene);
    bDirty = false;
    PopulateHierarchy();
    SelectObject(Root != nullptr ? Root->GetObjectHandle() : ObjectHandle{});
    UpdateTitle();
    return true;
}

// ============================================================================ UI construction

void PrefabEditorWindow::BuildToolbar()
{
    QToolBar* Toolbar = addToolBar(tr("Prefab"));
    Toolbar->setMovable(false);

    QAction* SaveAction = Toolbar->addAction(tr("Save"), this, [this]() { Save(); });
    SaveAction->setShortcut(QKeySequence::Save);
    QAction* SaveAsAction = Toolbar->addAction(tr("Save As..."), this, [this]() { SaveAs(); });
    SaveAsAction->setShortcut(QKeySequence::SaveAs);
    Toolbar->addSeparator();
    QAction* UndoAction = Toolbar->addAction(tr("Undo"), this, [this]() { Undo(); });
    UndoAction->setShortcut(QKeySequence::Undo);
    QAction* RedoAction = Toolbar->addAction(tr("Redo"), this, [this]() { Redo(); });
    RedoAction->setShortcut(QKeySequence::Redo);
    Toolbar->addSeparator();

    auto* CreateButton = new QPushButton(tr("Create"), Toolbar);
    auto* CreateMenu = new QMenu(CreateButton);
    connect(CreateMenu, &QMenu::aboutToShow, this, [this, CreateMenu]()
    {
        CreateMenu->clear();
        PopulateCreateMenus(CreateMenu->addMenu(tr("Objects")), CreateMenu->addMenu(tr("Components")));
    });
    CreateButton->setMenu(CreateMenu);
    Toolbar->addWidget(CreateButton);
    Toolbar->addSeparator();

    BreadcrumbLabel = new QLabel(Toolbar);
    BreadcrumbLabel->setStyleSheet("color:#8fb8ff; padding:0 10px; font-weight:600;");
    Toolbar->addWidget(BreadcrumbLabel);
}

void PrefabEditorWindow::BuildUi()
{
    auto* Splitter = new QSplitter(Qt::Horizontal, this);

    auto* InspectorPanel = new QWidget(Splitter);
    InspectorPanel->setObjectName("InspectorPanel");
    InspectorPanel->setMinimumWidth(280);
    auto* InspectorLayout = new QVBoxLayout(InspectorPanel);
    InspectorLayout->setContentsMargins(8, 8, 8, 8);
    InspectorLayout->setSpacing(6);

    PrefabPanel = new PrefabOverridesPanel(InspectorPanel);
    PrefabOverridesPanel::Callbacks Handlers;
    Handlers.OpenPrefab = [this](const AssetId& Target)
    {
        emit OpenPrefabRequested(QString::fromStdString(Target.ToString()));
    };
    Handlers.Revert = [this](ObjectHandle Root, const std::vector<PrefabOverride>& Selection)
    {
        return RevertInstance(Root, Selection);
    };
    Handlers.Apply = [this](ObjectHandle Root, const std::vector<PrefabOverride>& Selection)
    {
        return ApplyInstance(Root, Selection);
    };
    Handlers.Unpack = [this](ObjectHandle Root, PrefabUnpackMode Mode)
    {
        return UnpackInstance(Root, Mode);
    };
    Handlers.Select = [this](ObjectHandle Target)
    {
        SelectObject(Target);
    };
    PrefabPanel->SetCallbacks(Handlers);
    InspectorLayout->addWidget(PrefabPanel);

    NameEdit = new QLineEdit(InspectorPanel);
    NameEdit->setPlaceholderText(tr("Object name"));
    InspectorLayout->addWidget(NameEdit);
    connect(NameEdit, &QLineEdit::editingFinished, this, &PrefabEditorWindow::OnNameEdited);

    auto* TransformForm = new QFormLayout();
    auto MakeRow = [&](QDoubleSpinBox* (&Spins)[3])
    {
        auto* Row = new QWidget(InspectorPanel);
        auto* RowLayout = new QHBoxLayout(Row);
        RowLayout->setContentsMargins(0, 0, 0, 0);
        for (QDoubleSpinBox*& Spin : Spins)
        {
            Spin = new QDoubleSpinBox(Row);
            Spin->setDecimals(3);
            Spin->setRange(-1.0e6, 1.0e6);
            Spin->setSingleStep(0.1);
            RowLayout->addWidget(Spin);
            connect(Spin, &QDoubleSpinBox::editingFinished, this, &PrefabEditorWindow::OnTransformEdited);
        }
        return Row;
    };
    TransformForm->addRow(tr("Position"), MakeRow(PositionSpins));
    TransformForm->addRow(tr("Rotation"), MakeRow(RotationSpins));
    TransformForm->addRow(tr("Scale"), MakeRow(ScaleSpins));
    InspectorLayout->addLayout(TransformForm);

    AddComponentButton = new QPushButton(tr("Add Component"), InspectorPanel);
    auto* AddComponentMenu = new QMenu(AddComponentButton);
    connect(AddComponentMenu, &QMenu::aboutToShow, this, [this, AddComponentMenu]()
    {
        AddComponentMenu->clear();
        QMenu Unused;
        PopulateCreateMenus(&Unused, AddComponentMenu);
    });
    AddComponentButton->setMenu(AddComponentMenu);
    InspectorLayout->addWidget(AddComponentButton);

    auto* Scroll = new QScrollArea(InspectorPanel);
    Scroll->setWidgetResizable(true);
    Scroll->setFrameShape(QFrame::NoFrame);
    auto* ComponentContainer = new QWidget(Scroll);
    ComponentContainer->setObjectName("PrefabComponents");
    ComponentLayout = new QVBoxLayout(ComponentContainer);
    ComponentLayout->setContentsMargins(0, 0, 0, 0);
    ComponentLayout->setSpacing(6);
    ComponentLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    Scroll->setWidget(ComponentContainer);
    InspectorLayout->addWidget(Scroll, 1);

    // ---------------- Viewport (center)
    auto* ViewportPanel = new QWidget(Splitter);
    auto* ViewportLayout = new QVBoxLayout(ViewportPanel);
    ViewportLayout->setContentsMargins(0, 0, 0, 0);
    ViewportLayout->setSpacing(0);
    auto* Header = new QFrame(ViewportPanel);
    Header->setObjectName("ViewportHeader");
    auto* HeaderLayout = new QHBoxLayout(Header);
    HeaderLayout->setContentsMargins(10, 4, 10, 4);
    auto* HeaderLabel = new QLabel(tr("Prefab Preview"), Header);
    HeaderLabel->setObjectName("RoseLabel");
    HeaderLayout->addWidget(HeaderLabel);
    HeaderLayout->addStretch(1);
    auto AddGizmoButton = [&](const QString& Text, EditorViewportWidget::GizmoOperation Operation)
    {
        auto* Button = new QPushButton(Text, Header);
        Button->setObjectName("ViewportGizmoButton");
        Button->setFixedHeight(24);
        HeaderLayout->addWidget(Button);
        connect(Button, &QPushButton::clicked, this, [this, Operation]()
        {
            Viewport->SetGizmoOperation(Operation);
        });
    };
    AddGizmoButton(QStringLiteral("W"), EditorViewportWidget::GizmoOperation::Translate);
    AddGizmoButton(QStringLiteral("E"), EditorViewportWidget::GizmoOperation::Rotate);
    AddGizmoButton(QStringLiteral("R"), EditorViewportWidget::GizmoOperation::Scale);
    ViewportLayout->addWidget(Header);

    Viewport = new EditorViewportWidget(ViewportPanel);
    Viewport->SetViewId(ViewId);
    Viewport->SetEngine(&BoundEngine);
    Viewport->SetEditorToolsEnabled(true);
    Viewport->installEventFilter(this);
    ViewportLayout->addWidget(Viewport, 1);
    connect(Viewport, &EditorViewportWidget::NativeSurfaceChanged, this, [this](RenderViewportWidget* Surface)
    {
        if (!Surface->HasValidNativeHandle())
        {
            BoundEngine.UnregisterRenderSurface(RenderSurfaceId{ViewId.Value});
        }
    });
    connect(Viewport, &EditorViewportWidget::ViewportResized, this, [this](RenderViewportWidget* Surface)
    {
        const NativeWindowInfo Info = Surface->BuildNativeWindowInfo();
        BoundEngine.ResizeRenderSurface(RenderSurfaceId{ViewId.Value}, Info.Width, Info.Height);
    });
    connect(Viewport, &EditorViewportWidget::ObjectSelectionRequested, this, &PrefabEditorWindow::OnViewportSelection);
    connect(Viewport, &EditorViewportWidget::AssetDropped, this,
        [this](QString AssetIdText, QString SubAsset, QString Type, QString VirtualPath, QPoint)
        {
            OnAssetDropped(AssetIdText, SubAsset, Type, VirtualPath);
        });
    Viewport->SetTransformCallbacks(
        [this](const Transform& Value)
        {
            if (GameObject* Object = GetSelectedObject())
            {
                Object->SetTransform(Value);
            }
        },
        [this](const Transform& OldValue, const Transform& NewValue)
        {
            GameObject* Object = GetSelectedObject();
            if (Object == nullptr)
            {
                return;
            }
            Object->SetTransform(OldValue);
            Execute(MakeSetTransformCommand(PreviewScene, Object->GetObjectHandle(), NewValue));
        });

    // ---------------- Hierarchy (right)
    auto* Tree = new PrefabHierarchyTree(Splitter);
    HierarchyTree = Tree;
    HierarchyTree->setHeaderHidden(true);
    HierarchyTree->setContextMenuPolicy(Qt::CustomContextMenu);
    HierarchyTree->setDragEnabled(true);
    HierarchyTree->setAcceptDrops(true);
    HierarchyTree->setDropIndicatorShown(true);
    HierarchyTree->setDragDropMode(QAbstractItemView::DragDrop);
    HierarchyTree->setDefaultDropAction(Qt::CopyAction);
    HierarchyTree->installEventFilter(this);
    Tree->ReparentRequested = [this](ObjectHandle Target, ObjectHandle NewParent)
    {
        return ReparentObject(Target, NewParent);
    };
    Tree->AssetDropped = [this](const EditorAssetPayload& Payload, ObjectHandle Target)
    {
        if (Target.IsValid())
        {
            SelectObject(Target);
        }
        OnAssetDropped(
            QString::fromStdString(Payload.Key.Asset.ToString()),
            Payload.Key.HasSubAsset() ? QString::fromStdString(Payload.Key.SubAsset->ToString()) : QString{},
            QString::fromStdString(Payload.Type.GetIdentifier()),
            Payload.VirtualPath);
    };
    connect(HierarchyTree, &QTreeWidget::itemSelectionChanged, this, [this]()
    {
        if (bUpdatingUi)
        {
            return;
        }
        QTreeWidgetItem* Current = HierarchyTree->currentItem();
        Selected = Current != nullptr ? LoadHandle(Current) : ObjectHandle{};
        RefreshSelectionUi();
    });
    connect(HierarchyTree, &QTreeWidget::customContextMenuRequested, this, &PrefabEditorWindow::OnHierarchyContextMenu);

    auto AddLocalShortcut = [&](const QKeySequence& Sequence, std::function<void()> Handler)
    {
        auto* Action = new QAction(HierarchyTree);
        Action->setShortcut(Sequence);
        Action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        connect(Action, &QAction::triggered, this, std::move(Handler));
        HierarchyTree->addAction(Action);
    };
    AddLocalShortcut(QKeySequence::Delete, [this]() { DeleteSelected(); });
    AddLocalShortcut(QKeySequence::Copy, [this]() { CopySelected(); });
    AddLocalShortcut(QKeySequence::Paste, [this]() { PasteClipboard(); });

    Splitter->addWidget(InspectorPanel);
    Splitter->addWidget(ViewportPanel);
    Splitter->addWidget(HierarchyTree);
    Splitter->setStretchFactor(1, 1);
    Splitter->setSizes({320, 760, 260});
    setCentralWidget(Splitter);
}

// ============================================================================ hierarchy

GameObject* PrefabEditorWindow::GetPrefabRoot() const
{
    if (PreviewScene == nullptr)
    {
        return nullptr;
    }
    const std::vector<GameObject*> Roots = PreviewScene->GetRootObjects();
    return Roots.empty() ? nullptr : Roots.front();
}

GameObject* PrefabEditorWindow::GetSelectedObject() const
{
    return PreviewScene != nullptr && Selected.IsValid() ? PreviewScene->FindByHandle(Selected) : nullptr;
}

void PrefabEditorWindow::PopulateHierarchy()
{
    bUpdatingUi = true;
    HierarchyTree->clear();
    if (PreviewScene != nullptr)
    {
        for (GameObject* Root : PreviewScene->GetRootObjects())
        {
            AddHierarchyItem(nullptr, Root);
        }
    }
    HierarchyTree->expandAll();
    bUpdatingUi = false;
    if (GetSelectedObject() == nullptr)
    {
        Selected = ObjectHandle{};
    }
    SelectObject(Selected);
}

void PrefabEditorWindow::AddHierarchyItem(QTreeWidgetItem* ParentItem, GameObject* Object)
{
    const QStringList Columns = {QString::fromStdString(Object->GetName())};
    QTreeWidgetItem* Item = ParentItem != nullptr ? new QTreeWidgetItem(ParentItem, Columns) : new QTreeWidgetItem(HierarchyTree, Columns);
    StoreHandle(Item, Object->GetObjectHandle());
    Item->setFlags(Item->flags() | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    PrefabOverridesPanel::DecorateHierarchyItem(Item, Object);
    for (GameObject* Child : Object->GetChildren())
    {
        AddHierarchyItem(Item, Child);
    }
}

void PrefabEditorWindow::SelectObject(ObjectHandle Target)
{
    Selected = Target;
    bUpdatingUi = true;
    HierarchyTree->clearSelection();
    QTreeWidgetItemIterator Iterator(HierarchyTree);
    while (*Iterator != nullptr)
    {
        if (LoadHandle(*Iterator) == Target)
        {
            HierarchyTree->setCurrentItem(*Iterator);
            break;
        }
        ++Iterator;
    }
    bUpdatingUi = false;
    RefreshSelectionUi();
}

void PrefabEditorWindow::RefreshSelectionUi()
{
    bUpdatingUi = true;
    GameObject* Object = GetSelectedObject();
    const bool bHasSelection = Object != nullptr;

    OverrideMarkers.clear();
    if (GameObject* InstanceRoot = PrefabUtility::GetOutermostInstanceRoot(Object))
    {
        PrefabUtility::CollectOverrideMarkers(*PreviewScene, *InstanceRoot, OverrideMarkers);
    }
    PrefabPanel->SetTarget(PreviewScene, Object, true);

    auto Marked = [&](const char* Field)
    {
        return bHasSelection && OverrideMarkers.count(Object->GetPersistentId() + "/" + Field) != 0;
    };
    const QString OverrideStyle = QStringLiteral("border-left:3px solid #5b8def; font-weight:700;");
    NameEdit->setEnabled(bHasSelection);
    NameEdit->setText(bHasSelection ? QString::fromStdString(Object->GetName()) : QString{});
    NameEdit->setStyleSheet(Marked("name") ? OverrideStyle : QString{});

    const Transform Current = bHasSelection ? Object->GetTransform() : Transform{};
    const Vector3 Euler = QuaternionToEulerDegrees(Current.Rotation);
    const float Values[3][3] = {
        {Current.Position.x, Current.Position.y, Current.Position.z},
        {Euler.x, Euler.y, Euler.z},
        {Current.Scale.x, Current.Scale.y, Current.Scale.z}};
    QDoubleSpinBox* (*Rows[3])[3] = {&PositionSpins, &RotationSpins, &ScaleSpins};
    const char* Fields[3] = {"position", "rotation", "scale"};
    for (int Row = 0; Row < 3; ++Row)
    {
        for (int Axis = 0; Axis < 3; ++Axis)
        {
            QDoubleSpinBox* Spin = (*Rows[Row])[Axis];
            const QSignalBlocker Blocker(Spin);
            Spin->setEnabled(bHasSelection);
            Spin->setValue(Values[Row][Axis]);
            Spin->setStyleSheet(Marked(Fields[Row]) ? OverrideStyle : QString{});
        }
    }
    AddComponentButton->setEnabled(bHasSelection);

    if (bHasSelection)
    {
        Viewport->SetSelectedTransform(Object->GetTransform(), Object->GetWorldTransform(), Object->GetObjectHandle());
    }
    else
    {
        Viewport->ClearSelectedTransform();
    }
    RebuildComponentInspectors();
    bUpdatingUi = false;
}

void PrefabEditorWindow::RebuildComponentInspectors()
{
    while (QLayoutItem* Item = ComponentLayout->takeAt(0))
    {
        if (QWidget* Widget = Item->widget())
        {
            Widget->hide();
            Widget->deleteLater();
        }
        delete Item;
    }
    ComponentInspectors.clear();

    GameObject* SelectedObject = GetSelectedObject();
    if (SelectedObject != nullptr)
    {
        for (Component* ComponentInstance : SelectedObject->GetAllComponents())
        {
            const QString Type = ComponentInstance->GetClass() != nullptr
                ? QString::fromStdString(ComponentInstance->GetClass()->GetTypeId().Value)
                : QString::fromStdString(ComponentInstance->GetName());
            const bool bAdded = OverrideMarkers.count(ComponentInstance->GetPersistentId() + "/*") != 0;
            auto* Card = new QGroupBox(bAdded ? QStringLiteral("+ ") + Type : Type);
            if (bAdded)
            {
                Card->setStyleSheet("QGroupBox { color:#7fd18b; }");
            }
            auto* CardLayout = new QVBoxLayout(Card);
            CardLayout->setContentsMargins(6, 8, 6, 6);

            auto* RemoveButton = new QPushButton(tr("Remove Component"), Card);
            const ObjectHandle Owner = SelectedObject->GetObjectHandle();
            const ObjectHandle Target = ComponentInstance->GetObjectHandle();
            connect(RemoveButton, &QPushButton::clicked, this, [this, Owner, Target]()
            {
                Execute(MakeRemoveComponentCommand(PreviewScene, Owner, Target));
            });
            CardLayout->addWidget(RemoveButton);

            auto* Inspector = new ReflectionInspector(Card);
            Inspector->SetAssetRegistry(&BoundEngine.GetAssetRegistry());
            Inspector->SetTypeLabelVisible(false);
            Inspector->SetOverrideMarkerProvider([this](Object* Instance, const PropertyId& Property)
            {
                return Instance != nullptr && OverrideMarkers.count(Instance->GetPersistentId() + "/" + Property.Value) != 0;
            });
            Inspector->SetPropertyCommitCallback([this](Object* Instance, const PropertyId& Property, const ReflectedValue& Value)
            {
                const bool bOk = Instance != nullptr
                    && CommandStack.Execute(MakeSetPropertyCommand(PreviewScene, Instance->GetObjectHandle(), Property, Value));
                if (bOk)
                {
                    PrefabPanel->Refresh();
                }
                return bOk;
            });
            Inspector->SetPropertyResetCallback([this](Object* Instance, const PropertyId& Property)
            {
                return Instance != nullptr
                    && CommandStack.Execute(MakeResetPropertyCommand(PreviewScene, Instance->GetObjectHandle(), Property));
            });
            Inspector->SetAssetCommitCallback([this](Object* Instance, const PropertyId& Property, const PropertyId& Companion, const AssetKey& Key)
            {
                if (Instance == nullptr)
                {
                    return false;
                }
                CommandStack.BeginGroup("Assign Asset");
                bool bOk = CommandStack.Execute(MakeSetPropertyCommand(PreviewScene, Instance->GetObjectHandle(), Property,
                    ReflectedValue::MakeString(Key.IsValid() ? Key.Asset.ToString() : std::string{})));
                if (bOk && !Companion.Value.empty())
                {
                    bOk = CommandStack.Execute(MakeSetPropertyCommand(PreviewScene, Instance->GetObjectHandle(), Companion,
                        ReflectedValue::MakeString(Key.HasSubAsset() ? Key.SubAsset->ToString() : std::string{})));
                }
                CommandStack.EndGroup();
                if (bOk)
                {
                    RefreshSelectionUi();
                }
                return bOk;
            });
            Inspector->SetInspectedObject(ComponentInstance);
            Inspector->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
            CardLayout->addWidget(Inspector);
            Card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
            ComponentLayout->addWidget(Card);
            ComponentInspectors.push_back(Inspector);
        }
    }
    ComponentLayout->addStretch(1);
}

void PrefabEditorWindow::UpdateTitle()
{
    const QString Name = QString::fromStdString(PrefabLibrary::Get().GetPrefabDisplayName(Prefab));
    setWindowTitle(tr("Prefab — %1%2").arg(Name, bDirty ? QStringLiteral(" *") : QString{}));

    QString Breadcrumb = QStringLiteral("\xE2\x97\x86 ") + Name;
    std::string Error;
    if (std::shared_ptr<const PrefabGraph> Graph = PrefabLibrary::Get().Resolve(Prefab, Error))
    {
        for (const PrefabGraphInstance& Instance : Graph->Instances)
        {
            if (Instance.RootObjectId == Graph->RootId)
            {
                Breadcrumb += tr("  ·  variant of %1").arg(QString::fromStdString(PrefabLibrary::Get().GetPrefabDisplayName(Instance.Prefab)));
            }
        }
    }
    BreadcrumbLabel->setText(Breadcrumb);
}

// ============================================================================ editing

bool PrefabEditorWindow::Execute(std::unique_ptr<EditorCommand> Command)
{
    if (PreviewScene == nullptr || !CommandStack.Execute(std::move(Command)))
    {
        return false;
    }
    AfterSceneChanged();
    return true;
}

void PrefabEditorWindow::AfterSceneChanged()
{
    PopulateHierarchy();
    UpdateTitle();
}

bool PrefabEditorWindow::ReparentObject(ObjectHandle Target, ObjectHandle NewParent)
{
    GameObject* Object = PreviewScene != nullptr ? PreviewScene->FindByHandle(Target) : nullptr;
    if (Object == nullptr)
    {
        PopulateHierarchy();
        return false;
    }
    if (!NewParent.IsValid())
    {
        // A prefab has exactly one root: dropping to the top level means "under the root".
        GameObject* Root = GetPrefabRoot();
        if (Root == nullptr || Root == Object)
        {
            PopulateHierarchy();
            return false;
        }
        NewParent = Root->GetObjectHandle();
    }
    if (Object->GetParent() != nullptr && Object->GetParent()->GetObjectHandle() == NewParent)
    {
        PopulateHierarchy();
        return true;
    }
    if (!Execute(MakeReparentObjectCommand(PreviewScene, Target, NewParent)))
    {
        PopulateHierarchy();
        return false;
    }
    return true;
}

void PrefabEditorWindow::OnNameEdited()
{
    GameObject* Object = GetSelectedObject();
    if (bUpdatingUi || Object == nullptr || NameEdit->text().toStdString() == Object->GetName())
    {
        return;
    }
    Execute(MakeRenameObjectCommand(PreviewScene, Object->GetObjectHandle(), NameEdit->text().toStdString()));
}

void PrefabEditorWindow::OnTransformEdited()
{
    GameObject* Object = GetSelectedObject();
    if (bUpdatingUi || Object == nullptr)
    {
        return;
    }
    Transform Updated = Object->GetTransform();
    Updated.Position = Vector3(
        static_cast<float>(PositionSpins[0]->value()),
        static_cast<float>(PositionSpins[1]->value()),
        static_cast<float>(PositionSpins[2]->value()));
    Updated.Rotation = Quaternion::CreateFromYawPitchRoll(
        static_cast<float>(RotationSpins[1]->value()) * DegreesToRadians,
        static_cast<float>(RotationSpins[0]->value()) * DegreesToRadians,
        static_cast<float>(RotationSpins[2]->value()) * DegreesToRadians);
    Updated.Rotation.Normalize();
    Updated.Scale = Vector3(
        static_cast<float>(ScaleSpins[0]->value()),
        static_cast<float>(ScaleSpins[1]->value()),
        static_cast<float>(ScaleSpins[2]->value()));
    Execute(MakeSetTransformCommand(PreviewScene, Object->GetObjectHandle(), Updated));
}

void PrefabEditorWindow::Undo()
{
    if (CommandStack.Undo())
    {
        AfterSceneChanged();
    }
}

void PrefabEditorWindow::Redo()
{
    if (CommandStack.Redo())
    {
        AfterSceneChanged();
    }
}

void PrefabEditorWindow::DeleteSelected()
{
    GameObject* Object = GetSelectedObject();
    if (Object == nullptr)
    {
        return;
    }
    if (Object == GetPrefabRoot())
    {
        statusBar()->showMessage(tr("The prefab root cannot be deleted"), 2500);
        return;
    }
    if (Execute(MakeDeleteObjectCommand(PreviewScene, Object->GetObjectHandle())))
    {
        SelectObject(ObjectHandle{});
    }
}

void PrefabEditorWindow::CopySelected()
{
    GameObject* Object = GetSelectedObject();
    std::string Serialized;
    if (Object == nullptr || !SceneSerializer::SerializeSubtreeToJson(*Object, Serialized).bOk)
    {
        return;
    }
    auto* MimeData = new QMimeData();
    MimeData->setData(SakuraSceneSubtreeMimeType, QByteArray::fromStdString(Serialized));
    QApplication::clipboard()->setMimeData(MimeData);
}

void PrefabEditorWindow::PasteClipboard()
{
    const QMimeData* MimeData = QApplication::clipboard()->mimeData();
    if (PreviewScene == nullptr || MimeData == nullptr || !MimeData->hasFormat(SakuraSceneSubtreeMimeType))
    {
        return;
    }
    ObjectHandle Parent;
    GameObject* Object = GetSelectedObject();
    if (Object != nullptr && Object->GetParent() != nullptr)
    {
        Parent = Object->GetParent()->GetObjectHandle();
    }
    else if (GameObject* Root = GetPrefabRoot())
    {
        Parent = Root->GetObjectHandle();
    }
    ObjectHandle Created;
    if (Execute(MakePasteSubtreeCommand(PreviewScene, MimeData->data(SakuraSceneSubtreeMimeType).toStdString(), Parent, &Created)))
    {
        SelectObject(Created);
    }
}

void PrefabEditorWindow::PopulateCreateMenus(QMenu* ObjectsMenu, QMenu* ComponentsMenu)
{
    EditorActionContext Context = MakeActionContext();
    for (EditorAction* Action : EditorAction::CollectPublishedActions())
    {
        if (Action == nullptr)
        {
            continue;
        }
        QMenu* Target = nullptr;
        if (std::strcmp(Action->GetMenuCategory(), EditorAction::ObjectsCategory) == 0)
        {
            Target = ObjectsMenu;
        }
        else if (std::strcmp(Action->GetMenuCategory(), EditorAction::ComponentsCategory) == 0)
        {
            Target = ComponentsMenu;
        }
        if (Target == nullptr)
        {
            continue;
        }
        QAction* MenuAction = Target->addAction(QString::fromUtf8(Action->GetDisplayName()));
        MenuAction->setEnabled(Action->CanExecute(Context));
        connect(MenuAction, &QAction::triggered, this, [this, Action]()
        {
            EditorActionContext RunContext = MakeActionContext();
            if (Action->CanExecute(RunContext))
            {
                Action->Execute(RunContext);
            }
        });
    }
}

EditorActionContext PrefabEditorWindow::MakeActionContext()
{
    EditorActionContext Context;
    Context.EditScene = PreviewScene;
    Context.CommandStack = &CommandStack;
    Context.SelectedObject = Selected;
    Context.ExecuteCommand = [this](std::unique_ptr<EditorCommand> Command)
    {
        return Execute(std::move(Command));
    };
    Context.SelectObject = [this](ObjectHandle Target)
    {
        // Built-in "create object" actions create scene roots; a prefab keeps a single root.
        GameObject* Object = PreviewScene != nullptr ? PreviewScene->FindByHandle(Target) : nullptr;
        GameObject* Root = GetPrefabRoot();
        if (Object != nullptr && Object->GetParent() == nullptr && Root != nullptr && Root != Object)
        {
            GameObject* Previous = GetSelectedObject();
            GameObject* Parent = Previous != nullptr && Previous != Object ? Previous : Root;
            Execute(MakeReparentObjectCommand(PreviewScene, Target, Parent->GetObjectHandle()));
        }
        SelectObject(Target);
    };
    return Context;
}

void PrefabEditorWindow::OnHierarchyContextMenu(const QPoint& Position)
{
    QMenu Menu(this);
    PopulateCreateMenus(Menu.addMenu(tr("Objects")), Menu.addMenu(tr("Components")));
    Menu.addSeparator();
    Menu.addAction(tr("Copy"), this, [this]() { CopySelected(); });
    QAction* Paste = Menu.addAction(tr("Paste"), this, [this]() { PasteClipboard(); });
    Paste->setEnabled(QApplication::clipboard()->mimeData()->hasFormat(SakuraSceneSubtreeMimeType));
    Menu.addAction(tr("Delete"), this, [this]() { DeleteSelected(); });

    GameObject* Object = GetSelectedObject();
    if (GameObject* InstanceRoot = PrefabUtility::GetOutermostInstanceRoot(Object))
    {
        const ObjectHandle RootHandle = InstanceRoot->GetObjectHandle();
        const AssetId Nested = InstanceRoot->GetPrefabInstance()->Prefab;
        Menu.addSeparator();
        QMenu* PrefabMenu = Menu.addMenu(tr("Prefab"));
        PrefabMenu->addAction(tr("Open Prefab"), this, [this, Nested]()
        {
            emit OpenPrefabRequested(QString::fromStdString(Nested.ToString()));
        });
        PrefabMenu->addAction(tr("Revert All Overrides"), this, [this, RootHandle]() { RevertInstance(RootHandle, {}); });
        PrefabMenu->addAction(tr("Apply All to Inner Prefab"), this, [this, RootHandle]() { ApplyInstance(RootHandle, {}); });
        PrefabMenu->addAction(tr("Unpack"), this, [this, RootHandle]() { UnpackInstance(RootHandle, PrefabUnpackMode::OutermostRoot); });
        PrefabMenu->addAction(tr("Unpack Completely"), this, [this, RootHandle]() { UnpackInstance(RootHandle, PrefabUnpackMode::Completely); });
    }
    Menu.exec(HierarchyTree->viewport()->mapToGlobal(Position));
}

void PrefabEditorWindow::OnAssetDropped(QString AssetIdText, QString SubAssetIdentifier, QString AssetTypeIdentifier, QString VirtualPath)
{
    AssetKey Key{};
    AssetType Type = UnknownAssetType;
    if (PreviewScene == nullptr || !Guid::TryParse(AssetIdText.toStdString(), Key.Asset)
        || !TryParseAssetType(AssetTypeIdentifier.toStdString(), Type))
    {
        return;
    }
    if (!SubAssetIdentifier.isEmpty())
    {
        SubAssetId SubAsset{};
        if (Guid::TryParse(SubAssetIdentifier.toStdString(), SubAsset))
        {
            Key.SubAsset = SubAsset;
        }
    }
    GameObject* Parent = GetSelectedObject();
    if (Parent == nullptr)
    {
        Parent = GetPrefabRoot();
    }
    const ObjectHandle ParentHandle = Parent != nullptr ? Parent->GetObjectHandle() : ObjectHandle{};

    if (Type == PrefabAssetType)
    {
        if (PrefabLibrary::Get().DependsOn(Key.Asset, Prefab))
        {
            QMessageBox::warning(this, tr("Nested Prefab"), tr("A prefab cannot contain itself (directly or through nesting)."));
            return;
        }
        ObjectHandle Created;
        if (Execute(MakeInstantiatePrefabCommand(PreviewScene, Key.Asset, ParentHandle, nullptr, &Created)))
        {
            SelectObject(Created);
        }
        return;
    }
    if (Type == MaterialAssetType)
    {
        MeshRendererComponent* Renderer = Parent != nullptr ? Parent->GetComponent<MeshRendererComponent>() : nullptr;
        if (Renderer != nullptr)
        {
            Execute(MakeSetPropertyCommand(PreviewScene, Renderer->GetObjectHandle(), PropertyId{"material_asset_id"},
                ReflectedValue::MakeString(Key.Asset.ToString())));
        }
        return;
    }
    if (Type == ModelAssetType && !Key.HasSubAsset())
    {
        AssetRegistryEntry Entry{};
        if (BoundEngine.GetAssetRegistry().TryGetById(Key.Asset, Entry))
        {
            for (const SubAssetRecord& SubAsset : Entry.Metadata.SubAssets)
            {
                if (SubAsset.Type == StaticMeshAssetType)
                {
                    Key.SubAsset = SubAsset.Id;
                    Type = StaticMeshAssetType;
                    break;
                }
            }
        }
    }
    if (Type != StaticMeshAssetType || !Key.HasSubAsset())
    {
        statusBar()->showMessage(tr("Only prefabs, meshes and materials can be dropped here"), 2500);
        return;
    }

    ObjectHandle CreatedObject;
    const bool bOk = CommandStack.Execute(MakeCreateStaticMeshObjectCommand(PreviewScene,
        BoundEngine.GetAssetRegistry(), Key, std::filesystem::path(VirtualPath.toStdString()).stem().string(),
        ParentHandle, Transform{}, &CreatedObject));
    AfterSceneChanged();
    if (bOk)
    {
        SelectObject(CreatedObject);
    }
}

// ============================================================================ nested instance operations

bool PrefabEditorWindow::RevertInstance(ObjectHandle Root, const std::vector<PrefabOverride>& Selection)
{
    std::string Error;
    const bool bOk = Execute(MakePrefabSubtreeEditCommand(PreviewScene, Root, "Revert Prefab Overrides",
        [Selection](Scene& Target, GameObject& InstanceRoot, std::string& OutError)
        {
            return Selection.empty()
                ? PrefabUtility::RevertAll(Target, InstanceRoot, OutError)
                : PrefabUtility::RevertOverrides(Target, InstanceRoot, Selection, OutError);
        }, &Error));
    if (!bOk && !Error.empty())
    {
        QMessageBox::warning(this, tr("Revert"), QString::fromStdString(Error));
    }
    return bOk;
}

bool PrefabEditorWindow::ApplyInstance(ObjectHandle Root, const std::vector<PrefabOverride>& Selection)
{
    GameObject* InstanceRoot = PreviewScene != nullptr ? PreviewScene->FindByHandle(Root) : nullptr;
    if (InstanceRoot == nullptr)
    {
        return false;
    }
    std::string Error;
    if (!PrefabUtility::ApplyOverrides(*PreviewScene, *InstanceRoot, Selection, Error))
    {
        QMessageBox::warning(this, tr("Apply"), QString::fromStdString(Error));
        return false;
    }
    // Applying writes the inner prefab asset; that cannot be undone from this history.
    CommandStack.Clear();
    bDirty = true;
    AfterSceneChanged();
    statusBar()->showMessage(tr("Overrides applied to the inner prefab (undo history cleared)"), 4000);
    return true;
}

bool PrefabEditorWindow::UnpackInstance(ObjectHandle Root, PrefabUnpackMode Mode)
{
    std::string Error;
    const bool bOk = Execute(MakePrefabSubtreeEditCommand(PreviewScene, Root, "Unpack Prefab",
        [Mode](Scene& Target, GameObject& InstanceRoot, std::string& OutError)
        {
            return PrefabUtility::Unpack(Target, InstanceRoot, Mode, OutError);
        }, &Error));
    if (!bOk && !Error.empty())
    {
        QMessageBox::warning(this, tr("Unpack"), QString::fromStdString(Error));
    }
    return bOk;
}

// ============================================================================ save / close

bool PrefabEditorWindow::Save()
{
    if (PreviewScene == nullptr)
    {
        return false;
    }
    std::string Error;
    bSaving = true;
    const bool bOk = PrefabUtility::SavePrefabFromScene(*PreviewScene, Prefab, Error);
    bSaving = false;
    if (!bOk)
    {
        QMessageBox::warning(this, tr("Save Prefab"), QString::fromStdString(Error));
        return false;
    }
    bDirty = false;
    UpdateTitle();
    statusBar()->showMessage(tr("Prefab saved; instances updated"), 2500);
    emit PrefabSaved(QString::fromStdString(Prefab.ToString()));
    return true;
}

bool PrefabEditorWindow::SaveAs()
{
    if (PreviewScene == nullptr)
    {
        return false;
    }
    const QString ContentRoot = QString::fromStdString(BoundEngine.GetAssetRegistry().GetGameContentRoot().string());
    const QString Chosen = QFileDialog::getSaveFileName(this, tr("Save Prefab As"), ContentRoot, tr("Prefab (*.prefab)"));
    if (Chosen.isEmpty())
    {
        return false;
    }
    std::filesystem::path Path = Chosen.toStdString();
    if (Path.extension() != PrefabFileExtension)
    {
        Path += PrefabFileExtension;
    }
    AssetId NewPrefab{};
    std::string Error;
    if (!PrefabUtility::SavePrefabFromSceneAs(*PreviewScene, Path, NewPrefab, Error))
    {
        QMessageBox::warning(this, tr("Save Prefab As"), QString::fromStdString(Error));
        return false;
    }
    Prefab = NewPrefab;
    bDirty = false;
    CommandStack.Clear();
    UpdateTitle();
    emit PrefabSaved(QString::fromStdString(Prefab.ToString()));
    return true;
}

bool PrefabEditorWindow::PromptSaveIfDirty()
{
    if (!bDirty)
    {
        return true;
    }
    const auto Answer = QMessageBox::question(this, tr("Unsaved Prefab"),
        tr("Prefab '%1' has unsaved changes. Save before closing?")
            .arg(QString::fromStdString(PrefabLibrary::Get().GetPrefabDisplayName(Prefab))),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (Answer == QMessageBox::Cancel)
    {
        return false;
    }
    return Answer == QMessageBox::Discard || Save();
}

void PrefabEditorWindow::closeEvent(QCloseEvent* Event)
{
    if (!PromptSaveIfDirty())
    {
        Event->ignore();
        return;
    }
    TickTimer->stop();
    // Detach the native surface before Qt destroys the viewport window.
    BoundEngine.UnregisterRenderSurface(RenderSurfaceId{ViewId.Value});
    QMainWindow::closeEvent(Event);
}

void PrefabEditorWindow::OnPrefabChanged(const AssetId& Changed)
{
    if (bSaving || PreviewScene == nullptr)
    {
        return;
    }
    if (Changed == Prefab)
    {
        // Written from elsewhere (e.g. Apply from the main scene).
        if (!bDirty)
        {
            std::string Error;
            LoadPrefab(Error);
        }
        else
        {
            statusBar()->showMessage(tr("The prefab asset was changed outside this window"), 5000);
        }
        return;
    }
    std::string Errors;
    if (PrefabUtility::RefreshInstances(*PreviewScene, Changed, Errors) > 0)
    {
        PopulateHierarchy();
        UpdateTitle();
    }
}

// ============================================================================ viewport

void PrefabEditorWindow::OnTick()
{
    if (isVisible())
    {
        ConfigureRenderView();
    }
}

void PrefabEditorWindow::ConfigureRenderView()
{
    Viewport->SyncGizmoOverlay();
    if (!Viewport->HasValidNativeHandle() || !Viewport->isVisible() || !BoundEngine.IsPresenting())
    {
        return;
    }
    BoundEngine.GetSceneAssetResolver().ResolveScene(*PreviewScene);
    const NativeWindowInfo Info = Viewport->BuildNativeWindowInfo();
    const RenderSurfaceId Surface{ViewId.Value};
    if (!BoundEngine.RegisterRenderSurface(Surface, Info))
    {
        return;
    }
    RenderCamera Camera;
    Viewport->GetViewCamera().BuildRenderCamera(
        static_cast<float>(qMax(1u, Info.Width)) / static_cast<float>(qMax(1u, Info.Height)), Camera);
    BoundEngine.ConfigureRenderSurface(Surface, true, Camera, RenderConfiguration);
    BoundEngine.SetRenderSurfaceScene(Surface, PreviewScene);
}

void PrefabEditorWindow::OnViewportSelection(QPoint Position)
{
    if (PreviewScene == nullptr)
    {
        return;
    }
    const RenderViewCamera& Camera = Viewport->GetViewCamera();
    Vector3 Forward = Camera.Target - Camera.Position;
    if (Forward.LengthSquared() <= 0.000001f)
    {
        return;
    }
    Forward.Normalize();
    Vector3 Right = Forward.Cross(Camera.Up);
    Right.Normalize();
    Vector3 Up = Right.Cross(Forward);
    Up.Normalize();
    const float Width = static_cast<float>(std::max(1, Viewport->width()));
    const float Height = static_cast<float>(std::max(1, Viewport->height()));
    const float HalfField = std::tan(Camera.FieldOfViewDegrees * DegreesToRadians * 0.5f);
    Vector3 Direction = Forward
        + Right * ((static_cast<float>(Position.x()) / Width * 2.0f - 1.0f) * HalfField * Width / Height)
        + Up * ((1.0f - static_cast<float>(Position.y()) / Height * 2.0f) * HalfField);
    Direction.Normalize();

    GameObject* Closest = nullptr;
    float ClosestDistance = std::numeric_limits<float>::max();
    for (GameObject* Candidate : PreviewScene->GetAllObjects())
    {
        MeshRendererComponent* Renderer = Candidate->IsActiveInHierarchy() ? Candidate->GetComponent<MeshRendererComponent>() : nullptr;
        float Distance = 0.0f;
        if (Renderer != nullptr && Renderer->bVisible
            && RayHitsBounds(Camera.Position, Direction, Renderer->LocalBounds.TransformedBy(Candidate->GetWorldMatrix()), Distance)
            && Distance < ClosestDistance)
        {
            ClosestDistance = Distance;
            Closest = Candidate;
        }
    }
    SelectObject(Closest != nullptr ? Closest->GetObjectHandle() : ObjectHandle{});
}

bool PrefabEditorWindow::HandleGizmoHotkey(QEvent* Event)
{
    if (Event->type() != QEvent::ShortcutOverride && Event->type() != QEvent::KeyPress)
    {
        return false;
    }
    if (qobject_cast<QLineEdit*>(QApplication::focusWidget()) != nullptr || Viewport->IsCameraNavigationActive())
    {
        return false;
    }
    auto* KeyEvent = static_cast<QKeyEvent*>(Event);
    if (KeyEvent->modifiers() != Qt::NoModifier)
    {
        return false;
    }
    EditorViewportWidget::GizmoOperation Operation;
    switch (KeyEvent->nativeScanCode())
    {
    case 0x11: Operation = EditorViewportWidget::GizmoOperation::Translate; break;
    case 0x12: Operation = EditorViewportWidget::GizmoOperation::Rotate; break;
    case 0x13: Operation = EditorViewportWidget::GizmoOperation::Scale; break;
    default: return false;
    }
    if (Event->type() == QEvent::KeyPress)
    {
        Viewport->SetGizmoOperation(Operation);
    }
    KeyEvent->accept();
    return true;
}

bool PrefabEditorWindow::eventFilter(QObject* Watched, QEvent* Event)
{
    if ((Watched == Viewport || Watched == HierarchyTree) && HandleGizmoHotkey(Event))
    {
        return true;
    }
    return QMainWindow::eventFilter(Watched, Event);
}
