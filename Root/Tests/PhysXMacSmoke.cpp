#include <PxPhysicsAPI.h>
#include <cmath>
#include <iostream>

int main()
{
    using namespace physx;
    static_assert(PX_OSX && PX_A64, "This smoke test verifies native Apple Silicon PhysX");
    PxDefaultAllocator Allocator;
    PxDefaultErrorCallback Errors;
    PxFoundation* Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, Allocator, Errors);
    if (Foundation == nullptr)
    {
        return 1;
    }
    PxPhysics* Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *Foundation, PxTolerancesScale());
    if (Physics == nullptr)
    {
        Foundation->release();
        return 2;
    }
    PxDefaultCpuDispatcher* Dispatcher = PxDefaultCpuDispatcherCreate(2);
    PxSceneDesc Description(Physics->getTolerancesScale());
    Description.gravity = PxVec3(0.0f, -9.81f, 0.0f);
    Description.cpuDispatcher = Dispatcher;
    Description.filterShader = PxDefaultSimulationFilterShader;
    PxScene* Scene = Physics->createScene(Description);
    PxMaterial* Material = Physics->createMaterial(0.5f, 0.5f, 0.0f);
    if (Scene == nullptr || Material == nullptr)
    {
        return 3;
    }
    PxRigidStatic* Ground = PxCreatePlane(*Physics, PxPlane(0, 1, 0, 0), *Material);
    PxRigidDynamic* Sphere = PxCreateDynamic(*Physics, PxTransform(PxVec3(0, 3, 0)),
        PxSphereGeometry(0.5f), *Material, 1.0f);
    if (Ground == nullptr || Sphere == nullptr)
    {
        return 4;
    }
    Scene->addActor(*Ground);
    Scene->addActor(*Sphere);
    for (int Step = 0; Step < 180; ++Step)
    {
        Scene->simulate(1.0f / 60.0f);
        Scene->fetchResults(true);
    }
    const float Height = Sphere->getGlobalPose().p.y;
    const bool Passed = std::isfinite(Height) && std::abs(Height - 0.5f) < 0.05f;
    std::cout << "PhysX " << PX_PHYSICS_VERSION_MAJOR << '.' << PX_PHYSICS_VERSION_MINOR
        << '.' << PX_PHYSICS_VERSION_BUGFIX << " macOS ARM64: 180 CPU steps, sphere y="
        << Height << (Passed ? " PASS\n" : " FAIL\n");
    Sphere->release();
    Ground->release();
    Material->release();
    Scene->release();
    Dispatcher->release();
    Physics->release();
    Foundation->release();
    return Passed ? 0 : 5;
}
