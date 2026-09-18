#include "Assets/AssetRegistry.h"

#include "Core/Threading/ThreadContext.h"

#include <algorithm>
#include <cctype>

void AssetRegistry::SetContentRoot(const std::filesystem::path& InContentRoot)
{
    AssertGameThread();
    ContentRoot = std::filesystem::absolute(InContentRoot).lexically_normal();
}

void AssetRegistry::Clear()
{
    AssertGameThread();
    EntriesById.clear();
    IdsByRelativePath.clear();
    ScanDiagnostics.clear();
}

AssetType AssetRegistry::InferTypeFromExtension(const std::string& RelativePath)
{
    const std::filesystem::path Path(RelativePath);
    std::string Extension = Path.extension().string();
    std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char Character)
    {
        return static_cast<char>(std::tolower(Character));
    });

    if (Extension == ".glb")
    {
        return AssetType::Model;
    }
    if (Extension == ".png" || Extension == ".jpg" || Extension == ".jpeg")
    {
        return AssetType::Texture;
    }
    if (Extension == ".material")
    {
        return AssetType::Material;
    }
    return AssetType::Unknown;
}

AssetDiagnostic AssetRegistry::IngestMetaFile(const std::filesystem::path& AbsoluteMetaPath)
{
    AssetMetadata Metadata{};
    AssetDiagnostic Error{};
    if (!AssetMetadataIO::TryLoadFromFile(AbsoluteMetaPath.string(), Metadata, Error))
    {
        return Error;
    }

    // AbsoluteMetaPath = Content/x/y.glb.meta → asset = Content/x/y.glb
    std::string AbsoluteMetaString = AbsoluteMetaPath.generic_string();
    std::filesystem::path AbsoluteAssetPath = AbsoluteMetaPath;
    if (AbsoluteMetaString.size() >= 5 && AbsoluteMetaString.compare(AbsoluteMetaString.size() - 5, 5, ".meta") == 0)
    {
        AbsoluteAssetPath = AbsoluteMetaString.substr(0, AbsoluteMetaString.size() - 5);
    }

    if (!std::filesystem::exists(AbsoluteAssetPath))
    {
        return AssetDiagnostic::Fail(
            AssetErrorCode::NotFound,
            "AssetRegistry",
            "Data file missing for .meta",
            {Metadata.Guid},
            AbsoluteAssetPath.string());
    }

    std::string RelativePath;
    if (!AssetPath::TryMakeRelative(AbsoluteAssetPath, ContentRoot, RelativePath, Error))
    {
        return Error;
    }

    if (EntriesById.find(Metadata.Guid) != EntriesById.end())
    {
        return AssetDiagnostic::Fail(
            AssetErrorCode::DuplicateId,
            "AssetRegistry",
            "Duplicate GUID detected during scan",
            {Metadata.Guid},
            RelativePath);
    }

    if (IdsByRelativePath.find(RelativePath) != IdsByRelativePath.end())
    {
        return AssetDiagnostic::Fail(
            AssetErrorCode::DuplicateId,
            "AssetRegistry",
            "Duplicate relative path during scan",
            {Metadata.Guid},
            RelativePath);
    }

    AssetRegistryEntry Entry{};
    Entry.Metadata = std::move(Metadata);
    Entry.RelativePath = RelativePath;
    Entry.AbsolutePath = AbsoluteAssetPath.string();
    Entry.AbsoluteMetaPath = AbsoluteMetaPath.string();
    Entry.bRegistered = true;

    IdsByRelativePath[RelativePath] = Entry.Metadata.Guid;
    EntriesById.emplace(Entry.Metadata.Guid, std::move(Entry));
    return AssetDiagnostic::Ok();
}

AssetDiagnostic AssetRegistry::ScanContent()
{
    AssertGameThread();
    Clear();

    if (ContentRoot.empty())
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "AssetRegistry", "Content root is not set");
    }

    std::error_code Error;
    if (!std::filesystem::exists(ContentRoot, Error))
    {
        std::filesystem::create_directories(ContentRoot, Error);
    }

    for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(ContentRoot, Error))
    {
        if (Error)
        {
            break;
        }
        if (!Entry.is_regular_file())
        {
            continue;
        }

        const std::string PathString = Entry.path().generic_string();
        if (!AssetPath::HasMetaSuffix(PathString))
        {
            continue;
        }

        AssetDiagnostic Diagnostic = IngestMetaFile(Entry.path());
        if (Diagnostic.HasError())
        {
            ScanDiagnostics.push_back(Diagnostic);
            PrintString(std::string("AssetRegistry scan: ") + AssetErrorCodeToString(Diagnostic.Code) + " — " + Diagnostic.Message + " @ " + Diagnostic.Path);
        }
    }

    return AssetDiagnostic::Ok();
}

AssetDiagnostic AssetRegistry::RegisterExistingAsset(const std::string& RelativePath, AssetMetadata Metadata)
{
    AssertGameThread();

    const std::string Normalized = AssetPath::NormalizeRelative(RelativePath);
    const std::filesystem::path AbsolutePath = AssetPath::CombineContent(ContentRoot, Normalized);
    if (!AssetPath::IsInsideContent(AbsolutePath, ContentRoot))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "AssetRegistry", "Path outside Content", {Metadata.Guid}, Normalized);
    }
    if (!std::filesystem::exists(AbsolutePath))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::NotFound, "AssetRegistry", "Asset file not found", {Metadata.Guid}, Normalized);
    }
    if (!Metadata.Guid.IsValid())
    {
        Metadata.Guid = Guid::Generate();
    }
    if (Metadata.Type == AssetType::Unknown)
    {
        Metadata.Type = InferTypeFromExtension(Normalized);
    }
    if (EntriesById.find(Metadata.Guid) != EntriesById.end())
    {
        return AssetDiagnostic::Fail(AssetErrorCode::DuplicateId, "AssetRegistry", "GUID already registered", {Metadata.Guid}, Normalized);
    }
    if (IdsByRelativePath.find(Normalized) != IdsByRelativePath.end())
    {
        return AssetDiagnostic::Fail(AssetErrorCode::ImportConflict, "AssetRegistry", "Path already registered", {Metadata.Guid}, Normalized);
    }

    const std::string MetaRelative = AssetPath::MetaPathForAsset(Normalized);
    const std::filesystem::path AbsoluteMeta = AssetPath::CombineContent(ContentRoot, MetaRelative);

    AssetDiagnostic Error{};
    if (!AssetMetadataIO::TrySaveToFile(AbsoluteMeta.string(), Metadata, Error))
    {
        return Error;
    }

    AssetRegistryEntry Entry{};
    Entry.Metadata = std::move(Metadata);
    Entry.RelativePath = Normalized;
    Entry.AbsolutePath = AbsolutePath.string();
    Entry.AbsoluteMetaPath = AbsoluteMeta.string();
    Entry.bRegistered = true;

    IdsByRelativePath[Normalized] = Entry.Metadata.Guid;
    EntriesById.emplace(Entry.Metadata.Guid, std::move(Entry));
    return AssetDiagnostic::Ok();
}

AssetDiagnostic AssetRegistry::Unregister(const AssetId& Id)
{
    AssertGameThread();
    auto Found = EntriesById.find(Id);
    if (Found == EntriesById.end())
    {
        return AssetDiagnostic::Fail(AssetErrorCode::NotFound, "AssetRegistry", "Asset not registered", {Id});
    }
    IdsByRelativePath.erase(Found->second.RelativePath);
    EntriesById.erase(Found);
    return AssetDiagnostic::Ok();
}

bool AssetRegistry::TryGetById(const AssetId& Id, AssetRegistryEntry& OutEntry) const
{
    auto Found = EntriesById.find(Id);
    if (Found == EntriesById.end())
    {
        return false;
    }
    OutEntry = Found->second;
    return true;
}

bool AssetRegistry::TryGetByPath(const std::string& RelativePath, AssetRegistryEntry& OutEntry) const
{
    const std::string Normalized = AssetPath::NormalizeRelative(RelativePath);
    auto FoundPath = IdsByRelativePath.find(Normalized);
    if (FoundPath == IdsByRelativePath.end())
    {
        return false;
    }
    return TryGetById(FoundPath->second, OutEntry);
}

bool AssetRegistry::TryResolveKey(const AssetKey& Key, AssetRegistryEntry& OutEntry, const SubAssetRecord** OutSubAsset) const
{
    if (!TryGetById(Key.Asset, OutEntry))
    {
        return false;
    }
    if (OutSubAsset != nullptr)
    {
        *OutSubAsset = nullptr;
    }
    if (!Key.HasSubAsset())
    {
        return true;
    }
    for (const SubAssetRecord& Record : OutEntry.Metadata.SubAssets)
    {
        if (Record.Id == *Key.SubAsset)
        {
            if (OutSubAsset != nullptr)
            {
                *OutSubAsset = &Record;
            }
            return true;
        }
    }
    return false;
}

bool AssetRegistry::Exists(const AssetId& Id) const
{
    return EntriesById.find(Id) != EntriesById.end();
}

bool AssetRegistry::PathExists(const std::string& RelativePath) const
{
    return IdsByRelativePath.find(AssetPath::NormalizeRelative(RelativePath)) != IdsByRelativePath.end();
}

std::vector<AssetRegistryEntry> AssetRegistry::FindByType(AssetType Type) const
{
    std::vector<AssetRegistryEntry> Result;
    for (const auto& Pair : EntriesById)
    {
        if (Pair.second.Metadata.Type == Type)
        {
            Result.push_back(Pair.second);
        }
    }
    return Result;
}

std::vector<AssetRegistryEntry> AssetRegistry::FindByDirectory(const std::string& RelativeDirectory) const
{
    const std::string Prefix = AssetPath::NormalizeRelative(RelativeDirectory);
    std::vector<AssetRegistryEntry> Result;
    for (const auto& Pair : EntriesById)
    {
        if (Prefix.empty() || Pair.second.RelativePath.rfind(Prefix, 0) == 0)
        {
            Result.push_back(Pair.second);
        }
    }
    return Result;
}

AssetDiagnostic AssetRegistry::RenamePair(const std::string& OldRelativePath, const std::string& NewRelativePath)
{
    AssertGameThread();

    AssetRegistryEntry Entry{};
    if (!TryGetByPath(OldRelativePath, Entry))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::NotFound, "AssetRegistry", "Source asset not found", {}, OldRelativePath);
    }

    const std::string NewNormalized = AssetPath::NormalizeRelative(NewRelativePath);
    if (PathExists(NewNormalized))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::ImportConflict, "AssetRegistry", "Destination path already registered", {Entry.Metadata.Guid}, NewNormalized);
    }

    const std::filesystem::path NewAbsolute = AssetPath::CombineContent(ContentRoot, NewNormalized);
    const std::filesystem::path NewMetaAbsolute = AssetPath::CombineContent(ContentRoot, AssetPath::MetaPathForAsset(NewNormalized));
    if (!AssetPath::IsInsideContent(NewAbsolute, ContentRoot))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "AssetRegistry", "Destination outside Content", {Entry.Metadata.Guid}, NewNormalized);
    }

    std::error_code Error;
    std::filesystem::create_directories(NewAbsolute.parent_path(), Error);
    std::filesystem::rename(Entry.AbsolutePath, NewAbsolute, Error);
    if (Error)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "AssetRegistry", Error.message(), {Entry.Metadata.Guid}, NewNormalized);
    }
    std::filesystem::rename(Entry.AbsoluteMetaPath, NewMetaAbsolute, Error);
    if (Error)
    {
        std::filesystem::rename(NewAbsolute, Entry.AbsolutePath);
        return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "AssetRegistry", "Failed to rename .meta; data file rolled back", {Entry.Metadata.Guid}, NewNormalized);
    }

    IdsByRelativePath.erase(Entry.RelativePath);
    Entry.RelativePath = NewNormalized;
    Entry.AbsolutePath = NewAbsolute.string();
    Entry.AbsoluteMetaPath = NewMetaAbsolute.string();
    IdsByRelativePath[NewNormalized] = Entry.Metadata.Guid;
    EntriesById[Entry.Metadata.Guid] = Entry;
    return AssetDiagnostic::Ok();
}
