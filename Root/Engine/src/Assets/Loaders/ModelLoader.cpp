#include "Assets/Loaders/ModelLoader.h"
#include "Assets/Resources/MaterialResource.h"
#include "Assets/Resources/StaticMeshResource.h"
#include "Assets/Resources/TextureResource.h"
#include "Core/Threading/ThreadContext.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <variant>

namespace
{
constexpr uint64_t MaxAssetFileBytes = 256ull * 1024ull * 1024ull;

DirectX::SimpleMath::Matrix ToMatrix(const fastgltf::math::fmat4x4& Source)
{
    DirectX::SimpleMath::Matrix Result;
    for (int Row = 0; Row < 4; ++Row)
    {
        for (int Column = 0; Column < 4; ++Column)
        {
            Result.m[Row][Column] = Source[Column][Row];
        }
    }
    return Result;
}

DirectX::SimpleMath::Matrix NodeLocalTransform(const fastgltf::Node& Node)
{
    if (const fastgltf::TRS* Trs = std::get_if<fastgltf::TRS>(&Node.transform))
    {
        const DirectX::SimpleMath::Vector3 Translation(Trs->translation[0], Trs->translation[1], Trs->translation[2]);
        const DirectX::SimpleMath::Quaternion Rotation(Trs->rotation[0], Trs->rotation[1], Trs->rotation[2], Trs->rotation[3]);
        const DirectX::SimpleMath::Vector3 Scale(Trs->scale[0], Trs->scale[1], Trs->scale[2]);
        return DirectX::SimpleMath::Matrix::CreateScale(Scale)
            * DirectX::SimpleMath::Matrix::CreateFromQuaternion(Rotation)
            * DirectX::SimpleMath::Matrix::CreateTranslation(Translation);
    }

    return ToMatrix(std::get<fastgltf::math::fmat4x4>(Node.transform));
}

bool HasExternalUri(const fastgltf::DataSource& Source)
{
    return std::holds_alternative<fastgltf::sources::URI>(Source);
}

AssetDiagnostic RejectExternal(const AssetKey& Key, const std::string& Path)
{
    return AssetDiagnostic::Fail(
        AssetErrorCode::UnsupportedFeature,
        "ModelLoader",
        "External URI or network resources are not allowed",
        Key,
        Path,
        "ModelLoader");
}

bool TryReadFileBytes(const std::filesystem::path& AbsolutePath, std::vector<uint8_t>& OutBytes, AssetDiagnostic& OutDiagnostic, const AssetKey& Key)
{
    std::error_code ErrorCode;
    const auto FileSize = std::filesystem::file_size(AbsolutePath, ErrorCode);
    if (ErrorCode)
    {
        OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::NotFound, "ModelLoader", "Failed to stat GLB", Key, AbsolutePath.string(), "ModelLoader");
        return false;
    }

    if (FileSize > MaxAssetFileBytes)
    {
        OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "ModelLoader", "File exceeds 256MB limit", Key, AbsolutePath.string(), "ModelLoader");
        return false;
    }

    std::ifstream Input(AbsolutePath, std::ios::binary);
    if (!Input)
    {
        OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::NotFound, "ModelLoader", "Failed to open GLB", Key, AbsolutePath.string(), "ModelLoader");
        return false;
    }

    OutBytes.resize(static_cast<size_t>(FileSize));
    Input.read(reinterpret_cast<char*>(OutBytes.data()), static_cast<std::streamsize>(FileSize));
    if (!Input)
    {
        OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "ModelLoader", "Failed to read GLB", Key, AbsolutePath.string(), "ModelLoader");
        return false;
    }

    return true;
}

AxisAlignedBounds ComputeBounds(const std::vector<StaticMeshVertex>& Vertices)
{
    AxisAlignedBounds Bounds;
    Bounds.Minimum = DirectX::SimpleMath::Vector3(
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max());
    Bounds.Maximum = DirectX::SimpleMath::Vector3(
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest());

    if (Vertices.empty())
    {
        Bounds.Minimum = DirectX::SimpleMath::Vector3::Zero;
        Bounds.Maximum = DirectX::SimpleMath::Vector3::Zero;
        return Bounds;
    }

    for (const StaticMeshVertex& Vertex : Vertices)
    {
        Bounds.Minimum = DirectX::SimpleMath::Vector3::Min(Bounds.Minimum, Vertex.Position);
        Bounds.Maximum = DirectX::SimpleMath::Vector3::Max(Bounds.Maximum, Vertex.Position);
    }

    return Bounds;
}

bool BuildStaticMeshFromPrimitive(
    const fastgltf::Asset& Asset,
    const fastgltf::Primitive& Primitive,
    StaticMeshResource& OutMesh,
    AssetDiagnostic& OutDiagnostic,
    const AssetKey& Key,
    const std::string& Path)
{
    if (Primitive.type != fastgltf::PrimitiveType::Triangles)
    {
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::UnsupportedFeature,
            "ModelLoader",
            "Only triangle primitives are supported",
            Key,
            Path,
            "ModelLoader");
        return false;
    }

    const auto* PositionAttribute = Primitive.findAttribute("POSITION");
    if (PositionAttribute == Primitive.attributes.end())
    {
        OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "ModelLoader", "Primitive missing POSITION", Key, Path, "ModelLoader");
        return false;
    }

    const fastgltf::Accessor& PositionAccessor = Asset.accessors[PositionAttribute->accessorIndex];
    std::vector<fastgltf::math::fvec3> Positions(PositionAccessor.count);
    fastgltf::copyFromAccessor<fastgltf::math::fvec3>(Asset, PositionAccessor, Positions.data());

    std::vector<fastgltf::math::fvec3> Normals;
    const auto* NormalAttribute = Primitive.findAttribute("NORMAL");
    if (NormalAttribute != Primitive.attributes.end())
    {
        const fastgltf::Accessor& NormalAccessor = Asset.accessors[NormalAttribute->accessorIndex];
        Normals.resize(NormalAccessor.count);
        fastgltf::copyFromAccessor<fastgltf::math::fvec3>(Asset, NormalAccessor, Normals.data());
    }

    std::vector<fastgltf::math::fvec2> TexCoords;
    const auto* TexCoordAttribute = Primitive.findAttribute("TEXCOORD_0");
    if (TexCoordAttribute != Primitive.attributes.end())
    {
        const fastgltf::Accessor& TexCoordAccessor = Asset.accessors[TexCoordAttribute->accessorIndex];
        TexCoords.resize(TexCoordAccessor.count);
        fastgltf::copyFromAccessor<fastgltf::math::fvec2>(Asset, TexCoordAccessor, TexCoords.data());
    }

    OutMesh.Vertices.resize(Positions.size());
    for (size_t Index = 0; Index < Positions.size(); ++Index)
    {
        StaticMeshVertex& Vertex = OutMesh.Vertices[Index];
        Vertex.Position = DirectX::SimpleMath::Vector3(Positions[Index][0], Positions[Index][1], Positions[Index][2]);
        if (Index < Normals.size())
        {
            Vertex.Normal = DirectX::SimpleMath::Vector3(Normals[Index][0], Normals[Index][1], Normals[Index][2]);
            Vertex.bHasNormal = true;
        }
        if (Index < TexCoords.size())
        {
            Vertex.TexCoord = DirectX::SimpleMath::Vector2(TexCoords[Index][0], TexCoords[Index][1]);
            Vertex.bHasTexCoord = true;
        }
    }

    if (Primitive.indicesAccessor.has_value())
    {
        const fastgltf::Accessor& IndexAccessor = Asset.accessors[*Primitive.indicesAccessor];
        OutMesh.Indices.resize(IndexAccessor.count);
        if (IndexAccessor.componentType == fastgltf::ComponentType::UnsignedShort)
        {
            std::vector<uint16_t> ShortIndices(IndexAccessor.count);
            fastgltf::copyFromAccessor<uint16_t>(Asset, IndexAccessor, ShortIndices.data());
            for (size_t Index = 0; Index < ShortIndices.size(); ++Index)
            {
                OutMesh.Indices[Index] = ShortIndices[Index];
            }
        }
        else if (IndexAccessor.componentType == fastgltf::ComponentType::UnsignedInt)
        {
            fastgltf::copyFromAccessor<uint32_t>(Asset, IndexAccessor, OutMesh.Indices.data());
        }
        else if (IndexAccessor.componentType == fastgltf::ComponentType::UnsignedByte)
        {
            std::vector<uint8_t> ByteIndices(IndexAccessor.count);
            fastgltf::copyFromAccessor<uint8_t>(Asset, IndexAccessor, ByteIndices.data());
            for (size_t Index = 0; Index < ByteIndices.size(); ++Index)
            {
                OutMesh.Indices[Index] = ByteIndices[Index];
            }
        }
        else
        {
            OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::UnsupportedFeature, "ModelLoader", "Unsupported index component type", Key, Path, "ModelLoader");
            return false;
        }
    }
    else
    {
        OutMesh.Indices.resize(OutMesh.Vertices.size());
        for (uint32_t Index = 0; Index < static_cast<uint32_t>(OutMesh.Vertices.size()); ++Index)
        {
            OutMesh.Indices[Index] = Index;
        }
    }

    StaticMeshSubmesh Submesh{};
    Submesh.IndexOffset = 0;
    Submesh.IndexCount = static_cast<uint32_t>(OutMesh.Indices.size());
    Submesh.MaterialSlot = Primitive.materialIndex.has_value() ? static_cast<int32_t>(*Primitive.materialIndex) : 0;
    OutMesh.Submeshes.push_back(Submesh);
    OutMesh.Bounds = ComputeBounds(OutMesh.Vertices);
    return true;
}

std::shared_ptr<ModelDocument> BuildDocument(
    std::vector<uint8_t> BackingBytes,
    const ContentHash& Fingerprint,
    const fastgltf::Asset& Asset,
    AssetDiagnostic& OutDiagnostic,
    const AssetKey& Key,
    const std::string& Path)
{
    for (const fastgltf::Buffer& Buffer : Asset.buffers)
    {
        if (HasExternalUri(Buffer.data))
        {
            OutDiagnostic = RejectExternal(Key, Path);
            return nullptr;
        }
    }

    for (const fastgltf::Image& Image : Asset.images)
    {
        if (HasExternalUri(Image.data))
        {
            OutDiagnostic = RejectExternal(Key, Path);
            return nullptr;
        }
    }

    auto Document = std::make_shared<ModelDocument>();
    Document->Fingerprint = Fingerprint;
    Document->BackingBytes = std::move(BackingBytes);

    Document->Materials.reserve(Asset.materials.size());
    for (const fastgltf::Material& SourceMaterial : Asset.materials)
    {
        auto Material = std::make_shared<MaterialResource>();
        Material->BaseColor = DirectX::SimpleMath::Vector4(
            SourceMaterial.pbrData.baseColorFactor[0],
            SourceMaterial.pbrData.baseColorFactor[1],
            SourceMaterial.pbrData.baseColorFactor[2],
            SourceMaterial.pbrData.baseColorFactor[3]);
        Material->Metallic = SourceMaterial.pbrData.metallicFactor;
        Material->Roughness = SourceMaterial.pbrData.roughnessFactor;
        Document->Materials.push_back(std::move(Material));
    }

    Document->Meshes.reserve(Asset.meshes.size());
    for (size_t MeshIndex = 0; MeshIndex < Asset.meshes.size(); ++MeshIndex)
    {
        const fastgltf::Mesh& SourceMesh = Asset.meshes[MeshIndex];
        if (SourceMesh.primitives.empty())
        {
            continue;
        }

        auto MeshResource = std::make_shared<StaticMeshResource>();
        StaticMeshResource Combined{};
        for (const fastgltf::Primitive& Primitive : SourceMesh.primitives)
        {
            StaticMeshResource Part{};
            if (!BuildStaticMeshFromPrimitive(Asset, Primitive, Part, OutDiagnostic, Key, Path))
            {
                return nullptr;
            }

            const uint32_t VertexOffset = static_cast<uint32_t>(Combined.Vertices.size());
            const uint32_t IndexOffset = static_cast<uint32_t>(Combined.Indices.size());
            Combined.Vertices.insert(Combined.Vertices.end(), Part.Vertices.begin(), Part.Vertices.end());
            for (uint32_t IndexValue : Part.Indices)
            {
                Combined.Indices.push_back(IndexValue + VertexOffset);
            }
            for (StaticMeshSubmesh Submesh : Part.Submeshes)
            {
                Submesh.IndexOffset += IndexOffset;
                Combined.Submeshes.push_back(Submesh);
            }
        }

        Combined.Bounds = ComputeBounds(Combined.Vertices);
        *MeshResource = std::move(Combined);

        ModelMeshPart Part{};
        Part.Name = SourceMesh.name.empty() ? ("Mesh_" + std::to_string(MeshIndex)) : std::string(SourceMesh.name);
        Part.Mesh = MeshResource;
        Part.MaterialIndex = SourceMesh.primitives.front().materialIndex.has_value()
            ? static_cast<int32_t>(*SourceMesh.primitives.front().materialIndex)
            : -1;
        Document->Meshes.push_back(std::move(Part));
    }

    Document->Nodes.resize(Asset.nodes.size());
    for (size_t NodeIndex = 0; NodeIndex < Asset.nodes.size(); ++NodeIndex)
    {
        const fastgltf::Node& SourceNode = Asset.nodes[NodeIndex];
        ModelNode& Node = Document->Nodes[NodeIndex];
        Node.Name = SourceNode.name.empty() ? ("Node_" + std::to_string(NodeIndex)) : std::string(SourceNode.name);
        Node.LocalTransform = NodeLocalTransform(SourceNode);
        Node.ParentIndex = -1;
        Node.MeshIndex = SourceNode.meshIndex.has_value() ? static_cast<int32_t>(*SourceNode.meshIndex) : -1;
        Node.MaterialIndex = -1;
        if (Node.MeshIndex >= 0 && Node.MeshIndex < static_cast<int32_t>(Document->Meshes.size()))
        {
            Node.MaterialIndex = Document->Meshes[static_cast<size_t>(Node.MeshIndex)].MaterialIndex;
        }
    }

    for (size_t NodeIndex = 0; NodeIndex < Asset.nodes.size(); ++NodeIndex)
    {
        for (size_t ChildIndex : Asset.nodes[NodeIndex].children)
        {
            if (ChildIndex < Document->Nodes.size())
            {
                Document->Nodes[ChildIndex].ParentIndex = static_cast<int32_t>(NodeIndex);
            }
        }
    }

    return Document;
}
}

std::shared_ptr<const ModelDocument> ModelLoader::LoadOrGetSharedDocument(
    const std::filesystem::path& AbsolutePath,
    const ContentHash& ExpectedFingerprint,
    AssetDiagnostic& OutDiagnostic)
{
    const std::string PathKey = AbsolutePath.lexically_normal().string();

    {
        std::lock_guard<std::mutex> Lock(SharedDocumentsMutex);
        auto Iterator = SharedDocuments.find(PathKey);
        if (Iterator != SharedDocuments.end())
        {
            if (ExpectedFingerprint.IsValid() && Iterator->second.Fingerprint != ExpectedFingerprint)
            {
                OutDiagnostic = AssetDiagnostic::Fail(
                    AssetErrorCode::AssetChanged,
                    "ModelLoader",
                    "Source fingerprint mismatch for cached document",
                    {},
                    PathKey,
                    "ModelLoader");
                return nullptr;
            }
            return Iterator->second.Document;
        }
    }

    std::vector<uint8_t> FileBytes;
    AssetKey EmptyKey{};
    if (!TryReadFileBytes(AbsolutePath, FileBytes, OutDiagnostic, EmptyKey))
    {
        return nullptr;
    }

    ContentHash ActualHash = ContentHash::FromBytes(FileBytes.data(), FileBytes.size());
    if (ExpectedFingerprint.IsValid() && ActualHash != ExpectedFingerprint)
    {
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::AssetChanged,
            "ModelLoader",
            "Source fingerprint mismatch",
            {},
            PathKey,
            "ModelLoader");
        return nullptr;
    }

    auto DataBufferResult = fastgltf::GltfDataBuffer::FromBytes(
        reinterpret_cast<const std::byte*>(FileBytes.data()),
        FileBytes.size());
    if (!DataBufferResult)
    {
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "ModelLoader",
            "Failed to create GLB data buffer",
            {},
            PathKey,
            "ModelLoader");
        return nullptr;
    }

    fastgltf::Parser Parser;
    auto AssetResult = Parser.loadGltfBinary(DataBufferResult.get(), AbsolutePath.parent_path(), fastgltf::Options::None);
    if (!AssetResult)
    {
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "ModelLoader",
            std::string("fastgltf parse failed: ") + std::string(fastgltf::getErrorMessage(AssetResult.error())),
            {},
            PathKey,
            "ModelLoader");
        return nullptr;
    }

    std::shared_ptr<ModelDocument> Document = BuildDocument(
        std::move(FileBytes),
        ActualHash,
        AssetResult.get(),
        OutDiagnostic,
        EmptyKey,
        PathKey);
    if (!Document)
    {
        return nullptr;
    }

    std::shared_ptr<const ModelDocument> ConstDocument = std::move(Document);
    {
        std::lock_guard<std::mutex> Lock(SharedDocumentsMutex);
        SharedDocumentEntry Entry{};
        Entry.Fingerprint = ActualHash;
        Entry.Document = ConstDocument;
        SharedDocuments[PathKey] = Entry;
    }

    return ConstDocument;
}

void ModelLoader::ClearSharedDocuments()
{
    std::lock_guard<std::mutex> Lock(SharedDocumentsMutex);
    SharedDocuments.clear();
}

std::shared_ptr<const void> ModelLoader::Load(const AssetLoadContext& Context, AssetDiagnostic& OutDiagnostic)
{
    const std::filesystem::path AbsolutePath(Context.Entry.AbsolutePath);
    const std::string Extension = AbsolutePath.extension().string();
    std::string LowerExtension = Extension;
    std::transform(LowerExtension.begin(), LowerExtension.end(), LowerExtension.begin(), [](unsigned char Character)
    {
        return static_cast<char>(std::tolower(Character));
    });

    if (LowerExtension != ".glb")
    {
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::UnsupportedFormat,
            "ModelLoader",
            "Only self-contained GLB is supported",
            Context.Key,
            Context.Entry.RelativePath,
            "ModelLoader");
        return nullptr;
    }

    std::shared_ptr<const ModelDocument> Document = LoadOrGetSharedDocument(
        AbsolutePath,
        Context.Entry.Metadata.SourceFingerprint,
        OutDiagnostic);
    if (!Document)
    {
        OutDiagnostic.Key = Context.Key;
        OutDiagnostic.Path = Context.Entry.RelativePath;
        return nullptr;
    }

    if (Context.SubAsset.has_value())
    {
        if (Context.SubAsset->Type == AssetType::StaticMesh)
        {
            const int32_t MeshIndex = Context.SubAsset->Selector.Index;
            if (MeshIndex < 0 || MeshIndex >= static_cast<int32_t>(Document->Meshes.size()) || !Document->Meshes[static_cast<size_t>(MeshIndex)].Mesh)
            {
                OutDiagnostic = AssetDiagnostic::Fail(
                    AssetErrorCode::NotFound,
                    "ModelLoader",
                    "StaticMesh subasset index out of range",
                    Context.Key,
                    Context.Entry.RelativePath,
                    "ModelLoader");
                return nullptr;
            }

            return Document->Meshes[static_cast<size_t>(MeshIndex)].Mesh;
        }

        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::TypeMismatch,
            "ModelLoader",
            "Unsupported model subasset type",
            Context.Key,
            Context.Entry.RelativePath,
            "ModelLoader");
        return nullptr;
    }

    auto Resource = std::make_shared<ModelResource>();
    Resource->Document = Document;
    return Resource;
}
