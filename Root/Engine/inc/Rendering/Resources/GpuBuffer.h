#pragma once

#include <cstddef>
#include <cstdint>

namespace Diligent
{
struct IBuffer;
struct IRenderDevice;
struct IDeviceContext;
}

enum class GpuBufferUsage
{
    Vertex,
    Index,
    Uniform
};

class GpuBuffer
{
public:
    GpuBuffer() = default;
    ~GpuBuffer();

    GpuBuffer(const GpuBuffer&) = delete;
    GpuBuffer& operator=(const GpuBuffer&) = delete;

    bool Create(
        Diligent::IRenderDevice* Device,
        GpuBufferUsage Usage,
        const void* InitialData,
        uint64_t ByteSize,
        const char* Name,
        bool bDynamic = false);

    void Update(Diligent::IDeviceContext* Context, const void* Data, uint64_t ByteSize);

    Diligent::IBuffer* GetBuffer() const;
    uint64_t GetByteSize() const { return ByteSize; }

private:
    struct State;
    State* BufferState = nullptr;
    uint64_t ByteSize = 0;
};
