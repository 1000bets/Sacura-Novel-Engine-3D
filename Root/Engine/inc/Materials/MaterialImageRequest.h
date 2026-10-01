#pragma once
#include "Materials/MaterialRenderSnapshot.h"
#include <atomic>
#include <mutex>

enum class MaterialImageState { Queued, Reading, Ready, Failed, Cancelled };

struct MaterialImageRequest
{
    std::shared_ptr<const MaterialRenderSnapshot> Snapshot;
    uint64_t Generation = 0;
    uint32_t Width = 0;
    uint32_t Height = 0;
    float Time = 0.f;
    bool bPreview = false;
    int32_t PreviewShape = 0;
    DirectX::SimpleMath::Vector4 Color = DirectX::SimpleMath::Vector4::One;
    std::atomic<MaterialImageState> State{MaterialImageState::Queued};
    std::atomic<bool> bCancelled{false};
    mutable std::mutex Mutex;
    std::vector<uint8_t> Pixels;
    std::string Diagnostic;
};
