#pragma once

#include "Gameplay/ObjectHandle.h"
#include "Reflection/ReflectedValue.h"
#include "Reflection/TypeId.h"

#include <memory>
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
};

class EditorCommandStack
{
public:
    void BindDocument(SceneDocument* Document);

    bool Execute(std::unique_ptr<EditorCommand> Command);
    bool Undo();
    bool Redo();
    void Clear();

    void BeginGroup(const std::string& DisplayName);
    void EndGroup();

    bool CanUndo() const;
    bool CanRedo() const;
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
        const char* GetDisplayName() const override { return DisplayName.c_str(); }
    };

    void PushDone(std::unique_ptr<EditorCommand> Command);
    void NotifyDocumentChanged();

    SceneDocument* BoundDocument = nullptr;
    std::vector<std::unique_ptr<EditorCommand>> UndoCommands;
    std::vector<std::unique_ptr<EditorCommand>> RedoCommands;
    std::unique_ptr<GroupCommand> OpenGroup;
    std::string EmptyName;
    mutable std::string UndoDisplayNameCache;
    mutable std::string RedoDisplayNameCache;
};
