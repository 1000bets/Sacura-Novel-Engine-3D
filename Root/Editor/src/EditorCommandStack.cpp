#include "EditorCommandStack.h"
#include "SceneDocument.h"

#include "Core/Threading/ThreadContext.h"

void EditorCommandStack::BindDocument(SceneDocument* Document)
{
    BoundDocument = Document;
    Clear();
}

void EditorCommandStack::NotifyDocumentChanged()
{
    if (BoundDocument != nullptr)
    {
        BoundDocument->MarkDirty();
    }
}

void EditorCommandStack::PushDone(std::unique_ptr<EditorCommand> Command)
{
    UndoCommands.push_back(std::move(Command));
    RedoCommands.clear();
    NotifyDocumentChanged();
}

bool EditorCommandStack::Execute(std::unique_ptr<EditorCommand> Command)
{
    if (Command == nullptr)
    {
        return false;
    }

    if (!Command->Do())
    {
        PrintString(std::string("EditorCommandStack: execute failed: ") + Command->GetDisplayName());
        return false;
    }

    if (OpenGroup != nullptr)
    {
        OpenGroup->Children.push_back(std::move(Command));
        NotifyDocumentChanged();
        return true;
    }

    PushDone(std::move(Command));
    return true;
}

bool EditorCommandStack::GroupCommand::Do()
{
    for (std::unique_ptr<EditorCommand>& Child : Children)
    {
        if (Child == nullptr || !Child->Do())
        {
            return false;
        }
    }
    return true;
}

bool EditorCommandStack::GroupCommand::Undo()
{
    for (auto Iterator = Children.rbegin(); Iterator != Children.rend(); ++Iterator)
    {
        if (*Iterator == nullptr || !(*Iterator)->Undo())
        {
            return false;
        }
    }
    return true;
}

bool EditorCommandStack::Undo()
{
    if (UndoCommands.empty())
    {
        return false;
    }

    std::unique_ptr<EditorCommand> Command = std::move(UndoCommands.back());
    UndoCommands.pop_back();
    if (!Command->Undo())
    {
        PrintString(std::string("EditorCommandStack: undo failed: ") + Command->GetDisplayName());
        UndoCommands.push_back(std::move(Command));
        return false;
    }

    RedoCommands.push_back(std::move(Command));
    NotifyDocumentChanged();
    return true;
}

bool EditorCommandStack::Redo()
{
    if (RedoCommands.empty())
    {
        return false;
    }

    std::unique_ptr<EditorCommand> Command = std::move(RedoCommands.back());
    RedoCommands.pop_back();
    if (!Command->Do())
    {
        PrintString(std::string("EditorCommandStack: redo failed: ") + Command->GetDisplayName());
        RedoCommands.push_back(std::move(Command));
        return false;
    }

    UndoCommands.push_back(std::move(Command));
    NotifyDocumentChanged();
    return true;
}

void EditorCommandStack::Clear()
{
    UndoCommands.clear();
    RedoCommands.clear();
    OpenGroup.reset();
}

void EditorCommandStack::BeginGroup(const std::string& DisplayName)
{
    if (OpenGroup != nullptr)
    {
        EndGroup();
    }
    OpenGroup = std::make_unique<GroupCommand>();
    OpenGroup->DisplayName = DisplayName.empty() ? "Edit" : DisplayName;
}

void EditorCommandStack::EndGroup()
{
    if (OpenGroup == nullptr)
    {
        return;
    }

    if (OpenGroup->Children.empty())
    {
        OpenGroup.reset();
        return;
    }

    if (OpenGroup->Children.size() == 1)
    {
        PushDone(std::move(OpenGroup->Children.front()));
        OpenGroup.reset();
        return;
    }

    PushDone(std::move(OpenGroup));
}

bool EditorCommandStack::CanUndo() const
{
    return !UndoCommands.empty();
}

bool EditorCommandStack::CanRedo() const
{
    return !RedoCommands.empty();
}

const std::string& EditorCommandStack::GetUndoDisplayName() const
{
    if (UndoCommands.empty())
    {
        return EmptyName;
    }
    UndoDisplayNameCache = UndoCommands.back()->GetDisplayName();
    return UndoDisplayNameCache;
}

const std::string& EditorCommandStack::GetRedoDisplayName() const
{
    if (RedoCommands.empty())
    {
        return EmptyName;
    }
    RedoDisplayNameCache = RedoCommands.back()->GetDisplayName();
    return RedoDisplayNameCache;
}
