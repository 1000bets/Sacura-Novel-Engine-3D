#pragma once

#include "Assets/Guid.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

class AssetType
{
public:
    AssetType() = default;
    explicit AssetType(std::string Identifier)
        : Value(std::move(Identifier))
    {
    }

    bool IsValid() const { return !Value.empty(); }
    const std::string& GetIdentifier() const { return Value; }

    bool operator==(const AssetType& Other) const { return Value == Other.Value; }
    bool operator!=(const AssetType& Other) const { return Value != Other.Value; }
    bool operator<(const AssetType& Other) const { return Value < Other.Value; }

private:
    std::string Value;
};

struct AssetTypeHash
{
    size_t operator()(const AssetType& Type) const;
};

inline const AssetType UnknownAssetType{};
inline const AssetType SceneAssetType{"Scene"};
inline const AssetType StoryAssetType{"Story"};
inline const AssetType ModelAssetType{"Model"};
inline const AssetType TextureAssetType{"Texture"};
inline const AssetType MaterialAssetType{"Material"};
inline const AssetType StaticMeshAssetType{"StaticMesh"};
inline const AssetType SkeletalMeshAssetType{"SkeletalMesh"};
inline const AssetType SkeletonAssetType{"Skeleton"};
inline const AssetType AnimationClipAssetType{"AnimationClip"};
inline const AssetType SkinBindingAssetType{"SkinBinding"};

const char* AssetTypeToString(const AssetType& Type);
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
