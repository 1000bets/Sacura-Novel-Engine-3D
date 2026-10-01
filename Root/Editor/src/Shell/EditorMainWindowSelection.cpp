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
    StoreObjectHandle(Item, ObjectInstance->GetObjectHandle());
    if (!BoundEngine.GetPlaySession().IsSimulating())
    {
        Item->setFlags(Item->flags() | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    }

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
            RefreshSelectionUi();
            UpdateWindowTitleDirty();
            UpdateStatus();
        }
        return bResult;
    });
    connect(ComponentInspector, &ReflectionInspector::PropertyChanged, this, [this]()
    {
        UpdateWindowTitleDirty();
    });
}

void EditorMainWindow::RebuildComponentInspectors()
{
    if (ComponentCombo == nullptr || ComponentInspectorLayout == nullptr)
    {
        return;
    }

    while (QLayoutItem* Item = ComponentInspectorLayout->takeAt(0))
    {
        if (QWidget* ItemWidget = Item->widget())
        {
            ItemWidget->deleteLater();
        }
        delete Item;
    }
    ComponentInspectors.clear();

    GameObject* Selected = GetSelectedGameObject();
    QStringList ComponentTypes;
    if (Selected != nullptr)
    {
        for (Component* ComponentInstance : Selected->GetAllComponents())
        {
            if (ComponentInstance == nullptr)
            {
                continue;
            }
            const QString ComponentType = ComponentInstance->GetClass() != nullptr
                ? QString::fromStdString(ComponentInstance->GetClass()->GetTypeId().Value)
                : QString::fromStdString(ComponentInstance->GetName());
            if (!ComponentTypes.contains(ComponentType))
            {
                ComponentTypes.push_back(ComponentType);
            }

            auto* ComponentCard = new QGroupBox(ComponentType);
            ComponentCard->setObjectName("ComponentInspectorCard");
            ComponentCard->setProperty("componentType", ComponentType);
            auto* ComponentCardLayout = new QVBoxLayout(ComponentCard);
            ComponentCardLayout->setContentsMargins(6, 8, 6, 6);
            auto* ComponentInspector = new ReflectionInspector(ComponentCard);
            ConfigureComponentInspector(ComponentInspector);
            ComponentInspector->SetInspectedObject(ComponentInstance);
            ComponentInspector->setEnabled(!BoundEngine.GetPlaySession().IsSimulating());
            ComponentCardLayout->addWidget(ComponentInspector);
            ComponentInspectorLayout->addWidget(ComponentCard);
            ComponentInspectors.push_back(ComponentInspector);
        }
    }
    ComponentInspectorLayout->addStretch(1);

    QSignalBlocker ComboSignalBlocker(ComponentCombo);
    ComponentCombo->clear();
    ComponentCombo->addItem(QStringLiteral("All"), QString{});
    for (const QString& ComponentType : ComponentTypes)
    {
        ComponentCombo->addItem(ComponentType, ComponentType);
    }
    int FilterIndex = ComponentCombo->findData(ComponentFilter);
    if (FilterIndex < 0)
    {
        ComponentFilter.clear();
        FilterIndex = 0;
    }
    ComponentCombo->setCurrentIndex(FilterIndex);
    ComponentCombo->setEnabled(Selected != nullptr && !ComponentTypes.isEmpty());
    OnComponentFilterChanged(FilterIndex);
}

void EditorMainWindow::RefreshSelectionUi()
{
    bUpdatingSelectionUi = true;

    GameObject* Selected = GetSelectedGameObject();
    const bool bHasSelection = Selected != nullptr;
    const bool bEditableSelection = bHasSelection && !BoundEngine.GetPlaySession().IsSimulating();
    if (ObjectNameEdit != nullptr)
    {
        ObjectNameEdit->setEnabled(bEditableSelection);
        ObjectNameEdit->setText(bHasSelection ? QString::fromStdString(Selected->GetName()) : QString());
    }

    auto SetSpinEnabled = [&](QDoubleSpinBox* Spin, double Value)
    {
        if (Spin == nullptr)
        {
            return;
        }
        Spin->setEnabled(bEditableSelection);
        Spin->setValue(Value);
    };

    if (bHasSelection)
    {
        const Transform& ObjectTransform = Selected->GetTransform();
        if (PrimaryViewport != nullptr)
        {
            PrimaryViewport->SetSelectedTransform(ObjectTransform, Selected->GetWorldTransform());
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

    RebuildComponentInspectors();

    bUpdatingSelectionUi = false;
}

void EditorMainWindow::OnHierarchySelectionChanged()
{
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

void EditorMainWindow::OnTransformEdited()
{
    if (bUpdatingSelectionUi)
    {
        return;
    }

    Scene* EditScene = GetEditScene();
    GameObject* Selected = GetSelectedGameObject();
    if (EditScene == nullptr || Selected == nullptr)
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
    ExecuteCommand(MakeSetTransformCommand(EditScene, Selected->GetObjectHandle(), Updated));
}

void EditorMainWindow::OnComponentFilterChanged(int Index)
{
    if (ComponentCombo == nullptr || Index < 0 || Index >= ComponentCombo->count())
    {
        return;
    }

    ComponentFilter = ComponentCombo->itemData(Index).toString();
    for (ReflectionInspector* ComponentInspector : ComponentInspectors)
    {
        if (ComponentInspector == nullptr || ComponentInspector->GetInspectedObject() == nullptr)
        {
            continue;
        }
        Object* ComponentInstance = ComponentInspector->GetInspectedObject();
        const QString ComponentType = ComponentInstance->GetClass() != nullptr
            ? QString::fromStdString(ComponentInstance->GetClass()->GetTypeId().Value)
            : QString::fromStdString(ComponentInstance->GetName());
        ComponentInspector->parentWidget()->setVisible(ComponentFilter.isEmpty() || ComponentFilter == ComponentType);
    }
}

