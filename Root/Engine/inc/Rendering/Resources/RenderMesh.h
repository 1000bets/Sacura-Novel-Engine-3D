#pragma once

#include "Rendering/Resources/GpuBuffer.h"

#include <cstdint>
#include <memory>

class RenderMesh
{
public:
    bool Create(
        Diligent::IRenderDevice* Device,
        const void* Vertices,
        uint64_t VertexByteSize,
        uint32_t VertexCount,
        const void* Indices,
        uint64_t IndexByteSize,
        uint32_t IndexCount);

    GpuBuffer& GetVertexBuffer() { return Vertices; }
    GpuBuffer& GetIndexBuffer() { return Indices; }
    const GpuBuffer& GetVertexBuffer() const { return Vertices; }
    const GpuBuffer& GetIndexBuffer() const { return Indices; }

    uint32_t GetVertexCount() const { return VertexCount; }
    uint32_t GetIndexCount() const { return IndexCount; }

private:
    GpuBuffer Vertices;
    GpuBuffer Indices;
    uint32_t VertexCount = 0;
    uint32_t IndexCount = 0;
};
