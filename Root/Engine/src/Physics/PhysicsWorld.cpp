#include "Physics/PhysicsWorld.h"
#include "World/Scene.h"
#include "World/GameObject.h"
#include "World/Components/PhysicsBodyComponent.h"
#include "Core/Object/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

PhysicsWorld::PhysicsWorld(Scene& InWorld) : World(InWorld) {}
PhysicsWorld::~PhysicsWorld() = default;

static bool ValidatePhysicsBodyComposition(GameObject& Owner, std::string& Error)
{
    size_t BodyCount = 0;
    bool bHasMovableBody = false;
    for (PhysicsBodyComponent* Body : Owner.GetComponents<PhysicsBodyComponent>())
    {
        if (Body->IsEnabled() && !Body->IsPendingRemoval())
        {
            ++BodyCount;
            bHasMovableBody = bHasMovableBody || Body->MotionType != PhysicsMotionType::Static;
        }
    }
    if (bHasMovableBody && BodyCount > 1)
    {
        Error = "A movable GameObject requires exactly one physics body; use child objects for additional colliders";
        return false;
    }
    return true;
}

bool PhysicsWorld::Prepare(AssetManager* InAssets)
{
    AssertGameThread();
    Assets = InAssets;
    bPreparationFailed = false;
    bool bReady = true;
    for (GameObject* Owner : World.GetAllObjects())
    {
        if (!Owner->IsActiveInHierarchy())
        {
            continue;
        }
        if (!ValidatePhysicsBodyComposition(*Owner, Diagnostic))
        {
            bPreparationFailed = true;
            bReady = false;
            continue;
        }
        for (PhysicsBodyComponent* ComponentInstance : Owner->GetComponents<PhysicsBodyComponent>())
        {
            if (!ComponentInstance->IsEnabled() || ComponentInstance->IsPendingRemoval())
            {
                continue;
            }
            if (!ComponentInstance->PrepareCollisionMesh(Assets))
            {
                Diagnostic = ComponentInstance->GetDiagnostic();
                bPreparationFailed = bPreparationFailed || !ComponentInstance->IsCollisionMeshPending();
                bReady = false;
                continue;
            }
            PhysicsBodyDefinition Definition;
            std::string Error;
            if (!ComponentInstance->BuildDefinition(Definition, Error))
            {
                Diagnostic = Error;
                bPreparationFailed = true;
                bReady = false;
            }
        }
    }
    if (bReady)
    {
        Diagnostic.clear();
    }
    return bReady;
}

bool PhysicsWorld::Initialize()
{
    AssertGameThread();
    if (!Backend)
    {
        Backend = CreatePhysicsWorld();
        if (!Backend->Initialize(Gravity, Diagnostic))
        {
            Backend.reset();
            PrintString("Physics initialization failed: " + Diagnostic);
            return false;
        }
    }
    return Synchronize();
}

void PhysicsWorld::Shutdown()
{
    AssertGameThread();
    Bodies.clear();
    BodyOwners.clear();
    Contacts.clear();
    Backend.reset();
    AccumulatedTime = 0.0;
}

bool PhysicsWorld::Synchronize()
{
    AssertGameThread();
    if (!Backend)
    {
        return false;
    }
    bool bReady = true;
    std::unordered_set<uint64_t> Present;
    for (GameObject* Owner : World.GetAllObjects())
    {
        if (!Owner->IsActiveInHierarchy())
        {
            continue;
        }
        if (!ValidatePhysicsBodyComposition(*Owner, Diagnostic))
        {
            bReady = false;
            continue;
        }
        for (PhysicsBodyComponent* ComponentInstance : Owner->GetComponents<PhysicsBodyComponent>())
        {
            if (!ComponentInstance->IsEnabled() || ComponentInstance->IsPendingRemoval())
            {
                continue;
            }
            if (!ComponentInstance->PrepareCollisionMesh(Assets))
            {
                bReady = false;
                continue;
            }
            const uint64_t Configuration = ComponentInstance->GetConfigurationFingerprint();
            auto Found = Bodies.find(ComponentInstance->GetID());
            if (Found != Bodies.end() && (Found->second.Component != ComponentInstance->GetObjectHandle()
                || Found->second.Configuration != Configuration))
            {
                Backend->DestroyBody(Found->second.Body);
                BodyOwners.erase(Found->second.Body);
                Bodies.erase(Found);
                Found = Bodies.end();
            }
            if (Found == Bodies.end())
            {
                PhysicsBodyDefinition Definition;
                if (!ComponentInstance->BuildDefinition(Definition, Diagnostic))
                {
                    bReady = false;
                    continue;
                }
                RuntimeBody Body;
                Body.Component = ComponentInstance->GetObjectHandle();
                Body.Owner = Owner->GetObjectHandle();
                Body.Configuration = Configuration;
                Body.MotionType = Definition.MotionType;
                Body.LastPublishedTransform = Owner->GetWorldTransform();
                Body.Body = Backend->CreateBody(Definition, Diagnostic);
                if (Body.Body == 0)
                {
                    PrintString("Physics body creation failed: " + Diagnostic);
                    bReady = false;
                    continue;
                }
                Found = Bodies.emplace(ComponentInstance->GetID(), Body).first;
                BodyOwners.emplace(Body.Body, Body.Owner);
            }
            Present.insert(ComponentInstance->GetID());
            RuntimeBody& Body = Found->second;
            const bool bStoryControlled = Owner->IsStoryControllingTransform();
            if (ComponentInstance->MotionType == PhysicsMotionType::Dynamic
                && Body.bStoryControlled != bStoryControlled)
            {
                Backend->SetKinematic(Body.Body, bStoryControlled);
            }
            Body.bStoryControlled = bStoryControlled;
            const Transform CurrentTransform = Owner->GetWorldTransform();
            if (CurrentTransform.GetMatrix() != Body.LastPublishedTransform.GetMatrix())
            {
                const bool bTeleport = Body.MotionType != PhysicsMotionType::Kinematic || Body.bStoryControlled;
                Backend->SetBodyTransform(Body.Body, CurrentTransform, bTeleport);
                Body.LastPublishedTransform = CurrentTransform;
                Body.VerticalVelocity = 0.f;
            }
        }
    }
    for (auto Iterator = Bodies.begin(); Iterator != Bodies.end();)
    {
        if (Present.count(Iterator->first) == 0)
        {
            Backend->DestroyBody(Iterator->second.Body);
            BodyOwners.erase(Iterator->second.Body);
            Iterator = Bodies.erase(Iterator);
        }
        else
        {
            ++Iterator;
        }
    }
    return bReady;
}

void PhysicsWorld::Step(float DeltaTime)
{
    for (auto& Entry : Bodies)
    {
        RuntimeBody& Body = Entry.second;
        PhysicsBodyComponent* ComponentInstance = MemorySubsystem::Get()->ResolveHandle<PhysicsBodyComponent>(Body.Component);
        if (Body.FrameForce.LengthSquared() > 0.f)
        {
            Backend->AddForce(Body.Body, Body.FrameForce, false);
        }
        if (Body.FrameTorque.LengthSquared() > 0.f)
        {
            Backend->AddTorque(Body.Body, Body.FrameTorque, false);
        }
        if (ComponentInstance == nullptr || Body.bStoryControlled || ComponentInstance->MotionType != PhysicsMotionType::Character)
        {
            continue;
        }
        if (ComponentInstance->bUseGravity)
        {
            Body.VerticalVelocity += Gravity.y * DeltaTime;
        }
        Vector3 Displacement = Body.CharacterVelocity * DeltaTime;
        Displacement.y += Body.VerticalVelocity * DeltaTime;
        Transform PreviousPose;
        Backend->GetBodyTransform(Body.Body, PreviousPose);
        const PhysicsCharacterResult Result = Backend->MoveCharacter(Body.Body, Displacement, DeltaTime);
        Body.ActualVelocity = (Result.Position - PreviousPose.Position) / DeltaTime;
        Body.bGrounded = Result.bGrounded;
        if ((Result.bGrounded && Body.VerticalVelocity < 0.f) || (Result.bHitCeiling && Body.VerticalVelocity > 0.f))
        {
            Body.VerticalVelocity = 0.f;
        }
    }
    Backend->Simulate(DeltaTime);
    for (auto& Entry : Bodies)
    {
        RuntimeBody& Body = Entry.second;
        GameObject* Owner = World.FindByHandle(Body.Owner);
        if (Owner == nullptr || Body.bStoryControlled
            || (Body.MotionType != PhysicsMotionType::Dynamic && Body.MotionType != PhysicsMotionType::Character))
        {
            continue;
        }
        Transform Current;
        if (Backend->GetBodyTransform(Body.Body, Current))
        {
            const Vector3 AuthoredLocalScale = Owner->GetTransform().Scale;
            Current.Scale = Owner->GetWorldTransform().Scale;
            if (Owner->SetWorldTransform(Current))
            {
                Transform LocalTransform = Owner->GetTransform();
                LocalTransform.Scale = AuthoredLocalScale;
                Owner->SetTransform(LocalTransform);
                Body.LastPublishedTransform = Owner->GetWorldTransform();
            }
        }
    }
    for (PhysicsContact Contact : Backend->ConsumeContacts())
    {
        Contact.FirstObject = ResolveBodyObject(Contact.FirstBody);
        Contact.SecondObject = ResolveBodyObject(Contact.SecondBody);
        Contacts.push_back(Contact);
    }
}

void PhysicsWorld::Tick(float DeltaTime)
{
    AssertGameThread();
    Contacts.clear();
    if (!Backend || !std::isfinite(DeltaTime) || DeltaTime <= 0.f)
    {
        return;
    }
    Synchronize();
    constexpr double FixedStep = 1.0 / 60.0;
    AccumulatedTime += std::min(static_cast<double>(DeltaTime), 0.25);
    int StepCount = 0;
    while (AccumulatedTime + 0.000000001 >= FixedStep && StepCount < 8)
    {
        Step(static_cast<float>(FixedStep));
        AccumulatedTime -= FixedStep;
        ++StepCount;
    }
    if (StepCount == 8)
    {
        AccumulatedTime = std::fmod(std::max(0.0, AccumulatedTime), FixedStep);
    }
    for (auto& Entry : Bodies)
    {
        Entry.second.FrameForce = Vector3::Zero;
        Entry.second.FrameTorque = Vector3::Zero;
    }
}

void PhysicsWorld::StepOnce()
{
    AssertGameThread();
    Contacts.clear();
    if (Backend)
    {
        Synchronize();
        Step(1.f / 60.f);
        for (auto& Entry : Bodies)
        {
            Entry.second.FrameForce = Vector3::Zero;
            Entry.second.FrameTorque = Vector3::Zero;
        }
    }
}

PhysicsWorld::RuntimeBody* PhysicsWorld::FindBody(const PhysicsBodyComponent& ComponentInstance)
{
    auto Found = Bodies.find(ComponentInstance.GetID());
    if (Found == Bodies.end() || Found->second.Component != ComponentInstance.GetObjectHandle())
    {
        return nullptr;
    }
    return &Found->second;
}

const PhysicsWorld::RuntimeBody* PhysicsWorld::FindBody(const PhysicsBodyComponent& ComponentInstance) const
{
    auto Found = Bodies.find(ComponentInstance.GetID());
    if (Found == Bodies.end() || Found->second.Component != ComponentInstance.GetObjectHandle())
    {
        return nullptr;
    }
    return &Found->second;
}

PhysicsBodyId PhysicsWorld::GetBodyId(const PhysicsBodyComponent& ComponentInstance) const
{
    const RuntimeBody* Body = FindBody(ComponentInstance);
    if (Body != nullptr)
    {
        return Body->Body;
    }
    return 0;
}

ObjectHandle PhysicsWorld::ResolveBodyObject(PhysicsBodyId Identity) const
{
    const auto Found = BodyOwners.find(Identity);
    if (Found != BodyOwners.end() && World.FindByHandle(Found->second) != nullptr)
    {
        return Found->second;
    }
    return {};
}

bool PhysicsWorld::Raycast(const Vector3& Origin, const Vector3& Direction, float Distance,
    PhysicsHit& Hit, const PhysicsQueryFilter& Filter) const
{
    if (!Backend || !Backend->Raycast(Origin, Direction, Distance, Filter, Hit))
    {
        return false;
    }
    Hit.Object = ResolveBodyObject(Hit.Body);
    return true;
}

bool PhysicsWorld::Sweep(const PhysicsBodyDefinition& Shape, const Vector3& Direction, float Distance,
    PhysicsHit& Hit, const PhysicsQueryFilter& Filter) const
{
    if (!Backend || !Backend->Sweep(Shape, Direction, Distance, Filter, Hit))
    {
        return false;
    }
    Hit.Object = ResolveBodyObject(Hit.Body);
    return true;
}

std::vector<PhysicsHit> PhysicsWorld::Overlap(const PhysicsBodyDefinition& Shape, const PhysicsQueryFilter& Filter) const
{
    std::vector<PhysicsHit> Result;
    if (Backend)
    {
        for (PhysicsBodyId Identity : Backend->Overlap(Shape, Filter))
        {
            PhysicsHit Hit;
            Hit.Body = Identity;
            Hit.Object = ResolveBodyObject(Identity);
            Result.push_back(Hit);
        }
    }
    return Result;
}

void PhysicsWorld::SetCharacterVelocity(const PhysicsBodyComponent& ComponentInstance, const Vector3& Velocity)
{
    AssertGameThread();
    if (!std::isfinite(Velocity.x) || !std::isfinite(Velocity.y) || !std::isfinite(Velocity.z))
    {
        return;
    }
    if (RuntimeBody* Body = FindBody(ComponentInstance))
    {
        Body->CharacterVelocity = Velocity;
    }
}

void PhysicsWorld::Jump(const PhysicsBodyComponent& ComponentInstance, float Speed)
{
    if (RuntimeBody* Body = FindBody(ComponentInstance))
    {
        if (Body->bGrounded && std::isfinite(Speed) && Speed > 0.f)
        {
            Body->VerticalVelocity = Speed;
            Body->bGrounded = false;
        }
    }
}

bool PhysicsWorld::IsGrounded(const PhysicsBodyComponent& ComponentInstance) const
{
    const RuntimeBody* Body = FindBody(ComponentInstance);
    return Body != nullptr && Body->bGrounded;
}

Vector3 PhysicsWorld::GetVelocity(const PhysicsBodyComponent& ComponentInstance) const
{
    const RuntimeBody* Body = FindBody(ComponentInstance);
    if (Body == nullptr)
    {
        return Vector3::Zero;
    }
    if (ComponentInstance.MotionType == PhysicsMotionType::Character)
    {
        return Body->ActualVelocity;
    }
    return Backend->GetLinearVelocity(Body->Body);
}

bool PhysicsWorld::SetVelocity(const PhysicsBodyComponent& ComponentInstance, const Vector3& Velocity)
{
    return Backend && Backend->SetLinearVelocity(GetBodyId(ComponentInstance), Velocity);
}

bool PhysicsWorld::AddForce(const PhysicsBodyComponent& ComponentInstance, const Vector3& Force, bool bImpulse)
{
    AssertGameThread();
    RuntimeBody* Body = FindBody(ComponentInstance);
    if (Body == nullptr || ComponentInstance.MotionType != PhysicsMotionType::Dynamic
        || Body->bStoryControlled || !std::isfinite(Force.x) || !std::isfinite(Force.y) || !std::isfinite(Force.z))
    {
        return false;
    }
    if (bImpulse)
    {
        return Backend->AddForce(Body->Body, Force, true);
    }
    Body->FrameForce += Force;
    return true;
}

bool PhysicsWorld::AddTorque(const PhysicsBodyComponent& ComponentInstance, const Vector3& Torque, bool bImpulse)
{
    AssertGameThread();
    RuntimeBody* Body = FindBody(ComponentInstance);
    if (Body == nullptr || ComponentInstance.MotionType != PhysicsMotionType::Dynamic
        || Body->bStoryControlled || !std::isfinite(Torque.x) || !std::isfinite(Torque.y) || !std::isfinite(Torque.z))
    {
        return false;
    }
    if (bImpulse)
    {
        return Backend->AddTorque(Body->Body, Torque, true);
    }
    Body->FrameTorque += Torque;
    return true;
}

bool PhysicsWorld::SetAngularVelocity(const PhysicsBodyComponent& ComponentInstance, const Vector3& Velocity)
{
    AssertGameThread();
    return Backend && Backend->SetAngularVelocity(GetBodyId(ComponentInstance), Velocity);
}

Vector3 PhysicsWorld::GetAngularVelocity(const PhysicsBodyComponent& ComponentInstance) const
{
    AssertGameThread();
    if (Backend)
    {
        return Backend->GetAngularVelocity(GetBodyId(ComponentInstance));
    }
    return Vector3::Zero;
}

bool PhysicsWorld::SetGravity(const Vector3& Value)
{
    AssertGameThread();
    if (!std::isfinite(Value.x) || !std::isfinite(Value.y) || !std::isfinite(Value.z))
    {
        return false;
    }
    Gravity = Value;
    if (Backend)
    {
        Backend->SetGravity(Gravity);
    }
    return true;
}

std::vector<PhysicsDebugLine> PhysicsWorld::GetDebugLines() const
{
    if (Backend)
    {
        return Backend->GetDebugLines();
    }
    std::vector<PhysicsDebugLine> Lines;
    for (GameObject* Owner : World.GetAllObjects())
    {
        for (PhysicsBodyComponent* ComponentInstance : Owner->GetComponents<PhysicsBodyComponent>())
        {
            if (!Owner->IsActiveInHierarchy() || !ComponentInstance->IsEnabled())
            {
                continue;
            }
            if (ComponentInstance->bUseMeshDefaults && !ComponentInstance->PrepareCollisionMesh(World.GetAssetManager()))
            {
                continue;
            }
            PhysicsBodyDefinition Definition;
            std::string Error;
            if (!ComponentInstance->BuildDefinition(Definition, Error))
            {
                continue;
            }
            if (Definition.MotionType == PhysicsMotionType::Character)
            {
                Definition.Center = Vector3(0.f, Definition.Radius + Definition.CapsuleHalfHeight, 0.f);
            }
            const PhysicsTriangleMesh Mesh = MakePhysicsTriangleMesh(Definition);
            for (size_t Index = 0; Index + 2 < Mesh.Indices.size(); Index += 3)
            {
                for (size_t Edge = 0; Edge < 3; ++Edge)
                {
                    Lines.push_back({Mesh.Vertices[Mesh.Indices[Index + Edge]],
                        Mesh.Vertices[Mesh.Indices[Index + (Edge + 1) % 3]]});
                }
            }
        }
    }
    return Lines;
}
