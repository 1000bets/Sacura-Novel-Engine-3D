#include "Core/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Game/Scene.h"
#include "Gameplay/CameraComponent.h"
#include "Gameplay/Component.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/LightComponent.h"
#include "Gameplay/MeshRendererComponent.h"
#include "Rendering/SceneExtractor.h"
#include "Rendering/RenderScene.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
int FailureCount = 0;

void Expect(bool Condition, const char* Message)
{
    if (!Condition)
    {
        ++FailureCount;
        std::cout << "FAIL: " << Message << '\n';
    }
    else
    {
        std::cout << "  OK: " << Message << '\n';
    }
}

bool NearlyEqual(float Left, float Right, float Epsilon = 0.001f)
{
    return std::fabs(Left - Right) <= Epsilon;
}

void TestParentChildOrders()
{
    std::cout << "\n=== Parent/child creation orders ===\n";
    MemorySubsystem Memory;
    Memory.Initialize();

    {
        Scene* World = Memory.NewObject<Scene>("WorldA");
        GameObject* Child = World->CreateGameObject("ChildFirst");
        GameObject* Parent = World->CreateGameObject("ParentSecond");
        Expect(Child->SetParent(Parent), "Child-then-parent SetParent");
        Expect(Parent->GetChildren().size() == 1 && Parent->GetChildren()[0] == Child, "Parent lists child");
        Expect(World->DestroyGameObject(Parent), "Destroy parent removes subtree");
        Expect(World->GetObjectCount() == 0, "Scene empty after parent destroy");
        Memory.DestroyObject(World);
    }

    {
        Scene* World = Memory.NewObject<Scene>("WorldB");
        GameObject* Parent = World->CreateGameObject("ParentFirst");
        GameObject* Child = World->CreateGameObject("ChildSecond");
        Expect(Child->SetParent(Parent), "Parent-then-child SetParent");
        Expect(World->DestroyGameObject(Child), "Destroy child keeps parent");
        Expect(World->GetObjectCount() == 1 && Parent->GetChildren().empty(), "Parent remains without child");
        Expect(World->DestroyGameObject(Parent), "Destroy remaining parent");
        Memory.DestroyObject(World);
    }

    Memory.Deinitialize();
}

void TestForeignDestroyCycleAndDuplicate()
{
    std::cout << "\n=== Foreign destroy / cycle / duplicate component ===\n";
    MemorySubsystem Memory;
    Memory.Initialize();

    Scene* WorldA = Memory.NewObject<Scene>("A");
    Scene* WorldB = Memory.NewObject<Scene>("B");
    GameObject* ObjectA = WorldA->CreateGameObject("A1");
    GameObject* ObjectB = WorldB->CreateGameObject("B1");
    Expect(!WorldA->DestroyGameObject(ObjectB), "Reject foreign DestroyGameObject");
    Expect(WorldB->GetObjectCount() == 1, "Foreign object still alive");

    GameObject* Parent = WorldA->CreateGameObject("P");
    GameObject* Child = WorldA->CreateGameObject("C");
    Expect(Child->SetParent(Parent), "Valid parent");
    Expect(!Parent->SetParent(Child), "Reject parent cycle");
    Expect(Parent->GetParent() == nullptr, "Parent unchanged after rejected cycle");

    MeshRendererComponent* First = ObjectA->AddComponent<MeshRendererComponent>();
    Expect(First != nullptr, "Add first MeshRenderer");
    Expect(ObjectA->AddExistingComponent(First) == nullptr, "Reject duplicate attach of same component");

    ObjectHandle Handle = ObjectA->GetObjectHandle();
    Expect(WorldA->FindByHandle(Handle) == ObjectA, "FindByHandle live");
    WorldA->DestroyGameObject(ObjectA);
    Expect(WorldA->FindByHandle(Handle) == nullptr, "Stale handle resolves null");

    WorldA->QueueDestroyGameObject(Parent);
    WorldA->QueueDestroyGameObject(Child);
    WorldA->FlushPendingDestroys();
    Expect(WorldA->GetObjectCount() == 0, "Deferred destroy flushed");

    Memory.DestroyObject(WorldA);
    Memory.DestroyObject(WorldB);
    Memory.Deinitialize();
}
void TestWorldTransformsAndExtractor()
{
    std::cout << "\n=== World transforms for mesh/camera/light ===\n";
    MemorySubsystem Memory;
    Memory.Initialize();

    Scene* World = Memory.NewObject<Scene>("TransformWorld");
    GameObject* Pivot = World->CreateGameObject("Pivot");
    Pivot->GetTransform().Position = Vector3(10.f, 0.f, 0.f);
    Pivot->GetTransform().Rotation = Quaternion::CreateFromAxisAngle(Vector3::UnitY, 3.14159265f * 0.5f);

    GameObject* CameraObject = World->CreateGameObject("Camera");
    CameraObject->GetTransform().Position = Vector3(0.f, 0.f, -2.f);
    Expect(CameraObject->SetParent(Pivot), "Camera under pivot");
    CameraComponent* Camera = CameraObject->AddComponent<CameraComponent>();
    Camera->bPrimary = true;

    GameObject* LightObject = World->CreateGameObject("Light");
    LightObject->GetTransform().Position = Vector3(0.f, 1.f, 0.f);
    Expect(LightObject->SetParent(Pivot), "Light under pivot");
    LightObject->AddComponent<LightComponent>();

    GameObject* MeshObject = World->CreateGameObject("Mesh");
    MeshObject->GetTransform().Position = Vector3(0.f, 0.f, 0.f);
    Expect(MeshObject->SetParent(Pivot), "Mesh under pivot");
    MeshObject->AddComponent<MeshRendererComponent>();

    const Vector3 CameraWorld = CameraObject->GetWorldPosition();
    const Vector3 PivotPosition = Pivot->GetTransform().Position;
    const float Distance = (CameraWorld - PivotPosition).Length();
    Expect(NearlyEqual(Distance, 2.f, 0.05f), "Camera world position reflects parent");

    SceneExtractor Extractor;
    RenderScene Rendered;
    Extractor.Extract(*World, Rendered);
    Expect(Rendered.Camera.bValid, "Extractor camera valid");
    Expect(NearlyEqual(Rendered.Camera.Position.x, CameraWorld.x)
            && NearlyEqual(Rendered.Camera.Position.y, CameraWorld.y)
            && NearlyEqual(Rendered.Camera.Position.z, CameraWorld.z),
        "Extractor camera uses world position");
    Expect(!Rendered.Lights.empty(), "Extractor light present");
    Expect(NearlyEqual(Rendered.Lights[0].Position.x, LightObject->GetWorldPosition().x), "Light uses world position");
    Expect(!Rendered.Objects.empty(), "Mesh extracted");

    Pivot->SetActive(false);
    Extractor.Extract(*World, Rendered);
    Expect(!Rendered.Camera.bValid && Rendered.Lights.empty() && Rendered.Objects.empty(),
        "Inactive ancestor hides camera/light/mesh");

    Memory.DestroyObject(World);
    Memory.Deinitialize();
}
}

int main()
{
    SetCurrentThreadRole(ThreadRole::Game);
    SetCurrentThreadDebugName("Game Thread");

    TestParentChildOrders();
    TestForeignDestroyCycleAndDuplicate();
    TestWorldTransformsAndExtractor();

    std::cout << "\nFailures: " << FailureCount << '\n';
    return FailureCount == 0 ? 0 : 1;
}
