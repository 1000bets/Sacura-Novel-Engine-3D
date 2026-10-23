#pragma once

#include "RefCntAutoPtr.hpp"
#include "Texture.h"
#include "Rendering/RenderSky.h"

namespace Diligent
{
struct IRenderDevice;
}

class EnvironmentLighting
{
public:
    bool Initialize(Diligent::IRenderDevice* Device, const RenderSky* Sky = nullptr);
    Diligent::RefCntAutoPtr<Diligent::ITexture> Irradiance;
    Diligent::RefCntAutoPtr<Diligent::ITexture> Reflection;
    Diligent::RefCntAutoPtr<Diligent::ITexture> IntegratedBrdf;
};
