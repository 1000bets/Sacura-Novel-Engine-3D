#pragma once

#include "Rendering/RenderCamera.h"
#include "Materials/MaterialBindings.h"
#include "Rendering/RenderLight.h"
#include "Rendering/RenderObject.h"

#include <vector>

struct RenderScene
{
    std::vector<RenderObject> Objects;
    std::vector<RenderLight> Lights;
    RenderCamera Camera;
    std::vector<RenderPostProcessEffect> PostProcessEffects;

    void Clear()
    {
        Objects.clear();
        PostProcessEffects.clear();
        Lights.clear();
        Camera = RenderCamera{};
    }
};
