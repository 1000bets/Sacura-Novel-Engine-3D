#include "Assets/AssetCreationFactory.h"
#include "Actions/SharedEditorRegistrars.h"
#include "Core/IO/AtomicFileWriter.h"
#include "Engine.h"
#include "Input/InputDocument.h"
#include <fstream>
#include <algorithm>
#include <iostream>

class CustomDocumentFactory : public AssetCreationFactory
{
    ENGINE_CLASS(CustomDocumentFactory, AssetCreationFactory, "test.CustomDocumentFactory")
public:
    int64_t Count = 3;
    std::string Label = "Default";
    ENGINE_REFLECT_FIELD(CustomDocumentFactory, Count, "count", RF_EDITOR_EDITABLE, RF_UI_RANGE(1.0, 10.0))
    ENGINE_REFLECT_FIELD(CustomDocumentFactory, Label, "label", RF_EDITOR_EDITABLE)
    AssetType GetAssetType() const override { return AssetType{"test.Document"}; }
    const char* GetDisplayName() const override { return "Custom document"; }
    const char* GetExtension() const override { return ".customdocument"; }
    bool Create(AssetRegistry& Registry, const std::filesystem::path& Destination, std::string& Error) const override
    {
        return AtomicFileWriter::WriteText(Destination, nlohmann::json{{"count", Count}, {"label", Label}}.dump(), Error);
    }
};
ENGINE_CLASS_END(CustomDocumentFactory)
ENGINE_IMPLEMENT_FIELD(CustomDocumentFactory, Count)
ENGINE_IMPLEMENT_FIELD(CustomDocumentFactory, Label)

int main(int ArgumentCount, char** Arguments)
{
    if (ArgumentCount != 2)
    {
        return 2;
    }
    ForceTouchSharedEditorRegistrars();
    (void)&CustomDocumentFactory::s_ReflectionClassRegistrar;
    Engine Runtime;
    Runtime.InitializeHeadless({});
    int Result = 0;
    try
    {
        const auto Root = std::filesystem::u8path(Arguments[1]) / Guid::Generate().ToString();
        std::filesystem::create_directories(Root);
        AssetRegistry Registry;
        Registry.SetGameContentRoot(Root);
        Registry.RegisterAssetType({AssetType{"test.Document"}, "Custom", {".customdocument"}, true});
        const auto Require = [](bool Value) {if (!Value) {throw std::runtime_error("Factory extension check failed");}};
        const auto Catalogue = AssetCreationFactory::DescribeFactories();
        const auto Factory = std::find_if(Catalogue.begin(), Catalogue.end(), [](const auto& Entry)
        { return Entry.at("id") == "test.CustomDocumentFactory"; });
        Require(Factory != Catalogue.end());
        Require(Factory->at("settings").size() == 2);
        AssetRegistryEntry Created;
        std::string Error;
        const TypeId FactoryType{"test.CustomDocumentFactory"};
        Require(AssetCreationFactory::CreateAsset(Registry, FactoryType, "/Game/First.customdocument",
            {{"count", 7}, {"label", "Configured"}}, Created, Error));
        std::ifstream File(Created.AbsolutePath);
        nlohmann::json Data;
        File >> Data;
        Require(Data.at("count") == 7 && Data.at("label") == "Configured");
        Require(AssetCreationFactory::CreateAsset(Registry, FactoryType, "/Game/Defaults.customdocument", nlohmann::json::object(), Created, Error));
        std::ifstream DefaultsFile(Created.AbsolutePath);
        DefaultsFile >> Data;
        Require(Data.at("count") == 3 && Data.at("label") == "Default");
        Require(!AssetCreationFactory::CreateAsset(Registry, FactoryType, "/Game/Rejected.customdocument", {{"count", 99}}, Created, Error));
        Require(!AssetCreationFactory::CreateAsset(Registry, FactoryType, "/Game/Unknown.customdocument", {{"extra", true}}, Created, Error));
        const TypeId InputFactoryType{"editor.CreateInputActionAssetFactory"};
        const auto InputFactory = std::find_if(Catalogue.begin(), Catalogue.end(), [&](const auto& Entry)
        {
            return Entry.at("id") == InputFactoryType.Value;
        });
        Require(InputFactory != Catalogue.end());
        const auto& InputSettings = InputFactory->at("settings");
        const auto ValueSetting = std::find_if(InputSettings.begin(), InputSettings.end(), [](const auto& Entry)
        {
            return Entry.at("id") == "value_type";
        });
        Require(ValueSetting != InputSettings.end());
        Require(ValueSetting->at("typeId") == "engine.InputValueType");
        Require(ValueSetting->at("value") == "Boolean");
        Require(ValueSetting->at("enumValues") == nlohmann::json({"Boolean", "Axis1D", "Axis2D", "Axis3D"}));
        Require(AssetCreationFactory::CreateAsset(Registry, InputFactoryType, "/Game/Movement.inputaction",
            {{"value_type", "Axis2D"}}, Created, Error));
        Require(InputDocument::Read(Created.AbsolutePath, Data, Error));
        Require(Data.at("Action").get<InputAction>().ValueType == InputValueType::Axis2D);
        Require(!AssetCreationFactory::CreateAsset(Registry, InputFactoryType, "/Game/Invalid.inputaction",
            {{"value_type", "Axis4D"}}, Created, Error));
        Require(!std::filesystem::exists(Root / "Invalid.inputaction"));
        Require(AssetCreationFactory::CreateAsset(Registry, InputFactoryType, "/Game/DefaultInput.inputaction",
            nlohmann::json::object(), Created, Error));
        Require(InputDocument::Read(Created.AbsolutePath, Data, Error));
        Require(Data.at("Action").get<InputAction>().ValueType == InputValueType::Boolean);
        std::cout << "Reflected custom asset factory checks passed\n";
    }
    catch (const std::exception& Error)
    {
        std::cerr << Error.what() << '\n';
        Result = 1;
    }
    Runtime.Shutdown();
    return Result;
}
