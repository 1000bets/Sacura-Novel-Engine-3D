#include "Core/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Engine.h"
#include "Game/Scene.h"
#include "Game/SceneSerializer.h"
#include "Gameplay/CameraComponent.h"
#include "Gameplay/GameObject.h"
#include "Project/ProjectGenerator.h"
#include "Project/ProjectSession.h"
#include "SceneDocument.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
int Failures = 0;

void Expect(bool Condition, const char* Message)
{
    if (!Condition)
    {
        ++Failures;
        std::cout << "FAIL: " << Message << "\n" << std::flush;
        return;
    }
    std::cout << "PASS: " << Message << "\n" << std::flush;
}

std::filesystem::path MakeTempRoot()
{
    const std::filesystem::path Root = std::filesystem::temp_directory_path() / "SakuraSceneSerializerTest";
    std::error_code Error;
    std::filesystem::remove_all(Root, Error);
    std::filesystem::create_directories(Root, Error);
    return Root;
}

bool JsonContains(const std::string& Text, const std::string& Needle)
{
    return Text.find(Needle) != std::string::npos;
}
}

int main()
{
    SetCurrentThreadRole(ThreadRole::Game);
    const std::filesystem::path TempRoot = MakeTempRoot();

    Engine BoundEngine;
    BoundEngine.InitializeHeadless({});

    {
        Scene* Source = MemorySubsystem::Get()->NewObject<Scene>("RoundTrip");
        GameObject* Parent = Source->CreateGameObject("Parent");
        GameObject* Child = Source->CreateGameObject("Child");
        Child->SetParent(Parent);
        Parent->GetTransform().Position = Vector3(1.f, 2.f, 3.f);
        Child->GetTransform().Scale = Vector3(2.f, 2.f, 2.f);
        CameraComponent* Camera = Child->AddComponent<CameraComponent>();
        Camera->SetFieldOfViewDegrees(72.5f);
        Camera->NearPlane = 0.25f;

        std::string FirstJson;
        Expect(SceneSerializer::SerializeToJson(*Source, FirstJson).bOk, "Serialize source scene");
        Expect(JsonContains(FirstJson, "engine.CameraComponent"), "Serialized CameraComponent type");
        Expect(JsonContains(FirstJson, "\"parent\""), "Serialized parent field");

        Scene* Loaded = nullptr;
        Expect(SceneSerializer::DeserializeFromJson(FirstJson, Loaded).bOk, "Deserialize round-trip");
        Expect(Loaded != nullptr && Loaded->GetObjectCount() == 2, "Round-trip object count");
        GameObject* LoadedParent = Loaded->FindByName("Parent");
        GameObject* LoadedChild = Loaded->FindByName("Child");
        Expect(LoadedParent != nullptr && LoadedChild != nullptr, "Round-trip names");
        Expect(LoadedChild->GetParent() == LoadedParent, "Round-trip parent link");
        Expect(LoadedParent->GetTransform().Position.x == 1.f, "Round-trip transform");
        CameraComponent* LoadedCamera = LoadedChild->GetComponent<CameraComponent>();
        Expect(LoadedCamera != nullptr && LoadedCamera->GetFieldOfViewDegrees() == 72.5f, "Round-trip camera FOV");

        std::string SecondJson;
        Expect(SceneSerializer::SerializeToJson(*Loaded, SecondJson).bOk, "Serialize second pass");
        Expect(JsonContains(SecondJson, "72.5") || JsonContains(SecondJson, "72.50"), "Second pass keeps FOV");

        MemorySubsystem::Get()->DestroyObject(Source);
        MemorySubsystem::Get()->DestroyObject(Loaded);
    }

    {
        Scene* Loaded = nullptr;
        Expect(!SceneSerializer::DeserializeFromJson("{ not json", Loaded).bOk, "Reject corrupt JSON");
        Expect(Loaded == nullptr, "Corrupt JSON yields null scene");
    }

    {
        const std::string DuplicateIds =
            R"({"format":"sakura.scene","formatVersion":1,"name":"Dup","objects":[{"id":"a","name":"A","parent":null,"components":[]},{"id":"a","name":"B","parent":null,"components":[]}]})";
        Scene* Loaded = nullptr;
        Expect(!SceneSerializer::DeserializeFromJson(DuplicateIds, Loaded).bOk, "Reject duplicate object ids");
    }

    {
        const std::string UnknownType =
            R"({"format":"sakura.scene","formatVersion":1,"name":"Unknown","objects":[{"id":"a","name":"A","parent":null,"components":[{"id":"c1","type":"game.MissingComponent","typeVersion":1,"properties":{}}]}]})";
        Scene* Loaded = nullptr;
        Expect(!SceneSerializer::DeserializeFromJson(UnknownType, Loaded).bOk, "Reject unknown component type");
    }

    {
        const std::string Cycle =
            R"({"format":"sakura.scene","formatVersion":1,"name":"Cycle","objects":[{"id":"a","name":"A","parent":"b","components":[]},{"id":"b","name":"B","parent":"a","components":[]}]})";
        Scene* Loaded = nullptr;
        Expect(!SceneSerializer::DeserializeFromJson(Cycle, Loaded).bOk, "Reject parent cycle");
    }

    {
        const std::string MissingParent =
            R"({"format":"sakura.scene","formatVersion":1,"name":"Missing","objects":[{"id":"a","name":"A","parent":"ghost","components":[]}]})";
        Scene* Loaded = nullptr;
        Expect(!SceneSerializer::DeserializeFromJson(MissingParent, Loaded).bOk, "Reject missing parent reference");
    }

    {
        Scene* Source = MemorySubsystem::Get()->NewObject<Scene>("Clipboard");
        GameObject* Parent = Source->CreateGameObject("CopiedParent");
        GameObject* Child = Source->CreateGameObject("CopiedChild");
        Child->SetParent(Parent);
        Child->AddComponent<CameraComponent>();

        std::string SerializedSubtree;
        std::string RemappedSubtree;
        Expect(
            SceneSerializer::SerializeSubtreeToJson(*Parent, SerializedSubtree).bOk,
            "Serialize clipboard subtree");
        Expect(
            SceneSerializer::RemapSubtreePersistentIds(SerializedSubtree, RemappedSubtree).bOk,
            "Remap clipboard subtree identities");
        GameObject* PastedRoot = nullptr;
        Expect(
            SceneSerializer::DeserializeSubtreeFromJson(*Source, RemappedSubtree, nullptr, PastedRoot).bOk,
            "Paste remapped subtree beside source");
        Expect(Source->GetObjectCount() == 4, "Pasted subtree duplicates all objects");
        Expect(
            PastedRoot != nullptr && PastedRoot->GetPersistentId() != Parent->GetPersistentId(),
            "Pasted root receives a new persistent identity");
        Expect(
            PastedRoot != nullptr
                && !PastedRoot->GetChildren().empty()
                && PastedRoot->GetChildren().front()->GetComponent<CameraComponent>() != nullptr,
            "Pasted subtree preserves hierarchy and components");
        MemorySubsystem::Get()->DestroyObject(Source);
    }

    {
        std::filesystem::create_directories(TempRoot / "blocked");
        const std::filesystem::path Blocker = TempRoot / "blocked" / "not_a_dir";
        {
            std::ofstream Stream(Blocker);
            Stream << "x";
        }
        const std::filesystem::path Impossible = Blocker / "child.scene";
        Scene* EmptyAllocated = MemorySubsystem::Get()->NewObject<Scene>("EmptyWrite");
        Expect(!SceneSerializer::SerializeToFile(*EmptyAllocated, Impossible).bOk, "Reject impossible write path");
        MemorySubsystem::Get()->DestroyObject(EmptyAllocated);
    }

    {
        ProjectGeneratorRequest Request{};
        Request.ParentDirectory = TempRoot;
        Request.ProjectName = "SceneProj";
        ProjectDescriptor Descriptor{};
        std::string Error;
        Expect(ProjectGenerator::CreateProject(Request, Descriptor, Error), "Create project with startupScene file");
        Expect(std::filesystem::exists(Descriptor.StartupScene), "startupScene file exists");

        ProjectSession Session;
        Session.BindEngine(&BoundEngine);
        Expect(Session.OpenProject(Descriptor.ProjectFile), "Open project loads startupScene");
        Expect(BoundEngine.GetActiveScene() != nullptr, "Active scene published from startupScene");
        Expect(BoundEngine.GetActiveScene()->IsLoaded(), "Startup scene marked loaded");

        SceneDocument Document;
        Document.BindEngine(&BoundEngine);
        Expect(Document.Open(Descriptor.StartupScene, Error), "SceneDocument open");
        Document.MarkDirty();
        Expect(Document.IsDirty(), "SceneDocument dirty before save");
        Expect(Document.Save(Error), "SceneDocument save");
        Expect(!Document.IsDirty(), "Dirty cleared only after successful save");

        const std::filesystem::path MissingStartup = TempRoot / "SceneProj" / "Content" / "Scenes" / "Gone.scene";
        Descriptor.StartupScene = MissingStartup;
        std::string SaveError;
        Expect(Descriptor.TrySaveToFile(SaveError), "Rewrite project with missing startupScene");
        Document.Close();
        Session.CloseProject();
        Expect(Session.OpenProject(Descriptor.ProjectFile), "Open still succeeds when startup missing");
        Expect(Session.GetHealth() == ProjectSessionHealth::Degraded, "Missing startupScene is Degraded");
        Expect(BoundEngine.GetActiveScene() == nullptr, "Missing startup does not publish empty scene");
        Session.CloseProject();
    }

    {
        const std::filesystem::path GoodScene = TempRoot / "good.scene";
        Scene* Built = MemorySubsystem::Get()->NewObject<Scene>("KeepMe");
        Built->CreateGameObject("Keep");
        Expect(SceneSerializer::SerializeToFile(*Built, GoodScene).bOk, "Write good scene fixture");
        MemorySubsystem::Get()->DestroyObject(Built);

        SceneDocument Document;
        Document.BindEngine(&BoundEngine);
        std::string Error;
        Expect(Document.Open(GoodScene, Error), "Open good document");
        Scene* First = Document.GetScene();
        Expect(First != nullptr && First->FindByName("Keep") != nullptr, "Good document contents");
        Expect(!Document.Open(TempRoot / "missing.scene", Error), "Failed open rejected");
        Expect(Document.GetScene() == First, "Failed open keeps previous document");
        Expect(Document.GetScene()->FindByName("Keep") != nullptr, "Previous document still intact");
        Document.Close();
    }

    {
        Scene* Source = MemorySubsystem::Get()->NewObject<Scene>("PersistentIds");
        GameObject* FirstObject = Source->CreateGameObject("First");
        GameObject* Survivor = Source->CreateGameObject("Survivor");
        const std::string PersistentId = Survivor->GetPersistentId();
        Source->DestroyGameObject(FirstObject);
        Survivor->SetName("Renamed");
        std::string Serialized;
        Expect(SceneSerializer::SerializeToJson(*Source, Serialized).bOk, "Serialize persistent identities");
        Scene* Loaded = nullptr;
        Expect(SceneSerializer::DeserializeFromJson(Serialized, Loaded).bOk, "Reload persistent identities");
        Expect(Loaded->FindByPersistentId(PersistentId) != nullptr, "Object ID survives reorder and rename");
        MemorySubsystem::Get()->DestroyObject(Source);
        BoundEngine.AdoptScene(std::unique_ptr<Scene>(Loaded));
        SceneDocument Document;
        Document.BindEngine(&BoundEngine);
        std::string Error;
        Expect(Document.AdoptScene(Loaded, TempRoot / "persistent.scene", Error), "Document observes engine scene");
        Expect(BoundEngine.GetPlaySession().StartPlay(), "Start play with observed scene");
        BoundEngine.UnloadProjectContent();
        Expect(!Document.IsOpen(), "Document detects scene destruction before document close");
        Expect(!BoundEngine.GetPlaySession().IsSimulating(), "Closing project stops play first");
        Document.Close();
        Expect(BoundEngine.GetActiveScene() == nullptr, "Repeated close is safe");
    }

    BoundEngine.Shutdown();
    std::error_code CleanupError;
    std::filesystem::remove_all(TempRoot, CleanupError);

    std::cout << "Failures: " << Failures << "\n" << std::flush;
    return Failures == 0 ? 0 : 1;
}
