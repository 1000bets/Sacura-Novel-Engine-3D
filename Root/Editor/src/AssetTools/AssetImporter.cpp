#include "AssetTools/AssetImporter.h"
#include "AssetTools/FbxToGlbConverter.h"
#include "AssetTools/GlbImporter.h"
#include "AssetTools/ImageImporter.h"
#include "Assets/AssetPath.h"
#include "Assets/ContentHash.h"
#include "Core/Threading/ThreadContext.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace
{
std::string ToLowerExtension(const std::filesystem::path& Path)
{
    std::string Extension = Path.extension().string();
    std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char Character)
    {
        return static_cast<char>(std::tolower(Character));
    });
    return Extension;
}
}

AssetImporter::AssetImporter(AssetRegistry& InRegistry)
    : Registry(InRegistry)
{
}

std::vector<SubAssetRecord> AssetImporter::BuildUniqueSubAssets(
    const std::string& DestinationRelativePath,
    const std::vector<SubAssetRecord>& SourceSubAssets) const
{
    std::vector<SubAssetRecord> Result = SourceSubAssets;
    const std::string ContainerName = std::filesystem::path(DestinationRelativePath).stem().string();
    std::unordered_set<std::string> AssignedNames;
    for (size_t SubAssetIndex = 0; SubAssetIndex < Result.size(); ++SubAssetIndex)
    {
        SubAssetRecord& Record = Result[SubAssetIndex];
        std::string BaseName = Result.size() == 1
            ? ContainerName
            : ContainerName + "_" + (Record.Name.empty() ? "Mesh_" + std::to_string(SubAssetIndex) : Record.Name);
        std::string Candidate = BaseName;
        int32_t Suffix = 2;
        auto Normalize = [](std::string Value)
        {
            std::transform(Value.begin(), Value.end(), Value.begin(), [](unsigned char Character)
            {
                return static_cast<char>(std::tolower(Character));
            });
            return Value;
        };
        while (Registry.LeafNameExists(Candidate) || AssignedNames.find(Normalize(Candidate)) != AssignedNames.end())
        {
            Candidate = BaseName + "_" + std::to_string(Suffix++);
        }
        Record.Name = Candidate;
        AssignedNames.insert(Normalize(Candidate));
    }
    return Result;
}

ImportResult AssetImporter::PublishPair(
    const ImportRequest& Request,
    const std::filesystem::path& StagedDataPath,
    AssetType Type,
    const nlohmann::json& ImportInfo,
    const std::vector<SubAssetRecord>& SubAssets)
{
    ImportResult Result{};

    const std::string NormalizedDestination = AssetPath::NormalizeRelative(Request.DestinationRelativePath);
    if (Registry.PathExists(NormalizedDestination))
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::ImportConflict,
            "AssetImporter",
            "Destination path already registered; overwrite disabled",
            {},
            NormalizedDestination);
        return Result;
    }

    const std::filesystem::path ContentRoot = Registry.GetContentRoot();
    if (ContentRoot.empty())
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InternalError,
            "AssetImporter",
            "Content root is not set");
        return Result;
    }

    const std::filesystem::path DestinationAbsolute = AssetPath::CombineContent(ContentRoot, NormalizedDestination);
    if (!AssetPath::IsInsideContent(DestinationAbsolute, ContentRoot))
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "AssetImporter",
            "Destination outside Content",
            {},
            NormalizedDestination);
        return Result;
    }

    if (std::filesystem::exists(DestinationAbsolute) && !Request.bOverwrite)
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::ImportConflict,
            "AssetImporter",
            "Destination file already exists; overwrite disabled",
            {},
            NormalizedDestination);
        return Result;
    }

    std::error_code ErrorCode;
    std::filesystem::create_directories(DestinationAbsolute.parent_path(), ErrorCode);
    std::filesystem::copy_file(
        StagedDataPath,
        DestinationAbsolute,
        Request.bOverwrite ? std::filesystem::copy_options::overwrite_existing : std::filesystem::copy_options::none,
        ErrorCode);
    if (ErrorCode)
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::ImportConflict,
            "AssetImporter",
            ErrorCode.message(),
            {},
            NormalizedDestination);
        return Result;
    }

    AssetMetadata Metadata{};
    Metadata.SchemaVersion = 1;
    Metadata.Guid = Guid::Generate();
    Metadata.Type = Type;
    Metadata.ImportInfo = ImportInfo;
    Metadata.SubAssets = BuildUniqueSubAssets(NormalizedDestination, SubAssets);
    ContentHash Fingerprint{};
    AssetDiagnostic HashError{};
    if (!ContentHash::TryHashFile(DestinationAbsolute.string(), Fingerprint, HashError))
    {
        Result.Diagnostic = HashError;
        return Result;
    }
    Metadata.SourceFingerprint = Fingerprint;

    const std::filesystem::path MetaAbsolute = AssetPath::CombineContent(ContentRoot, AssetPath::MetaPathForAsset(NormalizedDestination));
    if (!AssetMetadataIO::TrySaveToFile(MetaAbsolute.string(), Metadata, Result.Diagnostic))
    {
        std::filesystem::remove(DestinationAbsolute, ErrorCode);
        return Result;
    }

    Result.Diagnostic = Registry.RegisterExistingAsset(NormalizedDestination, Metadata);
    if (Result.Diagnostic.HasError())
    {
        std::filesystem::remove(DestinationAbsolute, ErrorCode);
        std::filesystem::remove(MetaAbsolute, ErrorCode);
        return Result;
    }

    Result.Metadata = Metadata;
    Result.PublishedRelativePath = NormalizedDestination;
    Result.PublishedAbsolutePath = DestinationAbsolute;
    PrintString(std::string("AssetImporter: published ") + NormalizedDestination);
    return Result;
}

ImportResult AssetImporter::ImportGlb(const ImportRequest& Request)
{
    const std::filesystem::path StagingRoot = Request.StagingDirectory.empty()
        ? (std::filesystem::temp_directory_path() / "SakuraAssetStaging")
        : Request.StagingDirectory;

    const std::filesystem::path StagedPath = StagingRoot / Request.DestinationRelativePath;
    ImportResult StageResult = GlbImporter::ValidateAndCopy(Request.SourcePath, StagedPath);
    if (!StageResult.Succeeded())
    {
        return StageResult;
    }

    nlohmann::json ImportInfo = {
        {"source", Request.SourcePath.string()},
        {"importer", "GlbImporter"}
    };
    return PublishPair(
        Request,
        StagedPath,
        ModelAssetType,
        ImportInfo,
        StageResult.Metadata.SubAssets);
}

ImportResult AssetImporter::ImportImage(const ImportRequest& Request)
{
    const std::filesystem::path StagingRoot = Request.StagingDirectory.empty()
        ? (std::filesystem::temp_directory_path() / "SakuraAssetStaging")
        : Request.StagingDirectory;

    const std::filesystem::path StagedPath = StagingRoot / Request.DestinationRelativePath;
    ImportResult StageResult = ImageImporter::CopyImage(Request.SourcePath, StagedPath);
    if (!StageResult.Succeeded())
    {
        return StageResult;
    }

    nlohmann::json ImportInfo = {
        {"source", Request.SourcePath.string()},
        {"importer", "ImageImporter"}
    };
    return PublishPair(Request, StagedPath, TextureAssetType, ImportInfo);
}

ImportResult AssetImporter::ImportFbx(const ImportRequest& Request)
{
    const std::filesystem::path StagingRoot = Request.StagingDirectory.empty()
        ? (std::filesystem::temp_directory_path() / "SakuraAssetStaging")
        : Request.StagingDirectory;

    std::filesystem::path DestinationGlb = Request.DestinationRelativePath;
    if (ToLowerExtension(DestinationGlb) != ".glb")
    {
        DestinationGlb.replace_extension(".glb");
    }

    ImportRequest Adjusted = Request;
    Adjusted.DestinationRelativePath = DestinationGlb.generic_string();

    const std::filesystem::path StagedPath = StagingRoot / Adjusted.DestinationRelativePath;
    ImportResult ConvertResult = FbxToGlbConverter::Convert(
        Request.SourcePath,
        StagedPath,
        Request.bGenerateMissingNormals);
    if (!ConvertResult.Succeeded())
    {
        return ConvertResult;
    }

    nlohmann::json ImportInfo = {
        {"source", Request.SourcePath.string()},
        {"importer", "FbxToGlbConverter"},
        {"convertedTo", "glb"},
        {"generateMissingNormals", Request.bGenerateMissingNormals}
    };
    return PublishPair(
        Adjusted,
        StagedPath,
        ModelAssetType,
        ImportInfo,
        ConvertResult.Metadata.SubAssets);
}

ImportResult AssetImporter::Import(const ImportRequest& Request)
{
    ImportResult Result{};
    if (Request.SourcePath.empty() || Request.DestinationRelativePath.empty())
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "AssetImporter",
            "Source or destination path is empty");
        return Result;
    }

    if (!Request.StagingDirectory.empty() && !Registry.GetContentRoot().empty())
    {
        std::error_code ErrorCode;
        const std::filesystem::path StagingAbsolute = std::filesystem::weakly_canonical(Request.StagingDirectory, ErrorCode);
        const std::filesystem::path ContentAbsolute = std::filesystem::weakly_canonical(Registry.GetContentRoot(), ErrorCode);
        if (!ErrorCode)
        {
            const std::string StagingText = StagingAbsolute.generic_string();
            const std::string ContentText = ContentAbsolute.generic_string();
            if (StagingText.size() >= ContentText.size() && StagingText.compare(0, ContentText.size(), ContentText) == 0)
            {
                Result.Diagnostic = AssetDiagnostic::Fail(
                    AssetErrorCode::InvalidData,
                    "AssetImporter",
                    "Staging directory must be outside Content");
                return Result;
            }
        }
    }

    const std::string Extension = ToLowerExtension(Request.SourcePath);
    if (Extension == ".glb")
    {
        return ImportGlb(Request);
    }
    if (Extension == ".png" || Extension == ".jpg" || Extension == ".jpeg")
    {
        return ImportImage(Request);
    }
    if (Extension == ".fbx")
    {
        return ImportFbx(Request);
    }

    Result.Diagnostic = AssetDiagnostic::Fail(
        AssetErrorCode::UnsupportedFormat,
        "AssetImporter",
        "Unsupported import extension",
        {},
        Request.SourcePath.string());
    return Result;
}
