#include "Physics/IPhysicsWorld.h"
#include "Core/Threading/ThreadContext.h"

#include <PxPhysicsAPI.h>
#include <characterkinematic/PxControllerManager.h>
#include <characterkinematic/PxCapsuleController.h>
#include <cooking/PxCooking.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <map>
#include <unordered_map>

using namespace physx;

static PxVec3 ToPhysXVector(const Vector3& Value)
{
    return PxVec3(Value.x, Value.y, Value.z);
}

static Vector3 FromPhysXVector(const PxVec3& Value)
{
    return Vector3(Value.x, Value.y, Value.z);
}

static bool IsValidPhysXVector(const Vector3& Value)
{
    return std::isfinite(Value.x) && std::isfinite(Value.y) && std::isfinite(Value.z);
}

static PxTransform ToPhysXTransform(const Transform& Value)
{
    Quaternion Rotation = Value.Rotation;
    Rotation.Normalize();
    return PxTransform(ToPhysXVector(Value.Position), PxQuat(Rotation.x, Rotation.y, Rotation.z, Rotation.w));
}

struct PhysXBodyMetadata
{
    PhysicsBodyId Identity = 0;
    bool bQueryEnabled = true;
};

static PhysicsBodyId GetPhysXBodyId(const PxActor* Actor)
{
    if (Actor == nullptr || Actor->userData == nullptr)
    {
        return 0;
    }
    return static_cast<const PhysXBodyMetadata*>(Actor->userData)->Identity;
}

static PxFilterFlags FilterPhysXSimulation(PxFilterObjectAttributes FirstAttributes, PxFilterData FirstData,
    PxFilterObjectAttributes SecondAttributes, PxFilterData SecondData, PxPairFlags& PairFlags,
    const void*, PxU32)
{
    if ((FirstData.word0 & SecondData.word1) == 0 || (SecondData.word0 & FirstData.word1) == 0)
    {
        return PxFilterFlag::eSUPPRESS;
    }
    if (PxFilterObjectIsTrigger(FirstAttributes) || PxFilterObjectIsTrigger(SecondAttributes))
    {
        PairFlags = PxPairFlag::eTRIGGER_DEFAULT;
    }
    else
    {
        PairFlags = PxPairFlag::eCONTACT_DEFAULT | PxPairFlag::eNOTIFY_TOUCH_FOUND
            | PxPairFlag::eNOTIFY_TOUCH_PERSISTS | PxPairFlag::eNOTIFY_TOUCH_LOST
            | PxPairFlag::eNOTIFY_CONTACT_POINTS | PxPairFlag::eDETECT_CCD_CONTACT;
    }
    return PxFilterFlag::eDEFAULT;
}

class PhysXQueryFilter final : public PxQueryFilterCallback
{
public:
    explicit PhysXQueryFilter(const PhysicsQueryFilter& InFilter, bool bInOverlap = false, uint32_t InSimulationLayer = 0)
        : Filter(InFilter), bOverlap(bInOverlap), SimulationLayer(InSimulationLayer)
    {
    }

    PxQueryHitType::Enum preFilter(const PxFilterData&, const PxShape* Shape, const PxRigidActor* Actor, PxHitFlags&) override
    {
        if (SimulationLayer != 0)
        {
            if (!Shape->getFlags().isSet(PxShapeFlag::eSIMULATION_SHAPE)
                || (Shape->getQueryFilterData().word1 & SimulationLayer) == 0)
            {
                return PxQueryHitType::eNONE;
            }
        }
        else if (Actor->userData != nullptr && !static_cast<const PhysXBodyMetadata*>(Actor->userData)->bQueryEnabled)
        {
            return PxQueryHitType::eNONE;
        }
        if ((Shape->getQueryFilterData().word0 & Filter.LayerMask) == 0
            || (Filter.IgnoredBody != 0 && GetPhysXBodyId(Actor) == Filter.IgnoredBody)
            || (!Filter.bIncludeTriggers && Shape->getFlags().isSet(PxShapeFlag::eTRIGGER_SHAPE)))
        {
            return PxQueryHitType::eNONE;
        }
        if (bOverlap)
        {
            return PxQueryHitType::eTOUCH;
        }
        return PxQueryHitType::eBLOCK;
    }

    PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&, const PxShape*, const PxRigidActor*) override
    {
        return PxQueryHitType::eBLOCK;
    }

private:
    PhysicsQueryFilter Filter;
    bool bOverlap = false;
    uint32_t SimulationLayer = 0;
};

class PhysXControllerFilter final : public PxControllerFilterCallback
{
public:
    bool filter(const PxController& First, const PxController& Second) override
    {
        PxShape* FirstShape = nullptr;
        PxShape* SecondShape = nullptr;
        First.getActor()->getShapes(&FirstShape, 1);
        Second.getActor()->getShapes(&SecondShape, 1);
        if (FirstShape == nullptr || SecondShape == nullptr)
        {
            return false;
        }
        const PxFilterData FirstData = FirstShape->getSimulationFilterData();
        const PxFilterData SecondData = SecondShape->getSimulationFilterData();
        return (FirstData.word0 & SecondData.word1) != 0 && (SecondData.word0 & FirstData.word1) != 0;
    }
};

class PhysXOverlapCollector final : public PxOverlapCallback
{
public:
    PhysXOverlapCollector() : PxOverlapCallback(Hits, 64) {}

    PxAgain processTouches(const PxOverlapHit* Buffer, PxU32 Count) override
    {
        for (PxU32 HitIndex = 0; HitIndex < Count; ++HitIndex)
        {
            Bodies.push_back(GetPhysXBodyId(Buffer[HitIndex].actor));
        }
        return true;
    }

    PxOverlapHit Hits[64];
    std::vector<PhysicsBodyId> Bodies;
};

struct PhysXRuntime
{
    PxDefaultAllocator Allocator;
    PxDefaultErrorCallback Errors;
    PxFoundation* Foundation = nullptr;
    PxPhysics* Physics = nullptr;

    ~PhysXRuntime()
    {
        if (Physics != nullptr)
        {
            PxCloseExtensions();
            Physics->release();
        }
        if (Foundation != nullptr)
        {
            Foundation->release();
        }
    }
};

static std::shared_ptr<PhysXRuntime> AcquirePhysXRuntime()
{
    static std::weak_ptr<PhysXRuntime> ExistingRuntime;
    std::shared_ptr<PhysXRuntime> Runtime = ExistingRuntime.lock();
    if (!Runtime)
    {
        Runtime = std::make_shared<PhysXRuntime>();
        Runtime->Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, Runtime->Allocator, Runtime->Errors);
        if (Runtime->Foundation == nullptr)
        {
            return {};
        }
        Runtime->Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *Runtime->Foundation, PxTolerancesScale(), false, nullptr);
        if (Runtime->Physics == nullptr || !PxInitExtensions(*Runtime->Physics, nullptr))
        {
            return {};
        }
        ExistingRuntime = Runtime;
    }
    return Runtime;
}

class PhysXWorld final : public IPhysicsWorld, private PxSimulationEventCallback
{
public:
    ~PhysXWorld() override
    {
        AssertGameThread();
        while (!Bodies.empty())
        {
            DestroyBody(Bodies.begin()->first);
        }
        if (Controllers != nullptr)
        {
            Controllers->release();
        }
        if (World != nullptr)
        {
            World->release();
        }
        if (Dispatcher != nullptr)
        {
            Dispatcher->release();
        }
    }

    bool Initialize(const Vector3& Gravity, std::string& Error) override
    {
        AssertGameThread();
        if (!IsValidPhysXVector(Gravity))
        {
            Error = "Physics gravity must be finite";
            return false;
        }
        Runtime = AcquirePhysXRuntime();
        if (!Runtime)
        {
            Error = "PhysX initialization failed";
            return false;
        }
        Physics = Runtime->Physics;
        Dispatcher = PxDefaultCpuDispatcherCreate(0);
        PxSceneDesc Description(Physics->getTolerancesScale());
        Description.gravity = ToPhysXVector(Gravity);
        Description.cpuDispatcher = Dispatcher;
        Description.filterShader = FilterPhysXSimulation;
        Description.simulationEventCallback = this;
        Description.flags |= PxSceneFlag::eENABLE_CCD;
        Description.kineKineFilteringMode = PxPairFilteringMode::eKEEP;
        Description.staticKineFilteringMode = PxPairFilteringMode::eKEEP;
        World = Physics->createScene(Description);
        if (World == nullptr)
        {
            Error = "PhysX scene creation failed";
            return false;
        }
        Controllers = PxCreateControllerManager(*World);
        if (Controllers == nullptr)
        {
            Error = "PhysX character manager creation failed";
            return false;
        }
        return true;
    }

    PhysicsBodyId CreateBody(const PhysicsBodyDefinition& Definition, std::string& Error) override
    {
        AssertGameThread();
        if (World == nullptr || !ValidatePhysicsBody(Definition, Error))
        {
            return 0;
        }
        auto Body = std::make_unique<BodyState>();
        static std::atomic<uint64_t> NextIdentity{1};
        Body->Metadata.Identity = NextIdentity.fetch_add(1);
        Body->Metadata.bQueryEnabled = Definition.bQueryEnabled;
        Body->Definition = Definition;
        Body->Material = Physics->createMaterial(Definition.StaticFriction, Definition.DynamicFriction, Definition.Restitution);
        if (Definition.MotionType == PhysicsMotionType::Character)
        {
            PxCapsuleControllerDesc Description;
            Description.radius = Definition.Radius;
            Description.height = 2.f * Definition.CapsuleHalfHeight;
            Description.upDirection = PxVec3(0.f, 1.f, 0.f);
            Description.stepOffset = Definition.StepHeight;
            Description.slopeLimit = std::cos(Definition.MaximumSlopeDegrees * 3.14159265358979323846f / 180.f);
            Description.contactOffset = std::min(0.05f, Definition.Radius * 0.1f);
            Description.material = Body->Material;
            Description.climbingMode = PxCapsuleClimbingMode::eCONSTRAINED;
            Description.nonWalkableMode = PxControllerNonWalkableMode::ePREVENT_CLIMBING_AND_FORCE_SLIDING;
            Body->Controller = Controllers->createController(Description);
            if (Body->Controller == nullptr)
            {
                Body->Material->release();
                Error = "PhysX character creation failed";
                return 0;
            }
            const Vector3& Position = Definition.WorldTransform.Position;
            Body->Controller->setFootPosition(PxExtendedVec3(Position.x, Position.y, Position.z));
            Body->Actor = Body->Controller->getActor();
        }
        else
        {
            const PxTransform Pose = ToPhysXTransform(Definition.WorldTransform);
            if (Definition.MotionType == PhysicsMotionType::Static)
            {
                Body->Actor = Physics->createRigidStatic(Pose);
            }
            else
            {
                Body->Actor = Physics->createRigidDynamic(Pose);
            }
            if (Body->Actor == nullptr)
            {
                Body->Material->release();
                Error = "PhysX actor creation failed";
                return 0;
            }
            PxShapeFlags ShapeFlags = PxShapeFlag::eSCENE_QUERY_SHAPE;
            if (Definition.bTrigger)
            {
                ShapeFlags |= PxShapeFlag::eTRIGGER_SHAPE;
            }
            else if (Definition.bSimulationEnabled)
            {
                ShapeFlags |= PxShapeFlag::eSIMULATION_SHAPE;
            }
            std::vector<PhysicsShapeDefinition> Elements = Definition.Shapes;
            if (Elements.empty())
            {
                Elements.push_back(Definition);
            }
            bool bContainsTriangleMesh = false;
            for (const auto& Element : Elements)
            {
                PxGeometryHolder Geometry;
                PxTriangleMesh* TriangleMesh = nullptr;
                if (!MakeGeometry(Element, Geometry, TriangleMesh, Error))
                {
                    Body->Actor->release();
                    Body->Material->release();
                    return 0;
                }
                PxShape* Shape = Physics->createShape(Geometry.any(), *Body->Material, true, ShapeFlags);
                if (TriangleMesh != nullptr)
                {
                    TriangleMesh->release();
                }
                if (Shape == nullptr)
                {
                    Body->Actor->release();
                    Body->Material->release();
                    Error = "PhysX shape creation failed";
                    return 0;
                }
                auto Rotation = Element.ShapeRotation;
                Rotation.Normalize();
                PxTransform LocalPose(ToPhysXVector(Element.Center), PxQuat(Rotation.x, Rotation.y, Rotation.z, Rotation.w));
                if (Element.ShapeType == PhysicsShapeType::Capsule)
                {
                    LocalPose.q *= PxQuat(PxHalfPi, PxVec3(0.f, 0.f, 1.f));
                }
                Shape->setLocalPose(LocalPose);
                const bool bAttached = Body->Actor->attachShape(*Shape);
                Shape->release();
                if (!bAttached)
                {
                    Body->Actor->release();
                    Body->Material->release();
                    Error = "PhysX compound shape attachment failed";
                    return 0;
                }
                bContainsTriangleMesh = bContainsTriangleMesh || Element.ShapeType == PhysicsShapeType::TriangleMesh;
            }
            if (PxRigidDynamic* Dynamic = Body->Actor->is<PxRigidDynamic>())
            {
                Dynamic->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, Definition.MotionType == PhysicsMotionType::Kinematic);
                Dynamic->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD,
                    Definition.bContinuousCollisionDetection && Definition.MotionType == PhysicsMotionType::Dynamic);
                Dynamic->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, !Definition.bUseGravity);
                Dynamic->setLinearDamping(Definition.LinearDamping);
                Dynamic->setAngularDamping(Definition.AngularDamping);
                if (!bContainsTriangleMesh)
                {
                    PxRigidBodyExt::setMassAndUpdateInertia(*Dynamic, Definition.Mass, nullptr, true);
                }
            }
            World->addActor(*Body->Actor);
        }
        Body->Actor->userData = &Body->Metadata;
        std::vector<PxShape*> Shapes(Body->Actor->getNbShapes());
        Body->Actor->getShapes(Shapes.data(), static_cast<PxU32>(Shapes.size()));
        for (PxShape* Shape : Shapes)
        {
            Shape->setSimulationFilterData(PxFilterData(Definition.CollisionLayer, Definition.CollisionMask, 0, 0));
            Shape->setQueryFilterData(PxFilterData(Definition.CollisionLayer, Definition.CollisionMask, 0, 0));
        }
        const PhysicsBodyId Identity = Body->Metadata.Identity;
        Bodies.emplace(Identity, std::move(Body));
        return Identity;
    }

    void DestroyBody(PhysicsBodyId Identity) override
    {
        AssertGameThread();
        auto Found = Bodies.find(Identity);
        if (Found == Bodies.end())
        {
            return;
        }
        BodyState& Body = *Found->second;
        for (auto Iterator = ActiveShapePairs.begin(); Iterator != ActiveShapePairs.end();)
        {
            if (Iterator->second.first == Identity || Iterator->second.second == Identity)
            {
                ActiveShapeCounts.erase(Iterator->second);
                Iterator = ActiveShapePairs.erase(Iterator);
            }
            else
            {
                ++Iterator;
            }
        }
        for (auto Iterator = ActivePairs.begin(); Iterator != ActivePairs.end();)
        {
            if (Iterator->second.FirstBody == Identity || Iterator->second.SecondBody == Identity)
            {
                PhysicsContact Ended = Iterator->second;
                Ended.Kind = PhysicsContactKind::End;
                Contacts.push_back(Ended);
                Iterator = ActivePairs.erase(Iterator);
            }
            else
            {
                ++Iterator;
            }
        }
        if (Body.Controller != nullptr)
        {
            Body.Controller->release();
        }
        else
        {
            Body.Actor->release();
        }
        Body.Material->release();
        Bodies.erase(Found);
    }

    bool GetBodyTransform(PhysicsBodyId Identity, Transform& Result) const override
    {
        AssertGameThread();
        const BodyState* Body = FindBody(Identity);
        if (Body == nullptr)
        {
            return false;
        }
        const PxTransform Pose = Body->Actor->getGlobalPose();
        Result.Position = FromPhysXVector(Pose.p);
        Result.Rotation = Quaternion(Pose.q.x, Pose.q.y, Pose.q.z, Pose.q.w);
        Result.Scale = Vector3::One;
        if (Body->Controller != nullptr)
        {
            const PxExtendedVec3 Position = Body->Controller->getFootPosition();
            Result.Position = Vector3(static_cast<float>(Position.x), static_cast<float>(Position.y), static_cast<float>(Position.z));
            Result.Rotation = Body->Definition.WorldTransform.Rotation;
        }
        return true;
    }

    bool SetBodyTransform(PhysicsBodyId Identity, const Transform& Value, bool bTeleport) override
    {
        AssertGameThread();
        BodyState* Body = FindBody(Identity);
        const Quaternion& Rotation = Value.Rotation;
        if (Body == nullptr || !IsValidPhysXVector(Value.Position)
            || !std::isfinite(Rotation.x) || !std::isfinite(Rotation.y) || !std::isfinite(Rotation.z)
            || !std::isfinite(Rotation.w) || !std::isfinite(Rotation.LengthSquared()) || Rotation.LengthSquared() < 0.000001f)
        {
            return false;
        }
        Body->Definition.WorldTransform = Value;
        if (Body->Controller != nullptr)
        {
            return Body->Controller->setFootPosition(PxExtendedVec3(Value.Position.x, Value.Position.y, Value.Position.z));
        }
        if (PxRigidDynamic* Dynamic = Body->Actor->is<PxRigidDynamic>())
        {
            if (!bTeleport && Dynamic->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC))
            {
                Dynamic->setKinematicTarget(ToPhysXTransform(Value));
                return true;
            }
        }
        Body->Actor->setGlobalPose(ToPhysXTransform(Value));
        return true;
    }

    bool SetKinematic(PhysicsBodyId Identity, bool bKinematic) override
    {
        AssertGameThread();
        BodyState* Body = FindBody(Identity);
        if (Body != nullptr && Body->Controller == nullptr)
        {
            if (PxRigidDynamic* Dynamic = Body->Actor->is<PxRigidDynamic>())
            {
                if (bKinematic)
                {
                    Dynamic->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, false);
                }
                Dynamic->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, bKinematic);
                if (!bKinematic)
                {
                    Dynamic->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, Body->Definition.bContinuousCollisionDetection);
                }
                return true;
            }
        }
        return false;
    }

    bool SetLinearVelocity(PhysicsBodyId Identity, const Vector3& Velocity) override
    {
        AssertGameThread();
        if (!IsValidPhysXVector(Velocity))
        {
            return false;
        }
        BodyState* Body = FindBody(Identity);
        if (Body != nullptr && Body->Controller == nullptr)
        {
            if (PxRigidDynamic* Dynamic = Body->Actor->is<PxRigidDynamic>())
            {
                if (!Dynamic->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC))
                {
                    Dynamic->setLinearVelocity(ToPhysXVector(Velocity));
                    return true;
                }
            }
        }
        return false;
    }

    Vector3 GetLinearVelocity(PhysicsBodyId Identity) const override
    {
        AssertGameThread();
        const BodyState* Body = FindBody(Identity);
        if (Body != nullptr && Body->Controller == nullptr)
        {
            if (const PxRigidDynamic* Dynamic = Body->Actor->is<PxRigidDynamic>())
            {
                return FromPhysXVector(Dynamic->getLinearVelocity());
            }
        }
        return Vector3::Zero;
    }

    bool AddForce(PhysicsBodyId Identity, const Vector3& Force, bool bImpulse) override
    {
        AssertGameThread();
        if (!IsValidPhysXVector(Force))
        {
            return false;
        }
        BodyState* Body = FindBody(Identity);
        if (Body != nullptr && Body->Controller == nullptr)
        {
            if (PxRigidDynamic* Dynamic = Body->Actor->is<PxRigidDynamic>())
            {
                if (!Dynamic->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC))
                {
                    if (bImpulse)
                    {
                        Dynamic->addForce(ToPhysXVector(Force), PxForceMode::eIMPULSE);
                    }
                    else
                    {
                        Dynamic->addForce(ToPhysXVector(Force), PxForceMode::eFORCE);
                    }
                    return true;
                }
            }
        }
        return false;
    }

    bool SetAngularVelocity(PhysicsBodyId Identity, const Vector3& Velocity) override
    {
        AssertGameThread();
        BodyState* Body = FindBody(Identity);
        if (Body != nullptr && Body->Controller == nullptr && IsValidPhysXVector(Velocity))
        {
            if (PxRigidDynamic* Dynamic = Body->Actor->is<PxRigidDynamic>())
            {
                if (!Dynamic->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC))
                {
                    Dynamic->setAngularVelocity(ToPhysXVector(Velocity));
                    return true;
                }
            }
        }
        return false;
    }

    Vector3 GetAngularVelocity(PhysicsBodyId Identity) const override
    {
        AssertGameThread();
        const BodyState* Body = FindBody(Identity);
        if (Body != nullptr && Body->Controller == nullptr)
        {
            if (const PxRigidDynamic* Dynamic = Body->Actor->is<PxRigidDynamic>())
            {
                return FromPhysXVector(Dynamic->getAngularVelocity());
            }
        }
        return Vector3::Zero;
    }

    bool AddTorque(PhysicsBodyId Identity, const Vector3& Torque, bool bImpulse) override
    {
        AssertGameThread();
        BodyState* Body = FindBody(Identity);
        if (Body != nullptr && Body->Controller == nullptr && IsValidPhysXVector(Torque))
        {
            if (PxRigidDynamic* Dynamic = Body->Actor->is<PxRigidDynamic>())
            {
                if (!Dynamic->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC))
                {
                    if (bImpulse)
                    {
                        Dynamic->addTorque(ToPhysXVector(Torque), PxForceMode::eIMPULSE);
                    }
                    else
                    {
                        Dynamic->addTorque(ToPhysXVector(Torque), PxForceMode::eFORCE);
                    }
                    return true;
                }
            }
        }
        return false;
    }

    PhysicsCharacterResult MoveCharacter(PhysicsBodyId Identity, const Vector3& Displacement, float DeltaTime) override
    {
        AssertGameThread();
        PhysicsCharacterResult Result;
        BodyState* Body = FindBody(Identity);
        if (Body == nullptr || Body->Controller == nullptr || !std::isfinite(DeltaTime) || DeltaTime <= 0.f
            || !IsValidPhysXVector(Displacement))
        {
            return Result;
        }
        PhysicsQueryFilter Filter;
        Filter.IgnoredBody = Identity;
        Filter.LayerMask = Body->Definition.CollisionMask;
        PhysXQueryFilter Callback(Filter, false, Body->Definition.CollisionLayer);
        PhysXControllerFilter ControllerFilter;
        PxControllerFilters Filters(nullptr, &Callback, &ControllerFilter);
        const PxControllerCollisionFlags Flags = Body->Controller->move(ToPhysXVector(Displacement), 0.00001f, DeltaTime, Filters);
        const PxExtendedVec3 Position = Body->Controller->getFootPosition();
        Result.Position = Vector3(static_cast<float>(Position.x), static_cast<float>(Position.y), static_cast<float>(Position.z));
        Result.bGrounded = Flags.isSet(PxControllerCollisionFlag::eCOLLISION_DOWN);
        Result.bHitCeiling = Flags.isSet(PxControllerCollisionFlag::eCOLLISION_UP);
        Result.bHitWall = Flags.isSet(PxControllerCollisionFlag::eCOLLISION_SIDES);
        return Result;
    }

    void Simulate(float DeltaTime) override
    {
        AssertGameThread();
        if (!std::isfinite(DeltaTime) || DeltaTime <= 0.f)
        {
            return;
        }
        World->simulate(DeltaTime);
        World->fetchResults(true);
    }

    void SetGravity(const Vector3& Gravity) override
    {
        AssertGameThread();
        if (IsValidPhysXVector(Gravity))
        {
            World->setGravity(ToPhysXVector(Gravity));
        }
    }

    bool Raycast(const Vector3& Origin, const Vector3& Direction, float Distance,
        const PhysicsQueryFilter& Filter, PhysicsHit& Result) const override
    {
        AssertGameThread();
        if (!std::isfinite(Distance) || Distance <= 0.f || !IsValidPhysXVector(Origin) || !IsValidPhysXVector(Direction)
            || !std::isfinite(Direction.LengthSquared()) || Direction.LengthSquared() < 0.000001f)
        {
            return false;
        }
        Vector3 NormalizedDirection = Direction;
        NormalizedDirection.Normalize();
        PxRaycastBuffer Hit;
        PhysXQueryFilter Callback(Filter);
        PxQueryFilterData Query;
        Query.flags |= PxQueryFlag::ePREFILTER;
        if (!World->raycast(ToPhysXVector(Origin), ToPhysXVector(NormalizedDirection), Distance,
            Hit, PxHitFlag::eDEFAULT, Query, &Callback) || !Hit.hasBlock)
        {
            return false;
        }
        Result.Body = GetPhysXBodyId(Hit.block.actor);
        Result.Position = FromPhysXVector(Hit.block.position);
        Result.Normal = FromPhysXVector(Hit.block.normal);
        Result.Distance = Hit.block.distance;
        return true;
    }

    bool Sweep(const PhysicsBodyDefinition& Definition, const Vector3& Direction, float Distance,
        const PhysicsQueryFilter& Filter, PhysicsHit& Result) const override
    {
        AssertGameThread();
        if (!Definition.Shapes.empty())
        {
            std::string Error;
            if (!ValidatePhysicsBody(Definition, Error))
            {
                return false;
            }
            auto Element = Definition;
            Element.Shapes.clear();
            bool bHit = false;
            for (const auto& Shape : Definition.Shapes)
            {
                static_cast<PhysicsShapeDefinition&>(Element) = Shape;
                PhysicsHit Candidate;
                if (Sweep(Element, Direction, Distance, Filter, Candidate) && (!bHit || Candidate.Distance < Result.Distance))
                {
                    Result = Candidate;
                    bHit = true;
                }
            }
            return bHit;
        }
        if (!std::isfinite(Distance) || Distance <= 0.f || !IsValidPhysXVector(Direction)
            || !std::isfinite(Direction.LengthSquared()) || Direction.LengthSquared() < 0.000001f
            || Definition.ShapeType == PhysicsShapeType::TriangleMesh)
        {
            return false;
        }
        PxGeometryHolder Geometry;
        PxTriangleMesh* UnusedMesh = nullptr;
        std::string Error;
        if (!ValidatePhysicsBody(Definition, Error) || !MakeGeometry(Definition, Geometry, UnusedMesh, Error))
        {
            return false;
        }
        PxTransform Pose = ToPhysXTransform(Definition.WorldTransform);
        Pose.p += Pose.q.rotate(ToPhysXVector(Definition.Center));
        auto ShapeRotation = Definition.ShapeRotation;
        ShapeRotation.Normalize();
        Pose.q *= PxQuat(ShapeRotation.x, ShapeRotation.y, ShapeRotation.z, ShapeRotation.w);
        if (Definition.ShapeType == PhysicsShapeType::Capsule)
        {
            Pose.q *= PxQuat(PxHalfPi, PxVec3(0.f, 0.f, 1.f));
        }
        Vector3 NormalizedDirection = Direction;
        NormalizedDirection.Normalize();
        PxSweepBuffer Hit;
        PhysXQueryFilter Callback(Filter);
        PxQueryFilterData Query;
        Query.flags |= PxQueryFlag::ePREFILTER;
        if (!World->sweep(Geometry.any(), Pose, ToPhysXVector(NormalizedDirection), Distance,
            Hit, PxHitFlag::eDEFAULT, Query, &Callback) || !Hit.hasBlock)
        {
            return false;
        }
        Result.Body = GetPhysXBodyId(Hit.block.actor);
        Result.Position = FromPhysXVector(Hit.block.position);
        Result.Normal = FromPhysXVector(Hit.block.normal);
        Result.Distance = Hit.block.distance;
        Result.bInitialOverlap = Hit.block.hadInitialOverlap();
        return true;
    }

    std::vector<PhysicsBodyId> Overlap(const PhysicsBodyDefinition& Definition, const PhysicsQueryFilter& Filter) const override
    {
        AssertGameThread();
        if (!Definition.Shapes.empty())
        {
            std::string Error;
            if (!ValidatePhysicsBody(Definition, Error))
            {
                return {};
            }
            std::vector<PhysicsBodyId> Result;
            auto Element = Definition;
            Element.Shapes.clear();
            for (const auto& Shape : Definition.Shapes)
            {
                static_cast<PhysicsShapeDefinition&>(Element) = Shape;
                const auto Part = Overlap(Element, Filter);
                Result.insert(Result.end(), Part.begin(), Part.end());
            }
            std::sort(Result.begin(), Result.end());
            Result.erase(std::unique(Result.begin(), Result.end()), Result.end());
            return Result;
        }
        if (Definition.ShapeType == PhysicsShapeType::TriangleMesh)
        {
            return {};
        }
        PxGeometryHolder Geometry;
        PxTriangleMesh* UnusedMesh = nullptr;
        std::string Error;
        if (!ValidatePhysicsBody(Definition, Error) || !MakeGeometry(Definition, Geometry, UnusedMesh, Error))
        {
            return {};
        }
        PxTransform Pose = ToPhysXTransform(Definition.WorldTransform);
        Pose.p += Pose.q.rotate(ToPhysXVector(Definition.Center));
        auto ShapeRotation = Definition.ShapeRotation;
        ShapeRotation.Normalize();
        Pose.q *= PxQuat(ShapeRotation.x, ShapeRotation.y, ShapeRotation.z, ShapeRotation.w);
        if (Definition.ShapeType == PhysicsShapeType::Capsule)
        {
            Pose.q *= PxQuat(PxHalfPi, PxVec3(0.f, 0.f, 1.f));
        }
        PhysXOverlapCollector Collector;
        PhysXQueryFilter Callback(Filter, true);
        PxQueryFilterData Query;
        Query.flags |= PxQueryFlag::ePREFILTER | PxQueryFlag::eNO_BLOCK;
        World->overlap(Geometry.any(), Pose, Collector, Query, &Callback);
        for (PxU32 HitIndex = 0; HitIndex < Collector.nbTouches; ++HitIndex)
        {
            Collector.Bodies.push_back(GetPhysXBodyId(Collector.touches[HitIndex].actor));
        }
        std::sort(Collector.Bodies.begin(), Collector.Bodies.end());
        Collector.Bodies.erase(std::unique(Collector.Bodies.begin(), Collector.Bodies.end()), Collector.Bodies.end());
        return Collector.Bodies;
    }

    std::vector<PhysicsContact> ConsumeContacts() override
    {
        AssertGameThread();
        std::vector<PhysicsContact> Result;
        Result.swap(Contacts);
        return Result;
    }

    std::vector<PhysicsDebugLine> GetDebugLines() const override
    {
        AssertGameThread();
        std::vector<PhysicsDebugLine> Result;
        for (const auto& Entry : Bodies)
        {
            PhysicsBodyDefinition Definition = Entry.second->Definition;
            GetBodyTransform(Entry.first, Definition.WorldTransform);
            if (Definition.MotionType == PhysicsMotionType::Character)
            {
                Definition.Center = Vector3(0.f, Definition.Radius + Definition.CapsuleHalfHeight, 0.f);
            }
            const PhysicsTriangleMesh Mesh = MakePhysicsTriangleMesh(Definition);
            for (size_t TriangleIndex = 0; TriangleIndex + 2 < Mesh.Indices.size(); TriangleIndex += 3)
            {
                for (size_t EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
                {
                    PhysicsDebugLine Line;
                    Line.Start = Mesh.Vertices[Mesh.Indices[TriangleIndex + EdgeIndex]];
                    Line.End = Mesh.Vertices[Mesh.Indices[TriangleIndex + (EdgeIndex + 1) % 3]];
                    if (Definition.bTrigger)
                    {
                        Line.LineColor = Color(1.f, 0.65f, 0.2f, 1.f);
                    }
                    Result.push_back(Line);
                }
            }
        }
        return Result;
    }

private:
    struct BodyState
    {
        PhysXBodyMetadata Metadata;
        PxRigidActor* Actor = nullptr;
        PxController* Controller = nullptr;
        PxMaterial* Material = nullptr;
        PhysicsBodyDefinition Definition;
    };

    BodyState* FindBody(PhysicsBodyId Identity) const
    {
        auto Found = Bodies.find(Identity);
        if (Found == Bodies.end())
        {
            return nullptr;
        }
        return Found->second.get();
    }

    bool MakeGeometry(const PhysicsShapeDefinition& Definition, PxGeometryHolder& Geometry,
        PxTriangleMesh*& TriangleMesh, std::string& Error) const
    {
        switch (Definition.ShapeType)
        {
        case PhysicsShapeType::Box:
            Geometry.storeAny(PxBoxGeometry(ToPhysXVector(Definition.HalfExtents)));
            return true;
        case PhysicsShapeType::Sphere:
            Geometry.storeAny(PxSphereGeometry(Definition.Radius));
            return true;
        case PhysicsShapeType::Capsule:
            Geometry.storeAny(PxCapsuleGeometry(Definition.Radius, Definition.CapsuleHalfHeight));
            return true;
        case PhysicsShapeType::TriangleMesh:
        {
            std::vector<PxVec3> Vertices;
            Vertices.reserve(Definition.TriangleMesh.Vertices.size());
            for (const Vector3& Vertex : Definition.TriangleMesh.Vertices)
            {
                Vertices.push_back(ToPhysXVector(Vertex));
            }
            PxTriangleMeshDesc Description;
            Description.points.count = static_cast<PxU32>(Vertices.size());
            Description.points.stride = sizeof(PxVec3);
            Description.points.data = Vertices.data();
            Description.triangles.count = static_cast<PxU32>(Definition.TriangleMesh.Indices.size() / 3);
            Description.triangles.stride = 3 * sizeof(uint32_t);
            Description.triangles.data = Definition.TriangleMesh.Indices.data();
            TriangleMesh = PxCreateTriangleMesh(PxCookingParams(Physics->getTolerancesScale()),
                Description, Physics->getPhysicsInsertionCallback());
            if (TriangleMesh == nullptr)
            {
                Error = "PhysX triangle mesh cooking failed";
                return false;
            }
            Geometry.storeAny(PxTriangleMeshGeometry(TriangleMesh));
            return true;
        }
        }
        Error = "Unknown physics shape type";
        return false;
    }

    void onConstraintBreak(PxConstraintInfo*, PxU32) override {}
    void onWake(PxActor**, PxU32) override {}
    void onSleep(PxActor**, PxU32) override {}
    void onAdvance(const PxRigidBody* const*, const PxTransform*, const PxU32) override {}

    void RecordContact(PhysicsContact Contact, const PxShape* FirstShape, const PxShape* SecondShape)
    {
        const auto BodiesKey = std::minmax(Contact.FirstBody, Contact.SecondBody);
        const auto FirstIdentity = reinterpret_cast<uintptr_t>(FirstShape);
        const auto SecondIdentity = reinterpret_cast<uintptr_t>(SecondShape);
        const std::pair<uintptr_t, uintptr_t> ShapesKey{
            std::min(FirstIdentity, SecondIdentity), std::max(FirstIdentity, SecondIdentity)};
        if (Contact.Kind == PhysicsContactKind::End)
        {
            if (ActiveShapePairs.erase(ShapesKey) == 0)
            {
                return;
            }
            auto Found = ActiveShapeCounts.find(BodiesKey);
            if (Found != ActiveShapeCounts.end() && --Found->second > 0)
            {
                return;
            }
            ActiveShapeCounts.erase(BodiesKey);
            ActivePairs.erase(BodiesKey);
        }
        else
        {
            const bool bAlreadyTouching = ActivePairs.find(BodiesKey) != ActivePairs.end();
            if (ActiveShapePairs.emplace(ShapesKey, BodiesKey).second)
            {
                ++ActiveShapeCounts[BodiesKey];
            }
            if (bAlreadyTouching)
            {
                Contact.Kind = PhysicsContactKind::Persist;
            }
            ActivePairs[BodiesKey] = Contact;
        }
        Contacts.push_back(Contact);
    }

    void onContact(const PxContactPairHeader& Header, const PxContactPair* Pairs, PxU32 Count) override
    {
        if (Header.flags.isSet(PxContactPairHeaderFlag::eREMOVED_ACTOR_0)
            || Header.flags.isSet(PxContactPairHeaderFlag::eREMOVED_ACTOR_1))
        {
            return;
        }
        for (PxU32 PairIndex = 0; PairIndex < Count; ++PairIndex)
        {
            PhysicsContact Contact;
            Contact.FirstBody = GetPhysXBodyId(Header.actors[0]);
            Contact.SecondBody = GetPhysXBodyId(Header.actors[1]);
            const PxContactPair& Pair = Pairs[PairIndex];
            if (Pair.events.isSet(PxPairFlag::eNOTIFY_TOUCH_LOST))
            {
                Contact.Kind = PhysicsContactKind::End;
            }
            else if (Pair.events.isSet(PxPairFlag::eNOTIFY_TOUCH_PERSISTS))
            {
                Contact.Kind = PhysicsContactKind::Persist;
            }
            PxContactPairPoint Point;
            if (Pair.extractContacts(&Point, 1) > 0)
            {
                Contact.Position = FromPhysXVector(Point.position);
                Contact.Normal = FromPhysXVector(Point.normal);
                Contact.Impulse = FromPhysXVector(Point.impulse);
            }
            RecordContact(Contact, Pair.shapes[0], Pair.shapes[1]);
        }
    }

    void onTrigger(PxTriggerPair* Pairs, PxU32 Count) override
    {
        for (PxU32 PairIndex = 0; PairIndex < Count; ++PairIndex)
        {
            const PxTriggerPair& Pair = Pairs[PairIndex];
            if (Pair.flags.isSet(PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER)
                || Pair.flags.isSet(PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
            {
                continue;
            }
            PhysicsContact Contact;
            Contact.FirstBody = GetPhysXBodyId(Pair.triggerActor);
            Contact.SecondBody = GetPhysXBodyId(Pair.otherActor);
            Contact.bTrigger = true;
            if (Pair.status == PxPairFlag::eNOTIFY_TOUCH_LOST)
            {
                Contact.Kind = PhysicsContactKind::End;
            }
            RecordContact(Contact, Pair.triggerShape, Pair.otherShape);
        }
    }

    std::shared_ptr<PhysXRuntime> Runtime;
    PxPhysics* Physics = nullptr;
    PxDefaultCpuDispatcher* Dispatcher = nullptr;
    PxScene* World = nullptr;
    PxControllerManager* Controllers = nullptr;
    std::unordered_map<PhysicsBodyId, std::unique_ptr<BodyState>> Bodies;
    std::vector<PhysicsContact> Contacts;
    std::map<std::pair<PhysicsBodyId, PhysicsBodyId>, PhysicsContact> ActivePairs;
    std::map<std::pair<uintptr_t, uintptr_t>, std::pair<PhysicsBodyId, PhysicsBodyId>> ActiveShapePairs;
    std::map<std::pair<PhysicsBodyId, PhysicsBodyId>, size_t> ActiveShapeCounts;
};

std::unique_ptr<IPhysicsWorld> CreatePhysicsWorld()
{
    return std::make_unique<PhysXWorld>();
}
