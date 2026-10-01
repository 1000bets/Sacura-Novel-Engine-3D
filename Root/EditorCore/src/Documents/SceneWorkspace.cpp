#include "Documents/SceneWorkspace.h"

#include "Engine.h"
#include "Project/ProjectSession.h"
#include "World/Scene.h"

SceneWorkspace::SceneWorkspace(Engine& InApplication)
    : Application(InApplication)
{
    Document.BindEngine(&Application);
    Commands.BindDocument(&Document);
}

bool SceneWorkspace::AdoptSessionScene(const ProjectSession& Session, std::string& OutError)
{
    const ProjectDescriptor* Descriptor = Session.GetProject();
    Scene* World = Application.GetEditScene();
    if (Descriptor == nullptr || World == nullptr || Descriptor->StartupScene.empty())
    {
        OutError = "No startup scene is available";
        return false;
    }
    if (!Document.AdoptScene(World, Descriptor->StartupScene, OutError))
    {
        return false;
    }
    Commands.Clear();
    ClearSelection();
    return true;
}

bool SceneWorkspace::Select(Scene* World, ObjectHandle Identity)
{
    if (World == nullptr || World->FindByHandle(Identity) == nullptr)
    {
        ClearSelection();
        return false;
    }
    SelectedIdentity = Identity;
    return true;
}

void SceneWorkspace::ClearSelection()
{
    SelectedIdentity = {};
}

GameObject* SceneWorkspace::GetSelectedObject(Scene* World) const
{
    if (World == nullptr)
    {
        return nullptr;
    }
    return World->FindByHandle(SelectedIdentity);
}
