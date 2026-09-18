#pragma once

#include "Assets/Guid.h"

#include <cstdint>
#include <optional>
#include <string>

enum class AssetType
{
    Unknown = 0,
    Model,
    Texture,
    Material,
    StaticMesh,
    SkeletalMesh,
    Skeleton,
    AnimationClip,
    SkinBinding
};

const char* AssetTypeToString(AssetType Type);
bool TryParseAssetType(const std::string& Text, AssetType& OutType);

struct AssetKey
{
    AssetId Asset{};
    std::optional<SubAssetId> SubAsset;

    bool IsValid() const { return Asset.IsValid(); }
    bool HasSubAsset() const { return SubAsset.has_value() && SubAsset->IsValid(); }

    bool operator==(const AssetKey& Other) const;
    bool operator!=(const AssetKey& Other) const;
};

struct AssetKeyHash
{
    size_t operator()(const AssetKey& Key) const;
};

enum class AssetErrorCode
{
    None = 0,
    NotFound,
    InvalidMetadata,
    DuplicateId,
    TypeMismatch,
    UnsupportedFormat,
    UnsupportedFeature,
    InvalidData,
    DependencyFailed,
    DependencyCycle,
    AssetChanged,
    ImportConflict,
    Cancelled,
    GpuUploadFailed,
    InternalError
};

struct AssetDiagnostic
{
    AssetErrorCode Code = AssetErrorCode::None;
    std::string Operation;
    AssetKey Key{};
    std::string Path;
    std::string Loader;
    std::string Message;

    bool HasError() const { return Code != AssetErrorCode::None; }
    static AssetDiagnostic Ok();
    static AssetDiagnostic Fail(
        AssetErrorCode Code,
        const std::string& Operation,
        const std::string& Message,
        const AssetKey& Key = {},
        const std::string& Path = {},
        const std::string& Loader = {});
};

const char* AssetErrorCodeToString(AssetErrorCode Code);
