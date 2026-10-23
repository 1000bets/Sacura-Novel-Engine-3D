#pragma once
#include "Assets/AssetMetadata.h"
#include <SimpleMath.h>
#include <map>
#include <vector>

struct StaticMeshCollisionElement
{
    int64_t ShapeType = 0;
    DirectX::SimpleMath::Vector3 Center = DirectX::SimpleMath::Vector3::Zero;
    DirectX::SimpleMath::Vector3 RotationDegrees = DirectX::SimpleMath::Vector3::Zero;
    DirectX::SimpleMath::Vector3 HalfExtents = {0.5f, 0.5f, 0.5f};
    float Radius = 0.5f;
    float CapsuleHalfHeight = 0.5f;
    nlohmann::json ToJson() const;
};

struct StaticMeshDefaults
{
    std::map<int32_t, AssetKey> Materials;
    bool bCollisionEnabled = false;
    std::vector<StaticMeshCollisionElement> CollisionElements{StaticMeshCollisionElement{}};
    nlohmann::json ToJson() const;
    static bool Parse(const nlohmann::json& Value, StaticMeshDefaults& Result, std::string& Error);
    static nlohmann::json Read(const AssetMetadata& Metadata, const AssetKey& Key);
};
