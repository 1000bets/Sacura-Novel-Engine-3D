#pragma once

#include "Assets/AssetTypes.h"
#include "Assets/ContentHash.h"

#include <SimpleMath.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct StaticMeshResource;
struct MaterialResource;
struct TextureResource;

struct ModelNode
{
    std::string Name;
    DirectX::SimpleMath::Matrix LocalTransform = DirectX::SimpleMath::Matrix::Identity;
    int32_t ParentIndex = -1;
    int32_t MeshIndex = -1;
    int32_t MaterialIndex = -1;
};

struct ModelMeshPart
{
    std::string Name;
    std::shared_ptr<const StaticMeshResource> Mesh;
    int32_t MaterialIndex = -1;
};

struct ModelDocument
{
    ContentHash Fingerprint{};
    std::vector<uint8_t> BackingBytes;
    std::vector<ModelNode> Nodes;
    std::vector<ModelMeshPart> Meshes;
    std::vector<std::shared_ptr<const MaterialResource>> Materials;
    std::vector<std::shared_ptr<const TextureResource>> EmbeddedTextures;
};

struct ModelResource
{
    std::shared_ptr<const ModelDocument> Document;
};
