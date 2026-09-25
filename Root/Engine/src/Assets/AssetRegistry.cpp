#include "Assets/AssetRegistry.h"

#include "Assets/ContentHash.h"
#include "Core/Threading/ThreadContext.h"

#include <algorithm>
#include <cctype>

AssetRegistry::AssetRegistry()
{
    RegisterAssetType({SceneAssetType, "Scene", {".scene"}, true});
    RegisterAssetType({StoryAssetType, "Story", {".story"}, true});
    RegisterAssetType({ModelAssetType, "Model", {".glb"}, false});
    RegisterAssetType({TextureAssetType, "Texture", {".png", ".jpg", ".jpeg"}, true});
    RegisterAssetType({MaterialAssetType, "Material", {".material"}, true});
    RegisterAssetType({StaticMeshAssetType, "Static Mesh", {}, true});
    RegisterAssetType({SkeletalMeshAssetType, "Skeletal Mesh", {}, true});
    RegisterAssetType({SkeletonAssetType, "Skeleton", {}, true});
    RegisterAssetType({AnimationClipAssetType, "Animation Clip", {}, true});
    RegisterAssetType({SkinBindingAssetType, "Skin Binding", {}, false});
}

AssetDiagnostic AssetRegistry::RegisterAssetType(const AssetTypeRegistration& Registration)
{
    if (!Registration.Type.IsValid() || Registration.DisplayName.empty())
    {
        return AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "AssetRegistry",
            "Asset type registration requires an identifier and display name");
    }

    const std::string& Identifier = Registration.Type.GetIdentifier();
    if (TypeRegistrations.find(Identifier) != TypeRegistrations.end())
    {
        return AssetDiagnostic::Fail(
            AssetErrorCode::ImportConflict,
            "AssetRegistry",
            "Asset type is already registered: " + Identifier);
    }

    AssetTypeRegistration Normalized = Registration;
    for (std::string& Extension : Normalized.Extensions)
    {
        std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char Character)
        {
            return static_cast<char>(std::tolower(Character));
        });
        if (Extension.empty() || Extension.front() != '.')
        {
            Extension.insert(Extension.begin(), '.');
        }
        if (TypesByExtension.find(Extension) != TypesByExtension.end())
        {
            return AssetDiagnostic::Fail(
                AssetErrorCode::ImportConflict,
                "AssetRegistry",
                "Asset extension is already registered: " + Extension);
        }
    }

    TypeRegistrations.emplace(Identifier, Normalized);
    for (const std::string& Extension : Normalized.Extensions)
    {
        TypesByExtension.emplace(Extension, Normalized.Type);
    }
    return AssetDiagnostic::Ok();
}

bool AssetRegistry::TryGetAssetTypeRegistration(
    const AssetType& Type,
    AssetTypeRegistration& OutRegistration) const
{
    auto Found = TypeRegistrations.find(Type.GetIdentifier());
    if (Found == TypeRegistrations.end())
    {
        return false;
    }
    OutRegistration = Found->second;
    return true;
}

std::vector<AssetTypeRegistration> AssetRegistry::GetAssetTypeRegistrations() const
{
    std::vector<AssetTypeRegistration> Registrations;
    Registrations.reserve(TypeRegistrations.size());
    for (const auto& Pair : TypeRegistrations)
    {
        Registrations.push_back(Pair.second);
    }
    std::sort(Registrations.begin(), Registrations.end(), [](const AssetTypeRegistration& First, const AssetTypeRegistration& Second)
    {
        return First.DisplayName < Second.DisplayName;
    });
    return Registrations;
}

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

AssetType AssetRegistry::InferTypeFromExtension(const std::string& RelativePath) const
{
    const std::filesystem::path Path(RelativePath);
    std::string Extension = Path.extension().string();
    std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char Character)
    {
        return static_cast<char>(std::tolower(Character));
    });

    auto Found = TypesByExtension.find(Extension);
    return Found != TypesByExtension.end() ? Found->second : UnknownAssetType;
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
    if (TypeRegistrations.find(Metadata.Type.GetIdentifier()) == TypeRegistrations.end())
    {
        return AssetDiagnostic::Fail(
            AssetErrorCode::UnsupportedFormat,
            "AssetRegistry",
            "Asset type is not registered: " + Metadata.Type.GetIdentifier(),
            {Metadata.Guid},
            AbsoluteAssetPath.generic_string());
    }
    for (const SubAssetRecord& Record : Metadata.SubAssets)
    {
        if (TypeRegistrations.find(Record.Type.GetIdentifier()) == TypeRegistrations.end())
        {
            return AssetDiagnostic::Fail(
                AssetErrorCode::UnsupportedFormat,
                "AssetRegistry",
                "Subasset type is not registered: " + Record.Type.GetIdentifier(),
                {Metadata.Guid},
                AbsoluteAssetPath.generic_string());
        }
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

    if (Mount == AssetMount::Game && Metadata.SubAssets.empty()
        && LeafNameExists(AbsoluteAssetPath.stem().string()))
    {
        return AssetDiagnostic::Fail(
            AssetErrorCode::ImportConflict,
            "AssetRegistry",
            "Asset name already exists in project",
            {Metadata.Guid},
            VirtualPath);
    }
    if (Mount == AssetMount::Game && !Metadata.SubAssets.empty())
    {
        std::vector<std::string> IncomingNames;
        for (const SubAssetRecord& Record : Metadata.SubAssets)
        {
            std::string NormalizedName = Record.Name;
            std::transform(NormalizedName.begin(), NormalizedName.end(), NormalizedName.begin(), [](unsigned char Character)
            {
                return static_cast<char>(std::tolower(Character));
            });
            if (Record.Name.empty()
                || LeafNameExists(Record.Name)
                || std::find(IncomingNames.begin(), IncomingNames.end(), NormalizedName) != IncomingNames.end())
            {
                return AssetDiagnostic::Fail(
                    AssetErrorCode::ImportConflict,
                    "AssetRegistry",
                    "Subasset name must be unique in project",
                    {Metadata.Guid},
                    VirtualPath);
            }
            IncomingNames.push_back(std::move(NormalizedName));
        }
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
        if (Error)
        {
            return AssetDiagnostic::Fail(
                AssetErrorCode::InternalError,
                "AssetRegistry",
                "Failed to create content root: " + Error.message(),
                {},
                ContentRoot.generic_string());
        }
    }

    std::vector<std::filesystem::path> AssetFiles;
    std::vector<std::filesystem::path> MetadataFiles;
    Error.clear();
    for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(ContentRoot, Error))
    {
        if (Error)
        {
            return AssetDiagnostic::Fail(
                AssetErrorCode::InternalError,
                "AssetRegistry",
                "Failed while iterating content root: " + Error.message(),
                {},
                ContentRoot.generic_string());
        }
        if (!Entry.is_regular_file())
        {
            continue;
        }

        const std::string PathString = Entry.path().generic_string();
        if (AssetPath::HasMetaSuffix(PathString))
        {
            MetadataFiles.push_back(Entry.path());
            continue;
        }
        if (InferTypeFromExtension(PathString).IsValid())
        {
            AssetFiles.push_back(Entry.path());
        }
    }

    if (Error)
    {
        return AssetDiagnostic::Fail(
            AssetErrorCode::InternalError,
            "AssetRegistry",
            "Failed while iterating content root: " + Error.message(),
            {},
            ContentRoot.generic_string());
    }

    std::sort(AssetFiles.begin(), AssetFiles.end());
    for (const std::filesystem::path& AssetFile : AssetFiles)
    {
        const std::filesystem::path MetadataFile = AssetFile.string() + ".meta";
        if (std::filesystem::exists(MetadataFile))
        {
            continue;
        }

        AssetMetadata Metadata{};
        Metadata.Guid = Guid::Generate();
        Metadata.Type = InferTypeFromExtension(AssetFile.generic_string());
        AssetDiagnostic Diagnostic{};
        if (!ContentHash::TryHashFile(AssetFile.string(), Metadata.SourceFingerprint, Diagnostic)
            || !AssetMetadataIO::TrySaveToFile(MetadataFile.string(), Metadata, Diagnostic))
        {
            ScanDiagnostics.push_back(Diagnostic);
            PrintString(std::string("AssetRegistry scan: failed to create metadata for ") + AssetFile.generic_string());
            continue;
        }
        MetadataFiles.push_back(MetadataFile);
        PrintString(std::string("AssetRegistry scan: created metadata for ") + AssetFile.generic_string());
    }

    std::sort(MetadataFiles.begin(), MetadataFiles.end());
    MetadataFiles.erase(std::unique(MetadataFiles.begin(), MetadataFiles.end()), MetadataFiles.end());
    for (const std::filesystem::path& MetadataFile : MetadataFiles)
    {
        AssetDiagnostic Diagnostic = IngestMetaFile(Mount, MetadataFile);
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

    const AssetDiagnostic EngineScan = ScanMount(AssetMount::Engine, EngineContentRoot);
    if (EngineScan.HasError())
    {
        ScanDiagnostics.push_back(EngineScan);
        return EngineScan;
    }

    const AssetDiagnostic GameScan = ScanMount(AssetMount::Game, GameContentRoot);
    if (GameScan.HasError())
    {
        ScanDiagnostics.push_back(GameScan);
        return GameScan;
    }

    if (!ScanDiagnostics.empty())
    {
        return AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "AssetRegistry",
            std::to_string(ScanDiagnostics.size()) + " asset(s) failed during content scan");
    }

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
    if (!Metadata.Type.IsValid())
    {
        Metadata.Type = InferTypeFromExtension(Normalized);
    }
    if (TypeRegistrations.find(Metadata.Type.GetIdentifier()) == TypeRegistrations.end())
    {
        return AssetDiagnostic::Fail(
            AssetErrorCode::UnsupportedFormat,
            "AssetRegistry",
            "Asset type is not registered: " + Metadata.Type.GetIdentifier(),
            {Metadata.Guid},
            VirtualPath);
    }
    if (EntriesById.find(Metadata.Guid) != EntriesById.end())
    {
        return AssetDiagnostic::Fail(AssetErrorCode::DuplicateId, "AssetRegistry", "GUID already registered", {Metadata.Guid}, VirtualPath);
    }
    if (IdsByVirtualPath.find(VirtualPath) != IdsByVirtualPath.end())
    {
        return AssetDiagnostic::Fail(AssetErrorCode::ImportConflict, "AssetRegistry", "Path already registered", {Metadata.Guid}, VirtualPath);
    }
    if (Metadata.SubAssets.empty())
    {
        if (Mount == AssetMount::Game && LeafNameExists(std::filesystem::path(Normalized).stem().string()))
        {
            return AssetDiagnostic::Fail(AssetErrorCode::ImportConflict, "AssetRegistry", "Asset name already exists in project", {Metadata.Guid}, VirtualPath);
        }
    }
    else
    {
        std::vector<std::string> IncomingNames;
        for (const SubAssetRecord& Record : Metadata.SubAssets)
        {
            if (TypeRegistrations.find(Record.Type.GetIdentifier()) == TypeRegistrations.end())
            {
                return AssetDiagnostic::Fail(
                    AssetErrorCode::UnsupportedFormat,
                    "AssetRegistry",
                    "Subasset type is not registered: " + Record.Type.GetIdentifier(),
                    {Metadata.Guid},
                    VirtualPath);
            }
            std::string NormalizedName = Record.Name;
            std::transform(NormalizedName.begin(), NormalizedName.end(), NormalizedName.begin(), [](unsigned char Character)
            {
                return static_cast<char>(std::tolower(Character));
            });
            if (Record.Name.empty()
                || (Mount == AssetMount::Game && LeafNameExists(Record.Name))
                || std::find(IncomingNames.begin(), IncomingNames.end(), NormalizedName) != IncomingNames.end())
            {
                return AssetDiagnostic::Fail(AssetErrorCode::ImportConflict, "AssetRegistry", "Subasset name must be unique in project", {Metadata.Guid}, VirtualPath);
            }
            IncomingNames.push_back(std::move(NormalizedName));
        }
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
    AssetKey IgnoredKey{};
    IgnoredKey.Asset = Entry.Metadata.Guid;
    if (Entry.Metadata.SubAssets.empty()
        && NewMount == AssetMount::Game
        && LeafNameExists(std::filesystem::path(NewNormalized).stem().string(), IgnoredKey))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::ImportConflict, "AssetRegistry", "Asset name already exists in project", {Entry.Metadata.Guid}, NewVirtualPath);
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

AssetDiagnostic AssetRegistry::DeletePair(const std::string& RelativeOrVirtualPath)
{
    AssertGameThread();

    AssetRegistryEntry Entry{};
    if (!TryGetByPath(RelativeOrVirtualPath, Entry))
    {
        return AssetDiagnostic::Fail(
            AssetErrorCode::NotFound,
            "AssetRegistry",
            "Asset not found",
            {},
            RelativeOrVirtualPath);
    }
    if (Entry.Mount != AssetMount::Game)
    {
        return AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "AssetRegistry",
            "Engine assets are read-only",
            {Entry.Metadata.Guid},
            Entry.VirtualPath);
    }

    const std::filesystem::path DataPath(Entry.AbsolutePath);
    const std::filesystem::path MetadataPath(Entry.AbsoluteMetaPath);
    const std::string DeleteSuffix = ".sakura-delete-" + Entry.Metadata.Guid.ToString();
    const std::filesystem::path StagedDataPath = DataPath.string() + DeleteSuffix;
    const std::filesystem::path StagedMetadataPath = MetadataPath.string() + DeleteSuffix;

    std::error_code Error;
    std::filesystem::rename(DataPath, StagedDataPath, Error);
    if (Error)
    {
        return AssetDiagnostic::Fail(
            AssetErrorCode::InternalError,
            "AssetRegistry",
            Error.message(),
            {Entry.Metadata.Guid},
            Entry.VirtualPath);
    }

    std::filesystem::rename(MetadataPath, StagedMetadataPath, Error);
    if (Error)
    {
        std::error_code RollbackError;
        std::filesystem::rename(StagedDataPath, DataPath, RollbackError);
        return AssetDiagnostic::Fail(
            AssetErrorCode::InternalError,
            "AssetRegistry",
            "Failed to stage .meta for deletion; data file rolled back",
            {Entry.Metadata.Guid},
            Entry.VirtualPath);
    }

    IdsByVirtualPath.erase(Entry.VirtualPath);
    EntriesById.erase(Entry.Metadata.Guid);
    std::filesystem::remove(StagedDataPath, Error);
    Error.clear();
    std::filesystem::remove(StagedMetadataPath, Error);
    return AssetDiagnostic::Ok();
}

AssetDiagnostic AssetRegistry::RenameSubAsset(const AssetKey& Key, const std::string& NewName)
{
    AssertGameThread();
    if (!Key.HasSubAsset() || NewName.empty())
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "AssetRegistry", "Invalid subasset rename request", Key);
    }

    auto Found = EntriesById.find(Key.Asset);
    if (Found == EntriesById.end() || Found->second.Mount != AssetMount::Game)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::NotFound, "AssetRegistry", "Game subasset not found", Key);
    }
    if (LeafNameExists(NewName, Key))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::ImportConflict, "AssetRegistry", "Asset name already exists in project", Key);
    }

    AssetMetadata UpdatedMetadata = Found->second.Metadata;
    auto SubAsset = std::find_if(
        UpdatedMetadata.SubAssets.begin(),
        UpdatedMetadata.SubAssets.end(),
        [&Key](const SubAssetRecord& Record)
        {
            return Record.Id == *Key.SubAsset;
        });
    if (SubAsset == UpdatedMetadata.SubAssets.end())
    {
        return AssetDiagnostic::Fail(AssetErrorCode::NotFound, "AssetRegistry", "Subasset not found", Key);
    }
    SubAsset->Name = NewName;

    AssetDiagnostic Error{};
    if (!AssetMetadataIO::TrySaveToFile(Found->second.AbsoluteMetaPath, UpdatedMetadata, Error))
    {
        return Error;
    }
    Found->second.Metadata = std::move(UpdatedMetadata);
    return AssetDiagnostic::Ok();
}

AssetDiagnostic AssetRegistry::DeleteSubAsset(const AssetKey& Key)
{
    AssertGameThread();
    if (!Key.HasSubAsset())
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "AssetRegistry", "Invalid subasset delete request", Key);
    }

    auto Found = EntriesById.find(Key.Asset);
    if (Found == EntriesById.end() || Found->second.Mount != AssetMount::Game)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::NotFound, "AssetRegistry", "Game subasset not found", Key);
    }
    if (Found->second.Metadata.SubAssets.size() == 1
        && Found->second.Metadata.SubAssets.front().Id == *Key.SubAsset)
    {
        return DeletePair(Found->second.VirtualPath);
    }

    AssetMetadata UpdatedMetadata = Found->second.Metadata;
    const auto NewEnd = std::remove_if(
        UpdatedMetadata.SubAssets.begin(),
        UpdatedMetadata.SubAssets.end(),
        [&Key](const SubAssetRecord& Record)
        {
            return Record.Id == *Key.SubAsset;
        });
    if (NewEnd == UpdatedMetadata.SubAssets.end())
    {
        return AssetDiagnostic::Fail(AssetErrorCode::NotFound, "AssetRegistry", "Subasset not found", Key);
    }
    UpdatedMetadata.SubAssets.erase(NewEnd, UpdatedMetadata.SubAssets.end());

    AssetDiagnostic Error{};
    if (!AssetMetadataIO::TrySaveToFile(Found->second.AbsoluteMetaPath, UpdatedMetadata, Error))
    {
        return Error;
    }
    Found->second.Metadata = std::move(UpdatedMetadata);
    return AssetDiagnostic::Ok();
}

bool AssetRegistry::LeafNameExists(const std::string& Name, const AssetKey& IgnoredKey) const
{
    std::string NormalizedName = Name;
    std::transform(NormalizedName.begin(), NormalizedName.end(), NormalizedName.begin(), [](unsigned char Character)
    {
        return static_cast<char>(std::tolower(Character));
    });

    for (const auto& Pair : EntriesById)
    {
        const AssetRegistryEntry& Entry = Pair.second;
        if (Entry.Mount != AssetMount::Game)
        {
            continue;
        }
        if (Entry.Metadata.SubAssets.empty())
        {
            std::string ExistingName = std::filesystem::path(Entry.RelativePath).stem().string();
            std::transform(ExistingName.begin(), ExistingName.end(), ExistingName.begin(), [](unsigned char Character)
            {
                return static_cast<char>(std::tolower(Character));
            });
            if (ExistingName == NormalizedName && Entry.Metadata.Guid != IgnoredKey.Asset)
            {
                return true;
            }
            continue;
        }
        for (const SubAssetRecord& Record : Entry.Metadata.SubAssets)
        {
            std::string ExistingName = Record.Name;
            std::transform(ExistingName.begin(), ExistingName.end(), ExistingName.begin(), [](unsigned char Character)
            {
                return static_cast<char>(std::tolower(Character));
            });
            const bool bIgnored = IgnoredKey.Asset == Entry.Metadata.Guid
                && IgnoredKey.HasSubAsset()
                && *IgnoredKey.SubAsset == Record.Id;
            if (!bIgnored && ExistingName == NormalizedName)
            {
                return true;
            }
        }
    }
    return false;
}
