#include "Assets/FbxToGlbConverter.h"

#include "ufbx.h"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>

#include <cstring>
#include <fstream>
#include <vector>

namespace
{
constexpr uint64_t MaxAssetFileBytes = 256ull * 1024ull * 1024ull;

struct TriangleMeshData
{
    std::vector<float> Positions;
    std::vector<float> Normals;
    std::vector<float> TexCoords;
    std::vector<uint32_t> Indices;
    bool bHasNormals = false;
    bool bHasTexCoords = false;
};

bool TryBuildTriangleMesh(const ufbx_mesh* Mesh, const ufbx_node* Node,
    TriangleMeshData& OutMesh, ImportResult& Result)
{
    if (Mesh == nullptr)
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "FbxToGlbConverter",
            "Null mesh");
        return false;
    }

    // Bake the evaluated rest pose into one static mesh. The C++ viewport does not
    // render skeletal meshes yet, but it can display their posed geometry.
    const ufbx_vertex_vec3* Positions = Mesh->skinned_position.exists
        ? &Mesh->skinned_position : &Mesh->vertex_position;
    const ufbx_vertex_vec3* Normals = Mesh->skinned_normal.exists
        ? &Mesh->skinned_normal : &Mesh->vertex_normal;
    const bool bLocal = Node != nullptr && (!Mesh->skinned_position.exists || Mesh->skinned_is_local);
    const ufbx_matrix NormalToWorld = bLocal
        ? ufbx_matrix_for_normals(&Node->geometry_to_world) : ufbx_matrix{};
    OutMesh.bHasNormals = Normals->exists;
    OutMesh.bHasTexCoords = Mesh->vertex_uv.exists;
    std::vector<uint32_t> TriIndices;
    TriIndices.reserve(Mesh->max_face_triangles * 3);

    for (size_t FaceIndex = 0; FaceIndex < Mesh->faces.count; ++FaceIndex)
    {
        const ufbx_face Face = Mesh->faces[FaceIndex];
        const size_t Required = ufbx_get_triangulate_face_num_indices(Face);
        TriIndices.resize(Required);
        const uint32_t TriangleCount = ufbx_triangulate_face(TriIndices.data(), TriIndices.size(), Mesh, Face);
        if (TriangleCount == 0)
        {
            Result.Diagnostic = AssetDiagnostic::Fail(
                AssetErrorCode::InvalidData,
                "FbxToGlbConverter",
                "Failed to triangulate face");
            return false;
        }

        for (uint32_t Triangle = 0; Triangle < TriangleCount; ++Triangle)
        {
            for (uint32_t Corner = 0; Corner < 3; ++Corner)
            {
                const uint32_t Index = TriIndices[Triangle * 3 + Corner];
                ufbx_vec3 Position = ufbx_get_vertex_vec3(Positions, Index);
                if (bLocal) Position = ufbx_transform_position(&Node->geometry_to_world, Position);
                OutMesh.Positions.push_back(static_cast<float>(Position.x));
                OutMesh.Positions.push_back(static_cast<float>(Position.y));
                OutMesh.Positions.push_back(static_cast<float>(Position.z));

                ufbx_vec3 Normal = Normals->exists
                    ? ufbx_get_vertex_vec3(Normals, Index) : ufbx_vec3{0.0, 1.0, 0.0};
                if (bLocal) Normal = ufbx_transform_direction(&NormalToWorld, Normal);
                OutMesh.Normals.push_back(static_cast<float>(Normal.x));
                OutMesh.Normals.push_back(static_cast<float>(Normal.y));
                OutMesh.Normals.push_back(static_cast<float>(Normal.z));

                const ufbx_vec2 TexCoord = Mesh->vertex_uv.exists
                    ? ufbx_get_vertex_vec2(&Mesh->vertex_uv, Index) : ufbx_vec2{};
                OutMesh.TexCoords.push_back(static_cast<float>(TexCoord.x));
                OutMesh.TexCoords.push_back(static_cast<float>(TexCoord.y));

                OutMesh.Indices.push_back(static_cast<uint32_t>(OutMesh.Indices.size()));
            }
        }
    }

    return !OutMesh.Indices.empty();
}

void AppendBytes(std::vector<std::byte>& Destination, const void* Source, size_t ByteCount)
{
    const auto* Bytes = static_cast<const std::byte*>(Source);
    Destination.insert(Destination.end(), Bytes, Bytes + ByteCount);
}

size_t AlignUp(size_t Value, size_t Alignment)
{
    return (Value + Alignment - 1) & ~(Alignment - 1);
}
}

ImportResult FbxToGlbConverter::Convert(
    const std::filesystem::path& SourceFbxPath,
    const std::filesystem::path& DestinationGlbPath,
    bool bGenerateMissingNormals)
{
    ImportResult Result{};

    std::error_code ErrorCode;
    const auto FileSize = std::filesystem::file_size(SourceFbxPath, ErrorCode);
    if (ErrorCode)
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::NotFound,
            "FbxToGlbConverter",
            "Source FBX not found",
            {},
            SourceFbxPath.string());
        return Result;
    }

    if (FileSize > MaxAssetFileBytes)
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "FbxToGlbConverter",
            "File exceeds 256MB limit",
            {},
            SourceFbxPath.string());
        return Result;
    }

    ufbx_load_opts Options{};
    Options.generate_missing_normals = bGenerateMissingNormals;
    Options.evaluate_skinning = true;
    Options.target_axes = ufbx_axes_right_handed_y_up;
    Options.target_unit_meters = 1.0;
    ufbx_error Error{};
    ufbx_scene* Scene = ufbx_load_file(SourceFbxPath.string().c_str(), &Options, &Error);
    if (Scene == nullptr)
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "FbxToGlbConverter",
            Error.description.data != nullptr ? std::string(Error.description.data, Error.description.length) : "ufbx load failed",
            {},
            SourceFbxPath.string());
        return Result;
    }

    if (Scene->meshes.count == 0)
    {
        ufbx_free_scene(Scene);
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "FbxToGlbConverter",
            "FBX contains no meshes",
            {},
            SourceFbxPath.string());
        return Result;
    }

    TriangleMeshData Combined{};
    for (size_t MeshIndex = 0; MeshIndex < Scene->meshes.count; ++MeshIndex)
    {
        const ufbx_mesh* SourceMesh = Scene->meshes[MeshIndex];
        const size_t InstanceCount = SourceMesh->instances.count > 0 ? SourceMesh->instances.count : 1;
        for (size_t InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
        {
            const ufbx_node* Node = SourceMesh->instances.count > 0
                ? SourceMesh->instances[InstanceIndex] : nullptr;
            TriangleMeshData Part{};
            if (!TryBuildTriangleMesh(SourceMesh, Node, Part, Result))
            {
                ufbx_free_scene(Scene);
                return Result;
            }

            const uint32_t VertexOffset = static_cast<uint32_t>(Combined.Positions.size() / 3);
            Combined.Positions.insert(Combined.Positions.end(), Part.Positions.begin(), Part.Positions.end());
            Combined.Normals.insert(Combined.Normals.end(), Part.Normals.begin(), Part.Normals.end());
            Combined.TexCoords.insert(Combined.TexCoords.end(), Part.TexCoords.begin(), Part.TexCoords.end());
            Combined.bHasNormals = Combined.bHasNormals || Part.bHasNormals;
            Combined.bHasTexCoords = Combined.bHasTexCoords || Part.bHasTexCoords;
            for (uint32_t IndexValue : Part.Indices)
            {
                Combined.Indices.push_back(IndexValue + VertexOffset);
            }
        }
    }

    ufbx_free_scene(Scene);

    std::vector<std::byte> BufferBytes;
    const size_t PositionOffset = 0;
    AppendBytes(BufferBytes, Combined.Positions.data(), Combined.Positions.size() * sizeof(float));
    const size_t NormalOffset = Combined.bHasNormals ? AlignUp(BufferBytes.size(), 4) : 0;
    if (Combined.bHasNormals)
    {
        BufferBytes.resize(NormalOffset);
        AppendBytes(BufferBytes, Combined.Normals.data(), Combined.Normals.size() * sizeof(float));
    }
    const size_t TexCoordOffset = Combined.bHasTexCoords ? AlignUp(BufferBytes.size(), 4) : 0;
    if (Combined.bHasTexCoords)
    {
        BufferBytes.resize(TexCoordOffset);
        AppendBytes(BufferBytes, Combined.TexCoords.data(), Combined.TexCoords.size() * sizeof(float));
    }
    const size_t IndexOffset = AlignUp(BufferBytes.size(), 4);
    BufferBytes.resize(IndexOffset);
    AppendBytes(BufferBytes, Combined.Indices.data(), Combined.Indices.size() * sizeof(uint32_t));

    fastgltf::Asset Asset{};
    {
        fastgltf::AssetInfo Info{};
        Info.gltfVersion = "2.0";
        Info.generator = "Sacura FbxToGlbConverter";
        Asset.assetInfo = std::move(Info);
    }

    fastgltf::Buffer Buffer{};
    fastgltf::sources::Vector BufferSource{};
    BufferSource.bytes.resize(BufferBytes.size());
    std::memcpy(BufferSource.bytes.data(), BufferBytes.data(), BufferBytes.size());
    Buffer.byteLength = BufferSource.bytes.size();
    Buffer.data = std::move(BufferSource);
    Asset.buffers.push_back(std::move(Buffer));

    auto AddBufferView = [&Asset](size_t ByteOffset, size_t ByteLength, fastgltf::BufferTarget Target) -> size_t
    {
        fastgltf::BufferView View{};
        View.bufferIndex = 0;
        View.byteOffset = ByteOffset;
        View.byteLength = ByteLength;
        View.target = Target;
        Asset.bufferViews.push_back(std::move(View));
        return Asset.bufferViews.size() - 1;
    };

    auto AddAccessor = [&Asset](size_t BufferViewIndex, fastgltf::AccessorType Type, fastgltf::ComponentType Component, size_t Count) -> size_t
    {
        fastgltf::Accessor Accessor{};
        Accessor.bufferViewIndex = BufferViewIndex;
        Accessor.byteOffset = 0;
        Accessor.componentType = Component;
        Accessor.type = Type;
        Accessor.count = Count;
        Asset.accessors.push_back(std::move(Accessor));
        return Asset.accessors.size() - 1;
    };

    const size_t VertexCount = Combined.Positions.size() / 3;
    const size_t PositionView = AddBufferView(PositionOffset, Combined.Positions.size() * sizeof(float), fastgltf::BufferTarget::ArrayBuffer);
    const size_t PositionAccessor = AddAccessor(PositionView, fastgltf::AccessorType::Vec3, fastgltf::ComponentType::Float, VertexCount);

    size_t NormalAccessor = 0;
    if (Combined.bHasNormals)
    {
        const size_t NormalView = AddBufferView(NormalOffset, Combined.Normals.size() * sizeof(float), fastgltf::BufferTarget::ArrayBuffer);
        NormalAccessor = AddAccessor(NormalView, fastgltf::AccessorType::Vec3, fastgltf::ComponentType::Float, VertexCount);
    }

    size_t TexCoordAccessor = 0;
    if (Combined.bHasTexCoords)
    {
        const size_t TexCoordView = AddBufferView(TexCoordOffset, Combined.TexCoords.size() * sizeof(float), fastgltf::BufferTarget::ArrayBuffer);
        TexCoordAccessor = AddAccessor(TexCoordView, fastgltf::AccessorType::Vec2, fastgltf::ComponentType::Float, VertexCount);
    }

    const size_t IndexView = AddBufferView(IndexOffset, Combined.Indices.size() * sizeof(uint32_t), fastgltf::BufferTarget::ElementArrayBuffer);
    const size_t IndexAccessor = AddAccessor(IndexView, fastgltf::AccessorType::Scalar, fastgltf::ComponentType::UnsignedInt, Combined.Indices.size());

    fastgltf::Primitive Primitive{};
    Primitive.type = fastgltf::PrimitiveType::Triangles;
    Primitive.indicesAccessor = IndexAccessor;
    Primitive.attributes.emplace_back(fastgltf::Attribute{std::pmr::string("POSITION"), PositionAccessor});
    if (Combined.bHasNormals)
    {
        Primitive.attributes.emplace_back(fastgltf::Attribute{std::pmr::string("NORMAL"), NormalAccessor});
    }
    if (Combined.bHasTexCoords)
    {
        Primitive.attributes.emplace_back(fastgltf::Attribute{std::pmr::string("TEXCOORD_0"), TexCoordAccessor});
    }

    fastgltf::Material Material{};
    Material.name = "Default";
    Material.pbrData.baseColorFactor = {1.f, 1.f, 1.f, 1.f};
    Material.pbrData.metallicFactor = 0.f;
    Material.pbrData.roughnessFactor = 1.f;
    Asset.materials.push_back(std::move(Material));
    Primitive.materialIndex = 0;

    fastgltf::Mesh Mesh{};
    Mesh.name = "StaticMesh";
    Mesh.primitives.push_back(std::move(Primitive));
    Asset.meshes.push_back(std::move(Mesh));

    fastgltf::Node Node{};
    Node.name = "Root";
    Node.meshIndex = 0;
    Asset.nodes.push_back(std::move(Node));

    fastgltf::Scene GltfScene{};
    GltfScene.name = "Scene";
    GltfScene.nodeIndices.push_back(0);
    Asset.scenes.push_back(std::move(GltfScene));
    Asset.defaultScene = 0;

    fastgltf::Exporter Exporter;
    auto ExportResult = Exporter.writeGltfBinary(Asset, fastgltf::ExportOptions::None);
    if (!ExportResult)
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InternalError,
            "FbxToGlbConverter",
            std::string("GLB export failed: ") + std::string(fastgltf::getErrorMessage(ExportResult.error())),
            {},
            DestinationGlbPath.string());
        return Result;
    }

    std::filesystem::create_directories(DestinationGlbPath.parent_path(), ErrorCode);
    std::ofstream Output(DestinationGlbPath, std::ios::binary);
    if (!Output)
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InternalError,
            "FbxToGlbConverter",
            "Failed to open destination GLB",
            {},
            DestinationGlbPath.string());
        return Result;
    }

    Output.write(
        reinterpret_cast<const char*>(ExportResult->output.data()),
        static_cast<std::streamsize>(ExportResult->output.size()));
    if (!Output)
    {
        Result.Diagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InternalError,
            "FbxToGlbConverter",
            "Failed to write destination GLB",
            {},
            DestinationGlbPath.string());
        return Result;
    }

    SubAssetRecord MeshSubAsset{};
    MeshSubAsset.Id = Guid::Generate();
    MeshSubAsset.Type = StaticMeshAssetType;
    MeshSubAsset.Name = "StaticMesh";
    MeshSubAsset.Selector.Kind = "mesh";
    MeshSubAsset.Selector.Index = 0;
    Result.Metadata.SubAssets.push_back(std::move(MeshSubAsset));
    Result.PublishedAbsolutePath = DestinationGlbPath;
    return Result;
}
