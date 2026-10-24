#include "Core/Threading/CpuProfileScope.h"
#include "Project/GameExporter.h"
#include "Core/EnginePaths.h"
#include "Prefab/PrefabUtility.h"
#include "Commands/PrefabCommands.h"
#include "Core/Object/MemorySubsystem.h"
#include "Story/Narrative.h"
#include "Assets/AssetOperations.h"
#include "Actions/EditorAction.h"
#include "Assets/AssetPreview.h"
#include "Assets/AssetManager.h"
#include "Assets/Resources/TextureResource.h"
#include "Core/Threading/JobSystem.h"
#include "Commands/StaticMeshCommands.h"
#include "World/Components/MeshRendererComponent.h"
#include "World/Components/CharacterComponent.h"
#include "World/Components/PhysicsBodyComponent.h"
#include "World/Components/LightComponent.h"
#include "World/Components/CameraComponent.h"
#include "Core/IO/Base64.h"
#include "Core/IO/ImageBuffer.h"
#include "NativeBridge.h"
#include "Scripting/ScriptingSubsystem.h"
#include "World/Components/ScriptComponent.h"
#include "Commands/EditorCommands.h"
#include "Core/Threading/ThreadContext.h"
#include "World/Scene.h"
#include "World/GameObject.h"
#include "Reflection/Json/ReflectionJson.h"
#include "Reflection/PropertyAccess.h"
#include "Project/ProjectPaths.h"
#include <fstream>
#include <algorithm>
#include <set>
#include <cmath>
#include <charconv>
#include <limits>
#include "Assets/AssetImporter.h"
#include "Assets/StoryAssetCreation.h"
#include "Assets/AssetCreationFactory.h"
#include "Story/StoryDocumentIO.h"
#include "World/Serialization/SceneSerializer.h"
#include "Core/Object/MemorySubsystem.h"

using Json = nlohmann::json;
namespace {
struct Failure : std::runtime_error {
    std::string Code;
    Failure(std::string Code, std::string Message):std::runtime_error(Message),Code(std::move(Code)){}
};
void Require(bool Ok,const char* Code,const std::string& Message) { if(!Ok)throw Failure(Code,Message); }
void Diagnostic(const ReflectionDiagnostic& D) { Require(D.bOk,"PROPERTY_INVALID",D.Message); }
void ValidateJson(const Json& V,int Depth=0) {
    Require(Depth<=16,"INVALID_ARGUMENT","JSON nesting exceeds limit");
    if(V.is_number())Require(std::isfinite(V.get<double>()),"INVALID_ARGUMENT","Non-finite number");
    if(V.is_string())Require(V.get_ref<const std::string&>().size()<=16384 && V.get_ref<const std::string&>().find('\0')==std::string::npos,"INVALID_ARGUMENT","Invalid string");
    if(V.is_array()||V.is_object()) {Require(V.size()<=4096,"INVALID_ARGUMENT","Collection exceeds limit");for(const auto& Child:V)ValidateJson(Child,Depth+1);}
}
Json TransformJson(const Transform& T) {return {{"position",{T.Position.x,T.Position.y,T.Position.z}},
    {"rotation",{T.Rotation.x,T.Rotation.y,T.Rotation.z,T.Rotation.w}}, {"scale",{T.Scale.x,T.Scale.y,T.Scale.z}}};}
Transform ParseTransform(const Json& J) {
    Require(J.is_object()&&J.size()==3&&J.contains("position")&&J.contains("rotation")&&J.contains("scale"),"INVALID_ARGUMENT","Expected position, rotation, scale");
    for(const auto* Key:{"position","rotation","scale"}) {
        const auto& A=J.at(Key);Require(A.is_array()&&A.size()==(std::string(Key)=="rotation"?4:3),"INVALID_ARGUMENT","Invalid transform vector");
        for(const auto& V:A)Require(V.is_number()&&std::isfinite(V.get<double>())&&std::abs(V.get<double>())<=1e8,"INVALID_ARGUMENT","Invalid transform number");
    }
    Transform T;T.Position={J["position"][0],J["position"][1],J["position"][2]};
    T.Scale={J["scale"][0],J["scale"][1],J["scale"][2]};
    T.Rotation={J["rotation"][0],J["rotation"][1],J["rotation"][2],J["rotation"][3]};
    Require(T.Rotation.LengthSquared()>1e-10f,"INVALID_ARGUMENT","Zero quaternion");T.Rotation.Normalize();
    Require(std::abs(T.Scale.x)>1e-6&&std::abs(T.Scale.y)>1e-6&&std::abs(T.Scale.z)>1e-6,"INVALID_ARGUMENT","Singular scale");return T;
}
bool Readable(const PropertyDescriptor& P) {return !P.Attributes.bEditorHidden&&HasPropertyFlag(P.Attributes.Flags,PropertyFlags::EditorVisible);}
bool Editable(const PropertyDescriptor& P) {return Readable(P)&&!P.bReadOnly&&!HasPropertyFlag(P.Attributes.Flags,PropertyFlags::EditorReadOnly)&&HasPropertyFlag(P.Attributes.Flags,PropertyFlags::EditorEditable);}
}
NativeBridge::NativeBridge(Engine& R,SceneWorkspace& W,ProjectSession& P,const std::filesystem::path& File,std::function<void()> Reset, bool bInRuntimeOnly)
    :Runtime(R),Workspace(W),Project(P),Session(Guid::Generate().ToString()),ResetInput(std::move(Reset)) {
    bRuntimeOnly = bInRuntimeOnly;
    AssertGameThread();std::ifstream Input(File);Input>>Contract;Require(Contract.at("version")==1,"PROTOCOL_ERROR","Unsupported contract");
    LastWorld=World()->GetObjectHandle();
    LastEditWorld=Runtime.GetEditScene()->GetObjectHandle();
    AssetDiagnostic D; if(P.GetProject()) ContentHash::TryHashFile(P.GetProject()->ProjectFile.u8string(),ProjectFingerprint,D);
    if(P.GetProject()&&!P.GetProject()->StartupStory.empty()) {
        Story.Open(P.GetProject()->StartupStory,StoryError);
    }
}
void NativeBridge::ObserveWorld() {
    if (!TransformTransaction.empty() && (!Runtime.GetEditScene() || Runtime.GetEditScene()->GetObjectHandle() != TransformWorld || Runtime.GetPlaySession().IsSimulating()))
    {
        CancelTransformEdit();
    }
    const auto Edit=Runtime.GetEditScene()?Runtime.GetEditScene()->GetObjectHandle():ObjectHandle{};
    if(Edit!=LastEditWorld){Workspace.GetCommands().Clear();Workspace.ClearSelection();LastEditWorld=Edit;}
    const auto Active=Runtime.GetActiveScene()?Runtime.GetActiveScene()->GetObjectHandle():ObjectHandle{};
    if(Active!=LastWorld){LastWorld=Active;++Epoch;Workspace.ClearSelection();}
}
Json NativeBridge::Context() const {return {{"session",Session},{"epoch",Epoch},{"revision",Revision}};}
void NativeBridge::CancelTransformEdit()
{
    auto* EditScene = Runtime.GetEditScene();
    if (!TransformTransaction.empty() && EditScene && EditScene->GetObjectHandle() == TransformWorld)
    {
        for (auto* SceneObject : EditScene->GetAllObjects())
        {
            if (SceneObject->GetPersistentId() == TransformObjectId && !SceneObject->IsPendingDestroy())
            {
                SceneObject->SetTransform(TransformBefore);
                break;
            }
        }
    }
    TransformTransaction.clear();
    TransformObjectId.clear();
}
Scene* NativeBridge::World() {auto* S=Runtime.GetActiveScene();Require(S!=nullptr,"NO_SCENE","No scene is open");return S;}
GameObject* NativeBridge::Find(const std::string& Id) {
    for(auto* O:World()->GetAllObjects())if(O->GetPersistentId()==Id&&!O->IsPendingDestroy())return O;
    throw Failure("NOT_FOUND","Object no longer exists: "+Id);
}
Component* NativeBridge::FindComponent(const Json& P) {
    for(auto* C:Find(P.at("objectId"))->GetAllComponents())if(C->GetPersistentId()==P.at("componentId"))return C;
    throw Failure("NOT_FOUND","Component no longer exists on this object");
}
void NativeBridge::ValidateParams(const std::string& M,const Json& P) {
    Require(Contract["methods"].contains(M),"UNKNOWN_METHOD","Unsupported method: "+M);
    Require(P.is_object(),"INVALID_ARGUMENT","params must be an object");
    std::set<std::string> Allowed;
    for(auto It=Contract["methods"][M].begin();It!=Contract["methods"][M].end();++It) {
        std::string K=It.key();bool Optional=K.back()=='?';if(Optional)K.pop_back();Allowed.insert(K);
        if(!P.contains(K)){Require(Optional,"INVALID_ARGUMENT","Missing parameter: "+K);continue;}
        const auto& V=P[K];const std::string T=It.value();
        if (T == "serializedJson")
        {
            Require(V.is_string() && V.get_ref<const std::string&>().size() <= 1024 * 1024, "INVALID_ARGUMENT", "Invalid " + K);
            const auto Document = Json::parse(V.get_ref<const std::string&>(), nullptr, false);
            Require(!Document.is_discarded(), "INVALID_ARGUMENT", "Invalid JSON document");
            ValidateJson(Document);
            continue;
        }
        ValidateJson(V, 1);
        if(T=="json")continue;
        if(T=="number"){Require(V.is_number()&&std::abs(V.get<double>())<=1e8,"INVALID_ARGUMENT","Invalid "+K);continue;}
        if(T=="transform"){ParseTransform(V);continue;}
        if(T=="boolean"){Require(V.is_boolean(),"INVALID_ARGUMENT",K+" must be boolean");continue;}
        if(T=="nullableId"&&V.is_null())continue;
        Require(V.is_string()&&!V.get_ref<const std::string&>().empty()&&V.get_ref<const std::string&>().size()<=(T=="string"?1024:256),"INVALID_ARGUMENT","Invalid "+K);
    }
    for(auto It=P.begin();It!=P.end();++It)Require(Allowed.count(It.key())!=0,"INVALID_ARGUMENT","Unknown parameter: "+It.key());
}
std::filesystem::path NativeBridge::ContentPath(const std::string& Text) {
    auto Root=std::filesystem::weakly_canonical(ProjectPaths::Content());auto Relative=std::filesystem::u8path(Text);
    Require(!Relative.is_absolute()&&!Relative.has_root_name()&&Text.find(':')==std::string::npos,"INVALID_PATH","Use a path relative to project Content");
    auto Path=std::filesystem::weakly_canonical(Root/Relative);auto R=Root.begin(),P=Path.begin();
    for(;R!=Root.end();++R,++P)Require(P!=Path.end()&&*R==*P,"INVALID_PATH","Scene path leaves project Content");
    return Path;
}
std::filesystem::path NativeBridge::ScenePath(const std::string& Text) {auto Path=ContentPath(Text);Require(Path.extension()==".scene","INVALID_PATH","Expected .scene file");return Path;}
void NativeBridge::Commit(std::unique_ptr<EditorCommand> C) {Require(Workspace.GetCommands().Execute(std::move(C)),"COMMAND_FAILED","EditorCore rejected the command");}
Json NativeBridge::ConvertValue(const TypeId& T,const Json& V,bool Decode) {
    if(Decode&&(T.Value=="engine.float"||T.Value=="engine.Vector3"||T.Value=="engine.EulerAngles"||T.Value=="engine.Color"||T.Value=="engine.Quaternion")) {
        const auto Check=[](const Json& N){Require(N.is_number()&&std::isfinite(N.get<double>())&&std::abs(N.get<double>())<=std::numeric_limits<float>::max(),"INVALID_ARGUMENT","Value is outside float range");};
        if(V.is_array())for(const auto& N:V)Check(N);else if(!V.is_object())Check(V);
    }
    if(T.Value=="engine.int64") {
        if(!Decode)return V.is_number_integer()?Json(V.dump()):V;
        Require(V.is_string(),"INVALID_ARGUMENT","int64 values use decimal strings");
        const auto S=V.get<std::string>();int64_t I=0;auto R=std::from_chars(S.data(),S.data()+S.size(),I);
        Require(R.ec==std::errc{}&&R.ptr==S.data()+S.size(),"INVALID_ARGUMENT","Invalid int64");return I;
    }
    auto& Reflection=ReflectionSubsystem::Get();auto* Type=Reflection.FindType(T);
    if(Type&&Type->Kind==TypeKind::Array&&V.is_array()){Json A=Json::array();for(const auto& E:V)A.push_back(ConvertValue(Type->ElementTypeId,E,Decode));return A;}
    if(Type&&Type->Kind==TypeKind::Struct&&V.is_object()) {
        Json O=V;if(const auto* Fields=Reflection.FindStructProperties(T))for(const auto& F:*Fields)if(O.contains(F.Id.Value))O[F.Id.Value]=ConvertValue(F.ValueTypeId,O[F.Id.Value],Decode);return O;
    }
    return V;
}
Json NativeBridge::Components(GameObject* O) {
    Json A=Json::array();for(auto* C:O->GetAllComponents())A.push_back({{"id",C->GetPersistentId()},{"typeId",C->GetTypeId().Value},{"displayName",C->GetClass()->GetDisplayName()},{"enabled",C->IsEnabled()}});return A;
}
Json NativeBridge::Properties(Component* C) {
    CpuProfileScope CpuMeasurement("Bridge.Properties");
    Json A=Json::array();if(!C->GetClass())return A;
    for(const auto* P:C->GetClass()->GetAllProperties())if(Readable(*P)) {
        Json Item{{"id",P->Id.Value},{"typeId",P->ValueTypeId.Value},{"editable",Editable(*P)},
            {"label",P->Attributes.DisplayName?P->Attributes.DisplayName:P->Id.Value},
            {"localizationKey", "Editor.Reflection." + C->GetTypeId().Value + "." + P->Id.Value + ".DisplayName"},
            {"category",P->Attributes.Category?P->Attributes.Category:""},{"value",nullptr}};
        ReflectedValue V;auto D=PropertyAccess::GetProperty(C,*P,V,PropertyAccessContext::Inspector);Json Value;
        if(D.bOk)D=ReflectionJson::ReflectedValueToJson(P->ValueTypeId,V,Value);
        if(D.bOk)Item["value"]=ConvertValue(P->ValueTypeId,Value,false);else Item["error"]=D.Message;
        if (P->Id.Value == "animation")
        {
            if (auto* Character = dynamic_cast<CharacterComponent*>(C))
            {
                Character->Resolve(Runtime.GetAssetManager());
                Item["choices"] = Json::array();
                if (const auto* Definition = Character->GetDefinition())
                {
                    for (const auto& Motion : Definition->Animations)
                    {
                        if (Motion.bEnabled)
                        {
                            Item["choices"].push_back({{"id", Motion.Identifier}, {"name", Motion.Name}});
                        }
                    }
                }
            }
        }
        if (const auto* Enumeration = ReflectionSubsystem::Get().FindEnum(P->ValueTypeId))
        {
            Item["enumValues"] = Json::array();
            for (const auto& Entry : Enumeration->Values)
            {
                Item["enumValues"].push_back(Entry.Name);
            }
        }
        if(P->Attributes.AssetTypeFilter||!P->Attributes.AllowedAssetTypes.empty()) {
            auto Types=P->Attributes.AllowedAssetTypes;
            if(P->Attributes.AssetTypeFilter)Types.push_back(P->Attributes.AssetTypeFilter);
            Item["assetTypes"]=Types;Item["subAssetId"]=nullptr;
            if (D.bOk && V.ValueKind == ReflectedValue::Kind::AssetReference)
            {
                Item["assetId"] = nullptr;
                if (V.AssetValue.IsValid()) { Item["assetId"] = V.AssetValue.Asset.ToString(); }
                if (V.AssetValue.HasSubAsset()) { Item["subAssetId"] = V.AssetValue.SubAsset->ToString(); }
            }
            if(P->Attributes.CompanionProperty) {
                const auto* Other=C->GetClass()->FindProperty(PropertyId{P->Attributes.CompanionProperty});ReflectedValue Sub;
                if(Other&&PropertyAccess::GetProperty(C,*Other,Sub).bOk)Item["subAssetId"]=Sub.StringValue;
            }
        }
        if(P->Attributes.bHasRange)Item["range"]={P->Attributes.RangeMinimum,P->Attributes.RangeMaximum};A.push_back(std::move(Item));
    }return A;
}
Json NativeBridge::Hierarchy() {
    Json A=Json::array();for(auto* O:World()->GetAllObjects())if(!O->IsPendingDestroy())A.push_back({{"id",O->GetPersistentId()},
        {"name",O->GetName()},{"parentId",O->GetParent()?Json(O->GetParent()->GetPersistentId()):Json(nullptr)},
        {"active",O->IsActive()},{"transform",TransformJson(O->GetTransform())},{"components",Components(O)}});return A;
}
bool NativeBridge::ProjectChanged() const {
    if(!Project.GetProject()) return false;
    ContentHash Current; AssetDiagnostic D;
    return !ContentHash::TryHashFile(Project.GetProject()->ProjectFile.u8string(),Current,D) || Current!=ProjectFingerprint;
}
void NativeBridge::ResolveWidgetImages(Json& Widget)
{
    for (auto& Element : Widget.at("elements"))
    {
        if (!Element.contains("imageAssetId") || Element.at("imageAssetId").is_null())
        {
            continue;
        }
        const auto Image = Asset(Element.at("imageAssetId"));
        Require(Image.Metadata.Type == TextureAssetType, "GAME_VIEW_ERROR", "Widget image must reference a texture");
        const auto Identifier = Image.Metadata.Guid.ToString() + Image.Metadata.SourceFingerprint.ToHex();
        const auto Cached = RuntimeWidgetImages.find(Identifier);
        if (Cached != RuntimeWidgetImages.end())
        {
            Element["src"] = Cached->second;
            continue;
        }
        const AssetKey Key{Image.Metadata.Guid, std::nullopt};
        auto& Manager = Runtime.GetAssetManager();
        if (Manager.GetLoadState(Key) == AssetLoadState::Unloaded)
        {
            Manager.LoadAsync<TextureResource>(Key);
        }
        AssetHandle<TextureResource> Texture;
        if (Manager.TryGetLoaded(Key, Texture))
        {
            const auto Pixels = ImageBuffer::FromRgba(Texture->Pixels.data(), Texture->Width, Texture->Height, Texture->RowStride).ResizeToFit(512, 512);
            const auto Bytes = Pixels.EncodePng();
            Element["src"] = "data:image/png;base64," + Base64::Encode(Bytes.data(), Bytes.size());
            RuntimeWidgetImages[Identifier] = Element.at("src").get<std::string>();
        }
    }
}

Json NativeBridge::GameViews()
{
    Json Views = Json::array();
    if (!Runtime.IsGameRunning() && !Runtime.GetPlaySession().IsSimulating())
    {
        return Views;
    }
    for (auto* SceneObject : World()->GetAllObjects())
    {
        for (auto* Script : SceneObject->GetComponents<ScriptComponent>())
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
                Require(Invoked.bOk, "GAME_VIEW_ERROR", Invoked.Message);
                Require(Result.ValueKind == ReflectedValue::Kind::String && Result.StringValue.size() <= 65536,
                    "GAME_VIEW_ERROR", "Game view must return a bounded JSON string");
                auto View = Json::parse(Result.StringValue);
                Require(View.is_object(), "GAME_VIEW_ERROR", "Game view must be an object");
                if (View.contains("widgetLibrary"))
                {
                    const auto Library = Asset(View.at("widgetLibrary"));
                    Require(Library.Metadata.Type == WidgetLibraryAssetType, "GAME_VIEW_ERROR", "widgetLibrary must reference a WidgetLibrary asset");
                    auto& Cached = RuntimeWidgetLibraries[Library.Metadata.Guid.ToString()];
                    if (Cached.second.is_null() || Cached.first != Library.Metadata.SourceFingerprint)
                    {
                        Json Document;
                        std::string Error;
                        Require(WidgetLibraryDocument::Read(std::filesystem::u8path(Library.AbsolutePath), Document, Error), "GAME_VIEW_ERROR", Error);
                        Cached = {Library.Metadata.SourceFingerprint, std::move(Document)};
                    }
                    View["widgets"] = Json::object();
                    for (auto Widget : Cached.second.at("widgets"))
                    {
                        const auto Kind = Widget.at("kind").get<std::string>();
                        if (Cached.second.at("ui").at(Kind) == Widget.at("id"))
                        {
                            ResolveWidgetImages(Widget);
                            if (Kind == "hud")
                            {
                                View["widget"] = Widget;
                            }
                            else
                            {
                                View["widgets"][Kind] = Widget;
                            }
                        }
                    }
                }
                View["objectId"] = SceneObject->GetPersistentId();
                View["componentId"] = Script->GetPersistentId();
                Views.push_back(std::move(View));
            }
        }
    }
    Require(Views.dump().size() <= 6291456, "GAME_VIEW_ERROR", "Resolved game UI exceeds the 6 MiB transport budget");
    return Views;
}

void NativeBridge::ExecuteGameAction(const Json& Parameters)
{
    Require(Runtime.IsGameRunning() || Runtime.GetPlaySession().IsSimulating(),
        "INVALID_STATE", "Game actions require a game session");
    const bool bPaused = Runtime.IsGamePaused() || Runtime.GetPlaySession().GetState() == PlaySessionState::Paused;
    auto* Script = dynamic_cast<ScriptComponent*>(FindComponent(Parameters));
    Require(Script != nullptr && Script->IsPlaying(), "INVALID_ARGUMENT", "Game action requires a playing script component");
    bool bAvailable = false;
    for (const auto& View : GameViews())
    {
        if (View.at("componentId") != Parameters.at("componentId"))
        {
            continue;
        }
        for (const auto& Action : View.value("actions", Json::array()))
        {
            if (Action.at("id") == Parameters.at("action") && Action.value("enabled", true)
                && (!bPaused || Action.value("allowWhilePaused", false)))
            {
                bAvailable = true;
            }
        }
    }
    Require(bAvailable, "INVALID_STATE", "Game action is not currently available");
    ReflectedValue Result;
    const auto Invoked = ScriptingSubsystem::Get().InvokeMethod(*Script, "ExecuteGameAction",
        {{"Action", ReflectedValue::MakeString(Parameters.at("action").get<std::string>())}}, Result);
    Require(Invoked.bOk, "GAME_ACTION_ERROR", Invoked.Message);
}

Json NativeBridge::Describe() {
    auto* Selected=Workspace.GetSelectedObject(World());auto& Play=Runtime.GetPlaySession();
    auto& Playback=Runtime.GetStoryRuntime();
    Json Choices=Json::array();
    for(size_t I=0;I<Playback.GetChoiceCount();++I) Choices.push_back(Playback.GetChoice(I).Label);
    const auto Phase=Playback.GetPlaybackPhase();
    Json Dialogue={{"phase",Phase==StoryPlaybackPhase::Line?"line":Phase==StoryPlaybackPhase::Choice?"choice":Phase==StoryPlaybackPhase::Action?"action":Phase==StoryPlaybackPhase::End?"end":Phase==StoryPlaybackPhase::Failed?"failed":"idle"},
        {"speaker",Playback.GetDisplayedSpeaker()},{"text",Playback.GetDisplayedLine()},
        {"choices",Choices},{"canAdvance",Playback.CanAdvanceDialogue()},
        {"canChoose",Playback.CanSelectChoice()},{"error",Playback.GetLastError()}};
    Json StartupAudioPath = nullptr;
    if (Project.GetProject() && !Project.GetProject()->StartupAudio.empty())
    {
        StartupAudioPath = Project.GetProject()->StartupAudio.lexically_relative(ProjectPaths::Content()).generic_string();
    }
    return {{"startupAudioPath",StartupAudioPath},{"gameRunning",Runtime.IsGameRunning()},{"gamePaused",Runtime.IsGamePaused()},{"dirty",Workspace.GetDocument().IsDirty()},{"storyDirty",Story.IsDirty()},{"assetDocumentsDirty",HasDirtyAssetDocuments()},
        {"externalChanges",Workspace.GetDocument().HasExternalChanges()},{"projectExternalChanges",ProjectChanged()},
        {"path",Workspace.GetDocument().GetDocumentPath().generic_string()},
        {"project",Project.GetProject()?Project.GetProject()->Name:""},
        {"play",Play.GetState()==PlaySessionState::Stopped?"stopped":Play.GetState()==PlaySessionState::Paused?"paused":"playing"},
        {"dialogue",Dialogue},{"gameViews",GameViews()},{"startupAudio",Project.GetProject()&&!Project.GetProject()->StartupAudio.empty()},
        {"selection",Selected?Json(Selected->GetPersistentId()):Json(nullptr)},
        {"canUndo",Workspace.GetCommands().CanUndo()},{"canRedo",Workspace.GetCommands().CanRedo()},{"assetError",AssetError}};
}
Json NativeBridge::Dispatch(const std::string& M,const Json& P) {
    if (bRuntimeOnly)
    {
        Require(M == "bridge.describe" || M == "game.state" || M == "game.advance" || M == "game.choose"
            || M == "game.pause" || M == "game.action" || M == "audio.resolveStartup", "PLAYER_READ_ONLY", "Editor operations are unavailable in Player");
    }
    if (M == "game.action")
    {
        ExecuteGameAction(P);
        return Describe();
    }
    if (M.rfind("game.", 0) == 0)
    {
        Require(bRuntimeOnly && Runtime.IsGameRunning(), "INVALID_STATE", "Standalone game is not running");
        if (M == "game.advance")
        {
            Require(!Runtime.IsGamePaused() && Runtime.GetStoryRuntime().AdvanceDialogue(), "INVALID_STATE", "Dialogue cannot advance");
        }
        if (M == "game.choose")
        {
            const double Index = P.at("index");
            Require(!Runtime.IsGamePaused() && Index >= 0 && std::floor(Index) == Index
                && Index < Runtime.GetStoryRuntime().GetChoiceCount(), "INVALID_ARGUMENT", "Choice is unavailable");
            Require(Runtime.GetStoryRuntime().SelectChoice(static_cast<size_t>(Index)), "INVALID_STATE", "Choice cannot be selected");
        }
        if (M == "game.pause")
        {
            Runtime.SetGamePaused(P.at("paused"));
        }
        return Describe();
    }

    if (M == "scene.pickMesh")
    {
        Require(!Runtime.GetPlaySession().IsSimulating() && !GetPreviewScene(), "PLAY_READ_ONLY", "Material drops require the editable main scene");
        const float Horizontal = P.at("horizontal");
        const float Vertical = P.at("vertical");
        Require(Horizontal >= 0.f && Horizontal <= 1.f && Vertical >= 0.f && Vertical <= 1.f, "INVALID_ARGUMENT", "Viewport coordinates must be normalized");
        Require(static_cast<bool>(PickViewportMesh), "UNAVAILABLE", "Viewport picking is unavailable");
        return PickViewportMesh(Horizontal, Vertical);
    }
    if (M.rfind("prefab.", 0) == 0) { return PrefabTools(M, P); }
    if (M == "assetDocument.execute") { return AssetDocumentTools(P); }
    if(M.rfind("mesh.",0)==0 || M=="assets.place") { return MeshTools(M, P); }
    if(M.rfind("story.",0)==0||M.rfind("assets.",0)==0)return DispatchDocuments(M,P);
    auto& Commands=Workspace.GetCommands();auto& Document=Workspace.GetDocument();auto& Play=Runtime.GetPlaySession();
    const bool Read=M=="project.settings.get"||M=="editor.actions"||M=="bridge.describe"||M=="scene.getHierarchy"||M=="object.getComponents"||M=="component.getProperties"||M=="transform.get"||M=="reflection.getComponentTypes"||M=="assets.getScenes"||M=="audio.resolveStartup";
    if(!Read&&M!="selection.set"&&M.rfind("play.",0)!=0)Require(!Play.IsSimulating(),"PLAY_READ_ONLY","Stop play before editing or opening/saving a scene");
    if(!Read)ResetInput();
    if (M == "editor.actions")
    {
        Json Result = Json::array();
        for (const auto* Action : EditorAction::CollectPublishedActions())
        {
            Result.push_back({{"id", Action->GetTypeId().Value}, {"label", Action->GetDisplayName()},
                {"category", Action->GetMenuCategory()}, {"requiresSelection", Action->RequiresSelection()}});
        }
        for (const auto& Entry : Runtime.GetAssetRegistry().FindByType(ModelAssetType))
        {
            if (Entry.VirtualPath.rfind("/Engine/BasicShapes/", 0) != 0) { continue; }
            for (const auto& Subasset : Entry.Metadata.SubAssets)
            {
                if (Subasset.Type != StaticMeshAssetType) { continue; }
                Result.push_back({{"id", Entry.Metadata.Guid.ToString() + ":" + Subasset.Id.ToString()},
                    {"label", Subasset.Name}, {"category", "Objects"}, {"requiresSelection", false},
                    {"assetId", Entry.Metadata.Guid.ToString()}, {"subAssetId", Subasset.Id.ToString()}});
            }
        }
        return Result;
    }
    if (M == "editor.executeAction")
    {
        for (const auto* Action : EditorAction::CollectPublishedActions())
        {
            if (Action->GetTypeId().Value != P.at("actionId")) { continue; }
            EditorActionContext Context;
            Context.EditScene = World();
            Context.CommandStack = &Commands;
            if (P.contains("parentId") && !P.at("parentId").is_null())
            {
                Context.SelectedObject = Find(P.at("parentId"))->GetObjectHandle();
            }
            if (Action->RequiresSelection())
            {
                Context.SelectedObject = Workspace.GetSelectedIdentity();
            }
            Context.ExecuteCommand = [&Commands](std::unique_ptr<EditorCommand> Command)
            {
                return Commands.Execute(std::move(Command));
            };
            Context.SelectObject = [this](ObjectHandle Identity)
            {
                Workspace.Select(World(), Identity);
            };
            Require(Action->CanExecute(Context) && Action->Execute(Context), "COMMAND_FAILED", "Editor action could not execute");
            const auto* Selected = Workspace.GetSelectedObject(World());
            Json Identifier = nullptr;
            if (Selected) { Identifier = Selected->GetPersistentId(); }
            return {{"id", Identifier}};
        }
        throw Failure("NOT_FOUND", "Editor action is not registered");
    }
    if(M=="bridge.describe")return Describe();
    if(M=="scene.getHierarchy")return Hierarchy();
    if (M == "project.settings.get" || M == "project.settings.set")
    {
        Require(Project.GetProject() != nullptr, "NO_PROJECT", "No project is open");
        if (M == "project.settings.set")
        {
            Require(!ProjectChanged(), "EXTERNAL_CHANGES", "Project descriptor changed on disk; reopen the project before saving settings");
            const auto& Settings = P.at("settings");
            Require(Settings.is_object() && Settings.size() == 3, "INVALID_ARGUMENT", "Expected name, startupScene and startupStory");
            for (const auto* Key : {"name", "startupScene", "startupStory"})
            {
                Require(Settings.contains(Key) && Settings.at(Key).is_string(), "INVALID_ARGUMENT", "Project settings must be strings");
            }
            ProjectDescriptor Updated = *Project.GetProject();
            Updated.Name = Settings.at("name").get<std::string>();
            const auto LastCharacter = Updated.Name.find_last_not_of(" \t\r\n\v\f");
            if (LastCharacter == std::string::npos)
            {
                Updated.Name.clear();
            }
            else
            {
                Updated.Name.erase(LastCharacter + 1);
                Updated.Name.erase(0, Updated.Name.find_first_not_of(" \t\r\n\v\f"));
            }
            for (const auto* Key : {"startupScene", "startupStory"})
            {
                const auto Relative = std::filesystem::u8path(Settings.at(Key).get<std::string>());
                Require(!Relative.is_absolute() && !Relative.has_root_name(), "INVALID_ARGUMENT", "Startup paths must be project-relative");
                std::filesystem::path Absolute;
                if (!Relative.empty())
                {
                    Absolute = (Updated.ProjectRoot / Relative).lexically_normal();
                    std::string Extension = ".scene";
                    if (std::string(Key) == "startupStory")
                    {
                        Extension = ".story";
                    }
                    Require(Absolute.extension() == Extension, "INVALID_ARGUMENT", "Unexpected startup document extension");
                }
                if (std::string(Key) == "startupScene")
                {
                    Updated.StartupScene = Absolute;
                }
                else
                {
                    Updated.StartupStory = Absolute;
                }
            }
            std::string Error;
            const bool bSaved = Project.UpdateSettings(Updated, Error);
            Require(bSaved, "INVALID_ARGUMENT", Error);
            AssetDiagnostic Diagnostic;
            ContentHash::TryHashFile(Updated.ProjectFile.u8string(), ProjectFingerprint, Diagnostic);
        }
        const auto& Descriptor = *Project.GetProject();
        auto RelativePath = [&Descriptor](const std::filesystem::path& Path)
        {
            if (Path.empty())
            {
                return std::string();
            }
            return Path.lexically_relative(Descriptor.ProjectRoot).generic_u8string();
        };
        return {{"name", Descriptor.Name}, {"engineVersion", Descriptor.EngineVersion},
            {"startupScene", RelativePath(Descriptor.StartupScene)}, {"startupStory", RelativePath(Descriptor.StartupStory)}};
    }
    if (M == "project.export")
    {
        Require(Project.GetProject() != nullptr && !ProjectChanged(), "EXTERNAL_CHANGES", "Reopen the changed project before exporting");
        Require(!Document.IsDirty() && !Story.IsDirty() && !HasDirtyAssetDocuments(), "UNSAVED_CHANGES", "Save all documents before exporting");
        std::string Error;
        Require(GameExporter::Export(Project.GetProject()->ProjectFile,
            std::filesystem::u8path(P.at("runtimeDirectory").get<std::string>()), EnginePaths::Root(),
            std::filesystem::u8path(P.at("destination").get<std::string>()), P.at("executableName"), Error), "EXPORT_ERROR", Error);
        return {{"destination", P.at("destination")}};
    }
    if(M=="project.save") {
        Require(!ProjectChanged(),"EXTERNAL_CHANGES","Project descriptor changed on disk; reopen the project before saving startup settings");
        Require(!Document.HasExternalChanges()&&!Story.HasExternalChanges(),"EXTERNAL_CHANGES","A document changed on disk. Reload it or save a scene copy first");
        std::string Error;
        Require(Document.Save(Error),"IO_ERROR",Error);
        if(Story.IsOpen()&&Story.IsDirty()) Require(Story.Save(Error),"STORY_ERROR",Error);
        for (auto& Entry : AssetDocuments)
        {
            if (Entry.second.Reflected && Entry.second.Reflected->IsDirty())
            {
                Require(Entry.second.Reflected->Save(Error), "IO_ERROR", Error);
            }
            if (Entry.second.Widgets && Entry.second.Widgets->IsDirty())
            {
                Require(Entry.second.Widgets->Save(Error), "IO_ERROR", Error);
            }
            if (Entry.second.Script && Entry.second.Script->IsDirty())
            {
                Require(Entry.second.Script->Save(Error), "IO_ERROR", Error);
            }
            if (Entry.second.Mesh && Entry.second.Mesh->IsDirty())
            {
                Require(Entry.second.Mesh->Save(Runtime.GetAssetManager(), Error), "IO_ERROR", Error);
            }
            if (Entry.second.Material && Entry.second.Material->GetDocument().IsDirty())
            {
                const auto Saved = Entry.second.Material->Execute(MaterialEditOperation::Save);
                Require(!Saved.HasError(), "IO_ERROR", Saved.Message);
            }
        }
        if(P.value("useCurrentAsStartup",false)) {
            Require(Project.SetStartupDocuments(Document.GetDocumentPath(),Story.IsOpen()?Story.GetPath():Project.GetProject()->StartupStory,Error),"IO_ERROR",Error);
            AssetDiagnostic D; ContentHash::TryHashFile(Project.GetProject()->ProjectFile.u8string(),ProjectFingerprint,D);
        }
        return Describe();
    }
    if(M=="scene.create") {
        Require(!Document.IsDirty(),"UNSAVED_CHANGES","Save the current scene first");
        auto Path=ScenePath(P.at("path"));Require(!std::filesystem::exists(Path),"ALREADY_EXISTS","Scene already exists");
        Require(!Runtime.GetAssetRegistry().LeafNameExists(Path.stem().u8string()),"ALREADY_EXISTS","Asset name already exists in project");
        std::unique_ptr<Scene> Empty(MemorySubsystem::Get()->NewObject<Scene>()); std::string Error;
        auto Saved=SceneSerializer::SerializeToFile(*Empty,Path);Require(Saved.bOk,"IO_ERROR",Saved.Error);
        Require(Document.Open(Path,Error),"IO_ERROR",Error);ObserveWorld();return Describe();
    }
    if(M=="scene.open") {
        Require(!Document.IsDirty()||P.value("discard",false),"UNSAVED_CHANGES","Save first or explicitly discard changes");
        auto Path=ScenePath(P.at("path"));std::string Error;
        Require(Document.Open(Path,Error),"IO_ERROR",Error);ObserveWorld();return Describe();
    }
    if(M=="scene.save") {
        if(P.contains("path")) {const auto Path=ScenePath(P.at("path"));Require(!std::filesystem::exists(Path)||Path==std::filesystem::weakly_canonical(Document.GetDocumentPath()),"ALREADY_EXISTS","Choose a new filename for the scene copy");}
        std::string Error;bool Ok=P.contains("path")?Document.SaveAs(ScenePath(P.at("path")),Error):Document.Save(Error);
        Require(Ok,"IO_ERROR",Error);return Describe();
    }
    if(M=="character.create") {
        const std::string Name=P.at("name").get<std::string>();
        Require(!Name.empty(),"INVALID_ARGUMENT","Character name is required");
        AssetRegistryEntry CharacterAsset;
        Require(Runtime.GetAssetRegistry().TryGetByPath("/Engine/Characters/StarterHumanoid/StarterHumanoid.character",CharacterAsset),
            "NOT_FOUND","Starter character asset is unavailable");
        ObjectHandle Created,CharacterComponent;
        Commands.BeginGroup("Create Character");
        try {
            Commit(MakeCreateObjectCommand(World(),Name,{},&Created));
            if(P.contains("transform"))Commit(MakeSetTransformCommand(World(),Created,ParseTransform(P["transform"])));
            Commit(MakeAddComponentCommand(World(),Created,TypeId{"engine.CharacterComponent"},&CharacterComponent));
            ReflectedValue CharacterValue;
            CharacterValue.ValueKind=ReflectedValue::Kind::AssetReference;
            CharacterValue.AssetValue={CharacterAsset.Metadata.Guid};
            Commit(MakeSetPropertyCommand(World(),CharacterComponent,PropertyId{"character"},CharacterValue));
            MaterialSlotOverrides Materials;
            for(int32_t Slot=0;Slot<2;++Slot) {
                AssetRegistryEntry Material;
                Require(Runtime.GetAssetRegistry().TryGetByPath("/Engine/Characters/StarterHumanoid/HumanoidMaterial"+std::to_string(Slot)+".materialinstance",Material),
                    "NOT_FOUND","Starter character material is unavailable");
                Materials.Entries.push_back({Slot,{Material.Metadata.Guid}});
            }
            ReflectedValue MaterialValue;
            Require(ReflectionValueCodec<MaterialSlotOverrides>::ToReflected(Materials,MaterialValue).bOk,
                "INVALID_ARGUMENT","Starter character materials are invalid");
            Commit(MakeSetPropertyCommand(World(),CharacterComponent,PropertyId{"material_slots"},MaterialValue));
            Commands.EndGroup();
        } catch(...) {Commands.CancelGroup();throw;}
        auto* Object=World()->FindByHandle(Created);
        Require(Object!=nullptr,"COMMAND_FAILED","Created character unavailable");
        return {{"id",Object->GetPersistentId()}};
    }
    if(M=="object.create") {
        ObjectHandle Created,Parent;if(P.contains("parentId")&&!P["parentId"].is_null())Parent=Find(P["parentId"])->GetObjectHandle();
        const bool Compound=P.contains("transform")||P.contains("componentType")||P.contains("primitive");
        TypeId ComponentType;
        if(P.contains("componentType")) {
            ComponentType=TypeId{P["componentType"].get<std::string>()};
        }
        AssetKey PrimitiveKey;
        if(P.contains("primitive")) {
            const std::string Shape=P["primitive"];
            const std::string Path=Shape=="cube"?"/Engine/Models/StarterBlock.glb"
                :Shape=="sphere"?"/Engine/Models/PrimitiveSphere.glb"
                :Shape=="cylinder"?"/Engine/Models/PrimitiveCylinder.glb":"";
            Require(!Path.empty()&&(!ComponentType.IsValid()||ComponentType==TypeId{"engine.MeshRendererComponent"}),
                "INVALID_ARGUMENT","Unknown primitive or incompatible component");
            ComponentType=TypeId{"engine.MeshRendererComponent"};
            AssetRegistryEntry Entry;
            Require(Runtime.GetAssetRegistry().TryGetByPath(Path,Entry)&&Entry.Metadata.SubAssets.size()==1
                &&Entry.Metadata.SubAssets.front().Type==StaticMeshAssetType,"NOT_FOUND","Built-in primitive mesh is unavailable");
            PrimitiveKey={Entry.Metadata.Guid,Entry.Metadata.SubAssets.front().Id};
        }
        if(ComponentType.IsValid()) {
            auto* Type=ReflectionSubsystem::Get().FindClass(ComponentType);
            Require(Type&&Type->IsConcrete()&&Type->IsA(TypeId{"engine.Component"}),"INVALID_ARGUMENT","Not a concrete component type");
        }
        if(P.contains("cameraAspect"))Require(ComponentType==TypeId{"engine.CameraComponent"}
            &&P["cameraAspect"].get<double>()>0.01&&P["cameraAspect"].get<double>()<100,
            "INVALID_ARGUMENT","cameraAspect requires a camera component and a positive aspect ratio");
        const bool HasPrimaryCamera=ComponentType==TypeId{"engine.CameraComponent"}&&World()->FindPrimaryCamera()!=nullptr;
        if(Compound)Commands.BeginGroup("Create Object");
        try {
            Commit(MakeCreateObjectCommand(World(),P["name"],Parent,&Created));
            if(P.contains("transform"))Commit(MakeSetTransformCommand(World(),Created,ParseTransform(P["transform"])));
            if(ComponentType.IsValid()) {
                ObjectHandle CreatedComponent;
                Commit(MakeAddComponentCommand(World(),Created,ComponentType,&CreatedComponent));
                if(PrimitiveKey.IsValid()) {
                    Commit(MakeSetPropertyCommand(World(),CreatedComponent,PropertyId{"mesh_asset_id"},
                        ReflectedValue::MakeString(PrimitiveKey.Asset.ToString())));
                    Commit(MakeSetPropertyCommand(World(),CreatedComponent,PropertyId{"mesh_sub_asset_id"},
                        ReflectedValue::MakeString(PrimitiveKey.SubAsset->ToString())));
                }
                if (ComponentType == TypeId{"engine.LightComponent"})
                {
                    ReflectedValue LightType;
                    Diagnostic(ReflectionValueCodec<RenderLightType>::ToReflected(RenderLightType::Point, LightType));
                    Commit(MakeSetPropertyCommand(World(), CreatedComponent, PropertyId{"light_type"}, LightType));
                }
                if(HasPrimaryCamera)Commit(MakeSetPropertyCommand(World(),CreatedComponent,
                    PropertyId{"primary"},ReflectedValue::MakeBool(false)));
                if(P.contains("cameraAspect"))Commit(MakeSetPropertyCommand(World(),CreatedComponent,
                    PropertyId{"aspect_ratio"},ReflectedValue::MakeFloat(P["cameraAspect"].get<float>())));
            }
            if(Compound)Commands.EndGroup();
        } catch(...) {if(Compound)Commands.CancelGroup();throw;}
        auto* O=World()->FindByHandle(Created);
        Require(O!=nullptr,"COMMAND_FAILED","Created object unavailable");return {{"id",O->GetPersistentId()}};
    }
    if (M == "object.copy")
    {
        std::string Subtree;
        const auto Result = SceneSerializer::SerializeSubtreeToJson(*Find(P.at("objectId")), Subtree);
        Require(Result.bOk, "COMMAND_FAILED", Result.Error);
        return {{"subtree", Subtree}};
    }
    if (M == "object.paste")
    {
        ObjectHandle Created;
        Commit(MakePasteSubtreeCommand(World(), P.at("subtree").get<std::string>(), ObjectHandle{}, &Created));
        auto* CreatedObject = World()->FindByHandle(Created);
        Require(CreatedObject != nullptr, "COMMAND_FAILED", "Pasted object unavailable");
        return {{"id", CreatedObject->GetPersistentId()}};
    }
    if(M=="object.duplicate") {
        auto* Source=Find(P["objectId"]);std::string Subtree;
        auto Result=SceneSerializer::SerializeSubtreeToJson(*Source,Subtree);
        Require(Result.bOk,"COMMAND_FAILED",Result.Error);
        const ObjectHandle Parent=Source->GetParent()?Source->GetParent()->GetObjectHandle():ObjectHandle{};
        ObjectHandle Created;Commit(MakePasteSubtreeCommand(World(),std::move(Subtree),Parent,&Created));
        auto* O=World()->FindByHandle(Created);
        Require(O!=nullptr,"COMMAND_FAILED","Duplicated object unavailable");return {{"id",O->GetPersistentId()}};
    }
    if(M=="object.destroy"){auto* O=Find(P["objectId"]);Commit(MakeDeleteObjectCommand(World(),O->GetObjectHandle()));return nullptr;}
    if(M=="object.rename"){auto* O=Find(P["objectId"]);Commit(MakeRenameObjectCommand(World(),O->GetObjectHandle(),P["name"]));return nullptr;}
    if(M=="object.setActive"){auto* O=Find(P["objectId"]);Commit(MakeSetObjectActiveCommand(World(),O->GetObjectHandle(),P["active"]));return nullptr;}
    if(M=="object.setParent") {
        auto* O=Find(P["objectId"]);auto* Parent=P["parentId"].is_null()?nullptr:Find(P["parentId"]);
        Require(!O->WouldCreateParentCycle(Parent),"INVALID_ARGUMENT","Parent cycle");Commit(MakeReparentObjectCommand(World(),O->GetObjectHandle(),Parent?Parent->GetObjectHandle():ObjectHandle{}));return nullptr;
    }
    if(M=="object.getComponents")return Components(Find(P["objectId"]));
    if(M=="component.setEnabled"){
        auto* O=Find(P["objectId"]);auto* C=FindComponent(P);
        Commit(MakeSetComponentEnabledCommand(World(),O->GetObjectHandle(),C->GetObjectHandle(),P["enabled"]));return nullptr;
    }
    if(M=="object.addComponent") {
        auto* O=Find(P["objectId"]);auto* Type=ReflectionSubsystem::Get().FindClass(TypeId{P["typeId"]});
        Require(Type&&Type->IsConcrete()&&Type->IsA(TypeId{"engine.Component"}),"INVALID_ARGUMENT","Not a concrete component type");
        ObjectHandle Created;Commit(MakeAddComponentCommand(World(),O->GetObjectHandle(),Type->Id,&Created));
        for(auto* C:O->GetAllComponents())if(C->GetObjectHandle()==Created)return {{"id",C->GetPersistentId()}};
        throw Failure("COMMAND_FAILED","Created component unavailable");
    }
    if(M=="object.removeComponent") {auto* C=FindComponent(P);Commit(MakeRemoveComponentCommand(World(),Find(P["objectId"])->GetObjectHandle(),C->GetObjectHandle()));return nullptr;}
    if(M=="component.getProperties")return Properties(FindComponent(P));
    if(M=="component.setAsset") {
        auto* C=FindComponent(P);const auto* Field=C->GetClass()->FindProperty(PropertyId{P.at("property")});
        Require(Field&&Editable(*Field)&&(Field->Attributes.AssetTypeFilter||!Field->Attributes.AllowedAssetTypes.empty()),"INVALID_ARGUMENT","Not an editable asset property");
        Guid Id,Sub;const auto Text=P.at("assetId");const auto SubText=P.value("subAssetId",Json(nullptr));
        AssetKey Key;
        if(!Text.is_null()) {
            Require(Guid::TryParse(Text,Id),"INVALID_ARGUMENT","Invalid asset GUID");Key.Asset=Id;
            if(!SubText.is_null()){Require(Guid::TryParse(SubText,Sub),"INVALID_ARGUMENT","Invalid subasset GUID");Key.SubAsset=Sub;}
            AssetRegistryEntry Entry;const SubAssetRecord* Record=nullptr;
            Require(Runtime.GetAssetRegistry().TryResolveKey(Key,Entry,&Record),"NOT_FOUND","Asset reference is not registered");
            const auto Type=Record?Record->Type:Entry.Metadata.Type;auto Allowed=Field->Attributes.AllowedAssetTypes;
            if(Field->Attributes.AssetTypeFilter)Allowed.push_back(Field->Attributes.AssetTypeFilter);
            Require(std::find(Allowed.begin(),Allowed.end(),Type.GetIdentifier())!=Allowed.end(),"INVALID_ARGUMENT","Asset type does not match property");
        } else Require(SubText.is_null(),"INVALID_ARGUMENT","Subasset requires an asset");
        const auto* Companion=Field->Attributes.CompanionProperty?C->GetClass()->FindProperty(PropertyId{Field->Attributes.CompanionProperty}):nullptr;
        Require(!Key.HasSubAsset()||Companion||Field->ValueTypeId.Value=="engine.AssetRef","INVALID_ARGUMENT","Property does not support subassets");
        if (Field->ValueTypeId.Value == "engine.AssetRef")
        {
            ReflectedValue Value;
            Value.ValueKind = ReflectedValue::Kind::AssetReference;
            Value.AssetValue = Key;
            Commit(MakeSetPropertyCommand(World(), C->GetObjectHandle(), Field->Id, Value));
            return Properties(C);
        }
        Commands.BeginGroup("Assign Asset");
        bool Ok=Commands.Execute(MakeSetPropertyCommand(World(),C->GetObjectHandle(),Field->Id,ReflectedValue::MakeString(Key.IsValid()?Id.ToString():"")));
        if(Ok&&Companion)Ok=Commands.Execute(MakeSetPropertyCommand(World(),C->GetObjectHandle(),Companion->Id,ReflectedValue::MakeString(Key.HasSubAsset()?Sub.ToString():"")));
        if(!Ok){Commands.CancelGroup();throw Failure("COMMAND_FAILED","Asset assignment failed");}Commands.EndGroup();return Properties(C);
    }
    if(M=="component.setProperty") {
        auto* C=FindComponent(P);auto* Property=C->GetClass()?C->GetClass()->FindProperty(PropertyId{P["property"]}):nullptr;
        Require(Property&&Editable(*Property),"PROPERTY_READ_ONLY","Property is absent or not editor-editable");
        Require(!Property->Attributes.AssetTypeFilter&&Property->Attributes.AllowedAssetTypes.empty(),"INVALID_ARGUMENT","Use component.setAsset to validate asset and companion identities together");
        ReflectedValue Value;auto Resolver=[&](const std::string& Id)->ObjectHandle{
            for(auto* O:World()->GetAllObjects()){if(O->GetPersistentId()==Id)return O->GetObjectHandle();for(auto* C:O->GetAllComponents())if(C->GetPersistentId()==Id)return C->GetObjectHandle();}
            throw Failure("NOT_FOUND","Referenced object does not exist");};
        Diagnostic(ReflectionJson::JsonToReflectedValue(Property->ValueTypeId,ConvertValue(Property->ValueTypeId,P["value"],true),Value,Resolver));
        Diagnostic(PropertyAccess::ValidatePropertyValue(*Property,Value));
        std::function<void(const ReflectedValue&)> CheckAssets=[&](const ReflectedValue& V){
            if(V.ValueKind==ReflectedValue::Kind::AssetReference&&V.AssetValue.Asset.IsValid()){AssetRegistryEntry Entry;Require(Runtime.GetAssetRegistry().TryResolveKey(V.AssetValue,Entry),"NOT_FOUND","Asset reference is not registered");}
            for(const auto& Child:V.Values)CheckAssets(Child);};CheckAssets(Value);
        Commit(MakeSetPropertyCommand(World(),C->GetObjectHandle(),Property->Id,std::move(Value)));return Properties(C);
    }
    if (M == "transform.edit")
    {
        const std::string Operation = P.at("operation");
        const std::string Transaction = P.at("transaction");
        const std::string ObjectId = P.at("objectId");
        if (Operation == "begin")
        {
            Require(TransformTransaction.empty(), "BUSY", "A transform edit is already active");
            auto* SceneObject = Find(ObjectId);
            TransformBefore = SceneObject->GetTransform();
            TransformTransaction = Transaction;
            TransformObjectId = ObjectId;
            TransformWorld = World()->GetObjectHandle();
            return TransformJson(TransformBefore);
        }
        if (Operation == "cancel" && TransformTransaction.empty())
        {
            return nullptr;
        }
        Require(Transaction == TransformTransaction && ObjectId == TransformObjectId, "STALE_EDIT", "Transform edit is no longer active");
        if (Operation == "cancel")
        {
            CancelTransformEdit();
            return nullptr;
        }
        Require(Operation == "update" || Operation == "commit", "INVALID_ARGUMENT", "Unknown transform edit operation");
        const auto Updated = ParseTransform(P.at("transform"));
        auto* SceneObject = Find(ObjectId);
        if (Operation == "update")
        {
            SceneObject->SetTransform(Updated);
        }
        else
        {
            const bool Changed = TransformJson(Updated) != TransformJson(TransformBefore);
            CancelTransformEdit();
            if (Changed)
            {
                Commit(MakeSetTransformCommand(World(), SceneObject->GetObjectHandle(), Updated));
            }
        }
        return TransformJson(SceneObject->GetTransform());
    }
    if(M=="transform.get")return TransformJson(Find(P["objectId"])->GetTransform());
    if(M=="transform.set"){auto* O=Find(P["objectId"]);Commit(MakeSetTransformCommand(World(),O->GetObjectHandle(),ParseTransform(P["transform"])));return TransformJson(O->GetTransform());}
    if(M=="selection.set"){if(P["objectId"].is_null())Workspace.ClearSelection();else Workspace.Select(World(),Find(P["objectId"])->GetObjectHandle());return Describe();}
    if(M=="editor.undo"){Require(Commands.Undo(),"COMMAND_FAILED","Nothing to undo or stale command");return Describe();}
    if(M=="editor.redo"){Require(Commands.Redo(),"COMMAND_FAILED","Nothing to redo or stale command");return Describe();}
    if (M == "audio.setStartup")
    {
        Require(!ProjectChanged(), "EXTERNAL_CHANGES", "Project descriptor changed externally");
        std::filesystem::path Audio;
        if (P.contains("assetId"))
        {
            const auto Entry = Asset(P.at("assetId"));
            Require(Entry.Metadata.Type == AudioAssetType && Entry.Mount == AssetMount::Game,
                "INVALID_ARGUMENT", "Expected project WAV asset");
            Audio = Entry.AbsolutePath;
        }
        std::string Error;
        Require(Project.SetStartupAudio(Audio, Error), "IO_ERROR", Error);
        AssetDiagnostic Diagnostic;
        ContentHash::TryHashFile(Project.GetProject()->ProjectFile.u8string(), ProjectFingerprint, Diagnostic);
        return Describe();
    }
    if(M=="audio.resolveStartup") {
        Require(Project.GetProject()&&!Project.GetProject()->StartupAudio.empty(),"NOT_FOUND","No startup audio");
        const auto Relative=Project.GetProject()->StartupAudio.lexically_relative(ProjectPaths::Content());
        const auto Path=ContentPath(Relative.generic_string());
        Require(Path.extension()==".wav"&&std::filesystem::is_regular_file(Path),"NOT_FOUND","Startup WAV is missing");
        return {{"absolutePath",Path.generic_string()}};
    }
    if(M=="play.start") {
        if(Play.GetState()==PlaySessionState::Paused)Play.Resume();
        else {
            Require(Play.StartPlay(),"COMMAND_FAILED","PlaySession failed to start");
            auto& Playback=Runtime.GetStoryRuntime();Playback.BindScene(Play.GetPlayWorld());
            const bool HasStory=Story.IsOpen()||(Project.GetProject()&&!Project.GetProject()->StartupStory.empty());
            const bool Loaded=!HasStory||(Story.IsOpen()?Playback.LoadDocument(Story.GetStory()):Playback.LoadFromFile(Project.GetProject()->StartupStory));
            if(!Loaded){const auto Error=Playback.GetLastError();Playback.BindScene(nullptr);Play.StopPlay();throw Failure("STORY_ERROR",Error);}
        }
        Workspace.ClearSelection();return Describe();
    }
    if(M=="play.advanceDialogue") {Require(Play.GetState()==PlaySessionState::Playing,"INVALID_STATE","Play is not running");Require(Runtime.GetStoryRuntime().AdvanceDialogue(),"INVALID_STATE","Dialogue cannot advance");return Describe();}
    if(M=="play.selectChoice") {Require(Play.GetState()==PlaySessionState::Playing,"INVALID_STATE","Play is not running");const auto Index=P.at("index").get<double>();Require(Index>=0&&std::floor(Index)==Index&&Index<Runtime.GetStoryRuntime().GetChoiceCount(),"INVALID_ARGUMENT","Invalid choice index");Require(Runtime.GetStoryRuntime().SelectChoice(size_t(Index)),"INVALID_STATE","Choice is unavailable");return Describe();}
    if(M=="play.pause"){Require(Play.IsSimulating(),"INVALID_STATE","Play is stopped");Play.Pause();return Describe();}
    if(M=="play.step"){Require(Play.GetState()==PlaySessionState::Paused,"INVALID_STATE","Pause before Step");Play.StepFrame(1.f/60.f);return Describe();}
    if(M=="play.stop"){Runtime.GetStoryRuntime().BindScene(nullptr);Play.StopPlay();Workspace.ClearSelection();return Describe();}
    if(M=="reflection.getComponentTypes") {
        Json A=Json::array();for(auto* C:ReflectionSubsystem::Get().GetPublishedCatalog().Classes)if(C->IsConcrete()&&C->IsA(TypeId{"engine.Component"}))A.push_back({{"typeId",C->Id.Value},{"displayName",C->GetDisplayName()},{"version",C->SchemaVersion}});return A;
    }
    if(M=="assets.getScenes") {
        Json A=Json::array();for(const auto& E:Runtime.GetAssetRegistry().FindByType(SceneAssetType))A.push_back({{"id",E.Metadata.Guid.ToString()},{"path",E.VirtualPath}});return A;
    }
    throw Failure("UNKNOWN_METHOD",M);
}
Json NativeBridge::Execute(const Json& Request) {
    AssertGameThread();ObserveWorld();Json Id=Request.is_object()?Request.value("id",Json(nullptr)):Json(nullptr);
    Json Result{{"type","bridge.result"},{"version",1},{"id",Id},{"ok",false}};
    try {
        Require(Request.is_object()&&Request.size()==7&&Request.value("type",std::string{})=="bridge.command"&&Request.value("version",0)==1&&Id.is_string()&&Id.get_ref<const std::string&>().size()<=128,"PROTOCOL_ERROR","Invalid request envelope");
        Require(Request.at("session")==Session,"STALE_SESSION","Engine process has changed");
        Require(Request.at("epoch")==Epoch,"STALE_SCENE","Scene/world has changed; refresh context");
        const std::string M=Request.at("method");ValidateParams(M,Request.at("params"));
        if (!TransformTransaction.empty() && M != "transform.edit" && M != "transform.get" && M != "bridge.describe" && M != "scene.getHierarchy" && M != "object.getComponents" && M != "component.getProperties" && M != "reflection.getComponentTypes" && M != "assets.list" && M != "assets.getScenes" && M != "editor.actions" && M != "mesh.inspect" && M != "story.get" && M != "assets.folders" && M != "assets.creationTypes" && M != "assets.resolve" && M != "assets.thumbnail" && M != "audio.resolveStartup")
        {
            CancelTransformEdit();
        }
        Result["value"]=Dispatch(M,Request.at("params"));Result["ok"]=true;
    }catch(const Failure& E){Result["error"]={{"code",E.Code},{"message",E.what()}};}
    catch(const std::exception& E){Result["error"]={{"code","INVALID_ARGUMENT"},{"message",E.what()}};}
    ObserveWorld();
    Result["context"]=Context();return Result;
}
Json NativeBridge::Poll(bool Force) {
    CpuProfileScope CpuMeasurement("Bridge.Poll");
    AssertGameThread();ObserveWorld();
    Json State=Describe();State.erase("context");State["hierarchy"]=Hierarchy();
    // Observe native gizmo/gameplay/property edits too; no JS mirror is authoritative.
    Json Values = Json::array();
    for (auto* Object : World()->GetAllObjects())
    {
        for (auto* ComponentInstance : Object->GetAllComponents())
        {
            Json ComponentValues = Json::array();
            if (ComponentInstance->GetClass())
            {
                for (const auto* Property : ComponentInstance->GetClass()->GetAllProperties())
                {
                    if (!Readable(*Property))
                    {
                        continue;
                    }
                    ReflectedValue Value;
                    auto Diagnostic = PropertyAccess::GetProperty(ComponentInstance, *Property, Value, PropertyAccessContext::Inspector);
                    Json SerializedValue;
                    if (Diagnostic.bOk)
                    {
                        Diagnostic = ReflectionJson::ReflectedValueToJson(Property->ValueTypeId, Value, SerializedValue);
                    }
                    if (!Diagnostic.bOk)
                    {
                        SerializedValue = {{"error", Diagnostic.Message}};
                    }
                    ComponentValues.push_back(std::move(SerializedValue));
                }
            }
            Values.push_back(std::move(ComponentValues));
        }
    }
    State["properties"] = std::move(Values);
    auto StoryState=StorySnapshot();StoryState.erase("revision");const auto StoryText=StoryState.dump(),AssetText=Assets().dump();
    const bool StoryChanged=StoryText!=PreviousStory,AssetsChanged=AssetText!=PreviousAssets;
    if(StoryChanged){PreviousStory=StoryText;++StoryRevision;}PreviousAssets=AssetText;
    auto Serialized=State.dump();const bool SceneChanged=Serialized!=PreviousState;
    if(!Force&&!SceneChanged&&!StoryChanged&&!AssetsChanged)return nullptr;PreviousState=std::move(Serialized);++Revision;
    return {{"type","bridge.event"},{"version",1},{"event","state.changed"},{"context",Context()},{"state",Describe()},
        {"changes",{{"scene",SceneChanged},{"assets",AssetsChanged},{"story",StoryChanged}}}};
}

AssetRegistryEntry NativeBridge::Asset(const std::string& Id) {
    Guid Guid;AssetRegistryEntry Entry;Require(Guid::TryParse(Id,Guid)&&Runtime.GetAssetRegistry().TryGetById(Guid,Entry),"NOT_FOUND","Asset no longer exists");return Entry;
}
void NativeBridge::ScanAssets()
{
    CpuProfileScope CpuMeasurement("Bridge.ScanAssets");
    AssertGameThread();
    auto& Registry = Runtime.GetAssetRegistry();
    std::map<std::string, Json> Previous;
    for (const auto& Type : Registry.GetAssetTypeRegistrations())
    {
        for (const auto& Entry : Registry.FindByType(Type.Type)) { Previous[Entry.Metadata.Guid.ToString()] = AssetMetadataIO::ToJson(Entry.Metadata); }
    }
    const auto Result = Registry.ScanContent();
    AssetError.clear();
    if (Result.HasError()) { AssetError = Result.Message; }
    for (const auto& Type : Registry.GetAssetTypeRegistrations())
    {
        for (const auto& Entry : Registry.FindByType(Type.Type))
        {
            const auto Found = Previous.find(Entry.Metadata.Guid.ToString());
            if (Found != Previous.end() && Found->second != AssetMetadataIO::ToJson(Entry.Metadata))
            {
                Runtime.GetAssetManager().InvalidateAsset(Entry.Metadata.Guid);
                Previews.clear();
            }
        }
    }
}
Json NativeBridge::Assets() {
    CpuProfileScope CpuMeasurement("Bridge.Assets");
    Json Result=Json::array();auto& Registry=Runtime.GetAssetRegistry();
    for(const auto& Type:Registry.GetAssetTypeRegistrations())
        for(const auto& E:Registry.FindByType(Type.Type)) {
            std::error_code Error;auto Bytes=std::filesystem::file_size(E.AbsolutePath,Error);const auto Size=Error?0:Bytes;
            const auto Modified=std::filesystem::last_write_time(E.AbsolutePath,Error);
            auto Version=Error?std::string{}:std::to_string(Modified.time_since_epoch().count());
            const auto MetadataModified = std::filesystem::last_write_time(E.AbsoluteMetaPath, Error);
            if (!Error) { Version += ":" + std::to_string(MetadataModified.time_since_epoch().count()); }
            Json Sub=Json::array();
            for(const auto& S:E.Metadata.SubAssets)Sub.push_back({{"id",S.Id.ToString()},{"typeId",S.Type.GetIdentifier()},{"name",S.Name}});
            Result.push_back({{"id",E.Metadata.Guid.ToString()},{"name",std::filesystem::path(E.RelativePath).filename().u8string()},
                {"path",E.VirtualPath},{"relativePath",E.RelativePath},{"typeId",E.Metadata.Type.GetIdentifier()},
                {"builtin",E.Mount==AssetMount::Engine},{"browserVisible",Type.bVisibleInContentBrowser||Type.Type==ModelAssetType},{"bytes",Size},{"version",Version},{"subassets",Sub}});
        }
    std::sort(Result.begin(),Result.end(),[](const Json& A,const Json& B){return A.at("path")<B.at("path");});return Result;
}
Json NativeBridge::StorySnapshot() {
    CpuProfileScope CpuMeasurement("Bridge.Story");
    if(!Story.IsOpen())return {{"open",false},{"revision",StoryRevision},{"error",StoryError}};
    std::string Text;auto D=StoryDocumentIO::SaveToJson(Story.GetStory(),Text);Require(D.bOk,"STORY_ERROR",D.Error);
    Json Positions=Json::object();for(const auto& [Id,P]:Story.GetPositions())Positions[Id]={{"x",P.X},{"y",P.Y}};
    Json Catalog = Json::array();
    for (const auto& Definition : GetNarrativeDefinitions())
    {
        Json Entry{{"type", Definition.Type}, {"label", Definition.Label}, {"pure", Definition.bPure},
            {"exits", Definition.Exits}, {"inputs", Json::array()}, {"outputs", Json::array()}};
        const auto AddPins = [&](const char* Name, const std::vector<NarrativePin>& Pins)
        {
            for (const auto& Pin : Pins)
            {
                Json Value{{"name", Pin.Property.Id.Value}, {"type", Pin.Property.ValueTypeId.Value}};
                if (Pin.bHasDefault)
                {
                    Value["default"] = Pin.Default;
                }
                Entry[Name].push_back(std::move(Value));
            }
        };
        AddPins("inputs", Definition.Inputs);
        AddPins("outputs", Definition.Outputs);
        Catalog.push_back(std::move(Entry));
    }
    return {{"open",true},{"narrativeCatalog",Catalog},{"revision",StoryRevision},{"document",Json::parse(Text)},{"positions",Positions},
        {"path",Story.GetPath().generic_string()},{"dirty",Story.IsDirty()},{"readOnly",Story.IsReadOnly()},
        {"externalChanges",Story.HasExternalChanges()},{"canUndo",Story.CanUndo()},{"canRedo",Story.CanRedo()}};
}
Json NativeBridge::DispatchDocuments(const std::string& M,const Json& P) {
    auto& Registry=Runtime.GetAssetRegistry();std::string Error;
    if(M=="assets.thumbnail")
    {
        const auto Entry = Asset(P.at("assetId"));
        AssetKey Key{Entry.Metadata.Guid, std::nullopt};
        if (P.contains("subAssetId") && !P.at("subAssetId").is_null())
        {
            Guid Identifier;
            Require(Guid::TryParse(P.at("subAssetId"), Identifier), "INVALID_ARGUMENT", "Invalid subasset");
            Key.SubAsset = Identifier;
        }
        AssetRegistryEntry Resolved;
        Require(Registry.TryResolveKey(Key, Resolved), "NOT_FOUND", "Asset or subasset unavailable");
        const std::string CacheKey = Key.Asset.ToString() + P.value("subAssetId", Json(nullptr)).dump()
            + std::to_string(std::filesystem::last_write_time(Entry.AbsolutePath).time_since_epoch().count())
            + AssetMetadataIO::ToJson(Entry.Metadata).dump();
        auto Found = Previews.find(CacheKey);
        if (Found == Previews.end())
        {
            if (Previews.size() >= 256) { Previews.clear(); }
            auto Result = std::make_shared<PreviewResult>();
            Previews.emplace(CacheKey, Result);
            auto Snapshot = std::make_shared<AssetRegistry>(Registry);
            JobSystem::Get()->Schedule([Result, Snapshot, Key]()
            {
                std::string Encoded;
                try
                {
                    ImageBuffer Image = GenerateAssetPreview(*Snapshot, Key);
                    if (!Image.Pixels.empty())
                    {
                        const auto Bytes = Image.ResizeToFit(128, 128).EncodePng();
                        Encoded = Base64::Encode(Bytes.data(), Bytes.size());
                    }
                }
                catch (const std::exception& Error)
                {
                    PrintString(std::string("Asset preview failed: ") + Error.what());
                }
                std::lock_guard<std::mutex> Lock(Result->Mutex);
                Result->Encoded = std::move(Encoded);
                Result->bReady = true;
            });
            return {{"ready", false}};
        }
        std::lock_guard<std::mutex> Lock(Found->second->Mutex);
        return {{"ready", Found->second->bReady}, {"png", Found->second->Encoded}};
    }
    if (M == "assets.creationTypes")
    {
        auto Factories = AssetCreationFactory::DescribeFactories();
        for (auto& Factory : Factories)
        {
            for (auto& Field : Factory["settings"])
            {
                Field["value"] = ConvertValue(TypeId{Field.at("typeId").get<std::string>()}, Field.at("value"), false);
            }
        }
        return Factories;
    }
    if (M == "assets.folders")
    {
        Json Folders = {"Game", "Engine"};
        for (const auto& Mount : {std::make_pair(Registry.GetGameContentRoot(), "Game/"), std::make_pair(Registry.GetEngineContentRoot(), "Engine/")})
        {
            std::error_code DirectoryError;
            for (std::filesystem::recursive_directory_iterator Directory(Mount.first, std::filesystem::directory_options::skip_permission_denied, DirectoryError), End; Directory != End && !DirectoryError; Directory.increment(DirectoryError))
            {
                if (Directory->is_directory() && !Directory->is_symlink())
                {
                    Folders.push_back(std::string(Mount.second) + Directory->path().lexically_relative(Mount.first).generic_u8string());
                }
                if (Directory->is_symlink())
                {
                    Directory.disable_recursion_pending();
                }
            }
        }
        return Folders;
    }
    if(M=="assets.list")return Assets();
    if(M=="assets.getScenes") {Json A=Json::array();for(const auto& E:Registry.FindByType(SceneAssetType))A.push_back({{"id",E.Metadata.Guid.ToString()},{"path",E.VirtualPath}});return A;}
    if(M=="assets.resolve") {
        const auto E=Asset(P.at("assetId"));return {{"id",E.Metadata.Guid.ToString()},{"path",E.VirtualPath},{"relativePath",E.RelativePath},
            {"typeId",E.Metadata.Type.GetIdentifier()},{"absolutePath",E.AbsolutePath},{"builtin",E.Mount==AssetMount::Engine}};
    }
    if(M=="assets.refresh"){ScanAssets();Require(AssetError.empty(),"ASSET_ERROR",AssetError);return Assets();}
    if(M=="story.get")return StorySnapshot();
    Require(!Runtime.GetPlaySession().IsSimulating(),"PLAY_READ_ONLY","Stop Play before editing assets or story");
    if (M == "assets.rename" || M == "assets.move" || M == "assets.delete" || M == "assets.duplicate"
        || M == "assets.createFolder" || M == "assets.renameFolder" || M == "assets.deleteFolder")
    {
        AssetOperations Operations(Runtime);
        Operations.SetProjectContext(&Project, &Workspace.GetDocument());
        AssetDiagnostic Result;
        Json Response = Json::object();
        const auto ValidateName = [&](const std::string& Name)
        {
            Require(!Name.empty() && Name.find_first_of("./\\:*?\"<>|") == std::string::npos && Name.back() != ' ', "INVALID_ARGUMENT", "Enter a single valid name without an extension");
        };
        const auto IsFolderOperation = M == "assets.createFolder" || M == "assets.renameFolder" || M == "assets.deleteFolder";
        std::filesystem::path AffectedPath;
        if (IsFolderOperation)
        {
            const std::string Folder = P.at("folder");
            Require(Operations.ResolveGameFolder(Folder, AffectedPath), "INVALID_PATH", "Choose a Game Content folder");
            if (M == "assets.createFolder")
            {
                const std::string Name = P.at("name");
                ValidateName(Name);
                const auto Destination = Folder + "/" + Name;
                Result = Operations.CreateFolder(Destination);
                Response["folder"] = Destination;
            }
            else
            {
                Require(!Story.IsOpen() || !AssetPath::IsInsideContent(Story.GetPath(), AffectedPath), "DOCUMENT_OPEN", "Close the story document before modifying its folder");
                for (const auto& Document : AssetDocuments)
                {
                    AssetKey DocumentAsset;
                    if (Document.second.Material)
                    {
                        DocumentAsset = Document.second.Material->GetDocument().GetAsset();
                    }
                    else if (Document.second.Mesh)
                    {
                        DocumentAsset = Document.second.Mesh->GetAsset();
                    }
                    AssetRegistryEntry DocumentEntry;
                    if (Registry.TryResolveKey(DocumentAsset, DocumentEntry))
                    {
                        Require(!AssetPath::IsInsideContent(DocumentEntry.AbsolutePath, AffectedPath), "DOCUMENT_OPEN", "Close affected asset documents before modifying their folder");
                    }
                }
                if (M == "assets.renameFolder")
                {
                    const std::string Name = P.at("name");
                    ValidateName(Name);
                    const auto Destination = std::filesystem::u8path(Folder).parent_path().generic_u8string() + "/" + Name;
                    Result = Operations.RenameFolder(Folder, Destination);
                    Response["folder"] = Destination;
                }
                else
                {
                    Result = Operations.DeleteFolder(Folder);
                    Response["folder"] = std::filesystem::u8path(Folder).parent_path().generic_u8string();
                }
            }
        }
        else
        {
            const auto Entry = Asset(P.at("assetId"));
            const AssetKey Key{Entry.Metadata.Guid, {}};
            if (M != "assets.duplicate")
            {
                Require(!Story.IsOpen() || Story.GetAsset().Asset != Key.Asset, "DOCUMENT_OPEN", "Close the story document before modifying its asset");
                for (const auto& Document : AssetDocuments)
                {
                    Require(!Document.second.Material || Document.second.Material->GetDocument().GetAsset().Asset != Key.Asset, "DOCUMENT_OPEN", "Close the asset document before modifying its file");
                    Require(!Document.second.Mesh || Document.second.Mesh->GetAsset().Asset != Key.Asset, "DOCUMENT_OPEN", "Close the mesh document before modifying its file");
                }
            }
            if (M == "assets.delete")
            {
                Result = Operations.DeleteAsset(Key);
            }
            else if (M == "assets.duplicate")
            {
                std::set<std::string> Previous;
                for (const auto& Existing : Registry.FindByType(Entry.Metadata.Type))
                {
                    Previous.insert(Existing.Metadata.Guid.ToString());
                }
                Result = Operations.DuplicateAsset(Key, P.at("folder").get<std::string>());
                for (const auto& Existing : Registry.FindByType(Entry.Metadata.Type))
                {
                    if (Previous.find(Existing.Metadata.Guid.ToString()) == Previous.end())
                    {
                        Response = {{"id", Existing.Metadata.Guid.ToString()}, {"path", Existing.VirtualPath}};
                        break;
                    }
                }
            }
            else
            {
                std::string Destination;
                if (M == "assets.rename")
                {
                    const std::string Name = P.at("name");
                    ValidateName(Name);
                    const auto Source = std::filesystem::u8path(Entry.VirtualPath);
                    Destination = (Source.parent_path() / std::filesystem::u8path(Name + Source.extension().u8string())).generic_u8string();
                }
                else
                {
                    Destination = P.at("folder").get<std::string>() + "/" + std::filesystem::u8path(Entry.VirtualPath).filename().u8string();
                }
                Result = Operations.RenameAsset(Entry.VirtualPath, Destination);
                Response = {{"id", Key.Asset.ToString()}, {"path", Destination}};
            }
        }
        Require(!Result.HasError(), "ASSET_ERROR", Result.Message);
        Previews.clear();
        ScanAssets();
        return Response;
    }
    if (M == "assets.create")
    {
        const std::string Relative = P.at("path");
        ContentPath(Relative);
        AssetRegistryEntry Created;
        const TypeId FactoryType{P.at("factoryId").get<std::string>()};
        auto Settings = P.value("settings", Json::object());
        const auto* Descriptor = ReflectionSubsystem::Get().FindClass(FactoryType);
        if (Descriptor && Settings.is_object())
        {
            for (auto Setting = Settings.begin(); Setting != Settings.end(); ++Setting)
            {
                const auto* Property = Descriptor->FindProperty(PropertyId{Setting.key()});
                if (Property) { Setting.value() = ConvertValue(Property->ValueTypeId, Setting.value(), true); }
            }
        }
        Require(AssetCreationFactory::CreateAsset(Registry, FactoryType,
            "/Game/" + Relative, Settings, Created, Error), "ASSET_ERROR", Error);
        return {{"id", Created.Metadata.Guid.ToString()}, {"path", Created.VirtualPath}, {"typeId", Created.Metadata.Type.GetIdentifier()}};
    }
    if(M=="assets.import") {
        // Main process obtains this path from the OS file picker, never from renderer input.
        auto Source=std::filesystem::u8path(P.at("sourcePath").get<std::string>());Require(std::filesystem::is_regular_file(Source),"INVALID_PATH","Import source does not exist");
        const auto Destination=P.at("destination").get<std::string>();ContentPath(Destination);
        ImportRequest Request;Request.SourcePath=Source;Request.DestinationRelativePath=Destination;
        Request.StagingDirectory=Project.GetProject()->ProjectRoot/".sacura-import";
        AssetImporter Importer(Registry);const auto Result=Importer.Import(Request);Require(Result.Succeeded(),"IMPORT_ERROR",Result.Diagnostic.Message);
        return {{"id",Result.Metadata.Guid.ToString()},{"path",Result.PublishedRelativePath}};
    }
    if(M=="story.open") {
        Require(!Story.IsDirty()||P.value("discard",false),"UNSAVED_CHANGES","Save the open story before opening another");
        auto E=Asset(P.at("assetId"));Require(E.Metadata.Type==StoryAssetType,"INVALID_ARGUMENT","Expected Story asset");
        Require(Story.Open(Registry,AssetKey{E.Metadata.Guid,{}},Error),"STORY_ERROR",Error);++Epoch;return StorySnapshot();
    }
    if(M=="story.create") {
        Require(!Story.IsDirty(),"UNSAVED_CHANGES","Save the current story first");const auto Relative=P.at("path").get<std::string>();const auto Path=ContentPath(Relative);
        Require(Path.extension()==".story","INVALID_PATH","Expected .story file");
        std::error_code DirectoryError;std::filesystem::create_directories(Path.parent_path(),DirectoryError);
        Require(!DirectoryError,"IO_ERROR",DirectoryError.message());
        AssetKey Created;auto D=StoryAssetCreation::Create(Registry,"/Game/"+Relative,Created);Require(!D.HasError(),"ASSET_ERROR",D.Message);
        Require(Story.Open(Registry,Created,Error),"STORY_ERROR",Error);++Epoch;
        StoryNodeId FirstLine;
        Require(Story.AddNode(StoryNodeKind::Line,{0,0},FirstLine,Error),"STORY_ERROR",Error);
        Require(Story.Save(Error),"STORY_ERROR",Error);
        return StorySnapshot();
    }
    Require(Story.IsOpen(),"NO_STORY","Open a Story asset first");
    if(M=="story.save"){Require(Story.Save(Error),"STORY_ERROR",Error);return StorySnapshot();}
    if(M=="story.undo"){Require(Story.Undo(),"COMMAND_FAILED","Story Undo is unavailable");return StorySnapshot();}
    if(M=="story.redo"){Require(Story.Redo(),"COMMAND_FAILED","Story Redo is unavailable");return StorySnapshot();}
    if (M == "story.addNarrative")
    {
        StoryNodeId Created;
        Require(Story.AddNarrativeNode(P.at("type"), {P.at("x"), P.at("y")}, Created, Error), "STORY_ERROR", Error);
        return StorySnapshot();
    }
    if (M == "story.setNarrativeInput")
    {
        Require(Story.SetNarrativeInput(P.at("nodeId"), P.at("input"), P.at("value"), Error,
            ReflectionJson::MakeSceneResolver(*Runtime.GetEditScene())), "STORY_ERROR", Error);
        return StorySnapshot();
    }
    if (M == "story.connectData")
    {
        Require(Story.ConnectData(P.at("sourceId"), P.at("output"), P.at("targetId"), P.at("input"), Error), "STORY_ERROR", Error);
        return StorySnapshot();
    }
    if (M == "story.validate")
    {
        Require(Story.ValidateForPlayback(Error), "STORY_ERROR", Error);
        return StorySnapshot();
    }
    if(M=="story.addNode") {
        const auto Kind=P.at("kind").get<std::string>();StoryNodeKind K;
        if(Kind=="Line")K=StoryNodeKind::Line;else if(Kind=="Choice")K=StoryNodeKind::Choice;else if(Kind=="Action")K=StoryNodeKind::Action;else if(Kind=="End")K=StoryNodeKind::End;else throw Failure("INVALID_ARGUMENT","Unsupported story node kind");
        StoryNodeId Created;Require(Story.AddNode(K,{P.at("x"),P.at("y")},Created,Error),"STORY_ERROR",Error);return StorySnapshot();
    }
    if(M=="story.deleteNode"){Require(Story.DeleteNodes({P.at("nodeId")},Error),"STORY_ERROR",Error);return StorySnapshot();}
    if(M=="story.connect") {
        const auto Port=P.at("port").get<double>();Require(Port==std::floor(Port)&&Port>=-1&&Port<4096,"INVALID_ARGUMENT","Invalid story port");
        Require(Story.Connect(P.at("sourceId"),int(Port),P.at("targetId").is_null()?std::string{}:P.at("targetId").get<std::string>(),Error),"STORY_ERROR",Error);return StorySnapshot();
    }
    if(M=="story.setStart"){Require(Story.SetStart(P.at("nodeId"),Error),"STORY_ERROR",Error);return StorySnapshot();}
    if(M=="story.moveNodes") {
        Require(P.at("positions").is_object(),"INVALID_ARGUMENT","Expected positions map");std::map<StoryNodeId,StoryGraphPosition> Positions;
        for(const auto& [Id,Point]:P.at("positions").items()){
            Require(Point.is_object()&&Point.size()==2&&Point.contains("x")&&Point.contains("y")&&Point.at("x").is_number()&&Point.at("y").is_number(),"INVALID_ARGUMENT","Invalid graph position");
            const double X=Point.at("x"),Y=Point.at("y");Require(std::abs(X)<1e7&&std::abs(Y)<1e7,"INVALID_ARGUMENT","Graph position out of range");Positions[Id]={X,Y};}
        Require(Story.MoveNodes(Positions,Error),"STORY_ERROR",Error);return StorySnapshot();
    }
    if(M=="story.updateNode"||M=="story.updateEvent") {
        // Decode through the runtime serializer, then commit through the existing document command.
        auto Encoded=StorySnapshot().at("document");bool Found=false;
        if(M=="story.updateNode") {
            const auto& Patch=P.at("patch");Require(Patch.is_object(),"INVALID_ARGUMENT","Expected node fields");
            const std::set<std::string> Allowed={"kind","speaker","text","prompt","choices","actions"};
            for(auto It=Patch.begin();It!=Patch.end();++It)Require(Allowed.count(It.key())!=0,"INVALID_ARGUMENT","Unsupported node field");
            for(auto& Node:Encoded["nodes"])if(Node.at("id")==P.at("nodeId")){Node.update(Patch);Found=true;break;}
        } else {
            Require(P.at("event").is_object()&&P.at("event").value("id",std::string{})==P.at("eventId"),"INVALID_ARGUMENT","Event identity cannot change");
            for(auto& Event:Encoded["events"])if(Event.at("id")==P.at("eventId")){Event=P.at("event");Found=true;break;}
        }
        Require(Found,"NOT_FOUND","Story node/event no longer exists");StoryDocument Decoded;
        const auto D=StoryDocumentIO::LoadFromJson(Encoded.dump(),Decoded);Require(D.bOk,"STORY_ERROR",D.Error);
        if(M=="story.updateNode"){for(const auto& N:Decoded.Nodes)if(N.Id==P.at("nodeId")){Require(Story.ReplaceNode(N,Error),"STORY_ERROR",Error);break;}}
        else for(const auto& E:Decoded.Events)if(E.Id==P.at("eventId")){Require(Story.ReplaceEvent(E,Error),"STORY_ERROR",Error);break;}
        return StorySnapshot();
    }
    if(M=="story.setOverrides") {
        const auto& Actions=P.at("actions");Require(Actions.is_array(),"INVALID_ARGUMENT","Expected actions array");
        auto Encoded=StorySnapshot().at("document");bool Found=false;
        for(auto& Node:Encoded["nodes"])if(Node.at("id")==P.at("nodeId")&&Node.contains("bindings"))
            for(auto& Binding:Node["bindings"])if(Binding.at("id")==P.at("bindingId")){
                Binding["actionOverrides"]=Json::object();
                for(const auto& Action:Actions){Require(Action.is_object()&&Action.contains("id")&&Action.at("id").is_string(),"INVALID_ARGUMENT","Override needs an action ID");Binding["actionOverrides"][Action.at("id").get<std::string>()]=Action;}
                Found=true;break;
            }
        Require(Found,"NOT_FOUND","Story placement no longer exists");StoryDocument Decoded;
        const auto D=StoryDocumentIO::LoadFromJson(Encoded.dump(),Decoded);Require(D.bOk,"STORY_ERROR",D.Error);
        for(const auto& Node:Decoded.Nodes)if(Node.Id==P.at("nodeId"))for(const auto& Batch:Node.EventBatches)
            for(const auto& Binding:Batch.Bindings)if(Binding.Id==P.at("bindingId")){
                Require(Story.SetEventOverrides(Node.Id,Binding.Id,Binding.ActionOverrides,Error),"STORY_ERROR",Error);return StorySnapshot();
            }
        throw Failure("NOT_FOUND","Story placement no longer exists");
    }
    auto Phase=[&](){const std::string S=P.at("phase");if(S=="BEFORE")return StoryEventPhase::Before;if(S=="ON_START")return StoryEventPhase::OnStart;Require(S=="AFTER","INVALID_ARGUMENT","Unknown event phase");return StoryEventPhase::After;};
    if(M=="story.addEvent"){
        const auto Kind=P.value("actionKind",std::string{"Wait"});StoryActionKind K;
        if(Kind=="Wait")K=StoryActionKind::Wait;else if(Kind=="MoveTo")K=StoryActionKind::MoveTo;
        else if(Kind=="CameraCut")K=StoryActionKind::CameraCut;else if(Kind=="NavigateTo")K=StoryActionKind::NavigateTo;
        else throw Failure("INVALID_ARGUMENT","Unsupported story action kind");
        Require(!P.contains("eventId")||!P.contains("actionKind"),"INVALID_ARGUMENT","Choose an existing event or a new action kind");
        std::string Id;Require(Story.AddEvent(P.at("nodeId"),Phase(),P.value("eventId",std::string{}),Id,Error,P.value("batchId",std::string{}),K),"STORY_ERROR",Error);return StorySnapshot();
    }
    if(M=="story.removeEvent"){Require(Story.RemoveEventBinding(P.at("nodeId"),P.at("bindingId"),Error),"STORY_ERROR",Error);return StorySnapshot();}
    if(M=="story.batchMode"){Require(Story.SetEventBatchMode(P.at("nodeId"),P.at("batchId"),P.at("parallel"),Error),"STORY_ERROR",Error);return StorySnapshot();}
    if(M=="story.placeEvent") {
        const std::string J=P.at("join");StoryEventJoin Join;
        if(J=="NONE")Join=StoryEventJoin::None;else if(J=="STARTED")Join=StoryEventJoin::Started;else if(J=="FLOW_END")Join=StoryEventJoin::FlowEnd;else {Require(J=="EVENT_END","INVALID_ARGUMENT","Unknown event join");Join=StoryEventJoin::EventEnd;}
        Require(Story.SetEventPlacement(P.at("nodeId"),P.at("bindingId"),Phase(),Join,Error,P.value("batchId",std::string{}),P.value("beforeId",std::string{})),"STORY_ERROR",Error);return StorySnapshot();
    }
    throw Failure("UNKNOWN_METHOD",M);
}

Json NativeBridge::MeshTools(const std::string& Method, const Json& Params)
{
    auto& Registry = Runtime.GetAssetRegistry();
    std::string Error;
    if (Method == "assets.place")
    {
        Require(!Runtime.GetPlaySession().IsSimulating(), "PLAY_READ_ONLY", "Stop Play before placing assets");
        const auto Entry = Asset(Params.at("assetId"));
        Guid Subasset;
        Require(Guid::TryParse(Params.at("subAssetId"), Subasset), "INVALID_ARGUMENT", "Invalid mesh subasset");
        Transform Placement;
        if (Params.contains("transform")) { Placement = ParseTransform(Params.at("transform")); }
        ObjectHandle Created;
        Commit(MakeCreateStaticMeshObjectCommand(World(), Registry, {Entry.Metadata.Guid, Subasset},
            std::filesystem::path(Entry.RelativePath).stem().u8string(), {}, Placement, &Created));
        auto* Object = World()->FindByHandle(Created);
        Workspace.Select(World(), Object->GetObjectHandle());
        return {{"id", Object->GetPersistentId()}};
    }
    auto* Mesh = dynamic_cast<MeshRendererComponent*>(FindComponent(Params));
    Require(Mesh != nullptr, "INVALID_ARGUMENT", "Select a mesh component");
    if (Method == "mesh.material")
    {
        Require(!Runtime.GetPlaySession().IsSimulating(), "PLAY_READ_ONLY", "Stop Play before editing materials");
        AssetKey Material;
        if (!Params.at("assetId").is_null()) { Material.Asset = Asset(Params.at("assetId")).Metadata.Guid; }
        auto Command = MakeAssignMeshMaterialCommand(World(), Registry, Mesh->GetObjectHandle(), Params.at("slot"), Material, Error);
        Require(Command != nullptr, "INVALID_ARGUMENT", Error);
        Commit(std::move(Command));
    }
    if (Method == "mesh.physics")
    {
        Require(!Runtime.GetPlaySession().IsSimulating(), "PLAY_READ_ONLY", "Stop Play before editing physics");
        auto Command = MakeSetMeshPhysicsCommand(World(), Registry, Mesh->GetObjectHandle(), Params.at("enabled"), Params.at("simulate"), Error);
        Require(Command != nullptr, "INVALID_ARGUMENT", Error);
        Commit(std::move(Command));
    }
    Json Slots = Json::array();
    std::set<int32_t> Indices;
    AssetHandle<StaticMeshResource> LoadedMesh;
    if (Runtime.GetAssetManager().TryGetLoaded(Mesh->GetMeshAssetKey(), LoadedMesh))
    {
        for (const auto& Part : LoadedMesh->Submeshes) { Indices.insert(Part.MaterialSlot); }
    }
    for (const auto& Part : Mesh->Submeshes) { Indices.insert(Part.MaterialSlot); }
    for (const auto& Slot : Mesh->MaterialSlots.Entries) { Indices.insert(Slot.Slot); }
    for (int32_t Slot : Indices)
    {
        AssetKey Material = Mesh->GetMaterialAssetKey();
        bool bInherited = true;
        const auto Default = Mesh->DefaultMaterialSlots.find(Slot);
        if (Default != Mesh->DefaultMaterialSlots.end()) { Material = Default->second; }
        for (const auto& Override : Mesh->MaterialSlots.Entries)
        {
            if (Override.Slot == Slot) { Material = Override.Material; bInherited = false; }
        }
        Json MaterialIdentity = nullptr;
        if (Material.IsValid())
        {
            MaterialIdentity = Material.Asset.ToString();
        }
        Slots.push_back({{"slot", Slot}, {"assetId", MaterialIdentity}, {"inherited", bInherited}});
    }
    const auto* Body = Mesh->GetGameObject()->GetComponent<PhysicsBodyComponent>();
    PhysicsMotionType MotionType = PhysicsMotionType::Static;
    if (Body)
    {
        MotionType = Body->MotionType;
    }
    ReflectedValue EncodedMotion;
    Diagnostic(ReflectionValueCodec<PhysicsMotionType>::ToReflected(MotionType, EncodedMotion));
    Json Motion;
    Diagnostic(ReflectionJson::ReflectedValueToJson(ReflectionEnumType<PhysicsMotionType>::GetTypeId(), EncodedMotion, Motion));
    return {{"slots", Slots}, {"collision", Body && Body->IsEnabled()}, {"simulate", Body && Body->MotionType == PhysicsMotionType::Dynamic},
        {"motion", Motion}};
}

bool NativeBridge::HasDirtyAssetDocuments() const
{
    if (OpenPrefab.IsValid() && Workspace.GetDocument().IsDirty()) { return true; }
    for (const auto& Entry : AssetDocuments)
    {
        if (Entry.second.Reflected && Entry.second.Reflected->IsDirty()) { return true; }
        if (Entry.second.Widgets && Entry.second.Widgets->IsDirty())
        {
            return true;
        }
        if (Entry.second.Script && Entry.second.Script->IsDirty()) { return true; }
        if (Entry.second.Mesh && Entry.second.Mesh->IsDirty()) { return true; }
        if (Entry.second.Material && Entry.second.Material->GetDocument().IsDirty()) { return true; }
    }
    return false;
}

Json NativeBridge::AssetDocumentTools(const Json& Params)
{
    auto& Registry = Runtime.GetAssetRegistry();
    const auto Entry = Asset(Params.at("assetId"));
    if (Entry.Metadata.Type == PrefabAssetType) { return PrefabDocumentTools(Params, Entry); }
    AssetKey Key{Entry.Metadata.Guid, std::nullopt};
    if (Params.contains("subAssetId") && !Params.at("subAssetId").is_null())
    {
        Guid Identifier;
        Require(Guid::TryParse(Params.at("subAssetId"), Identifier), "INVALID_ARGUMENT", "Invalid subasset");
        Key.SubAsset = Identifier;
    }
    AssetRegistryEntry Resolved;
    const SubAssetRecord* Subasset = nullptr;
    Require(Registry.TryResolveKey(Key, Resolved, &Subasset), "NOT_FOUND", "Document asset unavailable");
    AssetType Type = Entry.Metadata.Type;
    if (Subasset) { Type = Subasset->Type; }

    const std::string Identifier = Key.Asset.ToString() + Params.value("subAssetId", Json(nullptr)).dump();
    const std::string Operation = Params.at("operation");
    auto Found = AssetDocuments.find(Identifier);
    if (Found == AssetDocuments.end())
    {
        Require(Operation == "open", "NOT_FOUND", "Open the asset document first");
        AssetDocumentState State;
        std::string Error;
        if (Type == WidgetLibraryAssetType)
        {
            State.Widgets = std::make_unique<WidgetLibraryDocument>(Registry);
            Require(State.Widgets->Open(Key, Error), "DOCUMENT_ERROR", Error);
        }
        else if (Type == PythonScriptAssetType)
        {
            State.Script = std::make_unique<PythonScriptDocument>(Registry);
            Require(State.Script->Open(Key, Error), "DOCUMENT_ERROR", Error);
        }
        else if (Type == StaticMeshAssetType)
        {
            State.Mesh = std::make_unique<StaticMeshDocument>(Registry);
            Require(State.Mesh->Open(Key, Error), "DOCUMENT_ERROR", Error);
        }
        else if (Type == MaterialAssetType || Type == MaterialInstanceAssetType)
        {
            State.Material = std::make_unique<MaterialEditingSession>(Registry);
            const auto Result = State.Material->Open(Key);
            Require(!Result.HasError(), "DOCUMENT_ERROR", Result.Message);
        }
        else if (Type != TextureAssetType)
        {
            State.Reflected = ReflectedAssetDocument::Create(Type);
            if (State.Reflected)
            {
                Require(State.Reflected->Open(Registry, Resolved, Error), "DOCUMENT_ERROR", Error);
            }
        }
        Found = AssetDocuments.emplace(Identifier, std::move(State)).first;
    }
    auto& State = Found->second;
    if (State.Widgets)
    {
        if (Operation != "open" && Operation != "get" && Operation != "preview")
        {
            Require(!Runtime.GetPlaySession().IsSimulating(), "PLAY_READ_ONLY", "Stop Play before editing widgets");
            Require(Params.contains("revision") && Params.at("revision") == State.Revision,
                "STALE_DOCUMENT", "Refresh the widget document before editing");
            Require(Entry.Mount == AssetMount::Game, "READ_ONLY", "Engine widgets are read-only");
            std::string Error;
            if (Operation == "edit")
            {
                Require(State.Widgets->Edit(Params.at("value"), Error), "DOCUMENT_ERROR", Error);
            }
            else if (Operation == "save")
            {
                Require(State.Widgets->Save(Error), "DOCUMENT_ERROR", Error);
            }
            else if (Operation == "undo")
            {
                State.Widgets->Undo();
            }
            else if (Operation == "redo")
            {
                State.Widgets->Redo();
            }
            else if (Operation == "reload")
            {
                Require(!State.Widgets->IsDirty(), "UNSAVED_CHANGES", "Save or undo widget changes before reloading");
                Require(State.Widgets->Open(Key, Error), "DOCUMENT_ERROR", Error);
            }
            else
            {
                throw Failure("INVALID_ARGUMENT", "Unknown widget document operation");
            }
            ++State.Revision;
        }
        return {{"assetId", Key.Asset.ToString()}, {"type", "WidgetLibrary"}, {"revision", State.Revision},
            {"document", State.Widgets->GetDocument()}, {"dirty", State.Widgets->IsDirty()},
            {"canUndo", State.Widgets->CanUndo()}, {"canRedo", State.Widgets->CanRedo()},
            {"readOnly", Entry.Mount == AssetMount::Engine || Runtime.GetPlaySession().IsSimulating()}};
    }
    if (State.Reflected)
    {
        if (Operation != "open" && Operation != "get" && Operation != "preview")
        {
            Require(!Runtime.GetPlaySession().IsSimulating(), "PLAY_READ_ONLY", "Stop Play before editing assets");
            Require(Params.contains("revision") && Params.at("revision") == State.Revision,
                "STALE_DOCUMENT", "Refresh the asset document before editing");
            Require(!State.Reflected->IsReadOnly(), "READ_ONLY", "Asset is read-only");
            std::string Error;
            if (Operation == "property")
            {
                Require(State.Reflected->Edit(Params.at("value"), Error), "DOCUMENT_ERROR", Error);
            }
            else if (Operation == "save")
            {
                Require(State.Reflected->Save(Error), "DOCUMENT_ERROR", Error);
            }
            else if (Operation == "undo")
            {
                State.Reflected->Undo();
            }
            else if (Operation == "redo")
            {
                State.Reflected->Redo();
            }
            else if (Operation == "reload")
            {
                Require(!State.Reflected->IsDirty(), "UNSAVED_CHANGES", "Save or undo changes before reloading");
                Require(State.Reflected->Open(Registry, Resolved, Error), "DOCUMENT_ERROR", Error);
            }
            else
            {
                throw Failure("INVALID_ARGUMENT", "Unknown reflected document operation");
            }
            ++State.Revision;
        }
        return {{"assetId", Key.Asset.ToString()}, {"type", Type.GetIdentifier()}, {"editor", "properties"},
            {"revision", State.Revision}, {"dirty", State.Reflected->IsDirty()},
            {"readOnly", State.Reflected->IsReadOnly() || Runtime.GetPlaySession().IsSimulating()},
            {"canUndo", State.Reflected->CanUndo()}, {"canRedo", State.Reflected->CanRedo()},
            {"schema", State.Reflected->GetSchema()}, {"document", State.Reflected->GetValues()}};
    }
    if (!State.Script && !State.Mesh && !State.Material && Type != TextureAssetType)
    {
        Require(Operation == "open" || Operation == "get" || Operation == "preview", "READ_ONLY", "No editable document is registered for this asset type");
        return {{"assetId", Key.Asset.ToString()}, {"type", Type.GetIdentifier()}, {"editor", "properties"},
            {"revision", State.Revision}, {"dirty", false}, {"readOnly", true},
            {"document", Json::object()}, {"schema", nullptr},
            {"diagnostic", "No reflected document is registered for this asset type."}};
    }
    if (State.Script)
    {
        if (Operation != "open" && Operation != "get" && Operation != "preview")
        {
            Require(!Runtime.GetPlaySession().IsSimulating(), "PLAY_READ_ONLY", "Stop Play before editing scripts");
            Require(Params.contains("revision") && Params.at("revision") == State.Revision,
                "STALE_DOCUMENT", "Refresh the script document before editing");
            Require(Entry.Mount == AssetMount::Game, "READ_ONLY", "Engine scripts are read-only");
            std::string Error;
            if (Operation == "source")
            {
                Require(Params.at("value").is_string(), "INVALID_ARGUMENT", "Python source must be text");
                Require(State.Script->Edit(Params.at("value").get<std::string>(), Error), "DOCUMENT_ERROR", Error);
            }
            else if (Operation == "save")
            {
                Require(State.Script->Save(Error), "DOCUMENT_ERROR", Error);
            }
            else if (Operation == "undo")
            {
                State.Script->Undo();
            }
            else if (Operation == "redo")
            {
                State.Script->Redo();
            }
            else if (Operation == "reload")
            {
                Require(!State.Script->IsDirty(), "UNSAVED_CHANGES", "Save or undo script changes before reloading");
                Require(State.Script->Open(Key, Error), "DOCUMENT_ERROR", Error);
            }
            else
            {
                throw Failure("INVALID_ARGUMENT", "Unknown Python document operation");
            }
            ++State.Revision;
        }
        return {{"assetId", Key.Asset.ToString()}, {"type", "PythonScript"}, {"revision", State.Revision},
            {"source", State.Script->GetSource()}, {"dirty", State.Script->IsDirty()},
            {"readOnly", Entry.Mount == AssetMount::Engine}, {"document", Json::object()}};
    }
    if (Type == TextureAssetType)
    {
        Require(Operation == "open" || Operation == "get" || Operation == "preview",
            "READ_ONLY", "Texture editor currently supports inspection only");
        if (!State.TextureSnapshot.is_null())
        {
            return State.TextureSnapshot;
        }
        Json Snapshot = {{"assetId", Key.Asset.ToString()}, {"type", "Texture"}, {"revision", State.Revision},
            {"dirty", false}, {"readOnly", true}, {"preparing", true}, {"previewPng", ""},
            {"document", Json::object()}, {"diagnostic", ""}};
        if (Runtime.GetAssetManager().GetLoadState(Key) == AssetLoadState::Unloaded)
        {
            Runtime.GetAssetManager().LoadAsync<TextureResource>(Key);
        }
        AssetHandle<TextureResource> Texture;
        if (Runtime.GetAssetManager().TryGetLoaded(Key, Texture))
        {
            const auto Image = ImageBuffer::FromRgba(Texture->Pixels.data(), Texture->Width, Texture->Height,
                Texture->RowStride).ResizeToFit(1024, 1024);
            const auto Bytes = Image.EncodePng();
            Require(!Bytes.empty(), "DOCUMENT_ERROR", "Texture preview encoding failed");
            std::string ColorSpace = "Linear";
            if (Texture->ColorSpace == TextureColorSpace::Srgb)
            {
                ColorSpace = "sRGB";
            }
            Snapshot["document"] = {{"width", Texture->Width}, {"height", Texture->Height},
                {"channelCount", Texture->ChannelCount}, {"decodedBytes", Texture->Pixels.size()},
                {"colorSpace", ColorSpace}, {"format", "RGBA8"}, {"previewWidth", Image.Width}, {"previewHeight", Image.Height}};
            Snapshot["previewPng"] = Base64::Encode(Bytes.data(), Bytes.size());
            Snapshot["preparing"] = false;
            State.TextureSnapshot = Snapshot;
        }
        else if (Runtime.GetAssetManager().GetLoadState(Key) == AssetLoadState::Failed)
        {
            Snapshot["preparing"] = false;
            Snapshot["diagnostic"] = Runtime.GetAssetManager().GetLastDiagnostic(Key).Message;
            State.TextureSnapshot = Snapshot;
        }
        return Snapshot;
    }
    if (Operation == "preview")
    {
        PreviewDocument = Identifier;
        PreviewShape = Params.value("value", Json::object()).value("previewShape", PreviewShape);
        SelectedCollision = Params.value("value", Json::object()).value("selectedCollision", -1);
        bShowCollision = Params.value("value", Json::object()).value("showCollision", false);
    }
    if (Operation != "open" && Operation != "get" && Operation != "preview")
    {
        Require(!Runtime.GetPlaySession().IsSimulating(), "PLAY_READ_ONLY", "Stop Play before editing asset documents");
        Require(Params.contains("revision") && Params.at("revision") == State.Revision, "STALE_DOCUMENT", "Refresh the asset document before editing");
        Require(Entry.Mount == AssetMount::Game, "READ_ONLY", "Engine Content is read-only");
        std::string Error;
        if (State.Mesh)
        {
            if (Operation == "edit") { Require(State.Mesh->Edit(Params.at("value"), Error), "DOCUMENT_ERROR", Error); }
            else if (Operation == "undo") { State.Mesh->Undo(); }
            else if (Operation == "redo") { State.Mesh->Redo(); }
            else if (Operation == "save") { Require(State.Mesh->Save(Runtime.GetAssetManager(), Error), "DOCUMENT_ERROR", Error); }
            else if (Operation == "reload")
            {
                Require(!State.Mesh->IsDirty(), "UNSAVED_CHANGES", "Save or undo mesh changes before reloading");
                Require(State.Mesh->Open(Key, Error), "DOCUMENT_ERROR", Error);
            }
            else { throw Failure("INVALID_ARGUMENT", "Unknown mesh document operation"); }
        }
        else
        {
            MaterialEditOperation Action;
            if (Operation == "edit") { Action = MaterialEditOperation::Document; }
            else if (Operation == "source") { Action = MaterialEditOperation::Source; }
            else if (Operation == "undo") { Action = MaterialEditOperation::Undo; }
            else if (Operation == "redo") { Action = MaterialEditOperation::Redo; }
            else if (Operation == "save") { Action = MaterialEditOperation::Save; }
            else if (Operation == "compile") { Action = MaterialEditOperation::Compile; }
            else if (Operation == "reload")
            {
                Require(!State.Material->GetDocument().IsDirty(), "UNSAVED_CHANGES", "Save or undo material changes before reloading");
                Action = MaterialEditOperation::Reload;
            }
            else { throw Failure("INVALID_ARGUMENT", "Unknown material document operation"); }
            const auto Result = State.Material->Execute(Action, Params.value("value", Json{}));
            Require(!Result.HasError(), "DOCUMENT_ERROR", Result.Message);
        }
        ++State.Revision;
        Previews.clear();
    }
    Json Document;
    bool bDirty = false;
    std::string Source;
    std::string Diagnostic;
    if (State.Mesh) { Document = State.Mesh->GetDefaults().ToJson(); bDirty = State.Mesh->IsDirty(); }
    else
    {
        State.Material->Poll(Runtime.GetAssetGpuUploader());
        Document = State.Material->GetDocument().GetDocument();
        Source = State.Material->GetDocument().GetSource();
        bDirty = State.Material->GetDocument().IsDirty();
        Diagnostic = State.Material->GetDiagnostic();
        if (!State.ImageDiagnostic.empty()) { Diagnostic = State.ImageDiagnostic; }
    }
    Json Parameters = Json::array();
    std::string PreviewDomain = "Surface";
    if (State.Material)
    {
        ResolvedMaterial ResolvedMaterialValue;
        const auto Result = State.Material->GetDocument().BuildResolved(ResolvedMaterialValue);
        if (!Result.HasError())
        {
            if (ResolvedMaterialValue.Definition.Domain == MaterialDomain::PostProcess) { PreviewDomain = "PostProcess"; }
            if (ResolvedMaterialValue.Definition.Domain == MaterialDomain::UserInterface) { PreviewDomain = "UserInterface"; }
            for (const auto& Parameter : ResolvedMaterialValue.Definition.Parameters)
            {
                auto Value = Parameter.DefaultValue;
                const auto FoundValue = ResolvedMaterialValue.Values.find(Parameter.Identifier);
                if (FoundValue != ResolvedMaterialValue.Values.end()) { Value = FoundValue->second; }
                Parameters.push_back({{"id", Parameter.Identifier}, {"displayName", Parameter.DisplayName},
                    {"group", Parameter.Group}, {"type", MaterialDefinitionIO::ParameterTypeName(Value.Type)},
                    {"value", MaterialDocumentIO::WriteValue(Value)}});
            }
        }
        else if (Diagnostic.empty()) { Diagnostic = Result.Message; }
    }
    Json Slots = Json::array();
    if (State.Mesh)
    {
        Runtime.GetAssetManager().LoadAsync<StaticMeshResource>(Key);
        AssetHandle<StaticMeshResource> Resource;
        if (Runtime.GetAssetManager().TryGetLoaded(Key, Resource))
        {
            std::set<int32_t> Indices;
            for (const auto& Part : Resource->Submeshes) { Indices.insert(Part.MaterialSlot); }
            for (int32_t Index : Indices) { Slots.push_back(Index); }
        }
    }
    return {{"assetId", Key.Asset.ToString()}, {"subAssetId", Params.value("subAssetId", Json(nullptr))},
        {"type", Type.GetIdentifier()}, {"revision", State.Revision}, {"document", Document}, {"slots", Slots},
        {"previewDomain", PreviewDomain}, {"previewPng", State.ImagePng}, {"sourceReadOnly", State.Material && State.Material->GetDocument().IsSourceReadOnly()}, {"preparing", State.Material && State.Material->IsPreparing()}, {"parameters", Parameters}, {"selectedCollision", SelectedCollision}, {"dirty", bDirty}, {"readOnly", Entry.Mount == AssetMount::Engine}, {"source", Source}, {"diagnostic", Diagnostic}};
}

NativeBridge::~NativeBridge()
{
    CancelTransformEdit();
    if (PreviewScene)
    {
        Runtime.SetRenderSurfaceScene(RenderSurfaceId{1}, nullptr);
        MemorySubsystem::Get()->DestroyObject(PreviewScene);
    }
}

const StaticMeshDefaults* NativeBridge::GetPreviewCollision() const
{
    const auto Found = AssetDocuments.find(PreviewDocument);
    if (!bShowCollision || Found == AssetDocuments.end() || !Found->second.Mesh) { return nullptr; }
    return &Found->second.Mesh->GetDefaults();
}

bool NativeBridge::CanEditPreviewCollision() const
{
    const auto Found = AssetDocuments.find(PreviewDocument);
    return Found != AssetDocuments.end() && Found->second.Mesh
        && Found->second.Mesh->GetEntry().Mount == AssetMount::Game && !Runtime.GetPlaySession().IsSimulating();
}

uint64_t NativeBridge::GetPreviewRevision() const
{
    const auto Found = AssetDocuments.find(PreviewDocument);
    if (Found == AssetDocuments.end()) { return 0; }
    return Found->second.Revision;
}

bool NativeBridge::CommitPreviewCollision(const Json& Value, uint64_t ExpectedRevision)
{
    const auto Found = AssetDocuments.find(PreviewDocument);
    if (Found == AssetDocuments.end() || !Found->second.Mesh) { return false; }
    const auto Key = Found->second.Mesh->GetAsset();
    try
    {
        AssetDocumentTools({{"assetId", Key.Asset.ToString()}, {"subAssetId", Key.SubAsset->ToString()},
            {"operation", "edit"}, {"revision", ExpectedRevision}, {"value", Value}});
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

void NativeBridge::TickPreview()
{
    auto Found = AssetDocuments.find(PreviewDocument);
    if (Found == AssetDocuments.end()) { return; }
    auto& State = Found->second;
    if (!PreviewScene)
    {
        PreviewScene = MemorySubsystem::Get()->NewObject<Scene>();
        PreviewScene->SetAssetManager(&Runtime.GetAssetManager());
        auto* Light = PreviewScene->CreateGameObject("Preview Light");
        Transform LightTransform;
        LightTransform.Rotation = Quaternion::CreateFromYawPitchRoll(-0.5f, -0.7f, 0.f);
        Light->SetTransform(LightTransform);
        Light->SetVisual(false);
        Light->AddComponent<LightComponent>()->Intensity = 2.f;
        PreviewMesh = PreviewScene->CreateGameObject("Asset Preview")->AddComponent<MeshRendererComponent>();
        PreviewCamera = PreviewScene->CreateGameObject("Preview Camera")->AddComponent<CameraComponent>();
    }
    AssetKey MeshAsset;
    if (State.Mesh)
    {
        MeshAsset = State.Mesh->GetAsset();
        PreviewMesh->MaterialSlots.Entries.clear();
        for (const auto& Assignment : State.Mesh->GetDefaults().Materials)
        {
            MaterialSlotOverride Override;
            Override.Slot = Assignment.first;
            Override.Material = Assignment.second;
            PreviewMesh->MaterialSlots.Entries.push_back(Override);
        }
    }
    else
    {
        for (const auto& Entry : Runtime.GetAssetRegistry().FindByType(ModelAssetType))
        {
            if (Entry.VirtualPath.find("BasicShapes/" + PreviewShape + ".glb") == std::string::npos) { continue; }
            for (const auto& Subasset : Entry.Metadata.SubAssets)
            {
                if (Subasset.Type == StaticMeshAssetType) { MeshAsset = {Entry.Metadata.Guid, Subasset.Id}; break; }
            }
        }
        if (State.RequestedPreviewRevision != State.Revision)
        {
            State.Material->Execute(MaterialEditOperation::Compile);
            State.RequestedPreviewRevision = State.Revision;
        }
        State.Material->Poll(Runtime.GetAssetGpuUploader());
        if (State.Material->GetPreview())
        {
            const auto Material = State.Material->GetPreview();
            const auto Domain = Material->GetResolvedMaterial().Definition.Domain;
            PreviewCamera->PostProcessEffects.Entries.clear();
            if (Domain == MaterialDomain::Surface) { PreviewMesh->SetDynamicMaterial(Material); }
            else
            {
                PreviewMesh->SetDynamicMaterial(nullptr);
                if (Domain == MaterialDomain::PostProcess)
                {
                    MaterialPostProcessEffect Effect;
                    Effect.DynamicMaterial = Material;
                    PreviewCamera->PostProcessEffects.Entries.push_back(Effect);
                }
                if (Domain == MaterialDomain::UserInterface)
                {
                    Runtime.UpdateMaterialImage();
                    if (State.Image && State.Image->State.load() == MaterialImageState::Ready)
                    {
                        const auto CompletedImage = State.Image;
                        std::lock_guard<std::mutex> Lock(CompletedImage->Mutex);
                        if (State.Image->Generation == State.Material->GetPreviewRevision())
                        {
                            const auto Image = ImageBuffer::FromRgba(State.Image->Pixels.data(), State.Image->Width,
                                State.Image->Height, State.Image->Width * 4, true);
                            const auto Bytes = Image.EncodePng();
                            State.ImagePng = Base64::Encode(Bytes.data(), Bytes.size());
                        }
                        State.Image.reset();
                    }
                    if (State.Image && (State.Image->State.load() == MaterialImageState::Failed || State.Image->State.load() == MaterialImageState::Cancelled))
                    {
                        const auto CompletedImage = State.Image;
                        std::lock_guard<std::mutex> Lock(CompletedImage->Mutex);
                        State.ImageDiagnostic = CompletedImage->Diagnostic;
                        State.Image.reset();
                    }
                    const bool bAnimatedFrame = Material->GetResolvedMaterial().Definition.bAnimated && Runtime.GetMaterialTime() - State.ImageTime >= .25f;
                    if (!State.Image && (State.ImageRevision != State.Material->GetPreviewRevision() || bAnimatedFrame))
                    {
                        auto Image = std::make_shared<MaterialImageRequest>();
                        Image->Snapshot = Runtime.GetSceneAssetResolver().ResolveMaterial({}, Material);
                        Image->Width = 512;
                        Image->Height = 512;
                        Image->Generation = State.Material->GetPreviewRevision();
                        Image->Time = Runtime.GetMaterialTime();
                        if (Image->Snapshot && Runtime.UpdateMaterialImage(Image))
                        {
                            State.ImageDiagnostic.clear();
                            State.ImageTime = Image->Time;
                            State.ImageRevision = Image->Generation;
                            State.Image = Image;
                        }
                    }
                }
            }
        }
    }
    if (PreviewMesh->GetMeshAssetKey() != MeshAsset) { PreviewMesh->SetMeshAssetKey(MeshAsset); }
    Runtime.GetSceneAssetResolver().ResolveScene(*PreviewScene);
    Runtime.SetRenderSurfaceScene(RenderSurfaceId{1}, PreviewScene);
}

Json NativeBridge::PrefabTools(const std::string& Method, const Json& Params)
{
    Require(!Runtime.GetPlaySession().IsSimulating() && !Runtime.IsGameRunning(), "PLAY_READ_ONLY", "Stop Play before editing prefabs");
    std::string Error;
    if (Method == "prefab.instantiate")
    {
        const auto Entry = Asset(Params.at("assetId"));
        Require(Entry.Metadata.Type == PrefabAssetType, "INVALID_ARGUMENT", "Expected prefab asset");
        ObjectHandle Parent;
        if (Params.contains("parentId") && !Params.at("parentId").is_null())
        {
            Parent = Find(Params.at("parentId"))->GetObjectHandle();
        }
        ObjectHandle Created;
        Commit(MakeInstantiatePrefabCommand(World(), Entry.Metadata.Guid, Parent, nullptr, &Created));
        Workspace.Select(World(), Created);
        return Describe();
    }
    auto* Object = Find(Params.at("objectId"));
    if (Method == "prefab.create")
    {
        const auto Path = ContentPath(Params.at("path"));
        Require(Path.extension() == ".prefab", "INVALID_PATH", "Expected a .prefab path");
        AssetId Created;
        const bool bVariant = Params.value("variant", false);
        Commit(MakePrefabSubtreeEditCommand(World(), Object->GetObjectHandle(), "Create prefab",
            [&](Scene& Scene, GameObject& Root, std::string& Failure)
            {
                return PrefabUtility::CreatePrefabAsset(Scene, Root, Path, bVariant, Created, Failure);
            }, &Error));
        Require(Error.empty(), "PREFAB_ERROR", Error);
        return {{"assetId", Created.ToString()}};
    }
    auto* Root = PrefabUtility::GetOutermostInstanceRoot(Object);
    if (Method == "prefab.inspect")
    {
        if (!Root) { return {{"instance", false}, {"overrides", Json::array()}}; }
        std::vector<PrefabOverride> Overrides;
        Require(PrefabUtility::GetOverrides(*World(), *Root, Overrides, Error), "PREFAB_ERROR", Error);
        Json Result = Json::array();
        for (size_t Index = 0; Index < Overrides.size(); ++Index)
        {
            const auto& Override = Overrides[Index];
            Result.push_back({{"index", Index}, {"objectId", Override.HostObjectId}, {"componentId", Override.HostComponentId},
                {"property", Override.Property}, {"label", Override.DisplayText}});
        }
        return {{"instance", true}, {"assetId", PrefabUtility::GetPrefabAsset(Root).ToString()},
            {"rootId", Root->GetPersistentId()}, {"overrides", Result}};
    }
    Require(Root != nullptr, "INVALID_ARGUMENT", "Object is not a prefab instance");
    const auto Operation = Params.at("operation").get<std::string>();
    std::vector<PrefabOverride> Selection;
    if (Params.contains("selection"))
    {
        Require(Params["selection"].is_array(), "INVALID_ARGUMENT", "Expected override indices");
        std::vector<PrefabOverride> Overrides;
        Require(PrefabUtility::GetOverrides(*World(), *Root, Overrides, Error), "PREFAB_ERROR", Error);
        for (const auto& Index : Params["selection"])
        {
            Require(Index.is_number_unsigned() && Index.get<size_t>() < Overrides.size(), "INVALID_ARGUMENT", "Override index is invalid");
            Selection.push_back(Overrides[Index.get<size_t>()]);
        }
    }
    if (Operation == "apply")
    {
        Require(PrefabUtility::ApplyOverrides(*World(), *Root, Selection, Error), "PREFAB_ERROR", Error);
        Workspace.GetCommands().Clear();
        Workspace.GetDocument().MarkDirty();
    }
    else
    {
        Require(Operation == "revert" || Operation == "unpack" || Operation == "unpackAll", "INVALID_ARGUMENT", "Unknown prefab operation");
        Commit(MakePrefabSubtreeEditCommand(World(), Root->GetObjectHandle(), "Modify prefab instance",
            [Operation, Selection](Scene& Scene, GameObject& Root, std::string& Failure)
            {
                if (Operation == "revert")
                {
                    if (Selection.empty()) { return PrefabUtility::RevertAll(Scene, Root, Failure); }
                    return PrefabUtility::RevertOverrides(Scene, Root, Selection, Failure);
                }
                auto Mode = PrefabUnpackMode::OutermostRoot;
                if (Operation == "unpackAll") { Mode = PrefabUnpackMode::Completely; }
                return PrefabUtility::Unpack(Scene, Root, Mode, Failure);
            }, &Error));
        Require(Error.empty(), "PREFAB_ERROR", Error);
    }
    return Describe();
}

Json NativeBridge::PrefabDocumentTools(const Json& Params, const AssetRegistryEntry& Entry)
{
    Require(std::getenv("SACURA_ASSET_WORKSPACE") != nullptr, "INVALID_STATE", "Open prefabs in an isolated asset workspace");
    Require(!Runtime.GetPlaySession().IsSimulating() && !Runtime.IsGameRunning(), "PLAY_READ_ONLY", "Stop Play before editing prefabs");
    const std::string Operation = Params.at("operation");
    std::string Error;
    auto& Document = Workspace.GetDocument();
    if (!OpenPrefab.IsValid())
    {
        Require(Operation == "open", "INVALID_STATE", "Open the prefab document first");
        auto Candidate = std::unique_ptr<Scene>(MemorySubsystem::Get()->NewObject<Scene>("PrefabWorkspace"));
        GameObject* Root = nullptr;
        Require(PrefabUtility::LoadPrefabIntoScene(Entry.Metadata.Guid, *Candidate, Root, Error), "PREFAB_ERROR", Error);
        Require(Document.AdoptScene(Candidate.get(), std::filesystem::u8path(Entry.AbsolutePath), Error), "PREFAB_ERROR", Error);
        Candidate.release();
        OpenPrefab = Entry.Metadata.Guid;
        Workspace.GetCommands().Clear();
        ObserveWorld();
        Workspace.Select(World(), Root->GetObjectHandle());
    }
    Require(OpenPrefab == Entry.Metadata.Guid, "INVALID_ARGUMENT", "This workspace owns a different prefab");
    if (Operation != "get" && Operation != "open" && Operation != "preview")
    {
        const bool bSelecting = Operation == "command" && Params.at("value").value("method", std::string{}) == "selection.set";
        Require(Entry.Mount == AssetMount::Game || bSelecting, "READ_ONLY", "Engine prefabs are read-only");
        Require(Params.value("revision", uint64_t(-1)) == Document.GetRevision(), "STALE_DOCUMENT", "Prefab document revision changed");
        if (Operation == "save")
        {
            Require(!Document.HasExternalChanges(), "EXTERNAL_CHANGES", "Prefab changed on disk; preserve the draft before reopening");
            Require(PrefabUtility::SavePrefabFromScene(*World(), OpenPrefab, Error), "PREFAB_ERROR", Error);
            Require(Document.AdoptScene(World(), std::filesystem::u8path(Entry.AbsolutePath), Error), "PREFAB_ERROR", Error);
        }
        else if (Operation == "undo") { Require(Workspace.GetCommands().Undo(), "COMMAND_FAILED", "Undo is unavailable"); }
        else if (Operation == "redo") { Require(Workspace.GetCommands().Redo(), "COMMAND_FAILED", "Redo is unavailable"); }
        else if (Operation == "command")
        {
            const auto& Command = Params.at("value");
            const std::string Method = Command.at("method");
            const std::set<std::string> Allowed{"selection.set", "object.create", "object.destroy", "object.rename", "object.setParent",
                "object.addComponent", "object.removeComponent", "component.setProperty", "component.resetProperty", "transform.set",
                "prefab.instantiate", "prefab.inspect", "prefab.modify"};
            Require(Allowed.count(Method) != 0, "INVALID_ARGUMENT", "Unsupported prefab document command");
            ValidateParams(Method, Command.at("params"));
            Dispatch(Method, Command.at("params"));
        }
        else { Require(false, "INVALID_ARGUMENT", "Unsupported prefab document operation"); }
    }
    Json Objects = Hierarchy();
    for (auto& Object : Objects)
    {
        for (auto& Component : Object["components"])
        {
            Json Identity{{"objectId", Object["id"]}, {"componentId", Component["id"]}};
            Component["properties"] = Properties(FindComponent(Identity));
        }
    }
    return {{"assetId", OpenPrefab.ToString()}, {"subAssetId", nullptr}, {"type", "Prefab"}, {"name", Entry.RelativePath},
        {"revision", Document.GetRevision()}, {"dirty", Document.IsDirty()}, {"readOnly", Entry.Mount != AssetMount::Game},
        {"canUndo", Workspace.GetCommands().CanUndo()}, {"canRedo", Workspace.GetCommands().CanRedo()},
        {"document", Objects}, {"selection", Describe()["selection"]}};
}
