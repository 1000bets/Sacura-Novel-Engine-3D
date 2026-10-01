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

void EditorMainWindow::OnFocusActionsAndEvents()
{
    const QString EventTabTitle = QString::fromUtf8("\xD0\xA1\xD0\xBE\xD0\xB1\xD1\x8B\xD1\x82\xD0\xB8\xD0\xB5");
    for (int TabIndex = 0; TabIndex < DockTabs->count(); ++TabIndex)
    {
        if (DockTabs->tabText(TabIndex) == EventTabTitle)
        {
            DockTabs->setCurrentIndex(TabIndex);
            break;
        }
    }
    LayoutCombo->setCurrentIndex(1);
}

bool EditorMainWindow::ExecuteCommand(std::unique_ptr<EditorCommand> Command)
{
    if (BoundEngine.GetPlaySession().IsSimulating())
    {
        statusBar()->showMessage(QString::fromUtf8("Hierarchy edits disabled during play"), 2000);
        return false;
    }
    const bool bOk = CommandStack.Execute(std::move(Command));
    if (bOk)
    {
        PopulateHierarchy();
        RefreshSelectionUi();
        UpdateWindowTitleDirty();
        UpdateStatus();
    }
    return bOk;
}

void EditorMainWindow::OnUndo()
{
    if (!CommandStack.Undo())
    {
        return;
    }
    if (GetSelectedGameObject() == nullptr)
    {
        ClearSelection();
    }
    PopulateHierarchy();
    RefreshSelectionUi();
    UpdateWindowTitleDirty();
    UpdateStatus();
}

void EditorMainWindow::OnRedo()
{
    if (!CommandStack.Redo())
    {
        return;
    }
    PopulateHierarchy();
    RefreshSelectionUi();
    UpdateWindowTitleDirty();
    UpdateStatus();
}

void EditorMainWindow::PopulateCreateMenus(QMenu* ObjectsMenu, QMenu* ComponentsMenu)
{
    if (ObjectsMenu == nullptr || ComponentsMenu == nullptr)
    {
        return;
    }

    ObjectsMenu->clear();
    ComponentsMenu->clear();

    EditorActionContext Context = MakeEditorActionContext();
    int PreviousObjectSortOrder = -1;
    int PreviousComponentSortOrder = -1;
    for (EditorAction* Action : EditorAction::CollectPublishedActions())
    {
        if (Action == nullptr)
        {
            continue;
        }

        QMenu* TargetMenu = nullptr;
        int* PreviousSortOrder = nullptr;
        if (std::strcmp(Action->GetMenuCategory(), EditorAction::ObjectsCategory) == 0)
        {
            TargetMenu = ObjectsMenu;
            PreviousSortOrder = &PreviousObjectSortOrder;
        }
        else if (std::strcmp(Action->GetMenuCategory(), EditorAction::ComponentsCategory) == 0)
        {
            TargetMenu = ComponentsMenu;
            PreviousSortOrder = &PreviousComponentSortOrder;
        }
        if (TargetMenu == nullptr || PreviousSortOrder == nullptr)
        {
            continue;
        }

        if (*PreviousSortOrder >= 0 && Action->GetSortOrder() - *PreviousSortOrder >= 50)
        {
            TargetMenu->addSeparator();
        }
        *PreviousSortOrder = Action->GetSortOrder();

        QAction* MenuAction = TargetMenu->addAction(QString::fromUtf8(Action->GetDisplayName()));
        MenuAction->setEnabled(Action->CanExecute(Context));
        connect(MenuAction, &QAction::triggered, this, [this, Action]()
        {
            RunEditorAction(Action);
        });
    }
}

EditorActionContext EditorMainWindow::MakeEditorActionContext()
{
    EditorActionContext Context;
    Context.EditScene = GetEditScene();
    Context.CommandStack = &CommandStack;
    Context.SelectedObject = GetSelectedObjectHandle();
    Context.ExecuteCommand = [this](std::unique_ptr<EditorCommand> Command)
    {
        return ExecuteCommand(std::move(Command));
    };
    Context.SelectObject = [this](ObjectHandle Target)
    {
        SelectObject(Target);
    };
    return Context;
}

void EditorMainWindow::RunEditorAction(EditorAction* Action)
{
    if (Action == nullptr)
    {
        return;
    }
    EditorActionContext Context = MakeEditorActionContext();
    if (!Action->CanExecute(Context))
    {
        return;
    }
    Action->Execute(Context);
}

void EditorMainWindow::RunEditorActionByTypeId(const char* TypeIdString)
{
    Class* ActionClass = ReflectionSubsystem::Get().FindClass(TypeIdString);
    if (ActionClass == nullptr || ActionClass->GetClassDefaultObject() == nullptr)
    {
        return;
    }
    RunEditorAction(static_cast<EditorAction*>(ActionClass->GetClassDefaultObject()));
}

void EditorMainWindow::OnCreateEmptyObject()
{
    RunEditorActionByTypeId(CreateEmptyObjectAction::StaticReflectionTypeId());
}

void EditorMainWindow::OnDeleteSelectedObject()
{
    Scene* EditScene = GetEditScene();
    GameObject* Selected = GetSelectedGameObject();
    if (EditScene == nullptr || Selected == nullptr)
    {
        return;
    }

    const ObjectHandle Target = Selected->GetObjectHandle();
    if (!ExecuteCommand(MakeDeleteObjectCommand(EditScene, Target)))
    {
        return;
    }
    ClearSelection();
}

