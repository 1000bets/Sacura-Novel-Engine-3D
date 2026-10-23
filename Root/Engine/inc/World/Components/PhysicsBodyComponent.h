#pragma once

#include "World/Components/Component.h"
#include "Reflection/ReflectionMacros.h"
#include "Physics/PhysicsTypes.h"
#include "Assets/Resources/StaticMeshResource.h"
#include <memory>
#include <optional>

class AssetManager;

class PhysicsBodyComponent : public Component
{
    ENGINE_CLASS(PhysicsBodyComponent, Component, "engine.PhysicsBodyComponent", "Коллизия")

public:
    bool bUseMeshDefaults = false;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, bUseMeshDefaults, "use_mesh_defaults", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Use Static Mesh Collision Defaults"))
    int64_t MotionType = 0;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, MotionType, "motion_type", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Motion Type (0 Static, 1 Dynamic, 2 Kinematic, 3 Character)"), RF_UI_RANGE(0, 3))
    int64_t ShapeType = 0;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, ShapeType, "shape_type", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Shape (0 Box, 1 Sphere, 2 Capsule, 3 Triangle Mesh)"), RF_UI_RANGE(0, 3))
    Vector3 HalfExtents = Vector3(0.5f, 0.5f, 0.5f);
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, HalfExtents, "half_extents", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Box Half Extents"))
    Vector3 Center = Vector3::Zero;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, Center, "center", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Shape Center"))
    float Radius = 0.3f;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, Radius, "radius", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Radius"), RF_UI_RANGE(0.001, 10000))
    float CapsuleHalfHeight = 0.6f;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, CapsuleHalfHeight, "capsule_half_height", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Capsule Cylinder Half Height"), RF_UI_RANGE(0.001, 10000))
    float Mass = 1.f;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, Mass, "mass", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Mass (kg)"), RF_UI_RANGE(0.001, 1000000))
    float StaticFriction = 0.6f;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, StaticFriction, "static_friction", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Static Friction"), RF_UI_RANGE(0, 10))
    float DynamicFriction = 0.5f;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, DynamicFriction, "dynamic_friction", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Dynamic Friction"), RF_UI_RANGE(0, 10))
    float Restitution = 0.f;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, Restitution, "restitution", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Restitution"), RF_UI_RANGE(0, 1))
    float LinearDamping = 0.05f;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, LinearDamping, "linear_damping", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Linear Damping"), RF_UI_RANGE(0, 100))
    float AngularDamping = 0.05f;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, AngularDamping, "angular_damping", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Angular Damping"), RF_UI_RANGE(0, 100))
    float StepHeight = 0.3f;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, StepHeight, "step_height", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Character Step Height"), RF_UI_RANGE(0, 10))
    float MaximumSlopeDegrees = 45.f;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, MaximumSlopeDegrees, "maximum_slope_degrees", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Character Maximum Slope"), RF_UI_RANGE(0, 89))
    int64_t CollisionLayer = 1;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, CollisionLayer, "collision_layer", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Collision Layer Bit"), RF_UI_RANGE(1, 2147483648.0))
    int64_t CollisionMask = 4294967295LL;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, CollisionMask, "collision_mask", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Collision Mask"), RF_UI_RANGE(0, 4294967295.0))
    bool bUseGravity = true;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, bUseGravity, "use_gravity", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Use Gravity"))
    bool bTrigger = false;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, bTrigger, "trigger", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Trigger"))
    bool bQueryEnabled = true;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, bQueryEnabled, "query_enabled", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Scene Queries"))
    bool bContinuousCollisionDetection = false;
    bool bSimulationEnabled = true;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, bSimulationEnabled, "simulation_enabled", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Physical Collision"))
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, bContinuousCollisionDetection, "continuous_collision_detection", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Continuous Collision Detection"))
    bool bAffectsNavigation = true;
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, bAffectsNavigation, "affects_navigation", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Affects Navigation"))
    std::string CollisionMeshAssetId = "";
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, CollisionMeshAssetId, "collision_mesh_asset_id", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Collision Mesh"), RF_ASSET_TYPE("StaticMesh"), RF_COMPANION_PROPERTY("collision_mesh_sub_asset_id"))
    std::string CollisionMeshSubAssetId = "";
    ENGINE_REFLECT_FIELD(PhysicsBodyComponent, CollisionMeshSubAssetId, "collision_mesh_sub_asset_id", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Physics"), RF_DISPLAY_NAME("Collision Mesh Subasset"), RF_EDITOR_HIDDEN)

    void SetCollisionPreview(const StaticMeshDefaults& Defaults) { CollisionPreview = Defaults; bUseMeshDefaults = true; }
    bool PrepareCollisionMesh(AssetManager* Assets);
    bool IsCollisionMeshPending() const { return bCollisionMeshPending; }
    bool BuildDefinition(PhysicsBodyDefinition& Definition, std::string& Error) const;
    uint64_t GetConfigurationFingerprint() const;
    PhysicsBodyId GetBodyId() const;
    bool AddForce(const Vector3& Force);
    bool AddImpulse(const Vector3& Impulse);
    bool AddTorque(const Vector3& Torque);
    bool AddAngularImpulse(const Vector3& Impulse);
    bool SetAngularVelocity(const Vector3& Velocity);
    Vector3 GetAngularVelocity() const;
    bool SetVelocity(const Vector3& Velocity);
    Vector3 GetVelocity() const;
    void SetCharacterVelocity(const Vector3& Velocity);
    void Jump(float Speed);
    bool IsGrounded() const;
    std::string GetDiagnostic() const { return Diagnostic; }
    ENGINE_REFLECT_READONLY(PhysicsBodyComponent, DiagnosticProperty, "physics_diagnostic", std::string,
        GetDiagnostic, RF_EDITOR_READONLY, RF_DISPLAY_NAME("Physics Diagnostic"))

protected:
    PhysicsBodyComponent() = default;

private:
    std::optional<StaticMeshDefaults> CollisionPreview;
    std::shared_ptr<const StaticMeshResource> CollisionMesh;
    uint64_t MeshFingerprint = 0;
    std::string Diagnostic;
    bool bCollisionMeshPending = false;
};
ENGINE_CLASS_END(PhysicsBodyComponent)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, bUseMeshDefaults)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, MotionType)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, ShapeType)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, HalfExtents)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, Center)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, Radius)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, CapsuleHalfHeight)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, Mass)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, StaticFriction)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, DynamicFriction)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, Restitution)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, LinearDamping)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, AngularDamping)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, StepHeight)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, MaximumSlopeDegrees)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, CollisionLayer)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, CollisionMask)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, bUseGravity)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, bTrigger)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, bQueryEnabled)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, bContinuousCollisionDetection)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, bSimulationEnabled)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, bAffectsNavigation)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, CollisionMeshAssetId)
ENGINE_IMPLEMENT_FIELD(PhysicsBodyComponent, CollisionMeshSubAssetId)
ENGINE_IMPLEMENT_READONLY(PhysicsBodyComponent, DiagnosticProperty, std::string, GetDiagnostic)

class CharacterControllerComponent : public PhysicsBodyComponent
{
    ENGINE_CLASS(CharacterControllerComponent, PhysicsBodyComponent, "engine.CharacterControllerComponent", "Контроллер персонажа")
protected:
    CharacterControllerComponent()
    {
        MotionType = 3;
        ShapeType = 2;
        bAffectsNavigation = false;
    }
};
ENGINE_CLASS_END(CharacterControllerComponent)

