#include "Automation/EditorToolset.h"
#include "Prefab/PrefabOverridesPanel.h"
#include "Shell/EditorMainWindow.h"
#include "UI/Styles/StudioIcons.h"
#include "UI/Views/StoryWorkspaceView.h"
#include "UI/Views/HierarchyPanelView.h"
#include "UI/Styles/HierarchyStyle.h"
#include "UI/Views/ObjectInspectorView.h"
#include "UI/Views/StudioControls.h"
#include <QToolButton>
#include <QStackedWidget>
#include <QScopedValueRollback>
#include "UI/Adapters/InspectorUpdateGuard.h"

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

namespace
{
void RefreshPrimaryCameraBadge(QTreeWidget& Tree, Scene* World)
{
    const auto* Camera = World ? World->FindPrimaryCamera() : nullptr;
    const auto* Owner = Camera ? Camera->GetGameObject() : nullptr;
    const auto Primary = Owner ? Owner->GetObjectHandle() : ObjectHandle{};
    QTreeWidgetItemIterator Iterator(&Tree);
    while (*Iterator)
    {
        (*Iterator)->setData(0, EditorUi::HierarchyPrimaryCameraRole,
                            Primary.IsValid() && LoadObjectHandle(*Iterator) == Primary);
        ++Iterator;
    }
}
}

void EditorMainWindow::PopulateHierarchy()
{
    HierarchyTree->blockSignals(true);
    HierarchyTree->clear();

    Scene* HierarchyScene = GetHierarchyScene();
    if (HierarchyScene != nullptr)
    {
        for (GameObject* RootObject : HierarchyScene->GetRootObjects())
        {
            AddHierarchyItem(nullptr, RootObject);
        }
    }

    RefreshPrimaryCameraBadge(*HierarchyTree, HierarchyScene);
    HierarchyContent->Refresh();
    RefreshStoryTargets();
    HierarchyTree->expandAll();
    HierarchyTree->blockSignals(false);

    if (Workspace.GetSelectedIdentity().IsValid() && GetSelectedGameObject() != nullptr)
    {
        SelectObject(Workspace.GetSelectedIdentity());
    }
    else
    {
        ClearSelection();
    }
}

bool EditorMainWindow::ReparentHierarchyObject(ObjectHandle Target, ObjectHandle NewParent)
{
    Scene* EditScene = GetEditScene();
    if (EditScene == nullptr || BoundEngine.GetPlaySession().IsSimulating() || !Target.IsValid())
    {
        PopulateHierarchy();
        return false;
    }

    GameObject* ObjectInstance = EditScene->FindByHandle(Target);
    if (ObjectInstance == nullptr)
    {
        PopulateHierarchy();
        return false;
    }

    ObjectHandle CurrentParent;
    if (ObjectInstance->GetParent() != nullptr)
    {
        CurrentParent = ObjectInstance->GetParent()->GetObjectHandle();
    }
    if (CurrentParent == NewParent)
    {
        PopulateHierarchy();
        return true;
    }

    if (!ExecuteCommand(MakeReparentObjectCommand(EditScene, Target, NewParent)))
    {
        PopulateHierarchy();
        return false;
    }
    return true;
}

void EditorMainWindow::AddHierarchyItem(QTreeWidgetItem* ParentItem, GameObject* ObjectInstance)
{
    if (ObjectInstance == nullptr)
    {
        return;
    }

    QTreeWidgetItem* Item = nullptr;
    if (ParentItem != nullptr)
    {
        Item = new QTreeWidgetItem(ParentItem, {QString::fromStdString(ObjectInstance->GetName())});
    }
    else
    {
        Item = new QTreeWidgetItem(HierarchyTree, {QString::fromStdString(ObjectInstance->GetName())});
    }
    EditorUi::StudioIcon Icon = EditorUi::StudioIcon::Cube;
    if (ObjectInstance->GetComponent<CameraComponent>() != nullptr)
    {
        Icon = EditorUi::StudioIcon::Camera;
        Item->setData(0, EditorUi::HierarchyCameraRole, true);
    }
    else if (ObjectInstance->GetComponent<LightComponent>() != nullptr)
    {
        Icon = EditorUi::StudioIcon::Light;
    }
    Item->setIcon(0, EditorUi::MakeStudioIcon(Icon));
    StoreObjectHandle(Item, ObjectInstance->GetObjectHandle());
    if (!BoundEngine.GetPlaySession().IsSimulating())
    {
        Item->setFlags(Item->flags() | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    }

    PrefabOverridesPanel::DecorateHierarchyItem(Item, ObjectInstance);
    for (GameObject* Child : ObjectInstance->GetChildren())
    {
        AddHierarchyItem(Item, Child);
    }
}

void EditorMainWindow::ClearSelection()
{
    Workspace.ClearSelection();
    ComponentFilter.clear();
    RefreshSelectionUi();
}

void EditorMainWindow::SelectObject(ObjectHandle Target)
{
    Workspace.Select(GetHierarchyScene(), Target);
    QTreeWidgetItemIterator Iterator(HierarchyTree);
    while (*Iterator != nullptr)
    {
        if (LoadObjectHandle(*Iterator) == Target)
        {
            HierarchyTree->setCurrentItem(*Iterator);
            break;
        }
        ++Iterator;
    }
    RefreshSelectionUi();
    UpdateStatus();
}

ObjectHandle EditorMainWindow::GetSelectedObjectHandle() const
{
    return Workspace.GetSelectedIdentity();
}

GameObject* EditorMainWindow::GetSelectedGameObject() const
{
    return Workspace.GetSelectedObject(GetHierarchyScene());
}

void EditorMainWindow::ConfigureComponentInspector(ReflectionInspector* ComponentInspector)
{
    ComponentInspector->SetAssetRegistry(&BoundEngine.GetAssetRegistry());
    connect(ComponentInspector, &ReflectionInspector::AssetFocusRequested, this,
        [this](const QString& AssetIdentifier, const QString& SubAssetIdentifier)
    {
        Toolset->Call("asset_focus", {{"assetId", AssetIdentifier}, {"subAssetId", SubAssetIdentifier}});
    });
    ComponentInspector->SetCommandExecutor([this](std::unique_ptr<EditorCommand> Command)
    {
        if (bPlayRequested)
        {
            return false;
        }
        return ExecuteCommand(std::move(Command));
    });
    ComponentInspector->SetMeshOpenCallback([this](const AssetKey& Key) { OpenStaticMeshEditor(Key); });
    ComponentInspector->SetTypeLabelVisible(false);
    ComponentInspector->SetPropertyCommitCallback([this](
        Object* Instance,
        const PropertyId& Property,
        const ReflectedValue& NewValue)
    {
        if (Instance == nullptr || GetEditScene() == nullptr || BoundEngine.GetPlaySession().IsSimulating())
        {
            return false;
        }
        if (!CommandStack.Execute(MakeSetPropertyCommand(
            GetEditScene(),
            Instance->GetObjectHandle(),
            Property,
            NewValue)))
        {
            return false;
        }
        UpdateWindowTitleDirty();
        UpdateStatus();
        return true;
    });
    ComponentInspector->SetPropertyResetCallback([this](Object* Instance, const PropertyId& Property)
    {
        if (Instance == nullptr || GetEditScene() == nullptr || BoundEngine.GetPlaySession().IsSimulating())
        {
            return false;
        }
        if (!CommandStack.Execute(MakeResetPropertyCommand(
            GetEditScene(),
            Instance->GetObjectHandle(),
            Property)))
        {
            return false;
        }
        UpdateWindowTitleDirty();
        UpdateStatus();
        return true;
    });
    ComponentInspector->SetAssetCommitCallback([this](
        Object* Instance,
        const PropertyId& Property,
        const PropertyId& CompanionProperty,
        const AssetKey& Key)
    {
        if (Instance == nullptr || GetEditScene() == nullptr)
        {
            return false;
        }
        CommandStack.BeginGroup("Assign Asset");
        bool bResult = CommandStack.Execute(MakeSetPropertyCommand(
            GetEditScene(),
            Instance->GetObjectHandle(),
            Property,
            ReflectedValue::MakeString(Key.IsValid() ? Key.Asset.ToString() : std::string{})));
        if (bResult && !CompanionProperty.Value.empty())
        {
            bResult = CommandStack.Execute(MakeSetPropertyCommand(
                GetEditScene(),
                Instance->GetObjectHandle(),
                CompanionProperty,
                ReflectedValue::MakeString(Key.HasSubAsset() ? Key.SubAsset->ToString() : std::string{})));
        }
        CommandStack.EndGroup();
        if (bResult)
        {
            UpdateWindowTitleDirty();
            UpdateStatus();
        }
        return bResult;
    });
    connect(ComponentInspector, &ReflectionInspector::PropertyChanged, this, [this]()
    {
        RefreshPrimaryCameraBadge(*HierarchyTree, GetHierarchyScene());
        UpdateWindowTitleDirty();
    });
}

void EditorMainWindow::RebuildComponentInspectors()
{
    if (ComponentCombo == nullptr || ComponentInspectorLayout == nullptr)
    {
        return;
    }

    auto* ObjectView = static_cast<EditorUi::ObjectInspectorView*>(ObjectInspectorContent);
    GameObject* Selected = GetSelectedGameObject();
    const ObjectHandle Selection = Selected ? Selected->GetObjectHandle() : ObjectHandle{};
    const bool SameSelection = InspectorSelection == Selection;
    if (!SameSelection) { ComponentExpansion.clear(); }
    auto Previous = std::move(ComponentInspectors);
    ComponentInspectors.clear();
    std::vector<Component*> Components;
    if (Selected)
    {
        for (auto* Component : Selected->GetAllComponents())
        {
            if (Component) { Components.push_back(Component); }
        }
    }
    for (auto* Inspector : Previous)
    {
        auto* Card = Inspector->parentWidget();
        const auto Identity = Card->property("componentIdentity").toString();
        if (SameSelection)
        {
            if (auto* Toggle = Card->findChild<QToolButton*>()) { ComponentExpansion[Identity] = Toggle->isChecked(); }
        }
        const auto* Instance = Inspector->GetInspectedObject();
        const bool Retain = SameSelection && Instance
            && std::find(Components.begin(), Components.end(), Instance) != Components.end();
        if (!Retain)
        {
            Inspector->SetInspectedObject(nullptr);
            ObjectView->RemoveComponentSection(Identity);
        }
    }
    QStringList ComponentTypes;
    for (auto* ComponentInstance : Components)
    {
        const QString ComponentType = ComponentInstance->GetClass()
            ? QString::fromStdString(ComponentInstance->GetClass()->GetTypeId().Value)
            : QString::fromStdString(ComponentInstance->GetName());
        if (!ComponentTypes.contains(ComponentType)) { ComponentTypes.push_back(ComponentType); }
        ReflectionInspector* Inspector = nullptr;
        for (auto* Existing : Previous)
        {
            if (SameSelection && Existing->GetInspectedObject() == ComponentInstance) { Inspector = Existing; break; }
        }
        const auto Identity = QString::fromStdString(ComponentInstance->GetPersistentId());
        if (!Inspector)
        {
            Inspector = new ReflectionInspector();
            ConfigureComponentInspector(Inspector);
            Inspector->SetMeshPresentation(ComponentType == "engine.MeshRendererComponent");
            Inspector->SetInspectedObject(ComponentInstance);
            ObjectView->AddComponentSection(Identity, ComponentType, *Inspector,
                ComponentExpansion.value(Identity, ComponentType == "engine.MeshRendererComponent"));
        }
        else
        {
            Inspector->RefreshValues();
        }
        Inspector->setEnabled(!BoundEngine.GetPlaySession().IsSimulating());
        ObjectView->MoveComponentSection(Identity, static_cast<int>(ComponentInspectors.size()));
        ComponentInspectors.push_back(Inspector);
    }
    InspectorSelection = Selection;
    // Keep one trailing stretch; moving a card never destroys its layout or controls.
    for (int Index = ComponentInspectorLayout->count() - 1; Index >= 0; --Index)
    {
        if (ComponentInspectorLayout->itemAt(Index)->spacerItem()) { delete ComponentInspectorLayout->takeAt(Index); }
    }
    ComponentInspectorLayout->addStretch(1);

    QSignalBlocker ComboSignalBlocker(ComponentCombo);
    ComponentCombo->clear();
    ComponentCombo->addItem(tr("Все компоненты"), QString{});
    for (const QString& ComponentType : ComponentTypes)
    {
        ComponentCombo->addItem(EditorUi::ObjectInspectorView::ComponentCaption(ComponentType), ComponentType);
    }
    int FilterIndex = ComponentCombo->findData(ComponentFilter);
    if (FilterIndex < 0)
    {
        ComponentFilter.clear();
        FilterIndex = 0;
    }
    ComponentCombo->setCurrentIndex(FilterIndex);
    ComponentCombo->setEnabled(Selected != nullptr && !ComponentTypes.isEmpty());
    // The frontend uses a searchable component overview instead of a second filter select.
    ComponentCombo->hide();
    OnComponentFilterChanged(FilterIndex);
}

void EditorMainWindow::RefreshSelectionUi()
{
    const EditorUi::InspectorUpdateGuard InspectorUpdate(ObjectInspectorContent->findChild<QWidget*>("ObjectInspectorBody"));
    if (ScrubbingTransformField && (GetSelectedObjectHandle() != ScrubbingTransformObject
        || BoundEngine.GetPlaySession().IsSimulating()))
    {
        ScrubbingTransformField->FinishScrub(true);
    }
    bUpdatingSelectionUi = true;

    GameObject* Selected = GetSelectedGameObject();
    const bool bHasSelection = Selected != nullptr;
    if (InspectorPages != nullptr)
    {
        InspectorPages->setCurrentWidget(bHasSelection ? ObjectInspectorContent : EmptyInspectorPage);
        if (bHasSelection && StoryGraph != nullptr)
        {
            StoryGraph->ClearSelection();
        }
    }
    const bool bEditableSelection = bHasSelection && !BoundEngine.GetPlaySession().IsSimulating();
    PrefabPanel->SetTarget(GetHierarchyScene(), Selected, bEditableSelection);
    static_cast<EditorUi::ObjectInspectorView*>(ObjectInspectorContent)->SetEditable(bEditableSelection);
    auto* ObjectView = static_cast<EditorUi::ObjectInspectorView*>(ObjectInspectorContent);
    const QString Kind = !Selected ? QString{} : Selected->GetComponent<CameraComponent>() ? tr("Камера") :
        Selected->GetComponent<LightComponent>() ? tr("Источник света") :
        Selected->GetComponent<MeshRendererComponent>() ? tr("Активный меш") : tr("Объект сцены");
    ObjectView->SetIdentity(Selected ? QString::fromStdString(Selected->GetName()) : QString{}, Kind,
                            Selected && Selected->IsActive());
    const auto PreviousType = ObjectView->AddComponentType->currentData();
    ObjectView->AddComponentType->clear();
    auto ActionContext = MakeEditorActionContext();
    for (auto* Action : EditorAction::CollectPublishedActions())
    {
        if (Action && QString::fromUtf8(Action->GetMenuCategory()) == EditorAction::ComponentsCategory &&
            Action->CanExecute(ActionContext))
        {
            ObjectView->AddComponentType->addItem(QString::fromUtf8(Action->GetDisplayName()),
                QString::fromStdString(Action->GetClass()->GetTypeId().Value));
        }
    }
    for (Class* ComponentClass : ReflectionSubsystem::Get().GetPublishedCatalog().Classes)
    {
        if (ComponentClass->GetOrigin() == ReflectionOrigin::Python && ComponentClass->IsConcrete()
            && ComponentClass->IsA(TypeId{"engine.ScriptComponent"}))
        {
            ObjectView->AddComponentType->addItem(QString::fromStdString(ComponentClass->GetTypeId().Value),
                QString::fromStdString(ComponentClass->GetTypeId().Value));
        }
    }
    const int PreviousIndex = ObjectView->AddComponentType->findData(PreviousType);
    if (PreviousIndex >= 0) { ObjectView->AddComponentType->setCurrentIndex(PreviousIndex); }
    ObjectView->AddComponent->setEnabled(bEditableSelection && ObjectView->AddComponentType->count() > 0);
    ObjectView->ParentObject->clear();
    ObjectView->AnchorObject->clear();
    ObjectView->ParentObject->addItem(tr("Объекты сцены · корень"), QString{});
    ObjectView->AnchorObject->addItem(tr("Переместить к точке…"), QString{});
    if (Selected && Selected->GetScene())
    {
        for (auto* Candidate : Selected->GetScene()->GetAllObjects())
        {
            if (!Candidate || Candidate == Selected) { continue; }
            bool Descendant = false;
            for (auto* Parent = Candidate->GetParent(); Parent; Parent = Parent->GetParent())
            { if (Parent == Selected) { Descendant = true; break; } }
            if (Descendant) { continue; }
            const auto Label = QString::fromStdString(Candidate->GetName());
            const auto Identity = QString::fromStdString(Candidate->GetPersistentId());
            ObjectView->ParentObject->addItem(Label, Identity);
            ObjectView->AnchorObject->addItem(Label, Identity);
        }
        if (Selected->GetParent())
        { ObjectView->ParentObject->setCurrentIndex(ObjectView->ParentObject->findData(QString::fromStdString(Selected->GetParent()->GetPersistentId()))); }
    }
    if (ObjectNameEdit != nullptr)
    {
        ObjectNameEdit->setEnabled(bEditableSelection);
        ObjectNameEdit->setText(bHasSelection ? QString::fromStdString(Selected->GetName()) : QString());
    }

    RefreshTransformUi();
    RebuildComponentInspectors();

    bUpdatingSelectionUi = false;
}

void EditorMainWindow::RefreshTransformUi()
{
    // Transform edits do not change component cards or hierarchy. Keep their widgets,
    // disclosure state and scroll geometry intact when a drag is committed/cancelled.
    const QScopedValueRollback<bool> UpdatingSelection(bUpdatingSelectionUi, true);
    GameObject* Selected = GetSelectedGameObject();
    const bool bHasSelection = Selected != nullptr;
    const bool bEditableSelection = bHasSelection && !BoundEngine.GetPlaySession().IsSimulating();
    PrefabPanel->SetTarget(GetHierarchyScene(), Selected, bEditableSelection);
    auto SetSpinEnabled = [&](QDoubleSpinBox* Spin, double Value)
    {
        if (Spin == nullptr)
        {
            return;
        }
        const QSignalBlocker Blocker(Spin);
        Spin->setEnabled(bEditableSelection);
        Spin->setValue(Value);
    };

    if (bHasSelection)
    {
        const Transform& ObjectTransform = Selected->GetTransform();
        if (PrimaryViewport != nullptr)
        {
            PrimaryViewport->SetSelectedTransform(ObjectTransform, Selected->GetWorldTransform(), Selected->GetObjectHandle());
        }
        SetSpinEnabled(PositionXSpin, ObjectTransform.Position.x);
        SetSpinEnabled(PositionYSpin, ObjectTransform.Position.y);
        SetSpinEnabled(PositionZSpin, ObjectTransform.Position.z);
        const Vector3 RotationDegrees = QuaternionToEulerDegrees(ObjectTransform.Rotation);
        SetSpinEnabled(RotationXSpin, RotationDegrees.x);
        SetSpinEnabled(RotationYSpin, RotationDegrees.y);
        SetSpinEnabled(RotationZSpin, RotationDegrees.z);
        SetSpinEnabled(ScaleXSpin, ObjectTransform.Scale.x);
        SetSpinEnabled(ScaleYSpin, ObjectTransform.Scale.y);
        SetSpinEnabled(ScaleZSpin, ObjectTransform.Scale.z);
    }
    else
    {
        if (PrimaryViewport != nullptr)
        {
            PrimaryViewport->ClearSelectedTransform();
        }
        SetSpinEnabled(PositionXSpin, 0.0);
        SetSpinEnabled(PositionYSpin, 0.0);
        SetSpinEnabled(PositionZSpin, 0.0);
        SetSpinEnabled(RotationXSpin, 0.0);
        SetSpinEnabled(RotationYSpin, 0.0);
        SetSpinEnabled(RotationZSpin, 0.0);
        SetSpinEnabled(ScaleXSpin, 1.0);
        SetSpinEnabled(ScaleYSpin, 1.0);
        SetSpinEnabled(ScaleZSpin, 1.0);
    }

}

void EditorMainWindow::OnHierarchySelectionChanged()
{
    bStoryEditingContext = false;
    bWidgetEditingContext = false;
    QTreeWidgetItem* Current = HierarchyTree->currentItem();
    if (Current == nullptr)
    {
        ClearSelection();
        UpdateStatus();
        return;
    }

    Workspace.Select(GetHierarchyScene(), LoadObjectHandle(Current));
    ComponentFilter.clear();
    if (GetSelectedGameObject() == nullptr)
    {
        ClearSelection();
    }
    else
    {
        RefreshSelectionUi();
    }
    UpdateStatus();
}

void EditorMainWindow::OnHierarchyContextMenu(const QPoint& Position)
{
    if (BoundEngine.GetPlaySession().IsSimulating())
    {
        return;
    }
    QMenu Menu(this);
    Menu.addAction(QString::fromUtf8("Copy"), this, &EditorMainWindow::OnCopy);
    QAction* PasteAction = Menu.addAction(QString::fromUtf8("Paste"), this, &EditorMainWindow::OnPaste);
    PasteAction->setEnabled(
        !BoundEngine.GetPlaySession().IsSimulating()
        && QApplication::clipboard()->mimeData()->hasFormat(SakuraSceneSubtreeMimeType));
    Menu.addSeparator();
    QMenu* CreateObjectsMenu = Menu.addMenu(QString::fromUtf8("\xD0\x9E\xD0\xB1\xD1\x8A\xD0\xB5\xD0\xBA\xD1\x82\xD1\x8B"));
    QMenu* CreateComponentsMenu = Menu.addMenu(QString::fromUtf8("\xD0\x9A\xD0\xBE\xD0\xBC\xD0\xBF\xD0\xBE\xD0\xBD\xD0\xB5\xD0\xBD\xD1\x82\xD1\x8B"));
    PopulateCreateMenus(CreateObjectsMenu, CreateComponentsMenu);
    Menu.addSeparator();
    Menu.addAction(tr("Создать префаб из выбранного объекта"), this, &EditorMainWindow::CreatePrefabFromSelection);
    Menu.addAction(QString::fromUtf8("Delete Object"), this, &EditorMainWindow::OnDeleteSelectedObject);
    Menu.exec(HierarchyTree->viewport()->mapToGlobal(Position));
}

void EditorMainWindow::OnObjectNameEdited()
{
    if (bUpdatingSelectionUi)
    {
        return;
    }

    Scene* EditScene = GetEditScene();
    GameObject* Selected = GetSelectedGameObject();
    if (EditScene == nullptr || Selected == nullptr || ObjectNameEdit == nullptr)
    {
        return;
    }

    const std::string NewName = ObjectNameEdit->text().toStdString();
    if (NewName == Selected->GetName())
    {
        return;
    }
    ExecuteCommand(MakeRenameObjectCommand(EditScene, Selected->GetObjectHandle(), NewName));
}

void EditorMainWindow::BindTransformScrubbing(EditorUi::StudioNumberEdit* Field, int Index)
{
    connect(Field, &EditorUi::StudioNumberEdit::ScrubStarted, this, [this, Field]()
    {
        if (GetSelectedGameObject() == nullptr || BoundEngine.GetPlaySession().IsSimulating())
        {
            return;
        }
        ScrubbingTransformField = Field;
        ScrubbingTransformObject = GetSelectedObjectHandle();
        CommandStack.BeginGroup("Transform Object");
    });
    connect(Field, &QDoubleSpinBox::valueChanged, this, [this, Field, Index](double Value)
    {
        if (bUpdatingSelectionUi || ScrubbingTransformField != Field || !Field->IsScrubbing())
        {
            return;
        }
        Scene* EditScene = GetEditScene();
        GameObject* Selected = EditScene ? EditScene->FindByHandle(ScrubbingTransformObject) : nullptr;
        if (!Selected || GetSelectedObjectHandle() != ScrubbingTransformObject || BoundEngine.GetPlaySession().IsSimulating())
        {
            Field->FinishScrub(true);
            return;
        }
        Transform Updated = Selected->GetTransform();
        const auto SetAxis = [Index, Value](Vector3& Vector)
        {
            if (Index % 3 == 0) { Vector.x = static_cast<float>(Value); }
            else if (Index % 3 == 1) { Vector.y = static_cast<float>(Value); }
            else { Vector.z = static_cast<float>(Value); }
        };
        if (Index < 3)
        {
            SetAxis(Updated.Position);
        }
        else if (Index < 6)
        {
            // Use the authored fields so dragging past 90/180 degrees stays continuous.
            Vector3 Degrees(static_cast<float>(RotationXSpin->value()), static_cast<float>(RotationYSpin->value()),
                static_cast<float>(RotationZSpin->value()));
            Updated.Rotation = Quaternion::CreateFromYawPitchRoll(
                Degrees.y * DegreesToRadians, Degrees.x * DegreesToRadians, Degrees.z * DegreesToRadians);
            Updated.Rotation.Normalize();
        }
        else
        {
            SetAxis(Updated.Scale);
        }
        if (!CommandStack.Execute(MakeSetTransformCommand(EditScene, ScrubbingTransformObject, Updated)))
        {
            Field->FinishScrub(true);
            return;
        }
        PrimaryViewport->SetSelectedTransform(Updated, Selected->GetWorldTransform(), Selected->GetObjectHandle());
        UpdateWindowTitleDirty();
    });
    connect(Field, &EditorUi::StudioNumberEdit::ScrubFinished, this, [this, Field](bool Cancelled)
    {
        if (ScrubbingTransformField != Field)
        {
            return;
        }
        ScrubbingTransformField = nullptr;
        ScrubbingTransformObject = {};
        if (Cancelled) { CommandStack.CancelGroup(); }
        else { CommandStack.EndGroup(); }
        RefreshTransformUi();
        UpdateWindowTitleDirty();
        UpdateStatus();
    });
}

bool EditorMainWindow::ExecuteSelectedTransform(const Transform& Updated)
{
    Scene* EditScene = GetEditScene();
    GameObject* Selected = GetSelectedGameObject();
    if (!EditScene || !Selected || BoundEngine.GetPlaySession().IsSimulating())
    {
        return false;
    }
    if (!CommandStack.Execute(MakeSetTransformCommand(EditScene, Selected->GetObjectHandle(), Updated)))
    {
        return false;
    }
    RefreshTransformUi();
    UpdateWindowTitleDirty();
    UpdateStatus();
    return true;
}

void EditorMainWindow::OnTransformEdited()
{
    if (bUpdatingSelectionUi || ScrubbingTransformField || BoundEngine.GetPlaySession().IsSimulating())
    {
        return;
    }

    Scene* EditScene = GetEditScene();
    GameObject* Selected = GetSelectedGameObject();
    if (EditScene == nullptr || Selected == nullptr)
    {
        return;
    }

    // Focus loss after a scrub must not add another command or round untouched axes.
    const Transform& Current = Selected->GetTransform();
    const Vector3 CurrentDegrees = QuaternionToEulerDegrees(Current.Rotation);
    const double Values[]{Current.Position.x, Current.Position.y, Current.Position.z,
        CurrentDegrees.x, CurrentDegrees.y, CurrentDegrees.z, Current.Scale.x, Current.Scale.y, Current.Scale.z};
    const QDoubleSpinBox* Fields[]{PositionXSpin, PositionYSpin, PositionZSpin, RotationXSpin,
        RotationYSpin, RotationZSpin, ScaleXSpin, ScaleYSpin, ScaleZSpin};
    bool Changed = false;
    for (int Index = 0; Index < 9; ++Index)
    {
        Changed = Changed || std::abs(Fields[Index]->value() - Values[Index]) >= 0.00051;
    }
    if (!Changed)
    {
        return;
    }
    Transform Updated = Selected->GetTransform();
    Updated.Position = Vector3(
        static_cast<float>(PositionXSpin->value()),
        static_cast<float>(PositionYSpin->value()),
        static_cast<float>(PositionZSpin->value()));
    Updated.Rotation = Quaternion::CreateFromYawPitchRoll(
        static_cast<float>(RotationYSpin->value()) * DegreesToRadians,
        static_cast<float>(RotationXSpin->value()) * DegreesToRadians,
        static_cast<float>(RotationZSpin->value()) * DegreesToRadians);
    Updated.Rotation.Normalize();
    Updated.Scale = Vector3(
        static_cast<float>(ScaleXSpin->value()),
        static_cast<float>(ScaleYSpin->value()),
        static_cast<float>(ScaleZSpin->value()));
    ExecuteSelectedTransform(Updated);
}

void EditorMainWindow::OnComponentFilterChanged(int Index)
{
    if (ComponentCombo == nullptr || Index < 0 || Index >= ComponentCombo->count())
    {
        return;
    }

    ComponentFilter = ComponentCombo->itemData(Index).toString();
    static_cast<EditorUi::ObjectInspectorView*>(ObjectInspectorContent)->ApplyComponentFilter();
}

