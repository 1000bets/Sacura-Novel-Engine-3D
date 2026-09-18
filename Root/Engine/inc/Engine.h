#pragma once

#include "ISystem.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetRegistry.h"
#include "Core/Threading/JobSystem.h"
#include "Core/Threading/RenderThread.h"
#include "Platform/GraphicsBackend.h"
#include "Rendering/SceneExtractor.h"

#include <cstdint>
#include <filesystem>
#include <string>

class Scene;
class WindowSubsystem;

class Engine : public ISystem
{
public:
    Engine();
    ~Engine() override;

    void Initialize() override;
    void InitializeHeadless(const std::filesystem::path& ContentRoot);
    void Tick(float DeltaTime) override;
    void Shutdown() override;

    void Run();
    void RequestShutdown();

    bool IsRunning() const { return bRunning; }
    bool IsHeadless() const { return bHeadless; }

    JobSystem& GetJobSystem() { return Jobs; }
    RenderThread& GetRenderThread() { return Render; }
    SceneExtractor& GetSceneExtractor() { return Extractor; }
    AssetRegistry& GetAssetRegistry() { return Registry; }
    AssetManager& GetAssetManager() { return Assets; }
    WindowSubsystem* GetWindowSubsystem() const;

    void SetContentRoot(const std::filesystem::path& ContentRoot);
    const std::filesystem::path& GetContentRoot() const { return ContentRoot; }

    void SetActiveScene(Scene* Scene);
    Scene* GetActiveScene() const { return ActiveScene; }

    void SetGraphicsBackend(GraphicsBackend Backend) { PreferredBackend = Backend; }
    void SetShaderDirectory(const std::string& Directory) { ShaderDirectory = Directory; }

private:
    void BeginFrame();
    void EndFrame();
    void InitializeCommon(bool bCreateWindowAndRender);

    bool bRunning = false;
    bool bInitialized = false;
    bool bHeadless = false;
    uint64_t NextFrameIndex = 1;
    GraphicsBackend PreferredBackend = GraphicsBackend::Auto;
    std::string ShaderDirectory = "shaders";
    std::filesystem::path ContentRoot;

    JobSystem Jobs;
    RenderThread Render;
    SceneExtractor Extractor;
    AssetRegistry Registry;
    AssetManager Assets;
    Scene* ActiveScene = nullptr;
};
