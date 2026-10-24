#include "Engine.h"
#include "Core/Threading/ThreadContext.h"
#include "Story/StoryDocumentIO.h"
#include "Story/StoryEvents.h"
#include "World/Scene.h"
#include "World/Serialization/SceneSerializer.h"

bool Engine::RequestScene(const std::string& ScenePath, std::string& Error, const std::string& StoryPath)
{
    AssertGameThread();
    if (!PlayerInput || !GetActiveScene() || !GetActiveScene()->IsPlaying())
    {
        Error = "Scene transitions require a game session";
        return false;
    }
    if (IsSceneTransitionPending())
    {
        Error = "A scene transition is already pending";
        return false;
    }
    AssetRegistryEntry SceneAsset;
    if (!Registry.TryGetByPath(ScenePath, SceneAsset) || SceneAsset.Metadata.Type != SceneAssetType)
    {
        Error = "Scene asset not found: " + ScenePath;
        return false;
    }
    AssetRegistryEntry StoryAsset;
    if (!StoryPath.empty()
        && (!Registry.TryGetByPath(StoryPath, StoryAsset) || StoryAsset.Metadata.Type != StoryAssetType))
    {
        Error = "Story asset not found: " + StoryPath;
        return false;
    }
    RequestedScenePath = std::filesystem::u8path(SceneAsset.AbsolutePath);
    if (!StoryPath.empty())
    {
        RequestedStoryPath = std::filesystem::u8path(StoryAsset.AbsolutePath);
    }
    SceneTransitionError.clear();
    Error.clear();
    return true;
}

void Engine::FailSceneTransition(const std::string& Error)
{
    SceneTransitionError = Error;
    RequestedScene.reset();
    RequestedScenePath.clear();
    RequestedStoryPath.clear();
    RequestedSceneStory = {};
    PrintString("Scene transition failed: " + SceneTransitionError);
}

void Engine::ProcessSceneTransition()
{
    AssertGameThread();
    if (!IsSceneTransitionPending())
    {
        return;
    }
    if (!RequestedScene)
    {
        Scene* LoadedScene = nullptr;
        const auto Loaded = SceneSerializer::DeserializeFromFile(RequestedScenePath, LoadedScene);
        RequestedScene.reset(LoadedScene);
        if (!Loaded.bOk || !RequestedScene)
        {
            FailSceneTransition(Loaded.Error);
            return;
        }
        if (!RequestedStoryPath.empty())
        {
            const auto LoadedStory = StoryDocumentIO::LoadFromFile(RequestedStoryPath, RequestedSceneStory);
            if (!LoadedStory.bOk)
            {
                FailSceneTransition(LoadedStory.Error);
                return;
            }
            std::string Error;
            if (!ValidateStoryEvents(RequestedSceneStory, Error)
                || !ValidateNarrativeDocument(RequestedSceneStory, Error))
            {
                FailSceneTransition(Error);
                return;
            }
        }
        RequestedScene->SetAuthoringOperations(AuthoringOperations);
        if (!PrepareSceneSimulation(*RequestedScene))
        {
            FailSceneTransition("Scene simulation preparation failed");
            return;
        }
    }
    if (!PrepareSceneMaterials(*RequestedScene))
    {
        return;
    }
    auto* ActiveWorld = &OwnedScene;
    if (Play.IsSimulating())
    {
        ActiveWorld = &Play.PlayWorld;
    }
    auto PreviousScene = std::move(*ActiveWorld);
    *ActiveWorld = std::move(RequestedScene);
    const auto PreviousContexts = PlayerInput->GetContexts();
    PlayerInput->SetContexts(PlayerInput->GetConfiguration().StartupContexts);
    if (!(*ActiveWorld)->BeginPlay())
    {
        RequestedScene = std::move(*ActiveWorld);
        *ActiveWorld = std::move(PreviousScene);
        PlayerInput->SetContexts(PreviousContexts);
        FailSceneTransition("Scene simulation startup failed");
        return;
    }
    Story.BindScene(nullptr);
    PreviousScene->EndPlay();
    PreviousScene.reset();
    Story.BindScene(ActiveWorld->get());
    auto NextStory = std::move(RequestedSceneStory);
    RequestedScenePath.clear();
    RequestedStoryPath.clear();
    RequestedSceneStory = {};
    MaterialTime = 0.f;
    if (!Story.LoadDocument(NextStory))
    {
        SceneTransitionError = Story.GetLastError();
        Story.LoadDocument({});
        PrintString("Scene story startup failed: " + SceneTransitionError);
    }
    PrintString("Scene transition completed");
}
