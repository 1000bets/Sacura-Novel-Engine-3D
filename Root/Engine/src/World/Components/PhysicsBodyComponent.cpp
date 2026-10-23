#include "World/Components/PhysicsBodyComponent.h"
#include "World/Components/MeshRendererComponent.h"
#include "World/GameObject.h"
#include "World/Scene.h"
#include "Physics/PhysicsWorld.h"
#include "Assets/AssetManager.h"
#include "Assets/ContentHash.h"
#include "Reflection/Json/ReflectionJson.h"

#include <algorithm>
#include <cmath>

bool PhysicsBodyComponent::PrepareCollisionMesh(AssetManager* Assets)
{
    Diagnostic.clear();
    bCollisionMeshPending = false;
    if (!bUseMeshDefaults && ShapeType != static_cast<int64_t>(PhysicsShapeType::TriangleMesh))
    {
        CollisionMesh.reset();
        MeshFingerprint = 0;
        return true;
    }
    AssetKey Key;
    AssetId::TryParse(CollisionMeshAssetId, Key.Asset);
    SubAssetId Subasset;
    if (SubAssetId::TryParse(CollisionMeshSubAssetId, Subasset))
    {
        Key.SubAsset = Subasset;
    }
    if (!Key.IsValid() && CollisionMeshAssetId.empty())
    {
        if (const MeshRendererComponent* Renderer = GetGameObject()->GetComponent<MeshRendererComponent>())
        {
            Key = Renderer->GetMeshAssetKey();
        }
    }
    if (Assets == nullptr || !Key.IsValid())
    {
        CollisionMesh.reset();
        Diagnostic = "Triangle mesh collision requires a valid StaticMesh asset";
        return false;
    }
    AssetHandle<StaticMeshResource> Loaded;
    if (!Assets->TryGetLoaded(Key, Loaded))
    {
        CollisionMesh.reset();
        if (Assets->GetLoadState(Key) == AssetLoadState::Failed)
        {
            Diagnostic = Assets->GetLastDiagnostic(Key).Message;
        }
        else
        {
            Assets->LoadAsync<StaticMeshResource>(Key);
            bCollisionMeshPending = true;
            Diagnostic = "Collision mesh is loading";
        }
        return false;
    }
    if (bUseMeshDefaults)
    {
        const StaticMeshDefaults* DefaultsValue = &Loaded->Defaults;
        if (CollisionPreview)
        {
            DefaultsValue = &*CollisionPreview;
        }
        const auto& Defaults = *DefaultsValue;
        if (!Defaults.CollisionElements.empty())
        {
            const auto& Element = Defaults.CollisionElements.front();
            ShapeType = Element.ShapeType;
            Center = Element.Center;
            HalfExtents = Element.HalfExtents;
            Radius = Element.Radius;
            CapsuleHalfHeight = Element.CapsuleHalfHeight;
        }
        bSimulationEnabled = Defaults.bCollisionEnabled;
        bQueryEnabled = Defaults.bCollisionEnabled;
    }
    if (CollisionMesh != Loaded.GetSharedResource())
    {
        CollisionMesh = Loaded.GetSharedResource();
        std::vector<float> Positions;
        Positions.reserve(CollisionMesh->Vertices.size() * 3);
        for (const StaticMeshVertex& Vertex : CollisionMesh->Vertices)
        {
            Positions.insert(Positions.end(), {Vertex.Position.x, Vertex.Position.y, Vertex.Position.z});
        }
        MeshFingerprint = ContentHash::FromBytes(Positions.data(), Positions.size() * sizeof(float)).Value
            ^ ContentHash::FromBytes(CollisionMesh->Indices.data(), CollisionMesh->Indices.size() * sizeof(uint32_t)).Value;
        const auto Settings = CollisionMesh->Defaults.ToJson().at("collision").dump();
        MeshFingerprint ^= ContentHash::FromBytes(Settings.data(), Settings.size()).Value;
    }
    return true;
}

bool PhysicsBodyComponent::BuildDefinition(PhysicsBodyDefinition& Definition, std::string& Error) const
{
    Definition = PhysicsBodyDefinition{};
    if (MotionType < 0 || MotionType > 3 || ShapeType < 0 || ShapeType > 3
        || CollisionLayer < 1 || CollisionLayer > UINT32_MAX || CollisionMask < 0 || CollisionMask > UINT32_MAX)
    {
        Error = "Physics motion, shape or collision filter is out of range";
        return false;
    }
    Definition.MotionType = static_cast<PhysicsMotionType>(MotionType);
    Definition.ShapeType = static_cast<PhysicsShapeType>(ShapeType);
    Definition.WorldTransform = GetGameObject()->GetWorldTransform();
    const Vector3 Scale = Definition.WorldTransform.Scale;
    const Vector3 AbsoluteScale(std::abs(Scale.x), std::abs(Scale.y), std::abs(Scale.z));
    if (AbsoluteScale.x < 0.000001f || AbsoluteScale.y < 0.000001f || AbsoluteScale.z < 0.000001f)
    {
        Error = "Physics body requires a nonzero world scale";
        return false;
    }
    Definition.WorldTransform.Scale = Vector3::One;
    Definition.Center = Center * Scale;
    Definition.HalfExtents = HalfExtents * AbsoluteScale;
    Definition.Radius = Radius * std::max(AbsoluteScale.x, AbsoluteScale.z);
    if (Definition.ShapeType == PhysicsShapeType::Sphere)
    {
        Definition.Radius = Radius * std::max({AbsoluteScale.x, AbsoluteScale.y, AbsoluteScale.z});
    }
    Definition.CapsuleHalfHeight = CapsuleHalfHeight * AbsoluteScale.y;
    Definition.Mass = Mass;
    Definition.StaticFriction = StaticFriction;
    Definition.DynamicFriction = DynamicFriction;
    Definition.Restitution = Restitution;
    Definition.LinearDamping = LinearDamping;
    Definition.AngularDamping = AngularDamping;
    Definition.StepHeight = StepHeight;
    Definition.MaximumSlopeDegrees = MaximumSlopeDegrees;
    Definition.CollisionLayer = static_cast<uint32_t>(CollisionLayer);
    Definition.CollisionMask = static_cast<uint32_t>(CollisionMask);
    Definition.bUseGravity = bUseGravity;
    Definition.bTrigger = bTrigger;
    Definition.bQueryEnabled = bQueryEnabled;
    Definition.bSimulationEnabled = bSimulationEnabled;
    Definition.bContinuousCollisionDetection = bContinuousCollisionDetection;
    if (bUseMeshDefaults)
    {
        if (!CollisionMesh)
        {
            Error = "Mesh collision defaults have not finished loading";
            return false;
        }
        const StaticMeshDefaults* Defaults = &CollisionMesh->Defaults;
        if (CollisionPreview)
        {
            Defaults = &*CollisionPreview;
        }
        for (const auto& Element : Defaults->CollisionElements)
        {
            PhysicsShapeDefinition Shape;
            Shape.ShapeType = static_cast<PhysicsShapeType>(Element.ShapeType);
            Shape.Center = Element.Center * Scale;
            Shape.HalfExtents = Element.HalfExtents * AbsoluteScale;
            Shape.Radius = Element.Radius * std::max(AbsoluteScale.x, AbsoluteScale.z);
            if (Shape.ShapeType == PhysicsShapeType::Sphere)
            {
                Shape.Radius = Element.Radius * std::max({AbsoluteScale.x, AbsoluteScale.y, AbsoluteScale.z});
            }
            Shape.CapsuleHalfHeight = Element.CapsuleHalfHeight * AbsoluteScale.y;
            Shape.ShapeRotation = Quaternion::CreateFromYawPitchRoll(Element.RotationDegrees.y * 0.01745329252f,
                Element.RotationDegrees.x * 0.01745329252f, Element.RotationDegrees.z * 0.01745329252f);
            if (Shape.ShapeType == PhysicsShapeType::TriangleMesh)
            {
                Shape.TriangleMesh.Vertices.reserve(CollisionMesh->Vertices.size());
                for (const auto& Vertex : CollisionMesh->Vertices)
                {
                    Shape.TriangleMesh.Vertices.push_back(Vector3::Transform(Vertex.Position, Shape.ShapeRotation) * Scale);
                }
                Shape.ShapeRotation = Quaternion::Identity;
                Shape.TriangleMesh.Indices = CollisionMesh->Indices;
                if (Scale.x * Scale.y * Scale.z < 0.f)
                {
                    for (size_t Index = 0; Index + 2 < Shape.TriangleMesh.Indices.size(); Index += 3)
                    {
                        std::swap(Shape.TriangleMesh.Indices[Index + 1], Shape.TriangleMesh.Indices[Index + 2]);
                    }
                }
            }
            else if (Shape.ShapeType != PhysicsShapeType::Sphere && Element.RotationDegrees != Vector3::Zero
                && (Scale.x <= 0.f || Scale.y <= 0.f || Scale.z <= 0.f
                    || std::abs(Scale.x - Scale.y) > 0.00001f || std::abs(Scale.y - Scale.z) > 0.00001f))
            {
                Error = "Rotated collision primitives require a positive uniform object scale";
                return false;
            }
            Definition.Shapes.push_back(std::move(Shape));
        }
        if (!Definition.Shapes.empty())
        {
            static_cast<PhysicsShapeDefinition&>(Definition) = Definition.Shapes.front();
        }
        else
        {
            static_cast<PhysicsShapeDefinition&>(Definition) = PhysicsShapeDefinition{};
        }
    }
    else
    {
        if (Definition.ShapeType == PhysicsShapeType::TriangleMesh)
        {
            if (!CollisionMesh)
            {
                Error = "Collision mesh has not finished loading";
                return false;
            }
            Definition.TriangleMesh.Vertices.reserve(CollisionMesh->Vertices.size());
            for (const StaticMeshVertex& Vertex : CollisionMesh->Vertices)
            {
                Definition.TriangleMesh.Vertices.push_back(Vertex.Position * Scale);
            }
            Definition.TriangleMesh.Indices = CollisionMesh->Indices;
            if (Scale.x * Scale.y * Scale.z < 0.f)
            {
                for (size_t Index = 0; Index + 2 < Definition.TriangleMesh.Indices.size(); Index += 3)
                {
                    std::swap(Definition.TriangleMesh.Indices[Index + 1], Definition.TriangleMesh.Indices[Index + 2]);
                }
            }
        }
    }
    return ValidatePhysicsBody(Definition, Error);
}

uint64_t PhysicsBodyComponent::GetConfigurationFingerprint() const
{
    const Vector3 Scale = GetGameObject()->GetWorldTransform().Scale;
    const nlohmann::json Configuration = {bUseMeshDefaults, MotionType, ShapeType, HalfExtents.x, HalfExtents.y, HalfExtents.z,
        Center.x, Center.y, Center.z, Radius, CapsuleHalfHeight, Mass, StaticFriction, DynamicFriction,
        Restitution, LinearDamping, AngularDamping, StepHeight, MaximumSlopeDegrees, CollisionLayer,
        CollisionMask, bUseGravity, bTrigger, bQueryEnabled, bSimulationEnabled, bContinuousCollisionDetection,
        CollisionMeshAssetId, CollisionMeshSubAssetId, Scale.x, Scale.y, Scale.z, MeshFingerprint};
    const std::string Serialized = Configuration.dump();
    return ContentHash::FromBytes(Serialized.data(), Serialized.size()).Value;
}

PhysicsBodyId PhysicsBodyComponent::GetBodyId() const
{
    return GetGameObject()->GetScene()->GetPhysics().GetBodyId(*this);
}

bool PhysicsBodyComponent::AddForce(const Vector3& Force)
{
    return GetGameObject()->GetScene()->GetPhysics().AddForce(*this, Force, false);
}

bool PhysicsBodyComponent::AddImpulse(const Vector3& Impulse)
{
    return GetGameObject()->GetScene()->GetPhysics().AddForce(*this, Impulse, true);
}

bool PhysicsBodyComponent::AddTorque(const Vector3& Torque)
{
    return GetGameObject()->GetScene()->GetPhysics().AddTorque(*this, Torque, false);
}

bool PhysicsBodyComponent::AddAngularImpulse(const Vector3& Impulse)
{
    return GetGameObject()->GetScene()->GetPhysics().AddTorque(*this, Impulse, true);
}

bool PhysicsBodyComponent::SetAngularVelocity(const Vector3& Velocity)
{
    return GetGameObject()->GetScene()->GetPhysics().SetAngularVelocity(*this, Velocity);
}

Vector3 PhysicsBodyComponent::GetAngularVelocity() const
{
    return GetGameObject()->GetScene()->GetPhysics().GetAngularVelocity(*this);
}

bool PhysicsBodyComponent::SetVelocity(const Vector3& Velocity)
{
    return GetGameObject()->GetScene()->GetPhysics().SetVelocity(*this, Velocity);
}

Vector3 PhysicsBodyComponent::GetVelocity() const
{
    return GetGameObject()->GetScene()->GetPhysics().GetVelocity(*this);
}

void PhysicsBodyComponent::SetCharacterVelocity(const Vector3& Velocity)
{
    GetGameObject()->GetScene()->GetPhysics().SetCharacterVelocity(*this, Velocity);
}

void PhysicsBodyComponent::Jump(float Speed)
{
    GetGameObject()->GetScene()->GetPhysics().Jump(*this, Speed);
}

bool PhysicsBodyComponent::IsGrounded() const
{
    return GetGameObject()->GetScene()->GetPhysics().IsGrounded(*this);
}
