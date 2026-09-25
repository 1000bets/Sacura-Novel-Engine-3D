#include "Rendering/RHI/RenderDevice.h"
#include "Core/Threading/ThreadContext.h"

#include <cassert>
#include <unordered_map>

#include "GraphicsTypes.h"
#include "RefCntAutoPtr.hpp"

#if D3D12_SUPPORTED
#    include "EngineFactoryD3D12.h"
#endif
#if VULKAN_SUPPORTED
#    include "EngineFactoryVk.h"
#endif
#if GL_SUPPORTED
#    include "EngineFactoryOpenGL.h"
#endif
#include "NativeWindow.h"

using namespace Diligent;

struct RenderDevice::DiligentState
{
    RefCntAutoPtr<IEngineFactory> Factory;
    RefCntAutoPtr<IRenderDevice> Device;
    RefCntAutoPtr<IDeviceContext> ImmediateContext;
    RefCntAutoPtr<ISwapChain> SwapChain;
    std::unordered_map<uint32_t, RefCntAutoPtr<ISwapChain>> Surfaces;
};

GraphicsBackend RenderDevice::ResolveBackend(GraphicsBackend Preference) const
{
    if (Preference != GraphicsBackend::Auto)
    {
        return Preference;
    }

#if defined(_WIN32) && D3D12_SUPPORTED
    return GraphicsBackend::D3D12;
#elif VULKAN_SUPPORTED
    return GraphicsBackend::Vulkan;
#elif GL_SUPPORTED
    return GraphicsBackend::OpenGL;
#else
    return GraphicsBackend::D3D12;
#endif
}

void RenderDevice::Initialize(const NativeWindowInfo& WindowInfo, GraphicsBackend BackendPreference)
{
    AssertRenderThread();
    assert(!bInitialized);

    State = new DiligentState();
    Width = WindowInfo.Width;
    Height = WindowInfo.Height;

    const GraphicsBackend Preferred = ResolveBackend(BackendPreference);
    GraphicsBackend Candidates[] = {Preferred, GraphicsBackend::D3D12, GraphicsBackend::Vulkan, GraphicsBackend::OpenGL};

    bool bCreated = false;
    for (GraphicsBackend Candidate : Candidates)
    {
        if (State->ImmediateContext)
        {
            State->ImmediateContext->Flush();
            State->ImmediateContext->WaitForIdle();
        }
        State->SwapChain.Release();
        State->ImmediateContext.Release();
        State->Device.Release();
        State->Factory.Release();
        if (InitializeBackend(Candidate, WindowInfo))
        {
            ActiveBackend = Candidate;
            bCreated = true;
            break;
        }
    }

    if (!bCreated)
    {
        PrintString("RenderDevice: failed to initialize any graphics backend");
        delete State;
        State = nullptr;
        return;
    }

    bInitialized = true;

    const char* BackendName = "Unknown";
    switch (ActiveBackend)
    {
    case GraphicsBackend::D3D12: BackendName = "D3D12"; break;
    case GraphicsBackend::Vulkan: BackendName = "Vulkan"; break;
    case GraphicsBackend::OpenGL: BackendName = "OpenGL"; break;
    default: break;
    }

    PrintString(std::string("Renderer initialized"));
    PrintString(std::string("Backend: ") + BackendName);
    PrintString(std::string("GPU: ") + AdapterName);
    PrintString("Resolution: " + std::to_string(Width) + "x" + std::to_string(Height));
}

bool RenderDevice::InitializeBackend(GraphicsBackend Backend, const NativeWindowInfo& WindowInfo)
{
    if (WindowInfo.WindowHandle == nullptr)
    {
        PrintString("RenderDevice: missing native window handle");
        return false;
    }

    SwapChainDesc SCDesc;
    SCDesc.Width = WindowInfo.Width;
    SCDesc.Height = WindowInfo.Height;
    SCDesc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM_SRGB;
    SCDesc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;
    SCDesc.BufferCount = 2;
    SCDesc.IsPrimary = false;

    NativeWindow Window{};
#if PLATFORM_WIN32
    Window.hWnd = WindowInfo.WindowHandle;
#endif

    IDeviceContext* Contexts[1] = {};

#if D3D12_SUPPORTED
    if (Backend == GraphicsBackend::D3D12)
    {
#    if ENGINE_DLL
        auto GetFactory = LoadGraphicsEngineD3D12();
        IEngineFactoryD3D12* FactoryD3D12 = GetFactory();
#    else
        IEngineFactoryD3D12* FactoryD3D12 = GetEngineFactoryD3D12();
#    endif
        if (!FactoryD3D12 || !FactoryD3D12->LoadD3D12())
        {
            return false;
        }

        EngineD3D12CreateInfo EngineCI;
        EngineCI.Features.DurationQueries = DEVICE_FEATURE_STATE_OPTIONAL;
        EngineCI.Features.IndependentBlend = DEVICE_FEATURE_STATE_ENABLED;
#    ifdef _DEBUG
        EngineCI.SetValidationLevel(VALIDATION_LEVEL_2);
#    endif
        FactoryD3D12->CreateDeviceAndContextsD3D12(EngineCI, &State->Device, Contexts);
        if (!State->Device)
        {
            return false;
        }

        State->ImmediateContext.Attach(Contexts[0]);
        FactoryD3D12->CreateSwapChainD3D12(State->Device, State->ImmediateContext, SCDesc, FullScreenModeDesc{}, Window, &State->SwapChain);
        State->Factory = FactoryD3D12;

        const GraphicsAdapterInfo& Adapter = State->Device->GetAdapterInfo();
        AdapterName = Adapter.Description;
        return State->SwapChain != nullptr;
    }
#endif

#if VULKAN_SUPPORTED
    if (Backend == GraphicsBackend::Vulkan)
    {
#    if ENGINE_DLL
        auto GetFactory = LoadGraphicsEngineVk();
        IEngineFactoryVk* FactoryVk = GetFactory();
#    else
        IEngineFactoryVk* FactoryVk = GetEngineFactoryVk();
#    endif
        if (!FactoryVk)
        {
            return false;
        }

        EngineVkCreateInfo EngineCI;
        EngineCI.Features.DurationQueries = DEVICE_FEATURE_STATE_OPTIONAL;
        EngineCI.Features.IndependentBlend = DEVICE_FEATURE_STATE_ENABLED;
#    ifdef _DEBUG
        EngineCI.SetValidationLevel(VALIDATION_LEVEL_2);
#    endif
        FactoryVk->CreateDeviceAndContextsVk(EngineCI, &State->Device, Contexts);
        if (!State->Device)
        {
            return false;
        }

        State->ImmediateContext.Attach(Contexts[0]);
        FactoryVk->CreateSwapChainVk(State->Device, State->ImmediateContext, SCDesc, Window, &State->SwapChain);
        State->Factory = FactoryVk;

        const GraphicsAdapterInfo& Adapter = State->Device->GetAdapterInfo();
        AdapterName = Adapter.Description;
        return State->SwapChain != nullptr;
    }
#endif

#if GL_SUPPORTED
    if (Backend == GraphicsBackend::OpenGL)
    {
#    if ENGINE_DLL
        auto GetFactory = LoadGraphicsEngineOpenGL();
        IEngineFactoryOpenGL* FactoryGL = GetFactory();
#    else
        IEngineFactoryOpenGL* FactoryGL = GetEngineFactoryOpenGL();
#    endif
        if (!FactoryGL)
        {
            return false;
        }

        EngineGLCreateInfo EngineCI;
        EngineCI.Features.DurationQueries = DEVICE_FEATURE_STATE_OPTIONAL;
        EngineCI.Features.IndependentBlend = DEVICE_FEATURE_STATE_ENABLED;
        EngineCI.Window = Window;
#    ifdef _DEBUG
        EngineCI.SetValidationLevel(VALIDATION_LEVEL_2);
#    endif
        FactoryGL->CreateDeviceAndSwapChainGL(EngineCI, &State->Device, Contexts, SCDesc, &State->SwapChain);
        if (!State->Device || !State->SwapChain)
        {
            return false;
        }

        State->ImmediateContext.Attach(Contexts[0]);
        State->Factory = FactoryGL;

        const GraphicsAdapterInfo& Adapter = State->Device->GetAdapterInfo();
        AdapterName = Adapter.Description;
        return true;
    }
#endif

    return false;
}

void RenderDevice::Shutdown()
{
    AssertRenderThread();
    if (!bInitialized)
    {
        return;
    }

    if (State != nullptr)
    {
        if (State->ImmediateContext)
        {
            State->ImmediateContext->Flush();
            State->ImmediateContext->WaitForIdle();
        }

        State->Surfaces.clear();
        State->SwapChain.Release();
        State->ImmediateContext.Release();
        State->Device.Release();
        State->Factory.Release();
        delete State;
        State = nullptr;
    }

    bInitialized = false;
    PrintString("RenderDevice: shutdown");
}

void RenderDevice::Resize(uint32_t NewWidth, uint32_t NewHeight)
{
    AssertRenderThread();
    if (!bInitialized || State == nullptr || !State->SwapChain)
    {
        return;
    }

    if (NewWidth == 0 || NewHeight == 0)
    {
        return;
    }

    Width = NewWidth;
    Height = NewHeight;
    WaitForIdle();
    State->SwapChain->Resize(NewWidth, NewHeight);
    PrintString("RenderDevice: resized to " + std::to_string(NewWidth) + "x" + std::to_string(NewHeight));
}

IRenderDevice* RenderDevice::GetDevice() const
{
    return State ? State->Device.RawPtr() : nullptr;
}

IDeviceContext* RenderDevice::GetImmediateContext() const
{
    return State ? State->ImmediateContext.RawPtr() : nullptr;
}

ISwapChain* RenderDevice::GetSwapChain() const
{
    return State ? State->SwapChain.RawPtr() : nullptr;
}

IEngineFactory* RenderDevice::GetEngineFactory() const
{
    return State ? State->Factory.RawPtr() : nullptr;
}

void RenderDevice::WaitForIdle()
{
    AssertRenderThread();
    if (State != nullptr && State->ImmediateContext)
    {
        State->ImmediateContext->Flush();
        State->ImmediateContext->WaitForIdle();
    }
}

bool RenderDevice::AddSurface(RenderSurfaceId Surface, const NativeWindowInfo& WindowInfo)
{
    AssertRenderThread();
    if (!bInitialized || Surface.Value == 0 || GetSurface(Surface) != nullptr)
    {
        return false;
    }
    SwapChainDesc Description;
    Description.Width = WindowInfo.Width;
    Description.Height = WindowInfo.Height;
    Description.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM_SRGB;
    Description.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;
    Description.BufferCount = 2;
    Description.IsPrimary = false;
    NativeWindow Window{};
#if PLATFORM_WIN32
    Window.hWnd = WindowInfo.WindowHandle;
#endif
    RefCntAutoPtr<ISwapChain> Created;
#if D3D12_SUPPORTED
    if (ActiveBackend == GraphicsBackend::D3D12)
    {
        RefCntAutoPtr<IEngineFactoryD3D12> Factory(State->Factory, IID_EngineFactoryD3D12);
        Factory->CreateSwapChainD3D12(State->Device, State->ImmediateContext, Description,
            FullScreenModeDesc{}, Window, &Created);
    }
#endif
#if VULKAN_SUPPORTED
    if (ActiveBackend == GraphicsBackend::Vulkan)
    {
        RefCntAutoPtr<IEngineFactoryVk> Factory(State->Factory, IID_EngineFactoryVk);
        Factory->CreateSwapChainVk(State->Device, State->ImmediateContext, Description, Window, &Created);
    }
#endif
    if (!Created)
    {
        PrintString("RenderDevice: additional native surfaces require D3D12 or Vulkan");
        return false;
    }
    if (Surface.Value == 1)
    {
        State->SwapChain = std::move(Created);
    }
    else
    {
        State->Surfaces.emplace(Surface.Value, std::move(Created));
    }
    return true;
}

ISwapChain* RenderDevice::GetSurface(RenderSurfaceId Surface) const
{
    if (State == nullptr)
    {
        return nullptr;
    }
    if (Surface.Value == 1)
    {
        return State->SwapChain;
    }
    const auto Iterator = State->Surfaces.find(Surface.Value);
    if (Iterator == State->Surfaces.end())
    {
        return nullptr;
    }
    return Iterator->second;
}

void RenderDevice::RemoveSurface(RenderSurfaceId Surface)
{
    AssertRenderThread();
    State->ImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
    State->ImmediateContext->InvalidateState();
    WaitForIdle();
    if (Surface.Value == 1)
    {
        State->SwapChain.Release();
    }
    else
    {
        State->Surfaces.erase(Surface.Value);
    }
    WaitForIdle();
    State->Device->ReleaseStaleResources(true);
}

void RenderDevice::ResizeSurface(RenderSurfaceId Surface, uint32_t NewWidth, uint32_t NewHeight)
{
    AssertRenderThread();
    if (auto* SwapChain = GetSurface(Surface))
    {
        if (NewWidth > 0 && NewHeight > 0)
        {
            WaitForIdle();
            SwapChain->Resize(NewWidth, NewHeight);
        }
    }
}
