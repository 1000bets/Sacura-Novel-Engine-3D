#include "Core/Threading/CpuProfileScope.h"
#include "Rendering/RHI/SharedFrameExporter.h"
#include "Rendering/RHI/RenderDevice.h"
#include "Core/Threading/ThreadContext.h"
#include "Texture.h"
#include "DeviceContext.h"
#include "Fence.h"
#include "RefCntAutoPtr.hpp"
#include <chrono>
#include <algorithm>
#include <stdexcept>
#include <deque>
#if defined(_WIN32) && D3D12_SUPPORTED
#include <d3d12.h>
#include "RenderDeviceD3D12.h"
#endif

struct SharedFrameExporter::State
{
    std::shared_ptr<SharedGpuFrameStream> Stream;
    uint64_t NextToken = 1;
#if defined(_WIN32) && D3D12_SUPPORTED
    HANDLE Consumer = nullptr;
    Diligent::RefCntAutoPtr<Diligent::IFence> Completion;
    std::deque<SharedGpuFrame> PendingFrames;
    struct Slot {
        Diligent::RefCntAutoPtr<Diligent::ITexture> Texture;
        HANDLE Remote = nullptr;
        std::shared_ptr<std::atomic<bool>> Released = std::make_shared<std::atomic<bool>>(true);
        uint32_t Width = 0, Height = 0;
    };
    std::vector<std::unique_ptr<Slot>> Slots;
    void CloseRemote(HANDLE Remote) {
        if (!Remote || !Consumer) return;
        HANDLE Local = nullptr;
        if (DuplicateHandle(Consumer, Remote, GetCurrentProcess(), &Local, 0, FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE)) CloseHandle(Local);
    }
    ~State() { for (auto& Slot : Slots) CloseRemote(Slot->Remote); if (Consumer) CloseHandle(Consumer); }
#endif
};
SharedFrameExporter::SharedFrameExporter(std::shared_ptr<SharedGpuFrameStream> Stream) : Data(std::make_unique<State>()) { Data->Stream = std::move(Stream); }
SharedFrameExporter::~SharedFrameExporter() = default;
void SharedFrameExporter::Poll()
{
    AssertRenderThread();
#if defined(_WIN32) && D3D12_SUPPORTED
    if (!Data->Completion)
    {
        return;
    }
    const uint64_t Completed = Data->Completion->GetCompletedValue();
    while (!Data->PendingFrames.empty() && Data->PendingFrames.front().Token <= Completed)
    {
        auto Frame = std::move(Data->PendingFrames.front());
        Data->PendingFrames.pop_front();
        Frame.ReadyMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        Data->Stream->Publish(std::move(Frame));
    }
#endif
}
void SharedFrameExporter::Export(RenderDevice& Device, Diligent::ITexture* Source, uint64_t FrameIndex)
{
    CpuProfileScope CpuMeasurement("Render.SharedExport");
    AssertRenderThread();
    if (!Data->Stream->Enabled || !Source) return;
#if defined(_WIN32) && D3D12_SUPPORTED
    using namespace Diligent;
    try {
        Poll();
        if (Device.GetActiveBackend() != GraphicsBackend::D3D12) throw std::runtime_error("Shared GPU export currently requires D3D12; fallback backend cannot export.");
        if (!Data->Completion)
        {
            FenceDesc Description;
            Description.Name = "Shared frame producer completion";
            Device.GetDevice()->CreateFence(Description, &Data->Completion);
            if (!Data->Completion)
            {
                throw std::runtime_error("Shared frame producer fence allocation failed.");
            }
        }
        if (!Data->Consumer) Data->Consumer = OpenProcess(PROCESS_DUP_HANDLE, FALSE, Data->Stream->ConsumerProcess);
        if (!Data->Consumer) throw std::runtime_error("Cannot open GPU frame consumer process.");
        const auto Desc = Source->GetDesc();
        // Retire old generations only after the consumer's completed-frame ACK.
        for (auto It = Data->Slots.begin(); It != Data->Slots.end();) {
            if (((*It)->Width != Desc.Width || (*It)->Height != Desc.Height) && (*It)->Released->load()) {
                Data->CloseRemote((*It)->Remote); It = Data->Slots.erase(It);
            } else ++It;
        }
        State::Slot* Available = nullptr;
        unsigned Matching = 0;
        for (auto& Slot : Data->Slots) if (Slot->Width == Desc.Width && Slot->Height == Desc.Height) {
            ++Matching; if (Slot->Released->load()) Available = Slot.get();
        }
        if (!Available && Matching < 3 && Data->Slots.size() < 9) {
            auto Slot = std::make_unique<State::Slot>();
            RefCntAutoPtr<IRenderDeviceD3D12> NativeDevice(Device.GetDevice(), IID_RenderDeviceD3D12);
            D3D12_HEAP_PROPERTIES Heap{}; Heap.Type = D3D12_HEAP_TYPE_DEFAULT;
            D3D12_RESOURCE_DESC Resource{};
            Resource.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            Resource.Width = Desc.Width; Resource.Height = Desc.Height;
            Resource.DepthOrArraySize = 1; Resource.MipLevels = 1;
            Resource.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            Resource.SampleDesc.Count = 1;
            Resource.Flags = D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            ID3D12Resource* NativeTexture = nullptr;
            auto* D3D = NativeDevice->GetD3D12Device();
            if (FAILED(D3D->CreateCommittedResource(&Heap, D3D12_HEAP_FLAG_SHARED, &Resource,
                D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&NativeTexture)))) throw std::runtime_error("D3D12 shared texture allocation failed.");
            HANDLE Local = nullptr;
            const HRESULT Created = D3D->CreateSharedHandle(NativeTexture, nullptr, GENERIC_ALL, nullptr, &Local);
            NativeDevice->CreateTextureFromD3DResource(NativeTexture, RESOURCE_STATE_COMMON, &Slot->Texture);
            NativeTexture->Release();
            if (FAILED(Created)) throw std::runtime_error("D3D12 CreateSharedHandle failed.");
            const bool Duplicated = DuplicateHandle(GetCurrentProcess(), Local, Data->Consumer, &Slot->Remote, 0, FALSE, DUPLICATE_SAME_ACCESS) != 0;
            CloseHandle(Local);
            if (!Duplicated || !Slot->Texture) { Data->CloseRemote(Slot->Remote); throw std::runtime_error("Shared texture wrapping/handle duplication failed."); }
            Slot->Width = Desc.Width; Slot->Height = Desc.Height;
            Available = Slot.get(); Data->Slots.push_back(std::move(Slot));
        }
        uint64_t Bytes = 0;
        for (const auto& Slot : Data->Slots) Bytes += uint64_t(Slot->Width) * Slot->Height * 4;
        Data->Stream->TextureCount = Data->Slots.size(); Data->Stream->TextureBytes = Bytes;
        if (!Available) { ++Data->Stream->Dropped; return; }
        auto* Context = Device.GetImmediateContext();
        Context->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
        CopyTextureAttribs Copy; Copy.pSrcTexture = Source; Copy.pDstTexture = Available->Texture;
        Copy.SrcTextureTransitionMode = Copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        Context->CopyTexture(Copy);
        StateTransitionDesc Transition(Available->Texture, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COMMON, STATE_TRANSITION_FLAG_UPDATE_STATE);
        Context->TransitionResourceStates(1, &Transition);
        Available->Released = std::make_shared<std::atomic<bool>>(false);
        SharedGpuFrame Frame;
        Frame.Token = Data->NextToken++; Frame.WindowsNtHandle = reinterpret_cast<uintptr_t>(Available->Remote);
        Frame.FrameIndex = FrameIndex; Frame.Width = Desc.Width; Frame.Height = Desc.Height;
        Frame.ReadyMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        Frame.Released = Available->Released;
        Context->EnqueueSignal(Data->Completion, Frame.Token);
        Data->PendingFrames.push_back(std::move(Frame));
        Context->Flush();
        Poll();
    } catch (const std::exception& Error) { Data->Stream->Fail(Error.what()); }
#else
    Data->Stream->Fail("Shared GPU export is implemented only for Windows D3D12.");
#endif
}

