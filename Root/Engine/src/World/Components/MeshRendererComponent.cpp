#include "World/Components/MeshRendererComponent.h"
#include "Core/Threading/ThreadContext.h"
#include <algorithm>

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
    DynamicMaterial.reset();
    MaterialSnapshot.reset();
}

AssetDiagnostic MeshRendererComponent::SetSlotMaterial(int32_t Slot, const AssetKey& Material)
{
    AssertGameThread();
    if (Slot < 0)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "MaterialSlot", "Material slot cannot be negative");
    }
    auto Existing = std::find_if(MaterialSlots.Entries.begin(), MaterialSlots.Entries.end(),
        [Slot](const MaterialSlotOverride& Entry) { return Entry.Slot == Slot; });
    if (Existing != MaterialSlots.Entries.end())
    {
        MaterialSlots.Entries.erase(Existing);
    }
    if (Material.IsValid())
    {
        MaterialSlots.Entries.push_back({Slot, Material});
    }
    DynamicMaterialSlots.erase(Slot);
    SlotSnapshots.erase(Slot);
    return AssetDiagnostic::Ok();
}

AssetDiagnostic MeshRendererComponent::SetDynamicMaterial(std::shared_ptr<DynamicMaterialInstance> Material, int32_t Slot)
{
    AssertGameThread();
    if (Slot < -1 || (Material && Material->GetResolvedMaterial().Definition.Domain != MaterialDomain::Surface))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::TypeMismatch, "DynamicMaterial", "A surface material and valid slot are required");
    }
    if (Slot == -1)
    {
        DynamicMaterial = std::move(Material);
        MaterialSnapshot.reset();
    }
    else if (Material)
    {
        DynamicMaterialSlots[Slot] = std::move(Material);
        SlotSnapshots.erase(Slot);
    }
    else
    {
        DynamicMaterialSlots.erase(Slot);
        SlotSnapshots.erase(Slot);
    }
    return AssetDiagnostic::Ok();
}
