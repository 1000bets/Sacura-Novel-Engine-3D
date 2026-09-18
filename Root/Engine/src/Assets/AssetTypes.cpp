#include "Assets/AssetTypes.h"

const char* AssetTypeToString(AssetType Type)
{
    switch (Type)
    {
    case AssetType::Model: return "Model";
    case AssetType::Texture: return "Texture";
    case AssetType::Material: return "Material";
    case AssetType::StaticMesh: return "StaticMesh";
    case AssetType::SkeletalMesh: return "SkeletalMesh";
    case AssetType::Skeleton: return "Skeleton";
    case AssetType::AnimationClip: return "AnimationClip";
    case AssetType::SkinBinding: return "SkinBinding";
    default: return "Unknown";
    }
}

bool TryParseAssetType(const std::string& Text, AssetType& OutType)
{
    if (Text == "Model") { OutType = AssetType::Model; return true; }
    if (Text == "Texture") { OutType = AssetType::Texture; return true; }
    if (Text == "Material") { OutType = AssetType::Material; return true; }
    if (Text == "StaticMesh") { OutType = AssetType::StaticMesh; return true; }
    if (Text == "SkeletalMesh") { OutType = AssetType::SkeletalMesh; return true; }
    if (Text == "Skeleton") { OutType = AssetType::Skeleton; return true; }
    if (Text == "AnimationClip") { OutType = AssetType::AnimationClip; return true; }
    if (Text == "SkinBinding") { OutType = AssetType::SkinBinding; return true; }
    OutType = AssetType::Unknown;
    return false;
}

bool AssetKey::operator==(const AssetKey& Other) const
{
    return Asset == Other.Asset && SubAsset == Other.SubAsset;
}

bool AssetKey::operator!=(const AssetKey& Other) const
{
    return !(*this == Other);
}

size_t AssetKeyHash::operator()(const AssetKey& Key) const
{
    GuidHash HashGuid;
    size_t Hash = HashGuid(Key.Asset);
    if (Key.SubAsset.has_value())
    {
        Hash ^= HashGuid(*Key.SubAsset) + 0x9e3779b97f4a7c15ull + (Hash << 6) + (Hash >> 2);
    }
    return Hash;
}

AssetDiagnostic AssetDiagnostic::Ok()
{
    return {};
}

AssetDiagnostic AssetDiagnostic::Fail(
    AssetErrorCode Code,
    const std::string& Operation,
    const std::string& Message,
    const AssetKey& Key,
    const std::string& Path,
    const std::string& Loader)
{
    AssetDiagnostic Diagnostic{};
    Diagnostic.Code = Code;
    Diagnostic.Operation = Operation;
    Diagnostic.Message = Message;
    Diagnostic.Key = Key;
    Diagnostic.Path = Path;
    Diagnostic.Loader = Loader;
    return Diagnostic;
}

const char* AssetErrorCodeToString(AssetErrorCode Code)
{
    switch (Code)
    {
    case AssetErrorCode::None: return "None";
    case AssetErrorCode::NotFound: return "NotFound";
    case AssetErrorCode::InvalidMetadata: return "InvalidMetadata";
    case AssetErrorCode::DuplicateId: return "DuplicateId";
    case AssetErrorCode::TypeMismatch: return "TypeMismatch";
    case AssetErrorCode::UnsupportedFormat: return "UnsupportedFormat";
    case AssetErrorCode::UnsupportedFeature: return "UnsupportedFeature";
    case AssetErrorCode::InvalidData: return "InvalidData";
    case AssetErrorCode::DependencyFailed: return "DependencyFailed";
    case AssetErrorCode::DependencyCycle: return "DependencyCycle";
    case AssetErrorCode::AssetChanged: return "AssetChanged";
    case AssetErrorCode::ImportConflict: return "ImportConflict";
    case AssetErrorCode::Cancelled: return "Cancelled";
    case AssetErrorCode::GpuUploadFailed: return "GpuUploadFailed";
    case AssetErrorCode::InternalError: return "InternalError";
    default: return "Unknown";
    }
}
