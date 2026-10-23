#include "ProjectTemplateContent.h"

#include "Assets/Guid.h"
#include "World/Serialization/SceneSerializer.h"

#include <nlohmann/json.hpp>
#include <initializer_list>
#include <utility>

namespace
{
using Json = nlohmann::json;
constexpr const char* StarterMeshId = "5bcbb1a6-db24-4248-b7d9-40555c563724";
constexpr const char* StarterSubMeshId = "997a9fc2-2f8e-4e5d-82a1-af384af37962";
constexpr const char* StarterCharacterId = "38ab7fb7-d867-5017-9e75-dcb9cb20f0e8";
constexpr const char* StarterCharacterSubMeshId = "a7380ae6-1d02-5203-8261-518f678d7b0c";
constexpr const char* StarterMaterialId = "42b33bd1-5bf7-5462-920b-297de12b0f18";

StoryNode Line(const std::string& Id, const std::string& Speaker, const std::string& Text, const std::string& Next)
{
    StoryNode Node;
    Node.Id = Id;
    Node.Kind = StoryNodeKind::Line;
    Node.Speaker = Speaker;
    Node.Text = Text;
    Node.NextNodeId = Next;
    return Node;
}

StoryNode Choice(const std::string& Id, const std::string& Prompt,
    std::initializer_list<std::pair<std::string, std::string>> Answers)
{
    StoryNode Node;
    Node.Id = Id;
    Node.Kind = StoryNodeKind::Choice;
    Node.Prompt = Prompt;
    for (const auto& [Label, Next] : Answers)
    {
        Node.Choices.push_back({Label, Next, Guid::Generate().ToString()});
    }
    return Node;
}

StoryNode End(const std::string& Id)
{
    StoryNode Node;
    Node.Id = Id;
    Node.Kind = StoryNodeKind::End;
    return Node;
}

StoryNode Wait(const std::string& Id, float Seconds, const std::string& Next)
{
    StoryNode Node;
    Node.Id = Id;
    Node.Kind = StoryNodeKind::Action;
    Node.NextNodeId = Next;
    StoryActionDefinition Action;
    Action.Kind = StoryActionKind::Wait;
    Action.DurationSeconds = Seconds;
    Action.Id = Guid::Generate().ToString();
    Node.Actions.push_back(Action);
    return Node;
}

StoryNode Move(const std::string& Id, const std::string& ObjectId,
    const Vector3& Position, const std::string& Next)
{
    StoryNode Node;
    Node.Id = Id;
    Node.Kind = StoryNodeKind::Action;
    Node.NextNodeId = Next;
    StoryActionDefinition Action;
    Action.Kind = StoryActionKind::MoveTo;
    Action.TargetObjectId = ObjectId;
    Action.TargetPosition = Position;
    Action.DurationSeconds = 1.0f;
    Action.Id = Guid::Generate().ToString();
    Node.Actions.push_back(Action);
    return Node;
}

Json SceneObject(const std::string& Name, const std::string& Id, const Json& Position,
    const Json& Components = Json::array(), const Json& Scale = Json::array({1, 1, 1}))
{
    return Json{{"id", Id}, {"name", Name}, {"parent", nullptr},
        {"bActive", true}, {"bVisual", true}, {"components", Components},
        {"transform", {{"position", Position}, {"rotation", Json::array({0, 0, 0, 1})},
            {"scale", Scale}}}};
}

Json SceneComponentJson(const char* Type, const Json& Properties)
{
    return Json{{"id", Guid::Generate().ToString()}, {"type", Type},
        {"typeVersion", 1}, {"bEnabled", true}, {"properties", Properties}};
}

Json BlockMesh()
{
    return SceneComponentJson("engine.MeshRendererComponent",
        {{"mesh_asset_id", StarterMeshId}, {"mesh_sub_asset_id", StarterSubMeshId},
            {"material_asset_id", StarterMaterialId}, {"visible", true}});
}

Json CharacterMesh()
{
    return SceneComponentJson("engine.MeshRendererComponent",
        {{"mesh_asset_id", StarterCharacterId}, {"mesh_sub_asset_id", StarterCharacterSubMeshId},
            {"material_asset_id", StarterMaterialId}, {"visible", true}});
}
}

bool IsBuiltInProjectTemplate(const std::string& TemplateId)
{
    return TemplateId == "Empty" || TemplateId == "VisualNovel" || TemplateId == "KineticNovel"
        || TemplateId == "builtin-standard" || TemplateId == "builtin-quest"
        || TemplateId == "builtin-shooter" || TemplateId == "builtin-novel"
        || TemplateId == "builtin-horror";
}

bool BuildProjectTemplateContent(const std::string& TemplateId,
    std::string& OutSceneJson, StoryDocument& OutStory, std::string& OutGuide)
{
    if (!IsBuiltInProjectTemplate(TemplateId) || TemplateId == "Empty")
    {
        return false;
    }

    const bool bQuest = TemplateId == "builtin-quest";
    const bool bShooter = TemplateId == "builtin-shooter";
    const bool bHorror = TemplateId == "builtin-horror";
    const bool bKinetic = TemplateId == "KineticNovel";
    const bool bNovel = TemplateId == "builtin-novel" || TemplateId == "VisualNovel";
    const bool bStandard = TemplateId == "builtin-standard";
    const bool bCharacter = bKinetic || bNovel || bStandard;
    const std::string FocusId = Guid::Generate().ToString();

    Json Objects = Json::array();
    Objects.push_back(SceneObject("Main Camera", Guid::Generate().ToString(),
        Json::array({0.0, 2.0, 6.0}), Json::array({SceneComponentJson("engine.CameraComponent",
            {{"primary", true}, {"field_of_view", bShooter ? 75.0 : 60.0}})})));
    Objects.push_back(SceneObject("Key Light", Guid::Generate().ToString(),
        Json::array({0.0, 4.0, 0.0}), Json::array({SceneComponentJson("engine.LightComponent",
            {{"light_type", 0}, {"intensity", bHorror ? 0.35 : 1.0}})})));
    Objects.push_back(SceneObject("Stage Floor", Guid::Generate().ToString(),
        Json::array({0.0, -0.75, -1.0}), Json::array({BlockMesh()}),
        Json::array({8.0, 0.15, 8.0})));
    Objects.push_back(SceneObject(bQuest ? "Locked Door" : bShooter ? "Training Target"
        : bHorror ? "Exit Door" : bNovel ? "Мира" : bCharacter ? "Mira" : "Story Focus", FocusId,
        bCharacter ? Json::array({-1.05, 0.0, -1.0}) : Json::array({0.0, 0.0, 0.0}),
        bCharacter ? Json::array({CharacterMesh(), SceneComponentJson("engine.CharacterComponent",
            {{"description", "Starter story character"}})}) : Json::array({BlockMesh()}),
        bCharacter ? Json::array({1.0, 1.0, 1.0})
            : Json::array({bQuest || bHorror ? 0.5 : 1.0,
                bQuest || bHorror ? 2.0 : 1.0, 0.5})));
    if (bQuest || bShooter || bHorror)
    {
        Objects.push_back(SceneObject(bQuest ? "Key" : bShooter ? "Player Start" : "Recorder",
            Guid::Generate().ToString(), Json::array({-2.0, 0.0, -1.0}),
            Json::array({BlockMesh()}), Json::array({0.3, 0.3, 0.3})));
    }
    if (bNovel)
    {
        Objects.push_back(SceneObject("Window Frame", Guid::Generate().ToString(),
            Json::array({0.0, 1.1, -3.0}), Json::array({BlockMesh()}), Json::array({3.0, 2.2, 0.12})));
        Objects.push_back(SceneObject("Writing Desk", Guid::Generate().ToString(),
            Json::array({0.0, -0.25, 0.5}), Json::array({BlockMesh()}), Json::array({2.4, 0.15, 1.2})));
        Objects.push_back(SceneObject("Letter", Guid::Generate().ToString(),
            Json::array({0.0, -0.13, 0.5}), Json::array({BlockMesh()}), Json::array({0.55, 0.02, 0.4})));
    }
    OutSceneJson = Json{{"format", SceneSerializer::FormatId},
        {"formatVersion", SceneSerializer::FormatVersion},
        {"name", bQuest ? "Quest" : bShooter ? "Training Range" : bHorror ? "Night Shift" : "Main"},
        {"objects", Objects}}.dump(2);

    OutStory = {};
    OutStory.StartNodeId = "intro";
    if (bQuest)
    {
        OutStory.Name = "The Locked Door";
        OutStory.Nodes = {
            Line("intro", "Narrator", "A key lies on the desk. The door at the end of the room is locked.", "inspect"),
            Choice("inspect", "Where do you look?", {{"Take the key", "key"}, {"Try the door", "locked"}}),
            Line("locked", "Narrator", "The handle will not move. The key is still on the desk.", "inspect"),
            Line("key", "Narrator", "The key is cold in your hand.", "door"),
            Choice("door", "Open the door?", {{"Unlock it", "open"}, {"Stay here", "stay"}}),
            Move("open", FocusId, Vector3(1.5f, 0.f, 0.f), "escaped"),
            Line("escaped", "Narrator", "The door slides aside. The corridor is yours to explore.", "end"),
            Line("stay", "Narrator", "You decide to wait a little longer.", "end"), End("end")};
        OutGuide = "# Quest starter\n\nOpen Content/Stories/Opening.story. Choices represent the key and door sequence; the door object moves when unlocked. Edit the story graph and add visuals or interaction components to the scene.\n";
    }
    else if (bShooter)
    {
        OutStory.Name = "Training Range";
        OutStory.Nodes = {
            Line("intro", "Instructor", "Welcome to the training range. Your first target is ahead.", "approach"),
            Choice("approach", "Choose your route", {{"Approach the target", "target"}, {"Leave the range", "leave"}}),
            Move("target", FocusId, Vector3(1.5f, 0.f, 0.f), "result"),
            Line("result", "Instructor", "Target repositioned. Add a player controller and weapon rules to turn this scene into combat.", "end"),
            Line("leave", "Instructor", "Training can continue another day.", "end"), End("end")};
        OutGuide = "# Shooter starter\n\nThe scene contains a camera, light, target marker and player-start marker. Main.story demonstrates branching and a timed target movement. FPS controls, weapons, health and enemy AI are not installed by this template.\n";
    }
    else if (bHorror)
    {
        OutStory.Name = "Last Round";
        OutStory.Nodes = {
            Line("intro", "Guard", "The night shift is almost over. A recorder is still running in the empty hall.", "listen"),
            Choice("listen", "What do you do?", {{"Listen to the recording", "wait"}, {"Go to the exit", "exit"}}),
            Wait("wait", 1.0f, "voice"),
            Line("voice", "Recording", "Do not trust the reflection in the glass.", "exit"),
            Choice("exit", "The exit light flickers.", {{"Open the door", "open"}, {"Stay inside", "stay"}}),
            Move("open", FocusId, Vector3(1.5f, 0.f, 0.f), "outside"),
            Line("outside", "Guard", "Cold air rushes in. The hall is silent again.", "end"),
            Line("stay", "Guard", "You make one last round through the dark hall.", "end"), End("end")};
        OutGuide = "# Horror starter\n\nOpening.story contains an inspection choice, timed pause and alternate endings. The scene supplies an exit, recorder marker and dim light. Add art, sound and movement rules to extend the prototype.\n";
    }
    else if (bKinetic)
    {
        OutStory.Name = "First Chapter";
        OutStory.Nodes = {
            Line("intro", "Narrator", "Rain taps against the window.", "line2"),
            Line("line2", "Mira", "I thought I would never find this place again.", "line3"),
            Line("line3", "Narrator", "The story is just beginning.", "end"), End("end")};
        OutGuide = "# Kinetic novel starter\n\nEdit the linear dialogue in Content/Stories/Opening.story. The story has no choices.\n";
    }
    else if (bNovel)
    {
        OutStory.Name = "Письмо после дождя";
        OutStory.Nodes = {
            Line("intro", "Рассказчик", "Дождь стих. На столе лежит письмо, а за окном ещё мерцают капли.", "greeting"),
            Line("greeting", "Мира", "Ты здесь. Я боялась открывать его одна.", "reply"),
            Choice("reply", "Что ответить Мире?", {{"Давай прочитаем вместе", "kind"}, {"Мне нужно немного времени", "pause"}}),
            Line("kind", "Мира", "Спасибо. Тогда начнём с первой строки.", "end"),
            Line("pause", "Рассказчик", "Мира кивает и оставляет письмо на столе до завтра.", "end"), End("end")};
        OutGuide = "# Визуальная новелла — тестовая сцена\n\n"
            "При запуске Play показывается окно диалога. Нажмите «Дальше», затем выберите ответ. "
            "Content/Stories/Opening.story содержит развилку; стартовая сцена — камеру, свет, окно, стол и письмо. "
            "Content/Audio/RainTheme.wav — короткая музыкальная заставка, которая звучит при запуске Play в настольном редакторе.\n";
    }
    else
    {
        OutStory.Name = bStandard ? "Letters After Rain" : "First Meeting";
        OutStory.Nodes = {
            Line("intro", "Narrator", bStandard ? "A letter waits on the table after the rain." : "Two friends meet beneath the evening sky.", "greeting"),
            Line("greeting", bStandard ? "Mira" : "Ari", bStandard ? "Will you read it with me?" : "I have something to tell you.", "reply"),
            Choice("reply", "How do you answer?", {{"Listen", "kind"}, {"Ask for time", "pause"}}),
            Line("kind", bStandard ? "Mira" : "Ari", "Thank you for staying.", "end"),
            Line("pause", "Narrator", "There will be another moment to talk.", "end"), End("end")};
        OutGuide = "# Visual novel starter\n\nContent/Stories/Opening.story has dialogue, a choice and two endings. Open it in the Story workspace to change lines and branches. Add characters, art and audio under Content.\n";
    }
    return true;
}
