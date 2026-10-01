#define SAKURA_PREFAB_DIAG 1
// Headless regression test for the prefab system (Engine + EditorCore link, no QApplication/GPU).
// Covers: create/instantiate, property + structural overrides, save/load round trip, propagation,
// revert, apply, nested prefabs, variants, unpack, paste remap, partial-subtree undo, cycles.

#include "Assets/AssetRegistry.h"
#include "EditorCommandStack.h"
#include "PrefabCommands.h"
#include "Core/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Game/Scene.h"
#include "Game/SceneSerializer.h"
#include "Gameplay/CameraComponent.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/LightComponent.h"
#include "Prefab/PrefabLibrary.h"
#include "Prefab/PrefabUtility.h"
#include "Reflection/PropertyAccess.h"
#include "Reflection/ReflectionSubsystem.h"

#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>

namespace
{
int Failures = 0;

void Expect(bool Condition, const std::string& Message)
{
    if (!Condition)
    {
        ++Failures;
        std::cout << "FAIL: " << Message << "\n" << std::flush;
        return;
    }
    std::cout << "PASS: " << Message << "\n" << std::flush;
}

GameObject* FindChild(GameObject* Parent, const std::string& Name)
{
    if (Parent == nullptr)
    {
        return nullptr;
    }
    for (GameObject* Child : Parent->GetChildren())
    {
        if (Child->GetName() == Name)
        {
            return Child;
        }
    }
    return nullptr;
}

float Fov(GameObject* Object)
{
    CameraComponent* Camera = Object != nullptr ? Object->GetComponent<CameraComponent>() : nullptr;
    return Camera != nullptr ? Camera->GetFieldOfViewDegrees() : -1.0f;
}

void SetFov(GameObject* Object, float Value)
{
    PropertyAccess::SetProperty(Object->GetComponent<CameraComponent>(), PropertyId{"field_of_view"}, ReflectedValue::MakeFloat(Value));
}

size_t CountOverrides(Scene& World, GameObject* Root, PrefabOverrideKind Kind)
{
    std::vector<PrefabOverride> Overrides;
    std::string Error;
    PrefabUtility::GetOverrides(World, *Root, Overrides, Error);
    size_t Count = 0;
    for (const PrefabOverride& Override : Overrides)
    {
        Count += Override.Kind == Kind ? 1 : 0;
    }
    return Count;
}

size_t CountAllOverrides(Scene& World, GameObject* Root)
{
    std::vector<PrefabOverride> Overrides;
    std::string Error;
    PrefabUtility::GetOverrides(World, *Root, Overrides, Error);
    return Overrides.size();
}

// Edits a prefab asset the same way the prefab editor window does.
bool EditPrefab(const AssetId& Prefab, const std::function<void(Scene&, GameObject*)>& Edit)
{
    Scene* Temp = MemorySubsystem::Get()->NewObject<Scene>("PrefabEdit");
    GameObject* Root = nullptr;
    std::string Error;
    bool bOk = PrefabUtility::LoadPrefabIntoScene(Prefab, *Temp, Root, Error);
    if (bOk)
    {
        Edit(*Temp, Root);
        bOk = PrefabUtility::SavePrefabFromScene(*Temp, Prefab, Error);
    }
    if (!bOk)
    {
        std::cout << "  prefab edit error: " << Error << "\n";
    }
    MemorySubsystem::Get()->DestroyObject(Temp);
    return bOk;
}

Scene* RoundTrip(Scene* World)
{
    std::string Json;
    const SceneSerializeResult Saved = SceneSerializer::SerializeToJson(*World, Json);
    Scene* Loaded = nullptr;
    const SceneSerializeResult Restored = SceneSerializer::DeserializeFromJson(Json, Loaded);
    if (!Saved.bOk || !Restored.bOk)
    {
        std::cout << "  round trip error: " << Saved.Error << " / " << Restored.Error << "\n";
        return nullptr;
    }
    MemorySubsystem::Get()->DestroyObject(World);
#ifdef SAKURA_PREFAB_DIAG
    int InstanceRoots = 0;
    for (GameObject* Object : Loaded->GetAllObjects())
    {
        if (Object->GetPrefabInstance() != nullptr)
        {
            ++InstanceRoots;
            std::cout << "  [diag] loaded instance root '" << Object->GetName()
                      << "' missing=" << (Object->GetPrefabInstance()->IsMissing() ? 1 : 0)
                      << " id=" << Object->GetPersistentId() << "\n";
        }
    }
    std::cout << "  [diag] instance roots after load: " << InstanceRoots << "\n";
    std::cout << "  [diag] --- serialized scene JSON ---\n" << Json << "\n  [diag] --- end JSON ---\n";
#endif
    return Loaded;
}
}

int main()
{
    SetCurrentThreadRole(ThreadRole::Game);
    MemorySubsystem Memory;
    Memory.Initialize();
    Expect(ReflectionSubsystem::Get().InitializeNative().bOk, "Reflection initialized");

    const std::filesystem::path ContentRoot = std::filesystem::temp_directory_path() / "SakuraPrefabTest" / "Content";
    std::filesystem::remove_all(ContentRoot.parent_path());
    std::filesystem::create_directories(ContentRoot);
    {
    AssetRegistry Registry;
    Registry.SetGameContentRoot(ContentRoot);
    Registry.ScanContent();
    PrefabLibrary::Get().BindRegistry(&Registry);
    std::string Error;

    Scene* World = Memory.NewObject<Scene>("World");

    // ------------------------------------------------------------ create
    GameObject* Door = World->CreateGameObject("Door");
    GameObject* Handle = World->CreateGameObject("Handle");
    Handle->SetParent(Door);
    Handle->AddComponent<CameraComponent>();
    SetFov(Handle, 60.0f);
    const std::string DoorId = Door->GetPersistentId();
    const std::string HandleId = Handle->GetPersistentId();

    AssetId DoorAsset{};
    Error.clear();
    const bool bDoorCreated = PrefabUtility::CreatePrefabAsset(*World, *Door, ContentRoot / "Door.prefab", false, DoorAsset, Error);
    Expect(bDoorCreated, "Create Door prefab: " + (Error.empty() ? std::string("<no error text>") : Error));
    if (!bDoorCreated || !DoorAsset.IsValid())
    {
        // Nothing below can run without the base asset; stop instead of crashing.
        std::cout << "ABORT: base prefab asset was not created (registry/path issue), skipping remaining checks\n";
        std::cout << (Failures == 0 ? "ALL PASS" : "FAILURES: " + std::to_string(Failures)) << "\n";
        return 1;
    }
    Expect(PrefabUtility::IsOutermostInstanceRoot(Door), "Source object became an instance root");
    Expect(Door->GetPersistentId() == DoorId && Handle->GetPersistentId() == HandleId, "Create prefab keeps object ids");
    Expect(CountAllOverrides(*World, Door) == 0, "Fresh instance has no overrides");

    // ------------------------------------------------------------ instantiate + property overrides
    GameObject* Door2 = PrefabUtility::InstantiatePrefab(*World, DoorAsset, nullptr, Error);
    Expect(Door2 != nullptr && FindChild(Door2, "Handle") != nullptr, "Instantiate Door: " + Error);
    Expect(Door2 != nullptr && Door2->GetPersistentId() != DoorId, "Second instance has its own ids");
    GameObject* Handle2 = FindChild(Door2, "Handle");
    SetFov(Handle2, 42.0f);
    Transform Moved = Door2->GetTransform();
    Moved.Position = Vector3(5.f, 0.f, 0.f);
    Door2->SetTransform(Moved);
    Expect(CountOverrides(*World, Door2, PrefabOverrideKind::ComponentProperty) == 1, "Component property override detected");
    Expect(CountOverrides(*World, Door2, PrefabOverrideKind::ObjectProperty) == 1, "Transform override detected");

    // ------------------------------------------------------------ save/load
    const std::string Door2Id = Door2->GetPersistentId();
    World = RoundTrip(World);
    Expect(World != nullptr, "Scene with prefab instances round-trips");
    Door = World->FindByPersistentId(DoorId);
    Door2 = World->FindByPersistentId(Door2Id);
    Expect(PrefabUtility::IsOutermostInstanceRoot(Door2), "Instance link survives load");
    Expect(Fov(FindChild(Door2, "Handle")) == 42.0f, "Override value survives load");
    Expect(Door2->GetTransform().Position.x == 5.f, "Transform override survives load");
    Expect(CountAllOverrides(*World, Door2) == 2, "Override set survives load");

    // ------------------------------------------------------------ propagation
    Expect(EditPrefab(DoorAsset, [](Scene& Temp, GameObject* Root)
    {
        SetFov(FindChild(Root, "Handle"), 70.0f);
        GameObject* Knob = Temp.CreateGameObject("Knob");
        Knob->SetParent(Root);
    }), "Edit Door prefab (fov 70 + Knob)");
    PrefabUtility::RefreshInstances(*World, DoorAsset, Error);
    Expect(Fov(FindChild(Door, "Handle")) == 70.0f, "Non-overridden property propagated");
    Expect(Fov(FindChild(Door2, "Handle")) == 42.0f, "Overridden property kept after propagation");
    Expect(FindChild(Door, "Knob") != nullptr && FindChild(Door2, "Knob") != nullptr, "New prefab child propagated");
    Expect(World->FindByPersistentId(HandleId) == FindChild(Door, "Handle"), "Propagation preserves live objects");

    // ------------------------------------------------------------ structural overrides
    World->DestroyGameObject(FindChild(Door2, "Knob"));
    FindChild(Door2, "Handle")->AddComponent<LightComponent>();
    GameObject* Sign = World->CreateGameObject("Sign");
    Sign->SetParent(FindChild(Door2, "Handle"));
    Expect(CountOverrides(*World, Door2, PrefabOverrideKind::RemovedObject) == 1, "Removed object override");
    Expect(CountOverrides(*World, Door2, PrefabOverrideKind::AddedComponent) == 1, "Added component override");
    Expect(CountOverrides(*World, Door2, PrefabOverrideKind::AddedObject) == 1, "Added object override");
    World = RoundTrip(World);
    Door = World->FindByPersistentId(DoorId);
    Door2 = World->FindByPersistentId(Door2Id);
    Expect(FindChild(Door2, "Knob") == nullptr, "Removed object stays removed after load");
    Expect(FindChild(Door2, "Handle")->GetComponent<LightComponent>() != nullptr, "Added component survives load");
    Expect(FindChild(FindChild(Door2, "Handle"), "Sign") != nullptr, "Added child survives load");

    // ------------------------------------------------------------ revert
    std::vector<PrefabOverride> Overrides;
    PrefabUtility::GetOverrides(*World, *Door2, Overrides, Error);
    std::vector<PrefabOverride> RemovedOnly;
    for (const PrefabOverride& Override : Overrides)
    {
        if (Override.Kind == PrefabOverrideKind::RemovedObject)
        {
            RemovedOnly.push_back(Override);
        }
    }
    Expect(PrefabUtility::RevertOverrides(*World, *Door2, RemovedOnly, Error), "Revert single override");
    Expect(FindChild(Door2, "Knob") != nullptr, "Reverted removal restores the object");
    Expect(Fov(FindChild(Door2, "Handle")) == 42.0f, "Other overrides untouched by single revert");
    Expect(PrefabUtility::RevertAll(*World, *Door2, Error), "Revert all");
    Expect(Fov(FindChild(Door2, "Handle")) == 70.0f && Door2->GetTransform().Position.x == 0.f, "Revert all restores template values");
    Expect(FindChild(FindChild(Door2, "Handle"), "Sign") == nullptr, "Revert all removes added objects");
    Expect(FindChild(Door2, "Handle")->GetComponent<LightComponent>() == nullptr, "Revert all removes added components");
    Expect(CountAllOverrides(*World, Door2) == 0, "No overrides after revert all");

    // ------------------------------------------------------------ apply
    SetFov(FindChild(Door, "Handle"), 55.0f);
    GameObject* Lamp = World->CreateGameObject("Lamp");
    Lamp->SetParent(Door);
    const std::string LampId = Lamp->GetPersistentId();
    Expect(PrefabUtility::ApplyOverrides(*World, *Door, {}, Error), "Apply all overrides: " + Error);
    Expect(CountAllOverrides(*World, Door) == 0, "Applied instance has no overrides");
    Expect(Fov(FindChild(Door2, "Handle")) == 55.0f, "Applied value reached other instances");
    Expect(FindChild(Door2, "Lamp") != nullptr, "Applied added object reached other instances");
    Expect(World->FindByPersistentId(LampId) != nullptr
        && World->FindByPersistentId(LampId)->GetPrefabInstanceId() == Door->GetPrefabInstanceId(),
        "Applied added object kept its id and became prefab-owned");

    // ------------------------------------------------------------ nested
    GameObject* Car = World->CreateGameObject("Car");
    GameObject* NestedDoor = PrefabUtility::InstantiatePrefab(*World, DoorAsset, Car, Error);
    AssetId CarAsset{};
    Expect(NestedDoor != nullptr && PrefabUtility::CreatePrefabAsset(*World, *Car, ContentRoot / "Car.prefab", false, CarAsset, Error),
        "Create Car prefab with nested Door: " + Error);
    Expect(PrefabUtility::IsNestedPrefabRoot(NestedDoor), "Nested prefab root detected");
    GameObject* Car2 = PrefabUtility::InstantiatePrefab(*World, CarAsset, nullptr, Error);
    GameObject* Car2Door = FindChild(Car2, "Door");
    Expect(Car2Door != nullptr && Fov(FindChild(Car2Door, "Handle")) == 55.0f, "Nested prefab instantiated");
    SetFov(FindChild(Car2Door, "Handle"), 11.0f);
    Expect(EditPrefab(DoorAsset, [](Scene&, GameObject* Root)
    {
        FindChild(Root, "Knob")->SetName("DoorKnob");
    }), "Edit inner Door prefab");
    PrefabUtility::RefreshInstances(*World, DoorAsset, Error);
    Expect(FindChild(Car2Door, "DoorKnob") != nullptr, "Inner prefab change propagates through nesting");
    Expect(Fov(FindChild(Car2Door, "Handle")) == 11.0f, "Override on nested object kept");
    const std::string Car2Id = Car2->GetPersistentId();
    const std::string Car2DoorId = Car2Door->GetPersistentId();
    World = RoundTrip(World);
    Car2 = World->FindByPersistentId(Car2Id);
    Car2Door = World->FindByPersistentId(Car2DoorId);
    Expect(Car2Door != nullptr && Fov(FindChild(Car2Door, "Handle")) == 11.0f, "Nested override survives load");

    // ------------------------------------------------------------ variant
    AssetId VariantAsset{};
    Expect(PrefabUtility::CreateVariantAsset(DoorAsset, ContentRoot / "DoorRed.prefab", VariantAsset, Error),
        "Create Door variant: " + Error);
    Expect(EditPrefab(VariantAsset, [](Scene&, GameObject* Root)
    {
        SetFov(FindChild(Root, "Handle"), 99.0f);
    }), "Edit variant (fov 99)");
    GameObject* Red = PrefabUtility::InstantiatePrefab(*World, VariantAsset, nullptr, Error);
    Expect(Red != nullptr && Fov(FindChild(Red, "Handle")) == 99.0f, "Variant override applied to instance");
    Expect(EditPrefab(DoorAsset, [](Scene& Temp, GameObject* Root)
    {
        Temp.CreateGameObject("Peephole")->SetParent(Root);
        SetFov(FindChild(Root, "Handle"), 30.0f);
    }), "Edit base Door (Peephole + fov 30)");
    PrefabUtility::RefreshInstances(*World, DoorAsset, Error);
    Expect(FindChild(Red, "Peephole") != nullptr, "Base change reaches variant instances");
    Expect(Fov(FindChild(Red, "Handle")) == 99.0f, "Variant override wins over base change");
    Expect(Fov(FindChild(World->FindByPersistentId(DoorId), "Handle")) == 30.0f, "Base instance receives base change");

    // ------------------------------------------------------------ undoable editor commands
    {
        EditorCommandStack Stack;
        ObjectHandle CreatedHandle;
        Expect(Stack.Execute(MakeInstantiatePrefabCommand(World, DoorAsset, ObjectHandle{}, nullptr, &CreatedHandle)),
            "Instantiate prefab command");
        GameObject* Commanded = World->FindByHandle(CreatedHandle);
        const std::string CommandedId = Commanded != nullptr ? Commanded->GetPersistentId() : std::string{};
        SetFov(FindChild(Commanded, "Handle"), 5.0f);
        std::string CommandError;
        Expect(Stack.Execute(MakePrefabSubtreeEditCommand(World, Commanded->GetObjectHandle(), "Revert All",
            [](Scene& Target, GameObject& Root, std::string& OutError)
            {
                return PrefabUtility::RevertAll(Target, Root, OutError);
            }, &CommandError)), "Revert all as undoable command");
        Expect(Fov(FindChild(World->FindByPersistentId(CommandedId), "Handle")) == 30.0f, "Command reverted override");
        Expect(Stack.Undo() && Fov(FindChild(World->FindByPersistentId(CommandedId), "Handle")) == 5.0f, "Undo revert restores override");
        Expect(Stack.Redo() && Fov(FindChild(World->FindByPersistentId(CommandedId), "Handle")) == 30.0f, "Redo revert");
        Expect(Stack.Undo() && Stack.Undo() && World->FindByPersistentId(CommandedId) == nullptr, "Undo instantiate removes instance");
        Expect(Stack.Redo() && PrefabUtility::IsOutermostInstanceRoot(World->FindByPersistentId(CommandedId)),
            "Redo instantiate restores the same instance ids");
    }

    // ------------------------------------------------------------ paste / partial undo
    Door2 = World->FindByPersistentId(Door2Id);
    SetFov(FindChild(Door2, "Handle"), 12.0f);
    std::string Subtree;
    std::string Remapped;
    GameObject* Pasted = nullptr;
    Expect(SceneSerializer::SerializeSubtreeToJson(*Door2, Subtree).bOk
        && SceneSerializer::RemapSubtreePersistentIds(Subtree, Remapped).bOk
        && SceneSerializer::DeserializeSubtreeFromJson(*World, Remapped, nullptr, Pasted).bOk,
        "Copy/paste a prefab instance");
    Expect(Pasted != nullptr && PrefabUtility::IsOutermostInstanceRoot(Pasted) && Pasted->GetPersistentId() != Door2Id,
        "Pasted copy is a new instance");
    Expect(Pasted != nullptr && Fov(FindChild(Pasted, "Handle")) == 12.0f, "Pasted instance keeps overrides");

    GameObject* Door2Handle = FindChild(Door2, "Handle");
    std::string HandleSubtree;
    SceneSerializer::SerializeSubtreeToJson(*Door2Handle, HandleSubtree);
    World->DestroyGameObject(Door2Handle);
    GameObject* Restored = nullptr;
    Expect(SceneSerializer::DeserializeSubtreeFromJson(*World, HandleSubtree, Door2, Restored).bOk, "Undo delete of an instance child");
    Expect(CountOverrides(*World, Door2, PrefabOverrideKind::RemovedObject) == 0
        && CountOverrides(*World, Door2, PrefabOverrideKind::AddedObject) == 0,
        "Restored child is prefab-owned again");

    // ------------------------------------------------------------ unpack
    Car2 = World->FindByPersistentId(Car2Id);
    Expect(PrefabUtility::Unpack(*World, *Car2, PrefabUnpackMode::OutermostRoot, Error), "Unpack Car one level");
    Car2Door = World->FindByPersistentId(Car2DoorId);
    Expect(!PrefabUtility::IsPartOfPrefabInstance(Car2), "Unpacked root is plain");
    Expect(PrefabUtility::IsOutermostInstanceRoot(Car2Door), "Nested prefab becomes an outermost instance");
    Expect(Fov(FindChild(Car2Door, "Handle")) == 11.0f && CountOverrides(*World, Car2Door, PrefabOverrideKind::ComponentProperty) == 1,
        "Nested overrides transfer to the unpacked instance");
    Expect(PrefabUtility::Unpack(*World, *Red, PrefabUnpackMode::Completely, Error) && !PrefabUtility::IsPartOfPrefabInstance(FindChild(Red, "Handle")),
        "Unpack completely");

    // ------------------------------------------------------------ cycles
    bool bCycleRejected = false;
    EditPrefab(DoorAsset, [&](Scene& Temp, GameObject* Root)
    {
        std::string CycleError;
        GameObject* Inner = PrefabUtility::InstantiatePrefab(Temp, CarAsset, Root, CycleError);
        std::string SaveError;
        bCycleRejected = Inner != nullptr && !PrefabUtility::SavePrefabFromScene(Temp, DoorAsset, SaveError);
        if (Inner != nullptr)
        {
            Temp.DestroyGameObject(Inner);
        }
    });
    Expect(bCycleRejected, "Prefab nesting cycle is rejected");

    Memory.DestroyObject(World);
    PrefabLibrary::Get().BindRegistry(nullptr);
    }
    ReflectionSubsystem::Get().Shutdown();
    if(!std::getenv("SAKURA_PREFAB_TEST_KEEP")) std::filesystem::remove_all(ContentRoot.parent_path());

    std::cout << (Failures == 0 ? "ALL PASS" : "FAILURES: " + std::to_string(Failures)) << "\n";
    return Failures == 0 ? 0 : 1;
}
