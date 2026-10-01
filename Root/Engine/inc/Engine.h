#pragma once

#include "ISystem.h"
#include "Rendering/Assets/AssetGpuUploader.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetRegistry.h"
#include "World/Resources/SceneAssetResolver.h"
#include "Core/Threading/JobSystem.h"
#include "Rendering/Threading/RenderThread.h"
#include "Platform/GraphicsBackend.h"
#include "Platform/NativeWindowInfo.h"
#include "Project/ProjectDescriptor.h"
#include "World/Simulation/PlaySession.h"
#include "Rendering/PresentationController.h"
#include "Rendering/ImGuiOverlaySnapshot.h"
#include "Scripting/ScriptingSubsystem.h"
#include "Story/StoryRuntime.h"
#include <memory>
#include "Materials/MaterialWidgetBinding.h"

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
    SceneExtractor& GetSceneExtractor() { return Presentation.GetSceneExtractor(); }
    AssetRegistry& GetAssetRegistry() { return Registry; }
    AssetManager& GetAssetManager() { return Assets; }
    AssetGpuUploader& GetAssetGpuUploader() { return GpuUploader; }
    SceneAssetResolver& GetSceneAssetResolver() { return AssetResolver; }
    std::shared_ptr<DynamicMaterialInstance> CreateDynamicMaterialInstance(const AssetKey& Material);
    ScriptingSubsystem& GetScripting() { return Scripting; }
    WindowSubsystem* GetWindowSubsystem() const;

    void SetContentRoot(const std::filesystem::path& ContentRoot);
    const std::filesystem::path& GetContentRoot() const { return ContentRoot; }

    void SetScriptsRoot(const std::filesystem::path& InScriptsRoot);
    const std::filesystem::path& GetScriptsRoot() const { return ScriptsRoot; }

    AssetDiagnostic LoadProjectContent(const ProjectDescriptor& Descriptor);
    const std::vector<AssetDiagnostic>& GetProjectDiagnostics() const { return ProjectDiagnostics; }
    const std::string& GetInitializationError() const { return InitializationError; }
    void UnloadProjectContent();

    bool StartPresenting(const NativeWindowInfo& WindowInfo);
    bool IsPresenting() const;
    void StopPresenting();

    void AdoptScene(std::unique_ptr<Scene> NewScene);
    Scene* GetActiveScene() const;
    Scene* GetEditScene() const { return OwnedScene.get(); }
    bool StartGame();
    bool PrepareSceneMaterials(Scene& World);
    void RegisterMaterialWidgetBinding(const std::shared_ptr<MaterialWidgetBinding>& Binding);
    float GetMaterialTime() const { return MaterialTime; }
    void StopGame();
    StoryRuntime& GetStoryRuntime() { return Story; }
    void SetStartupStory(const std::filesystem::path& StoryPath) { StartupStory = StoryPath; }
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
    ReflectionDiagnostic ImportProjectScripts();

    bool bRunning = false;
    bool bInitialized = false;
    bool bHeadless = false;
    uint64_t NextFrameIndex = 1;
    GraphicsBackend PreferredBackend = GraphicsBackend::Auto;
    std::string ShaderDirectory;
    std::filesystem::path ContentRoot;
    std::filesystem::path ScriptsRoot;
    std::vector<AssetDiagnostic> ProjectDiagnostics;
    std::string InitializationError;

    JobSystem Jobs;
    RenderThread Render;
    PresentationController Presentation;
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
    float MaterialTime = 0.f;
    std::vector<std::weak_ptr<MaterialWidgetBinding>> MaterialWidgetBindings;
};
