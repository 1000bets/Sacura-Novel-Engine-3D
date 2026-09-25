#pragma once

#include "Assets/AssetGpuUploader.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetRegistry.h"

class Scene;

class SceneAssetResolver
{
public:
    void Bind(
        AssetRegistry* Registry,
        AssetManager* Manager,
        AssetGpuUploader* Uploader);

    void ResolveScene(Scene& TargetScene);

private:
    AssetRegistry* BoundRegistry = nullptr;
    AssetManager* BoundManager = nullptr;
    AssetGpuUploader* BoundUploader = nullptr;
};
