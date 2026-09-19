#include "Assets/AssetRegistry.h"

#include "Core/Threading/ThreadContext.h"

#include <algorithm>
#include <cctype>

void AssetRegistry::SetContentRoot(const std::filesystem::path& InContentRoot)
{
    SetGameContentRoot(InContentRoot);
}

void AssetRegistry::SetGameContentRoot(const std::filesystem::path& InContentRoot)
{
    AssertGameThread();
    if (InContentRoot.empty())
    {
        GameContentRoot.clear();
        return;
    }
    GameContentRoot = std::filesystem::absolute(InContentRoot).lexically_normal();
}

void AssetRegistry::SetEngineContentRoot(const std::filesystem::path& InContentRoot)
{
    AssertGameThread();
    if (InContentRoot.empty())
    {
        EngineContentRoot.clear();
        return;
    }
    EngineContentRoot = std::filesystem::absolute(InContentRoot).lexically_normal();
}

void AssetRegistry::Clear()
{
    AssertGameThread();
    EntriesById.clear();
    IdsByVirtualPath.clear();
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

bool AssetRegistry::ResolvePathRoots(
    const std::string& RelativeOrVirtualPath,
    AssetMount& OutMount,
    std::string& OutRelativeInsideContent,
    std::filesystem::path& OutContentRoot) const
{
    if (!AssetPath::TryParseVirtualPath(RelativeOrVirtualPath, OutMount, OutRelativeInsideContent))
    {
        return false;
    }

    OutContentRoot = (OutMount == AssetMount::Engine) ? EngineContentRoot : GameContentRoot;
    return !OutContentRoot.empty();
}

AssetDiagnostic AssetRegistry::IngestMetaFile(AssetMount Mount, const std::filesystem::path& AbsoluteMetaPath)
{
    AssetMetadata Metadata{};
    AssetDiagnostic Error{};
    if (!AssetMetadataIO::TryLoadFromFile(AbsoluteMetaPath.string(), Metadata, Error))
    {
        return Error;
    }

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

    const std::filesystem::path& ContentRoot = (Mount == AssetMount::Engine) ? EngineContentRoot : GameContentRoot;
    std::string RelativePath;
    if (!AssetPath::TryMakeRelative(AbsoluteAssetPath, ContentRoot, RelativePath, Error))
    {
        return Error;
    }

    const std::string VirtualPath = AssetPath::MakeVirtualPath(Mount, RelativePath);

    if (EntriesById.find(Metadata.Guid) != EntriesById.end())
    {
        return AssetDiagnostic::Fail(
            AssetErrorCode::DuplicateId,
            "AssetRegistry",
            "Duplicate GUID detected during scan",
            {Metadata.Guid},
            VirtualPath);
    }

    if (IdsByVirtualPath.find(VirtualPath) != IdsByVirtualPath.end())
    {
        return AssetDiagnostic::Fail(
            AssetErrorCode::DuplicateId,
            "AssetRegistry",
            "Duplicate virtual path during scan",
            {Metadata.Guid},
            VirtualPath);
    }

    AssetRegistryEntry Entry{};
    Entry.Metadata = std::move(Metadata);
    Entry.Mount = Mount;
    Entry.RelativePath = RelativePath;
    Entry.VirtualPath = VirtualPath;
    Entry.AbsolutePath = AbsoluteAssetPath.string();
    Entry.AbsoluteMetaPath = AbsoluteMetaPath.string();
    Entry.bRegistered = true;

    IdsByVirtualPath[VirtualPath] = Entry.Metadata.Guid;
    EntriesById.emplace(Entry.Metadata.Guid, std::move(Entry));
    return AssetDiagnostic::Ok();
}

AssetDiagnostic AssetRegistry::ScanMount(AssetMount Mount, const std::filesystem::path& ContentRoot)
{
    if (ContentRoot.empty())
    {
        return AssetDiagnostic::Ok();
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

        AssetDiagnostic Diagnostic = IngestMetaFile(Mount, Entry.path());
        if (Diagnostic.HasError())
        {
            ScanDiagnostics.push_back(Diagnostic);
            PrintString(std::string("AssetRegistry scan: ") + AssetErrorCodeToString(Diagnostic.Code) + " — " + Diagnostic.Message + " @ " + Diagnostic.Path);
        }
    }

    return AssetDiagnostic::Ok();
}

AssetDiagnostic AssetRegistry::ScanContent()
{
    AssertGameThread();
    Clear();

    if (GameContentRoot.empty() && EngineContentRoot.empty())
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "AssetRegistry", "No content roots are set");
    }

    ScanMount(AssetMount::Engine, EngineContentRoot);
    ScanMount(AssetMount::Game, GameContentRoot);
    return AssetDiagnostic::Ok();
}

AssetDiagnostic AssetRegistry::RegisterExistingAsset(const std::string& RelativeOrVirtualPath, AssetMetadata Metadata)
{
    AssertGameThread();

    AssetMount Mount = AssetMount::Game;
    std::string RelativeInsideContent;
    std::filesystem::path ContentRoot;
    if (!ResolvePathRoots(RelativeOrVirtualPath, Mount, RelativeInsideContent, ContentRoot))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "AssetRegistry", "Content root is not set for mount", {Metadata.Guid}, RelativeOrVirtualPath);
    }

    const std::string Normalized = AssetPath::NormalizeRelative(RelativeInsideContent);
    const std::string VirtualPath = AssetPath::MakeVirtualPath(Mount, Normalized);
    const std::filesystem::path AbsolutePath = AssetPath::CombineContent(ContentRoot, Normalized);
    if (!AssetPath::IsInsideContent(AbsolutePath, ContentRoot))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "AssetRegistry", "Path outside Content", {Metadata.Guid}, VirtualPath);
    }
    if (!std::filesystem::exists(AbsolutePath))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::NotFound, "AssetRegistry", "Asset file not found", {Metadata.Guid}, VirtualPath);
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
        return AssetDiagnostic::Fail(AssetErrorCode::DuplicateId, "AssetRegistry", "GUID already registered", {Metadata.Guid}, VirtualPath);
    }
    if (IdsByVirtualPath.find(VirtualPath) != IdsByVirtualPath.end())
    {
        return AssetDiagnostic::Fail(AssetErrorCode::ImportConflict, "AssetRegistry", "Path already registered", {Metadata.Guid}, VirtualPath);
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
    Entry.Mount = Mount;
    Entry.RelativePath = Normalized;
    Entry.VirtualPath = VirtualPath;
    Entry.AbsolutePath = AbsolutePath.string();
    Entry.AbsoluteMetaPath = AbsoluteMeta.string();
    Entry.bRegistered = true;

    IdsByVirtualPath[VirtualPath] = Entry.Metadata.Guid;
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
    IdsByVirtualPath.erase(Found->second.VirtualPath);
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

bool AssetRegistry::TryGetByPath(const std::string& RelativeOrVirtualPath, AssetRegistryEntry& OutEntry) const
{
    AssetMount Mount = AssetMount::Game;
    std::string RelativeInsideContent;
    AssetPath::TryParseVirtualPath(RelativeOrVirtualPath, Mount, RelativeInsideContent);
    const std::string VirtualPath = AssetPath::MakeVirtualPath(Mount, RelativeInsideContent);

    auto FoundPath = IdsByVirtualPath.find(VirtualPath);
    if (FoundPath == IdsByVirtualPath.end())
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

bool AssetRegistry::PathExists(const std::string& RelativeOrVirtualPath) const
{
    AssetMount Mount = AssetMount::Game;
    std::string RelativeInsideContent;
    AssetPath::TryParseVirtualPath(RelativeOrVirtualPath, Mount, RelativeInsideContent);
    const std::string VirtualPath = AssetPath::MakeVirtualPath(Mount, RelativeInsideContent);
    return IdsByVirtualPath.find(VirtualPath) != IdsByVirtualPath.end();
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

std::vector<AssetRegistryEntry> AssetRegistry::FindByDirectory(const std::string& RelativeOrVirtualDirectory) const
{
    AssetMount Mount = AssetMount::Game;
    std::string RelativeInsideContent;
    AssetPath::TryParseVirtualPath(RelativeOrVirtualDirectory, Mount, RelativeInsideContent);
    const std::string Prefix = AssetPath::MakeVirtualPath(Mount, RelativeInsideContent);

    std::vector<AssetRegistryEntry> Result;
    for (const auto& Pair : EntriesById)
    {
        if (Prefix.empty() || Pair.second.VirtualPath.rfind(Prefix, 0) == 0)
        {
            Result.push_back(Pair.second);
        }
    }
    return Result;
}

AssetDiagnostic AssetRegistry::RenamePair(const std::string& OldRelativeOrVirtualPath, const std::string& NewRelativeOrVirtualPath)
{
    AssertGameThread();

    AssetRegistryEntry Entry{};
    if (!TryGetByPath(OldRelativeOrVirtualPath, Entry))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::NotFound, "AssetRegistry", "Source asset not found", {}, OldRelativeOrVirtualPath);
    }

    AssetMount NewMount = AssetMount::Game;
    std::string NewRelativeInsideContent;
    std::filesystem::path NewContentRoot;
    if (!ResolvePathRoots(NewRelativeOrVirtualPath, NewMount, NewRelativeInsideContent, NewContentRoot))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "AssetRegistry", "Destination content root is not set", {Entry.Metadata.Guid}, NewRelativeOrVirtualPath);
    }
    if (NewMount != Entry.Mount)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "AssetRegistry", "Cannot rename across mounts", {Entry.Metadata.Guid}, NewRelativeOrVirtualPath);
    }

    const std::string NewNormalized = AssetPath::NormalizeRelative(NewRelativeInsideContent);
    const std::string NewVirtualPath = AssetPath::MakeVirtualPath(NewMount, NewNormalized);
    if (PathExists(NewVirtualPath))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::ImportConflict, "AssetRegistry", "Destination path already registered", {Entry.Metadata.Guid}, NewVirtualPath);
    }

    const std::filesystem::path NewAbsolute = AssetPath::CombineContent(NewContentRoot, NewNormalized);
    const std::filesystem::path NewMetaAbsolute = AssetPath::CombineContent(NewContentRoot, AssetPath::MetaPathForAsset(NewNormalized));
    if (!AssetPath::IsInsideContent(NewAbsolute, NewContentRoot))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "AssetRegistry", "Destination outside Content", {Entry.Metadata.Guid}, NewVirtualPath);
    }

    std::error_code Error;
    std::filesystem::create_directories(NewAbsolute.parent_path(), Error);
    std::filesystem::rename(Entry.AbsolutePath, NewAbsolute, Error);
    if (Error)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "AssetRegistry", Error.message(), {Entry.Metadata.Guid}, NewVirtualPath);
    }
    std::filesystem::rename(Entry.AbsoluteMetaPath, NewMetaAbsolute, Error);
    if (Error)
    {
        std::filesystem::rename(NewAbsolute, Entry.AbsolutePath);
        return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "AssetRegistry", "Failed to rename .meta; data file rolled back", {Entry.Metadata.Guid}, NewVirtualPath);
    }

    IdsByVirtualPath.erase(Entry.VirtualPath);
    Entry.RelativePath = NewNormalized;
    Entry.VirtualPath = NewVirtualPath;
    Entry.AbsolutePath = NewAbsolute.string();
    Entry.AbsoluteMetaPath = NewMetaAbsolute.string();
    IdsByVirtualPath[NewVirtualPath] = Entry.Metadata.Guid;
    EntriesById[Entry.Metadata.Guid] = Entry;
    return AssetDiagnostic::Ok();
}
