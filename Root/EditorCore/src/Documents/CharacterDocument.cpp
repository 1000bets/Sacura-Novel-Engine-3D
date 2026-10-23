#include "Documents/CharacterDocument.h"
#include "Assets/AssetImporter.h"
#include "Assets/CharacterAnimationImport.h"
#include "Core/IO/AtomicFileWriter.h"
#include "Commands/EditorCommands.h"
#include "World/Components/CharacterComponent.h"
#include "World/Scene.h"
#include "World/GameObject.h"
#include "World/Serialization/SceneSerializer.h"
#include "Core/Object/MemorySubsystem.h"
#include <algorithm>
#include <cctype>

bool CharacterDocument::Open(AssetRegistry& InRegistry, const AssetRegistryEntry& InEntry, std::string& Error)
{
    CharacterDefinition Candidate;
    if (InEntry.Metadata.Type != CharacterAssetType || !CharacterDefinition::Read(InEntry.AbsolutePath, Candidate, Error))
    {
        return false;
    }
    Registry = &InRegistry;
    Entry = InEntry;
    Data = std::move(Candidate);
    Saved = Data.ToJson();
    Past.clear();
    Future.clear();
    return true;
}

bool CharacterDocument::Apply(const nlohmann::json& Candidate, std::string& Error)
{
    if (IsReadOnly())
    {
        Error = "Engine characters are read-only";
        return false;
    }
    CharacterDefinition Parsed;
    if (!CharacterDefinition::Parse(Candidate, Parsed, Error))
    {
        return false;
    }
    if (Parsed.ToJson() != Data.ToJson())
    {
        Past.push_back(Data);
        Future.clear();
        Data = std::move(Parsed);
    }
    return true;
}

bool CharacterDocument::Save(AssetManager& Assets, std::string& Error)
{
    CharacterDefinition Current;
    AssetRegistryEntry CurrentEntry;
    if (IsReadOnly() || !Registry->TryGetById(Entry.Metadata.Guid, CurrentEntry))
    {
        Error = "Character asset is read-only or has been removed";
        return false;
    }
    if (!CharacterDefinition::Read(CurrentEntry.AbsolutePath, Current, Error))
    {
        return false;
    }
    if (Current.ToJson() != Saved)
    {
        Error = "Character changed on disk; reopen it before saving";
        return false;
    }
    if (!AtomicFileWriter::WriteText(CurrentEntry.AbsolutePath, Data.ToJson().dump(2), Error))
    {
        return false;
    }
    Entry = CurrentEntry;
    Saved = Data.ToJson();
    const std::string Serialized = Data.ToJson().dump(2);
    const auto Updated = Registry->UpdateSourceFingerprint(Entry.Metadata.Guid, ContentHash::FromBytes(Serialized.data(), Serialized.size()));
    Assets.InvalidateAsset(Entry.Metadata.Guid);
    if (Updated.HasError())
    {
        Error = Updated.Message;
        return false;
    }
    return true;
}

void CharacterDocument::Undo()
{
    if (!Past.empty())
    {
        Future.push_back(Data);
        Data = std::move(Past.back());
        Past.pop_back();
    }
}

void CharacterDocument::Redo()
{
    if (!Future.empty())
    {
        Past.push_back(Data);
        Data = std::move(Future.back());
        Future.pop_back();
    }
}

bool CharacterDocument::Create(AssetRegistry& Registry, const CharacterDefinition& Data, AssetRegistryEntry& Entry, std::string& Error)
{
    CharacterDefinition Validated;
    if (!CharacterDefinition::Parse(Data.ToJson(), Validated, Error))
    {
        return false;
    }
    if (Data.Name.find_first_of("/\\:*?\"<>|") != std::string::npos || Data.Name == "." || Data.Name == ".."
        || Data.Name.back() == '.' || Data.Name.back() == ' ')
    {
        Error = "Enter a valid character filename";
        return false;
    }
    const auto Filename = Data.Name + ".character";
    const auto Path = Registry.GetGameContentRoot() / "Characters" / std::filesystem::u8path(Filename);
    if (Registry.GetGameContentRoot().empty() || Registry.LeafNameExists(Filename) || std::filesystem::exists(Path))
    {
        Error = "Character name already exists or no project is open";
        return false;
    }
    if (!AtomicFileWriter::WriteText(Path, Data.ToJson().dump(2), Error))
    {
        return false;
    }
    Registry.ScanContent();
    if (!Registry.TryGetByPath("Characters/" + Filename, Entry))
    {
        Error = "Character asset registration failed";
        return false;
    }
    return true;
}

bool CharacterDocument::ImportModel(AssetRegistry& Registry, const std::filesystem::path& Source, bool bAnimationsOnly, std::string& Error)
{
    if (IsReadOnly())
    {
        Error = "Engine characters are read-only";
        return false;
    }
    ImportRequest Request;
    if (bAnimationsOnly && !Data.Model.IsValid())
    {
        Error = "Load a character model before importing animation files";
        return false;
    }
    std::string Extension = Source.extension().string();
    std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char Value)
    { return static_cast<char>(std::tolower(Value)); });
    if (bAnimationsOnly && Extension == ".fbx")
    {
        CharacterDefinition Candidate = Data;
        std::vector<CharacterAnimation> Imported;
        if (!ImportCharacterFbxAnimations(Registry, Source, Candidate, Imported, Error))
        {
            return false;
        }
        Candidate.Animations.insert(Candidate.Animations.end(), Imported.begin(), Imported.end());
        return Apply(Candidate.ToJson(), Error);
    }
    Request.SourcePath = Source;
    Request.DestinationRelativePath = "Characters/Models/" + Source.stem().u8string() + ".glb";
    Request.StagingDirectory = Registry.GetGameContentRoot().parent_path() / "Saved" / "CharacterImport";
    AssetImporter Importer(Registry);
    const auto Imported = Importer.Import(Request);
    if (!Imported.Succeeded())
    {
        Error = Imported.Diagnostic.Message;
        return false;
    }
    const auto RejectImport = [&](const std::string& Message)
    {
        Error = Message;
        Registry.DeletePair(Imported.PublishedRelativePath);
        return false;
    };
    CharacterDefinition Candidate = Data;
    if (!bAnimationsOnly)
    {
        Candidate.Model = {};
        Candidate.Skeleton = {};
        Candidate.Animations.clear();
        Candidate.IdleAnimation.clear();
        Candidate.WalkAnimation.clear();
    }
    for (const auto& Part : Imported.Metadata.SubAssets)
    {
        const AssetKey Key{Imported.Metadata.Guid, Part.Id};
        if (Part.Type == SkeletalMeshAssetType && !bAnimationsOnly && !Candidate.Model.IsValid())
        {
            Candidate.Model = Key;
        }
        if (Part.Type == SkeletonAssetType && !bAnimationsOnly && !Candidate.Skeleton.IsValid())
        {
            Candidate.Skeleton = Key;
        }
        if (Part.Type == AnimationClipAssetType)
        {
            CharacterAnimation Animation;
            Animation.Identifier = Part.Id.ToString();
            Animation.Name = Part.Name;
            Animation.Clip = Key;
            Candidate.Animations.push_back(Animation);
            if (Candidate.IdleAnimation.empty())
            {
                Candidate.IdleAnimation = Animation.Identifier;
            }
            std::string MotionName = Animation.Name;
            std::transform(MotionName.begin(), MotionName.end(), MotionName.begin(), [](unsigned char Value)
            { return static_cast<char>(std::tolower(Value)); });
            if (Candidate.WalkAnimation.empty() && MotionName.find("walk") != std::string::npos)
            {
                Candidate.WalkAnimation = Animation.Identifier;
            }
        }
    }
    if (!bAnimationsOnly && (!Candidate.Model.IsValid() || !Candidate.Skeleton.IsValid()))
    {
        return RejectImport("Imported model has no skeletal mesh or skeleton");
    }
    ModelLoader ModelLoaderInstance;
    SkeletalLoader Loader(ModelLoaderInstance);
    const auto Load = [&](const AssetKey& Key, AssetDiagnostic& Diagnostic)
    {
        AssetLoadContext Context;
        Context.Key = Key;
        const SubAssetRecord* Part = nullptr;
        if (!Registry.TryResolveKey(Key, Context.Entry, &Part))
        {
            Diagnostic = AssetDiagnostic::Fail(AssetErrorCode::NotFound, "CharacterImport", "Character dependency not found");
            return std::shared_ptr<const void>{};
        }
        BindLoadContextSubAsset(Context, Part);
        return Loader.Load(Context, Diagnostic);
    };
    AssetDiagnostic Diagnostic;
    const auto Skeleton = std::static_pointer_cast<const SkeletonResource>(Load(Candidate.Skeleton, Diagnostic));
    if (!Skeleton)
    {
        return RejectImport(Diagnostic.Message);
    }
    if (!bAnimationsOnly)
    {
        const auto Model = std::static_pointer_cast<const SkeletalMeshResource>(Load(Candidate.Model, Diagnostic));
        if (!Model)
        {
            return RejectImport(Diagnostic.Message);
        }
    }
    size_t ImportedCount = 0;
    for (const auto& Motion : Candidate.Animations)
    {
        if (Motion.Clip.Asset != Imported.Metadata.Guid)
        {
            continue;
        }
        ++ImportedCount;
        const auto Clip = std::static_pointer_cast<const AnimationClipResource>(Load(Motion.Clip, Diagnostic));
        if (!Clip)
        {
            return RejectImport(Diagnostic.Message);
        }
        if (Clip->JointNames.size() != static_cast<size_t>(Skeleton->Skeleton->num_joints()))
        {
            return RejectImport("Animation skeleton does not match the character");
        }
        for (int Joint = 0; Joint < Skeleton->Skeleton->num_joints(); ++Joint)
        {
            if (Clip->JointNames[Joint] != Skeleton->Skeleton->joint_names()[Joint])
            {
                return RejectImport("Animation joint names do not match the character");
            }
        }
    }
    if (bAnimationsOnly && ImportedCount == 0)
    {
        return RejectImport("The file contains no character animations");
    }
    return Apply(Candidate.ToJson(), Error);
}

std::shared_ptr<CharacterDocument> CharacterDocuments::Open(AssetRegistry& Registry, const AssetRegistryEntry& Entry, std::string& Error)
{
    const auto Identifier = Entry.Metadata.Guid.ToString();
    const auto Existing = Documents.find(Identifier);
    if (Existing != Documents.end())
    {
        return Existing->second;
    }
    auto Document = std::make_shared<CharacterDocument>();
    if (!Document->Open(Registry, Entry, Error))
    {
        return nullptr;
    }
    Documents[Identifier] = Document;
    return Document;
}

bool CharacterDocuments::Delete(AssetRegistry& Registry, AssetManager& Assets, const AssetRegistryEntry& Entry, std::string& Error)
{
    if (Entry.Mount == AssetMount::Engine || Entry.Metadata.Type != CharacterAssetType)
    {
        Error = "Only project character assets can be deleted";
        return false;
    }
    const auto Result = Registry.DeletePair(Entry.VirtualPath);
    if (Result.HasError())
    {
        Error = Result.Message;
        return false;
    }
    Assets.InvalidateAsset(Entry.Metadata.Guid);
    Documents.erase(Entry.Metadata.Guid.ToString());
    return true;
}

bool CharacterDocuments::SaveAll(AssetManager& Assets, std::string& Error)
{
    for (const auto& Entry : Documents)
    {
        if (Entry.second->IsDirty() && !Entry.second->Save(Assets, Error))
        {
            return false;
        }
    }
    return true;
}

bool CharacterDocuments::HasDirtyDocuments() const
{
    for (const auto& Entry : Documents)
    {
        if (Entry.second->IsDirty())
        {
            return true;
        }
    }
    return false;
}

bool InstantiateCharacter(Scene* World, EditorCommandStack& Commands, const AssetRegistryEntry& Entry,
    ObjectHandle& Created, std::string& Error)
{
    if (!World || World->IsPlaying() || Entry.Metadata.Type != CharacterAssetType)
    {
        Error = "An editable scene and a character asset are required";
        return false;
    }
    CharacterDefinition Definition;
    if (!CharacterDefinition::Read(Entry.AbsolutePath, Definition, Error))
    {
        return false;
    }
    auto* TemplateScene = MemorySubsystem::Get()->NewObject<Scene>();
    auto* TemplateObject = TemplateScene->CreateGameObject(Definition.Name);
    TemplateObject->AddComponent<CharacterComponent>()->Character.Asset = Entry.Metadata.Guid;
    std::string Serialized;
    const auto Captured = SceneSerializer::SerializeSubtreeToJson(*TemplateObject, Serialized);
    MemorySubsystem::Get()->DestroyObject(TemplateScene);
    if (!Captured.bOk)
    {
        Error = Captured.Error;
        return false;
    }
    Commands.BeginGroup("Create Character");
    if (!Commands.Execute(MakePasteSubtreeCommand(World, std::move(Serialized), {}, &Created)))
    {
        Commands.CancelGroup();
        Error = "Character instantiation failed";
        return false;
    }
    Commands.EndGroup();
    return true;
}
