#include "NativeBridge.h"
#include "Commands/EditorCommands.h"
#include "Core/Threading/ThreadContext.h"
#include "World/Scene.h"
#include "World/GameObject.h"
#include "Reflection/Json/ReflectionJson.h"
#include "Reflection/PropertyAccess.h"
#include "Project/ProjectPaths.h"
#include <fstream>
#include <set>
#include <cmath>
#include <charconv>
#include <limits>
#include "Assets/AssetImporter.h"
#include "Assets/StoryAssetCreation.h"
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
NativeBridge::NativeBridge(Engine& R,SceneWorkspace& W,ProjectSession& P,const std::filesystem::path& File,std::function<void()> Reset)
    :Runtime(R),Workspace(W),Project(P),Session(Guid::Generate().ToString()),ResetInput(std::move(Reset)) {
    AssertGameThread();std::ifstream Input(File);Input>>Contract;Require(Contract.at("version")==1,"PROTOCOL_ERROR","Unsupported contract");
    LastWorld=World()->GetObjectHandle();
    LastEditWorld=Runtime.GetEditScene()->GetObjectHandle();
    AssetDiagnostic D; if(P.GetProject()) ContentHash::TryHashFile(P.GetProject()->ProjectFile.u8string(),ProjectFingerprint,D);
    if(P.GetProject()&&!P.GetProject()->StartupStory.empty()) {
        Story.Open(P.GetProject()->StartupStory,StoryError);
    }
}
void NativeBridge::ObserveWorld() {
    const auto Edit=Runtime.GetEditScene()?Runtime.GetEditScene()->GetObjectHandle():ObjectHandle{};
    if(Edit!=LastEditWorld){Workspace.GetCommands().Clear();Workspace.ClearSelection();LastEditWorld=Edit;}
    const auto Active=Runtime.GetActiveScene()?Runtime.GetActiveScene()->GetObjectHandle():ObjectHandle{};
    if(Active!=LastWorld){LastWorld=Active;++Epoch;Workspace.ClearSelection();}
}
Json NativeBridge::Context() const {return {{"session",Session},{"epoch",Epoch},{"revision",Revision}};}
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
    Require(P.is_object(),"INVALID_ARGUMENT","params must be an object");ValidateJson(P);
    std::set<std::string> Allowed;
    for(auto It=Contract["methods"][M].begin();It!=Contract["methods"][M].end();++It) {
        std::string K=It.key();bool Optional=K.back()=='?';if(Optional)K.pop_back();Allowed.insert(K);
        if(!P.contains(K)){Require(Optional,"INVALID_ARGUMENT","Missing parameter: "+K);continue;}
        const auto& V=P[K];const std::string T=It.value();
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
    Json A=Json::array();for(auto* C:O->GetAllComponents())A.push_back({{"id",C->GetPersistentId()},{"typeId",C->GetTypeId().Value},{"enabled",C->IsEnabled()}});return A;
}
Json NativeBridge::Properties(Component* C) {
    Json A=Json::array();if(!C->GetClass())return A;
    for(const auto* P:C->GetClass()->GetAllProperties())if(Readable(*P)) {
        Json Item{{"id",P->Id.Value},{"typeId",P->ValueTypeId.Value},{"editable",Editable(*P)},
            {"label",P->Attributes.DisplayName?P->Attributes.DisplayName:P->Id.Value},
            {"category",P->Attributes.Category?P->Attributes.Category:""},{"value",nullptr}};
        ReflectedValue V;auto D=PropertyAccess::GetProperty(C,*P,V,PropertyAccessContext::Inspector);Json Value;
        if(D.bOk)D=ReflectionJson::ReflectedValueToJson(P->ValueTypeId,V,Value);
        if(D.bOk)Item["value"]=ConvertValue(P->ValueTypeId,Value,false);else Item["error"]=D.Message;
        if(P->Attributes.AssetTypeFilter||!P->Attributes.AllowedAssetTypes.empty()) {
            auto Types=P->Attributes.AllowedAssetTypes;
            if(P->Attributes.AssetTypeFilter)Types.push_back(P->Attributes.AssetTypeFilter);
            Item["assetTypes"]=Types;Item["subAssetId"]=nullptr;
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
    return {{"dirty",Workspace.GetDocument().IsDirty()},{"storyDirty",Story.IsDirty()},
        {"externalChanges",Workspace.GetDocument().HasExternalChanges()},{"projectExternalChanges",ProjectChanged()},
        {"path",Workspace.GetDocument().GetDocumentPath().generic_string()},
        {"project",Project.GetProject()?Project.GetProject()->Name:""},
        {"play",Play.GetState()==PlaySessionState::Stopped?"stopped":Play.GetState()==PlaySessionState::Paused?"paused":"playing"},
        {"dialogue",Dialogue},{"startupAudio",Project.GetProject()&&!Project.GetProject()->StartupAudio.empty()},
        {"selection",Selected?Json(Selected->GetPersistentId()):Json(nullptr)},
        {"canUndo",Workspace.GetCommands().CanUndo()},{"canRedo",Workspace.GetCommands().CanRedo()},{"assetError",AssetError}};
}
Json NativeBridge::Dispatch(const std::string& M,const Json& P) {
    if(M.rfind("story.",0)==0||M.rfind("assets.",0)==0)return DispatchDocuments(M,P);
    auto& Commands=Workspace.GetCommands();auto& Document=Workspace.GetDocument();auto& Play=Runtime.GetPlaySession();
    const bool Read=M=="bridge.describe"||M=="scene.getHierarchy"||M=="object.getComponents"||M=="component.getProperties"||M=="transform.get"||M=="reflection.getComponentTypes"||M=="assets.getScenes"||M=="audio.resolveStartup";
    if(!Read&&M!="selection.set"&&M.rfind("play.",0)!=0)Require(!Play.IsSimulating(),"PLAY_READ_ONLY","Stop play before editing or opening/saving a scene");
    if(!Read)ResetInput();
    if(M=="bridge.describe")return Describe();
    if(M=="scene.getHierarchy")return Hierarchy();
    if(M=="project.save") {
        Require(!ProjectChanged(),"EXTERNAL_CHANGES","Project descriptor changed on disk; reopen the project before saving startup settings");
        Require(!Document.HasExternalChanges()&&!Story.HasExternalChanges(),"EXTERNAL_CHANGES","A document changed on disk. Reload it or save a scene copy first");
        std::string Error;
        Require(Document.Save(Error),"IO_ERROR",Error);
        if(Story.IsOpen()&&Story.IsDirty()) Require(Story.Save(Error),"STORY_ERROR",Error);
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
        AssetRegistryEntry Model;
        Require(Runtime.GetAssetRegistry().TryGetByPath("/Engine/Models/StarterCharacter.glb",Model)
            &&Model.Metadata.SubAssets.size()==1&&Model.Metadata.SubAssets.front().Type==StaticMeshAssetType,
            "NOT_FOUND","Starter character mesh is unavailable");
        ObjectHandle Created,Mesh;
        Commands.BeginGroup("Create Character");
        try {
            Commit(MakeCreateObjectCommand(World(),Name,{},&Created));
            if(P.contains("transform"))Commit(MakeSetTransformCommand(World(),Created,ParseTransform(P["transform"])));
            Commit(MakeAddComponentCommand(World(),Created,TypeId{"engine.CharacterComponent"}));
            Commit(MakeAddComponentCommand(World(),Created,TypeId{"engine.MeshRendererComponent"},&Mesh));
            Commit(MakeSetPropertyCommand(World(),Mesh,PropertyId{"mesh_asset_id"},
                ReflectedValue::MakeString(Model.Metadata.Guid.ToString())));
            Commit(MakeSetPropertyCommand(World(),Mesh,PropertyId{"mesh_sub_asset_id"},
                ReflectedValue::MakeString(Model.Metadata.SubAssets.front().Id.ToString())));
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
                if(ComponentType==TypeId{"engine.LightComponent"})Commit(MakeSetPropertyCommand(World(),CreatedComponent,
                    PropertyId{"light_type"},ReflectedValue::MakeInt64(1)));
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
        Require(!Key.HasSubAsset()||Companion,"INVALID_ARGUMENT","Property does not support subassets");
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
    if(M=="transform.get")return TransformJson(Find(P["objectId"])->GetTransform());
    if(M=="transform.set"){auto* O=Find(P["objectId"]);Commit(MakeSetTransformCommand(World(),O->GetObjectHandle(),ParseTransform(P["transform"])));return TransformJson(O->GetTransform());}
    if(M=="selection.set"){if(P["objectId"].is_null())Workspace.ClearSelection();else Workspace.Select(World(),Find(P["objectId"])->GetObjectHandle());return Describe();}
    if(M=="editor.undo"){Require(Commands.Undo(),"COMMAND_FAILED","Nothing to undo or stale command");return Describe();}
    if(M=="editor.redo"){Require(Commands.Redo(),"COMMAND_FAILED","Nothing to redo or stale command");return Describe();}
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
        Json A=Json::array();for(auto* C:ReflectionSubsystem::Get().GetPublishedCatalog().Classes)if(C->IsConcrete()&&C->IsA(TypeId{"engine.Component"}))A.push_back({{"typeId",C->Id.Value},{"version",C->SchemaVersion}});return A;
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
        Result["value"]=Dispatch(M,Request.at("params"));Result["ok"]=true;
    }catch(const Failure& E){Result["error"]={{"code",E.Code},{"message",E.what()}};}
    catch(const std::exception& E){Result["error"]={{"code","INVALID_ARGUMENT"},{"message",E.what()}};}
    ObserveWorld();
    Result["context"]=Context();return Result;
}
Json NativeBridge::Poll(bool Force) {
    AssertGameThread();ObserveWorld();
    Json State=Describe();State.erase("context");State["hierarchy"]=Hierarchy();
    // Observe native gizmo/gameplay/property edits too; no JS mirror is authoritative.
    Json Values=Json::array();for(auto* O:World()->GetAllObjects())for(auto* C:O->GetAllComponents())Values.push_back(Properties(C));State["properties"]=std::move(Values);
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
void NativeBridge::ScanAssets(){AssertGameThread();const auto D=Runtime.GetAssetRegistry().ScanContent();AssetError=D.HasError()?D.Message:"";}
Json NativeBridge::Assets() {
    Json Result=Json::array();auto& Registry=Runtime.GetAssetRegistry();
    // Model containers are hidden by Qt's asset tree, which instead exposes their
    // subassets. The Bridge must include the container identity for mesh bindings.
    for(const auto& Type:Registry.GetAssetTypeRegistrations())if(Type.bVisibleInContentBrowser||Type.Type==ModelAssetType)
        for(const auto& E:Registry.FindByType(Type.Type)) {
            std::error_code Error;auto Bytes=std::filesystem::file_size(E.AbsolutePath,Error);const auto Size=Error?0:Bytes;
            const auto Modified=std::filesystem::last_write_time(E.AbsolutePath,Error);
            const auto Version=Error?std::string{}:std::to_string(Modified.time_since_epoch().count());Json Sub=Json::array();
            for(const auto& S:E.Metadata.SubAssets)Sub.push_back({{"id",S.Id.ToString()},{"typeId",S.Type.GetIdentifier()},{"name",S.Name}});
            Result.push_back({{"id",E.Metadata.Guid.ToString()},{"name",std::filesystem::path(E.RelativePath).filename().u8string()},
                {"path",E.VirtualPath},{"relativePath",E.RelativePath},{"typeId",E.Metadata.Type.GetIdentifier()},
                {"builtin",E.Mount==AssetMount::Engine},{"bytes",Size},{"version",Version},{"subassets",Sub}});
        }
    std::sort(Result.begin(),Result.end(),[](const Json& A,const Json& B){return A.at("path")<B.at("path");});return Result;
}
Json NativeBridge::StorySnapshot() {
    if(!Story.IsOpen())return {{"open",false},{"revision",StoryRevision},{"error",StoryError}};
    std::string Text;auto D=StoryDocumentIO::SaveToJson(Story.GetStory(),Text);Require(D.bOk,"STORY_ERROR",D.Error);
    Json Positions=Json::object();for(const auto& [Id,P]:Story.GetPositions())Positions[Id]={{"x",P.X},{"y",P.Y}};
    return {{"open",true},{"revision",StoryRevision},{"document",Json::parse(Text)},{"positions",Positions},
        {"path",Story.GetPath().generic_string()},{"dirty",Story.IsDirty()},{"readOnly",Story.IsReadOnly()},
        {"externalChanges",Story.HasExternalChanges()},{"canUndo",Story.CanUndo()},{"canRedo",Story.CanRedo()}};
}
Json NativeBridge::DispatchDocuments(const std::string& M,const Json& P) {
    auto& Registry=Runtime.GetAssetRegistry();std::string Error;
    if(M=="assets.list")return Assets();
    if(M=="assets.getScenes") {Json A=Json::array();for(const auto& E:Registry.FindByType(SceneAssetType))A.push_back({{"id",E.Metadata.Guid.ToString()},{"path",E.VirtualPath}});return A;}
    if(M=="assets.resolve") {
        const auto E=Asset(P.at("assetId"));return {{"id",E.Metadata.Guid.ToString()},{"path",E.VirtualPath},{"relativePath",E.RelativePath},
            {"typeId",E.Metadata.Type.GetIdentifier()},{"absolutePath",E.AbsolutePath},{"builtin",E.Mount==AssetMount::Engine}};
    }
    if(M=="assets.refresh"){ScanAssets();Require(AssetError.empty(),"ASSET_ERROR",AssetError);return Assets();}
    if(M=="story.get")return StorySnapshot();
    Require(!Runtime.GetPlaySession().IsSimulating(),"PLAY_READ_ONLY","Stop Play before editing assets or story");
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
