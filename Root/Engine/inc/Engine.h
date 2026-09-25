#pragma once

#include "ISystem.h"
#include "Assets/AssetGpuUploader.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetRegistry.h"
#include "Assets/SceneAssetResolver.h"
#include "Core/Threading/JobSystem.h"
#include "Core/Threading/RenderThread.h"
#include "Platform/GraphicsBackend.h"
#include "Platform/NativeWindowInfo.h"
#include "Project/ProjectDescriptor.h"
#include "Game/PlaySession.h"
#include "Rendering/SceneExtractor.h"
#include "Rendering/ImGuiOverlaySnapshot.h"
#include "Scripting/ScriptingSubsystem.h"
#include "Story/StoryRuntime.h"
#include <memory>

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
    AssetGpuUploader& GetAssetGpuUploader() { return GpuUploader; }
    SceneAssetResolver& GetSceneAssetResolver() { return AssetResolver; }
    ScriptingSubsystem& GetScripting() { return Scripting; }
    WindowSubsystem* GetWindowSubsystem() const;

    void SetContentRoot(const std::filesystem::path& ContentRoot);
    const std::filesystem::path& GetContentRoot() const { return ContentRoot; }

    void SetScriptsRoot(const std::filesystem::path& InScriptsRoot);
    const std::filesystem::path& GetScriptsRoot() const { return ScriptsRoot; }

    AssetDiagnostic LoadProjectContent(const ProjectDescriptor& Descriptor);
    void UnloadProjectContent();

    bool StartPresenting(const NativeWindowInfo& WindowInfo);
    bool IsPresenting() const;
    void StopPresenting();

    void AdoptScene(std::unique_ptr<Scene> NewScene);
    Scene* GetActiveScene() const;
    Scene* GetEditScene() const { return OwnedScene.get(); }
    bool StartGame();
    void StopGame();
    StoryRuntime& GetStoryRuntime() { return Story; }
    void SetEditorRenderCamera(const RenderCamera& Camera);
    void UseGameRenderCamera();
    void ResizePresentation(uint32_t Width, uint32_t Height);
    bool RegisterRenderSurface(RenderSurfaceId Surface, const NativeWindowInfo& WindowInfo);
    void UnregisterRenderSurface(RenderSurfaceId Surface);
    void ResizeRenderSurface(RenderSurfaceId Surface, uint32_t Width, uint32_t Height);
    void ConfigureRenderSurface(RenderSurfaceId Surface, bool bEditScene, const RenderCamera& Camera, const RenderSettings& Settings = {});
    void SetRenderSurfaceImGuiOverlay(RenderSurfaceId Surface, ImGuiOverlaySnapshot Overlay);
    RenderStatistics GetRenderStatistics(RenderSurfaceId Surface) const { return Render.GetStatistics(Surface); }
    bool IsInitialized() const { return bInitialized; }
    bool IsGameRunning() const { return bGameRunning; }

    PlaySession& GetPlaySession() { return Play; }
    const PlaySession& GetPlaySession() const { return Play; }

    void SetGraphicsBackend(GraphicsBackend Backend) { PreferredBackend = Backend; }
    void SetShaderDirectory(const std::string& Directory) { ShaderDirectory = Directory; }

private:
    friend class PlaySession;
    void BeginStory(Scene* World);
    void TickWorld(Scene& World, float DeltaTime);
    void BeginFrame();
    void EndFrame();
    void InitializeCommon(bool bCreateWindowAndRender);
    void ResolveShaderDirectory();
    void ScanConfiguredContent();
    void ImportProjectScripts();

    bool bRunning = false;
    bool bInitialized = false;
    bool bHeadless = false;
    uint64_t NextFrameIndex = 1;
    GraphicsBackend PreferredBackend = GraphicsBackend::Auto;
    std::string ShaderDirectory;
    std::filesystem::path ContentRoot;
    std::filesystem::path ScriptsRoot;

    JobSystem Jobs;
    RenderThread Render;
    SceneExtractor Extractor;
    AssetRegistry Registry;
    AssetManager Assets;
    AssetGpuUploader GpuUploader;
    SceneAssetResolver AssetResolver;
    ScriptingSubsystem& Scripting;
    PlaySession Play;
    std::unique_ptr<Scene> OwnedScene;
    StoryRuntime Story;
    std::filesystem::path StartupStory;
    bool bGameRunning = false;
    bool bEditorRenderCamera = false;
    RenderCamera EditorRenderCamera;
    uint32_t PresentationWidth = 1;
    uint32_t PresentationHeight = 1;
    struct PresentationState
    {
        NativeWindowInfo Window;
        RenderCamera Camera;
        RenderSettings Settings;
        ImGuiOverlaySnapshot Overlay;
        bool bEditScene = false;
    };
    std::unordered_map<uint32_t, PresentationState> Presentations;
};
