#include "Rendering/RHI/Renderer.h"
#include "Rendering/RHI/EnvironmentLighting.h"
#include "Core/Threading/RenderFrameData.h"
#include "Core/Threading/ThreadContext.h"
#include "Rendering/Visibility.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"
#include "PipelineState.h"
#include "Shader.h"
#include "ShaderResourceBinding.h"
#include "TextureView.h"
#include "EngineFactory.h"
#include "Sampler.h"
#include "Query.h"
#include "Fence.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

using namespace Diligent;

struct ForwardLightConstants
{
    Vector4 PositionType;
    Vector4 DirectionRange;
    Vector4 ColorIntensity;
    Vector4 ConeShadow;
};

struct ForwardFrameConstants
{
    Matrix ViewProjection = Matrix::Identity;
    Vector4 CameraPosition;
    Vector4 FrameParameters;
    Vector4 OutputParameters;
    ForwardLightConstants Lights[32]{};
    Matrix ShadowMatrices[16]{};
};

struct ForwardObjectConstants
{
    Matrix World;
    Matrix NormalTransform;
    Color BaseColor;
    Vector4 MaterialParameters;
    Vector4 EmissiveAlphaMode;
};

struct PostprocessConstants
{
    Vector4 Parameters;
    Vector4 Resolution;
};

static_assert(sizeof(ForwardLightConstants) == 64);
static_assert(sizeof(ForwardFrameConstants) == 3184);
static_assert(sizeof(ForwardObjectConstants) == 176);
static_assert(sizeof(PostprocessConstants) == 32);

struct ForwardPipeline
{
    RefCntAutoPtr<IPipelineState> State;
    RefCntAutoPtr<IShaderResourceBinding> Binding;
};

struct Renderer::PipelineState
{
    struct QueryFrame
    {
        std::array<RefCntAutoPtr<IQuery>, 4> Queries;
        bool bPending = false;
        uint64_t FrameIndex = 0;
    };
    struct SurfaceState
    {
        RefCntAutoPtr<ITexture> Radiance;
        RefCntAutoPtr<ITexture> Depth;
        RefCntAutoPtr<ITexture> Accumulation;
        RefCntAutoPtr<ITexture> Revealage;
        RefCntAutoPtr<ITexture> ToneMapped;
        RefCntAutoPtr<ITexture> ShadowAtlas;
        std::array<QueryFrame, 4> Timings;
        RenderStatistics Statistics;
        uint32_t Width = 0;
        uint32_t Height = 0;
        bool bSuspended = false;
    };
    ForwardPipeline Opaque[2];
    ForwardPipeline Transparent[2];
    ForwardPipeline Shadow[2];
    ForwardPipeline ToneMap;
    ForwardPipeline Antialias;
    GpuBuffer FrameBuffer;
    GpuBuffer ObjectBuffer;
    GpuBuffer PostBuffer;
    EnvironmentLighting Environment;
    RefCntAutoPtr<ITexture> WhiteTexture;
    RefCntAutoPtr<IFence> Completion;
    std::unordered_map<uint32_t, SurfaceState> Surfaces;
};

static RefCntAutoPtr<ITexture> CreateTarget(IRenderDevice* Device, const char* Name, uint32_t Width, uint32_t Height,
    TEXTURE_FORMAT Format, Diligent::BIND_FLAGS Bindings)
{
    TextureDesc Description;
    Description.Name = Name;
    Description.Type = RESOURCE_DIM_TEX_2D;
    Description.Width = Width;
    Description.Height = Height;
    Description.Format = Format;
    Description.BindFlags = Bindings;
    RefCntAutoPtr<ITexture> Texture;
    Device->CreateTexture(Description, nullptr, &Texture);
    return Texture;
}

static void BindTexture(ForwardPipeline& Pipeline, const char* Name, ITextureView* Texture)
{
    if (auto* Variable = Pipeline.Binding->GetVariableByName(SHADER_TYPE_PIXEL, Name))
    {
        Variable->Set(Texture);
    }
}

bool Renderer::Initialize(const NativeWindowInfo& WindowInfo, GraphicsBackend BackendPreference, const std::string& ShaderDirectory)
{
    AssertRenderThread();
    Device.Initialize(WindowInfo, BackendPreference);
    if (!Device.IsInitialized())
    {
        return false;
    }
    Resources.Initialize(Device.GetDevice());
    Pipeline = new PipelineState();
    if (!CreatePipeline(ShaderDirectory) || !CreateSurfaceResources(RenderSurfaceId{1}))
    {
        PrintString("Renderer: forward initialization failed");
        Shutdown();
        return false;
    }
    DefaultMesh = Resources.CreateDefaultQuadMesh();
    bInitialized = DefaultMesh.IsValid();
    if (!bInitialized)
    {
        Shutdown();
        return false;
    }
    PrintString("Renderer: Forward PBR, shadow atlas, weighted transparency and HDR ready");
    return true;
}

bool Renderer::CreatePipeline(const std::string& ShaderDirectory)
{
    auto* GraphicsDevice = Device.GetDevice();
    if (!Pipeline->FrameBuffer.Create(GraphicsDevice, GpuBufferUsage::Uniform, nullptr, sizeof(ForwardFrameConstants), "Forward frame", true)
        || !Pipeline->ObjectBuffer.Create(GraphicsDevice, GpuBufferUsage::Uniform, nullptr, sizeof(ForwardObjectConstants), "Forward object", true)
        || !Pipeline->PostBuffer.Create(GraphicsDevice, GpuBufferUsage::Uniform, nullptr, sizeof(PostprocessConstants), "Postprocess", true)
        || !Pipeline->Environment.Initialize(GraphicsDevice))
    {
        return false;
    }
    FenceDesc FenceDescription;
    FenceDescription.Name = "Frame completion";
    GraphicsDevice->CreateFence(FenceDescription, &Pipeline->Completion);
    if (!Pipeline->Completion)
    {
        return false;
    }
    uint32_t White = 0xffffffff;
    TextureDesc WhiteDescription;
    WhiteDescription.Name = "White fallback";
    WhiteDescription.Type = RESOURCE_DIM_TEX_2D;
    WhiteDescription.Width = 1;
    WhiteDescription.Height = 1;
    WhiteDescription.Format = TEX_FORMAT_RGBA8_UNORM;
    WhiteDescription.BindFlags = BIND_SHADER_RESOURCE;
    WhiteDescription.Usage = USAGE_IMMUTABLE;
    TextureSubResData WhiteSubresource(&White, sizeof(White));
    TextureData WhiteData(&WhiteSubresource, 1);
    GraphicsDevice->CreateTexture(WhiteDescription, &WhiteData, &Pipeline->WhiteTexture);
    if (!Pipeline->WhiteTexture)
    {
        return false;
    }
    RefCntAutoPtr<IShaderSourceInputStreamFactory> SourceFactory;
    Device.GetEngineFactory()->CreateDefaultShaderSourceStreamFactory(ShaderDirectory.c_str(), &SourceFactory);
    auto CreateShader = [&](const char* File, const char* Entry, SHADER_TYPE Stage)
    {
        ShaderCreateInfo Description;
        Description.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        Description.Desc.UseCombinedTextureSamplers = true;
        Description.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;
        Description.pShaderSourceStreamFactory = SourceFactory;
        Description.FilePath = File;
        Description.EntryPoint = Entry;
        Description.Desc.Name = Entry;
        Description.Desc.ShaderType = Stage;
        RefCntAutoPtr<IShader> Shader;
        GraphicsDevice->CreateShader(Description, &Shader);
        return Shader;
    };
    const auto Vertex = CreateShader("Forward.hlsl", "VSMain", SHADER_TYPE_VERTEX);
    const auto OpaquePixel = CreateShader("Forward.hlsl", "PSMain", SHADER_TYPE_PIXEL);
    const auto TransparentPixel = CreateShader("Forward.hlsl", "TransparencyMain", SHADER_TYPE_PIXEL);
    const auto ShadowPixel = CreateShader("Forward.hlsl", "ShadowMain", SHADER_TYPE_PIXEL);
    const auto FullscreenVertex = CreateShader("Postprocess.hlsl", "FullscreenMain", SHADER_TYPE_VERTEX);
    const auto TonePixel = CreateShader("Postprocess.hlsl", "ToneMapMain", SHADER_TYPE_PIXEL);
    const auto AntialiasPixel = CreateShader("Postprocess.hlsl", "AntialiasMain", SHADER_TYPE_PIXEL);
    if (!Vertex || !OpaquePixel || !TransparentPixel || !ShadowPixel || !FullscreenVertex || !TonePixel || !AntialiasPixel)
    {
        return false;
    }
    const LayoutElement Layout[] = {
        LayoutElement{0, 0, 3, VT_FLOAT32, False}, LayoutElement{1, 0, 3, VT_FLOAT32, False},
        LayoutElement{2, 0, 2, VT_FLOAT32, False}, LayoutElement{3, 0, 4, VT_FLOAT32, False}};
    auto CreateState = [&](ForwardPipeline& Target, IShader* Pixel, uint32_t Kind, bool bDoubleSided)
    {
        GraphicsPipelineStateCreateInfo Description;
        Description.PSODesc.Name = "Forward pipeline";
        Description.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
        Description.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
        auto& Graphics = Description.GraphicsPipeline;
        Graphics.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        Graphics.RasterizerDesc.CullMode = CULL_MODE_BACK;
        Graphics.RasterizerDesc.FrontCounterClockwise = True;
        if (bDoubleSided || Kind >= 3)
        {
            Graphics.RasterizerDesc.CullMode = CULL_MODE_NONE;
        }
        Graphics.DepthStencilDesc.DepthEnable = Kind < 3;
        Graphics.DepthStencilDesc.DepthWriteEnable = Kind != 1 && Kind < 3;
        Graphics.NumRenderTargets = 1;
        Graphics.RTVFormats[0] = TEX_FORMAT_RGBA16_FLOAT;
        Graphics.DSVFormat = TEX_FORMAT_D32_FLOAT;
        Description.pVS = Vertex;
        Description.pPS = Pixel;
        Graphics.InputLayout.LayoutElements = Layout;
        Graphics.InputLayout.NumElements = 4;
        if (Kind == 1)
        {
            Graphics.NumRenderTargets = 2;
            Graphics.RTVFormats[1] = TEX_FORMAT_R16_FLOAT;
            Graphics.BlendDesc.IndependentBlendEnable = True;
            auto& Accumulation = Graphics.BlendDesc.RenderTargets[0];
            Accumulation.BlendEnable = True;
            Accumulation.SrcBlend = BLEND_FACTOR_ONE;
            Accumulation.DestBlend = BLEND_FACTOR_ONE;
            Accumulation.SrcBlendAlpha = BLEND_FACTOR_ONE;
            Accumulation.DestBlendAlpha = BLEND_FACTOR_ONE;
            auto& Revealage = Graphics.BlendDesc.RenderTargets[1];
            Revealage.BlendEnable = True;
            Revealage.SrcBlend = BLEND_FACTOR_ZERO;
            Revealage.DestBlend = BLEND_FACTOR_INV_SRC_COLOR;
            Revealage.SrcBlendAlpha = BLEND_FACTOR_ZERO;
            Revealage.DestBlendAlpha = BLEND_FACTOR_ONE;
        }
        if (Kind == 2)
        {
            Graphics.NumRenderTargets = 0;
            Graphics.RTVFormats[0] = TEX_FORMAT_UNKNOWN;
            Graphics.RasterizerDesc.DepthBias = 100;
            Graphics.RasterizerDesc.SlopeScaledDepthBias = 1.f;
        }
        if (Kind >= 3)
        {
            Description.pVS = FullscreenVertex;
            Graphics.InputLayout = InputLayoutDesc{};
            Graphics.DSVFormat = TEX_FORMAT_UNKNOWN;
        }
        if (Kind == 4)
        {
            Graphics.RTVFormats[0] = Device.GetSwapChain()->GetDesc().ColorBufferFormat;
        }
        SamplerDesc Linear;
        Linear.MinFilter = FILTER_TYPE_LINEAR;
        Linear.MagFilter = FILTER_TYPE_LINEAR;
        Linear.MipFilter = FILTER_TYPE_LINEAR;
        Linear.AddressU = TEXTURE_ADDRESS_CLAMP;
        Linear.AddressV = TEXTURE_ADDRESS_CLAMP;
        Linear.AddressW = TEXTURE_ADDRESS_CLAMP;
        SamplerDesc Wrap = Linear;
        Wrap.AddressU = TEXTURE_ADDRESS_WRAP;
        Wrap.AddressV = TEXTURE_ADDRESS_WRAP;
        SamplerDesc Point = Linear;
        Point.MinFilter = FILTER_TYPE_POINT;
        Point.MagFilter = FILTER_TYPE_POINT;
        Point.MipFilter = FILTER_TYPE_POINT;
        std::vector<ImmutableSamplerDesc> Samplers;
        if (Kind < 3)
        {
            Samplers.emplace_back(SHADER_TYPE_PIXEL, "BaseColorImage", Wrap);
            if (Kind != 2)
            {
                Samplers.emplace_back(SHADER_TYPE_PIXEL, "ShadowAtlas", Point);
                Samplers.emplace_back(SHADER_TYPE_PIXEL, "IrradianceImage", Linear);
                Samplers.emplace_back(SHADER_TYPE_PIXEL, "ReflectionImage", Linear);
                Samplers.emplace_back(SHADER_TYPE_PIXEL, "BrdfImage", Linear);
            }
        }
        else
        {
            Samplers.emplace_back(SHADER_TYPE_PIXEL, "SceneImage", Linear);
            if (Kind == 3)
            {
                Samplers.emplace_back(SHADER_TYPE_PIXEL, "AccumulationImage", Linear);
                Samplers.emplace_back(SHADER_TYPE_PIXEL, "RevealageImage", Linear);
            }
        }
        Description.PSODesc.ResourceLayout.ImmutableSamplers = Samplers.data();
        Description.PSODesc.ResourceLayout.NumImmutableSamplers = static_cast<uint32_t>(Samplers.size());
        GraphicsDevice->CreateGraphicsPipelineState(Description, &Target.State);
        if (!Target.State)
        {
            return false;
        }
        Target.State->CreateShaderResourceBinding(&Target.Binding, true);
        if (!Target.Binding)
        {
            return false;
        }
        for (SHADER_TYPE Stage : {SHADER_TYPE_VERTEX, SHADER_TYPE_PIXEL})
        {
            if (auto* Variable = Target.Binding->GetVariableByName(Stage, "FrameConstants"))
            {
                Variable->Set(Pipeline->FrameBuffer.GetBuffer());
            }
            if (auto* Variable = Target.Binding->GetVariableByName(Stage, "ObjectConstants"))
            {
                Variable->Set(Pipeline->ObjectBuffer.GetBuffer());
            }
            if (auto* Variable = Target.Binding->GetVariableByName(Stage, "PostConstants"))
            {
                Variable->Set(Pipeline->PostBuffer.GetBuffer());
            }
        }
        if (Kind < 2)
        {
            BindTexture(Target, "IrradianceImage", Pipeline->Environment.Irradiance->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
            BindTexture(Target, "ReflectionImage", Pipeline->Environment.Reflection->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
            BindTexture(Target, "BrdfImage", Pipeline->Environment.IntegratedBrdf->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        }
        return true;
    };
    for (uint32_t SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        if (!CreateState(Pipeline->Opaque[SideIndex], OpaquePixel, 0, SideIndex != 0)
            || !CreateState(Pipeline->Transparent[SideIndex], TransparentPixel, 1, SideIndex != 0)
            || !CreateState(Pipeline->Shadow[SideIndex], ShadowPixel, 2, SideIndex != 0))
        {
            return false;
        }
    }
    return CreateState(Pipeline->ToneMap, TonePixel, 3, true) && CreateState(Pipeline->Antialias, AntialiasPixel, 4, true);
}

bool Renderer::CreateSurfaceResources(RenderSurfaceId Surface)
{
    auto* SwapChain = Device.GetSurface(Surface);
    if (SwapChain == nullptr)
    {
        return false;
    }
    auto& Target = Pipeline->Surfaces[Surface.Value];
    Target.Width = SwapChain->GetDesc().Width;
    Target.Height = SwapChain->GetDesc().Height;
    const auto ColorBindings = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
    Target.Radiance = CreateTarget(Device.GetDevice(), "HDR scene", Target.Width, Target.Height, TEX_FORMAT_RGBA16_FLOAT, ColorBindings);
    Target.Accumulation = CreateTarget(Device.GetDevice(), "Transparency accumulation", Target.Width, Target.Height, TEX_FORMAT_RGBA16_FLOAT, ColorBindings);
    Target.Revealage = CreateTarget(Device.GetDevice(), "Transparency revealage", Target.Width, Target.Height, TEX_FORMAT_R16_FLOAT, ColorBindings);
    Target.ToneMapped = CreateTarget(Device.GetDevice(), "Tone mapped scene", Target.Width, Target.Height, TEX_FORMAT_RGBA16_FLOAT, ColorBindings);
    Target.Depth = CreateTarget(Device.GetDevice(), "Scene depth", Target.Width, Target.Height, TEX_FORMAT_D32_FLOAT, BIND_DEPTH_STENCIL);
    if (!Target.ShadowAtlas)
    {
        Target.ShadowAtlas = CreateTarget(Device.GetDevice(), "Shadow atlas", 4096, 4096, TEX_FORMAT_D32_FLOAT, BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE);
        if (Device.GetDevice()->GetDeviceInfo().Features.DurationQueries == DEVICE_FEATURE_STATE_ENABLED)
        {
            for (auto& Timing : Target.Timings)
            {
                for (auto& Query : Timing.Queries)
                {
                    QueryDesc Description;
                    Description.Name = "Forward pass duration";
                    Description.Type = QUERY_TYPE_DURATION;
                    Device.GetDevice()->CreateQuery(Description, &Query);
                }
            }
        }
    }
    return Target.Radiance && Target.Depth && Target.Accumulation && Target.Revealage && Target.ToneMapped && Target.ShadowAtlas;
}

void Renderer::Render(const RenderFrameData& Frame)
{
    AssertRenderThread();
    if (!bInitialized)
    {
        return;
    }
    if (Frame.Views.empty())
    {
        RenderViewFrame View;
        View.Scene = Frame.Scene;
        RenderView(View, Frame.FrameIndex);
    }
    else
    {
        for (const auto& View : Frame.Views)
        {
            RenderView(View, Frame.FrameIndex);
        }
    }
    auto* Context = Device.GetImmediateContext();
    Context->EnqueueSignal(Pipeline->Completion, Frame.FrameIndex + 1);
    Context->Flush();
    for (uint32_t SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        ITextureView* White = Pipeline->WhiteTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        BindTexture(Pipeline->Opaque[SideIndex], "BaseColorImage", White);
        BindTexture(Pipeline->Transparent[SideIndex], "BaseColorImage", White);
        BindTexture(Pipeline->Shadow[SideIndex], "BaseColorImage", White);
    }
    Context->FinishFrame();
    Resources.CollectRetired(Pipeline->Completion->GetCompletedValue());
    Device.GetDevice()->ReleaseStaleResources();
}

void Renderer::RenderView(const RenderViewFrame& View, uint64_t FrameIndex)
{
    const auto Iterator = Pipeline->Surfaces.find(View.Surface.Value);
    auto* SwapChain = Device.GetSurface(View.Surface);
    if (Iterator == Pipeline->Surfaces.end() || SwapChain == nullptr || Iterator->second.bSuspended)
    {
        return;
    }
    const auto CpuStart = std::chrono::steady_clock::now();
    auto& Target = Iterator->second;
    auto* Context = Device.GetImmediateContext();
    RenderStatistics Statistics;
    Statistics.FrameIndex = FrameIndex;
    Statistics.SubmittedObjects = static_cast<uint32_t>(View.Scene.Objects.size());
    PipelineState::QueryFrame* ActiveTiming = nullptr;
    for (auto& Timing : Target.Timings)
    {
        if (Timing.bPending)
        {
            std::array<QueryDataDuration, 4> Results;
            bool bReady = true;
            for (uint32_t PassIndex = 0; PassIndex < 4; ++PassIndex)
            {
                bReady = Timing.Queries[PassIndex]->GetData(&Results[PassIndex], sizeof(QueryDataDuration), false) && bReady;
            }
            if (bReady)
            {
                if (Timing.FrameIndex >= Target.Statistics.GpuFrameIndex)
                {
                    double* Durations[] = {&Target.Statistics.ShadowMilliseconds, &Target.Statistics.OpaqueMilliseconds,
                        &Target.Statistics.TransparencyMilliseconds, &Target.Statistics.PostprocessMilliseconds};
                    bool bValid = true;
                    for (uint32_t PassIndex = 0; PassIndex < 4; ++PassIndex)
                    {
                        if (Results[PassIndex].Frequency > 0)
                        {
                            *Durations[PassIndex] = static_cast<double>(Results[PassIndex].Duration) * 1000.0 / Results[PassIndex].Frequency;
                        }
                        else
                        {
                            bValid = false;
                        }
                    }
                    Target.Statistics.bGpuTimingsAvailable = bValid;
                    Target.Statistics.GpuFrameIndex = Timing.FrameIndex;
                }
                for (auto& Query : Timing.Queries)
                {
                    Query->Invalidate();
                }
                Timing.bPending = false;
            }
        }
        if (!Timing.bPending && Timing.Queries[0] && Timing.Queries[1] && Timing.Queries[2] && Timing.Queries[3] && ActiveTiming == nullptr)
        {
            ActiveTiming = &Timing;
        }
    }
    Statistics.GpuFrameIndex = Target.Statistics.GpuFrameIndex;
    Statistics.ShadowMilliseconds = Target.Statistics.ShadowMilliseconds;
    Statistics.OpaqueMilliseconds = Target.Statistics.OpaqueMilliseconds;
    Statistics.TransparencyMilliseconds = Target.Statistics.TransparencyMilliseconds;
    Statistics.PostprocessMilliseconds = Target.Statistics.PostprocessMilliseconds;
    Statistics.bGpuTimingsAvailable = Target.Statistics.bGpuTimingsAvailable;
    auto BeginPass = [&](uint32_t PassIndex)
    {
        if (ActiveTiming != nullptr)
        {
            Context->BeginQuery(ActiveTiming->Queries[PassIndex]);
        }
    };
    auto EndPass = [&](uint32_t PassIndex)
    {
        if (ActiveTiming != nullptr)
        {
            Context->EndQuery(ActiveTiming->Queries[PassIndex]);
        }
    };
    ForwardFrameConstants Frame;
    Frame.ViewProjection = View.Scene.Camera.ViewProjection;
    Frame.CameraPosition = Vector4(View.Scene.Camera.Position.x, View.Scene.Camera.Position.y, View.Scene.Camera.Position.z, 1.f);
    Frame.FrameParameters.y = std::max(0.f, View.Settings.EnvironmentIntensity);
    Frame.OutputParameters = Vector4(0.f, -1.f, 0.f, 0.f);
    if (Device.GetActiveBackend() == GraphicsBackend::OpenGL)
    {
        Frame.OutputParameters = Vector4(1.f, 1.f, 0.f, 0.f);
    }
    uint32_t ShadowCount = 0;
    for (const auto& Light : View.Scene.Lights)
    {
        if (Light.Type != RenderLightType::Directional && View.Scene.Camera.bValid
            && !IntersectsFrustum(AxisAlignedBounds::FromCenterExtents(Light.Position, Vector3(Light.Range)), Frame.ViewProjection))
        {
            continue;
        }
        if (Statistics.LightCount == 32)
        {
            ++Statistics.DroppedLights;
            continue;
        }
        auto& Output = Frame.Lights[Statistics.LightCount++];
        Vector3 Direction = Light.Direction;
        Direction.Normalize();
        Output.PositionType = Vector4(Light.Position.x, Light.Position.y, Light.Position.z, static_cast<float>(Light.Type));
        Output.DirectionRange = Vector4(Direction.x, Direction.y, Direction.z, std::max(0.01f, Light.Range));
        Output.ColorIntensity = Vector4(Light.LightColor.x, Light.LightColor.y, Light.LightColor.z, std::max(0.f, Light.Intensity));
        const float OuterDegrees = std::clamp(Light.OuterConeAngle, 1.f, 89.f);
        const float OuterAngle = OuterDegrees * 0.0174532925f;
        const float InnerAngle = std::clamp(Light.InnerConeAngle, 0.f, OuterDegrees - 0.01f) * 0.0174532925f;
        Output.ConeShadow = Vector4(std::cos(InnerAngle), std::cos(OuterAngle), -1.f, 0.f);
        uint32_t FaceCount = 1;
        if (Light.Type == RenderLightType::Point)
        {
            FaceCount = 6;
        }
        if (!Light.bCastShadows)
        {
            continue;
        }
        if (ShadowCount + FaceCount > 16)
        {
            ++Statistics.DroppedShadowLights;
            continue;
        }
        Output.ConeShadow.z = static_cast<float>(ShadowCount);
        for (uint32_t FaceIndex = 0; FaceIndex < FaceCount; ++FaceIndex)
        {
            Vector3 Position = Light.Position;
            Vector3 Up = Vector3::Up;
            Matrix Projection;
            if (Light.Type == RenderLightType::Directional)
            {
                const float Distance = std::max(1.f, View.Settings.ShadowDistance);
                Vector3 CameraForward = Vector3::TransformNormal(Vector3::Forward, View.Scene.Camera.View.Invert());
                CameraForward.Normalize();
                const Vector3 Center = View.Scene.Camera.Position + CameraForward * (Distance * 0.4f);
                Position = Center - Direction * Distance;
                Projection = Matrix::CreateOrthographic(Distance, Distance, 0.1f, Distance * 2.f);
            }
            else if (Light.Type == RenderLightType::Point)
            {
                const Vector3 Directions[] = {Vector3::Right, Vector3::Left, Vector3::Up, Vector3::Down, Vector3::Backward, Vector3::Forward};
                Direction = Directions[FaceIndex];
                Projection = Matrix::CreatePerspectiveFieldOfView(1.5707963268f, 1.f, 0.05f, std::max(0.1f, Light.Range));
            }
            else
            {
                Projection = Matrix::CreatePerspectiveFieldOfView(OuterAngle * 2.f, 1.f, 0.05f, std::max(0.1f, Light.Range));
            }
            if (std::abs(Direction.Dot(Up)) > 0.99f)
            {
                Up = Vector3::Backward;
            }
            Frame.ShadowMatrices[ShadowCount++] = Matrix::CreateLookAt(Position, Position + Direction, Up) * Projection;
        }
    }
    Statistics.ShadowViews = ShadowCount;
    Frame.FrameParameters.x = static_cast<float>(Statistics.LightCount);
    std::vector<const RenderObject*> Visible;
    for (const auto& Object : View.Scene.Objects)
    {
        if (!Object.bVisible)
        {
            continue;
        }
        if (View.Scene.Camera.bValid && !IntersectsFrustum(Object.Bounds, Frame.ViewProjection))
        {
            ++Statistics.CulledObjects;
            continue;
        }
        Visible.push_back(&Object);
    }
    Statistics.VisibleObjects = static_cast<uint32_t>(Visible.size());
    std::stable_sort(Visible.begin(), Visible.end(), [&](const RenderObject* First, const RenderObject* Second)
    {
        return Vector3::DistanceSquared((First->Bounds.Minimum + First->Bounds.Maximum) * 0.5f, View.Scene.Camera.Position)
            < Vector3::DistanceSquared((Second->Bounds.Minimum + Second->Bounds.Maximum) * 0.5f, View.Scene.Camera.Position);
    });
    auto DrawObject = [&](const RenderObject& Object, ForwardPipeline& Selected)
    {
        const RenderMesh* Mesh = Resources.GetMesh(Object.Mesh);
        bool bPlaceholder = false;
        if (Mesh == nullptr || Object.bUsePlaceholder || Object.bMissingAsset)
        {
            Mesh = Resources.GetMesh(DefaultMesh);
            bPlaceholder = true;
        }
        if (Mesh == nullptr)
        {
            return;
        }
        ForwardObjectConstants Constants;
        Constants.World = Object.WorldMatrix;
        Constants.NormalTransform = Object.WorldMatrix.Invert().Transpose();
        Constants.BaseColor = Object.BaseColor;
        if (bPlaceholder)
        {
            Constants.BaseColor = Color(1.f, 0.35f, 0.75f, 1.f);
        }
        const auto& Material = Object.SurfaceMaterial;
        Constants.MaterialParameters = Vector4(Material.Metallic, Material.Roughness, Material.AlphaCutoff, 0.f);
        Constants.EmissiveAlphaMode = Vector4(Material.Emissive.x, Material.Emissive.y, Material.Emissive.z, static_cast<float>(Material.AlphaMode));
        ITextureView* Texture = Resources.GetTextureShaderView(Object.BaseColorTexture);
        if (Texture != nullptr && !bPlaceholder)
        {
            Constants.MaterialParameters.w = 1.f;
        }
        else
        {
            Texture = Pipeline->WhiteTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        }
        BindTexture(Selected, "BaseColorImage", Texture);
        Pipeline->ObjectBuffer.Update(Context, &Constants, sizeof(Constants));
        Context->SetPipelineState(Selected.State);
        IBuffer* Buffers[] = {Mesh->GetVertexBuffer().GetBuffer()};
        Uint64 Offsets[] = {0};
        Context->SetVertexBuffers(0, 1, Buffers, Offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
        Context->SetIndexBuffer(Mesh->GetIndexBuffer().GetBuffer(), 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        Context->CommitShaderResources(Selected.Binding, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        DrawIndexedAttribs Draw;
        Draw.IndexType = VT_UINT32;
        Draw.NumIndices = Mesh->GetIndexCount();
        Draw.Flags = DRAW_FLAG_VERIFY_ALL;
        Context->DrawIndexed(Draw);
        ++Statistics.DrawCalls;
        Statistics.Triangles += Draw.NumIndices / 3;
    };
    BeginPass(0);
    ITextureView* ShadowDepth = Target.ShadowAtlas->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
    Context->SetRenderTargets(0, nullptr, ShadowDepth, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Context->ClearDepthStencil(ShadowDepth, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    const Matrix CameraProjection = Frame.ViewProjection;
    for (uint32_t ShadowIndex = 0; ShadowIndex < ShadowCount; ++ShadowIndex)
    {
        Frame.ViewProjection = Frame.ShadowMatrices[ShadowIndex];
        Pipeline->FrameBuffer.Update(Context, &Frame, sizeof(Frame));
        Diligent::Viewport ShadowViewport(static_cast<float>((ShadowIndex % 4) * 1024), static_cast<float>((ShadowIndex / 4) * 1024), 1024.f, 1024.f);
        Context->SetViewports(1, &ShadowViewport, 4096, 4096);
        for (const auto& Object : View.Scene.Objects)
        {
            if (Object.bVisible && Object.SurfaceMaterial.bCastShadows && Object.SurfaceMaterial.AlphaMode != MaterialAlphaMode::Blend
                && IntersectsFrustum(Object.Bounds, Frame.ViewProjection))
            {
                DrawObject(Object, Pipeline->Shadow[Object.SurfaceMaterial.bDoubleSided]);
            }
        }
    }
    Frame.ViewProjection = CameraProjection;
    Pipeline->FrameBuffer.Update(Context, &Frame, sizeof(Frame));
    EndPass(0);
    for (uint32_t SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        BindTexture(Pipeline->Opaque[SideIndex], "ShadowAtlas", Target.ShadowAtlas->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        BindTexture(Pipeline->Transparent[SideIndex], "ShadowAtlas", Target.ShadowAtlas->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    }
    BeginPass(1);
    ITextureView* Radiance = Target.Radiance->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
    ITextureView* Depth = Target.Depth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
    Context->SetRenderTargets(1, &Radiance, Depth, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Diligent::Viewport FullViewport(0.f, 0.f, static_cast<float>(Target.Width), static_cast<float>(Target.Height));
    Context->SetViewports(1, &FullViewport, Target.Width, Target.Height);
    const float Background[] = {0.025f, 0.03f, 0.04f, 1.f};
    Context->ClearRenderTarget(Radiance, Background, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Context->ClearDepthStencil(Depth, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    for (const RenderObject* Object : Visible)
    {
        if (Object->SurfaceMaterial.AlphaMode != MaterialAlphaMode::Blend)
        {
            DrawObject(*Object, Pipeline->Opaque[Object->SurfaceMaterial.bDoubleSided]);
        }
    }
    EndPass(1);
    BeginPass(2);
    ITextureView* TransparencyTargets[] = {Target.Accumulation->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET), Target.Revealage->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET)};
    Context->SetRenderTargets(2, TransparencyTargets, Depth, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    const float Zero[] = {0.f, 0.f, 0.f, 0.f};
    const float One[] = {1.f, 1.f, 1.f, 1.f};
    Context->ClearRenderTarget(TransparencyTargets[0], Zero, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Context->ClearRenderTarget(TransparencyTargets[1], One, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    for (const RenderObject* Object : Visible)
    {
        if (Object->SurfaceMaterial.AlphaMode == MaterialAlphaMode::Blend)
        {
            DrawObject(*Object, Pipeline->Transparent[Object->SurfaceMaterial.bDoubleSided]);
        }
    }
    EndPass(2);
    BeginPass(3);
    PostprocessConstants Post;
    Post.Parameters = Vector4(std::max(0.f, View.Settings.Exposure), std::max(0.f, View.Settings.BloomIntensity), std::max(0.f, View.Settings.BloomThreshold), 0.f);
    if (View.Settings.bAntialiasing)
    {
        Post.Parameters.w = 1.f;
    }
    Post.Resolution = Vector4(1.f / Target.Width, 1.f / Target.Height, Frame.OutputParameters.x, 0.f);
    Pipeline->PostBuffer.Update(Context, &Post, sizeof(Post));
    ITextureView* ToneTarget = Target.ToneMapped->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
    Context->SetRenderTargets(1, &ToneTarget, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    BindTexture(Pipeline->ToneMap, "SceneImage", Target.Radiance->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    BindTexture(Pipeline->ToneMap, "AccumulationImage", Target.Accumulation->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    BindTexture(Pipeline->ToneMap, "RevealageImage", Target.Revealage->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    Context->SetPipelineState(Pipeline->ToneMap.State);
    Context->CommitShaderResources(Pipeline->ToneMap.Binding, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawAttribs FullscreenDraw;
    FullscreenDraw.NumVertices = 3;
    FullscreenDraw.Flags = DRAW_FLAG_VERIFY_ALL;
    Context->Draw(FullscreenDraw);
    ITextureView* BackBuffer = SwapChain->GetCurrentBackBufferRTV();
    Context->SetRenderTargets(1, &BackBuffer, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    BindTexture(Pipeline->Antialias, "SceneImage", Target.ToneMapped->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    Context->SetPipelineState(Pipeline->Antialias.State);
    Context->CommitShaderResources(Pipeline->Antialias.Binding, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Context->Draw(FullscreenDraw);
    Statistics.DrawCalls += 2;
    EndPass(3);
    if (ActiveTiming != nullptr)
    {
        ActiveTiming->bPending = true;
        ActiveTiming->FrameIndex = FrameIndex;
    }
    Statistics.CpuMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - CpuStart).count();
    Target.Statistics = Statistics;
    SwapChain->Present(0);
}

bool Renderer::AddSurface(RenderSurfaceId Surface, const NativeWindowInfo& WindowInfo)
{
    AssertRenderThread();
    if (!Device.AddSurface(Surface, WindowInfo))
    {
        return false;
    }
    if (!CreateSurfaceResources(Surface))
    {
        Device.RemoveSurface(Surface);
        Pipeline->Surfaces.erase(Surface.Value);
        return false;
    }
    return true;
}

void Renderer::RemoveSurface(RenderSurfaceId Surface)
{
    AssertRenderThread();
    Device.WaitForIdle();
    Pipeline->Surfaces.erase(Surface.Value);
    Device.RemoveSurface(Surface);
}

void Renderer::ResizeSurface(RenderSurfaceId Surface, uint32_t Width, uint32_t Height)
{
    AssertRenderThread();
    auto Iterator = Pipeline->Surfaces.find(Surface.Value);
    if (Iterator == Pipeline->Surfaces.end())
    {
        return;
    }
    auto& Target = Iterator->second;
    Target.bSuspended = Width == 0 || Height == 0;
    if (Target.bSuspended || (Width == Target.Width && Height == Target.Height
        && Target.Radiance && Target.Depth && Target.Accumulation && Target.Revealage && Target.ToneMapped))
    {
        return;
    }
    Device.ResizeSurface(Surface, Width, Height);
    if (!CreateSurfaceResources(Surface))
    {
        Target.bSuspended = true;
        PrintString("Renderer: failed to recreate surface attachments");
    }
}

void Renderer::Resize(uint32_t Width, uint32_t Height)
{
    ResizeSurface(RenderSurfaceId{1}, Width, Height);
}

std::unordered_map<uint32_t, RenderStatistics> Renderer::GetStatistics() const
{
    AssertRenderThread();
    std::unordered_map<uint32_t, RenderStatistics> Result;
    if (Pipeline != nullptr)
    {
        for (const auto& Entry : Pipeline->Surfaces)
        {
            RenderStatistics Statistics = Entry.second.Statistics;
            Resources.GetStatistics(Statistics);
            Result.emplace(Entry.first, Statistics);
        }
    }
    return Result;
}

void Renderer::Shutdown()
{
    AssertRenderThread();
    Device.WaitForIdle();
    delete Pipeline;
    Pipeline = nullptr;
    Resources.Shutdown();
    Device.Shutdown();
    bInitialized = false;
}
