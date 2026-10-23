#include "Commands/PrefabCommands.h"

#include "Core/Threading/ThreadContext.h"
#include "World/Scene.h"
#include "World/Serialization/SceneSerializer.h"
#include "World/GameObject.h"
#include "Prefab/PrefabUtility.h"

namespace
{
bool ReplaceSubtree(
    Scene& TargetScene,
    const std::string& CurrentRootId,
    const std::string& ParentId,
    const std::string& Document,
    ObjectHandle& OutRoot)
{
    GameObject* Current = TargetScene.FindByPersistentId(CurrentRootId);
    if (Current == nullptr)
    {
        return false;
    }
    GameObject* Parent = ParentId.empty() ? nullptr : TargetScene.FindByPersistentId(ParentId);
    TargetScene.DestroyGameObject(Current);
    GameObject* Restored = nullptr;
    const SceneSerializeResult Result = SceneSerializer::DeserializeSubtreeFromJson(TargetScene, Document, Parent, Restored);
    if (!Result.bOk || Restored == nullptr)
    {
        PrintString("PrefabCommands: subtree restore failed: " + Result.Error);
        return false;
    }
    OutRoot = Restored->GetObjectHandle();
    return true;
}

class InstantiatePrefabCommand : public EditorCommand
{
public:
    Scene* TargetScene = nullptr;
    AssetId Prefab{};
    ObjectHandle Parent;
    bool bHasTransform = false;
    Transform InitialTransform{};
    ObjectHandle* OutCreated = nullptr;

    std::string CreatedId;
    std::string ParentId;
    std::string Captured;
    ObjectHandle Created;

    bool Do() override
    {
        if (TargetScene == nullptr)
        {
            return false;
        }
        if (!Captured.empty())
        {
            GameObject* ParentObject = ParentId.empty() ? nullptr : TargetScene->FindByPersistentId(ParentId);
            GameObject* Restored = nullptr;
            if (!SceneSerializer::DeserializeSubtreeFromJson(*TargetScene, Captured, ParentObject, Restored).bOk || Restored == nullptr)
            {
                return false;
            }
            Created = Restored->GetObjectHandle();
            Captured.clear();
            Publish();
            return true;
        }

        GameObject* ParentObject = Parent.IsValid() ? TargetScene->FindByHandle(Parent) : nullptr;
        std::string Error;
        GameObject* Instance = PrefabUtility::InstantiatePrefab(*TargetScene, Prefab, ParentObject, Error);
        if (Instance == nullptr)
        {
            PrintString("InstantiatePrefabCommand failed: " + Error);
            return false;
        }
        if (bHasTransform)
        {
            Instance->SetTransform(InitialTransform);
        }
        CreatedId = Instance->GetPersistentId();
        ParentId = ParentObject != nullptr ? ParentObject->GetPersistentId() : std::string{};
        Created = Instance->GetObjectHandle();
        Publish();
        return true;
    }

    bool Undo() override
    {
        GameObject* Instance = TargetScene != nullptr ? TargetScene->FindByPersistentId(CreatedId) : nullptr;
        if (Instance == nullptr || !SceneSerializer::SerializeSubtreeToJson(*Instance, Captured).bOk)
        {
            return false;
        }
        ParentId = Instance->GetParent() != nullptr ? Instance->GetParent()->GetPersistentId() : std::string{};
        TargetScene->DestroyGameObject(Instance);
        Created = ObjectHandle{};
        return true;
    }

    const char* GetDisplayName() const override { return "Instantiate Prefab"; }

private:
    void Publish()
    {
        if (OutCreated != nullptr)
        {
            *OutCreated = Created;
            OutCreated = nullptr;
        }
    }
};

class PrefabSubtreeEditCommand : public EditorCommand
{
public:
    Scene* TargetScene = nullptr;
    ObjectHandle InitialRoot;
    std::string DisplayName;
    PrefabSubtreeMutation Mutation;
    std::string* OutError = nullptr;
    ObjectHandle* OutRoot = nullptr;

    bool bExecuted = false;
    std::string ParentId;
    std::string BeforeRootId;
    std::string AfterRootId;
    std::string Before;
    std::string After;

    bool Do() override
    {
        if (TargetScene == nullptr)
        {
            return false;
        }
        if (bExecuted)
        {
            ObjectHandle Restored;
            return ReplaceSubtree(*TargetScene, BeforeRootId, ParentId, After, Restored);
        }

        GameObject* Root = TargetScene->FindByHandle(InitialRoot);
        std::string Error;
        if (Root == nullptr || !Mutation)
        {
            Report("Target object no longer exists");
            return false;
        }
        BeforeRootId = Root->GetPersistentId();
        ParentId = Root->GetParent() != nullptr ? Root->GetParent()->GetPersistentId() : std::string{};
        if (!SceneSerializer::SerializeSubtreeToJson(*Root, Before).bOk)
        {
            Report("Failed to snapshot subtree");
            return false;
        }
        if (!Mutation(*TargetScene, *Root, Error))
        {
            ObjectHandle Restored;
            ReplaceSubtree(*TargetScene, Root->GetPersistentId(), ParentId, Before, Restored);
            Report(Error);
            return false;
        }
        AfterRootId = Root->GetPersistentId();
        if (!SceneSerializer::SerializeSubtreeToJson(*Root, After).bOk)
        {
            Report("Failed to snapshot subtree after edit");
            return false;
        }
        if (OutRoot != nullptr)
        {
            *OutRoot = Root->GetObjectHandle();
            OutRoot = nullptr;
        }
        OutError = nullptr;
        bExecuted = true;
        return true;
    }

    bool Undo() override
    {
        ObjectHandle Restored;
        return TargetScene != nullptr && ReplaceSubtree(*TargetScene, AfterRootId, ParentId, Before, Restored);
    }

    const char* GetDisplayName() const override { return DisplayName.c_str(); }

private:
    void Report(const std::string& Error)
    {
        if (OutError != nullptr)
        {
            *OutError = Error;
        }
    }
};
}

std::unique_ptr<EditorCommand> MakeInstantiatePrefabCommand(
    Scene* TargetScene,
    AssetId Prefab,
    ObjectHandle Parent,
    const Transform* InitialTransform,
    ObjectHandle* OutCreated)
{
    auto Command = std::make_unique<InstantiatePrefabCommand>();
    Command->TargetScene = TargetScene;
    Command->Prefab = Prefab;
    Command->Parent = Parent;
    Command->bHasTransform = InitialTransform != nullptr;
    if (InitialTransform != nullptr)
    {
        Command->InitialTransform = *InitialTransform;
    }
    Command->OutCreated = OutCreated;
    return Command;
}

std::unique_ptr<EditorCommand> MakePrefabSubtreeEditCommand(
    Scene* TargetScene,
    ObjectHandle Root,
    std::string DisplayName,
    PrefabSubtreeMutation Mutation,
    std::string* OutError,
    ObjectHandle* OutRoot)
{
    auto Command = std::make_unique<PrefabSubtreeEditCommand>();
    Command->TargetScene = TargetScene;
    Command->InitialRoot = Root;
    Command->DisplayName = std::move(DisplayName);
    Command->Mutation = std::move(Mutation);
    Command->OutError = OutError;
    Command->OutRoot = OutRoot;
    return Command;
}
