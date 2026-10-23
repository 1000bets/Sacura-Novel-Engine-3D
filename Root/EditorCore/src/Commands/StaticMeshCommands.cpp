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
