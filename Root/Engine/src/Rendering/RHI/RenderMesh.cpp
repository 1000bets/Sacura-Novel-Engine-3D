#include "Rendering/RHI/RenderMesh.h"
#include "Core/Threading/ThreadContext.h"

bool RenderMesh::Create(
    Diligent::IRenderDevice* Device,
    const void* InVertices,
    uint64_t VertexByteSize,
    uint32_t InVertexCount,
    const void* InIndices,
    uint64_t IndexByteSize,
    uint32_t InIndexCount)
{
    AssertRenderThread();

    if (!Vertices.Create(Device, GpuBufferUsage::Vertex, InVertices, VertexByteSize, "RenderMesh VB"))
    {
        return false;
    }

    if (!Indices.Create(Device, GpuBufferUsage::Index, InIndices, IndexByteSize, "RenderMesh IB"))
    {
        return false;
    }

    VertexCount = InVertexCount;
    IndexCount = InIndexCount;
    return true;
}
