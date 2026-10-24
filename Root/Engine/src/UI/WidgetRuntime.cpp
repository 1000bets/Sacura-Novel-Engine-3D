#include "UI/WidgetRuntime.h"
#include "UI/WidgetDefinition.h"
#include "Scripting/PythonRuntimeInternal.h"
#include "Engine.h"
#include "Project/ProjectDescriptor.h"
#include "World/Scene.h"
#include "World/GameObject.h"
#include "World/Components/ScriptComponent.h"
#include "Core/Threading/ThreadContext.h"
#include "Core/EnginePaths.h"
#include "Assets/AssetRegistry.h"
#include <fstream>
#include <iterator>
#include <map>
#include <algorithm>
#include <set>

using Json = nlohmann::json;

static Engine* WidgetEngine = nullptr;
static Json PublishedViews = Json::array();
static Json PublishedWidgets = Json::array();
static std::map<std::string, std::string> ProjectWidgets;
static std::string ProjectWidgetTitle;
static bool bFirstWidgetUpdate = false;
static bool bMainMenuVisible = false;
static uint64_t NextWidgetInstance = 1;
static size_t SelectedChoice = 0;

#if defined(SAKURA_ENABLE_PYTHON)
struct WidgetControllerType
{
    py::object Class;
    std::string Kind;
    std::string Module;
    Json Fields;
    std::string SourcePath;
};

struct WidgetInstance
{
    std::string Identifier;
    std::string Asset;
    std::string Controller;
    Json Layout;
    Json Context;
    py::object Object;
    bool bFailed = false;
};

static std::map<std::string, WidgetControllerType> WidgetControllers;
static std::map<std::string, WidgetInstance> WidgetInstances;

static Json FromPython(const py::object& Value)
{
    const auto Text = py::module_::import("json").attr("dumps")(Value, py::arg("allow_nan") = false).cast<std::string>();
    if (Text.size() > 65536)
    {
        throw std::runtime_error("Widget fields exceed 64 KiB");
    }
    return Json::parse(Text);
}

static py::object ToPython(const Json& Value)
{
    return py::module_::import("json").attr("loads")(Value.dump());
}

static void SetRuntimePaused(bool bPaused)
{
    if (WidgetEngine->GetPlaySession().IsSimulating())
    {
        if (bPaused)
        {
            WidgetEngine->GetPlaySession().Pause();
        }
        else
        {
            WidgetEngine->GetPlaySession().Resume();
        }
    }
    else
    {
        WidgetEngine->SetGamePaused(bPaused);
    }
}

static void InvokeWidgetGameAction(const Json& Context, const std::string& Action)
{
    const auto View = Context.value("GameView", Json::object());
    const bool bPaused = Context.value("Paused", false);
    bool bAllowed = false;
    for (const auto& Entry : View.value("actions", Json::array()))
    {
        if (Entry.at("id") == Action && Entry.value("enabled", true)
            && (!bPaused || Entry.value("allowWhilePaused", false)))
        {
            bAllowed = true;
        }
    }
    if (!bAllowed)
    {
        throw std::runtime_error("Widget game action is not published or enabled: " + Action);
    }
    for (auto* Object : WidgetEngine->GetActiveScene()->GetAllObjects())
    {
        for (auto* Script : Object->GetComponents<ScriptComponent>())
        {
            if (Script->GetPersistentId() == View.value("componentId", "") && Script->IsPlaying())
            {
                ReflectedValue Result;
                const auto Invoked = ScriptingSubsystem::Get().InvokeMethod(*Script, "ExecuteGameAction",
                    {{"Action", ReflectedValue::MakeString(Action)}}, Result);
                if (!Invoked.bOk)
                {
                    throw std::runtime_error(Invoked.Message);
                }
                return;
            }
        }
    }
    throw std::runtime_error("Widget game action owner is unavailable");
}

void RegisterWidgetBindings(py::module_& Module)
{
    Module.def("_register_widget", [](const py::object& Class, const std::string& Identifier,
        const std::string& Kind, const py::dict& Fields, const std::string& SourcePath)
    {
        if (Identifier.empty() || WidgetControllers.count(Identifier)
            || std::set<std::string>{"dialogue", "hud", "mainMenu", "pauseMenu"}.count(Kind) == 0)
        {
            throw py::value_error("Invalid or duplicate widget controller: " + Identifier);
        }
        WidgetControllers.emplace(Identifier, WidgetControllerType{Class, Kind,
            Class.attr("__module__").cast<std::string>(), FromPython(Fields), SourcePath});
    });
    Module.def("_widget_command", [](const std::string& Identifier, const std::string& Command, const py::object& Value)
    {
        AssertGameThread();
        auto Found = std::find_if(WidgetInstances.begin(), WidgetInstances.end(),
            [&](const auto& Entry) { return Entry.second.Identifier == Identifier; });
        if (!WidgetEngine || Found == WidgetInstances.end() || Found->second.bFailed)
        {
            throw py::value_error("Widget instance is no longer active");
        }
        auto& Story = WidgetEngine->GetStoryRuntime();
        const auto Argument = FromPython(Value);
        if (Command == "Advance")
        {
            if (!Found->second.Context.value("Paused", false))
            {
                Story.AdvanceDialogue();
            }
        }
        else if (Command == "Choose")
        {
            if (!Found->second.Context.value("Paused", false))
            {
                Story.SelectChoice(Argument.get<size_t>());
            }
        }
        else if (Command == "Pause")
        {
            SetRuntimePaused(true);
        }
        else if (Command == "Resume" || Command == "Start")
        {
            bMainMenuVisible = false;
            SetRuntimePaused(false);
        }
        else if (Command == "MainMenu")
        {
            bMainMenuVisible = true;
            SetRuntimePaused(true);
        }
        else if (Command == "GameAction")
        {
            InvokeWidgetGameAction(Found->second.Context, Argument.get<std::string>());
        }
        else
        {
            throw py::value_error("Unknown widget command: " + Command);
        }
    });
    const auto SourcePath = EnginePaths::Content() / "Scripts" / "widget_controllers.py";
    std::ifstream SourceFile(SourcePath, std::ios::binary);
    if (!SourceFile)
    {
        throw std::runtime_error("Cannot open engine widget controller source: " + SourcePath.string());
    }
    const std::string Source((std::istreambuf_iterator<char>(SourceFile)), std::istreambuf_iterator<char>());
    const auto Builtins = py::module_::import("builtins");
    const auto Code = Builtins.attr("compile")(Source, SourcePath.string(), "exec");
    Builtins.attr("exec")(Code, Module.attr("__dict__"));
}

void ClearWidgetBindings()
{
    WidgetRuntime::Stop();
    WidgetControllers.clear();
}
#endif

Json WidgetRuntime::Controllers()
{
    Json Result = Json::array();
#if defined(SAKURA_ENABLE_PYTHON)
    for (const auto& Entry : WidgetControllers)
    {
        Json Controller = {{"typeId", Entry.first}, {"kind", Entry.second.Kind},
            {"module", Entry.second.Module}, {"fields", Entry.second.Fields}, {"sourceAssetId", nullptr}};
        if (auto* Runtime = ScriptingSubsystem::Get().GetMaterialEngine())
        {
            for (const auto& Asset : Runtime->GetAssetRegistry().FindByType(PythonScriptAssetType))
            {
                std::error_code Error;
                if (std::filesystem::equivalent(Asset.AbsolutePath, Entry.second.SourcePath, Error))
                {
                    Controller["sourceAssetId"] = Asset.Metadata.Guid.ToString();
                    Controller["sourcePath"] = Asset.VirtualPath;
                    break;
                }
            }
        }
        Result.push_back(std::move(Controller));
    }
#endif
    return Result;
}

bool WidgetRuntime::ValidateController(const Json& Document, std::string& Error)
{
#if defined(SAKURA_ENABLE_PYTHON)
    const auto Controller = Document.value("controller", "");
    if (Controller.empty())
    {
        return true;
    }
    const auto Found = WidgetControllers.find(Controller);
    if (Found == WidgetControllers.end() || Found->second.Kind != Document.at("widget").at("kind"))
    {
        Error = "Widget controller is unavailable or has an incompatible contract: " + Controller;
        return false;
    }
    return true;
#else
    Error = "Python widgets are unavailable";
    return false;
#endif
}

bool WidgetRuntime::Start(Engine& Runtime, const std::filesystem::path& ProjectFile, std::string& Error)
{
    Stop();
    ProjectDescriptor Descriptor;
    if (!ProjectFile.empty())
    {
        if (!ProjectDescriptor::TryLoadFromFile(ProjectFile, Descriptor, Error))
        {
            return false;
        }
        for (const auto& Assignment : Descriptor.NarrativeWidgets)
        {
            if (Assignment.second.empty())
            {
                continue;
            }
            AssetId Identity;
            AssetRegistryEntry Entry;
            Json Document;
            if (!AssetId::TryParse(Assignment.second, Identity)
                || !Runtime.GetAssetRegistry().TryGetById(Identity, Entry)
                || Entry.Metadata.Type != WidgetAssetType
                || !WidgetDefinition::Read(Entry.AbsolutePath, Document, Error)
                || Document.at("widget").at("kind") != Assignment.first
                || !ValidateController(Document, Error))
            {
                Error = "Invalid project widget " + Assignment.first + ": " + Error;
                return false;
            }
        }
    }
    ProjectWidgets = Descriptor.NarrativeWidgets;
    ProjectWidgetTitle = Descriptor.Name;
    WidgetEngine = &Runtime;
    bFirstWidgetUpdate = true;
    return true;
}

void WidgetRuntime::Stop()
{
#if defined(SAKURA_ENABLE_PYTHON)
    for (auto& Entry : WidgetInstances)
    {
        try
        {
            Entry.second.Object.attr("OnDestroy")();
        }
        catch (const std::exception& Error)
        {
            PrintString(std::string("Widget destruction failed: ") + Error.what());
        }
    }
    WidgetInstances.clear();
#endif
    WidgetEngine = nullptr;
    PublishedWidgets = Json::array();
    PublishedViews = Json::array();
    ProjectWidgets.clear();
    bMainMenuVisible = false;
    SelectedChoice = 0;
}

Json WidgetRuntime::Snapshot()
{
    return PublishedWidgets;
}

Json WidgetRuntime::GameViews()
{
    return PublishedViews;
}

void WidgetRuntime::Update(Engine& Runtime)
{
    AssertGameThread();
    if (WidgetEngine != &Runtime || (!Runtime.IsGameRunning() && !Runtime.GetPlaySession().IsSimulating()))
    {
        return;
    }
#if defined(SAKURA_ENABLE_PYTHON)
    py::gil_scoped_acquire Gil;
    Json Views = Json::array();
    for (auto* Object : Runtime.GetActiveScene()->GetAllObjects())
    {
        for (auto* Script : Object->GetComponents<ScriptComponent>())
        {
            if (!Script->IsPlaying() || Script->IsScriptFailed())
            {
                continue;
            }
            for (const auto& Method : Script->GetClass()->Methods)
            {
                if (Method.Name != "GetGameView")
                {
                    continue;
                }
                ReflectedValue Result;
                const auto Invoked = ScriptingSubsystem::Get().InvokeMethod(*Script, "GetGameView", {}, Result);
                if (!Invoked.bOk || Result.ValueKind != ReflectedValue::Kind::String || Result.StringValue.size() > 65536)
                {
                    continue;
                }
                auto View = Json::parse(Result.StringValue, nullptr, false);
                if (View.is_object())
                {
                    View["objectId"] = Object->GetPersistentId();
                    View["componentId"] = Script->GetPersistentId();
                    Views.push_back(std::move(View));
                }
            }
        }
    }
    PublishedViews = Views;
    if (bFirstWidgetUpdate)
    {
        bFirstWidgetUpdate = false;
        bMainMenuVisible = Runtime.IsGameRunning() && ProjectWidgets.count("mainMenu") && !ProjectWidgets.at("mainMenu").empty();
        if (bMainMenuVisible)
        {
            SetRuntimePaused(true);
        }
    }
    const bool bPaused = Runtime.IsGamePaused() || Runtime.GetPlaySession().GetState() == PlaySessionState::Paused;
    const auto& Story = Runtime.GetStoryRuntime();
    Json Choices = Json::array();
    for (size_t Index = 0; Index < Story.GetChoiceCount(); ++Index)
    {
        Choices.push_back({{"id", std::to_string(Index)}, {"label", Story.GetChoice(Index).Label}});
    }
    std::string Phase = "idle";
    if (Story.GetPlaybackPhase() == StoryPlaybackPhase::Line)
    {
        Phase = "line";
    }
    else if (Story.GetPlaybackPhase() == StoryPlaybackPhase::Choice)
    {
        Phase = "choice";
    }
    Json View = Json::object();
    if (!Views.empty())
    {
        View = Views.front();
    }
    if (View.contains("dialogueChoices"))
    {
        Choices = View.at("dialogueChoices");
    }
    Json Context = {{"Dialogue", {{"Phase", Phase}, {"Speaker", Story.GetDisplayedSpeaker()},
        {"Text", Story.GetDisplayedLine()}, {"Choices", Choices}, {"CanAdvance", Story.CanAdvanceDialogue()},
        {"CanChoose", Story.CanSelectChoice()}, {"SelectedChoiceId", std::to_string(SelectedChoice)}}}, {"Paused", bPaused}, {"MainMenu", bMainMenuVisible},
        {"Title", View.value("projectTitle", ProjectWidgetTitle)}, {"Variables", View.value("variables", Json::object())}, {"GameView", View}};
    std::map<std::string, std::string> Assignments = ProjectWidgets;
    const auto ViewAssignments = View.value("widgetAssets", Json::object());
    for (const auto& Assignment : ViewAssignments.items())
    {
        if (!Assignments.count(Assignment.key()))
        {
            Assignments[Assignment.key()] = Assignment.value().get<std::string>();
        }
    }
    Json Snapshot = Json::array();
    std::set<std::string> Active;
    for (const auto& Assignment : Assignments)
    {
        if (Assignment.second.empty())
        {
            continue;
        }
        const auto Key = Assignment.first + ":" + Assignment.second;
        Active.insert(Key);
        try
        {
            auto Found = WidgetInstances.find(Key);
            if (Found == WidgetInstances.end())
            {
                AssetId Identity;
                AssetRegistryEntry Entry;
                Json Document;
                std::string Error;
                if (!AssetId::TryParse(Assignment.second, Identity) || !Runtime.GetAssetRegistry().TryGetById(Identity, Entry)
                    || Entry.Metadata.Type != WidgetAssetType || !WidgetDefinition::Read(Entry.AbsolutePath, Document, Error)
                    || Document.at("widget").at("kind") != Assignment.first || !ValidateController(Document, Error))
                {
                    throw std::runtime_error("Cannot instantiate widget: " + Assignment.second + " " + Error);
                }
                const auto Controller = Document.value("controller", "");
                if (Controller.empty())
                {
                    continue;
                }
                auto Object = WidgetControllers.at(Controller).Class();
                const auto Identifier = std::to_string(NextWidgetInstance++);
                Object.attr("InstanceId") = Identifier;
                Object.attr("AssetId") = Assignment.second;
                Object.attr("ControllerType") = Controller;
                Object.attr("_Fields") = ToPython(WidgetControllers.at(Controller).Fields);
                Object.attr("_Visible") = true;
                Found = WidgetInstances.emplace(Key, WidgetInstance{Identifier, Assignment.second, Controller,
                    Document.at("widget"), Json(), Object, false}).first;
                Object.attr("OnCreate")();
            }
            auto& Instance = Found->second;
            if (!Instance.bFailed)
            {
                if (Instance.Context != Context)
                {
                    Instance.Context = Context;
                    Instance.Object.attr("OnUpdate")(ToPython(Context));
                }
                Snapshot.push_back({{"instanceId", Instance.Identifier}, {"assetId", Instance.Asset},
                    {"controller", Instance.Controller}, {"kind", Assignment.first}, {"widget", Instance.Layout},
                    {"fields", FromPython(Instance.Object.attr("_Fields"))}, {"visible", Instance.Object.attr("_Visible").cast<bool>()}});
            }
        }
        catch (const std::exception& Error)
        {
            auto Found = WidgetInstances.find(Key);
            if (Found != WidgetInstances.end() && !Found->second.bFailed)
            {
                Found->second.bFailed = true;
                PrintString(std::string("Widget controller failed: ") + Error.what());
            }
            Snapshot.push_back({{"assetId", Assignment.second}, {"kind", Assignment.first}, {"error", Error.what()}, {"visible", false}});
        }
    }
    for (auto Iterator = WidgetInstances.begin(); Iterator != WidgetInstances.end();)
    {
        if (!Active.count(Iterator->first))
        {
            try
            {
                Iterator->second.Object.attr("OnDestroy")();
            }
            catch (const std::exception& Error)
            {
                PrintString(std::string("Widget destruction failed: ") + Error.what());
            }
            Iterator = WidgetInstances.erase(Iterator);
        }
        else
        {
            ++Iterator;
        }
    }
    PublishedWidgets = std::move(Snapshot);
#endif
}

bool WidgetRuntime::Dispatch(const std::string& Identifier, const std::string& ElementIdentifier, const Json& Value, std::string& Error)
{
#if defined(SAKURA_ENABLE_PYTHON)
    try
    {
        if (!WidgetEngine)
        {
            throw std::runtime_error("Widget session is not active");
        }
        for (auto& Entry : WidgetInstances)
        {
            auto& Instance = Entry.second;
            if (Instance.Identifier != Identifier || Instance.bFailed)
            {
                continue;
            }
            if (!Instance.Object.attr("_Visible").cast<bool>())
            {
                throw std::runtime_error("Widget is not visible");
            }
            const auto Fields = FromPython(Instance.Object.attr("_Fields"));
            for (const auto& Element : Instance.Layout.at("elements"))
            {
                if (Element.at("id") != ElementIdentifier)
                {
                    continue;
                }
                const auto Event = Element.value("event", "");
                if (Event.empty() || !Element.value("visible", true))
                {
                    throw std::runtime_error("Element has no enabled widget event");
                }
                const auto EnabledField = Element.value("enabledField", "");
                if (!EnabledField.empty() && !Fields.value(EnabledField, false))
                {
                    throw std::runtime_error("Widget element is disabled");
                }
                if (Element.at("type") == "choices")
                {
                    bool bFound = false;
                    for (const auto& Choice : Fields.at(Element.at("binding").at("path").get<std::string>()))
                    {
                        if (Choice.at("id") == Value && Choice.value("enabled", true))
                        {
                            bFound = true;
                        }
                    }
                    if (!bFound)
                    {
                        throw std::runtime_error("Widget choice is unavailable");
                    }
                }
                else if (Element.at("type") != "button" || !Value.is_null())
                {
                    throw std::runtime_error("Unsupported widget event payload");
                }
                Instance.Object.attr("OnEvent")(Event, ToPython(Value));
                Update(*WidgetEngine);
                return true;
            }
        }
        throw std::runtime_error("Widget instance or element is unavailable");
    }
    catch (const std::exception& Failure)
    {
        Error = Failure.what();
        return false;
    }
#else
    Error = "Python widgets are unavailable";
    return false;
#endif
}

void WidgetRuntime::NarrativeAction(const std::string& Action)
{
    if (!WidgetEngine || WidgetEngine->IsGamePaused() || WidgetEngine->GetPlaySession().GetState() == PlaySessionState::Paused)
    {
        return;
    }
    auto& Story = WidgetEngine->GetStoryRuntime();
    const auto Count = Story.GetChoiceCount();
    if (Action == "nextChoice" && Count > 0)
    {
        SelectedChoice = (SelectedChoice + 1) % Count;
    }
    else if (Action == "previousChoice" && Count > 0)
    {
        SelectedChoice = (SelectedChoice + Count - 1) % Count;
    }
    else if ((Action == "confirmChoice" || Action == "advance") && Story.CanSelectChoice() && Count > 0)
    {
        Story.SelectChoice(SelectedChoice % Count);
        SelectedChoice = 0;
    }
    else if (Action == "advance")
    {
        Story.AdvanceDialogue();
    }
}

Json WidgetRuntime::Slots()
{
    Json Result = Json::array();
    for (const auto& Entry : ProjectWidgets)
    {
        Result.push_back(Entry.first);
    }
    return Result;
}
