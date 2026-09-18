#include "Assets/Loaders/TextureLoader.h"
#include "Assets/Resources/TextureResource.h"

#include "stb_image.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
constexpr int MaxImageDimension = 8192;
constexpr uint64_t MaxAssetFileBytes = 256ull * 1024ull * 1024ull;
}

std::shared_ptr<const void> TextureLoader::Load(const AssetLoadContext& Context, AssetDiagnostic& OutDiagnostic)
{
    const std::filesystem::path AbsolutePath(Context.Entry.AbsolutePath);

    std::error_code ErrorCode;
    const auto FileSize = std::filesystem::file_size(AbsolutePath, ErrorCode);
    if (ErrorCode)
    {
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::NotFound,
            "TextureLoader",
            "Failed to stat image",
            Context.Key,
            Context.Entry.RelativePath,
            "TextureLoader");
        return nullptr;
    }

    if (FileSize > MaxAssetFileBytes)
    {
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "TextureLoader",
            "File exceeds 256MB limit",
            Context.Key,
            Context.Entry.RelativePath,
            "TextureLoader");
        return nullptr;
    }

    if (Context.Entry.Metadata.SourceFingerprint.IsValid())
    {
        ContentHash ActualHash{};
        AssetDiagnostic HashError{};
        if (!ContentHash::TryHashFile(AbsolutePath.string(), ActualHash, HashError))
        {
            OutDiagnostic = HashError;
            OutDiagnostic.Key = Context.Key;
            OutDiagnostic.Loader = "TextureLoader";
            return nullptr;
        }

        if (ActualHash != Context.Entry.Metadata.SourceFingerprint)
        {
            OutDiagnostic = AssetDiagnostic::Fail(
                AssetErrorCode::AssetChanged,
                "TextureLoader",
                "Source fingerprint mismatch",
                Context.Key,
                Context.Entry.RelativePath,
                "TextureLoader");
            return nullptr;
        }
    }

    std::ifstream Input(AbsolutePath, std::ios::binary);
    if (!Input)
    {
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::NotFound,
            "TextureLoader",
            "Failed to open image file",
            Context.Key,
            Context.Entry.RelativePath,
            "TextureLoader");
        return nullptr;
    }

    std::vector<uint8_t> FileBytes(static_cast<size_t>(FileSize));
    Input.read(reinterpret_cast<char*>(FileBytes.data()), static_cast<std::streamsize>(FileSize));
    if (!Input)
    {
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "TextureLoader",
            "Failed to read image file",
            Context.Key,
            Context.Entry.RelativePath,
            "TextureLoader");
        return nullptr;
    }

    int Width = 0;
    int Height = 0;
    int ChannelCount = 0;
    stbi_uc* Decoded = stbi_load_from_memory(
        FileBytes.data(),
        static_cast<int>(FileBytes.size()),
        &Width,
        &Height,
        &ChannelCount,
        STBI_rgb_alpha);
    if (Decoded == nullptr)
    {
        const char* Reason = stbi_failure_reason();
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "TextureLoader",
            std::string("stb_image failed: ") + (Reason != nullptr ? Reason : "(null)")
                + " bytes=" + std::to_string(FileBytes.size()),
            Context.Key,
            Context.Entry.RelativePath,
            "TextureLoader");
        return nullptr;
    }

    if (Width > MaxImageDimension || Height > MaxImageDimension)
    {
        stbi_image_free(Decoded);
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "TextureLoader",
            "Image exceeds 8192 on a side",
            Context.Key,
            Context.Entry.RelativePath,
            "TextureLoader");
        return nullptr;
    }

    auto Resource = std::make_shared<TextureResource>();
    Resource->Width = static_cast<uint32_t>(Width);
    Resource->Height = static_cast<uint32_t>(Height);
    Resource->ChannelCount = 4;
    Resource->RowStride = Resource->Width * 4;
    Resource->ColorSpace = TextureColorSpace::Srgb;
    Resource->Pixels.resize(static_cast<size_t>(Resource->RowStride) * Resource->Height);
    std::copy(Decoded, Decoded + Resource->Pixels.size(), Resource->Pixels.begin());
    stbi_image_free(Decoded);

    return Resource;
}
