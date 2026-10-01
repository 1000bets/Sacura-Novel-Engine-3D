#pragma once
#include "World/Components/Component.h"
#include "Reflection/ReflectionMacros.h"
#include <SimpleMath.h>

class ArrowComponent : public Component
{
    ENGINE_CLASS(ArrowComponent, Component, "engine.ArrowComponent", 1)
public:
    DirectX::SimpleMath::Color ArrowColor = DirectX::SimpleMath::Color(1.f, 0.25f, 0.1f, 1.f);
    float ArrowLength = 1.f;
    float ArrowThickness = 2.f;
    bool bVisible = true;
    ENGINE_REFLECT_FIELD(ArrowComponent, ArrowColor, "arrow_color", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_DISPLAY_NAME("Arrow Color"))
    ENGINE_REFLECT_FIELD(ArrowComponent, ArrowLength, "arrow_length", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_UI_RANGE(0.01, 10000.0), RF_DISPLAY_NAME("Arrow Length"))
    ENGINE_REFLECT_FIELD(ArrowComponent, ArrowThickness, "arrow_thickness", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_UI_RANGE(0.5, 20.0), RF_DISPLAY_NAME("Arrow Thickness"))
    ENGINE_REFLECT_FIELD(ArrowComponent, bVisible, "visible", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_DISPLAY_NAME("Visible"))
protected:
    ArrowComponent() = default;
};
ENGINE_CLASS_END(ArrowComponent)
ENGINE_IMPLEMENT_FIELD(ArrowComponent, ArrowColor)
ENGINE_IMPLEMENT_FIELD(ArrowComponent, ArrowLength)
ENGINE_IMPLEMENT_FIELD(ArrowComponent, ArrowThickness)
ENGINE_IMPLEMENT_FIELD(ArrowComponent, bVisible)
