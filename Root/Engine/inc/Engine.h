#pragma once

#include "ISystem.h"
#include "Core/Threading/JobSystem.h"
#include "Core/Threading/RenderThread.h"
#include "Platform/GraphicsBackend.h"
#include "Rendering/SceneExtractor.h"

#include <cstdint>
#include <string>

class Scene;
class WindowSubsystem;

class Engine : public ISystem
{
public:
    Engine();
    ~Engine() override;

    void Initialize() override;
    void Tick(float DeltaTime) override;
    void Shutdown() override;

    void Run();
    void RequestShutdown();

    bool IsRunning() const { return bRunning; }

    JobSystem& GetJobSystem() { return Jobs; }
    RenderThread& GetRenderThread() { return Render; }
    SceneExtractor& GetSceneExtractor() { return Extractor; }
    WindowSubsystem* GetWindowSubsystem() const;

    void SetActiveScene(Scene* Scene);
    Scene* GetActiveScene() const { return ActiveScene; }

    void SetGraphicsBackend(GraphicsBackend Backend) { PreferredBackend = Backend; }
    void SetShaderDirectory(const std::string& Directory) { ShaderDirectory = Directory; }

private:
    void BeginFrame();
    void EndFrame();

    bool bRunning = false;
    bool bInitialized = false;
    uint64_t NextFrameIndex = 1;
    GraphicsBackend PreferredBackend = GraphicsBackend::Auto;
    std::string ShaderDirectory = "shaders";

    JobSystem Jobs;
    RenderThread Render;
    SceneExtractor Extractor;
    Scene* ActiveScene = nullptr;
};
