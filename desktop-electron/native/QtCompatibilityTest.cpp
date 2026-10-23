// Project-owned CLI test: real Qt shell/controllers, no external UI automation.
#include "Engine.h"
#include "Core/EnginePaths.h"
#include "EditorReflectionAnchor.h"
#include "Project/ProjectSession.h"
#include "Shell/EditorMainWindow.h"
#include "UI/Views/StoryWorkspaceView.h"
#include "Commands/EditorCommands.h"
#include "World/Scene.h"
#include "World/GameObject.h"
#include "Reflection/Json/ReflectionJson.h"
#include "Reflection/PropertyAccess.h"
#include <QApplication>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using Json=nlohmann::json;
static void Check(bool Ok,const std::string& Message){if(!Ok)throw std::runtime_error(Message);}
struct EditorCompatibilityProbe {
    static void Run(EditorMainWindow& Window,Engine& Runtime,const Json& Expected) {
        auto* Scene=Runtime.GetEditScene();
        Check(Scene&&Scene->GetObjectCount()==Expected["hierarchy"].size(),"Object count differs");
        for(const auto& Row:Expected["hierarchy"]) {
            auto* O=Scene->FindByPersistentId(Row.at("id"));Check(O!=nullptr,"Persistent object ID lost");
            Check(O->GetName()==Row.at("name"),"Object name lost");
            Check((O->GetParent()?Json(O->GetParent()->GetPersistentId()):Json(nullptr))==Row.at("parentId"),"Parent reference lost");
            const auto& T=O->GetTransform();
            Check(Json::array({T.Position.x,T.Position.y,T.Position.z})==Row["transform"]["position"],"Position differs");
            Check(Json::array({T.Scale.x,T.Scale.y,T.Scale.z})==Row["transform"]["scale"],"Scale differs");
            Check(Json::array({T.Rotation.x,T.Rotation.y,T.Rotation.z,T.Rotation.w})==Row["transform"]["rotation"],"Rotation differs");
            Check(O->GetAllComponents().size()==Row["components"].size(),"Component count differs");
            for(const auto& C:Row["components"]) {
                Component* Found=nullptr;for(auto* Item:O->GetAllComponents())if(Item->GetPersistentId()==C["id"])Found=Item;
                Check(Found&&Found->GetTypeId().Value==C["typeId"],"Component identity/type lost");
            }
        }
        for(const auto& Property:Expected["properties"]) {
            auto* O=Scene->FindByPersistentId(Property.at("objectId"));Component* C=nullptr;
            for(auto* Item:O->GetAllComponents())if(Item->GetPersistentId()==Property["componentId"])C=Item;
            Check(C!=nullptr,"Missing property owner");
            const PropertyDescriptor* Field=nullptr;for(const auto* P:C->GetClass()->GetAllProperties())if(P->Id.Value==Property["property"])Field=P;
            Check(Field!=nullptr,"Missing reflected field");ReflectedValue Value;Json Encoded;
            Check(PropertyAccess::GetProperty(C,*Field,Value,PropertyAccessContext::Inspector).bOk,"Read reflected field failed");
            Check(ReflectionJson::ReflectedValueToJson(Field->ValueTypeId,Value,Encoded).bOk,"Encode reflected field failed");
            Check(Encoded==Property["value"],"Reflected property differs");
        }
        for(const auto& E:Expected["assets"]) {
            Guid Id;Check(Guid::TryParse(E.at("id"),Id),"Invalid asset GUID");AssetRegistryEntry A;
            Check(Runtime.GetAssetRegistry().TryResolveKey(AssetKey{Id,{}},A),"Asset GUID lost");
            Check(A.RelativePath==E["relativePath"]&&A.Metadata.Type.GetIdentifier()==E["typeId"],"Asset identity mapping differs");
            for(const auto& Sub:E["subassets"]) {
                Guid SubId;Check(Guid::TryParse(Sub.at("id"),SubId),"Invalid subasset GUID");
                Check(Runtime.GetAssetRegistry().TryResolveKey(AssetKey{Id,SubId},A),"Subasset reference lost");
            }
        }
        auto* Graph=dynamic_cast<EditorUi::StoryWorkspaceView*>(Window.findChild<QWidget*>("StoryWorkspace"));
        Check(Graph&&Graph->GetModel().Nodes.size()==Expected["story"]["document"]["nodes"].size(),"Qt Story Graph did not load");
        Check(Graph->GetModel().Start.toStdString()==Expected["story"]["document"]["startNodeId"],"Story start reference differs");
        for(const auto& Node:Expected["story"]["document"]["nodes"]) {
            const EditorUi::StoryGraphNode* Found=nullptr;
            for(const auto& N:Graph->GetModel().Nodes)if(N.Identity.toStdString()==Node["id"])Found=&N;
            Check(Found!=nullptr,"Qt story node ID lost");
            if(Node["kind"]=="Line") Check(Found->Text.toStdString()==Node["text"]&&Found->Speaker.toStdString()==Node["speaker"],"Qt dialogue differs");
            if(Node.contains("nextNodeId")&&!Node["nextNodeId"].get<std::string>().empty())
                Check(Found->Targets.contains(QString::fromStdString(Node["nextNodeId"])),"Qt story edge lost");
            if(Node.contains("actions")) for(const auto& Action:Node["actions"]) {
                bool Matched=false;for(const auto& A:Found->Actions)if(A.Identity.toStdString()==Action["id"]) {
                    Check(A.Target.toStdString()==Action.value("targetObjectId",std::string{}),"Qt story target reference lost");
                    Check(Scene->FindByPersistentId(A.Target.toStdString())!=nullptr,"Story target cannot resolve in Qt");Matched=true;
                }Check(Matched,"Qt story action ID lost");
            }
        }
        auto* Target=Scene->FindByPersistentId(Expected.at("editId"));
        Check(Window.Workspace.GetCommands().Execute(MakeRenameObjectCommand(Scene,Target->GetObjectHandle(),"Edited in Qt")),"Qt rename failed");
        auto T=Target->GetTransform();T.Position.x=42;
        Check(Window.Workspace.GetCommands().Execute(MakeSetTransformCommand(Scene,Target->GetObjectHandle(),T)),"Qt transform failed");
        Window.OnSaveScene();Check(!Window.Workspace.GetDocument().IsDirty(),"Qt save failed");
    }
};
int main(int Argc,char** Argv) {
    QApplication App(Argc,Argv);
    try {
        Check(Argc==3,"Expected project and semantic manifest");
        EnginePaths::InitializeFromExecutable(std::filesystem::path(App.arguments()[0].toStdWString()));
        Json Expected;std::ifstream(std::filesystem::path(App.arguments()[2].toStdWString()))>>Expected;
        Engine Runtime;ForceTouchEditorRegistrars();Runtime.InitializeHeadless({});
        ProjectSession Session;Session.BindEngine(&Runtime);
        Check(Session.OpenProject(std::filesystem::path(App.arguments()[1].toStdWString())),Session.GetLastError());
        Check(Session.GetHealth()==ProjectSessionHealth::Ready,"Qt project degraded");
        {EditorMainWindow Window(Runtime,Session);App.processEvents();EditorCompatibilityProbe::Run(Window,Runtime,Expected);}
        Session.CloseProject();Runtime.Shutdown();
        std::cout<<"QT_COMPATIBILITY_PASS\n";return 0;
    }catch(const std::exception& E){std::cerr<<E.what()<<std::endl;return 1;}
}
