#pragma once

#include "Rendering/RenderScene.h"

class Scene;

class SceneExtractor
{
public:
    void Extract(const Scene& SourceScene, RenderScene& Output) const;

private:
    struct ExtractionContext;
    void ExtractRenderableObjects(const Scene& SourceScene, RenderScene& Output, ExtractionContext& Context) const;
    void ExtractCamera(const Scene& SourceScene, RenderScene& Output, ExtractionContext& Context) const;
    void ExtractLights(const Scene& SourceScene, RenderScene& Output, ExtractionContext& Context) const;
};
