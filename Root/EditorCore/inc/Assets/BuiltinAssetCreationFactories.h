#pragma once
#include "Assets/AssetCreationFactory.h"
#include "Input/InputTypes.h"
#include "Materials/MaterialDefinition.h"

class CreatePythonScriptAssetFactory : public AssetCreationFactory
{
    ENGINE_CLASS(CreatePythonScriptAssetFactory, AssetCreationFactory, "editor.CreatePythonScriptAssetFactory")
public:
    AssetType GetAssetType() const override { return PythonScriptAssetType; }
    const char* GetDisplayName() const override { return "Python Script"; }
    const char* GetExtension() const override { return ".py"; }
    bool Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const override;
};
ENGINE_CLASS_END(CreatePythonScriptAssetFactory)

class CreateWidgetLibraryAssetFactory : public AssetCreationFactory
{
    ENGINE_CLASS(CreateWidgetLibraryAssetFactory, AssetCreationFactory, "editor.CreateWidgetLibraryAssetFactory")
public:
    AssetType GetAssetType() const override { return WidgetLibraryAssetType; }
    const char* GetDisplayName() const override { return "Библиотека виджетов UI"; }
    const char* GetExtension() const override { return ".widgets"; }
    bool Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const override;
};
ENGINE_CLASS_END(CreateWidgetLibraryAssetFactory)

class CreateSceneAssetFactory : public AssetCreationFactory
{
    ENGINE_CLASS(CreateSceneAssetFactory, AssetCreationFactory, "editor.CreateSceneAssetFactory")
public:
    AssetType GetAssetType() const override { return SceneAssetType; }
    const char* GetDisplayName() const override { return "Сцена"; }
    const char* GetExtension() const override { return ".scene"; }
    bool Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const override;
};
ENGINE_CLASS_END(CreateSceneAssetFactory)

class CreateStoryAssetFactory : public AssetCreationFactory
{
    ENGINE_CLASS(CreateStoryAssetFactory, AssetCreationFactory, "editor.CreateStoryAssetFactory")
public:
    AssetType GetAssetType() const override { return StoryAssetType; }
    const char* GetDisplayName() const override { return "История"; }
    const char* GetExtension() const override { return ".story"; }
    bool Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const override;
};
ENGINE_CLASS_END(CreateStoryAssetFactory)

class CreateMaterialAssetFactory : public AssetCreationFactory
{
    ENGINE_CLASS(CreateMaterialAssetFactory, AssetCreationFactory, "editor.CreateMaterialAssetFactory")
public:
    AssetType GetAssetType() const override { return MaterialAssetType; }
    const char* GetDisplayName() const override { return "Материал"; }
    const char* GetExtension() const override { return ".material"; }
    bool Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const override;
    virtual MaterialDomain GetDomain() const { return MaterialDomain::Surface; }
};
ENGINE_CLASS_END(CreateMaterialAssetFactory)

class CreateMaterialInstanceAssetFactory : public AssetCreationFactory
{
    ENGINE_CLASS(CreateMaterialInstanceAssetFactory, AssetCreationFactory, "editor.CreateMaterialInstanceAssetFactory")
public:
    AssetType GetAssetType() const override { return MaterialInstanceAssetType; }
    const char* GetDisplayName() const override { return "Экземпляр материала"; }
    const char* GetExtension() const override { return ".materialinstance"; }
    bool Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const override;
    bool IsSettingRequired(const PropertyId& Property) const override { return Property.Value == "parent"; }
    AssetKey Parent = {};
    ENGINE_REFLECT_FIELD(CreateMaterialInstanceAssetFactory, Parent, "parent", RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Родительский материал"), RF_ASSET_TYPES("Material", "MaterialInstance"))
};
ENGINE_CLASS_END(CreateMaterialInstanceAssetFactory)
ENGINE_IMPLEMENT_FIELD(CreateMaterialInstanceAssetFactory, Parent)

class CreateCharacterAssetFactory : public AssetCreationFactory
{
    ENGINE_CLASS(CreateCharacterAssetFactory, AssetCreationFactory, "editor.CreateCharacterAssetFactory")
public:
    AssetType GetAssetType() const override { return CharacterAssetType; }
    const char* GetDisplayName() const override { return "Персонаж"; }
    const char* GetExtension() const override { return ".character"; }
    bool Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const override;
    AssetKey Model = {};
    ENGINE_REFLECT_FIELD(CreateCharacterAssetFactory, Model, "model", RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Модель"), RF_ASSET_TYPES("StaticMesh", "SkeletalMesh"))
    std::string Description = {};
    ENGINE_REFLECT_FIELD(CreateCharacterAssetFactory, Description, "description", RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Описание"))
};
ENGINE_CLASS_END(CreateCharacterAssetFactory)
ENGINE_IMPLEMENT_FIELD(CreateCharacterAssetFactory, Model)
ENGINE_IMPLEMENT_FIELD(CreateCharacterAssetFactory, Description)

class CreateInputActionAssetFactory : public AssetCreationFactory
{
    ENGINE_CLASS(CreateInputActionAssetFactory, AssetCreationFactory, "editor.CreateInputActionAssetFactory")
public:
    AssetType GetAssetType() const override { return InputActionAssetType; }
    const char* GetDisplayName() const override { return "Действие ввода"; }
    const char* GetExtension() const override { return ".inputaction"; }
    bool Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const override;
    InputValueType ValueType = InputValueType::Boolean;
    ENGINE_REFLECT_FIELD(CreateInputActionAssetFactory, ValueType, "value_type", RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Тип значения"))
    bool bExecuteWhenPaused = false;
    ENGINE_REFLECT_FIELD(CreateInputActionAssetFactory, bExecuteWhenPaused, "execute_when_paused", RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Выполнять на паузе"))
    bool bClampMagnitude = true;
    ENGINE_REFLECT_FIELD(CreateInputActionAssetFactory, bClampMagnitude, "clamp_magnitude", RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Ограничивать величину единицей"))
};
ENGINE_CLASS_END(CreateInputActionAssetFactory)
ENGINE_IMPLEMENT_FIELD(CreateInputActionAssetFactory, ValueType)
ENGINE_IMPLEMENT_FIELD(CreateInputActionAssetFactory, bExecuteWhenPaused)
ENGINE_IMPLEMENT_FIELD(CreateInputActionAssetFactory, bClampMagnitude)

class CreateInputContextAssetFactory : public AssetCreationFactory
{
    ENGINE_CLASS(CreateInputContextAssetFactory, AssetCreationFactory, "editor.CreateInputContextAssetFactory")
public:
    AssetType GetAssetType() const override { return InputMappingContextAssetType; }
    const char* GetDisplayName() const override { return "Контекст ввода"; }
    const char* GetExtension() const override { return ".inputcontext"; }
    bool Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const override;
};
ENGINE_CLASS_END(CreateInputContextAssetFactory)

class CreatePrefabAssetFactory : public AssetCreationFactory
{
    ENGINE_CLASS(CreatePrefabAssetFactory, AssetCreationFactory, "editor.CreatePrefabAssetFactory")
public:
    AssetType GetAssetType() const override { return PrefabAssetType; }
    const char* GetDisplayName() const override { return "Префаб"; }
    const char* GetExtension() const override { return ".prefab"; }
    bool Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const override;
};
ENGINE_CLASS_END(CreatePrefabAssetFactory)



class CreatePostProcessMaterialAssetFactory : public CreateMaterialAssetFactory
{
    ENGINE_CLASS(CreatePostProcessMaterialAssetFactory, CreateMaterialAssetFactory, "editor.CreatePostProcessMaterialAssetFactory")
public:
    const char* GetDisplayName() const override { return "Материал постобработки"; }
    MaterialDomain GetDomain() const override { return MaterialDomain::PostProcess; }
};
ENGINE_CLASS_END(CreatePostProcessMaterialAssetFactory)

class CreateUserInterfaceMaterialAssetFactory : public CreateMaterialAssetFactory
{
    ENGINE_CLASS(CreateUserInterfaceMaterialAssetFactory, CreateMaterialAssetFactory, "editor.CreateUserInterfaceMaterialAssetFactory")
public:
    const char* GetDisplayName() const override { return "Материал интерфейса"; }
    MaterialDomain GetDomain() const override { return MaterialDomain::UserInterface; }
};
ENGINE_CLASS_END(CreateUserInterfaceMaterialAssetFactory)

