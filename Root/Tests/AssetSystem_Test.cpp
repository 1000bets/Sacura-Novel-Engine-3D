#include "Assets/AssetManager.h"
#include "Assets/AssetMetadata.h"
#include "Assets/AssetPath.h"
#include "Assets/AssetRegistry.h"
#include "Assets/ContentHash.h"
#include "Assets/Guid.h"
#include "Assets/Resources/MaterialResource.h"
#include "Assets/Resources/ModelResource.h"
#include "Assets/Resources/StaticMeshResource.h"
#include "Assets/Resources/TextureResource.h"
#include "Assets/Loaders/IAssetLoader.h"
#include "AssetTools/AssetImporter.h"
#include "AssetTools/ImportRequest.h"
#include "Core/Threading/JobSystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Engine.h"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>

#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
int FailureCount = 0;

void Expect(bool Condition, const char* Message)
{
    if (!Condition)
    {
        ++FailureCount;
        std::cout << "FAIL: " << Message << '\n';
    }
    else
    {
        std::cout << "  OK: " << Message << '\n';
    }
}

std::filesystem::path MakeTempRoot()
{
    const auto Stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path Root = std::filesystem::temp_directory_path() / ("SakuraAssetTest_" + std::to_string(Stamp));
    std::filesystem::create_directories(Root / "Content");
    std::filesystem::create_directories(Root / "Staging");
    std::filesystem::create_directories(Root / "External");
    return Root;
}

void WriteMinimalPng(const std::filesystem::path& Path)
{
    static const unsigned char Bytes[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
        0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00, 0x00,
        0x90, 0x77, 0x53, 0xDE,
        0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x9C, 0x63, 0xF8, 0xCF, 0xC0, 0x00, 0x00, 0x03, 0x01, 0x01, 0x00,
        0xC9, 0xFE, 0x92, 0xEF,
        0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44,
        0xAE, 0x42, 0x60, 0x82
    };
    std::filesystem::create_directories(Path.parent_path());
    std::ofstream Stream(Path, std::ios::binary);
    Stream.write(reinterpret_cast<const char*>(Bytes), sizeof(Bytes));
}

void WriteMinimalMaterial(const std::filesystem::path& Path, const AssetKey& TextureKey)
{
    nlohmann::json Document;
    Document["schemaVersion"] = 1;
    Document["baseColor"] = {1.0, 0.5, 0.25, 1.0};
    Document["metallic"] = 0.1;
    Document["roughness"] = 0.8;
    if (TextureKey.IsValid())
    {
        Document["baseColorTexture"] = AssetMetadataIO::AssetRefToJson(TextureKey);
    }
    std::ofstream Stream(Path);
    Stream << Document.dump(2);
}

bool PumpUntil(
    Engine& Eng,
    const AssetKey& Key,
    AssetLoadState Desired,
    int MaxFrames = 600)
{
    for (int Frame = 0; Frame < MaxFrames; ++Frame)
    {
        Eng.Tick(1.f / 60.f);
        if (Eng.GetAssetManager().GetLoadState(Key) == Desired)
        {
            return true;
        }
        if (Eng.GetAssetManager().GetLoadState(Key) == AssetLoadState::Failed
            || Eng.GetAssetManager().GetLoadState(Key) == AssetLoadState::Cancelled)
        {
            return Desired == AssetLoadState::Failed || Desired == AssetLoadState::Cancelled;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

void TestGuidAndMeta(const std::filesystem::path& Root)
{
    std::cout << "\n=== Guid / Metadata ===\n";
    const Guid First = Guid::Generate();
    const Guid Second = Guid::Generate();
    Expect(First.IsValid() && Second.IsValid() && First != Second, "Generate unique GUIDs");

    Guid Parsed{};
    Expect(Guid::TryParse(First.ToString(), Parsed) && Parsed == First, "Parse GUID roundtrip");

    AssetMetadata Metadata{};
    Metadata.Guid = First;
    Metadata.Type = TextureAssetType;
    Metadata.SchemaVersion = 1;
    SubAssetRecord Sub{};
    Sub.Id = Guid::Generate();
    Sub.Type = StaticMeshAssetType;
    Sub.Name = "Part";
    Sub.Selector = {"mesh", 0};
    Metadata.SubAssets.push_back(Sub);

    const auto MetaPath = Root / "meta_roundtrip.json";
    AssetDiagnostic Error{};
    Expect(AssetMetadataIO::TrySaveToFile(MetaPath.string(), Metadata, Error), "Save .meta");
    AssetMetadata Loaded{};
    Expect(AssetMetadataIO::TryLoadFromFile(MetaPath.string(), Loaded, Error), "Load .meta");
    Expect(Loaded.Guid == Metadata.Guid && Loaded.SubAssets.size() == 1, "Metadata preserves GUID/subassets");

    AssetType ParsedType = UnknownAssetType;
    Expect(TryParseAssetType(AssetTypeToString(SceneAssetType), ParsedType)
        && ParsedType == SceneAssetType, "Scene asset type roundtrip");
    Expect(TryParseAssetType(AssetTypeToString(StoryAssetType), ParsedType)
        && ParsedType == StoryAssetType, "Story asset type roundtrip");

    AssetMetadata Bad{};
    nlohmann::json Broken = {{"schemaVersion", 99}, {"guid", First.ToString()}, {"assetType", "Texture"}};
    Expect(!AssetMetadataIO::TryFromJson(Broken, Bad, Error) && Error.Code == AssetErrorCode::InvalidMetadata, "Reject unknown schemaVersion");
}

void TestRegistry(const std::filesystem::path& Root)
{
    std::cout << "\n=== AssetRegistry ===\n";
    Engine Eng;
    Eng.InitializeHeadless(Root / "Content");

    std::filesystem::create_directories(Root / "Content" / "Textures");
    WriteMinimalPng(Root / "Content" / "Textures" / "Red.png");

    AssetMetadata Metadata{};
    Metadata.Guid = Guid::Generate();
    Metadata.Type = TextureAssetType;
    ContentHash Hash{};
    AssetDiagnostic HashError{};
    Expect(ContentHash::TryHashFile((Root / "Content" / "Textures" / "Red.png").string(), Hash, HashError), "Hash texture");
    Metadata.SourceFingerprint = Hash;

    AssetDiagnostic RegisterError = Eng.GetAssetRegistry().RegisterExistingAsset("Textures/Red.png", Metadata);
    Expect(!RegisterError.HasError(), "RegisterExistingAsset texture");

    AssetRegistryEntry Entry{};
    Expect(Eng.GetAssetRegistry().TryGetById(Metadata.Guid, Entry), "Resolve by GUID");
    Expect(Eng.GetAssetRegistry().TryGetByPath("Textures/Red.png", Entry), "Resolve by path");

    AssetDiagnostic RenameError = Eng.GetAssetRegistry().RenamePair("Textures/Red.png", "Textures/RedRenamed.png");
    Expect(!RenameError.HasError(), "Rename pair keeps GUID");
    Expect(Eng.GetAssetRegistry().TryGetById(Metadata.Guid, Entry) && Entry.RelativePath == "Textures/RedRenamed.png", "GUID stable after rename");

    AssetMetadata Duplicate = Metadata;
    Duplicate.Guid = Metadata.Guid;
    WriteMinimalPng(Root / "Content" / "Textures" / "Other.png");
    AssetDiagnostic DupError = Eng.GetAssetRegistry().RegisterExistingAsset("Textures/Other.png", Duplicate);
    Expect(DupError.Code == AssetErrorCode::DuplicateId, "Duplicate GUID diagnosed");

    WriteMinimalPng(Root / "Content" / "Textures" / "Delete.png");
    AssetMetadata DeleteMetadata{};
    DeleteMetadata.Guid = Guid::Generate();
    DeleteMetadata.Type = TextureAssetType;
    Expect(
        ContentHash::TryHashFile(
            (Root / "Content" / "Textures" / "Delete.png").string(),
            DeleteMetadata.SourceFingerprint,
            HashError),
        "Hash asset for deletion");
    Expect(
        !Eng.GetAssetRegistry().RegisterExistingAsset("Textures/Delete.png", DeleteMetadata).HasError(),
        "Register asset for deletion");
    Expect(
        !Eng.GetAssetRegistry().DeletePair("Textures/Delete.png").HasError(),
        "Delete data and metadata pair");
    Expect(
        !std::filesystem::exists(Root / "Content" / "Textures" / "Delete.png")
            && !std::filesystem::exists(Root / "Content" / "Textures" / "Delete.png.meta"),
        "Deleted asset files are absent");
    Expect(!Eng.GetAssetRegistry().Exists(DeleteMetadata.Guid), "Deleted asset is unregistered");

    const std::filesystem::path CorruptMeta = Root / "Content" / "Textures" / "Broken.png.meta";
    {
        std::ofstream Stream(CorruptMeta);
        Stream << "{ not json";
    }
    WriteMinimalPng(Root / "Content" / "Textures" / "Broken.png");

    std::filesystem::create_directories(Root / "Content" / "Scenes");
    std::filesystem::create_directories(Root / "Content" / "Stories");
    std::filesystem::create_directories(Root / "Content" / "Dialogue");
    {
        std::ofstream SceneStream(Root / "Content" / "Scenes" / "Main.scene");
        SceneStream << R"({"format":"sakura.scene","version":1,"objects":[]})";
    }
    {
        std::ofstream StoryStream(Root / "Content" / "Stories" / "Demo.story");
        StoryStream << R"({"format":"sakura.story","version":1,"name":"Demo","startNodeId":"","nodes":[]})";
    }
    {
        std::ofstream DialogueStream(Root / "Content" / "Dialogue" / "Opening.dialogue");
        DialogueStream << R"({"line":"Hello"})";
    }

    const AssetType DialogueAssetType("game.Dialogue");
    Expect(
        !Eng.GetAssetRegistry().RegisterAssetType({
            DialogueAssetType,
            "Dialogue",
            {".dialogue"},
            true}).HasError(),
        "Register project-defined asset type");

    Eng.GetAssetRegistry().ScanContent();
    bool FoundCorrupt = false;
    for (const AssetDiagnostic& Diagnostic : Eng.GetAssetRegistry().GetScanDiagnostics())
    {
        if (Diagnostic.Code == AssetErrorCode::InvalidMetadata)
        {
            FoundCorrupt = true;
        }
    }
    Expect(FoundCorrupt, "Corrupt .meta diagnosed on scan");
    AssetRegistryEntry SceneEntry{};
    AssetRegistryEntry StoryEntry{};
    AssetRegistryEntry DialogueEntry{};
    Expect(
        Eng.GetAssetRegistry().TryGetByPath("Scenes/Main.scene", SceneEntry)
            && SceneEntry.Metadata.Type == SceneAssetType,
        "Metadata-less scene discovered as asset");
    Expect(
        Eng.GetAssetRegistry().TryGetByPath("Stories/Demo.story", StoryEntry)
            && StoryEntry.Metadata.Type == StoryAssetType,
        "Metadata-less story discovered as asset");
    Expect(
        Eng.GetAssetRegistry().TryGetByPath("Dialogue/Opening.dialogue", DialogueEntry)
            && DialogueEntry.Metadata.Type == DialogueAssetType,
        "Project-defined extension discovered without engine enum changes");
    Expect(
        std::filesystem::exists(Root / "Content" / "Scenes" / "Main.scene.meta")
            && std::filesystem::exists(Root / "Content" / "Stories" / "Demo.story.meta"),
        "Document metadata generated beside Content files");
    Expect(
        !Eng.GetAssetRegistry().DeletePair("Dialogue/Opening.dialogue").HasError(),
        "Project-defined asset uses common delete operation");

    Eng.Shutdown();
}

void TestImportAndLoad(const std::filesystem::path& Root)
{
    std::cout << "\n=== Import + Loading On Memory ===\n";
    Engine Eng;
    Eng.InitializeHeadless(Root / "Content");

    AssetImporter Importer(Eng.GetAssetRegistry());

    const auto ExternalPng = Root / "External" / "Source.png";
    WriteMinimalPng(ExternalPng);

    ImportRequest PngRequest{};
    PngRequest.SourcePath = ExternalPng;
    PngRequest.DestinationRelativePath = "Textures/Imported.png";
    PngRequest.StagingDirectory = Root / "Staging";
    ImportResult PngResult = Importer.Import(PngRequest);
    Expect(PngResult.Succeeded(), "Import PNG");
    Expect(std::filesystem::exists(Root / "Content" / "Textures" / "Imported.png.meta"), "PNG .meta published");

    ImportResult Conflict = Importer.Import(PngRequest);
    Expect(Conflict.Diagnostic.Code == AssetErrorCode::ImportConflict, "Import conflict without overwrite");

    AssetKey TextureKey{};
    TextureKey.Asset = PngResult.Metadata.Guid;
    Eng.GetAssetManager().LoadAsync<TextureResource>(TextureKey);
    if (!PumpUntil(Eng, TextureKey, AssetLoadState::Ready))
    {
        const AssetDiagnostic Diagnostic = Eng.GetAssetManager().GetLastDiagnostic(TextureKey);
        std::cout << "  Texture diagnostic: " << AssetErrorCodeToString(Diagnostic.Code) << " РІР‚вЂќ " << Diagnostic.Message
                  << " path=" << Diagnostic.Path << '\n';
        Expect(false, "Texture LoadAsync Ready");
    }
    else
    {
        Expect(true, "Texture LoadAsync Ready");
    }

    AssetHandle<TextureResource> TextureA;
    AssetHandle<TextureResource> TextureB;
    Expect(Eng.GetAssetManager().TryGetLoaded(TextureKey, TextureA), "TryGetLoaded texture A");
    Expect(Eng.GetAssetManager().TryGetLoaded(TextureKey, TextureB), "TryGetLoaded texture B");
    Expect(TextureA.IsValid() && TextureB.Get() == TextureA.Get(), "Shared immutable CPU texture");

    Eng.GetAssetManager().LoadAsync<TextureResource>(TextureKey);
    Expect(Eng.GetAssetManager().GetLoadState(TextureKey) == AssetLoadState::Ready, "Deduped load stays Ready");

    const auto MaterialPath = Root / "Content" / "Materials" / "Basic.material";
    std::filesystem::create_directories(MaterialPath.parent_path());
    WriteMinimalMaterial(MaterialPath, TextureKey);

    AssetMetadata MaterialMeta{};
    MaterialMeta.Guid = Guid::Generate();
    MaterialMeta.Type = MaterialAssetType;
    Expect(!Eng.GetAssetRegistry().RegisterExistingAsset("Materials/Basic.material", MaterialMeta).HasError(), "Register material");

    AssetKey MaterialKey{};
    MaterialKey.Asset = MaterialMeta.Guid;
    Eng.GetAssetManager().LoadAsync<MaterialResource>(MaterialKey);
    Expect(PumpUntil(Eng, MaterialKey, AssetLoadState::Ready), "Material with texture dependency Ready");

    AssetHandle<MaterialResource> Material;
    Expect(Eng.GetAssetManager().TryGetLoaded(MaterialKey, Material) && Material->BaseColorTexture.Key.Asset == TextureKey.Asset, "Material holds texture AssetRef");

    // Missing dependency РІвЂ вЂ™ Failed
    AssetKey MissingTexture{};
    MissingTexture.Asset = Guid::Generate();
    const auto BrokenMaterialPath = Root / "Content" / "Materials" / "Broken.material";
    WriteMinimalMaterial(BrokenMaterialPath, MissingTexture);
    AssetMetadata BrokenMeta{};
    BrokenMeta.Guid = Guid::Generate();
    BrokenMeta.Type = MaterialAssetType;
    Eng.GetAssetRegistry().RegisterExistingAsset("Materials/Broken.material", BrokenMeta);
    AssetKey BrokenKey{};
    BrokenKey.Asset = BrokenMeta.Guid;
    Eng.GetAssetManager().LoadAsync<MaterialResource>(BrokenKey);
    Expect(PumpUntil(Eng, BrokenKey, AssetLoadState::Failed), "Missing mandatory dependency Failed");
    Expect(Eng.GetAssetManager().GetLastDiagnostic(BrokenKey).Code == AssetErrorCode::DependencyFailed
        || Eng.GetAssetManager().GetLastDiagnostic(BrokenKey).Code == AssetErrorCode::NotFound,
        "Dependency failure diagnostic");

    Eng.GetAssetManager().InvalidateAsset(PngResult.Metadata.Guid);
    Expect(
        Eng.GetAssetManager().GetLoadState(TextureKey) == AssetLoadState::Unloaded,
        "Invalidate asset removes loaded slots");

    Eng.Shutdown();
}

void TestGlbCubeImportAndLoad(const std::filesystem::path& Root)
{
    std::cout << "\n=== GLB cube import + load ===\n";

    const auto ExternalGlb = Root / "External" / "Cube.glb";
    {
        std::vector<float> Positions = {
            -0.5f, -0.5f, -0.5f,
             0.5f, -0.5f, -0.5f,
             0.5f,  0.5f, -0.5f,
            -0.5f,  0.5f, -0.5f,
            -0.5f, -0.5f,  0.5f,
             0.5f, -0.5f,  0.5f,
             0.5f,  0.5f,  0.5f,
            -0.5f,  0.5f,  0.5f
        };
        std::vector<uint32_t> Indices = {
            0, 2, 1, 0, 3, 2,
            4, 5, 6, 4, 6, 7,
            0, 1, 5, 0, 5, 4,
            3, 7, 6, 3, 6, 2,
            0, 4, 7, 0, 7, 3,
            1, 2, 6, 1, 6, 5
        };
        std::vector<std::byte> BufferBytes(Positions.size() * sizeof(float) + Indices.size() * sizeof(uint32_t));
        std::memcpy(BufferBytes.data(), Positions.data(), Positions.size() * sizeof(float));
        std::memcpy(BufferBytes.data() + Positions.size() * sizeof(float), Indices.data(), Indices.size() * sizeof(uint32_t));

        fastgltf::Asset Asset{};
        {
            fastgltf::AssetInfo Info{};
            Info.gltfVersion = "2.0";
            Info.generator = "SakuraAssetTest";
            Asset.assetInfo = std::move(Info);
        }

        fastgltf::Buffer Buffer{};
        fastgltf::sources::Vector BufferSource{};
        BufferSource.bytes = BufferBytes;
        Buffer.byteLength = BufferSource.bytes.size();
        Buffer.data = std::move(BufferSource);
        Asset.buffers.push_back(std::move(Buffer));

        fastgltf::BufferView PositionView{};
        PositionView.bufferIndex = 0;
        PositionView.byteOffset = 0;
        PositionView.byteLength = Positions.size() * sizeof(float);
        PositionView.target = fastgltf::BufferTarget::ArrayBuffer;
        Asset.bufferViews.push_back(std::move(PositionView));

        fastgltf::BufferView IndexView{};
        IndexView.bufferIndex = 0;
        IndexView.byteOffset = Positions.size() * sizeof(float);
        IndexView.byteLength = Indices.size() * sizeof(uint32_t);
        IndexView.target = fastgltf::BufferTarget::ElementArrayBuffer;
        Asset.bufferViews.push_back(std::move(IndexView));

        fastgltf::Accessor PositionAccessor{};
        PositionAccessor.bufferViewIndex = 0;
        PositionAccessor.componentType = fastgltf::ComponentType::Float;
        PositionAccessor.count = Positions.size() / 3;
        PositionAccessor.type = fastgltf::AccessorType::Vec3;
        Asset.accessors.push_back(std::move(PositionAccessor));

        fastgltf::Accessor IndexAccessor{};
        IndexAccessor.bufferViewIndex = 1;
        IndexAccessor.componentType = fastgltf::ComponentType::UnsignedInt;
        IndexAccessor.count = Indices.size();
        IndexAccessor.type = fastgltf::AccessorType::Scalar;
        Asset.accessors.push_back(std::move(IndexAccessor));

        fastgltf::Primitive Primitive{};
        Primitive.type = fastgltf::PrimitiveType::Triangles;
        Primitive.indicesAccessor = 1;
        Primitive.attributes.emplace_back(fastgltf::Attribute{std::pmr::string("POSITION"), 0});

        fastgltf::Mesh Mesh{};
        Mesh.primitives.push_back(std::move(Primitive));
        Asset.meshes.push_back(std::move(Mesh));

        fastgltf::Node Node{};
        Node.meshIndex = 0;
        Asset.nodes.push_back(std::move(Node));

        fastgltf::Scene Scene{};
        Scene.nodeIndices.push_back(0);
        Asset.scenes.push_back(std::move(Scene));
        Asset.defaultScene = 0;

        fastgltf::Exporter Exporter;
        auto ExportResult = Exporter.writeGltfBinary(Asset, fastgltf::ExportOptions::None);
        Expect(static_cast<bool>(ExportResult), "Export cube GLB");
        if (!ExportResult)
        {
            return;
        }

        std::filesystem::create_directories(ExternalGlb.parent_path());
        std::ofstream Output(ExternalGlb, std::ios::binary);
        Output.write(
            reinterpret_cast<const char*>(ExportResult->output.data()),
            static_cast<std::streamsize>(ExportResult->output.size()));
        Expect(static_cast<bool>(Output), "Write cube GLB file");
    }

    Engine Eng;
    Eng.InitializeHeadless(Root / "Content");
    AssetImporter Importer(Eng.GetAssetRegistry());

    ImportRequest Request{};
    Request.SourcePath = ExternalGlb;
    Request.DestinationRelativePath = "Models/Cube.glb";
    Request.StagingDirectory = Root / "Staging";
    ImportResult Result = Importer.Import(Request);
    Expect(Result.Succeeded(), "Import cube GLB");
    if (!Result.Succeeded())
    {
        std::cout << "  Import error: " << Result.Diagnostic.Message << '\n';
        Eng.Shutdown();
        return;
    }

    Expect(Result.Metadata.SubAssets.size() == 1, "Cube import publishes one mesh subasset");
    if (Result.Metadata.SubAssets.size() != 1)
    {
        Eng.Shutdown();
        return;
    }
    const SubAssetRecord& CubeMeshSubAsset = Result.Metadata.SubAssets.front();
    Expect(CubeMeshSubAsset.Type == StaticMeshAssetType, "Cube subasset is StaticMesh");
    Expect(CubeMeshSubAsset.Name == "Cube", "Single imported mesh uses container name");
    Expect(CubeMeshSubAsset.Selector.Kind == "mesh", "Cube subasset selector kind");
    Expect(CubeMeshSubAsset.Selector.Index == 0, "Cube subasset selector index");

    ImportRequest DuplicateNameRequest = Request;
    DuplicateNameRequest.DestinationRelativePath = "Other/Cube.glb";
    ImportResult DuplicateNameResult = Importer.Import(DuplicateNameRequest);
    Expect(DuplicateNameResult.Succeeded(), "Import second cube with duplicate source name");
    Expect(
        DuplicateNameResult.Metadata.SubAssets.size() == 1
            && DuplicateNameResult.Metadata.SubAssets.front().Name == "Cube_2",
        "Imported mesh receives project-unique name");
    if (DuplicateNameResult.Succeeded() && DuplicateNameResult.Metadata.SubAssets.size() == 1)
    {
        AssetKey FirstMeshKey{};
        FirstMeshKey.Asset = Result.Metadata.Guid;
        FirstMeshKey.SubAsset = CubeMeshSubAsset.Id;
        Expect(
            Eng.GetAssetRegistry().RenameSubAsset(FirstMeshKey, "Cube_2").Code == AssetErrorCode::ImportConflict,
            "Subasset rename rejects duplicate project name");
        Expect(
            !Eng.GetAssetRegistry().RenameSubAsset(FirstMeshKey, "RenamedCube").HasError(),
            "Rename visible mesh asset");
        Expect(
            !Eng.GetAssetRegistry().RenameSubAsset(FirstMeshKey, "Cube").HasError(),
            "Restore visible mesh asset name");

        AssetKey SecondMeshKey{};
        SecondMeshKey.Asset = DuplicateNameResult.Metadata.Guid;
        SecondMeshKey.SubAsset = DuplicateNameResult.Metadata.SubAssets.front().Id;
        Expect(
            !Eng.GetAssetRegistry().DeleteSubAsset(SecondMeshKey).HasError(),
            "Delete last mesh leaf and backing pair");
        Expect(
            !Eng.GetAssetRegistry().Exists(DuplicateNameResult.Metadata.Guid)
                && !std::filesystem::exists(Root / "Content" / "Other" / "Cube.glb"),
            "Deleting last leaf removes hidden container");
    }

    std::filesystem::remove(ExternalGlb);

    AssetKey ModelKey{};
    ModelKey.Asset = Result.Metadata.Guid;
    Eng.GetAssetManager().LoadAsync<ModelResource>(ModelKey);
    Expect(PumpUntil(Eng, ModelKey, AssetLoadState::Ready), "Model LoadAsync Ready");

    AssetHandle<ModelResource> Model;
    Expect(Eng.GetAssetManager().TryGetLoaded(ModelKey, Model) && Model.IsValid(), "TryGetLoaded model");
    if (Model.IsValid() && Model->Document)
    {
        Expect(!Model->Document->Meshes.empty(), "Model has mesh parts");
    }

    AssetKey MeshKey{};
    MeshKey.Asset = Result.Metadata.Guid;
    MeshKey.SubAsset = CubeMeshSubAsset.Id;
    Eng.GetAssetManager().LoadAsync<StaticMeshResource>(MeshKey);
    Expect(PumpUntil(Eng, MeshKey, AssetLoadState::Ready), "Imported cube StaticMesh Ready");

    AssetHandle<StaticMeshResource> CubeMesh;
    Expect(
        Eng.GetAssetManager().TryGetLoaded(MeshKey, CubeMesh) && CubeMesh.IsValid(),
        "Imported cube StaticMesh loaded");
    if (CubeMesh.IsValid())
    {
        Expect(CubeMesh->Vertices.size() == 8, "Imported cube has eight vertices");
        Expect(CubeMesh->Indices.size() == 36, "Imported cube has twelve triangles");
    }

    Eng.Shutdown();
}

void TestAssetLoadContextOwnsSubAssetByValue()
{
    std::cout << "\n=== AssetLoadContext owns SubAsset by value ===\n";

    std::optional<AssetLoadContext> WorkerContext;
    SubAssetId ExpectedId = Guid::Generate();
    {
        AssetRegistryEntry Entry{};
        Entry.RelativePath = "Models/Owned.glb";
        Entry.Metadata.Guid = Guid::Generate();
        Entry.Metadata.Type = ModelAssetType;

        SubAssetRecord Record{};
        Record.Id = ExpectedId;
        Record.Type = StaticMeshAssetType;
        Record.Name = "Mesh0";
        Record.Selector.Kind = "mesh";
        Record.Selector.Index = 2;
        Entry.Metadata.SubAssets.push_back(Record);

        AssetLoadContext Local{};
        Local.Entry = Entry;
        BindLoadContextSubAsset(Local, &Entry.Metadata.SubAssets.front());
        Local.Key.Asset = Entry.Metadata.Guid;
        Local.Key.SubAsset = ExpectedId;

        WorkerContext = Local;
        Entry.Metadata.SubAssets.clear();
        Entry = AssetRegistryEntry{};
    }

    Expect(WorkerContext.has_value(), "Worker context survived producer scope");
    Expect(WorkerContext->SubAsset.has_value(), "SubAsset optional present after producer destroy");
    if (WorkerContext->SubAsset.has_value())
    {
        Expect(WorkerContext->SubAsset->Id == ExpectedId, "SubAssetId preserved");
        Expect(WorkerContext->SubAsset->Type == StaticMeshAssetType, "SubAsset Type preserved");
        Expect(WorkerContext->SubAsset->Selector.Index == 2, "Selector.Index preserved");
        Expect(WorkerContext->SubAsset->Name == "Mesh0", "SubAsset Name preserved");
    }

    AssetLoadContext Moved = std::move(*WorkerContext);
    WorkerContext.reset();
    Expect(Moved.SubAsset.has_value() && Moved.SubAsset->Selector.Index == 2, "Move preserves SubAsset");
}

void TestSubAssetLoadAfterProducerReturns(const std::filesystem::path& Root)
{
    std::cout << "\n=== SubAsset load after producer returns (gated worker) ===\n";

    const auto ExternalGlb = Root / "External" / "GateTriangle.glb";
    {
        std::vector<float> Positions = {
            -0.5f, -0.5f, 0.f,
             0.5f, -0.5f, 0.f,
             0.0f,  0.5f, 0.f
        };
        std::vector<uint32_t> Indices = {0, 1, 2};
        std::vector<std::byte> BufferBytes(Positions.size() * sizeof(float) + Indices.size() * sizeof(uint32_t));
        std::memcpy(BufferBytes.data(), Positions.data(), Positions.size() * sizeof(float));
        std::memcpy(BufferBytes.data() + Positions.size() * sizeof(float), Indices.data(), Indices.size() * sizeof(uint32_t));

        fastgltf::Asset Asset{};
        {
            fastgltf::AssetInfo Info{};
            Info.gltfVersion = "2.0";
            Info.generator = "SakuraAssetTest";
            Asset.assetInfo = std::move(Info);
        }

        fastgltf::Buffer Buffer{};
        fastgltf::sources::Vector BufferSource{};
        BufferSource.bytes = BufferBytes;
        Buffer.byteLength = BufferSource.bytes.size();
        Buffer.data = std::move(BufferSource);
        Asset.buffers.push_back(std::move(Buffer));

        fastgltf::BufferView PositionView{};
        PositionView.bufferIndex = 0;
        PositionView.byteOffset = 0;
        PositionView.byteLength = Positions.size() * sizeof(float);
        PositionView.target = fastgltf::BufferTarget::ArrayBuffer;
        Asset.bufferViews.push_back(std::move(PositionView));

        fastgltf::BufferView IndexView{};
        IndexView.bufferIndex = 0;
        IndexView.byteOffset = Positions.size() * sizeof(float);
        IndexView.byteLength = Indices.size() * sizeof(uint32_t);
        IndexView.target = fastgltf::BufferTarget::ElementArrayBuffer;
        Asset.bufferViews.push_back(std::move(IndexView));

        fastgltf::Accessor PositionAccessor{};
        PositionAccessor.bufferViewIndex = 0;
        PositionAccessor.componentType = fastgltf::ComponentType::Float;
        PositionAccessor.count = 3;
        PositionAccessor.type = fastgltf::AccessorType::Vec3;
        Asset.accessors.push_back(std::move(PositionAccessor));

        fastgltf::Accessor IndexAccessor{};
        IndexAccessor.bufferViewIndex = 1;
        IndexAccessor.componentType = fastgltf::ComponentType::UnsignedInt;
        IndexAccessor.count = 3;
        IndexAccessor.type = fastgltf::AccessorType::Scalar;
        Asset.accessors.push_back(std::move(IndexAccessor));

        fastgltf::Primitive Primitive{};
        Primitive.type = fastgltf::PrimitiveType::Triangles;
        Primitive.indicesAccessor = 1;
        Primitive.attributes.emplace_back(fastgltf::Attribute{std::pmr::string("POSITION"), 0});

        fastgltf::Mesh Mesh{};
        Mesh.primitives.push_back(std::move(Primitive));
        Asset.meshes.push_back(std::move(Mesh));

        fastgltf::Node Node{};
        Node.meshIndex = 0;
        Asset.nodes.push_back(std::move(Node));

        fastgltf::Scene Scene{};
        Scene.nodeIndices.push_back(0);
        Asset.scenes.push_back(std::move(Scene));
        Asset.defaultScene = 0;

        fastgltf::Exporter Exporter;
        auto ExportResult = Exporter.writeGltfBinary(Asset, fastgltf::ExportOptions::None);
        Expect(static_cast<bool>(ExportResult), "Export gated triangle GLB");
        if (!ExportResult)
        {
            return;
        }

        std::filesystem::create_directories(ExternalGlb.parent_path());
        std::ofstream Output(ExternalGlb, std::ios::binary);
        Output.write(
            reinterpret_cast<const char*>(ExportResult->output.data()),
            static_cast<std::streamsize>(ExportResult->output.size()));
        Expect(static_cast<bool>(Output), "Write gated triangle GLB file");
    }

    Engine Eng;
    Eng.InitializeHeadless(Root / "Content");

    AssetMetadata Metadata{};
    Metadata.Guid = Guid::Generate();
    Metadata.Type = ModelAssetType;
    SubAssetRecord MeshSub{};
    MeshSub.Id = Guid::Generate();
    MeshSub.Type = StaticMeshAssetType;
    MeshSub.Name = "Triangle";
    MeshSub.Selector.Kind = "mesh";
    MeshSub.Selector.Index = 0;
    Metadata.SubAssets.push_back(MeshSub);

    std::filesystem::create_directories(Root / "Content" / "Models");
    std::filesystem::copy_file(
        ExternalGlb,
        Root / "Content" / "Models" / "GateTriangle.glb",
        std::filesystem::copy_options::overwrite_existing);
    AssetDiagnostic RegisterError = Eng.GetAssetRegistry().RegisterExistingAsset("Models/GateTriangle.glb", Metadata);
    Expect(!RegisterError.HasError(), "Register gated model with StaticMesh subasset");

    auto ReleaseFlag = std::make_shared<std::atomic<bool>>(false);
    Eng.GetAssetManager().SetWorkerLoadReleaseFlag(ReleaseFlag);

    AssetKey MeshKey{};
    MeshKey.Asset = Metadata.Guid;
    MeshKey.SubAsset = MeshSub.Id;
    Eng.GetAssetManager().LoadAsync<StaticMeshResource>(MeshKey);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Expect(Eng.GetAssetManager().GetLoadState(MeshKey) != AssetLoadState::Ready, "Load still gated before release");

    ReleaseFlag->store(true, std::memory_order_release);
    Eng.GetAssetManager().ClearWorkerLoadReleaseFlag();

    Expect(PumpUntil(Eng, MeshKey, AssetLoadState::Ready), "StaticMesh subasset Ready after gate release");
    AssetHandle<StaticMeshResource> Mesh;
    Expect(Eng.GetAssetManager().TryGetLoaded(MeshKey, Mesh) && Mesh.IsValid(), "TryGetLoaded StaticMesh subasset");

    AssetKey WholeKey{};
    WholeKey.Asset = Metadata.Guid;
    Eng.GetAssetManager().LoadAsync<ModelResource>(WholeKey);
    Expect(PumpUntil(Eng, WholeKey, AssetLoadState::Ready), "Whole Model load Ready");
    Eng.GetAssetManager().LoadAsync<StaticMeshResource>(MeshKey);
    Expect(PumpUntil(Eng, MeshKey, AssetLoadState::Ready), "Repeated StaticMesh subasset request Ready");

    Eng.Shutdown();
}

void TestAssetManagerLifetimeAndCancel(const std::filesystem::path& Root)
{
    std::cout << "\n=== AssetManager lifetime / cancel / fault ===\n";

    {
        Engine Eng;
        Eng.InitializeHeadless(Root / "Content");
        WriteMinimalPng(Root / "Content" / "Textures" / "Life.png");

        AssetMetadata Metadata{};
        Metadata.Guid = Guid::Generate();
        Metadata.Type = TextureAssetType;
        Eng.GetAssetRegistry().RegisterExistingAsset("Textures/Life.png", Metadata);

        auto ReleaseFlag = std::make_shared<std::atomic<bool>>(false);
        Eng.GetAssetManager().SetWorkerLoadReleaseFlag(ReleaseFlag);

        AssetKey Key{};
        Key.Asset = Metadata.Guid;
        Eng.GetAssetManager().LoadAsync<TextureResource>(Key);
        Expect(Eng.GetAssetManager().GetLoadState(Key) != AssetLoadState::Ready, "Gated load not ready before release");

        Eng.GetAssetManager().CancelLoad(Key);
        ReleaseFlag->store(true, std::memory_order_release);
        Expect(PumpUntil(Eng, Key, AssetLoadState::Cancelled), "CancelLoad observed as Cancelled");
        Eng.GetAssetManager().ClearWorkerLoadReleaseFlag();

        Eng.GetAssetManager().SetWorkerLoadFault([]()
        {
            throw std::runtime_error("injected loader fault");
        });
        Eng.GetAssetManager().LoadAsync<TextureResource>(Key);
        Expect(PumpUntil(Eng, Key, AssetLoadState::Failed), "Loader exception becomes Failed");
        const AssetDiagnostic FaultDiagnostic = Eng.GetAssetManager().GetLastDiagnostic(Key);
        Expect(FaultDiagnostic.Code == AssetErrorCode::InternalError, "Fault diagnostic InternalError");
        Eng.GetAssetManager().ClearWorkerLoadFault();

        const uint64_t SessionBefore = Eng.GetAssetManager().GetSessionId();
        Eng.GetAssetManager().Shutdown();
        Eng.GetAssetManager().Initialize(Eng.GetAssetRegistry(), Eng.GetJobSystem());
        Expect(Eng.GetAssetManager().GetSessionId() != SessionBefore, "Re-Initialize bumps session");

        Eng.GetAssetManager().LoadAsync<TextureResource>(Key);
        Expect(PumpUntil(Eng, Key, AssetLoadState::Ready), "Load after re-Initialize Ready");
        Eng.Shutdown();
    }

    {
        SetCurrentThreadRole(ThreadRole::Game);
        JobSystem Jobs;
        Jobs.Initialize(2);
        AssetRegistry Registry;
        Registry.SetGameContentRoot(Root / "Content");

        AssetMetadata Metadata{};
        Metadata.Guid = Guid::Generate();
        Metadata.Type = TextureAssetType;
        Registry.RegisterExistingAsset("Textures/Life.png", Metadata);

        auto ReleaseFlag = std::make_shared<std::atomic<bool>>(false);
        {
            AssetManager Manager;
            Manager.Initialize(Registry, Jobs);
            Manager.SetWorkerLoadReleaseFlag(ReleaseFlag);

            AssetKey Key{};
            Key.Asset = Metadata.Guid;
            Manager.LoadAsync<TextureResource>(Key);

            // Destructor/Shutdown must wait for outstanding workers while JobSystem stays alive.
            ReleaseFlag->store(true, std::memory_order_release);
        }
        Expect(true, "AssetManager destructor with live JobSystem completed");
        Jobs.Shutdown();
    }

    {
        Engine Eng;
        Eng.InitializeHeadless(Root / "Content");

        AssetKey MissingTexture{};
        MissingTexture.Asset = Guid::Generate();
        const auto BrokenMaterialPath = Root / "Content" / "Materials" / "LifeBroken.material";
        WriteMinimalMaterial(BrokenMaterialPath, MissingTexture);

        AssetMetadata MaterialMeta{};
        MaterialMeta.Guid = Guid::Generate();
        MaterialMeta.Type = MaterialAssetType;
        Eng.GetAssetRegistry().RegisterExistingAsset("Materials/LifeBroken.material", MaterialMeta);

        AssetKey MaterialKey{};
        MaterialKey.Asset = MaterialMeta.Guid;
        Eng.GetAssetManager().LoadAsync<MaterialResource>(MaterialKey);
        Expect(PumpUntil(Eng, MaterialKey, AssetLoadState::Failed), "Dependency failure ends Material load");
        Eng.Shutdown();
    }
}

void TestHeadlessShutdown(const std::filesystem::path& Root)
{
    std::cout << "\n=== Shutdown during load ===\n";
    Engine Eng;
    Eng.InitializeHeadless(Root / "Content");

    WriteMinimalPng(Root / "Content" / "Textures" / "Shutdown.png");
    AssetMetadata Metadata{};
    Metadata.Guid = Guid::Generate();
    Metadata.Type = TextureAssetType;
    Eng.GetAssetRegistry().RegisterExistingAsset("Textures/Shutdown.png", Metadata);

    AssetKey Key{};
    Key.Asset = Metadata.Guid;
    Eng.GetAssetManager().LoadAsync<TextureResource>(Key);
    Eng.Shutdown();
    Expect(true, "Shutdown during load completed without crash");
}
}

void TestForwardMaterial(const std::filesystem::path& Root)
{
    const auto Path = Root / "Forward.material";
    MaterialLoader Loader;
    AssetLoadContext Context;
    Context.Entry.AbsolutePath = Path.string();
    auto Load = [&](const nlohmann::json& Document)
    {
        {
            std::ofstream Output(Path);
            Output << Document.dump();
        }
        AssetDiagnostic Diagnostic;
        return std::static_pointer_cast<const MaterialResource>(Loader.Load(Context, Diagnostic));
    };
    nlohmann::json Document = {{"schemaVersion", 1}, {"metallic", 0.7}, {"roughness", 0.2},
        {"alphaMode", "BLEND"}, {"baseColor", {0.2, 0.4, 0.8, 0.3}}, {"doubleSided", true},
        {"emissive", {4.0, 1.0, 0.0}}, {"castShadows", false}};
    const auto Material = Load(Document);
    Expect(Material && Material->AlphaMode == MaterialAlphaMode::Blend
        && Material->bDoubleSided && !Material->bCastShadows
        && Material->Emissive.x == 4.f, "Forward material fields survive loading");
    Document["alphaMode"] = "MASK";
    Document["alphaCutoff"] = 0.6;
    const auto Mask = Load(Document);
    Expect(Mask && Mask->AlphaMode == MaterialAlphaMode::Mask
        && Mask->AlphaCutoff == 0.6f, "Alpha mask contract");
    Document["roughness"] = -1.0;
    Expect(!Load(Document), "Negative roughness rejected");
    Document["roughness"] = "invalid";
    Expect(!Load(Document), "Invalid factor type rejected");
    Document["roughness"] = 0.5;
    Document["alphaMode"] = "unknown";
    Expect(!Load(Document), "Unknown alpha mode rejected");
    const auto Default = Load({{"schemaVersion", 1}});
    Expect(Default && Default->AlphaMode == MaterialAlphaMode::Opaque,
        "Existing material schema keeps opaque defaults");
}

int main()
{
    SetCurrentThreadRole(ThreadRole::Game);
    SetCurrentThreadDebugName("Game Thread");

    const std::filesystem::path Root = MakeTempRoot();
    TestForwardMaterial(Root);
    std::cout << "AssetSystem test root: " << Root.string() << '\n';

    TestGuidAndMeta(Root);
    TestRegistry(Root);
    TestAssetLoadContextOwnsSubAssetByValue();
    TestImportAndLoad(Root);
    TestGlbCubeImportAndLoad(Root);
    TestSubAssetLoadAfterProducerReturns(Root);
    TestAssetManagerLifetimeAndCancel(Root);
    TestHeadlessShutdown(Root);

    std::cout << "\nFailures: " << FailureCount << '\n';
    return FailureCount == 0 ? 0 : 1;
}
