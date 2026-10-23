#include "Assets/BuiltinAssetCreationFactories.h"
#include "Assets/MaterialAssetCreation.h"
#include "Assets/StoryAssetCreation.h"
#include "Core/IO/AtomicFileWriter.h"
#include "Characters/CharacterDefinition.h"
#include "World/Scene.h"
#include "World/GameObject.h"
#include "World/Serialization/SceneSerializer.h"
#include "Prefab/PrefabLibrary.h"
#include "Input/InputDocument.h"
#include "UI/WidgetLayout.h"
#include <QJsonDocument>

bool CreateSceneAssetFactory::Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const
{
    std::unique_ptr<Scene> Empty(MemorySubsystem::Get()->NewObject<Scene>());
    const auto Result = SceneSerializer::SerializeToFile(*Empty, Destination);
    Error = Result.Error;
    return Result.bOk;
}

bool CreateStoryAssetFactory::Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const
{
    AssetKey Created;
    const auto Relative = std::filesystem::relative(Destination, Registry.GetGameContentRoot()).generic_u8string();
    const auto Result = StoryAssetCreation::Create(Registry, "/Game/" + Relative, Created);
    Error = Result.Message;
    return !Result.HasError();
}

bool CreateMaterialAssetFactory::Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const
{
    AssetKey Created;
    const auto Relative = std::filesystem::relative(Destination, Registry.GetGameContentRoot()).generic_u8string();
    const auto Result = MaterialAssetCreation::Create(Registry, "/Game/" + Relative, {}, GetDomain(), Created);
    Error = Result.Message;
    return !Result.HasError();
}

bool CreateMaterialInstanceAssetFactory::Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const
{
    AssetKey Created;
    const auto Relative = std::filesystem::relative(Destination, Registry.GetGameContentRoot()).generic_u8string();
    const auto Result = MaterialAssetCreation::Create(Registry, "/Game/" + Relative, Parent, MaterialDomain::Surface, Created);
    Error = Result.Message;
    return !Result.HasError();
}

bool CreateCharacterAssetFactory::Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const
{
    auto Character = CharacterDefinition::Create(Destination.stem().u8string());
    Character.Model = Model;
    Character.Description = Description;
    return AtomicFileWriter::WriteText(Destination, Character.ToJson().dump(2), Error);
}

bool CreateInputActionAssetFactory::Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const
{
    InputAction Action;
    Action.Name = Destination.stem().u8string();
    Action.Identifier = Guid::Generate().ToString();
    Action.ValueType = ValueType;
    Action.bExecuteWhenPaused = bExecuteWhenPaused;
    return InputDocument::Write(Destination, {{"Action", Action}}, Error);
}

bool CreateInputContextAssetFactory::Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const
{
    InputMappingContext Context;
    Context.Name = Destination.stem().u8string();
    Context.Identifier = Guid::Generate().ToString();
    return InputDocument::Write(Destination, {{"Context", Context}}, Error);
}

bool CreatePrefabAssetFactory::Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const
{
    std::unique_ptr<Scene> Empty(MemorySubsystem::Get()->NewObject<Scene>());
    Empty->CreateGameObject(Destination.stem().u8string());
    std::string Serialized;
    const auto Result = SceneSerializer::SerializeToJson(*Empty, Serialized);
    if (!Result.bOk)
    {
        Error = Result.Error;
        return false;
    }
    return PrefabLibrary::WriteDocumentFile(Destination,
        PrefabLibrary::MakeDocument(Destination.stem().u8string(), nlohmann::json::parse(Serialized).at("objects")), Error);
}

bool CreateWidgetLibraryAssetFactory::Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const
{
    return AtomicFileWriter::WriteText(Destination, QJsonDocument(WidgetLayout::Defaults()).toJson().toStdString(), Error);
}

void ForceTouchBuiltinAssetCreationFactories()
{
    (void)&AssetCreationFactory::s_ReflectionClassRegistrar;
    (void)&CreateSceneAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreateStoryAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreateMaterialAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreateMaterialInstanceAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreateCharacterAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreateInputActionAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreateInputContextAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreatePrefabAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreateWidgetLibraryAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreatePostProcessMaterialAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreateUserInterfaceMaterialAssetFactory::s_ReflectionClassRegistrar;
}
