#pragma once

#include "Commands/EditorCommandStack.h"
#include "Documents/SceneDocument.h"

class Engine;
class GameObject;
class ProjectSession;

class SceneWorkspace
{
public:
    explicit SceneWorkspace(Engine& Application);
    bool AdoptSessionScene(const ProjectSession& Session, std::string& OutError);
    SceneDocument& GetDocument() { return Document; }
    EditorCommandStack& GetCommands() { return Commands; }
    bool Select(Scene* World, ObjectHandle Identity);
    void ClearSelection();
    ObjectHandle GetSelectedIdentity() const { return SelectedIdentity; }
    GameObject* GetSelectedObject(Scene* World) const;

private:
    Engine& Application;
    SceneDocument Document;
    EditorCommandStack Commands;
    ObjectHandle SelectedIdentity;
};
