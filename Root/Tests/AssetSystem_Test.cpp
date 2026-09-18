#include "Assets/AssetManager.h"
#include "Assets/AssetMetadata.h"
#include "Assets/AssetPath.h"
#include "Assets/AssetRegistry.h"
#include "Assets/ContentHash.h"
#include "Assets/Guid.h"
#include "Assets/Resources/MaterialResource.h"
#include "Assets/Resources/ModelResource.h"
#include "Assets/Resources/TextureResource.h"
#include "AssetTools/AssetImporter.h"
#include "AssetTools/ImportRequest.h"
#include "Core/Threading/JobSystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Engine.h"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>

#include <chrono>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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
    Metadata.Type = AssetType::Texture;
    Metadata.SchemaVersion = 1;
    SubAssetRecord Sub{};
    Sub.Id = Guid::Generate();
    Sub.Type = AssetType::StaticMesh;
    Sub.Name = "Part";
    Sub.Selector = {"mesh", 0};
    Metadata.SubAssets.push_back(Sub);

    const auto MetaPath = Root / "meta_roundtrip.json";
    AssetDiagnostic Error{};
    Expect(AssetMetadataIO::TrySaveToFile(MetaPath.string(), Metadata, Error), "Save .meta");
    AssetMetadata Loaded{};
    Expect(AssetMetadataIO::TryLoadFromFile(MetaPath.string(), Loaded, Error), "Load .meta");
    Expect(Loaded.Guid == Metadata.Guid && Loaded.SubAssets.size() == 1, "Metadata preserves GUID/subassets");

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
    Metadata.Type = AssetType::Texture;
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

    const std::filesystem::path CorruptMeta = Root / "Content" / "Textures" / "Broken.png.meta";
    {
        std::ofstream Stream(CorruptMeta);
        Stream << "{ not json";
    }
    WriteMinimalPng(Root / "Content" / "Textures" / "Broken.png");
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
        std::cout << "  Texture diagnostic: " << AssetErrorCodeToString(Diagnostic.Code) << " — " << Diagnostic.Message
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
    MaterialMeta.Type = AssetType::Material;
    Expect(!Eng.GetAssetRegistry().RegisterExistingAsset("Materials/Basic.material", MaterialMeta).HasError(), "Register material");

    AssetKey MaterialKey{};
    MaterialKey.Asset = MaterialMeta.Guid;
    Eng.GetAssetManager().LoadAsync<MaterialResource>(MaterialKey);
    Expect(PumpUntil(Eng, MaterialKey, AssetLoadState::Ready), "Material with texture dependency Ready");

    AssetHandle<MaterialResource> Material;
    Expect(Eng.GetAssetManager().TryGetLoaded(MaterialKey, Material) && Material->BaseColorTexture.Key.Asset == TextureKey.Asset, "Material holds texture AssetRef");

    // Missing dependency → Failed
    AssetKey MissingTexture{};
    MissingTexture.Asset = Guid::Generate();
    const auto BrokenMaterialPath = Root / "Content" / "Materials" / "Broken.material";
    WriteMinimalMaterial(BrokenMaterialPath, MissingTexture);
    AssetMetadata BrokenMeta{};
    BrokenMeta.Guid = Guid::Generate();
    BrokenMeta.Type = AssetType::Material;
    Eng.GetAssetRegistry().RegisterExistingAsset("Materials/Broken.material", BrokenMeta);
    AssetKey BrokenKey{};
    BrokenKey.Asset = BrokenMeta.Guid;
    Eng.GetAssetManager().LoadAsync<MaterialResource>(BrokenKey);
    Expect(PumpUntil(Eng, BrokenKey, AssetLoadState::Failed), "Missing mandatory dependency Failed");
    Expect(Eng.GetAssetManager().GetLastDiagnostic(BrokenKey).Code == AssetErrorCode::DependencyFailed
        || Eng.GetAssetManager().GetLastDiagnostic(BrokenKey).Code == AssetErrorCode::NotFound,
        "Dependency failure diagnostic");

    Eng.Shutdown();
}

void TestGlbCreateImportAndLoad(const std::filesystem::path& Root)
{
    std::cout << "\n=== GLB create + import + load ===\n";

    const auto ExternalGlb = Root / "External" / "Triangle.glb";
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
        Expect(static_cast<bool>(ExportResult), "Export triangle GLB");
        if (!ExportResult)
        {
            return;
        }

        std::filesystem::create_directories(ExternalGlb.parent_path());
        std::ofstream Output(ExternalGlb, std::ios::binary);
        Output.write(
            reinterpret_cast<const char*>(ExportResult->output.data()),
            static_cast<std::streamsize>(ExportResult->output.size()));
        Expect(static_cast<bool>(Output), "Write triangle GLB file");
    }

    Engine Eng;
    Eng.InitializeHeadless(Root / "Content");
    AssetImporter Importer(Eng.GetAssetRegistry());

    ImportRequest Request{};
    Request.SourcePath = ExternalGlb;
    Request.DestinationRelativePath = "Models/Triangle.glb";
    Request.StagingDirectory = Root / "Staging";
    ImportResult Result = Importer.Import(Request);
    Expect(Result.Succeeded(), "Import triangle GLB");
    if (!Result.Succeeded())
    {
        std::cout << "  Import error: " << Result.Diagnostic.Message << '\n';
        Eng.Shutdown();
        return;
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

    Eng.Shutdown();
}

void TestHeadlessShutdown(const std::filesystem::path& Root)
{
    std::cout << "\n=== Shutdown during load ===\n";
    Engine Eng;
    Eng.InitializeHeadless(Root / "Content");

    WriteMinimalPng(Root / "Content" / "Textures" / "Shutdown.png");
    AssetMetadata Metadata{};
    Metadata.Guid = Guid::Generate();
    Metadata.Type = AssetType::Texture;
    Eng.GetAssetRegistry().RegisterExistingAsset("Textures/Shutdown.png", Metadata);

    AssetKey Key{};
    Key.Asset = Metadata.Guid;
    Eng.GetAssetManager().LoadAsync<TextureResource>(Key);
    Eng.Shutdown();
    Expect(true, "Shutdown during load completed without crash");
}
}

int main()
{
    SetCurrentThreadRole(ThreadRole::Game);
    SetCurrentThreadDebugName("Game Thread");

    const std::filesystem::path Root = MakeTempRoot();
    std::cout << "AssetSystem test root: " << Root.string() << '\n';

    TestGuidAndMeta(Root);
    TestRegistry(Root);
    TestImportAndLoad(Root);
    TestGlbCreateImportAndLoad(Root);
    TestHeadlessShutdown(Root);

    std::cout << "\nFailures: " << FailureCount << '\n';
    return FailureCount == 0 ? 0 : 1;
}
