#include "Gameplay/MeshRendererComponent.h"

AssetKey MeshRendererComponent::GetMeshAssetKey() const
{
    AssetKey Key{};
    Guid AssetGuid{};
    if (!Guid::TryParse(MeshAssetId, AssetGuid) || !AssetGuid.IsValid())
    {
        return Key;
    }
    Key.Asset = AssetGuid;
    Guid SubGuid{};
    if (Guid::TryParse(MeshSubAssetId, SubGuid) && SubGuid.IsValid())
    {
        Key.SubAsset = SubGuid;
    }
    return Key;
}

AssetKey MeshRendererComponent::GetMaterialAssetKey() const
{
    AssetKey Key{};
    Guid AssetGuid{};
    if (!Guid::TryParse(MaterialAssetId, AssetGuid) || !AssetGuid.IsValid())
    {
        return Key;
    }
    Key.Asset = AssetGuid;
    return Key;
}

void MeshRendererComponent::SetMeshAssetKey(const AssetKey& Key)
{
    MeshAssetId = Key.Asset.IsValid() ? Key.Asset.ToString() : std::string{};
    MeshSubAssetId = (Key.SubAsset.has_value() && Key.SubAsset->IsValid())
        ? Key.SubAsset->ToString()
        : std::string{};
    Mesh = MeshHandle{};
    bMissingAsset = false;
    bPendingAsset = false;
}

void MeshRendererComponent::SetMaterialAssetKey(const AssetKey& Key)
{
    MaterialAssetId = Key.Asset.IsValid() ? Key.Asset.ToString() : std::string{};
    BaseColorTexture = TextureHandle{};
    bMissingAsset = false;
    bPendingAsset = false;
}
