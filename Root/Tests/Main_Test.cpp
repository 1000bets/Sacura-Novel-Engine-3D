#include "Engine.h"
#include "Editor.h"
#include "Game/Scene.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/Component.h"
#include "Core/MemorySubsystem.h"

#include <cassert>
#include <iostream>

// ---- Пользовательский компонент ----
class MeshComponent : public Component
{
    SAKURA_OBJECT(MeshComponent)
public:
    void OnCreate()  override { std::cout << "  MeshComponent::OnCreate\n"; }
    void OnDestroy() override { std::cout << "  MeshComponent::OnDestroy\n"; }
    std::string MeshPath;
protected:
    MeshComponent() = default;
};

int main()
{
    // ---- Engine ----
    std::cout << "=== Engine ===\n";
    Engine Eng;
    Eng.Initialize();

    auto* Mem = MemorySubsystem::Get();
    assert(Mem && "MemorySubsystem should be alive");
    assert(Mem->GetAliveCount() == 0);

    // ---- Scene + GameObjects ----
    std::cout << "\n=== Scene ===\n";
    auto* MyScene = Mem->NewObject<Scene>("TestScene");
    auto* Root    = MyScene->CreateGameObject("Root");
    auto* Child   = MyScene->CreateGameObject("Child");
    Child->SetParent(Root);

    assert(MyScene->GetObjectCount() == 2);
    assert(MyScene->GetRootObjects().size() == 1);
    assert(Mem->GetAliveCount() == 3); // Scene + 2 GO

    // ---- SimpleMath Transform ----
    std::cout << "\n=== Transform (SimpleMath) ===\n";
    Root->GetTransform().Position = Vector3(5.f, 0.f, 10.f);
    Root->GetTransform().Rotation = Quaternion::CreateFromYawPitchRoll(
        3.14159f / 2.f, 0.f, 0.f);

    Vector3 Fwd = Root->GetTransform().GetForward();
    Matrix  M   = Root->GetTransform().GetMatrix();
    std::cout << "  Pos: " << Root->GetTransform().Position.x
              << ", " << Root->GetTransform().Position.y
              << ", " << Root->GetTransform().Position.z << "\n";
    std::cout << "  Fwd: " << Fwd.x << ", " << Fwd.y << ", " << Fwd.z << "\n";
    (void)M;

    // ---- Component ----
    std::cout << "\n=== Component ===\n";
    auto* Mesh = Root->AddComponent<MeshComponent>();
    Mesh->MeshPath = "models/hero.glb";
    assert(Root->GetComponent<MeshComponent>() == Mesh);
    assert(Mem->GetAliveCount() == 4);

    // Component проксирует Transform от GO
    assert(Mesh->GetTransform().Position.x == 5.f);

    // ---- FindByID ----
    assert(Mem->FindByID(Mesh->GetID()) == Mesh);
    assert(Mem->FindByID<MeshComponent>(Mesh->GetID()) == Mesh);

    // ---- Destroy ----
    std::cout << "\n=== Destroy ===\n";
    MyScene->DestroyGameObject(Root); // Root + Child + MeshComponent
    assert(Mem->GetAliveCount() == 1);

    Mem->DestroyObject(MyScene);
    assert(Mem->GetAliveCount() == 0);

    // ---- Editor (stub) ----
    std::cout << "\n=== Editor ===\n";
    Editor Ed;
    Ed.SetEngine(&Eng);
    Ed.Initialize();
    assert(Ed.GetEngine() == &Eng);
    Ed.Shutdown();

    // ---- Shutdown ----
    std::cout << "\n=== Shutdown ===\n";
    Eng.Shutdown();

    std::cout << "\nAll tests passed.\n";
    return 0;
}
