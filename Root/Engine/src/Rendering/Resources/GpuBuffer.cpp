#include "Rendering/Resources/GpuBuffer.h"
#include "Core/Threading/ThreadContext.h"

#include "RenderDevice.h"
#include "DeviceContext.h"
#include "Buffer.h"
#include "RefCntAutoPtr.hpp"

#include <cassert>
#include <cstring>

using namespace Diligent;

struct GpuBuffer::State
{
    RefCntAutoPtr<IBuffer> Buffer;
    bool bDynamic = false;
};

GpuBuffer::~GpuBuffer()
{
    delete BufferState;
    BufferState = nullptr;
}

bool GpuBuffer::Create(
    IRenderDevice* Device,
    GpuBufferUsage Usage,
    const void* InitialData,
    uint64_t InByteSize,
    const char* Name,
    bool bDynamic)
{
    AssertRenderThread();
    assert(Device != nullptr);
    assert(InByteSize > 0);

    delete BufferState;
    BufferState = new State();
    BufferState->bDynamic = bDynamic;
    ByteSize = InByteSize;

    BufferDesc Desc;
    Desc.Name = Name;
    Desc.Size = InByteSize;
    Desc.Usage = bDynamic ? USAGE_DYNAMIC : USAGE_IMMUTABLE;
    Desc.CPUAccessFlags = bDynamic ? CPU_ACCESS_WRITE : CPU_ACCESS_NONE;

    switch (Usage)
    {
    case GpuBufferUsage::Vertex:
        Desc.BindFlags = BIND_VERTEX_BUFFER;
        break;
    case GpuBufferUsage::Index:
        Desc.BindFlags = BIND_INDEX_BUFFER;
        break;
    case GpuBufferUsage::Uniform:
        Desc.BindFlags = BIND_UNIFORM_BUFFER;
        break;
    }

    BufferData Data;
    if (InitialData != nullptr && !bDynamic)
    {
        Data.pData = InitialData;
        Data.DataSize = InByteSize;
    }

    Device->CreateBuffer(Desc, (InitialData != nullptr && !bDynamic) ? &Data : nullptr, &BufferState->Buffer);
    return BufferState->Buffer != nullptr;
}

void GpuBuffer::Update(IDeviceContext* Context, const void* Data, uint64_t InByteSize)
{
    AssertRenderThread();
    assert(BufferState != nullptr && BufferState->bDynamic);
    assert(InByteSize <= ByteSize);

    void* MappedData = nullptr;
    Context->MapBuffer(BufferState->Buffer, MAP_WRITE, MAP_FLAG_DISCARD, MappedData);
    memcpy(MappedData, Data, static_cast<size_t>(InByteSize));
    Context->UnmapBuffer(BufferState->Buffer, MAP_WRITE);
}

IBuffer* GpuBuffer::GetBuffer() const
{
    return BufferState ? BufferState->Buffer.RawPtr() : nullptr;
}
