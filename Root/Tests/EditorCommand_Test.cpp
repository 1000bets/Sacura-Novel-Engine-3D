#include "Core/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "EditorCommandStack.h"
#include "EditorCommands.h"
#include "Engine.h"
#include "Game/Scene.h"
#include "Gameplay/CameraComponent.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/LightComponent.h"
#include "Reflection/PropertyAccess.h"
#include "Reflection/ReflectedValue.h"
#include "SceneDocument.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace
{
int Failures = 0;

void Expect(bool Condition, const char* Message)
{
    if (!Condition)
    {
        ++Failures;
        std::cout << "FAIL: " << Message << "\n" << std::flush;
        return;
    }
    std::cout << "PASS: " << Message << "\n" << std::flush;
}
}

int main()
{
    SetCurrentThreadRole(ThreadRole::Game);

    Engine BoundEngine;
    BoundEngine.InitializeHeadless({});

    SceneDocument Document;
    Document.BindEngine(&BoundEngine);

    Scene* World = MemorySubsystem::Get()->NewObject<Scene>("CommandWorld");
    std::string Error;
    Expect(Document.AdoptScene(World, std::filesystem::temp_directory_path() / "SakuraEditorCommand.scene", Error),
        "Adopt scene into document");

    EditorCommandStack Stack;
    Stack.BindDocument(&Document);

    ObjectHandle Created;
    Expect(Stack.Execute(MakeCreateObjectCommand(World, "Parent", ObjectHandle{}, &Created)), "Create parent");
    GameObject* Parent = World->FindByHandle(Created);
    Expect(Parent != nullptr, "Parent exists");

    ObjectHandle ChildHandle;
    Expect(Stack.Execute(MakeCreateObjectCommand(World, "Child", Created, &ChildHandle)), "Create child");
    GameObject* Child = World->FindByHandle(ChildHandle);
    Expect(Child != nullptr && Child->GetParent() == Parent, "Child parented");

    Expect(Stack.Execute(MakeRenameObjectCommand(World, ChildHandle, "ChildRenamed")), "Rename child");
    Expect(Child->GetName() == "ChildRenamed", "Rename applied");
    Expect(Document.IsDirty(), "Document dirty after edit");

    Transform Moved = Child->GetTransform();
    Moved.Position = Vector3(3.f, 4.f, 5.f);
    Expect(Stack.Execute(MakeSetTransformCommand(World, ChildHandle, Moved)), "Set transform");
    Expect(Child->GetTransform().Position.x == 3.f, "Transform applied");

    ObjectHandle CameraHandle;
    Expect(Stack.Execute(MakeAddComponentCommand(
            World,
            ChildHandle,
            TypeId{CameraComponent::StaticReflectionTypeId()},
            &CameraHandle)),
        "Add CameraComponent");
    CameraComponent* Camera = Child->GetComponent<CameraComponent>();
    Expect(Camera != nullptr && Camera->GetObjectHandle() == CameraHandle, "Camera attached via factory");

    Expect(Stack.Execute(MakeSetPropertyCommand(
            World,
            CameraHandle,
            PropertyId{"field_of_view"},
            ReflectedValue::MakeFloat(42.f))),
        "Set camera FOV");
    Expect(Camera->GetFieldOfViewDegrees() == 42.f, "FOV applied");

    Expect(Stack.Execute(MakeResetPropertyCommand(World, CameraHandle, PropertyId{"field_of_view"})), "Reset FOV");
    Expect(Camera->GetFieldOfViewDegrees() == 60.f, "FOV reset to CDO");

    ObjectHandle LightHandle;
    Expect(Stack.Execute(MakeAddComponentCommand(
            World,
            ChildHandle,
            TypeId{LightComponent::StaticReflectionTypeId()},
            &LightHandle)),
        "Add LightComponent");
    Expect(Child->GetComponent<LightComponent>() != nullptr, "Light attached");

    Expect(!Stack.Execute(MakeReparentObjectCommand(World, Created, ChildHandle)), "Reject reparent cycle");
    Expect(Parent->GetParent() == nullptr, "Parent unchanged after cycle reject");

    Expect(Stack.Execute(MakeReparentObjectCommand(World, ChildHandle, ObjectHandle{})), "Reparent child to root");
    Expect(Child->GetParent() == nullptr, "Child is root");

    Expect(Stack.Undo(), "Undo reparent");
    Expect(Child->GetParent() == Parent, "Child restored under parent");

    const size_t CountBeforeDelete = World->GetObjectCount();
    Expect(Stack.Execute(MakeDeleteObjectCommand(World, Created)), "Delete parent subtree");
    Expect(World->FindByHandle(Created) == nullptr, "Parent gone");
    Expect(World->FindByHandle(ChildHandle) == nullptr, "Child gone with parent");
    Expect(World->GetObjectCount() + 2 == CountBeforeDelete || World->GetObjectCount() < CountBeforeDelete,
        "Object count reduced");

    Expect(Stack.Undo(), "Undo delete subtree");
    Expect(World->GetObjectCount() >= 2, "Subtree restored");
    GameObject* RestoredParent = World->FindByName("Parent");
    GameObject* RestoredChild = World->FindByName("ChildRenamed");
    Expect(RestoredParent != nullptr && RestoredChild != nullptr, "Restored names");
    Expect(RestoredChild->GetParent() == RestoredParent, "Restored hierarchy");
    Expect(RestoredChild->GetComponent<CameraComponent>() != nullptr, "Restored camera");
    Expect(RestoredChild->GetComponent<LightComponent>() != nullptr, "Restored light");

    Expect(Stack.Redo(), "Redo delete");
    Expect(World->FindByName("Parent") == nullptr, "Redo removed parent");

    Expect(Stack.Undo(), "Undo delete again");
    Expect(Document.Save(Error), "Save dirty scene");
    Expect(!Document.IsDirty(), "Clean after successful save");

    const std::filesystem::path SavedPath = Document.GetDocumentPath();
    Document.Close();
    Expect(Document.Open(SavedPath, Error), "Reopen saved scene");
    Expect(Document.GetScene() != nullptr && Document.GetScene()->FindByName("Parent") != nullptr, "Reopened parent");
    Expect(Document.GetScene()->FindByName("ChildRenamed") != nullptr, "Reopened child");

    Document.Close();
    BoundEngine.Shutdown();

    std::cout << "Failures: " << Failures << "\n";
    return Failures == 0 ? 0 : 1;
}
