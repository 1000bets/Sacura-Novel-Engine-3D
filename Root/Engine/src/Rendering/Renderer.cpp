#include "Rendering/Renderer.h"
#include "Core/Threading/RenderFrameData.h"
#include "Core/Threading/ThreadContext.h"
#include "Rendering/Resources/RenderVertex.h"

#include <cassert>

#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"
#include "PipelineState.h"
#include "Shader.h"
#include "ShaderResourceBinding.h"
#include "TextureView.h"
#include "GraphicsTypes.h"
#include "MapHelper.hpp"
#include "RefCntAutoPtr.hpp"
#include "EngineFactory.h"

using namespace Diligent;

struct Renderer::PipelineState
{
    RefCntAutoPtr<IShader> VertexShader;
    RefCntAutoPtr<IShader> PixelShader;
    RefCntAutoPtr<IPipelineState> Pipeline;
    RefCntAutoPtr<IShaderResourceBinding> ResourceBinding;
};

void Renderer::Initialize(const NativeWindowInfo& WindowInfo, GraphicsBackend BackendPreference, const std::string& ShaderDirectory)
{
    AssertRenderThread();
    assert(!bInitialized);

    Device.Initialize(WindowInfo, BackendPreference);
    if (!Device.IsInitialized())
    {
        PrintString("Renderer: device initialization failed");
        return;
    }

    Resources.Initialize(Device.GetDevice());
    Pipeline = new PipelineState();

    if (!CreateShaders(ShaderDirectory) || !CreatePipeline() || !CreateConstantBuffers())
    {
        PrintString("Renderer: pipeline initialization failed");
        Shutdown();
        return;
    }

    CreateDefaultMesh();
    bInitialized = true;
    PrintString("Renderer: ready");
}

bool Renderer::CreateShaders(const std::string& ShaderDirectory)
{
    RefCntAutoPtr<IShaderSourceInputStreamFactory> SourceFactory;
    Device.GetEngineFactory()->CreateDefaultShaderSourceStreamFactory(ShaderDirectory.c_str(), &SourceFactory);
    if (!SourceFactory)
    {
        PrintString("Renderer: failed to create shader source factory");
        return false;
    }

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc.UseCombinedTextureSamplers = true;
    ShaderCI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;
    ShaderCI.pShaderSourceStreamFactory = SourceFactory;
    ShaderCI.FilePath = "TestMesh.hlsl";

    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint = "VSMain";
        ShaderCI.Desc.Name = "TestMesh VS";
        Device.GetDevice()->CreateShader(ShaderCI, &Pipeline->VertexShader);
    }

    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "PSMain";
        ShaderCI.Desc.Name = "TestMesh PS";
        Device.GetDevice()->CreateShader(ShaderCI, &Pipeline->PixelShader);
    }

    return Pipeline->VertexShader && Pipeline->PixelShader;
}

bool Renderer::CreatePipeline()
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name = "TestMesh PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = Device.GetSwapChain()->GetDesc().ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat = Device.GetSwapChain()->GetDesc().DepthBufferFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

    LayoutElement LayoutElems[] = {
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        LayoutElement{1, 0, 4, VT_FLOAT32, False},
    };
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

    PSOCreateInfo.pVS = Pipeline->VertexShader;
    PSOCreateInfo.pPS = Pipeline->PixelShader;
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    Device.GetDevice()->CreateGraphicsPipelineState(PSOCreateInfo, &Pipeline->Pipeline);
    if (!Pipeline->Pipeline)
    {
        return false;
    }

    return true;
}

bool Renderer::CreateConstantBuffers()
{
    if (!FrameConstantBuffer.Create(Device.GetDevice(), GpuBufferUsage::Uniform, nullptr, sizeof(FrameConstants), "Frame CB", true))
    {
        return false;
    }

    if (!ObjectConstantBuffer.Create(Device.GetDevice(), GpuBufferUsage::Uniform, nullptr, sizeof(ObjectConstants), "Object CB", true))
    {
        return false;
    }

    Pipeline->Pipeline->GetStaticVariableByName(SHADER_TYPE_VERTEX, "FrameConstants")->Set(FrameConstantBuffer.GetBuffer());
    Pipeline->Pipeline->GetStaticVariableByName(SHADER_TYPE_VERTEX, "ObjectConstants")->Set(ObjectConstantBuffer.GetBuffer());
    Pipeline->Pipeline->CreateShaderResourceBinding(&Pipeline->ResourceBinding, true);
    return Pipeline->ResourceBinding != nullptr;
}

void Renderer::CreateDefaultMesh()
{
    DefaultMesh = Resources.CreateDefaultQuadMesh();
}

void Renderer::Render(const RenderFrameData& Frame)
{
    AssertRenderThread();
    if (!bInitialized)
    {
        return;
    }

    ISwapChain* SwapChain = Device.GetSwapChain();
    IDeviceContext* Context = Device.GetImmediateContext();
    ITextureView* RenderTarget = SwapChain->GetCurrentBackBufferRTV();
    ITextureView* DepthStencil = SwapChain->GetDepthBufferDSV();

    const float ClearColor[] = {0.15f, 0.15f, 0.18f, 1.0f};
    Context->SetRenderTargets(1, &RenderTarget, DepthStencil, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Context->ClearRenderTarget(RenderTarget, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Context->ClearDepthStencil(DepthStencil, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    FrameConstants FrameData{};
    if (Frame.Scene.Camera.bValid)
    {
        FrameData.ViewProjection = Frame.Scene.Camera.ViewProjection;
    }
    else
    {
        FrameData.ViewProjection = Matrix::Identity;
    }

    {
        MapHelper<FrameConstants> Mapped(Context, FrameConstantBuffer.GetBuffer(), MAP_WRITE, MAP_FLAG_DISCARD);
        *Mapped = FrameData;
    }

    Context->SetPipelineState(Pipeline->Pipeline);

    const RenderMesh* FallbackMesh = Resources.GetMesh(DefaultMesh);

    for (const RenderObject& Object : Frame.Scene.Objects)
    {
        if (!Object.bVisible)
        {
            continue;
        }

        const RenderMesh* Mesh = Resources.GetMesh(Object.Mesh);
        if (Mesh == nullptr)
        {
            Mesh = FallbackMesh;
        }

        if (Mesh == nullptr)
        {
            continue;
        }

        ObjectConstants ObjectData{};
        ObjectData.World = Object.WorldMatrix;
        {
            MapHelper<ObjectConstants> Mapped(Context, ObjectConstantBuffer.GetBuffer(), MAP_WRITE, MAP_FLAG_DISCARD);
            *Mapped = ObjectData;
        }

        IBuffer* VertexBuffers[] = {Mesh->GetVertexBuffer().GetBuffer()};
        Uint64 Offsets[] = {0};
        Context->SetVertexBuffers(0, 1, VertexBuffers, Offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
        Context->SetIndexBuffer(Mesh->GetIndexBuffer().GetBuffer(), 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        Context->CommitShaderResources(Pipeline->ResourceBinding, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawIndexedAttribs DrawAttrs;
        DrawAttrs.IndexType = VT_UINT32;
        DrawAttrs.NumIndices = Mesh->GetIndexCount();
        Context->DrawIndexed(DrawAttrs);
    }

    Context->Flush();
    SwapChain->Present();
}

void Renderer::Resize(uint32_t Width, uint32_t Height)
{
    AssertRenderThread();
    Device.Resize(Width, Height);
}

void Renderer::Shutdown()
{
    AssertRenderThread();
    if (!bInitialized && Pipeline == nullptr)
    {
        Device.Shutdown();
        return;
    }

    Resources.Shutdown();
    if (Pipeline != nullptr)
    {
        Pipeline->ResourceBinding.Release();
        Pipeline->Pipeline.Release();
        Pipeline->PixelShader.Release();
        Pipeline->VertexShader.Release();
        delete Pipeline;
        Pipeline = nullptr;
    }

    Device.Shutdown();
    bInitialized = false;
    PrintString("Renderer: shutdown");
}
