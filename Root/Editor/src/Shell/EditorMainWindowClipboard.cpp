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

bool EditorMainWindow::IsTextClipboardWidget(QWidget* Candidate)
{
    if (Candidate == nullptr)
    {
        return false;
    }
    if (qobject_cast<QLineEdit*>(Candidate) != nullptr
        || qobject_cast<QPlainTextEdit*>(Candidate) != nullptr
        || qobject_cast<QTextEdit*>(Candidate) != nullptr
        || qobject_cast<QAbstractSpinBox*>(Candidate) != nullptr)
    {
        return true;
    }
    if (auto* Combo = qobject_cast<QComboBox*>(Candidate))
    {
        return Combo->isEditable();
    }
    return qobject_cast<QAbstractSpinBox*>(Candidate->parentWidget()) != nullptr;
}

bool EditorMainWindow::CopyFocusedTextWidget()
{
    QWidget* FocusedWidget = QApplication::focusWidget();
    if (!IsTextClipboardWidget(FocusedWidget))
    {
        return false;
    }
    if (auto* LineEdit = qobject_cast<QLineEdit*>(FocusedWidget))
    {
        LineEdit->copy();
        return true;
    }
    if (auto* PlainTextEdit = qobject_cast<QPlainTextEdit*>(FocusedWidget))
    {
        PlainTextEdit->copy();
        return true;
    }
    if (auto* TextEdit = qobject_cast<QTextEdit*>(FocusedWidget))
    {
        TextEdit->copy();
        return true;
    }
    if (auto* NestedLineEdit = FocusedWidget->findChild<QLineEdit*>())
    {
        NestedLineEdit->copy();
        return true;
    }
    return false;
}

bool EditorMainWindow::PasteFocusedTextWidget()
{
    QWidget* FocusedWidget = QApplication::focusWidget();
    if (!IsTextClipboardWidget(FocusedWidget))
    {
        return false;
    }
    if (auto* LineEdit = qobject_cast<QLineEdit*>(FocusedWidget))
    {
        LineEdit->paste();
        return true;
    }
    if (auto* PlainTextEdit = qobject_cast<QPlainTextEdit*>(FocusedWidget))
    {
        PlainTextEdit->paste();
        return true;
    }
    if (auto* TextEdit = qobject_cast<QTextEdit*>(FocusedWidget))
    {
        TextEdit->paste();
        return true;
    }
    if (auto* NestedLineEdit = FocusedWidget->findChild<QLineEdit*>())
    {
        NestedLineEdit->paste();
        return true;
    }
    return false;
}

bool EditorMainWindow::HandleClipboardShortcut(QEvent* Event)
{
    if (Event == nullptr
        || (Event->type() != QEvent::ShortcutOverride && Event->type() != QEvent::KeyPress))
    {
        return false;
    }

    auto* KeyEvent = static_cast<QKeyEvent*>(Event);
    if (!KeyEvent->matches(QKeySequence::Copy) && !KeyEvent->matches(QKeySequence::Paste))
    {
        return false;
    }
    if (IsTextClipboardWidget(QApplication::focusWidget()))
    {
        return false;
    }
    if (Event->type() == QEvent::ShortcutOverride)
    {
        KeyEvent->accept();
        return true;
    }
    if (KeyEvent->matches(QKeySequence::Copy))
    {
        OnCopy();
    }
    else
    {
        OnPaste();
    }
    KeyEvent->accept();
    return true;
}

bool EditorMainWindow::CopySelectedObjectToClipboard()
{
    GameObject* Selected = GetSelectedGameObject();
    if (Selected == nullptr)
    {
        return false;
    }

    std::string SerializedSubtree;
    SceneSerializeResult Serialized = SceneSerializer::SerializeSubtreeToJson(
        *Selected,
        SerializedSubtree);
    if (!Serialized.bOk)
    {
        statusBar()->showMessage(
            tr("Copy failed: %1").arg(QString::fromStdString(Serialized.Error)),
            2500);
        return false;
    }

    auto* MimeData = new QMimeData();
    MimeData->setData(
        SakuraSceneSubtreeMimeType,
        QByteArray::fromStdString(SerializedSubtree));
    QApplication::clipboard()->setMimeData(MimeData);
    statusBar()->showMessage(tr("Copied object subtree"), 1500);
    return true;
}

bool EditorMainWindow::PasteObjectFromClipboard()
{
    Scene* EditScene = GetEditScene();
    const QMimeData* MimeData = QApplication::clipboard()->mimeData();
    if (EditScene == nullptr
        || BoundEngine.GetPlaySession().IsSimulating()
        || MimeData == nullptr
        || !MimeData->hasFormat(SakuraSceneSubtreeMimeType))
    {
        return false;
    }

    ObjectHandle Parent;
    if (GameObject* Selected = GetSelectedGameObject())
    {
        if (Selected->GetParent() != nullptr)
        {
            Parent = Selected->GetParent()->GetObjectHandle();
        }
    }

    ObjectHandle Created;
    std::unique_ptr<EditorCommand> Command = MakePasteSubtreeCommand(
        EditScene,
        MimeData->data(SakuraSceneSubtreeMimeType).toStdString(),
        Parent,
        &Created);
    if (!ExecuteCommand(std::move(Command)))
    {
        statusBar()->showMessage(tr("Paste failed"), 2500);
        return false;
    }
    SelectObject(Created);
    statusBar()->showMessage(tr("Pasted object subtree"), 1500);
    return true;
}

void EditorMainWindow::OnCopy()
{
    if (CopyFocusedTextWidget())
    {
        return;
    }
    QWidget* FocusedWidget = QApplication::focusWidget();
    if (ContentBrowser != nullptr
        && FocusedWidget != nullptr
        && (FocusedWidget == ContentBrowser || ContentBrowser->isAncestorOf(FocusedWidget)))
    {
        ContentBrowser->CopySelectionToClipboard();
        return;
    }
    CopySelectedObjectToClipboard();
}

void EditorMainWindow::OnPaste()
{
    if (PasteFocusedTextWidget())
    {
        return;
    }
    QWidget* FocusedWidget = QApplication::focusWidget();
    if (ContentBrowser != nullptr
        && FocusedWidget != nullptr
        && (FocusedWidget == ContentBrowser || ContentBrowser->isAncestorOf(FocusedWidget)))
    {
        ContentBrowser->PasteFromClipboard();
        return;
    }
    PasteObjectFromClipboard();
}

