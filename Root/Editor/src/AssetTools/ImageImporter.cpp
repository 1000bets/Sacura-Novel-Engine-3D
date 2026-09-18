#include "AssetTools/ImageImporter.h"

#include "stb_image.h"

#include <fstream>

namespace
{
constexpr int MaxImageDimension = 8192;
constexpr uint64_t MaxAssetFileBytes = 256ull * 1024ull * 1024ull;
}

ImportResult ImageImporter::CopyImage(
    const std::filesystem::path& SourcePath,
    const std::filesystem::path& DestinationPath)
{
    ImportResult Result{};

    std::error_code ErrorCode;
    const auto FileSize = std::filesystem::file_size(SourcePath, ErrorCode);
    if (ErrorCode)
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::NotFound,
            "ImageImporter",
            "Source image not found",
            {},
            SourcePath.string());
        return Result;
    }

    if (FileSize > MaxAssetFileBytes)
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "ImageImporter",
            "File exceeds 256MB limit",
            {},
            SourcePath.string());
        return Result;
    }

    int Width = 0;
    int Height = 0;
    int ChannelCount = 0;
    if (!stbi_info(SourcePath.string().c_str(), &Width, &Height, &ChannelCount))
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "ImageImporter",
            std::string("stb_image info failed: ") + stbi_failure_reason(),
            {},
            SourcePath.string());
        return Result;
    }

    if (Width > MaxImageDimension || Height > MaxImageDimension)
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "ImageImporter",
            "Image exceeds 8192 on a side",
            {},
            SourcePath.string());
        return Result;
    }

    std::filesystem::create_directories(DestinationPath.parent_path(), ErrorCode);
    std::filesystem::copy_file(SourcePath, DestinationPath, std::filesystem::copy_options::overwrite_existing, ErrorCode);
    if (ErrorCode)
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InternalError,
            "ImageImporter",
            ErrorCode.message(),
            {},
            DestinationPath.string());
        return Result;
    }

    Result.PublishedAbsolutePath = DestinationPath;
    return Result;
}
