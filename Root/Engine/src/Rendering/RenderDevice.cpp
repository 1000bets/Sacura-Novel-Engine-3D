#include "Rendering/RenderDevice.h"
#include "Core/Threading/ThreadContext.h"

#include <cassert>

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
#    ifdef _DEBUG
        EngineCI.SetValidationLevel(VALIDATION_LEVEL_2);
#    endif
        FactoryD3D12->CreateDeviceAndContextsD3D12(EngineCI, &State->Device, Contexts);
        if (!State->Device)
        {
            return false;
        }

        State->ImmediateContext = Contexts[0];
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
#    ifdef _DEBUG
        EngineCI.SetValidationLevel(VALIDATION_LEVEL_2);
#    endif
        FactoryVk->CreateDeviceAndContextsVk(EngineCI, &State->Device, Contexts);
        if (!State->Device)
        {
            return false;
        }

        State->ImmediateContext = Contexts[0];
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
        EngineCI.Window = Window;
#    ifdef _DEBUG
        EngineCI.SetValidationLevel(VALIDATION_LEVEL_2);
#    endif
        FactoryGL->CreateDeviceAndSwapChainGL(EngineCI, &State->Device, Contexts, SCDesc, &State->SwapChain);
        if (!State->Device || !State->SwapChain)
        {
            return false;
        }

        State->ImmediateContext = Contexts[0];
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
