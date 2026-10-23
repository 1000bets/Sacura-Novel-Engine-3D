#include "Assets/AssetManager.h"
#include "Materials/MaterialDocumentIO.h"
#include "Reflection/ReflectionValueCodec.h"
#include <set>
#include <algorithm>
#include "Commands/StaticMeshCommands.h"
#include "Assets/StaticMeshDefaults.h"
#include "Core/Object/MemorySubsystem.h"
#include "World/Components/PhysicsBodyComponent.h"
#include "World/Components/MeshRendererComponent.h"
#include "World/Scene.h"
#include "World/GameObject.h"
#include "World/Serialization/SceneSerializer.h"

std::unique_ptr<EditorCommand> MakeCreateStaticMeshObjectCommand(Scene* World, const AssetRegistry& Registry,
    const AssetKey& Asset, const std::string& Name, ObjectHandle Parent, const Transform& InitialTransform,
    ObjectHandle* Created)
{
    AssetRegistryEntry Entry;
    const SubAssetRecord* Subasset = nullptr;
    StaticMeshDefaults Defaults;
    std::string Error;
    if (!Registry.TryResolveKey(Asset, Entry, &Subasset) || !Subasset || Subasset->Type != StaticMeshAssetType
        || !StaticMeshDefaults::Parse(StaticMeshDefaults::Read(Entry.Metadata, Asset), Defaults, Error))
    {
        return nullptr;
    }
    auto* Memory = MemorySubsystem::Get();
    auto* Draft = Memory->NewObject<Scene>();
    auto* Object = Draft->CreateGameObject(Name);
    Object->SetTransform(InitialTransform);
    Object->AddComponent<MeshRendererComponent>()->SetMeshAssetKey(Asset);
    if (Defaults.bCollisionEnabled)
    {
        auto* Body = Object->AddComponent<PhysicsBodyComponent>();
        Body->bUseMeshDefaults = true;
        Body->CollisionMeshAssetId = Asset.Asset.ToString();
        Body->CollisionMeshSubAssetId = Asset.SubAsset->ToString();
        const auto& Element = Defaults.CollisionElements.front();
        Body->ShapeType = Element.ShapeType;
        Body->Center = Element.Center;
        Body->HalfExtents = Element.HalfExtents;
        Body->Radius = Element.Radius;
        Body->CapsuleHalfHeight = Element.CapsuleHalfHeight;
    }
    std::string Serialized;
    const auto Result = SceneSerializer::SerializeSubtreeToJson(*Object, Serialized);
    Memory->DestroyObject(Draft);
    if (!Result.bOk)
    {
        return nullptr;
    }
    return MakePasteSubtreeCommand(World, std::move(Serialized), Parent, Created);
}

class AssignMeshMaterialCommand : public EditorCommand
{
public:
    std::unique_ptr<EditorCommand> Global;
    std::unique_ptr<EditorCommand> Slots;
    bool Do() override
    {
        if (!Global->Do())
        {
            return false;
        }
        if (!Slots->Do())
        {
            Global->Undo();
            return false;
        }
        return true;
    }
    bool Undo() override
    {
        if (!Slots->Undo())
        {
            return false;
        }
        if (!Global->Undo())
        {
            Slots->Do();
            return false;
        }
        return true;
    }
    const char* GetDisplayName() const override { return "Assign Mesh Material"; }
};

std::unique_ptr<EditorCommand> MakeAssignMeshMaterialCommand(Scene* World, const AssetRegistry& Registry,
    ObjectHandle MeshIdentity, int32_t Slot, const AssetKey& Material, std::string& Error)
{
    auto* Mesh = dynamic_cast<MeshRendererComponent*>(MemorySubsystem::Get()->ResolveHandle(MeshIdentity));
    if (!World || World->IsPlaying() || !Mesh || Mesh->GetGameObject()->GetScene() != World || Slot < -1)
    {
        Error = "Mesh is unavailable or the scene is playing";
        return nullptr;
    }
    if (Material.IsValid())
    {
        ResolvedMaterial Resolved;
        if (MaterialDocumentIO::Resolve(Registry, Material, Resolved).HasError() || Resolved.Definition.Domain != MaterialDomain::Surface)
        {
            Error = "Choose a valid Surface material";
            return nullptr;
        }
    }
    std::set<int32_t> SlotNumbers;
    for (const auto& Part : Mesh->Submeshes)
    {
        SlotNumbers.insert(Part.MaterialSlot);
    }
    AssetHandle<StaticMeshResource> CpuMesh;
    if (World->GetAssetManager() && World->GetAssetManager()->TryGetLoaded(Mesh->GetMeshAssetKey(), CpuMesh))
    {
        for (const auto& Part : CpuMesh->Submeshes)
        {
            SlotNumbers.insert(Part.MaterialSlot);
        }
    }
    for (const auto& Entry : Mesh->MaterialSlots.Entries)
    {
        SlotNumbers.insert(Entry.Slot);
    }
    if (SlotNumbers.empty())
    {
        Error = "Wait for the mesh material slots to load";
        return nullptr;
    }
    if (Slot >= 0 && !SlotNumbers.count(Slot))
    {
        Error = "Material slot does not exist";
        return nullptr;
    }
    auto Updated = Mesh->MaterialSlots;
    const AssetKey Global = Mesh->GetMaterialAssetKey();
    if (Global.IsValid())
    {
        for (int32_t Number : SlotNumbers)
        {
            const auto Found = std::find_if(Updated.Entries.begin(), Updated.Entries.end(), [Number](const auto& Entry)
            {
                return Entry.Slot == Number;
            });
            if (Found == Updated.Entries.end())
            {
                Updated.Entries.push_back({Number, Global});
            }
        }
    }
    Updated.Entries.erase(std::remove_if(Updated.Entries.begin(), Updated.Entries.end(), [Slot](const auto& Entry)
    {
        return Slot < 0 || Entry.Slot == Slot;
    }), Updated.Entries.end());
    if (Material.IsValid())
    {
        for (int32_t Number : SlotNumbers)
        {
            if (Slot < 0 || Slot == Number)
            {
                Updated.Entries.push_back({Number, Material});
            }
        }
    }
    ReflectedValue Value;
    if (!ReflectionValueCodec<MaterialSlotOverrides>::ToReflected(Updated, Value).bOk)
    {
        return nullptr;
    }
    auto Command = std::make_unique<AssignMeshMaterialCommand>();
    Command->Global = MakeSetPropertyCommand(World, MeshIdentity, PropertyId{"material_asset_id"}, ReflectedValue::MakeString({}));
    Command->Slots = MakeSetPropertyCommand(World, MeshIdentity, PropertyId{"material_slots"}, Value);
    return Command;
}

class SetMeshPhysicsCommand : public EditorCommand
{
public:
    ObjectHandle WorldIdentity;
    std::string OwnerId;
    std::string BodyId;
    std::string CreatedBody;
    bool bOldEnabled = false;
    bool bNewEnabled = false;
    std::unique_ptr<EditorCommand> Motion;

    GameObject* ResolveOwner() const
    {
        auto* World = dynamic_cast<Scene*>(MemorySubsystem::Get()->ResolveHandle(WorldIdentity));
        if (!World || World->IsPlaying())
        {
            return nullptr;
        }
        return World->FindByPersistentId(OwnerId);
    }
    PhysicsBodyComponent* ResolveBody(GameObject* Owner) const
    {
        for (auto* Body : Owner->GetComponents<PhysicsBodyComponent>())
        {
            if (Body->GetPersistentId() == BodyId)
            {
                return Body;
            }
        }
        return nullptr;
    }
    bool Do() override
    {
        auto* Owner = ResolveOwner();
        if (!Owner)
        {
            return false;
        }
        if (!CreatedBody.empty())
        {
            if (!Owner->GetComponents<PhysicsBodyComponent>().empty())
            {
                return false;
            }
            Component* Restored = nullptr;
            return SceneSerializer::DeserializeComponentFromJson(*Owner, CreatedBody, Restored).bOk;
        }
        auto* Body = ResolveBody(Owner);
        if (!Body || !Motion->Do())
        {
            return false;
        }
        Body->SetEnabled(bNewEnabled);
        return true;
    }
    bool Undo() override
    {
        auto* Owner = ResolveOwner();
        if (!Owner)
        {
            return false;
        }
        auto* Body = ResolveBody(Owner);
        if (!Body)
        {
            return false;
        }
        if (!CreatedBody.empty())
        {
            Owner->RemoveComponent(Body);
            return true;
        }
        if (!Motion->Undo())
        {
            return false;
        }
        Body->SetEnabled(bOldEnabled);
        return true;
    }
    const char* GetDisplayName() const override { return "Set Mesh Physics"; }
};

std::unique_ptr<EditorCommand> MakeSetMeshPhysicsCommand(Scene* World, const AssetRegistry& Registry,
    ObjectHandle MeshIdentity, bool bEnabled, bool bSimulate, std::string& Error)
{
    auto* Mesh = dynamic_cast<MeshRendererComponent*>(MemorySubsystem::Get()->ResolveHandle(MeshIdentity));
    if (!World || World->IsPlaying() || !Mesh || Mesh->GetGameObject()->GetScene() != World)
    {
        Error = "Mesh is unavailable or the scene is playing";
        return nullptr;
    }
    auto* Owner = Mesh->GetGameObject();
    const auto Bodies = Owner->GetComponents<PhysicsBodyComponent>();
    if (Bodies.size() > 1 || (!Bodies.empty() && dynamic_cast<CharacterControllerComponent*>(Bodies.front())))
    {
        Error = "Configure multiple bodies or a character controller in its own component";
        return nullptr;
    }
    PhysicsBodyComponent* Body = nullptr;
    if (!Bodies.empty())
    {
        Body = Bodies.front();
    }
    StaticMeshDefaults Defaults;
    AssetRegistryEntry Entry;
    const bool bHasDefaults = Registry.TryResolveKey(Mesh->GetMeshAssetKey(), Entry)
        && StaticMeshDefaults::Parse(StaticMeshDefaults::Read(Entry.Metadata, Mesh->GetMeshAssetKey()), Defaults, Error);
    bool bTriangle = false;
    if (!Body || Body->bUseMeshDefaults)
    {
        if (bHasDefaults && Defaults.bCollisionEnabled)
        {
            for (const auto& Element : Defaults.CollisionElements)
            {
                bTriangle = bTriangle || Element.ShapeType == 3;
            }
        }
    }
    else
    {
        bTriangle = Body->ShapeType == 3;
    }
    if (bEnabled && bSimulate && bTriangle)
    {
        Error = "Triangle mesh collision cannot simulate a dynamic body. Use Box, Sphere or Capsule collision.";
        return nullptr;
    }
    auto Command = std::make_unique<SetMeshPhysicsCommand>();
    Command->WorldIdentity = World->GetObjectHandle();
    Command->OwnerId = Owner->GetPersistentId();
    Command->bNewEnabled = bEnabled;
    int64_t MotionType = 0;
    if (Body)
    {
        MotionType = Body->MotionType;
    }
    if (bEnabled && bSimulate)
    {
        MotionType = 1;
    }
    else if (bEnabled && MotionType == 1)
    {
        MotionType = 0;
    }
    if (Body)
    {
        if (Body->MotionType == 3)
        {
            Error = "Configure character motion in its own component";
            return nullptr;
        }
        Command->BodyId = Body->GetPersistentId();
        Command->bOldEnabled = Body->IsEnabled();
        Command->Motion = MakeSetPropertyCommand(World, Body->GetObjectHandle(), PropertyId{"motion_type"}, ReflectedValue::MakeInt64(MotionType));
        return Command;
    }
    if (!bEnabled)
    {
        Error = "The object has no physics body";
        return nullptr;
    }
    const bool bInherit = bHasDefaults && Defaults.bCollisionEnabled;
    AxisAlignedBounds Bounds = Mesh->LocalBounds;
    if (!bInherit)
    {
        AssetHandle<StaticMeshResource> CpuMesh;
        if (!World->GetAssetManager() || !World->GetAssetManager()->TryGetLoaded(Mesh->GetMeshAssetKey(), CpuMesh))
        {
            Error = "Wait for the mesh to finish loading";
            return nullptr;
        }
        Bounds = CpuMesh->Bounds;
    }
    auto* Draft = MemorySubsystem::Get()->NewObject<Scene>();
    auto* DraftBody = Draft->CreateGameObject("Physics Draft")->AddComponent<PhysicsBodyComponent>();
    DraftBody->MotionType = MotionType;
    DraftBody->bUseMeshDefaults = bInherit;
    DraftBody->Center = (Bounds.Minimum + Bounds.Maximum) * 0.5f;
    DraftBody->HalfExtents = Vector3::Max((Bounds.Maximum - Bounds.Minimum) * 0.5f, Vector3(0.005f));
    Command->BodyId = DraftBody->GetPersistentId();
    const auto Result = SceneSerializer::SerializeComponentToJson(*DraftBody, Command->CreatedBody);
    MemorySubsystem::Get()->DestroyObject(Draft);
    if (!Result.bOk)
    {
        Error = Result.Error;
        return nullptr;
    }
    return Command;
}
