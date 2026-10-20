#pragma once

#include "Physics/PhysicsTypes.h"

#include <memory>

class IPhysicsWorld
{
public:
    virtual ~IPhysicsWorld() = default;
    virtual bool Initialize(const Vector3& Gravity, std::string& Error) = 0;
    virtual PhysicsBodyId CreateBody(const PhysicsBodyDefinition& Definition, std::string& Error) = 0;
    virtual void DestroyBody(PhysicsBodyId Body) = 0;
    virtual bool GetBodyTransform(PhysicsBodyId Body, Transform& WorldTransform) const = 0;
    virtual bool SetBodyTransform(PhysicsBodyId Body, const Transform& WorldTransform, bool bTeleport) = 0;
    virtual bool SetKinematic(PhysicsBodyId Body, bool bKinematic) = 0;
    virtual bool SetLinearVelocity(PhysicsBodyId Body, const Vector3& Velocity) = 0;
    virtual Vector3 GetLinearVelocity(PhysicsBodyId Body) const = 0;
    virtual bool AddForce(PhysicsBodyId Body, const Vector3& Force, bool bImpulse) = 0;
    virtual bool SetAngularVelocity(PhysicsBodyId Body, const Vector3& Velocity) = 0;
    virtual Vector3 GetAngularVelocity(PhysicsBodyId Body) const = 0;
    virtual bool AddTorque(PhysicsBodyId Body, const Vector3& Torque, bool bImpulse) = 0;
    virtual PhysicsCharacterResult MoveCharacter(PhysicsBodyId Body, const Vector3& Displacement, float DeltaTime) = 0;
    virtual void Simulate(float DeltaTime) = 0;
    virtual void SetGravity(const Vector3& Gravity) = 0;
    virtual bool Raycast(const Vector3& Origin, const Vector3& Direction, float Distance,
        const PhysicsQueryFilter& Filter, PhysicsHit& Hit) const = 0;
    virtual bool Sweep(const PhysicsBodyDefinition& Shape, const Vector3& Direction, float Distance,
        const PhysicsQueryFilter& Filter, PhysicsHit& Hit) const = 0;
    virtual std::vector<PhysicsBodyId> Overlap(const PhysicsBodyDefinition& Shape,
        const PhysicsQueryFilter& Filter) const = 0;
    virtual std::vector<PhysicsContact> ConsumeContacts() = 0;
    virtual std::vector<PhysicsDebugLine> GetDebugLines() const = 0;
};

std::unique_ptr<IPhysicsWorld> CreatePhysicsWorld();
