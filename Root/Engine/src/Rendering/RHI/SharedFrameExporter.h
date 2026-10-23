#pragma once
#include "Rendering/SharedGpuFrame.h"
#include <memory>
class RenderDevice;
namespace Diligent { struct ITexture; }
class SharedFrameExporter
{
public:
    explicit SharedFrameExporter(std::shared_ptr<SharedGpuFrameStream> Stream);
    ~SharedFrameExporter();
    void Export(RenderDevice& Device, Diligent::ITexture* Source, uint64_t FrameIndex);
private:
    struct State;
    std::unique_ptr<State> Data;
};
