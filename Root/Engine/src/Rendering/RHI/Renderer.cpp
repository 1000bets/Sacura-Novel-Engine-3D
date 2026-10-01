#include "Rendering/RHI/Renderer.h"
#include "Rendering/RHI/MaterialShaderCompiler.h"
#include "Rendering/RHI/MaterialPreviewMesh.h"
#include "Rendering/RHI/EnvironmentLighting.h"
#include "Rendering/Frame/RenderFrameData.h"
#include "Core/Threading/ThreadContext.h"
#include "Rendering/Visibility.h"
#include "Rendering/RenderMaterial.h"
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
#include "DataBlob.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <functional>
#include <sstream>
#include <cstring>
#include "Materials/MaterialRenderSnapshot.h"

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

struct MaterialViewConstants
{
    Matrix InverseViewProjection = Matrix::Identity;
    Vector4 ResolutionTime;
    Vector4 WidgetColor = Vector4::One;
    Vector4 ViewOptions;
};
static_assert(sizeof(MaterialViewConstants) == 112);

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
        RefCntAutoPtr<ITexture> PostTargets[2];
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
    ForwardPipeline Composite;
    std::chrono::steady_clock::time_point StartTime = std::chrono::steady_clock::now();
    ForwardPipeline Antialias;
    ForwardPipeline PreviewAntialias;
    GpuBuffer FrameBuffer;
    GpuBuffer ObjectBuffer;
    GpuBuffer PostBuffer;
    GpuBuffer MaterialBuffer;
    GpuBuffer MaterialTextureBuffer;
    GpuBuffer MaterialViewBuffer;
    RefCntAutoPtr<IShaderSourceInputStreamFactory> SourceFactory;
    std::string MaterialContract;
    std::function<bool(ForwardPipeline&, IShader*, uint32_t, bool, IShader*, const ResolvedMaterial*, bool)> CreateState;
    std::function<void(ForwardPipeline&)> InitializeBinding;
    struct MaterialVariant
    {
        ResolvedMaterial Material;
        RefCntAutoPtr<IShader> Vertex;
        RefCntAutoPtr<IShader> Pixel;
        RefCntAutoPtr<IShader> ShadowPixel;
        RefCntAutoPtr<IDataBlob> VertexErrors;
        RefCntAutoPtr<IDataBlob> PixelErrors;
        RefCntAutoPtr<IDataBlob> ShadowErrors;
        ForwardPipeline Main;
        ForwardPipeline Shadow;
        bool bPipelineRequested = false;
        bool bReady = false;
        bool bFailed = false;
        std::string Error;
        std::shared_ptr<MaterialCompilation> Compilation;
    };
    std::unordered_map<std::string, MaterialVariant> Materials;
    struct ImageReadback
    {
        std::shared_ptr<MaterialImageRequest> Request;
        RefCntAutoPtr<ITexture> Target;
        RefCntAutoPtr<ITexture> Staging;
        uint64_t CompletionValue = 0;
    };
    std::vector<ImageReadback> Images;
    RefCntAutoPtr<IFence> ImageCompletion;
    uint64_t ImageSequence = 0;
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
    for (int32_t Shape = 0; Shape < 3; ++Shape)
    {
        PreviewMeshes[Shape] = Resources.CreateMeshFromCpuData(BuildMaterialPreviewMesh(Shape));
    }
    bInitialized = DefaultMesh.IsValid();
    if (!bInitialized)
    {
        Shutdown();
        return false;
    }

    CreateImGuiOverlay();

    PrintString("Renderer: Forward PBR, shadow atlas, weighted transparency and HDR ready");
    return true;
}

bool Renderer::CreatePipeline(const std::string& ShaderDirectory)
{
    auto* GraphicsDevice = Device.GetDevice();
    if (!Pipeline->FrameBuffer.Create(GraphicsDevice, GpuBufferUsage::Uniform, nullptr, sizeof(ForwardFrameConstants), "Forward frame", true)
        || !Pipeline->ObjectBuffer.Create(GraphicsDevice, GpuBufferUsage::Uniform, nullptr, sizeof(ForwardObjectConstants), "Forward object", true)
        || !Pipeline->PostBuffer.Create(GraphicsDevice, GpuBufferUsage::Uniform, nullptr, sizeof(PostprocessConstants), "Postprocess", true)
        || !Pipeline->MaterialBuffer.Create(GraphicsDevice, GpuBufferUsage::Uniform, nullptr, 4096, "Material parameters", true)
        || !Pipeline->MaterialTextureBuffer.Create(GraphicsDevice, GpuBufferUsage::Uniform, nullptr, 16, "Material texture availability", true)
        || !Pipeline->MaterialViewBuffer.Create(GraphicsDevice, GpuBufferUsage::Uniform, nullptr, 112, "Material view", true)
        || !Pipeline->Environment.Initialize(GraphicsDevice))
    {
        return false;
    }
    FenceDesc FenceDescription;
    FenceDescription.Name = "Frame completion";
    GraphicsDevice->CreateFence(FenceDescription, &Pipeline->Completion);
    FenceDescription.Name = "Material image readback";
    GraphicsDevice->CreateFence(FenceDescription, &Pipeline->ImageCompletion);
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
    Pipeline->SourceFactory = SourceFactory;
    std::ifstream ContractFile(std::filesystem::path(ShaderDirectory) / "MaterialContract.hlsli");
    Pipeline->MaterialContract.assign(std::istreambuf_iterator<char>(ContractFile), std::istreambuf_iterator<char>());
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
        CreateEngineShader(GraphicsDevice, Description, &Shader);
        return Shader;
    };
    const auto Vertex = CreateShader("Forward.hlsl", "VSMain", SHADER_TYPE_VERTEX);
    const auto OpaquePixel = CreateShader("Forward.hlsl", "PSMain", SHADER_TYPE_PIXEL);
    const auto TransparentPixel = CreateShader("Forward.hlsl", "TransparencyMain", SHADER_TYPE_PIXEL);
    const auto ShadowPixel = CreateShader("Forward.hlsl", "ShadowMain", SHADER_TYPE_PIXEL);
    const auto FullscreenVertex = CreateShader("Postprocess.hlsl", "FullscreenMain", SHADER_TYPE_VERTEX);
    const auto CompositePixel = CreateShader("Postprocess.hlsl", "CompositeMain", SHADER_TYPE_PIXEL);
    const auto TonePixel = CreateShader("Postprocess.hlsl", "ToneMapMain", SHADER_TYPE_PIXEL);
    const auto AntialiasPixel = CreateShader("Postprocess.hlsl", "AntialiasMain", SHADER_TYPE_PIXEL);
    const auto PreviewPixel = CreateShader("Postprocess.hlsl", "PreviewAntialiasMain", SHADER_TYPE_PIXEL);
    if (!Vertex || !OpaquePixel || !TransparentPixel || !ShadowPixel || !FullscreenVertex || !TonePixel || !CompositePixel || !AntialiasPixel)
    {
        return false;
    }
    const LayoutElement Layout[] = {
        LayoutElement{0, 0, 3, VT_FLOAT32, False}, LayoutElement{1, 0, 3, VT_FLOAT32, False},
        LayoutElement{2, 0, 2, VT_FLOAT32, False}, LayoutElement{3, 0, 4, VT_FLOAT32, False}, LayoutElement{4, 0, 4, VT_FLOAT32, False}};
    Pipeline->InitializeBinding = [this](ForwardPipeline& Target)
    {
        Target.State->CreateShaderResourceBinding(&Target.Binding, true);
        if (!Target.Binding)
        {
            return;
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
            if (auto* Variable = Target.Binding->GetVariableByName(Stage, "SakuraMaterialConstants"))
            {
                Variable->Set(Pipeline->MaterialBuffer.GetBuffer());
            }
            if (auto* Variable = Target.Binding->GetVariableByName(Stage, "SakuraMaterialTextureState"))
            {
                Variable->Set(Pipeline->MaterialTextureBuffer.GetBuffer());
            }
            if (auto* Variable = Target.Binding->GetVariableByName(Stage, "SakuraMaterialView"))
            {
                Variable->Set(Pipeline->MaterialViewBuffer.GetBuffer());
            }
            if (auto* Variable = Target.Binding->GetVariableByName(Stage, "PostConstants"))
            {
                Variable->Set(Pipeline->PostBuffer.GetBuffer());
            }
        }
        {
            BindTexture(Target, "IrradianceImage", Pipeline->Environment.Irradiance->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
            BindTexture(Target, "ReflectionImage", Pipeline->Environment.Reflection->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
            BindTexture(Target, "BrdfImage", Pipeline->Environment.IntegratedBrdf->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        }
    };
    Pipeline->CreateState = [this, GraphicsDevice, Vertex, FullscreenVertex, Layout](ForwardPipeline& Target, IShader* Pixel, uint32_t Kind, bool bDoubleSided, IShader* MaterialVertex, const ResolvedMaterial* Material, bool bAsync)
    {
        GraphicsPipelineStateCreateInfo Description;
        if (bAsync)
        {
            Description.Flags = PSO_CREATE_FLAG_ASYNCHRONOUS;
        }
        Description.PSODesc.Name = "Forward pipeline";
        Description.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
        Description.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
        auto& Graphics = Description.GraphicsPipeline;
        Graphics.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        Graphics.RasterizerDesc.CullMode = CULL_MODE_BACK;
        Graphics.RasterizerDesc.FrontCounterClockwise = True;
        if (bDoubleSided || (Kind >= 3 && Kind != 5))
        {
            Graphics.RasterizerDesc.CullMode = CULL_MODE_NONE;
        }
        Graphics.DepthStencilDesc.DepthEnable = Kind < 3 || Kind == 5;
        Graphics.DepthStencilDesc.DepthWriteEnable = Kind != 1 && Kind < 3;
        Graphics.NumRenderTargets = 1;
        Graphics.RTVFormats[0] = TEX_FORMAT_RGBA16_FLOAT;
        Graphics.DSVFormat = TEX_FORMAT_D32_FLOAT;
        Description.pVS = Vertex;
        Description.pPS = Pixel;
        Graphics.InputLayout.LayoutElements = Layout;
        Graphics.InputLayout.NumElements = 5;
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
        if (Kind >= 3 && Kind != 5)
        {
            Description.pVS = FullscreenVertex;
            Graphics.InputLayout = InputLayoutDesc{};
            Graphics.DSVFormat = TEX_FORMAT_UNKNOWN;
        }
        if (MaterialVertex != nullptr)
        {
            Description.pVS = MaterialVertex;
        }
        if (Kind == 5)
        {
            auto& Blend = Graphics.BlendDesc.RenderTargets[0];
            Blend.BlendEnable = True;
            Blend.SrcBlend = BLEND_FACTOR_ONE;
            Blend.DestBlend = BLEND_FACTOR_ONE;
            Blend.SrcBlendAlpha = BLEND_FACTOR_ZERO;
            Blend.DestBlendAlpha = BLEND_FACTOR_ONE;
        }
        if (Kind >= 6)
        {
            Graphics.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM;
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
        if (Kind < 3 || Kind == 5)
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
        std::vector<std::string> TextureNames;
        if (Material != nullptr)
        {
            for (const auto& Parameter : Material->Definition.Parameters)
            {
                if (Parameter.DefaultValue.Type == MaterialParameterType::Texture2D)
                {
                    TextureNames.push_back("SakuraTexture_" + Parameter.Identifier);
                }
            }
            size_t TextureIndex = 0;
            for (const auto& Parameter : Material->Definition.Parameters)
            {
                if (Parameter.DefaultValue.Type != MaterialParameterType::Texture2D)
                {
                    continue;
                }
                SamplerDesc Settings = Wrap;
                if (Parameter.bClamp)
                {
                    Settings = Linear;
                }
                if (Parameter.bPointFiltering)
                {
                    Settings.MinFilter = FILTER_TYPE_POINT;
                    Settings.MagFilter = FILTER_TYPE_POINT;
                    Settings.MipFilter = FILTER_TYPE_POINT;
                }
                Samplers.emplace_back(SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, TextureNames[TextureIndex].c_str(), Settings);
                ++TextureIndex;
            }
            if (Material->Definition.Domain == MaterialDomain::PostProcess)
            {
                Samplers.emplace_back(SHADER_TYPE_PIXEL, "SceneDepth", Point);
            }
        }
        Description.PSODesc.ResourceLayout.ImmutableSamplers = Samplers.data();
        Description.PSODesc.ResourceLayout.NumImmutableSamplers = static_cast<uint32_t>(Samplers.size());
        GraphicsDevice->CreateGraphicsPipelineState(Description, &Target.State);
        if (!Target.State)
        {
            return false;
        }
        if (!bAsync)
        {
            Pipeline->InitializeBinding(Target);
            return Target.Binding != nullptr;
        }
        return true;
    };
    for (uint32_t SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        if (!Pipeline->CreateState(Pipeline->Opaque[SideIndex], OpaquePixel, 0, SideIndex != 0, nullptr, nullptr, false)
            || !Pipeline->CreateState(Pipeline->Transparent[SideIndex], TransparentPixel, 1, SideIndex != 0, nullptr, nullptr, false)
            || !Pipeline->CreateState(Pipeline->Shadow[SideIndex], ShadowPixel, 2, SideIndex != 0, nullptr, nullptr, false))
        {
            return false;
        }
    }
    return Pipeline->CreateState(Pipeline->Composite, CompositePixel, 3, true, nullptr, nullptr, false)
        && Pipeline->CreateState(Pipeline->ToneMap, TonePixel, 3, true, nullptr, nullptr, false) && Pipeline->CreateState(Pipeline->Antialias, AntialiasPixel, 4, true, nullptr, nullptr, false)
        && Pipeline->CreateState(Pipeline->PreviewAntialias, PreviewPixel, 7, true, nullptr, nullptr, false);
}

std::string Renderer::GetMaterialVariantKey(const std::string& MaterialKey) const
{
    return MaterialKey + ":backend=" + std::to_string(static_cast<int>(Device.GetDevice()->GetDeviceInfo().Type))
        + ":vertex=position-normal-color-uv-tangent-v1:hdr=rgba16float:ui=rgba8:depth=d32";
}

void Renderer::PrepareMaterial(const ResolvedMaterial& Material, std::shared_ptr<MaterialCompilation> Compilation)
{
    AssertRenderThread();
    if (!bInitialized || !Pipeline)
    {
        Compilation->State.store(MaterialCompilationState::Cancelled);
        return;
    }
    auto Existing = Pipeline->Materials.find(GetMaterialVariantKey(Material.VariantKey));
    if (Existing != Pipeline->Materials.end())
    {
        Compilation->State.store(Existing->second.Compilation->State.load());
        std::lock_guard<std::mutex> Lock(Compilation->Mutex);
        Compilation->Diagnostic = Existing->second.Compilation->GetDiagnostic();
        return;
    }
    auto& Variant = Pipeline->Materials[GetMaterialVariantKey(Material.VariantKey)];
    Variant.Material = Material;
    Variant.Compilation = std::move(Compilation);
    Variant.Compilation->State.store(MaterialCompilationState::Compiling);
    std::ostringstream Source;
    Source << "#define SAKURA_CUSTOM_MATERIAL 1\n#define SAKURA_MATERIAL_DOMAIN " << static_cast<int>(Material.Definition.Domain)
        << "\n#define SAKURA_SHADING_MODEL " << static_cast<int>(Material.Definition.ShadingModel)
        << "\n#define SAKURA_BLEND_MODE " << static_cast<int>(Material.Definition.BlendMode)
        << "\n#define SAKURA_MODIFY_VERTEX " << static_cast<int>(Material.Definition.bModifyVertex)
        << "\n#define SAKURA_RECEIVE_SHADOWS " << static_cast<int>(Material.Definition.bReceiveShadows) << "\n";
    Source << Pipeline->MaterialContract << "\n" << MaterialDefinitionIO::BuildParameterDeclarations(Material)
        << Material.Definition.PreparedSource << "\n";
    const bool bSurface = Material.Definition.Domain == MaterialDomain::Surface;
    if (bSurface)
    {
        Source << "#include \"Forward.hlsl\"\n";
    }
    else
    {
        Source << "#include \"MaterialScreen.hlsl\"\n";
    }
    const std::string Prepared = Source.str();
    const bool bAsync = Device.GetDevice()->GetDeviceInfo().Features.AsyncShaderCompilation == DEVICE_FEATURE_STATE_ENABLED;
    auto CreateShader = [&](const char* Entry, SHADER_TYPE Stage, RefCntAutoPtr<IShader>& Shader, RefCntAutoPtr<IDataBlob>& Errors)
    {
        ShaderCreateInfo Description;
        Description.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        Description.Desc.UseCombinedTextureSamplers = true;
        Description.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;
        if (bAsync)
        {
            Description.CompileFlags |= SHADER_COMPILE_FLAG_ASYNCHRONOUS;
        }
        Description.pShaderSourceStreamFactory = Pipeline->SourceFactory;
        Description.Source = Prepared.c_str();
        Description.SourceLength = Prepared.size();
        Description.EntryPoint = Entry;
        Description.Desc.Name = Material.VariantKey.c_str();
        Description.Desc.ShaderType = Stage;
        CreateEngineShader(Device.GetDevice(), Description, &Shader, &Errors);
    };
    const char* VertexEntry = "MaterialFullscreenMain";
    const char* PixelEntry = "MaterialPixelMain";
    if (bSurface)
    {
        VertexEntry = "VSMain";
        PixelEntry = "PSMain";
        if (Material.Definition.BlendMode == MaterialBlendMode::Translucent)
        {
            PixelEntry = "TransparencyMain";
        }
        else if (Material.Definition.BlendMode == MaterialBlendMode::Additive)
        {
            PixelEntry = "AdditiveMain";
        }
    }
    CreateShader(VertexEntry, SHADER_TYPE_VERTEX, Variant.Vertex, Variant.VertexErrors);
    CreateShader(PixelEntry, SHADER_TYPE_PIXEL, Variant.Pixel, Variant.PixelErrors);
    if (bSurface && Material.Definition.bCastShadows && Material.Definition.BlendMode <= MaterialBlendMode::Masked)
    {
        CreateShader("ShadowMain", SHADER_TYPE_PIXEL, Variant.ShadowPixel, Variant.ShadowErrors);
    }
    UpdateMaterialCompilations();
}

void Renderer::UpdateMaterialCompilations()
{
    AssertRenderThread();
    for (auto& Pair : Pipeline->Materials)
    {
        auto& Variant = Pair.second;
        if (Variant.bReady || Variant.bFailed || Variant.Compilation->State.load() == MaterialCompilationState::Cancelled)
        {
            continue;
        }
        bool bFailed = !Variant.Vertex || !Variant.Pixel;
        bool bCompiling = false;
        for (const auto& Shader : {Variant.Vertex, Variant.Pixel, Variant.ShadowPixel})
        {
            if (!Shader)
            {
                continue;
            }
            const auto Status = Shader->GetStatus(false);
            bFailed = bFailed || Status == SHADER_STATUS_FAILED;
            bCompiling = bCompiling || Status == SHADER_STATUS_COMPILING;
        }
        if (bCompiling && !bFailed)
        {
            continue;
        }
        if (!bFailed && !Variant.bPipelineRequested)
        {
            const auto& Definition = Variant.Material.Definition;
            uint32_t Kind = 0;
            if (Definition.Domain == MaterialDomain::PostProcess)
            {
                Kind = 3;
            }
            else if (Definition.Domain == MaterialDomain::UserInterface)
            {
                Kind = 6;
            }
            else if (Definition.BlendMode == MaterialBlendMode::Translucent)
            {
                Kind = 1;
            }
            else if (Definition.BlendMode == MaterialBlendMode::Additive)
            {
                Kind = 5;
            }
            const bool bAsync = Device.GetDevice()->GetDeviceInfo().Features.AsyncShaderCompilation == DEVICE_FEATURE_STATE_ENABLED;
            bFailed = !Pipeline->CreateState(Variant.Main, Variant.Pixel, Kind, Definition.bDoubleSided,
                Variant.Vertex, &Variant.Material, bAsync);
            if (!bFailed && Variant.ShadowPixel)
            {
                bFailed = !Pipeline->CreateState(Variant.Shadow, Variant.ShadowPixel, 2, Definition.bDoubleSided,
                    Variant.Vertex, &Variant.Material, bAsync);
            }
            Variant.bPipelineRequested = true;
        }
        for (auto* State : {Variant.Main.State.RawPtr(), Variant.Shadow.State.RawPtr()})
        {
            if (State != nullptr)
            {
                bCompiling = bCompiling || State->GetStatus(false) == PIPELINE_STATE_STATUS_COMPILING;
                bFailed = bFailed || State->GetStatus(false) == PIPELINE_STATE_STATUS_FAILED;
            }
        }
        if (bFailed)
        {
            Variant.bFailed = true;
            std::string Diagnostic = "Material shader or pipeline compilation failed: " + Pair.first;
            for (const auto& Errors : {Variant.VertexErrors, Variant.PixelErrors, Variant.ShadowErrors})
            {
                if (Errors && Errors->GetSize() > 0)
                {
                    Diagnostic += "\n" + std::string(static_cast<const char*>(Errors->GetConstDataPtr()));
                }
            }
            {
                std::lock_guard<std::mutex> Lock(Variant.Compilation->Mutex);
                Variant.Compilation->Diagnostic = Diagnostic;
            }
            Variant.Compilation->State.store(MaterialCompilationState::Failed);
            PrintString(Diagnostic);
            continue;
        }
        if (bCompiling)
        {
            continue;
        }
        Pipeline->InitializeBinding(Variant.Main);
        if (Variant.Shadow.State)
        {
            Pipeline->InitializeBinding(Variant.Shadow);
        }
        Variant.bReady = Variant.Main.Binding != nullptr;
        if (Variant.bReady)
        {
            Variant.Compilation->State.store(MaterialCompilationState::Ready);
        }
    }
}

void Renderer::ClearMaterialResources()
{
    AssertRenderThread();
    if (Pipeline == nullptr)
    {
        return;
    }
    Device.WaitForIdle();
    for (auto& Image : Pipeline->Images)
    {
        Image.Request->bCancelled.store(true);
        Image.Request->State.store(MaterialImageState::Cancelled);
    }
    Pipeline->Images.clear();
    for (auto& Material : Pipeline->Materials)
    {
        Material.second.Compilation->State.store(MaterialCompilationState::Cancelled);
    }
    Pipeline->Materials.clear();
}

void Renderer::QueueMaterialImage(std::shared_ptr<MaterialImageRequest> Request)
{
    AssertRenderThread();
    if (!Pipeline || Request->bCancelled.load())
    {
        Request->State.store(MaterialImageState::Cancelled);
        return;
    }
    Pipeline->Images.push_back({std::move(Request), {}, {}, 0});
    PollMaterialImages();
}

void Renderer::PollMaterialImages()
{
    AssertRenderThread();
    if (!Pipeline)
    {
        return;
    }
    UpdateMaterialCompilations();
    auto* Context = Device.GetImmediateContext();
    for (auto Iterator = Pipeline->Images.begin(); Iterator != Pipeline->Images.end();)
    {
        auto& Readback = *Iterator;
        const auto& Request = Readback.Request;
        if (Readback.CompletionValue == 0 && Request->bCancelled.load())
        {
            Request->State.store(MaterialImageState::Cancelled);
            Iterator = Pipeline->Images.erase(Iterator);
            continue;
        }
        if (Readback.CompletionValue == 0)
        {
            const auto Found = Pipeline->Materials.find(GetMaterialVariantKey(Request->Snapshot->Material->VariantKey));
            if (Found == Pipeline->Materials.end() || !Found->second.bReady)
            {
                if (Found != Pipeline->Materials.end() && Found->second.bFailed)
                {
                    {
                        std::lock_guard<std::mutex> Lock(Request->Mutex);
                        Request->Diagnostic = Found->second.Compilation->GetDiagnostic();
                    }
                    Request->State.store(MaterialImageState::Failed);
                    Iterator = Pipeline->Images.erase(Iterator);
                    continue;
                }
                ++Iterator;
                continue;
            }
            auto& Selected = Found->second.Main;
            Readback.Target = CreateTarget(Device.GetDevice(), "Material widget", Request->Width, Request->Height,
                TEX_FORMAT_RGBA8_UNORM, BIND_RENDER_TARGET | BIND_SHADER_RESOURCE);
            TextureDesc StagingDescription;
            StagingDescription.Name = "Material readback";
            StagingDescription.Type = RESOURCE_DIM_TEX_2D;
            StagingDescription.Width = Request->Width;
            StagingDescription.Height = Request->Height;
            StagingDescription.Format = TEX_FORMAT_RGBA8_UNORM;
            StagingDescription.Usage = USAGE_STAGING;
            StagingDescription.CPUAccessFlags = CPU_ACCESS_READ;
            Device.GetDevice()->CreateTexture(StagingDescription, nullptr, &Readback.Staging);
            if (!Readback.Target || !Readback.Staging)
            {
                {
                    std::lock_guard<std::mutex> Lock(Request->Mutex);
                    Request->Diagnostic = "Unable to create material image textures";
                }
                Request->State.store(MaterialImageState::Failed);
                Iterator = Pipeline->Images.erase(Iterator);
                continue;
            }
            if (Request->Snapshot->Material->Definition.Domain != MaterialDomain::UserInterface)
            {
                RenderMaterialPreview(*Request, Readback.Target->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET));
            }
            else
            {
            Pipeline->MaterialBuffer.Update(Context, Request->Snapshot->Parameters.data(), 4096);
            uint32_t Presence[4] = {};
            uint32_t TextureIndex = 0;
            for (const auto& Binding : Request->Snapshot->Textures)
            {
                auto* Texture = Resources.GetTextureShaderView(Binding.Texture);
                if (Texture != nullptr)
                {
                    Presence[0] |= 1u << TextureIndex;
                }
                else
                {
                    Texture = Pipeline->WhiteTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
                }
                ++TextureIndex;
                BindTexture(Selected, ("SakuraTexture_" + Binding.Identifier).c_str(), Texture);
            }
            Pipeline->MaterialTextureBuffer.Update(Context, Presence, sizeof(Presence));
            MaterialViewConstants Constants;
            Constants.ResolutionTime = Vector4(static_cast<float>(Request->Width), static_cast<float>(Request->Height), Request->Time, 1.f);
            Constants.WidgetColor = Request->Color;
            Constants.ViewOptions.y = Request->Snapshot->Material->Definition.AlphaCutoff;
            if (Device.GetDevice()->GetDeviceInfo().IsGLDevice())
            {
                Constants.ViewOptions.x = 1.f;
            }
            Pipeline->MaterialViewBuffer.Update(Context, &Constants, sizeof(Constants));
            ITextureView* Target = Readback.Target->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
            Context->SetRenderTargets(1, &Target, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            const float Transparent[] = {0.f, 0.f, 0.f, 0.f};
            Context->ClearRenderTarget(Target, Transparent, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            Diligent::Viewport Viewport(0.f, 0.f, static_cast<float>(Request->Width), static_cast<float>(Request->Height));
            Context->SetViewports(1, &Viewport, Request->Width, Request->Height);
            Context->SetPipelineState(Selected.State);
            Context->CommitShaderResources(Selected.Binding, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            DrawAttribs Draw;
            Draw.NumVertices = 3;
            Draw.Flags = DRAW_FLAG_VERIFY_ALL;
            Context->Draw(Draw);
            }
            Context->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
            CopyTextureAttribs Copy;
            Copy.pSrcTexture = Readback.Target;
            Copy.pDstTexture = Readback.Staging;
            Copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            Copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            Context->CopyTexture(Copy);
            Readback.CompletionValue = ++Pipeline->ImageSequence;
            Context->EnqueueSignal(Pipeline->ImageCompletion, Readback.CompletionValue);
            Context->Flush();
            Request->State.store(MaterialImageState::Reading);
        }
        if (Pipeline->ImageCompletion->GetCompletedValue() < Readback.CompletionValue)
        {
            ++Iterator;
            continue;
        }
        if (Request->bCancelled.load())
        {
            Request->State.store(MaterialImageState::Cancelled);
            Iterator = Pipeline->Images.erase(Iterator);
            continue;
        }
        MappedTextureSubresource Mapped;
        Context->MapTextureSubresource(Readback.Staging, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, Mapped);
        if (Mapped.pData == nullptr)
        {
            ++Iterator;
            continue;
        }
        {
            std::lock_guard<std::mutex> Lock(Request->Mutex);
            Request->Pixels.resize(static_cast<size_t>(Request->Width) * Request->Height * 4);
            for (uint32_t Row = 0; Row < Request->Height; ++Row)
            {
                uint32_t SourceRow = Row;
                if (Device.GetDevice()->GetDeviceInfo().IsGLDevice())
                {
                    SourceRow = Request->Height - Row - 1;
                }
                std::memcpy(Request->Pixels.data() + static_cast<size_t>(Row) * Request->Width * 4,
                    static_cast<const uint8_t*>(Mapped.pData) + static_cast<size_t>(SourceRow) * Mapped.Stride, Request->Width * 4);
            }
        }
        Context->UnmapTextureSubresource(Readback.Staging, 0, 0);
        Request->State.store(MaterialImageState::Ready);
        Iterator = Pipeline->Images.erase(Iterator);
    }
}

bool Renderer::CreateSurfaceResources(RenderSurfaceId Surface)
{
    auto* SwapChain = Device.GetSurface(Surface);
    auto& Target = Pipeline->Surfaces[Surface.Value];
    if (SwapChain != nullptr)
    {
        Target.Width = SwapChain->GetDesc().Width;
        Target.Height = SwapChain->GetDesc().Height;
    }
    if (Target.Width == 0 || Target.Height == 0)
    {
        return false;
    }
    const auto ColorBindings = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
    Target.Radiance = CreateTarget(Device.GetDevice(), "HDR scene", Target.Width, Target.Height, TEX_FORMAT_RGBA16_FLOAT, ColorBindings);
    Target.Accumulation = CreateTarget(Device.GetDevice(), "Transparency accumulation", Target.Width, Target.Height, TEX_FORMAT_RGBA16_FLOAT, ColorBindings);
    Target.Revealage = CreateTarget(Device.GetDevice(), "Transparency revealage", Target.Width, Target.Height, TEX_FORMAT_R16_FLOAT, ColorBindings);
    Target.ToneMapped = CreateTarget(Device.GetDevice(), "Tone mapped scene", Target.Width, Target.Height, TEX_FORMAT_RGBA16_FLOAT, ColorBindings);
    for (auto& Intermediate : Target.PostTargets)
    {
        Intermediate = CreateTarget(Device.GetDevice(), "Material intermediate", Target.Width, Target.Height, TEX_FORMAT_RGBA16_FLOAT, ColorBindings);
    }
    Target.Depth = CreateTarget(Device.GetDevice(), "Scene depth", Target.Width, Target.Height, TEX_FORMAT_D32_FLOAT, BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE);
    if (!Target.ShadowAtlas)
    {
        Target.ShadowAtlas = CreateTarget(Device.GetDevice(), "Shadow atlas", 4096, 4096, TEX_FORMAT_D32_FLOAT, BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE);
        if (Surface.Value != 0xfffffffe && Device.GetDevice()->GetDeviceInfo().Features.DurationQueries == DEVICE_FEATURE_STATE_ENABLED)
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
    UpdateMaterialCompilations();
    if (!bInitialized)
    {
        return;
    }
    for (const RenderViewFrame& View : Frame.Views)
    {
        RenderView(View, Frame.FrameIndex);
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

void Renderer::RenderView(const RenderViewFrame& View, uint64_t FrameIndex, ITextureView* Output)
{
    const auto Iterator = Pipeline->Surfaces.find(View.Surface.Value);
    auto* SwapChain = Device.GetSurface(View.Surface);
    if (Iterator == Pipeline->Surfaces.end() || (SwapChain == nullptr && Output == nullptr) || Iterator->second.bSuspended)
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
    Frame.FrameParameters.w = View.TimeSeconds;
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
        auto& LightConstants = Frame.Lights[Statistics.LightCount++];
        Vector3 Direction = Light.Direction;
        Direction.Normalize();
        LightConstants.PositionType = Vector4(Light.Position.x, Light.Position.y, Light.Position.z, static_cast<float>(Light.Type));
        LightConstants.DirectionRange = Vector4(Direction.x, Direction.y, Direction.z, std::max(0.01f, Light.Range));
        LightConstants.ColorIntensity = Vector4(Light.LightColor.x, Light.LightColor.y, Light.LightColor.z, std::max(0.f, Light.Intensity));
        const float OuterDegrees = std::clamp(Light.OuterConeAngle, 1.f, 89.f);
        const float OuterAngle = OuterDegrees * 0.0174532925f;
        const float InnerAngle = std::clamp(Light.InnerConeAngle, 0.f, OuterDegrees - 0.01f) * 0.0174532925f;
        LightConstants.ConeShadow = Vector4(std::cos(InnerAngle), std::cos(OuterAngle), -1.f, 0.f);
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
        LightConstants.ConeShadow.z = static_cast<float>(ShadowCount);
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
    auto DrawObject = [&](const RenderObject& Object, ForwardPipeline& DefaultPipeline, bool bShadowPass = false)
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
        ForwardPipeline* Selected = &DefaultPipeline;
        bool bMissingMaterial = false;
        if (Object.MaterialSnapshot && !bPlaceholder)
        {
            const auto Found = Pipeline->Materials.find(GetMaterialVariantKey(Object.MaterialSnapshot->Material->VariantKey));
            if (Found != Pipeline->Materials.end() && Found->second.bReady)
            {
                if (bShadowPass)
                {
                    if (!Found->second.Shadow.State)
                    {
                        return;
                    }
                    Selected = &Found->second.Shadow;
                }
                else
                {
                    Selected = &Found->second.Main;
                }
                Pipeline->MaterialBuffer.Update(Context, Object.MaterialSnapshot->Parameters.data(), 4096);
                uint32_t Presence[4] = {};
                uint32_t TextureIndex = 0;
                for (const auto& Binding : Object.MaterialSnapshot->Textures)
                {
                    ITextureView* Texture = Resources.GetTextureShaderView(Binding.Texture);
                    if (Texture != nullptr)
                    {
                        Presence[0] |= 1u << TextureIndex;
                    }
                    ++TextureIndex;
                    if (Texture == nullptr)
                    {
                        Texture = Pipeline->WhiteTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
                    }
                    const std::string Name = "SakuraTexture_" + Binding.Identifier;
                    for (SHADER_TYPE Stage : {SHADER_TYPE_VERTEX, SHADER_TYPE_PIXEL})
                    {
                        if (auto* Variable = Selected->Binding->GetVariableByName(Stage, Name.c_str()))
                        {
                            Variable->Set(Texture);
                        }
                    }
                }
                Pipeline->MaterialTextureBuffer.Update(Context, Presence, sizeof(Presence));
                if (!bShadowPass)
                {
                    BindTexture(*Selected, "ShadowAtlas", Target.ShadowAtlas->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
                }
            }
            else
            {
                bMissingMaterial = true;
            }
        }
        ForwardObjectConstants Constants;
        Constants.World = Object.WorldMatrix;
        Constants.NormalTransform = Object.WorldMatrix.Invert().Transpose();
        Constants.BaseColor = Object.BaseColor;
        if (bPlaceholder || bMissingMaterial)
        {
            Constants.BaseColor = Color(1.f, 0.35f, 0.75f, 1.f);
        }
        RenderMaterial Material;
        if (Object.MaterialSnapshot)
        {
            Material.AlphaCutoff = Object.MaterialSnapshot->Material->Definition.AlphaCutoff;
        }
        Constants.MaterialParameters = Vector4(Material.Metallic, Material.Roughness, Material.AlphaCutoff, 0.f);
        Constants.EmissiveAlphaMode = Vector4(Material.Emissive.x, Material.Emissive.y, Material.Emissive.z, static_cast<float>(Material.AlphaMode));
        ITextureView* Texture = nullptr;
        if (Texture != nullptr && !bPlaceholder)
        {
            Constants.MaterialParameters.w = 1.f;
        }
        else
        {
            Texture = Pipeline->WhiteTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        }
        BindTexture(*Selected, "BaseColorImage", Texture);
        Pipeline->ObjectBuffer.Update(Context, &Constants, sizeof(Constants));
        Context->SetPipelineState(Selected->State);
        IBuffer* Buffers[] = {Mesh->GetVertexBuffer().GetBuffer()};
        Uint64 Offsets[] = {0};
        Context->SetVertexBuffers(0, 1, Buffers, Offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
        Context->SetIndexBuffer(Mesh->GetIndexBuffer().GetBuffer(), 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        Context->CommitShaderResources(Selected->Binding, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        DrawIndexedAttribs Draw;
        Draw.IndexType = VT_UINT32;
        Draw.NumIndices = Mesh->GetIndexCount();
        if (Object.IndexCount != 0 && !bPlaceholder)
        {
            Draw.NumIndices = Object.IndexCount;
            Draw.FirstIndexLocation = Object.IndexOffset;
        }
        Draw.Flags = DRAW_FLAG_VERIFY_ALL;
        Context->DrawIndexed(Draw);
        ++Statistics.DrawCalls;
        Statistics.Triangles += Draw.NumIndices / 3;
    };
    const auto IsTranslucent = [](const RenderObject& Object)
    {
        if (Object.MaterialSnapshot)
        {
            return Object.MaterialSnapshot->Material->Definition.BlendMode == MaterialBlendMode::Translucent;
        }
        return false;
    };
    const auto IsAdditive = [](const RenderObject& Object)
    {
        return Object.MaterialSnapshot && Object.MaterialSnapshot->Material->Definition.BlendMode == MaterialBlendMode::Additive;
    };
    const auto CastsShadow = [](const RenderObject& Object)
    {
        if (Object.MaterialSnapshot)
        {
            const auto& Definition = Object.MaterialSnapshot->Material->Definition;
            return Definition.bCastShadows && Definition.BlendMode <= MaterialBlendMode::Masked;
        }
        return true;
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
            if (Object.bVisible && CastsShadow(Object)
                && IntersectsFrustum(Object.Bounds, Frame.ViewProjection))
            {
                DrawObject(Object, Pipeline->Shadow[Object.MaterialSnapshot && Object.MaterialSnapshot->Material->Definition.bDoubleSided], true);
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
        if (!IsTranslucent(*Object) && !IsAdditive(*Object))
        {
            DrawObject(*Object, Pipeline->Opaque[Object->MaterialSnapshot && Object->MaterialSnapshot->Material->Definition.bDoubleSided]);
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
        if (IsTranslucent(*Object))
        {
            DrawObject(*Object, Pipeline->Transparent[Object->MaterialSnapshot && Object->MaterialSnapshot->Material->Definition.bDoubleSided]);
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
    DrawAttribs FullscreenDraw;
    FullscreenDraw.NumVertices = 3;
    FullscreenDraw.Flags = DRAW_FLAG_VERIFY_ALL;
    ITextureView* CompositeTarget = Target.PostTargets[0]->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
    Context->SetRenderTargets(1, &CompositeTarget, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    BindTexture(Pipeline->Composite, "SceneImage", Target.Radiance->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    BindTexture(Pipeline->Composite, "AccumulationImage", Target.Accumulation->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    BindTexture(Pipeline->Composite, "RevealageImage", Target.Revealage->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    Context->SetPipelineState(Pipeline->Composite.State);
    Context->CommitShaderResources(Pipeline->Composite.Binding, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Context->Draw(FullscreenDraw);
    ++Statistics.DrawCalls;
    Context->SetRenderTargets(1, &CompositeTarget, Depth, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    for (const RenderObject* Object : Visible)
    {
        if (IsAdditive(*Object))
        {
            DrawObject(*Object, Pipeline->Opaque[Object->MaterialSnapshot && Object->MaterialSnapshot->Material->Definition.bDoubleSided]);
        }
    }
    ITexture* CurrentColor = Target.PostTargets[0];
    auto ApplyPostEffects = [&](MaterialPostProcessStage Stage)
    {
        if (!View.Settings.bMaterialPostProcess)
        {
            return;
        }
        for (const auto& Effect : View.Scene.PostProcessEffects)
        {
            if (!Effect.Snapshot || Effect.Intensity <= 0.f || Effect.Snapshot->Material->Definition.PostProcessStage != Stage)
            {
                continue;
            }
            auto Found = Pipeline->Materials.find(GetMaterialVariantKey(Effect.Snapshot->Material->VariantKey));
            if (Found == Pipeline->Materials.end() || !Found->second.bReady)
            {
                continue;
            }
            auto& Selected = Found->second.Main;
            Pipeline->MaterialBuffer.Update(Context, Effect.Snapshot->Parameters.data(), 4096);
            uint32_t Presence[4] = {};
            uint32_t TextureIndex = 0;
            for (const auto& Binding : Effect.Snapshot->Textures)
            {
                auto* Texture = Resources.GetTextureShaderView(Binding.Texture);
                if (Texture != nullptr)
                {
                    Presence[0] |= 1u << TextureIndex;
                }
                else
                {
                    Texture = Pipeline->WhiteTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
                }
                ++TextureIndex;
                BindTexture(Selected, ("SakuraTexture_" + Binding.Identifier).c_str(), Texture);
            }
            Pipeline->MaterialTextureBuffer.Update(Context, Presence, sizeof(Presence));
            MaterialViewConstants Constants;
            Constants.InverseViewProjection = View.Scene.Camera.ViewProjection.Invert();
            Constants.ResolutionTime = Vector4(static_cast<float>(Target.Width), static_cast<float>(Target.Height), Frame.FrameParameters.w, Effect.Intensity);
            Constants.ViewOptions.x = Frame.OutputParameters.x;
            Pipeline->MaterialViewBuffer.Update(Context, &Constants, sizeof(Constants));
            ITexture* Destination = Target.PostTargets[0];
            if (CurrentColor == Destination)
            {
                Destination = Target.PostTargets[1];
            }
            ITextureView* DestinationView = Destination->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
            Context->SetRenderTargets(1, &DestinationView, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            BindTexture(Selected, "SceneImage", CurrentColor->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
            BindTexture(Selected, "SceneDepth", Target.Depth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
            Context->SetPipelineState(Selected.State);
            Context->CommitShaderResources(Selected.Binding, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            Context->Draw(FullscreenDraw);
            CurrentColor = Destination;
            ++Statistics.DrawCalls;
        }
    };
    ApplyPostEffects(MaterialPostProcessStage::BeforeToneMapping);
    ITextureView* ToneTarget = Target.ToneMapped->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
    Context->SetRenderTargets(1, &ToneTarget, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    BindTexture(Pipeline->ToneMap, "SceneImage", CurrentColor->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    Context->SetPipelineState(Pipeline->ToneMap.State);
    Context->CommitShaderResources(Pipeline->ToneMap.Binding, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Context->Draw(FullscreenDraw);
    ++Statistics.DrawCalls;
    CurrentColor = Target.ToneMapped;
    ApplyPostEffects(MaterialPostProcessStage::AfterToneMapping);
    ITextureView* BackBuffer = Output;
    ForwardPipeline* Antialias = &Pipeline->PreviewAntialias;
    if (Output == nullptr)
    {
        BackBuffer = SwapChain->GetCurrentBackBufferRTV();
        Antialias = &Pipeline->Antialias;
    }
    Context->SetRenderTargets(1, &BackBuffer, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    BindTexture(*Antialias, "SceneImage", CurrentColor->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    Context->SetPipelineState(Antialias->State);
    Context->CommitShaderResources(Antialias->Binding, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Context->Draw(FullscreenDraw);
    ++Statistics.DrawCalls;
    EndPass(3);
    if (ActiveTiming != nullptr)
    {
        ActiveTiming->bPending = true;
        ActiveTiming->FrameIndex = FrameIndex;
    }
    Statistics.CpuMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - CpuStart).count();
    Target.Statistics = Statistics;
    if (Output == nullptr)
    {
        RenderImGuiOverlay(Context, SwapChain, View.Overlay);
        SwapChain->Present(0);
    }
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
    DestroyImGuiOverlay();
    if (Pipeline)
    {
        for (const auto& Image : Pipeline->Images)
        {
            Image.Request->State.store(MaterialImageState::Cancelled);
        }
        for (const auto& Pair : Pipeline->Materials)
        {
            Pair.second.Compilation->State.store(MaterialCompilationState::Cancelled);
        }
    }
    delete Pipeline;
    Pipeline = nullptr;
    Resources.Shutdown();
    Device.Shutdown();
    bInitialized = false;
}

void Renderer::RenderMaterialPreview(const MaterialImageRequest& Request, ITextureView* Output)
{
    RenderViewFrame View;
    View.Surface = RenderSurfaceId{0xfffffffe};
    View.TimeSeconds = Request.Time;
    auto& Target = Pipeline->Surfaces[View.Surface.Value];
    if (Target.Width != Request.Width || Target.Height != Request.Height)
    {
        Target.Width = Request.Width;
        Target.Height = Request.Height;
        CreateSurfaceResources(View.Surface);
    }
    View.Scene.Camera.bValid = true;
    View.Scene.Camera.Position = Vector3(3.f, 2.f, 4.f);
    View.Scene.Camera.View = Matrix::CreateLookAt(View.Scene.Camera.Position, Vector3::Zero, Vector3::Up);
    View.Scene.Camera.Projection = Matrix::CreatePerspectiveFieldOfView(0.8f,
        static_cast<float>(Request.Width) / Request.Height, 0.1f, 100.f);
    View.Scene.Camera.ViewProjection = View.Scene.Camera.View * View.Scene.Camera.Projection;
    RenderObject Object;
    Object.Mesh = PreviewMeshes[std::clamp(Request.PreviewShape, 0, 2)];
    Object.Bounds = AxisAlignedBounds::FromCenterExtents(Vector3::Zero, Vector3(2.f));
    if (Request.Snapshot->Material->Definition.Domain == MaterialDomain::Surface)
    {
        Object.MaterialSnapshot = Request.Snapshot;
    }
    else
    {
        View.Scene.PostProcessEffects.push_back({Request.Snapshot, 1.f});
        Object.BaseColor = Color(0.7f, 0.3f, 0.2f, 1.f);
    }
    View.Scene.Objects.push_back(Object);
    RenderLight Light;
    Light.Direction = Vector3(-0.5f, -0.8f, -0.6f);
    Light.Intensity = 3.f;
    View.Scene.Lights.push_back(Light);
    RenderView(View, 0, Output);
}
