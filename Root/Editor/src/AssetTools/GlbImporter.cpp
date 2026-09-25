#include "AssetTools/GlbImporter.h"

#include <fastgltf/core.hpp>

#include <fstream>
#include <variant>
#include <vector>

namespace
{
constexpr uint64_t MaxAssetFileBytes = 256ull * 1024ull * 1024ull;
}

ImportResult GlbImporter::ValidateAndCopy(
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
            "GlbImporter",
            "Source GLB not found",
            {},
            SourcePath.string());
        return Result;
    }

    if (FileSize > MaxAssetFileBytes)
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "GlbImporter",
            "File exceeds 256MB limit",
            {},
            SourcePath.string());
        return Result;
    }

    std::ifstream Input(SourcePath, std::ios::binary);
    std::vector<uint8_t> Bytes(static_cast<size_t>(FileSize));
    Input.read(reinterpret_cast<char*>(Bytes.data()), static_cast<std::streamsize>(FileSize));
    if (!Input)
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "GlbImporter",
            "Failed to read GLB",
            {},
            SourcePath.string());
        return Result;
    }

    auto DataBufferResult = fastgltf::GltfDataBuffer::FromBytes(
        reinterpret_cast<const std::byte*>(Bytes.data()),
        Bytes.size());
    if (!DataBufferResult)
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "GlbImporter",
            "Invalid GLB container",
            {},
            SourcePath.string());
        return Result;
    }

    fastgltf::Parser Parser;
    auto AssetResult = Parser.loadGltfBinary(DataBufferResult.get(), SourcePath.parent_path(), fastgltf::Options::None);
    if (!AssetResult)
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "GlbImporter",
            std::string("GLB parse failed: ") + std::string(fastgltf::getErrorMessage(AssetResult.error())),
            {},
            SourcePath.string());
        return Result;
    }

    for (const fastgltf::Buffer& Buffer : AssetResult->buffers)
    {
        if (std::holds_alternative<fastgltf::sources::URI>(Buffer.data))
        {
            Result.Diagnostic = AssetDiagnostic::Fail(
                AssetErrorCode::UnsupportedFeature,
                "GlbImporter",
                "External URI buffers are not allowed",
                {},
                SourcePath.string());
            return Result;
        }
    }

    for (const fastgltf::Image& Image : AssetResult->images)
    {
        if (std::holds_alternative<fastgltf::sources::URI>(Image.data))
        {
            Result.Diagnostic = AssetDiagnostic::Fail(
                AssetErrorCode::UnsupportedFeature,
                "GlbImporter",
                "External URI images are not allowed",
                {},
                SourcePath.string());
            return Result;
        }
    }

    for (size_t MeshIndex = 0; MeshIndex < AssetResult->meshes.size(); ++MeshIndex)
    {
        const fastgltf::Mesh& Mesh = AssetResult->meshes[MeshIndex];
        if (Mesh.primitives.empty())
        {
            continue;
        }

        SubAssetRecord MeshSubAsset{};
        MeshSubAsset.Id = Guid::Generate();
        MeshSubAsset.Type = StaticMeshAssetType;
        MeshSubAsset.Name = Mesh.name.empty()
            ? "Mesh_" + std::to_string(MeshIndex)
            : std::string(Mesh.name);
        MeshSubAsset.Selector.Kind = "mesh";
        MeshSubAsset.Selector.Index = static_cast<int32_t>(MeshIndex);
        Result.Metadata.SubAssets.push_back(std::move(MeshSubAsset));
    }

    std::filesystem::create_directories(DestinationPath.parent_path(), ErrorCode);
    std::filesystem::copy_file(SourcePath, DestinationPath, std::filesystem::copy_options::overwrite_existing, ErrorCode);
    if (ErrorCode)
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InternalError,
            "GlbImporter",
            ErrorCode.message(),
            {},
            DestinationPath.string());
        return Result;
    }

    Result.PublishedAbsolutePath = DestinationPath;
    return Result;
}
