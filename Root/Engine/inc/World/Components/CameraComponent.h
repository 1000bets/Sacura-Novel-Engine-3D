#pragma once

#include "World/Components/Component.h"
#include "Reflection/ReflectionMacros.h"
#include "Materials/MaterialBindings.h"

class CameraComponent : public Component
{
    ENGINE_CLASS(CameraComponent, Component, "engine.CameraComponent", 1)

public:
    std::string MaterialDiagnostic;
    std::string GetMaterialDiagnostic() const { return MaterialDiagnostic; }
    ENGINE_REFLECT_READONLY(CameraComponent, MaterialDiagnosticProperty, "material_diagnostic",
        std::string, GetMaterialDiagnostic, RF_EDITOR_READONLY, RF_DISPLAY_NAME("Material Diagnostic"))
    float FieldOfViewDegrees = 60.f;
    float NearPlane = 0.1f;
    float FarPlane = 1000.f;
    float AspectRatio = 16.f / 9.f;
    bool bPrimary = true;
    MaterialPostProcessEffects PostProcessEffects;

    ENGINE_REFLECT_FIELD(CameraComponent, PostProcessEffects, "post_process_effects",
        RF_SERIALIZABLE, RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Postprocess Effects"))

    float GetFieldOfViewDegrees() const { return FieldOfViewDegrees; }
    void SetFieldOfViewDegrees(float Value) { FieldOfViewDegrees = Value; }

    ENGINE_REFLECT_PROPERTY(
        CameraComponent,
        FieldOfView,
        "field_of_view",
        float,
        GetFieldOfViewDegrees,
        SetFieldOfViewDegrees,
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE,
        RF_UI_RANGE(1.0, 179.0),
        RF_DISPLAY_NAME("Field Of View"))

    ENGINE_REFLECT_FIELD(
        CameraComponent,
        NearPlane,
        "near_plane",
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE,
        RF_DISPLAY_NAME("Near Plane"))

    ENGINE_REFLECT_FIELD(
        CameraComponent,
        FarPlane,
        "far_plane",
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE,
        RF_DISPLAY_NAME("Far Plane"))

    ENGINE_REFLECT_FIELD(
        CameraComponent,
        AspectRatio,
        "aspect_ratio",
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE,
        RF_DISPLAY_NAME("Aspect Ratio"))

    ENGINE_REFLECT_FIELD(
        CameraComponent,
        bPrimary,
        "primary",
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE,
        RF_DISPLAY_NAME("Primary"))

protected:
    CameraComponent() = default;
};

ENGINE_CLASS_END(CameraComponent)
ENGINE_IMPLEMENT_PROPERTY(CameraComponent, FieldOfView, float, GetFieldOfViewDegrees, SetFieldOfViewDegrees)
ENGINE_IMPLEMENT_FIELD(CameraComponent, NearPlane)
ENGINE_IMPLEMENT_FIELD(CameraComponent, FarPlane)
ENGINE_IMPLEMENT_FIELD(CameraComponent, AspectRatio)
ENGINE_IMPLEMENT_FIELD(CameraComponent, bPrimary)
ENGINE_IMPLEMENT_FIELD(CameraComponent, PostProcessEffects)

ENGINE_IMPLEMENT_READONLY(CameraComponent, MaterialDiagnosticProperty, std::string, GetMaterialDiagnostic)
