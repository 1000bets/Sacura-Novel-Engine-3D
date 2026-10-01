#include "Assets/AssetOperations.h"
#include "Assets/MaterialSourceRelocation.h"

#include "Assets/AssetMetadata.h"
#include "Engine.h"
#include "Project/ProjectSession.h"
#include "Documents/SceneDocument.h"

#include <algorithm>
#include <cctype>
#include <set>

AssetOperations::AssetOperations(Engine& InEngine)
    : BoundEngine(InEngine)
{
}

void AssetOperations::SetProjectContext(ProjectSession* Session, SceneDocument* Document)
{
    BoundSession = Session;
    BoundDocument = Document;
}

bool AssetOperations::ResolveGameFolder(const std::string& Folder, std::filesystem::path& OutAbsolute) const
{
    AssetMount Mount = AssetMount::Game;
    std::string Relative;
    const std::filesystem::path Root = BoundEngine.GetAssetRegistry().GetGameContentRoot();
    if (Root.empty() || !AssetPath::TryParseVirtualPath(Folder, Mount, Relative)
        || Mount != AssetMount::Game)
    {
        return false;
    }
    OutAbsolute = AssetPath::CombineContent(Root, Relative).lexically_normal();
    return AssetPath::IsInsideContent(OutAbsolute, Root);
}

AssetDiagnostic AssetOperations::RelocateProjectPaths(
    const std::filesystem::path& Source,
    const std::filesystem::path& Destination)
{
    if (BoundSession != nullptr)
    {
        std::string Error;
        if (!BoundSession->RelocateContentPath(Source, Destination, Error))
        {
            return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "AssetMove", Error);
        }
    }
    return AssetDiagnostic::Ok();
}

void AssetOperations::RefreshDocumentPath()
{
    if (BoundDocument != nullptr)
    {
        BoundDocument->RefreshAssetPath();
    }
}

void AssetOperations::InvalidateAsset(const AssetId& Identity)
{
    BoundEngine.GetAssetManager().InvalidateAsset(Identity);
    BoundEngine.GetAssetGpuUploader().InvalidateAsset(Identity);
}

AssetDiagnostic AssetOperations::RenameAsset(const std::string& Source, const std::string& Destination)
{
    AssetRegistry& Registry = BoundEngine.GetAssetRegistry();
    AssetRegistryEntry Entry;
    if (!Registry.TryGetByPath(Source, Entry) || Entry.Mount != AssetMount::Game)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "AssetMove", "Only Game assets can be moved");
    }
    std::filesystem::path DestinationAbsolute;
    if (!ResolveGameFolder(Destination, DestinationAbsolute))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "AssetMove", "Destination is outside Game Content");
    }
    std::vector<MaterialSourceEdit> ShaderEdits;
    AssetDiagnostic Result;
    if (Entry.Metadata.Type == ShaderSourceAssetType)
    {
        Result = MaterialSourceRelocation::Prepare(Registry, Entry.AbsolutePath, DestinationAbsolute, ShaderEdits);
        if (Result.HasError())
        {
            return Result;
        }
    }
    Result = Registry.RenamePair(Source, Destination);
    if (Result.HasError())
    {
        return Result;
    }
    Result = MaterialSourceRelocation::Apply(ShaderEdits);
    if (Result.HasError())
    {
        Registry.RenamePair(Destination, Source);
        return Result;
    }
    Result = RelocateProjectPaths(Entry.AbsolutePath, DestinationAbsolute);
    if (Result.HasError())
    {
        MaterialSourceRelocation::Restore(ShaderEdits);
        const AssetDiagnostic RolledBack = Registry.RenamePair(Destination, Source);
        if (RolledBack.HasError())
        {
            Result.Message += "; rollback failed: " + RolledBack.Message;
            RefreshDocumentPath();
        }
        return Result;
    }
    RefreshDocumentPath();
    return AssetDiagnostic::Ok();
}

AssetDiagnostic AssetOperations::RenameSubAsset(const AssetKey& Key, const std::string& Name)
{
    return BoundEngine.GetAssetRegistry().RenameSubAsset(Key, Name);
}

AssetDiagnostic AssetOperations::DeleteAsset(const AssetKey& Key)
{
    AssetRegistry& Registry = BoundEngine.GetAssetRegistry();
    AssetRegistryEntry Entry;
    if (!Registry.TryResolveKey(Key, Entry))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::NotFound, "AssetDelete", "Asset is no longer registered", Key);
    }
    if (BoundDocument != nullptr && BoundDocument->IsOpen() && BoundDocument->GetAssetId() == Key.Asset)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "AssetDelete", "Close the scene document before deleting its asset", Key);
    }
    AssetDiagnostic Result;
    if (Key.HasSubAsset())
    {
        Result = Registry.DeleteSubAsset(Key);
    }
    else
    {
        Result = Registry.DeletePair(Entry.VirtualPath);
    }
    if (!Result.HasError())
    {
        InvalidateAsset(Key.Asset);
    }
    return Result;
}

AssetDiagnostic AssetOperations::CreateFolder(const std::string& Folder)
{
    std::filesystem::path Absolute;
    if (!ResolveGameFolder(Folder, Absolute))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "FolderCreate", "Folder is outside Game Content");
    }
    std::error_code Error;
    if (!std::filesystem::create_directories(Absolute, Error))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::ImportConflict, "FolderCreate", "Folder already exists or cannot be created: " + Error.message());
    }
    return AssetDiagnostic::Ok();
}

AssetDiagnostic AssetOperations::RenameFolder(const std::string& Source, const std::string& Destination)
{
    std::filesystem::path SourceAbsolute;
    std::filesystem::path DestinationAbsolute;
    AssetRegistry& Registry = BoundEngine.GetAssetRegistry();
    if (!ResolveGameFolder(Source, SourceAbsolute) || !ResolveGameFolder(Destination, DestinationAbsolute)
        || SourceAbsolute == Registry.GetGameContentRoot().lexically_normal()
        || DestinationAbsolute == Registry.GetGameContentRoot().lexically_normal())
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "FolderMove", "Only nested Game folders can be moved");
    }
    std::error_code Error;
    if (std::filesystem::exists(DestinationAbsolute, Error))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::ImportConflict, "FolderMove", "Destination folder already exists");
    }
    std::vector<MaterialSourceEdit> ShaderEdits;
    AssetDiagnostic Result = MaterialSourceRelocation::Prepare(Registry, SourceAbsolute, DestinationAbsolute, ShaderEdits);
    if (Result.HasError())
    {
        return Result;
    }
    std::filesystem::rename(SourceAbsolute, DestinationAbsolute, Error);
    if (Error)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "FolderMove", Error.message());
    }
    Result = MaterialSourceRelocation::Apply(ShaderEdits);
    if (Result.HasError())
    {
        MaterialSourceRelocation::Restore(ShaderEdits);
        std::filesystem::rename(DestinationAbsolute, SourceAbsolute, Error);
        return Result;
    }
    Result = RelocateProjectPaths(SourceAbsolute, DestinationAbsolute);
    if (Result.HasError())
    {
        MaterialSourceRelocation::Restore(ShaderEdits);
        std::filesystem::rename(DestinationAbsolute, SourceAbsolute, Error);
        if (Error)
        {
            Result.Message += "; rollback failed: " + Error.message();
            Registry.ScanContent();
            RefreshDocumentPath();
        }
        return Result;
    }
    Result = Registry.ScanContent();
    RefreshDocumentPath();
    return Result;
}

AssetDiagnostic AssetOperations::DeleteFolder(const std::string& Folder)
{
    std::filesystem::path Absolute;
    AssetRegistry& Registry = BoundEngine.GetAssetRegistry();
    if (!ResolveGameFolder(Folder, Absolute) || Absolute == Registry.GetGameContentRoot().lexically_normal())
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "FolderDelete", "Only nested Game folders can be deleted");
    }
    if (BoundDocument != nullptr && BoundDocument->IsOpen()
        && AssetPath::IsInsideContent(BoundDocument->GetDocumentPath(), Absolute))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "FolderDelete", "Close the scene document before deleting its folder");
    }
    const std::filesystem::path Staging = Registry.GetGameContentRoot().parent_path() / ".sakura-trash" / Guid::Generate().ToString();
    std::error_code Error;
    std::filesystem::create_directories(Staging.parent_path(), Error);
    if (Error)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "FolderDelete", Error.message());
    }
    std::filesystem::rename(Absolute, Staging, Error);
    if (Error)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "FolderDelete", Error.message());
    }
    for (const AssetRegistryEntry& Entry : Registry.FindByDirectory(Folder))
    {
        if (AssetPath::IsInsideContent(Entry.AbsolutePath, Absolute))
        {
            InvalidateAsset(Entry.Metadata.Guid);
        }
    }
    const AssetDiagnostic Scan = Registry.ScanContent();
    std::filesystem::remove_all(Staging, Error);
    if (Error)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "FolderDelete", "Deleted assets remain recoverable at " + Staging.generic_string() + ": " + Error.message());
    }
    return Scan;
}

AssetDiagnostic AssetOperations::DuplicateAsset(const AssetKey& Source, const std::string& DestinationFolder)
{
    AssetRegistry& Registry = BoundEngine.GetAssetRegistry();
    AssetRegistryEntry Entry;
    std::filesystem::path Directory;
    if (!Registry.TryResolveKey(Source, Entry) || !ResolveGameFolder(DestinationFolder, Directory))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "AssetDuplicate", "Source or destination is invalid", Source);
    }
    AssetMetadata Metadata;
    AssetDiagnostic Result;
    if (!AssetMetadataIO::TryLoadFromFile(Entry.AbsoluteMetaPath, Metadata, Result))
    {
        return Result;
    }
    const std::filesystem::path SourcePath(Entry.AbsolutePath);
    const std::string BaseName = SourcePath.stem().string() + "_Copy";
    std::string Name = BaseName;
    int Suffix = 2;
    std::filesystem::path Destination = Directory / (Name + SourcePath.extension().string());
    while (std::filesystem::exists(Destination) || std::filesystem::exists(Destination.string() + ".meta")
        || (Metadata.SubAssets.empty() && Registry.LeafNameExists(Name)))
    {
        Name = BaseName + "_" + std::to_string(Suffix++);
        Destination = Directory / (Name + SourcePath.extension().string());
    }
    std::set<std::string> ReservedNames;
    for (SubAssetRecord& SubAsset : Metadata.SubAssets)
    {
        const std::string SubAssetBase = SubAsset.Name + "_Copy";
        std::string SubAssetName = SubAssetBase;
        std::string Normalized;
        int SubAssetSuffix = 2;
        for (;;)
        {
            Normalized = SubAssetName;
            std::transform(Normalized.begin(), Normalized.end(), Normalized.begin(), [](unsigned char Character)
            {
                return static_cast<char>(std::tolower(Character));
            });
            if (!Registry.LeafNameExists(SubAssetName) && ReservedNames.find(Normalized) == ReservedNames.end())
            {
                break;
            }
            SubAssetName = SubAssetBase + "_" + std::to_string(SubAssetSuffix++);
        }
        ReservedNames.insert(Normalized);
        SubAsset.Name = SubAssetName;
        SubAsset.Id = Guid::Generate();
    }
    Metadata.Guid = Guid::Generate();
    std::error_code Error;
    std::filesystem::create_directories(Directory, Error);
    if (Error)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "AssetDuplicate", Error.message(), Source);
    }
    std::filesystem::copy_file(SourcePath, Destination, std::filesystem::copy_options::none, Error);
    if (Error)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "AssetDuplicate", Error.message(), Source);
    }
    if (!AssetMetadataIO::TrySaveToFile(Destination.string() + ".meta", Metadata, Result))
    {
        std::filesystem::remove(Destination, Error);
        return Result;
    }
    return Registry.ScanContent();
}
