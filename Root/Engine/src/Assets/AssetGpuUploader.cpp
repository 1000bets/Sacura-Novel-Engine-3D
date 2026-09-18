#include "Assets/AssetGpuUploader.h"
#include "Core/Threading/RenderThread.h"
#include "Core/Threading/ThreadContext.h"
#include "Rendering/Renderer.h"
#include "Rendering/Resources/RenderResourceManager.h"

void AssetGpuUploader::RequestUploadMesh(AssetHandle<StaticMeshResource> Mesh, const AssetKey& Key)
{
    AssertGameThread();
    if (!Mesh.IsValid() || !Key.IsValid())
    {
        return;
    }

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
    }

    RenderThread* Thread = RenderThread::Get();
    if (Thread == nullptr)
    {
        std::lock_guard<std::mutex> Lock(Mutex);
        AssetGpuMeshEntry& Entry = Meshes[Key];
        Entry.State = AssetGpuState::Failed;
        Entry.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::GpuUploadFailed,
            "AssetGpuUploader",
            "RenderThread is not available",
            Key);
        return;
    }

    Thread->Enqueue([this, Mesh, Key]()
    {
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
        AssetGpuMeshEntry& Entry = Meshes[Key];
        if (Created.IsValid())
        {
            Entry.State = AssetGpuState::Resident;
            Entry.Mesh = Created;
            Entry.Diagnostic = AssetDiagnostic::Ok();
        }
        else
        {
            Entry.State = AssetGpuState::Failed;
            Entry.Diagnostic = Diagnostic;
        }
    });
}

void AssetGpuUploader::RequestUploadTexture(AssetHandle<TextureResource> Texture, const AssetKey& Key)
{
    AssertGameThread();
    if (!Texture.IsValid() || !Key.IsValid())
    {
        return;
    }

    {
        std::lock_guard<std::mutex> Lock(Mutex);
        AssetGpuTextureEntry& Entry = Textures[Key];
        if (Entry.State == AssetGpuState::UploadQueued || Entry.State == AssetGpuState::Resident)
        {
            return;
        }
        Entry.State = AssetGpuState::UploadQueued;
        Entry.Diagnostic = AssetDiagnostic::Ok();
        Entry.Texture = TextureHandle{};
    }

    RenderThread* Thread = RenderThread::Get();
    if (Thread == nullptr)
    {
        std::lock_guard<std::mutex> Lock(Mutex);
        AssetGpuTextureEntry& Entry = Textures[Key];
        Entry.State = AssetGpuState::Failed;
        Entry.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::GpuUploadFailed,
            "AssetGpuUploader",
            "RenderThread is not available",
            Key);
        return;
    }

    Thread->Enqueue([this, Texture, Key]()
    {
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
            Created = OwnedRenderer->GetResources().CreateTextureFromCpuData(*Texture);
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
        AssetGpuTextureEntry& Entry = Textures[Key];
        if (Created.IsValid())
        {
            Entry.State = AssetGpuState::Resident;
            Entry.Texture = Created;
            Entry.Diagnostic = AssetDiagnostic::Ok();
        }
        else
        {
            Entry.State = AssetGpuState::Failed;
            Entry.Diagnostic = Diagnostic;
        }
    });
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

AssetGpuState AssetGpuUploader::GetTextureState(const AssetKey& Key) const
{
    std::lock_guard<std::mutex> Lock(Mutex);
    auto Iterator = Textures.find(Key);
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

bool AssetGpuUploader::TryGetTexture(const AssetKey& Key, TextureHandle& OutTexture) const
{
    std::lock_guard<std::mutex> Lock(Mutex);
    auto Iterator = Textures.find(Key);
    if (Iterator == Textures.end() || Iterator->second.State != AssetGpuState::Resident)
    {
        return false;
    }
    OutTexture = Iterator->second.Texture;
    return OutTexture.IsValid();
}

void AssetGpuUploader::Clear()
{
    std::lock_guard<std::mutex> Lock(Mutex);
    Meshes.clear();
    Textures.clear();
}
