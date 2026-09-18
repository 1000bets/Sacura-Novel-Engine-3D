#include "AssetTools/AssetImporter.h"
#include "AssetTools/FbxToGlbConverter.h"
#include "AssetTools/GlbImporter.h"
#include "AssetTools/ImageImporter.h"
#include "Assets/AssetPath.h"
#include "Assets/ContentHash.h"
#include "Core/Threading/ThreadContext.h"

#include <algorithm>

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

ImportResult AssetImporter::PublishPair(
    const ImportRequest& Request,
    const std::filesystem::path& StagedDataPath,
    AssetType Type,
    const nlohmann::json& ImportInfo)
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
    return PublishPair(Request, StagedPath, AssetType::Model, ImportInfo);
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
    return PublishPair(Request, StagedPath, AssetType::Texture, ImportInfo);
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
    ImportResult ConvertResult = FbxToGlbConverter::Convert(Request.SourcePath, StagedPath);
    if (!ConvertResult.Succeeded())
    {
        return ConvertResult;
    }

    nlohmann::json ImportInfo = {
        {"source", Request.SourcePath.string()},
        {"importer", "FbxToGlbConverter"},
        {"convertedTo", "glb"}
    };
    return PublishPair(Adjusted, StagedPath, AssetType::Model, ImportInfo);
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
