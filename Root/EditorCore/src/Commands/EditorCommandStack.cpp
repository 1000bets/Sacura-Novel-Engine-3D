#include "Commands/EditorCommandStack.h"
#include "Documents/SceneDocument.h"

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
    NotifyDocumentChanged();
    if (BoundDocument != nullptr)
    {
        Command->CompletedRevision = BoundDocument->GetRevision();
    }
    UndoCommands.push_back(std::move(Command));
    RedoCommands.clear();
}

bool EditorCommandStack::Execute(std::unique_ptr<EditorCommand> Command)
{
    if (Command == nullptr)
    {
        return false;
    }

    if (BoundDocument != nullptr)
    {
        Command->PreviousRevision = BoundDocument->GetRevision();
    }
    if (!Command->Do())
    {
        CancelGroup();
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
    size_t AppliedCount = 0;
    for (std::unique_ptr<EditorCommand>& Child : Children)
    {
        if (!Child->Do())
        {
            while (AppliedCount > 0)
            {
                --AppliedCount;
                if (!Children[AppliedCount]->Undo())
                {
                    PrintString("EditorCommandStack: group rollback failed");
                }
            }
            return false;
        }
        ++AppliedCount;
    }
    return true;
}

bool EditorCommandStack::GroupCommand::Undo()
{
    size_t RemainingCount = Children.size();
    while (RemainingCount > 0)
    {
        --RemainingCount;
        if (!Children[RemainingCount]->Undo())
        {
            for (size_t RestoreIndex = RemainingCount + 1; RestoreIndex < Children.size(); ++RestoreIndex)
            {
                if (!Children[RestoreIndex]->Do())
                {
                    PrintString("EditorCommandStack: group undo recovery failed");
                }
            }
            return false;
        }
    }
    return true;
}

bool EditorCommandStack::Undo()
{
    if (OpenGroup != nullptr || UndoCommands.empty())
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

    if (BoundDocument != nullptr)
    {
        BoundDocument->RestoreRevision(Command->PreviousRevision);
    }
    RedoCommands.push_back(std::move(Command));
    return true;
}

bool EditorCommandStack::Redo()
{
    if (OpenGroup != nullptr || RedoCommands.empty())
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

    if (BoundDocument != nullptr)
    {
        BoundDocument->RestoreRevision(Command->CompletedRevision);
    }
    UndoCommands.push_back(std::move(Command));
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
    if (BoundDocument != nullptr)
    {
        OpenGroup->PreviousRevision = BoundDocument->GetRevision();
    }
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
        OpenGroup->Children.front()->PreviousRevision = OpenGroup->PreviousRevision;
        PushDone(std::move(OpenGroup->Children.front()));
        OpenGroup.reset();
        return;
    }

    PushDone(std::move(OpenGroup));
}

bool EditorCommandStack::CancelGroup()
{
    if (OpenGroup == nullptr)
    {
        return true;
    }
    if (!OpenGroup->Undo())
    {
        PrintString("EditorCommandStack: cannot cancel command group");
        return false;
    }
    if (BoundDocument != nullptr)
    {
        BoundDocument->RestoreRevision(OpenGroup->PreviousRevision);
    }
    OpenGroup.reset();
    return true;
}

bool EditorCommandStack::CanUndo() const
{
    return OpenGroup == nullptr && !UndoCommands.empty();
}

bool EditorCommandStack::CanRedo() const
{
    return OpenGroup == nullptr && !RedoCommands.empty();
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
