#pragma once

#include "Gameplay/Component.h"
#include "Rendering/AxisAlignedBounds.h"
#include "Rendering/RenderResourceHandles.h"

class MeshRendererComponent : public Component
{
    SAKURA_OBJECT(MeshRendererComponent)

public:
    MeshHandle Mesh;
    MaterialHandle Material;
    AxisAlignedBounds LocalBounds = AxisAlignedBounds::FromCenterExtents(
        Vector3::Zero,
        Vector3(0.5f, 0.5f, 0.5f));

    bool bVisible = true;

protected:
    MeshRendererComponent() = default;
};
