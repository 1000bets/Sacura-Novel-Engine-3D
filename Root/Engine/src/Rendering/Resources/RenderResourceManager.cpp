#include "Rendering/Resources/RenderResourceManager.h"
#include "Assets/Resources/StaticMeshResource.h"
#include "Assets/Resources/TextureResource.h"
#include "Core/Threading/ThreadContext.h"
#include "Rendering/Resources/RenderVertex.h"

#include "RenderDevice.h"
#include "Texture.h"
#include "TextureView.h"
#include "GraphicsTypes.h"
#include "RefCntAutoPtr.hpp"

#include <cassert>
#include <vector>

using namespace Diligent;

void RenderResourceManager::Initialize(Diligent::IRenderDevice* InDevice)
{
    AssertRenderThread();
    Device = InDevice;
    Meshes.clear();
    Textures.clear();
    NextMeshId = 1;
    NextTextureId = 1;
}

void RenderResourceManager::Shutdown()
{
    AssertRenderThread();
    for (auto& Pair : Textures)
    {
        if (Pair.second.ShaderView != nullptr)
        {
            Pair.second.ShaderView->Release();
            Pair.second.ShaderView = nullptr;
        }
        if (Pair.second.Texture != nullptr)
        {
            Pair.second.Texture->Release();
            Pair.second.Texture = nullptr;
        }
    }
    Textures.clear();
    Meshes.clear();
    Device = nullptr;
}

MeshHandle RenderResourceManager::CreateDefaultQuadMesh()
{
    AssertRenderThread();
    assert(Device != nullptr);

    const RenderVertex Vertices[4] = {
        {Vector3{-0.5f, -0.5f, 0.f}, Color{1.f, 0.2f, 0.2f, 1.f}},
        {Vector3{-0.5f, 0.5f, 0.f}, Color{0.2f, 1.f, 0.2f, 1.f}},
        {Vector3{0.5f, 0.5f, 0.f}, Color{0.2f, 0.2f, 1.f, 1.f}},
        {Vector3{0.5f, -0.5f, 0.f}, Color{1.f, 1.f, 0.2f, 1.f}},
    };

    const uint32_t Indices[6] = {0, 1, 2, 0, 2, 3};

    MeshHandle Handle;
    Handle.Value = NextMeshId++;

    RenderMesh& Mesh = Meshes[Handle.Value];
    const bool bCreated = Mesh.Create(
        Device,
        Vertices,
        sizeof(Vertices),
        4,
        Indices,
        sizeof(Indices),
        6);

    if (!bCreated)
    {
        Meshes.erase(Handle.Value);
        return MeshHandle{};
    }

    return Handle;
}

MeshHandle RenderResourceManager::CreateMeshFromCpuData(const StaticMeshResource& SourceMesh)
{
    AssertRenderThread();
    assert(Device != nullptr);

    if (SourceMesh.Vertices.empty() || SourceMesh.Indices.empty())
    {
        return MeshHandle{};
    }

    std::vector<RenderVertex> UploadVertices(SourceMesh.Vertices.size());
    for (size_t Index = 0; Index < SourceMesh.Vertices.size(); ++Index)
    {
        UploadVertices[Index].Position = SourceMesh.Vertices[Index].Position;
        UploadVertices[Index].Color = Color{1.f, 1.f, 1.f, 1.f};
    }

    MeshHandle Handle;
    Handle.Value = NextMeshId++;
    RenderMesh& Mesh = Meshes[Handle.Value];
    const bool bCreated = Mesh.Create(
        Device,
        UploadVertices.data(),
        UploadVertices.size() * sizeof(RenderVertex),
        static_cast<uint32_t>(UploadVertices.size()),
        SourceMesh.Indices.data(),
        SourceMesh.Indices.size() * sizeof(uint32_t),
        static_cast<uint32_t>(SourceMesh.Indices.size()));

    if (!bCreated)
    {
        Meshes.erase(Handle.Value);
        return MeshHandle{};
    }

    return Handle;
}

TextureHandle RenderResourceManager::CreateTextureFromCpuData(const TextureResource& SourceTexture)
{
    AssertRenderThread();
    assert(Device != nullptr);

    if (SourceTexture.Width == 0 || SourceTexture.Height == 0 || SourceTexture.Pixels.empty())
    {
        return TextureHandle{};
    }

    TextureDesc Desc;
    Desc.Name = "AssetTexture";
    Desc.Type = RESOURCE_DIM_TEX_2D;
    Desc.Width = SourceTexture.Width;
    Desc.Height = SourceTexture.Height;
    Desc.Format = SourceTexture.ColorSpace == TextureColorSpace::Srgb ? TEX_FORMAT_RGBA8_UNORM_SRGB : TEX_FORMAT_RGBA8_UNORM;
    Desc.MipLevels = 1;
    Desc.BindFlags = BIND_SHADER_RESOURCE;
    Desc.Usage = USAGE_IMMUTABLE;

    TextureSubResData Subresource;
    Subresource.pData = SourceTexture.Pixels.data();
    Subresource.Stride = SourceTexture.RowStride != 0 ? SourceTexture.RowStride : SourceTexture.Width * 4;

    TextureData Data;
    Data.pSubResources = &Subresource;
    Data.NumSubresources = 1;

    RefCntAutoPtr<ITexture> CreatedTexture;
    Device->CreateTexture(Desc, &Data, &CreatedTexture);
    if (!CreatedTexture)
    {
        return TextureHandle{};
    }

    if (CreatedTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE) == nullptr)
    {
        return TextureHandle{};
    }

    TextureHandle Handle;
    Handle.Value = NextTextureId++;

    GpuTextureEntry Entry{};
    Entry.Texture = CreatedTexture.Detach();
    Entry.ShaderView = Entry.Texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    if (Entry.ShaderView != nullptr)
    {
        Entry.ShaderView->AddRef();
    }
    Textures[Handle.Value] = Entry;
    return Handle;
}

const RenderMesh* RenderResourceManager::GetMesh(MeshHandle Mesh) const
{
    auto Iterator = Meshes.find(Mesh.Value);
    if (Iterator == Meshes.end())
    {
        return nullptr;
    }

    return &Iterator->second;
}

Diligent::ITexture* RenderResourceManager::GetTexture(TextureHandle Texture) const
{
    auto Iterator = Textures.find(Texture.Value);
    if (Iterator == Textures.end())
    {
        return nullptr;
    }

    return Iterator->second.Texture;
}

Diligent::ITextureView* RenderResourceManager::GetTextureShaderView(TextureHandle Texture) const
{
    auto Iterator = Textures.find(Texture.Value);
    if (Iterator == Textures.end())
    {
        return nullptr;
    }

    return Iterator->second.ShaderView;
}
