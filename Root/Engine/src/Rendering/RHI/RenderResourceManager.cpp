#include "Rendering/RHI/RenderResourceManager.h"
#include "Assets/Resources/StaticMeshResource.h"
#include "Assets/Resources/TextureResource.h"
#include "Core/Threading/ThreadContext.h"
#include "Rendering/Resources/RenderVertex.h"

#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Texture.h"
#include "TextureView.h"
#include "GraphicsTypes.h"
#include "RefCntAutoPtr.hpp"

#include <cassert>
#include <atomic>

static std::atomic<uint64_t> NextMeshId{1};
static std::atomic<uint64_t> NextTextureId{1};
#include <vector>

using namespace Diligent;

void RenderResourceManager::Initialize(Diligent::IRenderDevice* InDevice)
{
    AssertRenderThread();
    Device = InDevice;
    Meshes.clear();
    Textures.clear();
    RetiredMeshes.clear();
    RetiredTextures.clear();
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
    RetiredMeshes.clear();
    RetiredTextures.clear();
    Device = nullptr;
}

MeshHandle RenderResourceManager::CreateDefaultQuadMesh()
{
    AssertRenderThread();
    assert(Device != nullptr);

    const RenderVertex Vertices[4] = {
        {Vector3{-0.5f, -0.5f, 0.f}, Vector3::Forward, Vector2{0.f, 1.f}, DirectX::SimpleMath::Color(1.f, 0.2f, 0.2f, 1.f)},
        {Vector3{-0.5f, 0.5f, 0.f}, Vector3::Forward, Vector2{0.f, 0.f}, DirectX::SimpleMath::Color(0.2f, 1.f, 0.2f, 1.f)},
        {Vector3{0.5f, 0.5f, 0.f}, Vector3::Forward, Vector2{1.f, 0.f}, DirectX::SimpleMath::Color(0.2f, 0.2f, 1.f, 1.f)},
        {Vector3{0.5f, -0.5f, 0.f}, Vector3::Forward, Vector2{1.f, 1.f}, DirectX::SimpleMath::Color(1.f, 1.f, 0.2f, 1.f)},
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
        const auto& SourceVertex = SourceMesh.Vertices[Index];
        UploadVertices[Index].Position = SourceVertex.Position;
        UploadVertices[Index].Normal = Vector3::Zero;
        if (SourceVertex.bHasNormal)
        {
            UploadVertices[Index].Normal = SourceVertex.Normal;
        }
        UploadVertices[Index].TexCoord = SourceVertex.bHasTexCoord ? SourceVertex.TexCoord : Vector2::Zero;
        UploadVertices[Index].VertexColor = DirectX::SimpleMath::Color(1.f, 1.f, 1.f, 1.f);
    }

    for (size_t Index = 0; Index + 2 < SourceMesh.Indices.size(); Index += 3)
    {
        const uint32_t First = SourceMesh.Indices[Index];
        const uint32_t Second = SourceMesh.Indices[Index + 1];
        const uint32_t Third = SourceMesh.Indices[Index + 2];
        if (First >= UploadVertices.size() || Second >= UploadVertices.size() || Third >= UploadVertices.size())
        {
            return MeshHandle{};
        }
        const Vector3 Normal = (UploadVertices[Second].Position - UploadVertices[First].Position).Cross(
            UploadVertices[Third].Position - UploadVertices[First].Position);
        for (uint32_t VertexIndex : {First, Second, Third})
        {
            if (!SourceMesh.Vertices[VertexIndex].bHasNormal)
            {
                UploadVertices[VertexIndex].Normal += Normal;
            }
        }
    }
    for (auto& Vertex : UploadVertices)
    {
        if (Vertex.Normal.LengthSquared() > 0.0000001f)
        {
            Vertex.Normal.Normalize();
        }
        else
        {
            Vertex.Normal = Vector3::Up;
        }
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

void RenderResourceManager::ReleaseMesh(MeshHandle Mesh, uint64_t CompletionValue)
{
    AssertRenderThread();
    if (Meshes.find(Mesh.Value) != Meshes.end())
    {
        RetiredMeshes[Mesh.Value] = CompletionValue;
    }
}

void RenderResourceManager::ReleaseTexture(TextureHandle Texture, uint64_t CompletionValue)
{
    AssertRenderThread();
    if (Textures.find(Texture.Value) != Textures.end())
    {
        RetiredTextures[Texture.Value] = CompletionValue;
    }
}

void RenderResourceManager::CollectRetired(uint64_t CompletedValue)
{
    AssertRenderThread();
    for (auto Iterator = RetiredMeshes.begin(); Iterator != RetiredMeshes.end();)
    {
        if (Iterator->second <= CompletedValue)
        {
            Meshes.erase(Iterator->first);
            Iterator = RetiredMeshes.erase(Iterator);
        }
        else
        {
            ++Iterator;
        }
    }
    for (auto Iterator = RetiredTextures.begin(); Iterator != RetiredTextures.end();)
    {
        if (Iterator->second <= CompletedValue)
        {
            auto Texture = Textures.find(Iterator->first);
            if (Texture != Textures.end())
            {
                Texture->second.ShaderView->Release();
                Texture->second.Texture->Release();
                Textures.erase(Texture);
            }
            Iterator = RetiredTextures.erase(Iterator);
        }
        else
        {
            ++Iterator;
        }
    }
}

void RenderResourceManager::GetStatistics(RenderStatistics& Statistics) const
{
    AssertRenderThread();
    Statistics.ResidentMeshes = static_cast<uint32_t>(Meshes.size());
    Statistics.ResidentTextures = static_cast<uint32_t>(Textures.size());
    Statistics.RetiredResources = static_cast<uint32_t>(RetiredMeshes.size() + RetiredTextures.size());
    for (const auto& Entry : Meshes)
    {
        Statistics.ResidentBytes += Entry.second.GetVertexBuffer().GetByteSize() + Entry.second.GetIndexBuffer().GetByteSize();
    }
    for (const auto& Entry : Textures)
    {
        const auto& Description = Entry.second.Texture->GetDesc();
        Statistics.ResidentBytes += static_cast<uint64_t>(Description.Width) * Description.Height * 4;
    }
}
