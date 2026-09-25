#include "SceneDocument.h"

#include "Core/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Engine.h"
#include "Game/Scene.h"
#include "Game/SceneSerializer.h"

void SceneDocument::BindEngine(Engine* InEngine)
{
    BoundEngine = InEngine;
}

bool SceneDocument::AdoptScene(Scene* ExistingScene, const std::filesystem::path& AbsolutePath, std::string& OutError)
{
    LastError.clear();
    if (ExistingScene == nullptr)
    {
        OutError = "Scene is null";
        LastError = OutError;
        return false;
    }

    if (GetScene() == ExistingScene)
    {
        DocumentPath = AbsolutePath;
        bDirty = false;
        return true;
    }

    if (!ReplaceBoundScene(ExistingScene, AbsolutePath, OutError))
    {
        LastError = OutError;
        return false;
    }

    PrintString(std::string("SceneDocument: adopted scene ") + AbsolutePath.generic_string());
    return true;
}

void SceneDocument::MarkDirty()
{
    if (GetScene() != nullptr)
    {
        bDirty = true;
    }
}

Scene* SceneDocument::GetScene() const
{
    MemorySubsystem* Memory = MemorySubsystem::Get();
    if (Memory == nullptr)
    {
        return nullptr;
    }
    return Memory->ResolveHandle<Scene>(BoundScene);
}

void SceneDocument::Close()
{
    Scene* CurrentScene = GetScene();
    if (CurrentScene != nullptr && BoundEngine != nullptr && BoundEngine->GetEditScene() == CurrentScene)
    {
        BoundEngine->AdoptScene({});
    }
    BoundScene = {};
    DocumentPath.clear();
    bDirty = false;
    LastError.clear();
}

bool SceneDocument::ReplaceBoundScene(Scene* NewScene, const std::filesystem::path& Path, std::string& OutError)
{
    if (NewScene == nullptr || BoundEngine == nullptr
        || NewScene == BoundEngine->GetPlaySession().GetPlayWorld())
    {
        OutError = "Scene and engine are required";
        return false;
    }
    if (BoundEngine->GetEditScene() != NewScene)
    {
        BoundEngine->AdoptScene(std::unique_ptr<Scene>(NewScene));
    }
    BoundScene = NewScene->GetObjectHandle();
    DocumentPath = Path;
    bDirty = false;
    LastError.clear();
    return true;
}

bool SceneDocument::Open(const std::filesystem::path& AbsolutePath, std::string& OutError)
{
    LastError.clear();
    if (BoundEngine == nullptr)
    {
        OutError = "Engine is not bound";
        LastError = OutError;
        return false;
    }

    Scene* LoadedScene = nullptr;
    const SceneSerializeResult Loaded = SceneSerializer::DeserializeFromFile(AbsolutePath, LoadedScene);
    if (!Loaded.bOk || LoadedScene == nullptr)
    {
        OutError = Loaded.Error.empty() ? "Failed to load scene" : Loaded.Error;
        LastError = OutError;
        PrintString(std::string("SceneDocument: open failed: ") + OutError);
        return false;
    }

    if (!ReplaceBoundScene(LoadedScene, AbsolutePath, OutError))
    {
        if (MemorySubsystem* Memory = MemorySubsystem::Get())
        {
            Memory->DestroyObject(LoadedScene);
        }
        else
        {
            delete LoadedScene;
        }
        LastError = OutError;
        return false;
    }

    PrintString(std::string("SceneDocument: opened ") + AbsolutePath.generic_string());
    return true;
}

bool SceneDocument::Save(std::string& OutError)
{
    LastError.clear();
    if (GetScene() == nullptr)
    {
        OutError = "No scene document is open";
        LastError = OutError;
        return false;
    }
    if (DocumentPath.empty())
    {
        OutError = "Document path is empty";
        LastError = OutError;
        return false;
    }

    const SceneSerializeResult Saved = SceneSerializer::SerializeToFile(*GetScene(), DocumentPath);
    if (!Saved.bOk)
    {
        OutError = Saved.Error;
        LastError = OutError;
        PrintString(std::string("SceneDocument: save failed: ") + OutError);
        return false;
    }

    bDirty = false;
    PrintString(std::string("SceneDocument: saved ") + DocumentPath.generic_string());
    return true;
}

bool SceneDocument::SaveAs(const std::filesystem::path& AbsolutePath, std::string& OutError)
{
    LastError.clear();
    if (GetScene() == nullptr)
    {
        OutError = "No scene document is open";
        LastError = OutError;
        return false;
    }

    const SceneSerializeResult Saved = SceneSerializer::SerializeToFile(*GetScene(), AbsolutePath);
    if (!Saved.bOk)
    {
        OutError = Saved.Error;
        LastError = OutError;
        PrintString(std::string("SceneDocument: save-as failed: ") + OutError);
        return false;
    }

    DocumentPath = AbsolutePath;
    bDirty = false;
    PrintString(std::string("SceneDocument: saved as ") + AbsolutePath.generic_string());
    return true;
}
