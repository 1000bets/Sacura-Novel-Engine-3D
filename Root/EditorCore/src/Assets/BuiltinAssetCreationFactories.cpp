#include "Assets/BuiltinAssetCreationFactories.h"
#include <set>
#include "Documents/WidgetLibraryDocument.h"
#include "Core/EnginePaths.h"
#include "Assets/MaterialAssetCreation.h"
#include "Assets/StoryAssetCreation.h"
#include "Core/IO/AtomicFileWriter.h"
#include "Characters/CharacterDefinition.h"
#include "World/Scene.h"
#include "World/GameObject.h"
#include "World/Serialization/SceneSerializer.h"
#include "Prefab/PrefabLibrary.h"
#include "Input/InputDocument.h"

bool CreateWidgetLibraryAssetFactory::Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const
{
    nlohmann::json Document;
    if (!WidgetLibraryDocument::Read(EnginePaths::Config() / "UI" / "Default.widgets", Document, Error))
    {
        return false;
    }
    return AtomicFileWriter::WriteText(Destination, Document.dump(2) + "\n", Error);
}

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
    Action.bClampMagnitude = bClampMagnitude;
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

bool CreatePythonScriptAssetFactory::Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const
{
    const auto Relative = Destination.lexically_relative(Registry.GetGameContentRoot() / "Scripts");
    if (Relative.empty() || *Relative.begin() == "..")
    {
        Error = "Create Python scripts inside Content/Scripts";
        return false;
    }
    const std::string ClassName = Destination.stem().u8string();
    if (ClassName.empty() || !((ClassName.front() >= 'A' && ClassName.front() <= 'Z')
        || (ClassName.front() >= 'a' && ClassName.front() <= 'z') || ClassName.front() == '_'))
    {
        Error = "A Python script name must be a valid identifier";
        return false;
    }
    for (const char Character : ClassName)
    {
        if (!((Character >= 'A' && Character <= 'Z') || (Character >= 'a' && Character <= 'z')
            || (Character >= '0' && Character <= '9') || Character == '_'))
        {
            Error = "A Python script name must be a valid identifier";
            return false;
        }
    }
    const std::set<std::string> ReservedNames{"False", "None", "True", "and", "as", "assert", "async",
        "await", "break", "class", "continue", "def", "del", "elif", "else", "except", "finally",
        "for", "from", "global", "if", "import", "in", "is", "lambda", "nonlocal", "not", "or",
        "pass", "raise", "return", "try", "while", "with", "yield", "__init__"};
    if (ReservedNames.count(ClassName) != 0)
    {
        Error = "Choose a script name that is not a Python keyword or package initializer";
        return false;
    }
    auto ModulePath = Relative;
    ModulePath.replace_extension();
    std::string ModuleName = ModulePath.generic_u8string();
    for (char& Character : ModuleName)
    {
        if (Character == '/')
        {
            Character = '.';
        }
    }
    const std::string TypeIdentifier = "game." + ModuleName;
    const std::string Source = "from engine import ScriptComponent, register_class\n\n\n"
        "@register_class(type_id=\"" + TypeIdentifier + "\")\nclass " + ClassName
        + "(ScriptComponent):\n    def OnCreate(self):\n        pass\n\n"
        "    def OnUpdate(self, DeltaTime):\n        pass\n";
    return AtomicFileWriter::WriteText(Destination, Source, Error);
}

void ForceTouchBuiltinAssetCreationFactories()
{
    (void)&CreateWidgetLibraryAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreatePythonScriptAssetFactory::s_ReflectionClassRegistrar;
    (void)&AssetCreationFactory::s_ReflectionClassRegistrar;
    (void)&CreateSceneAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreateStoryAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreateMaterialAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreateMaterialInstanceAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreateCharacterAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreateInputActionAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreateInputContextAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreatePrefabAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreatePostProcessMaterialAssetFactory::s_ReflectionClassRegistrar;
    (void)&CreateUserInterfaceMaterialAssetFactory::s_ReflectionClassRegistrar;
}
