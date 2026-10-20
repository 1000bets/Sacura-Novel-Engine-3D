#pragma once

#include "Core/Transform.h"
#include "Core/Object/ObjectHandle.h"
#include "Rendering/AxisAlignedBounds.h"

#include <cstdint>
#include <string>
#include <vector>

using PhysicsBodyId = uint64_t;

enum class PhysicsMotionType
{
    Static,
    Dynamic,
    Kinematic,
    Character
};

enum class PhysicsShapeType
{
    Box,
    Sphere,
    Capsule,
    TriangleMesh
};

struct PhysicsTriangleMesh
{
    std::vector<Vector3> Vertices;
    std::vector<uint32_t> Indices;
};

struct PhysicsBodyDefinition
{
    PhysicsMotionType MotionType = PhysicsMotionType::Static;
    PhysicsShapeType ShapeType = PhysicsShapeType::Box;
    Transform WorldTransform;
    Vector3 Center = Vector3::Zero;
    Vector3 HalfExtents = Vector3(0.5f, 0.5f, 0.5f);
    float Radius = 0.3f;
    float CapsuleHalfHeight = 0.6f;
    float Mass = 1.f;
    float StaticFriction = 0.6f;
    float DynamicFriction = 0.5f;
    float Restitution = 0.f;
    float LinearDamping = 0.05f;
    float AngularDamping = 0.05f;
    float StepHeight = 0.3f;
    float MaximumSlopeDegrees = 45.f;
    uint32_t CollisionLayer = 1;
    uint32_t CollisionMask = UINT32_MAX;
    bool bUseGravity = true;
    bool bTrigger = false;
    bool bQueryEnabled = true;
    bool bSimulationEnabled = true;
    bool bContinuousCollisionDetection = false;
    PhysicsTriangleMesh TriangleMesh;
};

struct PhysicsQueryFilter
{
    uint32_t LayerMask = UINT32_MAX;
    PhysicsBodyId IgnoredBody = 0;
    bool bIncludeTriggers = false;
};

struct PhysicsHit
{
    PhysicsBodyId Body = 0;
    ObjectHandle Object;
    Vector3 Position = Vector3::Zero;
    Vector3 Normal = Vector3::Zero;
    float Distance = 0.f;
    bool bInitialOverlap = false;
};

enum class PhysicsContactKind
{
    Begin,
    Persist,
    End
};

struct PhysicsContact
{
    PhysicsBodyId FirstBody = 0;
    PhysicsBodyId SecondBody = 0;
    ObjectHandle FirstObject;
    ObjectHandle SecondObject;
    PhysicsContactKind Kind = PhysicsContactKind::Begin;
    Vector3 Position = Vector3::Zero;
    Vector3 Normal = Vector3::Zero;
    Vector3 Impulse = Vector3::Zero;
    bool bTrigger = false;
};

struct PhysicsCharacterResult
{
    Vector3 Position = Vector3::Zero;
    bool bGrounded = false;
    bool bHitCeiling = false;
    bool bHitWall = false;
};

struct PhysicsDebugLine
{
    Vector3 Start = Vector3::Zero;
    Vector3 End = Vector3::Zero;
    Color LineColor = Color(0.3f, 0.8f, 1.f, 1.f);
};

bool ValidatePhysicsBody(const PhysicsBodyDefinition& Definition, std::string& Error);
PhysicsTriangleMesh MakePhysicsTriangleMesh(const PhysicsBodyDefinition& Definition);
