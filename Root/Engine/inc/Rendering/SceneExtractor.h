#pragma once

#include "Rendering/RenderScene.h"

class Scene;

class SceneExtractor
{
public:
    void Extract(const Scene& SourceScene, RenderScene& Output) const;

private:
    Matrix ComputeWorldMatrix(const class GameObject& Object) const;
    void ExtractRenderableObjects(const Scene& SourceScene, RenderScene& Output) const;
    void ExtractCamera(const Scene& SourceScene, RenderScene& Output) const;
    void ExtractLights(const Scene& SourceScene, RenderScene& Output) const;
};
