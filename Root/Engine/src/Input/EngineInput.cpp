#include "Engine.h"
#include "UI/WidgetRuntime.h"
#include "Project/ProjectDescriptor.h"
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
    ProjectDescriptor Descriptor;
    if (!OpenProjectFile.empty() && !ProjectDescriptor::TryLoadFromFile(OpenProjectFile, Descriptor, Error))
    {
        PrintString("Input project settings failed: " + Error);
        return false;
    }
    std::map<std::string, std::string> Actions{{"advance", "engine.input.advance"}, {"pause", "engine.input.pause"}};
    for (const auto& Entry : Descriptor.NarrativeInputActions)
    {
        Actions[Entry.first] = Entry.second;
    }
    const auto SystemAction = [this, Actions](const InputActionEvent& Event)
    {
        if (Event.Phase == InputPhase::Triggered)
        {
            if (Event.Action == Actions.at("advance") && !bGamePaused
                && Play.GetState() != PlaySessionState::Paused)
            {
                WidgetRuntime::NarrativeAction("advance");
            }
            else if (Event.Action == Actions.at("pause"))
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
    for (const auto& Entry : Actions)
    {
        if (Entry.second.empty())
        {
            continue;
        }
        if (Session->BindAction(Entry.second, InputPhase::Triggered,
            [SystemAction, Entry](const InputActionEvent& Event)
            {
                if (Entry.first == "advance" || Entry.first == "pause")
                {
                    SystemAction(Event);
                }
                else
                {
                    WidgetRuntime::NarrativeAction(Entry.first);
                }
            }) == 0)
        {
            PrintString("Narrative input action is unavailable: " + Entry.second);
            return false;
        }
    }
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
    if (!WidgetRuntime::Start(*this, OpenProjectFile, Error))
    {
        PrintString("Widget session failed: " + Error);
        return false;
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
