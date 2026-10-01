#pragma once
#include "Shader.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"

void CreateEngineShader(Diligent::IRenderDevice* Device, const Diligent::ShaderCreateInfo& Description,
    Diligent::IShader** OutShader, Diligent::IDataBlob** OutErrors = nullptr);
