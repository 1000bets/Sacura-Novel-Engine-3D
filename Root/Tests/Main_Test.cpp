#include "Engine.h"
#include "Editor.h"
#include "Game/Scene.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/CameraComponent.h"
#include "Gameplay/LightComponent.h"
#include "Gameplay/MeshRendererComponent.h"
#include "Core/MemorySubsystem.h"
#include "Core/Threading/JobSystem.h"
#include "Core/Threading/RenderThread.h"
#include "Core/Threading/ThreadContext.h"
#include "Platform/WindowSubsystem.h"
#include "Rendering/SceneExtractor.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

static void TestThreadingFoundation(Engine& Eng)
{
    std::cout << "\n=== Threading Foundation ===\n";
    assert(IsGameThread());
    assert(Eng.GetRenderThread().IsReady());

    std::atomic<bool> bRenderCommandRan{false};
    Eng.GetRenderThread().Enqueue([&]()
    {
        AssertRenderThread();
        bRenderCommandRan.store(true, std::memory_order_release);
    });

    for (int Attempt = 0; Attempt < 200 && !bRenderCommandRan.load(); ++Attempt)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    assert(bRenderCommandRan.load());
    std::cout << "  Render command OK\n";
}

static void TestJobSystem(Engine& Eng)
{
    std::cout << "\n=== Job System ===\n";
    std::atomic<int> Counter{0};
    JobGroup Group;
    for (int Index = 0; Index < 100; ++Index)
    {
        Group.Add([&]() { Counter.fetch_add(1, std::memory_order_relaxed); });
    }
    Group.Wait();
    assert(Counter.load() == 100);
    std::cout << "  JobGroup OK\n";
}

static void TestRendering(Engine& Eng)
{
    std::cout << "\n=== Rendering ===\n";
    assert(Eng.GetWindowSubsystem() != nullptr);
    assert(Eng.GetRenderThread().GetDefaultMesh().IsValid());

    auto* Mem = MemorySubsystem::Get();
    Scene* SceneObject = Mem->NewObject<Scene>("RenderScene");

    GameObject* CameraObject = SceneObject->CreateGameObject("Camera");
    CameraObject->GetTransform().Position = Vector3(0.f, 0.f, -2.f);
    CameraComponent* Camera = CameraObject->AddComponent<CameraComponent>();
    Camera->bPrimary = true;
    Camera->FieldOfViewDegrees = 60.f;
    Camera->AspectRatio = 1280.f / 720.f;

    GameObject* QuadObject = SceneObject->CreateGameObject("Quad");
    QuadObject->GetTransform().Position = Vector3(0.f, 0.f, 0.f);
    MeshRendererComponent* MeshRenderer = QuadObject->AddComponent<MeshRendererComponent>();
    MeshRenderer->Mesh = Eng.GetRenderThread().GetDefaultMesh();

    Eng.SetActiveScene(SceneObject);

    for (int Frame = 0; Frame < 30; ++Frame)
    {
        Eng.Tick(1.f / 60.f);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    Eng.SetActiveScene(nullptr);
    Mem->DestroyObject(SceneObject);
    std::cout << "  Rendered 30 frames with default quad\n";
}

int main()
{
    std::cout << "=== Engine ===\n";
    Engine Eng;

    std::filesystem::path ShaderPath = std::filesystem::current_path() / "shaders";
    if (!std::filesystem::exists(ShaderPath))
    {
        ShaderPath = std::filesystem::path("M:/Sacura-Novel-Engine-3D/Root/Engine/shaders");
    }
    Eng.SetShaderDirectory(ShaderPath.string());
    Eng.SetGraphicsBackend(GraphicsBackend::Auto);
    Eng.Initialize();

    TestThreadingFoundation(Eng);
    TestJobSystem(Eng);
    TestRendering(Eng);

    std::cout << "\n=== Editor (stub) ===\n";
    Editor Ed;
    Ed.SetEngine(&Eng);
    Ed.Initialize();
    Ed.Shutdown();

    Eng.Shutdown();
    std::cout << "\nAll tests passed.\n";
    return 0;
}
