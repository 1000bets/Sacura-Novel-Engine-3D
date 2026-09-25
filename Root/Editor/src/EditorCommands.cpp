#include "EditorCommands.h"

#include "Core/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Game/Scene.h"
#include "Game/SceneSerializer.h"
#include "Gameplay/Component.h"
#include "Gameplay/GameObject.h"
#include "Reflection/Class.h"
#include "Reflection/PropertyAccess.h"
#include "Reflection/ReflectionSubsystem.h"

namespace
{
GameObject* ResolveObject(Scene* TargetScene, ObjectHandle Target)
{
    if (TargetScene == nullptr || !Target.IsValid())
    {
        return nullptr;
    }
    return TargetScene->FindByHandle(Target);
}

Component* ResolveComponent(Scene* TargetScene, ObjectHandle Owner, ObjectHandle ComponentTarget)
{
    GameObject* OwnerObject = ResolveObject(TargetScene, Owner);
    if (OwnerObject == nullptr || !ComponentTarget.IsValid())
    {
        return nullptr;
    }
    for (Component* ComponentInstance : OwnerObject->GetAllComponents())
    {
        if (ComponentInstance != nullptr && ComponentInstance->GetObjectHandle() == ComponentTarget)
        {
            return ComponentInstance;
        }
    }
    return nullptr;
}

class RenameObjectCommand : public EditorCommand
{
public:
    Scene* TargetScene = nullptr;
    ObjectHandle Target;
    std::string OldName;
    std::string NewName;

    bool Do() override
    {
        GameObject* ObjectInstance = ResolveObject(TargetScene, Target);
        if (ObjectInstance == nullptr)
        {
            return false;
        }
        if (OldName.empty())
        {
            OldName = ObjectInstance->GetName();
        }
        ObjectInstance->SetName(NewName);
        return true;
    }

    bool Undo() override
    {
        GameObject* ObjectInstance = ResolveObject(TargetScene, Target);
        if (ObjectInstance == nullptr)
        {
            return false;
        }
        ObjectInstance->SetName(OldName);
        return true;
    }

    const char* GetDisplayName() const override { return "Rename Object"; }
};

class SetTransformCommand : public EditorCommand
{
public:
    Scene* TargetScene = nullptr;
    ObjectHandle Target;
    Transform OldTransform;
    Transform NewTransform;
    bool bCapturedOld = false;

    bool Do() override
    {
        GameObject* ObjectInstance = ResolveObject(TargetScene, Target);
        if (ObjectInstance == nullptr)
        {
            return false;
        }
        if (!bCapturedOld)
        {
            OldTransform = ObjectInstance->GetTransform();
            bCapturedOld = true;
        }
        ObjectInstance->SetTransform(NewTransform);
        return true;
    }

    bool Undo() override
    {
        GameObject* ObjectInstance = ResolveObject(TargetScene, Target);
        if (ObjectInstance == nullptr)
        {
            return false;
        }
        ObjectInstance->SetTransform(OldTransform);
        return true;
    }

    const char* GetDisplayName() const override { return "Set Transform"; }
};

class CreateObjectCommand : public EditorCommand
{
public:
    Scene* TargetScene = nullptr;
    std::string Name;
    ObjectHandle Parent;
    ObjectHandle Created;
    std::string CapturedSubtree;
    ObjectHandle CapturedParent;
    ObjectHandle* OutCreated = nullptr;

    bool Do() override
    {
        if (TargetScene == nullptr)
        {
            return false;
        }

        if (!CapturedSubtree.empty())
        {
            GameObject* ParentObject = ResolveObject(TargetScene, CapturedParent);
            GameObject* Restored = nullptr;
            SceneSerializeResult RestoredResult = SceneSerializer::DeserializeSubtreeFromJson(
                *TargetScene,
                CapturedSubtree,
                ParentObject,
                Restored);
            if (!RestoredResult.bOk || Restored == nullptr)
            {
                PrintString(std::string("CreateObjectCommand redo failed: ") + RestoredResult.Error);
                return false;
            }
            Created = Restored->GetObjectHandle();
            if (OutCreated != nullptr)
            {
                *OutCreated = Created;
                OutCreated = nullptr;
            }
            CapturedSubtree.clear();
            return true;
        }

        GameObject* ParentObject = ResolveObject(TargetScene, Parent);
        GameObject* ObjectInstance = TargetScene->CreateGameObject(Name.empty() ? "GameObject" : Name);
        if (ObjectInstance == nullptr)
        {
            return false;
        }
        if (ParentObject != nullptr && !ObjectInstance->SetParent(ParentObject))
        {
            TargetScene->DestroyGameObject(ObjectInstance);
            return false;
        }
        Created = ObjectInstance->GetObjectHandle();
        if (OutCreated != nullptr)
        {
            *OutCreated = Created;
            OutCreated = nullptr;
        }
        return true;
    }

    bool Undo() override
    {
        GameObject* ObjectInstance = ResolveObject(TargetScene, Created);
        if (ObjectInstance == nullptr)
        {
            return false;
        }

        CapturedParent = ObjectHandle{};
        if (ObjectInstance->GetParent() != nullptr)
        {
            CapturedParent = ObjectInstance->GetParent()->GetObjectHandle();
        }

        SceneSerializeResult Captured = SceneSerializer::SerializeSubtreeToJson(*ObjectInstance, CapturedSubtree);
        if (!Captured.bOk)
        {
            PrintString(std::string("CreateObjectCommand undo capture failed: ") + Captured.Error);
            return false;
        }

        if (!TargetScene->DestroyGameObject(ObjectInstance))
        {
            CapturedSubtree.clear();
            return false;
        }
        Created = ObjectHandle{};
        return true;
    }

    const char* GetDisplayName() const override { return "Create Object"; }
};

class PasteSubtreeCommand : public EditorCommand
{
public:
    Scene* TargetScene = nullptr;
    ObjectHandle Parent;
    ObjectHandle Created;
    std::string CapturedSubtree;
    ObjectHandle* OutCreated = nullptr;

    bool Do() override
    {
        if (TargetScene == nullptr || CapturedSubtree.empty())
        {
            return false;
        }

        GameObject* ParentObject = ResolveObject(TargetScene, Parent);
        if (Parent.IsValid() && ParentObject == nullptr)
        {
            return false;
        }

        GameObject* Restored = nullptr;
        SceneSerializeResult RestoredResult = SceneSerializer::DeserializeSubtreeFromJson(
            *TargetScene,
            CapturedSubtree,
            ParentObject,
            Restored);
        if (!RestoredResult.bOk || Restored == nullptr)
        {
            PrintString(std::string("PasteSubtreeCommand failed: ") + RestoredResult.Error);
            return false;
        }

        Created = Restored->GetObjectHandle();
        CapturedSubtree.clear();
        if (OutCreated != nullptr)
        {
            *OutCreated = Created;
            OutCreated = nullptr;
        }
        return true;
    }

    bool Undo() override
    {
        GameObject* ObjectInstance = ResolveObject(TargetScene, Created);
        if (ObjectInstance == nullptr)
        {
            return false;
        }

        SceneSerializeResult Captured = SceneSerializer::SerializeSubtreeToJson(*ObjectInstance, CapturedSubtree);
        if (!Captured.bOk)
        {
            PrintString(std::string("PasteSubtreeCommand undo capture failed: ") + Captured.Error);
            return false;
        }
        if (!TargetScene->DestroyGameObject(ObjectInstance))
        {
            CapturedSubtree.clear();
            return false;
        }
        Created = ObjectHandle{};
        return true;
    }

    const char* GetDisplayName() const override { return "Paste Object"; }
};

class DeleteObjectCommand : public EditorCommand
{
public:
    Scene* TargetScene = nullptr;
    ObjectHandle Target;
    ObjectHandle Parent;
    std::string CapturedSubtree;
    ObjectHandle RestoredRoot;

    bool Do() override
    {
        GameObject* ObjectInstance = ResolveObject(TargetScene, Target);
        if (ObjectInstance == nullptr && RestoredRoot.IsValid())
        {
            ObjectInstance = ResolveObject(TargetScene, RestoredRoot);
        }
        if (ObjectInstance == nullptr)
        {
            return false;
        }

        Parent = ObjectHandle{};
        if (ObjectInstance->GetParent() != nullptr)
        {
            Parent = ObjectInstance->GetParent()->GetObjectHandle();
        }

        SceneSerializeResult Captured = SceneSerializer::SerializeSubtreeToJson(*ObjectInstance, CapturedSubtree);
        if (!Captured.bOk)
        {
            PrintString(std::string("DeleteObjectCommand capture failed: ") + Captured.Error);
            return false;
        }

        if (!TargetScene->DestroyGameObject(ObjectInstance))
        {
            CapturedSubtree.clear();
            return false;
        }
        Target = ObjectHandle{};
        RestoredRoot = ObjectHandle{};
        return true;
    }

    bool Undo() override
    {
        if (CapturedSubtree.empty() || TargetScene == nullptr)
        {
            return false;
        }

        GameObject* ParentObject = ResolveObject(TargetScene, Parent);
        GameObject* Restored = nullptr;
        SceneSerializeResult RestoredResult = SceneSerializer::DeserializeSubtreeFromJson(
            *TargetScene,
            CapturedSubtree,
            ParentObject,
            Restored);
        if (!RestoredResult.bOk || Restored == nullptr)
        {
            PrintString(std::string("DeleteObjectCommand undo failed: ") + RestoredResult.Error);
            return false;
        }

        RestoredRoot = Restored->GetObjectHandle();
        Target = RestoredRoot;
        return true;
    }

    const char* GetDisplayName() const override { return "Delete Object"; }
};

class ReparentObjectCommand : public EditorCommand
{
public:
    Scene* TargetScene = nullptr;
    ObjectHandle Target;
    ObjectHandle OldParent;
    ObjectHandle NewParent;
    bool bCapturedOld = false;

    bool Do() override
    {
        GameObject* ObjectInstance = ResolveObject(TargetScene, Target);
        if (ObjectInstance == nullptr)
        {
            return false;
        }
        if (!bCapturedOld)
        {
            OldParent = ObjectHandle{};
            if (ObjectInstance->GetParent() != nullptr)
            {
                OldParent = ObjectInstance->GetParent()->GetObjectHandle();
            }
            bCapturedOld = true;
        }

        GameObject* ParentObject = ResolveObject(TargetScene, NewParent);
        if (NewParent.IsValid() && ParentObject == nullptr)
        {
            return false;
        }
        return ObjectInstance->SetParent(ParentObject);
    }

    bool Undo() override
    {
        GameObject* ObjectInstance = ResolveObject(TargetScene, Target);
        if (ObjectInstance == nullptr)
        {
            return false;
        }
        GameObject* ParentObject = ResolveObject(TargetScene, OldParent);
        if (OldParent.IsValid() && ParentObject == nullptr)
        {
            return false;
        }
        return ObjectInstance->SetParent(ParentObject);
    }

    const char* GetDisplayName() const override { return "Reparent Object"; }
};

class AddComponentCommand : public EditorCommand
{
public:
    Scene* TargetScene = nullptr;
    ObjectHandle Owner;
    TypeId ComponentType;
    ObjectHandle CreatedComponent;
    std::string CapturedComponent;
    ObjectHandle* OutCreatedComponent = nullptr;

    bool Do() override
    {
        GameObject* OwnerObject = ResolveObject(TargetScene, Owner);
        if (OwnerObject == nullptr)
        {
            return false;
        }

        if (!CapturedComponent.empty())
        {
            Component* Restored = nullptr;
            SceneSerializeResult RestoredResult = SceneSerializer::DeserializeComponentFromJson(
                *OwnerObject,
                CapturedComponent,
                Restored);
            if (!RestoredResult.bOk || Restored == nullptr)
            {
                return false;
            }
            CreatedComponent = Restored->GetObjectHandle();
            if (OutCreatedComponent != nullptr)
            {
                *OutCreatedComponent = CreatedComponent;
                OutCreatedComponent = nullptr;
            }
            CapturedComponent.clear();
            return true;
        }

        Object* Created = ReflectionSubsystem::Get().CreateInstance(ComponentType);
        Component* ComponentInstance = dynamic_cast<Component*>(Created);
        if (ComponentInstance == nullptr)
        {
            if (Created != nullptr)
            {
                if (MemorySubsystem* Memory = MemorySubsystem::Get())
                {
                    Memory->DestroyObject(Created);
                }
                else
                {
                    delete Created;
                }
            }
            return false;
        }

        if (OwnerObject->AddExistingComponent(ComponentInstance, true) == nullptr)
        {
            if (MemorySubsystem* Memory = MemorySubsystem::Get())
            {
                Memory->DestroyObject(ComponentInstance);
            }
            else
            {
                delete ComponentInstance;
            }
            return false;
        }

        CreatedComponent = ComponentInstance->GetObjectHandle();
        if (OutCreatedComponent != nullptr)
        {
            *OutCreatedComponent = CreatedComponent;
            OutCreatedComponent = nullptr;
        }
        return true;
    }

    bool Undo() override
    {
        Component* ComponentInstance = ResolveComponent(TargetScene, Owner, CreatedComponent);
        GameObject* OwnerObject = ResolveObject(TargetScene, Owner);
        if (ComponentInstance == nullptr || OwnerObject == nullptr)
        {
            return false;
        }

        SceneSerializeResult Captured = SceneSerializer::SerializeComponentToJson(*ComponentInstance, CapturedComponent);
        if (!Captured.bOk)
        {
            return false;
        }

        OwnerObject->RemoveComponent(ComponentInstance);
        CreatedComponent = ObjectHandle{};
        return true;
    }

    const char* GetDisplayName() const override { return "Add Component"; }
};

class RemoveComponentCommand : public EditorCommand
{
public:
    Scene* TargetScene = nullptr;
    ObjectHandle Owner;
    ObjectHandle ComponentTarget;
    std::string CapturedComponent;
    ObjectHandle RestoredComponent;

    bool Do() override
    {
        Component* ComponentInstance = ResolveComponent(TargetScene, Owner, ComponentTarget);
        if (ComponentInstance == nullptr && RestoredComponent.IsValid())
        {
            ComponentInstance = ResolveComponent(TargetScene, Owner, RestoredComponent);
        }
        GameObject* OwnerObject = ResolveObject(TargetScene, Owner);
        if (ComponentInstance == nullptr || OwnerObject == nullptr)
        {
            return false;
        }

        SceneSerializeResult Captured = SceneSerializer::SerializeComponentToJson(*ComponentInstance, CapturedComponent);
        if (!Captured.bOk)
        {
            return false;
        }

        OwnerObject->RemoveComponent(ComponentInstance);
        ComponentTarget = ObjectHandle{};
        RestoredComponent = ObjectHandle{};
        return true;
    }

    bool Undo() override
    {
        GameObject* OwnerObject = ResolveObject(TargetScene, Owner);
        if (OwnerObject == nullptr || CapturedComponent.empty())
        {
            return false;
        }

        Component* Restored = nullptr;
        SceneSerializeResult RestoredResult = SceneSerializer::DeserializeComponentFromJson(
            *OwnerObject,
            CapturedComponent,
            Restored);
        if (!RestoredResult.bOk || Restored == nullptr)
        {
            return false;
        }

        RestoredComponent = Restored->GetObjectHandle();
        ComponentTarget = RestoredComponent;
        return true;
    }

    const char* GetDisplayName() const override { return "Remove Component"; }
};

class SetPropertyCommand : public EditorCommand
{
public:
    Scene* TargetScene = nullptr;
    ObjectHandle Target;
    PropertyId Property;
    ReflectedValue OldValue;
    ReflectedValue NewValue;
    bool bCapturedOld = false;

    Object* ResolveTargetObject() const
    {
        if (TargetScene == nullptr)
        {
            return nullptr;
        }
        if (GameObject* ObjectInstance = ResolveObject(TargetScene, Target))
        {
            return ObjectInstance;
        }
        for (GameObject* ObjectInstance : TargetScene->GetAllObjects())
        {
            if (ObjectInstance == nullptr)
            {
                continue;
            }
            for (Component* ComponentInstance : ObjectInstance->GetAllComponents())
            {
                if (ComponentInstance != nullptr && ComponentInstance->GetObjectHandle() == Target)
                {
                    return ComponentInstance;
                }
            }
        }
        return nullptr;
    }

    bool Do() override
    {
        Object* Instance = ResolveTargetObject();
        if (Instance == nullptr)
        {
            return false;
        }
        if (!bCapturedOld)
        {
            ReflectionDiagnostic Got = PropertyAccess::GetProperty(
                Instance,
                Property,
                OldValue,
                PropertyAccessContext::Inspector);
            if (!Got.bOk)
            {
                return false;
            }
            bCapturedOld = true;
        }

        ReflectionDiagnostic SetResult = PropertyAccess::SetProperty(
            Instance,
            Property,
            NewValue,
            PropertyAccessContext::Inspector);
        return SetResult.bOk;
    }

    bool Undo() override
    {
        Object* Instance = ResolveTargetObject();
        if (Instance == nullptr)
        {
            return false;
        }
        ReflectionDiagnostic SetResult = PropertyAccess::SetProperty(
            Instance,
            Property,
            OldValue,
            PropertyAccessContext::Inspector);
        return SetResult.bOk;
    }

    const char* GetDisplayName() const override { return "Set Property"; }
};

class ResetPropertyCommand : public EditorCommand
{
public:
    Scene* TargetScene = nullptr;
    ObjectHandle Target;
    PropertyId Property;
    ReflectedValue OldValue;
    bool bCapturedOld = false;

    Object* ResolveTargetObject() const
    {
        if (TargetScene == nullptr)
        {
            return nullptr;
        }
        if (GameObject* ObjectInstance = ResolveObject(TargetScene, Target))
        {
            return ObjectInstance;
        }
        for (GameObject* ObjectInstance : TargetScene->GetAllObjects())
        {
            if (ObjectInstance == nullptr)
            {
                continue;
            }
            for (Component* ComponentInstance : ObjectInstance->GetAllComponents())
            {
                if (ComponentInstance != nullptr && ComponentInstance->GetObjectHandle() == Target)
                {
                    return ComponentInstance;
                }
            }
        }
        return nullptr;
    }

    bool Do() override
    {
        Object* Instance = ResolveTargetObject();
        if (Instance == nullptr || Instance->GetClass() == nullptr)
        {
            return false;
        }
        Object* DefaultObject = Instance->GetClass()->GetClassDefaultObject();
        if (DefaultObject == nullptr)
        {
            return false;
        }

        if (!bCapturedOld)
        {
            ReflectionDiagnostic Got = PropertyAccess::GetProperty(
                Instance,
                Property,
                OldValue,
                PropertyAccessContext::Inspector);
            if (!Got.bOk)
            {
                return false;
            }
            bCapturedOld = true;
        }

        ReflectedValue DefaultValue;
        ReflectionDiagnostic GotDefault = PropertyAccess::GetProperty(
            DefaultObject,
            Property,
            DefaultValue,
            PropertyAccessContext::Inspector);
        if (!GotDefault.bOk)
        {
            return false;
        }

        ReflectionDiagnostic SetResult = PropertyAccess::SetProperty(
            Instance,
            Property,
            DefaultValue,
            PropertyAccessContext::Inspector);
        return SetResult.bOk;
    }

    bool Undo() override
    {
        Object* Instance = ResolveTargetObject();
        if (Instance == nullptr)
        {
            return false;
        }
        ReflectionDiagnostic SetResult = PropertyAccess::SetProperty(
            Instance,
            Property,
            OldValue,
            PropertyAccessContext::Inspector);
        return SetResult.bOk;
    }

    const char* GetDisplayName() const override { return "Reset Property"; }
};
}

std::unique_ptr<EditorCommand> MakeRenameObjectCommand(Scene* TargetScene, ObjectHandle Target, std::string NewName)
{
    auto Command = std::make_unique<RenameObjectCommand>();
    Command->TargetScene = TargetScene;
    Command->Target = Target;
    Command->NewName = std::move(NewName);
    return Command;
}

std::unique_ptr<EditorCommand> MakeSetTransformCommand(Scene* TargetScene, ObjectHandle Target, Transform NewTransform)
{
    auto Command = std::make_unique<SetTransformCommand>();
    Command->TargetScene = TargetScene;
    Command->Target = Target;
    Command->NewTransform = NewTransform;
    return Command;
}

std::unique_ptr<EditorCommand> MakeCreateObjectCommand(
    Scene* TargetScene,
    std::string Name,
    ObjectHandle Parent,
    ObjectHandle* OutCreated)
{
    auto Command = std::make_unique<CreateObjectCommand>();
    Command->TargetScene = TargetScene;
    Command->Name = std::move(Name);
    Command->Parent = Parent;
    Command->OutCreated = OutCreated;
    return Command;
}

std::unique_ptr<EditorCommand> MakePasteSubtreeCommand(
    Scene* TargetScene,
    std::string SerializedSubtree,
    ObjectHandle Parent,
    ObjectHandle* OutCreated)
{
    std::string RemappedSubtree;
    SceneSerializeResult Remapped = SceneSerializer::RemapSubtreePersistentIds(
        SerializedSubtree,
        RemappedSubtree);
    if (!Remapped.bOk)
    {
        PrintString(std::string("MakePasteSubtreeCommand failed: ") + Remapped.Error);
        return nullptr;
    }

    auto Command = std::make_unique<PasteSubtreeCommand>();
    Command->TargetScene = TargetScene;
    Command->Parent = Parent;
    Command->CapturedSubtree = std::move(RemappedSubtree);
    Command->OutCreated = OutCreated;
    return Command;
}

std::unique_ptr<EditorCommand> MakeDeleteObjectCommand(Scene* TargetScene, ObjectHandle Target)
{
    auto Command = std::make_unique<DeleteObjectCommand>();
    Command->TargetScene = TargetScene;
    Command->Target = Target;
    return Command;
}

std::unique_ptr<EditorCommand> MakeReparentObjectCommand(Scene* TargetScene, ObjectHandle Target, ObjectHandle NewParent)
{
    auto Command = std::make_unique<ReparentObjectCommand>();
    Command->TargetScene = TargetScene;
    Command->Target = Target;
    Command->NewParent = NewParent;
    return Command;
}

std::unique_ptr<EditorCommand> MakeAddComponentCommand(
    Scene* TargetScene,
    ObjectHandle Owner,
    TypeId ComponentType,
    ObjectHandle* OutCreatedComponent)
{
    auto Command = std::make_unique<AddComponentCommand>();
    Command->TargetScene = TargetScene;
    Command->Owner = Owner;
    Command->ComponentType = ComponentType;
    Command->OutCreatedComponent = OutCreatedComponent;
    return Command;
}

std::unique_ptr<EditorCommand> MakeRemoveComponentCommand(Scene* TargetScene, ObjectHandle Owner, ObjectHandle Component)
{
    auto Command = std::make_unique<RemoveComponentCommand>();
    Command->TargetScene = TargetScene;
    Command->Owner = Owner;
    Command->ComponentTarget = Component;
    return Command;
}

std::unique_ptr<EditorCommand> MakeSetPropertyCommand(
    Scene* TargetScene,
    ObjectHandle Target,
    PropertyId Property,
    ReflectedValue NewValue)
{
    auto Command = std::make_unique<SetPropertyCommand>();
    Command->TargetScene = TargetScene;
    Command->Target = Target;
    Command->Property = Property;
    Command->NewValue = std::move(NewValue);
    return Command;
}

std::unique_ptr<EditorCommand> MakeResetPropertyCommand(
    Scene* TargetScene,
    ObjectHandle Target,
    PropertyId Property)
{
    auto Command = std::make_unique<ResetPropertyCommand>();
    Command->TargetScene = TargetScene;
    Command->Target = Target;
    Command->Property = Property;
    return Command;
}
