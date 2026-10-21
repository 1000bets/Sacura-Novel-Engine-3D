#include "Assets/StoryAssetCreation.h"
#include "Core/IO/AtomicFileWriter.h"
#include "Story/StoryDocumentIO.h"
#include <algorithm>
#include <cctype>

AssetDiagnostic StoryAssetCreation::Create(AssetRegistry& Registry, const std::string& VirtualPath, AssetKey& Created)
{
    AssetMount Mount;
    std::string Relative;
    AssetPath::TryParseVirtualPath(VirtualPath, Mount, Relative);
    const std::filesystem::path RelativePath(Relative);
    const auto Destination = Registry.GetGameContentRoot() / RelativePath;
    std::string Extension = Destination.extension().string();
    std::transform(Extension.begin(), Extension.end(), Extension.begin(),
                   [](unsigned char Letter) { return static_cast<char>(std::tolower(Letter)); });
    if (Registry.GetGameContentRoot().empty() || Mount != AssetMount::Game || Relative.empty() ||
        RelativePath.is_absolute() || RelativePath.has_root_name() || Extension != ".story" ||
        !AssetPath::IsInsideContent(Destination, Registry.GetGameContentRoot()))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "CreateStory",
                                     "Choose a .story destination inside Game Content");
    }
    std::error_code FilesystemError;
    if (!std::filesystem::is_directory(Destination.parent_path(), FilesystemError) ||
        std::filesystem::exists(Destination, FilesystemError) ||
        std::filesystem::exists(Destination.string() + ".meta", FilesystemError) ||
        Registry.LeafNameExists(Destination.stem().string()))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::ImportConflict, "CreateStory",
                                     "The destination already exists or its name is used by another asset");
    }
    StoryDocument Document;
    Document.Name = Destination.stem().u8string();
    std::string Text;
    const auto Serialized = StoryDocumentIO::SaveToJson(Document, Text);
    if (!Serialized.bOk)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "CreateStory", Serialized.Error);
    }
    std::string Error;
    if (!AtomicFileWriter::WriteText(Destination, Text, Error))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "CreateStory", Error);
    }
    AssetMetadata Metadata;
    Metadata.Guid = Guid::Generate();
    Metadata.Type = StoryAssetType;
    Metadata.SourceFingerprint = ContentHash::FromBytes(Text.data(), Text.size());
    AssetDiagnostic Diagnostic;
    if (!AssetMetadataIO::TrySaveToFile(Destination.string() + ".meta", Metadata, Diagnostic))
    {
        std::filesystem::remove(Destination, FilesystemError);
        return Diagnostic;
    }
    Diagnostic = Registry.RegisterExistingAsset(AssetPath::MakeVirtualPath(AssetMount::Game, Relative), Metadata);
    if (Diagnostic.HasError())
    {
        std::filesystem::remove(Destination, FilesystemError);
        std::filesystem::remove(Destination.string() + ".meta", FilesystemError);
        return Diagnostic;
    }
    Created = AssetKey{Metadata.Guid, {}};
    return AssetDiagnostic::Ok();
}
