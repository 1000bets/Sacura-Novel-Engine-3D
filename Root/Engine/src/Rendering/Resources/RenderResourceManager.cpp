#include "Rendering/Resources/RenderResourceManager.h"
#include "Rendering/Resources/RenderVertex.h"
#include "Core/Threading/ThreadContext.h"

#include <cassert>

void RenderResourceManager::Initialize(Diligent::IRenderDevice* InDevice)
{
    AssertRenderThread();
    Device = InDevice;
    Meshes.clear();
    NextMeshId = 1;
}

void RenderResourceManager::Shutdown()
{
    AssertRenderThread();
    Meshes.clear();
    Device = nullptr;
}

MeshHandle RenderResourceManager::CreateDefaultQuadMesh()
{
    AssertRenderThread();
    assert(Device != nullptr);

    const RenderVertex Vertices[4] = {
        {Vector3{-0.5f, -0.5f, 0.f}, Color{1.f, 0.2f, 0.2f, 1.f}},
        {Vector3{-0.5f, 0.5f, 0.f}, Color{0.2f, 1.f, 0.2f, 1.f}},
        {Vector3{0.5f, 0.5f, 0.f}, Color{0.2f, 0.2f, 1.f, 1.f}},
        {Vector3{0.5f, -0.5f, 0.f}, Color{1.f, 1.f, 0.2f, 1.f}},
    };

    const uint32_t Indices[6] = {0, 1, 2, 0, 2, 3};

    MeshHandle Handle;
    Handle.Value = NextMeshId++;

    RenderMesh& Mesh = Meshes[Handle.Value];
    const bool bCreated = Mesh.Create(
        Device,
        Vertices,
        sizeof(Vertices),
        4,
        Indices,
        sizeof(Indices),
        6);

    if (!bCreated)
    {
        Meshes.erase(Handle.Value);
        return MeshHandle{};
    }

    return Handle;
}

const RenderMesh* RenderResourceManager::GetMesh(MeshHandle Mesh) const
{
    auto Iterator = Meshes.find(Mesh.Value);
    if (Iterator == Meshes.end())
    {
        return nullptr;
    }

    return &Iterator->second;
}
