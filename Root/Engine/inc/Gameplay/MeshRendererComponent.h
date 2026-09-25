#pragma once

#include "Rendering/RenderMaterial.h"

#include "Assets/AssetTypes.h"
#include "Gameplay/Component.h"
#include "Reflection/ReflectionMacros.h"
#include "Rendering/AxisAlignedBounds.h"
#include "Rendering/RenderResourceHandles.h"

#include <SimpleMath.h>
#include <string>

using namespace DirectX::SimpleMath;

class MeshRendererComponent : public Component
{
    ENGINE_CLASS(MeshRendererComponent, Component, "engine.MeshRendererComponent", 1)

public:
    std::string MeshAssetId;
    std::string MeshSubAssetId;
    std::string MaterialAssetId;

    MeshHandle Mesh;
    MaterialHandle Material;
    TextureHandle BaseColorTexture;
    RenderMaterial SurfaceMaterial;
    Color BaseColorFactor = Color(1.f, 1.f, 1.f, 1.f);

    AxisAlignedBounds LocalBounds = AxisAlignedBounds::FromCenterExtents(
        Vector3::Zero,
        Vector3(0.5f, 0.5f, 0.5f));

    bool bVisible = true;
    bool bMissingAsset = false;
    bool bPendingAsset = false;

    AssetKey GetMeshAssetKey() const;
    AssetKey GetMaterialAssetKey() const;
    void SetMeshAssetKey(const AssetKey& Key);
    void SetMaterialAssetKey(const AssetKey& Key);

    std::string GetMeshAssetId() const { return MeshAssetId; }
    void SetMeshAssetId(std::string Value)
    {
        MeshAssetId = std::move(Value);
        Mesh = MeshHandle{};
        bMissingAsset = false;
        bPendingAsset = false;
    }
    std::string GetMeshSubAssetId() const { return MeshSubAssetId; }
    void SetMeshSubAssetId(std::string Value)
    {
        MeshSubAssetId = std::move(Value);
        Mesh = MeshHandle{};
        bMissingAsset = false;
        bPendingAsset = false;
    }
    std::string GetMaterialAssetId() const { return MaterialAssetId; }
    void SetMaterialAssetId(std::string Value)
    {
        MaterialAssetId = std::move(Value);
        BaseColorTexture = TextureHandle{};
    }
    bool GetVisible() const { return bVisible; }
    void SetVisible(bool Value) { bVisible = Value; }

    ENGINE_REFLECT_PROPERTY(
        MeshRendererComponent,
        MeshAssetIdProperty,
        "mesh_asset_id",
        std::string,
        GetMeshAssetId,
        SetMeshAssetId,
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_DISPLAY_NAME("Mesh"),
        RF_ASSET_TYPE("StaticMesh"),
        RF_COMPANION_PROPERTY("mesh_sub_asset_id"))

    ENGINE_REFLECT_PROPERTY(
        MeshRendererComponent,
        MeshSubAssetIdProperty,
        "mesh_sub_asset_id",
        std::string,
        GetMeshSubAssetId,
        SetMeshSubAssetId,
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_EDITOR_HIDDEN,
        RF_DISPLAY_NAME("Mesh SubAsset"))

    ENGINE_REFLECT_PROPERTY(
        MeshRendererComponent,
        MaterialAssetIdProperty,
        "material_asset_id",
        std::string,
        GetMaterialAssetId,
        SetMaterialAssetId,
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_DISPLAY_NAME("Material"),
        RF_ASSET_TYPE("Material"))

    ENGINE_REFLECT_PROPERTY(
        MeshRendererComponent,
        VisibleProperty,
        "visible",
        bool,
        GetVisible,
        SetVisible,
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_DISPLAY_NAME("Visible"))

protected:
    MeshRendererComponent() = default;
};

ENGINE_CLASS_END(MeshRendererComponent)
ENGINE_IMPLEMENT_PROPERTY(MeshRendererComponent, MeshAssetIdProperty, std::string, GetMeshAssetId, SetMeshAssetId)
ENGINE_IMPLEMENT_PROPERTY(MeshRendererComponent, MeshSubAssetIdProperty, std::string, GetMeshSubAssetId, SetMeshSubAssetId)
ENGINE_IMPLEMENT_PROPERTY(MeshRendererComponent, MaterialAssetIdProperty, std::string, GetMaterialAssetId, SetMaterialAssetId)
ENGINE_IMPLEMENT_PROPERTY(MeshRendererComponent, VisibleProperty, bool, GetVisible, SetVisible)
