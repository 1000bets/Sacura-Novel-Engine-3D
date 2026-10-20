#pragma once

#include "Physics/IPhysicsWorld.h"

#include <unordered_map>
#include <memory>

class Scene;
class AssetManager;
class PhysicsBodyComponent;

class PhysicsWorld
{
public:
    explicit PhysicsWorld(Scene& World);
    ~PhysicsWorld();
    bool Prepare(AssetManager* Assets);
    bool Initialize();
    void Shutdown();
    void Tick(float DeltaTime);
    void StepOnce();
    bool Synchronize();
    PhysicsBodyId GetBodyId(const PhysicsBodyComponent& ComponentInstance) const;
    IPhysicsWorld* GetBackend() const { return Backend.get(); }
    bool Raycast(const Vector3& Origin, const Vector3& Direction, float Distance,
        PhysicsHit& Hit, const PhysicsQueryFilter& Filter = {}) const;
    bool Sweep(const PhysicsBodyDefinition& Shape, const Vector3& Direction, float Distance,
        PhysicsHit& Hit, const PhysicsQueryFilter& Filter = {}) const;
    std::vector<PhysicsHit> Overlap(const PhysicsBodyDefinition& Shape, const PhysicsQueryFilter& Filter = {}) const;
    const std::vector<PhysicsContact>& GetContacts() const { return Contacts; }
    void SetCharacterVelocity(const PhysicsBodyComponent& ComponentInstance, const Vector3& Velocity);
    void Jump(const PhysicsBodyComponent& ComponentInstance, float Speed);
    bool IsGrounded(const PhysicsBodyComponent& ComponentInstance) const;
    Vector3 GetVelocity(const PhysicsBodyComponent& ComponentInstance) const;
    bool SetVelocity(const PhysicsBodyComponent& ComponentInstance, const Vector3& Velocity);
    bool AddForce(const PhysicsBodyComponent& ComponentInstance, const Vector3& Force, bool bImpulse);
    bool AddTorque(const PhysicsBodyComponent& ComponentInstance, const Vector3& Torque, bool bImpulse);
    bool SetAngularVelocity(const PhysicsBodyComponent& ComponentInstance, const Vector3& Velocity);
    Vector3 GetAngularVelocity(const PhysicsBodyComponent& ComponentInstance) const;
    bool SetGravity(const Vector3& Value);
    const Vector3& GetGravity() const { return Gravity; }
    std::vector<PhysicsDebugLine> GetDebugLines() const;
    const std::string& GetDiagnostic() const { return Diagnostic; }
    bool HasPreparationFailed() const { return bPreparationFailed; }

private:
    struct RuntimeBody
    {
        ObjectHandle Component;
        ObjectHandle Owner;
        PhysicsBodyId Body = 0;
        PhysicsMotionType MotionType = PhysicsMotionType::Static;
        uint64_t Configuration = 0;
        Transform LastPublishedTransform;
        Vector3 CharacterVelocity = Vector3::Zero;
        Vector3 ActualVelocity = Vector3::Zero;
        Vector3 FrameForce = Vector3::Zero;
        Vector3 FrameTorque = Vector3::Zero;
        float VerticalVelocity = 0.f;
        bool bGrounded = false;
        bool bStoryControlled = false;
    };

    ObjectHandle ResolveBodyObject(PhysicsBodyId Body) const;
    RuntimeBody* FindBody(const PhysicsBodyComponent& ComponentInstance);
    const RuntimeBody* FindBody(const PhysicsBodyComponent& ComponentInstance) const;
    void Step(float DeltaTime);

    Scene& World;
    AssetManager* Assets = nullptr;
    std::unique_ptr<IPhysicsWorld> Backend;
    std::unordered_map<uint64_t, RuntimeBody> Bodies;
    std::unordered_map<PhysicsBodyId, ObjectHandle> BodyOwners;
    std::vector<PhysicsContact> Contacts;
    Vector3 Gravity = Vector3(0.f, -9.81f, 0.f);
    double AccumulatedTime = 0.0;
    std::string Diagnostic;
    bool bPreparationFailed = false;
};
