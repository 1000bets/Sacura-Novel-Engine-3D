#pragma once

#include "Core/Object/ObjectHandle.h"
#include "Reflection/ReflectedValue.h"
#include "Reflection/TypeId.h"

#include <memory>
#include <functional>
#include <string>
#include <vector>

class SceneDocument;

class EditorCommand
{
public:
    virtual ~EditorCommand() = default;
    virtual bool Do() = 0;
    virtual bool Undo() = 0;
    virtual const char* GetDisplayName() const = 0;
    // Conservative by default: only commands that preserve scene structure and
    // component properties may opt in, including groups of such commands.
    virtual bool IsTransformOnly() const { return false; }

private:
    friend class EditorCommandStack;
    uint64_t PreviousRevision = 0;
    uint64_t CompletedRevision = 0;
};

class EditorCommandStack
{
public:
    void BindDocument(SceneDocument* Document);
    void SetChangedCallback(std::function<void()> Callback) { ChangedCallback = std::move(Callback); }

    bool Execute(std::unique_ptr<EditorCommand> Command);
    bool Undo();
    bool Redo();
    void Clear();

    void BeginGroup(const std::string& DisplayName);
    void EndGroup();
    bool CancelGroup();

    bool CanUndo() const;
    bool CanRedo() const;
    bool IsUndoTransformOnly() const;
    bool IsRedoTransformOnly() const;
    const std::string& GetUndoDisplayName() const;
    const std::string& GetRedoDisplayName() const;

    SceneDocument* GetDocument() const { return BoundDocument; }

private:
    struct GroupCommand : public EditorCommand
    {
        std::string DisplayName;
        std::vector<std::unique_ptr<EditorCommand>> Children;

        bool Do() override;
        bool Undo() override;
        bool IsTransformOnly() const override;
        const char* GetDisplayName() const override { return DisplayName.c_str(); }
    };

    void PushDone(std::unique_ptr<EditorCommand> Command);
    void NotifyDocumentChanged();

    std::function<void()> ChangedCallback;
    SceneDocument* BoundDocument = nullptr;
    std::vector<std::unique_ptr<EditorCommand>> UndoCommands;
    std::vector<std::unique_ptr<EditorCommand>> RedoCommands;
    std::unique_ptr<GroupCommand> OpenGroup;
    std::string EmptyName;
    mutable std::string UndoDisplayNameCache;
    mutable std::string RedoDisplayNameCache;
};
