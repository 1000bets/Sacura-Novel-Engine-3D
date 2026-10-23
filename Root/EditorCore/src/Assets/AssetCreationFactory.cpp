#include "Assets/AssetCreationFactory.h"
#include "Assets/AssetPath.h"
#include "Reflection/Json/ReflectionJson.h"
#include "Reflection/PropertyAccess.h"
#include "Core/Threading/ThreadContext.h"
#include <algorithm>

std::vector<AssetCreationFactory*> AssetCreationFactory::CollectFactories()
{
    AssertGameThread();
    std::vector<AssetCreationFactory*> Result;
    for (Class* Descriptor : ReflectionSubsystem::Get().GetPublishedCatalog().Classes)
    {
        if (Descriptor->IsConcrete() && Descriptor->IsA(TypeId{StaticReflectionTypeId()}))
        {
            Result.push_back(static_cast<AssetCreationFactory*>(Descriptor->GetClassDefaultObject()));
        }
    }
    std::sort(Result.begin(), Result.end(), [](const auto* Left, const auto* Right)
    {
        return Left->GetTypeId().Value < Right->GetTypeId().Value;
    });
    return Result;
}

nlohmann::json AssetCreationFactory::DescribeSettings() const
{
    auto Fields = nlohmann::json::array();
    for (const auto* Property : GetClass()->GetAllProperties())
    {
        if (!HasPropertyFlag(Property->Attributes.Flags, PropertyFlags::EditorEditable) || Property->Attributes.bEditorHidden)
        {
            continue;
        }
        ReflectedValue Value;
        auto Result = PropertyAccess::GetProperty(const_cast<AssetCreationFactory*>(this), *Property, Value);
        if (!Result.bOk)
        {
            continue;
        }
        nlohmann::json Encoded;
        Result = ReflectionJson::ReflectedValueToJson(Property->ValueTypeId, Value, Encoded);
        if (!Result.bOk)
        {
            continue;
        }
        if (Value.ValueKind == ReflectedValue::Kind::AssetReference && !Value.AssetValue.IsValid())
        {
            Encoded = nullptr;
        }
        std::string Label = Property->Id.Value;
        if (Property->Attributes.DisplayName)
        {
            Label = Property->Attributes.DisplayName;
        }
        nlohmann::json Field{{"id", Property->Id.Value}, {"typeId", Property->ValueTypeId.Value},
            {"label", Label}, {"value", Encoded}, {"editable", true}, {"required", IsSettingRequired(Property->Id)}};
        if (const auto* Enumeration = ReflectionSubsystem::Get().FindEnum(Property->ValueTypeId))
        {
            Field["enumValues"] = nlohmann::json::array();
            for (const auto& Entry : Enumeration->Values)
            {
                Field["enumValues"].push_back(Entry.Name);
            }
        }
        auto AssetTypes = Property->Attributes.AllowedAssetTypes;
        if (Property->Attributes.AssetTypeFilter)
        {
            AssetTypes.emplace_back(Property->Attributes.AssetTypeFilter);
        }
        if (Value.ValueKind == ReflectedValue::Kind::AssetReference || !AssetTypes.empty())
        {
            Field["assetTypes"] = AssetTypes;
        }
        if (Property->Attributes.bHasRange)
        {
            Field["range"] = {Property->Attributes.RangeMinimum, Property->Attributes.RangeMaximum};
        }
        Fields.push_back(std::move(Field));
    }
    return Fields;
}

nlohmann::json AssetCreationFactory::DescribeFactories()
{
    auto Result = nlohmann::json::array();
    for (const auto* Factory : CollectFactories())
    {
        Result.push_back({{"id", Factory->GetTypeId().Value}, {"label", Factory->GetDisplayName()},
            {"typeId", Factory->GetAssetType().GetIdentifier()}, {"extension", Factory->GetExtension()},
            {"category", Factory->GetCategory()}, {"settings", Factory->DescribeSettings()}});
    }
    return Result;
}

bool AssetCreationFactory::CreateAsset(AssetRegistry& Registry, const TypeId& FactoryType,
    const std::string& VirtualPath, const nlohmann::json& Settings, AssetRegistryEntry& Created, std::string& Error)
{
    AssertGameThread();
    const auto Factories = CollectFactories();
    const auto Found = std::find_if(Factories.begin(), Factories.end(), [&](const auto* Factory) { return Factory->GetTypeId() == FactoryType; });
    if (Found == Factories.end() || !Settings.is_object())
    {
        Error = "Unknown asset factory or invalid settings";
        return false;
    }
    AssetMount Mount;
    std::string Relative;
    if (!AssetPath::TryParseVirtualPath(VirtualPath, Mount, Relative) || Mount != AssetMount::Game)
    {
        Error = "Choose a Game Content destination";
        return false;
    }
    const auto Destination = Registry.GetGameContentRoot() / std::filesystem::u8path(Relative);
    const auto Name = Destination.stem().u8string();
    if (!AssetPath::IsInsideContent(Destination, Registry.GetGameContentRoot())
        || Destination.extension().u8string() != (*Found)->GetExtension() || Name.empty()
        || Name.find_first_of("./\\:*?\"<>|") != std::string::npos || Name.back() == ' '
        || !std::filesystem::is_directory(Destination.parent_path()))
    {
        Error = "Choose an existing Content folder and a valid asset filename";
        return false;
    }
    if (Registry.LeafNameExists(Name) || std::filesystem::exists(Destination) || std::filesystem::exists(Destination.u8string() + ".meta"))
    {
        Error = "Asset name already exists";
        return false;
    }
    AssetTypeRegistration Registration;
    if (!Registry.TryGetAssetTypeRegistration((*Found)->GetAssetType(), Registration))
    {
        Error = "The factory asset type is not registered";
        return false;
    }
    if (std::find(Registration.Extensions.begin(), Registration.Extensions.end(), (*Found)->GetExtension()) == Registration.Extensions.end())
    {
        Error = "The factory extension is not registered for its asset type";
        return false;
    }
    std::unique_ptr<AssetCreationFactory> Instance(static_cast<AssetCreationFactory*>(ReflectionSubsystem::Get().CreateInstance(FactoryType)));
    for (auto Setting = Settings.begin(); Setting != Settings.end(); ++Setting)
    {
        const auto* Property = Instance->GetClass()->FindProperty(PropertyId{Setting.key()});
        if (!Property || !HasPropertyFlag(Property->Attributes.Flags, PropertyFlags::EditorEditable) || Property->Attributes.bEditorHidden)
        {
            Error = "Unknown creation setting: " + Setting.key();
            return false;
        }
        ReflectedValue Value;
        auto Result = ReflectionJson::JsonToReflectedValue(Property->ValueTypeId, Setting.value(), Value);
        if (Result.bOk)
        {
            Result = PropertyAccess::SetProperty(Instance.get(), *Property, Value, PropertyAccessContext::Inspector);
        }
        if (!Result.bOk)
        {
            Error = Result.Message;
            return false;
        }
    }
    for (const auto* Property : Instance->GetClass()->GetAllProperties())
    {
        if (!HasPropertyFlag(Property->Attributes.Flags, PropertyFlags::EditorEditable))
        {
            continue;
        }
        ReflectedValue Value;
        const auto Result = PropertyAccess::GetProperty(Instance.get(), *Property, Value);
        if (!Result.bOk)
        {
            Error = Result.Message;
            return false;
        }
        if (Instance->IsSettingRequired(Property->Id) && (Value.IsEmpty()
            || (Value.ValueKind == ReflectedValue::Kind::AssetReference && !Value.AssetValue.IsValid())
            || (Value.ValueKind == ReflectedValue::Kind::String && Value.StringValue.empty())))
        {
            Error = "Required creation setting: " + Property->Id.Value;
            return false;
        }
        if (Value.ValueKind == ReflectedValue::Kind::AssetReference && Value.AssetValue.IsValid())
        {
            AssetRegistryEntry Entry;
            const SubAssetRecord* Subasset = nullptr;
            if (!Registry.TryResolveKey(Value.AssetValue, Entry, &Subasset))
            {
                Error = "Creation asset is unavailable";
                return false;
            }
            AssetType Type = Entry.Metadata.Type;
            if (Subasset)
            {
                Type = Subasset->Type;
            }
            auto AllowedTypes = Property->Attributes.AllowedAssetTypes;
            if (Property->Attributes.AssetTypeFilter)
            {
                AllowedTypes.emplace_back(Property->Attributes.AssetTypeFilter);
            }
            if (!AllowedTypes.empty() && std::find(AllowedTypes.begin(), AllowedTypes.end(), Type.GetIdentifier()) == AllowedTypes.end())
            {
                Error = "Creation asset type is incompatible with " + Property->Id.Value;
                return false;
            }
        }
    }
    if (!Instance->Create(Registry, Destination, Error))
    {
        return false;
    }
    const auto Scan = Registry.ScanContent();
    if (Scan.HasError())
    {
        Error = Scan.Message;
        return false;
    }
    if (!Registry.TryGetByPath(VirtualPath, Created) || Created.Metadata.Type != Instance->GetAssetType())
    {
        Error = "Created asset registration failed";
        return false;
    }
    return true;
}
