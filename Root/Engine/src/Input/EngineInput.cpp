#include "Engine.h"
#include "Input/InputDocument.h"
#include "Core/Threading/ThreadContext.h"
#include "World/Scene.h"

bool Engine::BeginInputSession()
{
    AssertGameThread();
    InputConfiguration Configuration;
    std::string Error;
    if (!InputDocument::LoadConfiguration(Registry, Configuration, Error, OpenProjectFile))
    {
        PrintString("Input configuration failed: " + Error);
        return false;
    }
    auto Session = std::make_shared<InputSession>();
    if (!Session->Configure(std::move(Configuration), Error))
    {
        PrintString("Input session failed: " + Error);
        return false;
    }
    const auto SystemAction = [this](const InputActionEvent& Event)
    {
        if (Event.Phase == InputPhase::Triggered)
        {
            if (Event.Action == "engine.input.advance" && !bGamePaused
                && Play.GetState() != PlaySessionState::Paused)
            {
                Story.AdvanceDialogue();
            }
            else if (Event.Action == "engine.input.pause")
            {
                if (Play.GetState() == PlaySessionState::Playing)
                {
                    Play.Pause();
                }
                else if (Play.GetState() == PlaySessionState::Paused)
                {
                    Play.Resume();
                }
                else
                {
                    SetGamePaused(!bGamePaused);
                }
            }
        }
    };
    Session->BindAction("engine.input.advance", InputPhase::Triggered, SystemAction);
    Session->BindAction("engine.input.pause", InputPhase::Triggered, SystemAction);
    if (!OpenProjectFile.empty())
    {
        nlohmann::json Document;
        if (!InputDocument::ReadProjectInput(OpenProjectFile, Document, Error))
        {
            PrintString("Input story bindings failed: " + Error);
            return false;
        }
        try
        {
            for (const auto& Binding : Document.value("StoryBindings", std::vector<InputStoryBinding>{}))
            {
                if (Binding.Event.empty() || static_cast<int>(Binding.Phase) < 0 || static_cast<int>(Binding.Phase) > 4
                    || Session->BindAction(Binding.Action, Binding.Phase, [this, Binding](const InputActionEvent&)
                    {
                        if (!Story.TriggerEvent(Binding.Event))
                        {
                            PrintString("Input story event could not start: " + Binding.Event);
                        }
                    }) == 0)
                {
                    PrintString("Invalid input story binding");
                    return false;
                }
            }
        }
        catch (const std::exception& Exception)
        {
            PrintString(std::string("Invalid input story bindings: ") + Exception.what());
            return false;
        }
    }
    PlayerInput = std::move(Session);
    bGamePaused = false;
    return true;
}

void Engine::SetGamePaused(bool bPaused)
{
    AssertGameThread();
    bGamePaused = bPaused;
    if (PlayerInput)
    {
        PlayerInput->SetPaused(bPaused);
    }
}
