#pragma once
#include "Materials/DynamicMaterialInstance.h"

struct MaterialWidgetBinding
{
    AssetKey Asset;
    std::shared_ptr<DynamicMaterialInstance> Dynamic;
    bool bVisible = false;
    bool bPreview = false;
};
