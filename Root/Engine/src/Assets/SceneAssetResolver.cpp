#include "Assets/SceneAssetResolver.h"

#include "Assets/Resources/MaterialResource.h"
#include "Assets/Resources/StaticMeshResource.h"
#include "Assets/Resources/TextureResource.h"
#include "Core/Threading/ThreadContext.h"
#include "Game/Scene.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/MeshRendererComponent.h"

void SceneAssetResolver::Bind(
    AssetRegistry* Registry,
    AssetManager* Manager,
    AssetGpuUploader* Uploader)
{
    BoundRegistry = Registry;
    BoundManager = Manager;
    BoundUploader = Uploader;
}

void SceneAssetResolver::ResolveScene(Scene& TargetScene)
{
    AssertGameThread();
    if (BoundManager == nullptr || BoundUploader == nullptr)
    {
        return;
    }

    for (GameObject* ObjectInstance : TargetScene.GetAllObjects())
    {
        if (ObjectInstance == nullptr)
        {
            continue;
        }

        MeshRendererComponent* MeshRenderer = ObjectInstance->GetComponent<MeshRendererComponent>();
        if (MeshRenderer == nullptr || !MeshRenderer->IsEnabled())
        {
            continue;
        }

        const AssetKey MeshKey = MeshRenderer->GetMeshAssetKey();
        if (!MeshKey.IsValid())
        {
            if (!MeshRenderer->MeshAssetId.empty())
            {
                MeshRenderer->bMissingAsset = true;
                MeshRenderer->bPendingAsset = false;
                MeshRenderer->Mesh = MeshHandle{};
                PrintString(
                    std::string("SceneAssetResolver: invalid mesh asset id on '")
                    + ObjectInstance->GetName()
                    + "'");
            }
            continue;
        }

        if (BoundManager->GetLoadState(MeshKey) == AssetLoadState::Unloaded)
        {
            BoundManager->LoadAsync<StaticMeshResource>(MeshKey);
        }
        const AssetLoadState LoadState = BoundManager->GetLoadState(MeshKey);
        if (LoadState == AssetLoadState::Failed)
        {
            if (!MeshRenderer->bMissingAsset)
            {
                PrintString(
                    std::string("SceneAssetResolver: mesh load failed for '")
                    + ObjectInstance->GetName()
                    + "': "
                    + BoundManager->GetLastDiagnostic(MeshKey).Message);
            }
            MeshRenderer->bMissingAsset = true;
            MeshRenderer->bPendingAsset = false;
            MeshRenderer->Mesh = MeshHandle{};
            continue;
        }

        AssetHandle<StaticMeshResource> CpuMesh;
        if (!BoundManager->TryGetLoaded(MeshKey, CpuMesh) || !CpuMesh.IsValid())
        {
            MeshRenderer->bPendingAsset = true;
            MeshRenderer->bMissingAsset = false;
            continue;
        }

        BoundUploader->RequestUploadMesh(CpuMesh, MeshKey);
        MeshHandle GpuMesh{};
        if (BoundUploader->TryGetMesh(MeshKey, GpuMesh))
        {
            MeshRenderer->Mesh = GpuMesh;
            MeshRenderer->LocalBounds = CpuMesh->Bounds;
            MeshRenderer->bPendingAsset = false;
            MeshRenderer->bMissingAsset = false;
        }
        else if (BoundUploader->GetMeshState(MeshKey) == AssetGpuState::Failed)
        {
            MeshRenderer->bMissingAsset = true;
            MeshRenderer->bPendingAsset = false;
            MeshRenderer->Mesh = MeshHandle{};
            PrintString(
                std::string("SceneAssetResolver: mesh GPU upload failed for '")
                + ObjectInstance->GetName()
                + "'");
        }
        else
        {
            MeshRenderer->bPendingAsset = true;
            MeshRenderer->bMissingAsset = false;
        }

        MeshRenderer->BaseColorTexture = TextureHandle{};
        MeshRenderer->SurfaceMaterial = RenderMaterial{};
        MeshRenderer->BaseColorFactor = Color(1.f, 1.f, 1.f, 1.f);
        const AssetKey MaterialKey = MeshRenderer->GetMaterialAssetKey();
        if (!MaterialKey.IsValid())
        {
            continue;
        }

        BoundManager->LoadAsync<MaterialResource>(MaterialKey);
        AssetHandle<MaterialResource> CpuMaterial;
        if (!BoundManager->TryGetLoaded(MaterialKey, CpuMaterial) || !CpuMaterial.IsValid())
        {
            continue;
        }

        MeshRenderer->SurfaceMaterial = *CpuMaterial;
        MeshRenderer->BaseColorFactor = Color(
            CpuMaterial->BaseColor.x,
            CpuMaterial->BaseColor.y,
            CpuMaterial->BaseColor.z,
            CpuMaterial->BaseColor.w);

        const AssetKey TextureKey = CpuMaterial->BaseColorTexture.Key;
        if (!TextureKey.IsValid())
        {
            continue;
        }

        BoundManager->LoadAsync<TextureResource>(TextureKey);
        AssetHandle<TextureResource> CpuTexture;
        if (!BoundManager->TryGetLoaded(TextureKey, CpuTexture) || !CpuTexture.IsValid())
        {
            continue;
        }

        BoundUploader->RequestUploadTexture(CpuTexture, TextureKey);
        TextureHandle GpuTexture{};
        if (BoundUploader->TryGetTexture(TextureKey, GpuTexture))
        {
            MeshRenderer->BaseColorTexture = GpuTexture;
        }
    }
}
