#pragma once

#include "Rendering/RenderCamera.h"
#include "Rendering/RenderLight.h"
#include "Rendering/RenderObject.h"

#include <vector>

struct RenderScene
{
    std::vector<RenderObject> Objects;
    std::vector<RenderLight> Lights;
    RenderCamera Camera;

    void Clear()
    {
        Objects.clear();
        Lights.clear();
        Camera = RenderCamera{};
    }
};
