#pragma once

#include "Rendering/RenderResourceHandles.h"
#include "Rendering/Resources/RenderMesh.h"

#include <unordered_map>

class RenderResourceManager
{
public:
    void Initialize(Diligent::IRenderDevice* Device);
    void Shutdown();

    MeshHandle CreateDefaultQuadMesh();
    const RenderMesh* GetMesh(MeshHandle Mesh) const;

private:
    Diligent::IRenderDevice* Device = nullptr;
    std::unordered_map<uint64_t, RenderMesh> Meshes;
    uint64_t NextMeshId = 1;
};
