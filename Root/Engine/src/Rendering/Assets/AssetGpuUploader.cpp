#include "Rendering/Assets/AssetGpuUploader.h"
#include "Rendering/Threading/RenderThread.h"
#include "Core/Threading/ThreadContext.h"
#include "Rendering/RHI/Renderer.h"
#include "Rendering/RHI/RenderResourceManager.h"

class TextureResidencyLifetime
{
public:
    explicit TextureResidencyLifetime(TextureHandle InTexture) : Texture(InTexture) {}
    ~TextureResidencyLifetime()
    {
        RenderThread* Thread = RenderThread::Get();
        if (Thread == nullptr || !Thread->IsReady())
        {
            return;
        }
        const uint64_t Completion = Thread->GetRetirementValue();
        if (GetCurrentThreadRole() == ThreadRole::Render)
        {
            Thread->GetRenderer()->GetResources().ReleaseTexture(Texture, Completion);
        }
        else
        {
            Thread->Enqueue([Thread, Texture = Texture, Completion]()
            {
                Thread->GetRenderer()->GetResources().ReleaseTexture(Texture, Completion);
            });
        }
    }
private:
    TextureHandle Texture;
};

std::shared_ptr<MaterialCompilation> AssetGpuUploader::RequestMaterial(const ResolvedMaterial& Material)
{
    AssertGameThread();
    auto Found = Materials.find(Material.VariantKey);
    if (Found != Materials.end())
    {
        RenderThread* Thread = RenderThread::Get();
        if (Found->second->State.load() == MaterialCompilationState::Cancelled && Thread != nullptr && Thread->IsReady())
        {
            Materials.erase(Found);
        }
        else
        {
            return Found->second;
        }
    }
    auto Compilation = std::make_shared<MaterialCompilation>();
    Materials.emplace(Material.VariantKey, Compilation);
    RenderThread* Thread = RenderThread::Get();
    if (Thread == nullptr || !Thread->IsReady())
    {
        Compilation->State.store(MaterialCompilationState::Cancelled);
        return Compilation;
    }
    const auto Result = Thread->Enqueue([Thread, Material, Compilation]()
    {
        if (Compilation->State.load() != MaterialCompilationState::Cancelled)
        {
            Thread->GetRenderer()->PrepareMaterial(Material, Compilation);
        }
    });
    if (Result != RenderCommandEnqueueResult::Accepted)
    {
        Compilation->State.store(MaterialCompilationState::Cancelled);
    }
    return Compilation;
}

void AssetGpuUploader::RequestUploadMesh(AssetHandle<StaticMeshResource> Mesh, const AssetKey& Key)
{
    AssertGameThread();
    if (!Mesh.IsValid() || !Key.IsValid())
    {
        return;
    }

    const uint64_t CapturedSession = SessionId.load(std::memory_order_acquire);

    {
        std::lock_guard<std::mutex> Lock(Mutex);
        AssetGpuMeshEntry& Entry = Meshes[Key];
        if (Entry.State == AssetGpuState::UploadQueued || Entry.State == AssetGpuState::Resident)
        {
            return;
        }
        Entry.State = AssetGpuState::UploadQueued;
        Entry.Diagnostic = AssetDiagnostic::Ok();
        Entry.Mesh = MeshHandle{};
        Entry.SessionId = CapturedSession;
    }

    RenderThread* Thread = RenderThread::Get();
    if (Thread == nullptr || !Thread->IsReady())
    {
        std::lock_guard<std::mutex> Lock(Mutex);
        AssetGpuMeshEntry& Entry = Meshes[Key];
        if (Entry.SessionId == CapturedSession)
        {
            Entry.State = AssetGpuState::Failed;
            Entry.Diagnostic = AssetDiagnostic::Fail(
                AssetErrorCode::GpuUploadFailed,
                "AssetGpuUploader",
                "RenderThread is not available",
                Key);
        }
        return;
    }

    const RenderCommandEnqueueResult EnqueueResult = Thread->Enqueue([this, Mesh, Key, CapturedSession]()
    {
        if (CapturedSession != SessionId.load(std::memory_order_acquire))
        {
            return;
        }

        Renderer* OwnedRenderer = RenderThread::Get() != nullptr ? RenderThread::Get()->GetRenderer() : nullptr;
        MeshHandle Created{};
        AssetDiagnostic Diagnostic = AssetDiagnostic::Ok();
        if (OwnedRenderer == nullptr)
        {
            Diagnostic = AssetDiagnostic::Fail(
                AssetErrorCode::GpuUploadFailed,
                "AssetGpuUploader",
                "Renderer is not available",
                Key);
        }
        else
        {
            Created = OwnedRenderer->GetResources().CreateMeshFromCpuData(*Mesh);
            if (!Created.IsValid())
            {
                Diagnostic = AssetDiagnostic::Fail(
                    AssetErrorCode::GpuUploadFailed,
                    "AssetGpuUploader",
                    "CreateMeshFromCpuData failed",
                    Key);
            }
        }

        std::lock_guard<std::mutex> Lock(Mutex);
        auto Iterator = Meshes.find(Key);
        if (Iterator == Meshes.end() || Iterator->second.SessionId != CapturedSession)
        {
            if (Created.IsValid() && OwnedRenderer != nullptr)
            {
                OwnedRenderer->GetResources().ReleaseMesh(Created, RenderThread::Get()->GetRetirementValue());
            }
            return;
        }

        if (Created.IsValid())
        {
            Iterator->second.State = AssetGpuState::Resident;
            Iterator->second.Mesh = Created;
            Iterator->second.Diagnostic = AssetDiagnostic::Ok();
        }
        else
        {
            Iterator->second.State = AssetGpuState::Failed;
            Iterator->second.Diagnostic = Diagnostic;
        }
    });

    if (EnqueueResult != RenderCommandEnqueueResult::Accepted)
    {
        std::lock_guard<std::mutex> Lock(Mutex);
        AssetGpuMeshEntry& Entry = Meshes[Key];
        if (Entry.SessionId == CapturedSession)
        {
            Entry.State = AssetGpuState::Failed;
            Entry.Diagnostic = AssetDiagnostic::Fail(
                AssetErrorCode::GpuUploadFailed,
                "AssetGpuUploader",
                "Render command enqueue rejected",
                Key);
        }
    }
}

void AssetGpuUploader::RequestUploadTexture(AssetHandle<TextureResource> Texture, const AssetKey& Key, bool bSrgb)
{
    AssertGameThread();
    if (!Texture.IsValid() || !Key.IsValid())
    {
        return;
    }

    const uint64_t CapturedSession = SessionId.load(std::memory_order_acquire);

    {
        std::lock_guard<std::mutex> Lock(Mutex);
        AssetGpuTextureEntry& Entry = Textures[AssetGpuTextureKey{Key, bSrgb}];
        if (Entry.State == AssetGpuState::UploadQueued || Entry.State == AssetGpuState::Resident)
        {
            return;
        }
        Entry.State = AssetGpuState::UploadQueued;
        Entry.Diagnostic = AssetDiagnostic::Ok();
        Entry.Texture = TextureHandle{};
        Entry.SessionId = CapturedSession;
    }

    RenderThread* Thread = RenderThread::Get();
    if (Thread == nullptr || !Thread->IsReady())
    {
        std::lock_guard<std::mutex> Lock(Mutex);
        AssetGpuTextureEntry& Entry = Textures[AssetGpuTextureKey{Key, bSrgb}];
        if (Entry.SessionId == CapturedSession)
        {
            Entry.State = AssetGpuState::Failed;
            Entry.Diagnostic = AssetDiagnostic::Fail(
                AssetErrorCode::GpuUploadFailed,
                "AssetGpuUploader",
                "RenderThread is not available",
                Key);
        }
        return;
    }

    const RenderCommandEnqueueResult EnqueueResult = Thread->Enqueue([this, Texture, Key, CapturedSession, bSrgb]()
    {
        if (CapturedSession != SessionId.load(std::memory_order_acquire))
        {
            return;
        }

        Renderer* OwnedRenderer = RenderThread::Get() != nullptr ? RenderThread::Get()->GetRenderer() : nullptr;
        TextureHandle Created{};
        AssetDiagnostic Diagnostic = AssetDiagnostic::Ok();
        if (OwnedRenderer == nullptr)
        {
            Diagnostic = AssetDiagnostic::Fail(
                AssetErrorCode::GpuUploadFailed,
                "AssetGpuUploader",
                "Renderer is not available",
                Key);
        }
        else
        {
            Created = OwnedRenderer->GetResources().CreateTextureFromCpuData(*Texture, bSrgb);
            if (!Created.IsValid())
            {
                Diagnostic = AssetDiagnostic::Fail(
                    AssetErrorCode::GpuUploadFailed,
                    "AssetGpuUploader",
                    "CreateTextureFromCpuData failed",
                    Key);
            }
        }

        std::lock_guard<std::mutex> Lock(Mutex);
        auto Iterator = Textures.find(AssetGpuTextureKey{Key, bSrgb});
        if (Iterator == Textures.end() || Iterator->second.SessionId != CapturedSession)
        {
            if (Created.IsValid() && OwnedRenderer != nullptr)
            {
                OwnedRenderer->GetResources().ReleaseTexture(Created, RenderThread::Get()->GetRetirementValue());
            }
            return;
        }

        if (Created.IsValid())
        {
            Iterator->second.State = AssetGpuState::Resident;
            Iterator->second.Texture = Created;
            Iterator->second.Lifetime = std::make_shared<TextureResidencyLifetime>(Created);
            Iterator->second.Diagnostic = AssetDiagnostic::Ok();
        }
        else
        {
            Iterator->second.State = AssetGpuState::Failed;
            Iterator->second.Diagnostic = Diagnostic;
        }
    });

    if (EnqueueResult != RenderCommandEnqueueResult::Accepted)
    {
        std::lock_guard<std::mutex> Lock(Mutex);
        AssetGpuTextureEntry& Entry = Textures[AssetGpuTextureKey{Key, bSrgb}];
        if (Entry.SessionId == CapturedSession)
        {
            Entry.State = AssetGpuState::Failed;
            Entry.Diagnostic = AssetDiagnostic::Fail(
                AssetErrorCode::GpuUploadFailed,
                "AssetGpuUploader",
                "Render command enqueue rejected",
                Key);
        }
    }
}

AssetGpuState AssetGpuUploader::GetMeshState(const AssetKey& Key) const
{
    std::lock_guard<std::mutex> Lock(Mutex);
    auto Iterator = Meshes.find(Key);
    if (Iterator == Meshes.end())
    {
        return AssetGpuState::Absent;
    }
    return Iterator->second.State;
}

AssetGpuState AssetGpuUploader::GetTextureState(const AssetKey& Key, bool bSrgb) const
{
    std::lock_guard<std::mutex> Lock(Mutex);
    auto Iterator = Textures.find(AssetGpuTextureKey{Key, bSrgb});
    if (Iterator == Textures.end())
    {
        return AssetGpuState::Absent;
    }
    return Iterator->second.State;
}

bool AssetGpuUploader::TryGetMesh(const AssetKey& Key, MeshHandle& OutMesh) const
{
    std::lock_guard<std::mutex> Lock(Mutex);
    auto Iterator = Meshes.find(Key);
    if (Iterator == Meshes.end() || Iterator->second.State != AssetGpuState::Resident)
    {
        return false;
    }
    OutMesh = Iterator->second.Mesh;
    return OutMesh.IsValid();
}

bool AssetGpuUploader::TryGetTexture(const AssetKey& Key, TextureHandle& OutTexture, bool bSrgb) const
{
    std::lock_guard<std::mutex> Lock(Mutex);
    auto Iterator = Textures.find(AssetGpuTextureKey{Key, bSrgb});
    if (Iterator == Textures.end() || Iterator->second.State != AssetGpuState::Resident)
    {
        return false;
    }
    OutTexture = Iterator->second.Texture;
    return OutTexture.IsValid();
}

void AssetGpuUploader::InvalidateAsset(const AssetId& Id)
{
    AssertGameThread();
    std::vector<MeshHandle> ReleasedMeshes;
    std::vector<TextureHandle> ReleasedTextures;
    std::vector<std::shared_ptr<const void>> ReleasedLifetimes;
    {
        std::lock_guard<std::mutex> Lock(Mutex);
        for (auto Iterator = Meshes.begin(); Iterator != Meshes.end();)
        {
            if (Iterator->first.Asset != Id)
            {
                ++Iterator;
                continue;
            }
            if (Iterator->second.Mesh.IsValid())
            {
                ReleasedMeshes.push_back(Iterator->second.Mesh);
            }
            Iterator = Meshes.erase(Iterator);
        }
        for (auto Iterator = Textures.begin(); Iterator != Textures.end();)
        {
            if (Iterator->first.Asset.Asset != Id)
            {
                ++Iterator;
                continue;
            }
            if (Iterator->second.Lifetime)
            {
                ReleasedLifetimes.push_back(Iterator->second.Lifetime);
            }
            else if (Iterator->second.Texture.IsValid())
            {
                ReleasedTextures.push_back(Iterator->second.Texture);
            }
            Iterator = Textures.erase(Iterator);
        }
    }

    RenderThread* Thread = RenderThread::Get();
    if (Thread != nullptr && Thread->IsReady() && (!ReleasedMeshes.empty() || !ReleasedTextures.empty()))
    {
        const uint64_t CompletionValue = Thread->GetRetirementValue();
        Thread->Enqueue([
            Thread,
            CompletionValue,
            ReleasedMeshes = std::move(ReleasedMeshes),
            ReleasedTextures = std::move(ReleasedTextures)]()
        {
            auto& Resources = Thread->GetRenderer()->GetResources();
            for (MeshHandle Mesh : ReleasedMeshes)
            {
                Resources.ReleaseMesh(Mesh, CompletionValue);
            }
            for (TextureHandle Texture : ReleasedTextures)
            {
                Resources.ReleaseTexture(Texture, CompletionValue);
            }
        });
    }
}

void AssetGpuUploader::Clear()
{
    for (const auto& Pair : Materials)
    {
        Pair.second->State.store(MaterialCompilationState::Cancelled);
    }
    Materials.clear();
    std::vector<MeshHandle> ReleasedMeshes;
    std::vector<TextureHandle> ReleasedTextures;
    std::vector<std::shared_ptr<const void>> ReleasedLifetimes;
    {
        std::lock_guard<std::mutex> Lock(Mutex);
        SessionId.fetch_add(1, std::memory_order_acq_rel);
        for (const auto& Entry : Meshes)
        {
            if (Entry.second.Mesh.IsValid())
            {
                ReleasedMeshes.push_back(Entry.second.Mesh);
            }
        }
        for (const auto& Entry : Textures)
        {
            if (Entry.second.Lifetime)
            {
                ReleasedLifetimes.push_back(Entry.second.Lifetime);
            }
            else if (Entry.second.Texture.IsValid())
            {
                ReleasedTextures.push_back(Entry.second.Texture);
            }
        }
        Meshes.clear();
        Textures.clear();
    }
    RenderThread* Thread = RenderThread::Get();
    if (Thread != nullptr && Thread->IsReady())
    {
        const uint64_t CompletionValue = Thread->GetRetirementValue();
        Thread->Enqueue([Thread, CompletionValue, ReleasedMeshes = std::move(ReleasedMeshes), ReleasedTextures = std::move(ReleasedTextures)]()
        {
            Thread->GetRenderer()->ClearMaterialResources();
            auto& Resources = Thread->GetRenderer()->GetResources();
            for (MeshHandle Mesh : ReleasedMeshes)
            {
                Resources.ReleaseMesh(Mesh, CompletionValue);
            }
            for (TextureHandle Texture : ReleasedTextures)
            {
                Resources.ReleaseTexture(Texture, CompletionValue);
            }
        });
    }
}

std::shared_ptr<const void> AssetGpuUploader::GetTextureLifetime(const AssetKey& Key, bool bSrgb) const
{
    std::lock_guard<std::mutex> Lock(Mutex);
    const auto Found = Textures.find(AssetGpuTextureKey{Key, bSrgb});
    if (Found != Textures.end())
    {
        return Found->second.Lifetime;
    }
    return {};
}
