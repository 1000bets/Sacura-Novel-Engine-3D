#include "Documents/ReflectedAssetDocument.h"
#include "Documents/InputAssetDocument.h"
#include "Reflection/ReflectionSubsystem.h"
#include "Reflection/ReflectionValueCodec.h"
#include "Reflection/Json/ReflectionJson.h"
#include "Reflection/PropertyAccess.h"
#include <map>
#include <charconv>
#include <limits>
#include <stdexcept>

using Json = nlohmann::json;

static void CheckReflection(const ReflectionDiagnostic& Result)
{
    if (!Result.bOk)
    {
        throw std::runtime_error(Result.Message);
    }
}

static bool IsVisible(const PropertyDescriptor& Property)
{
    return !Property.Attributes.bEditorHidden && HasPropertyFlag(Property.Attributes.Flags, RF_EDITOR_VISIBLE);
}

static bool IsEditable(const PropertyDescriptor& Property)
{
    return IsVisible(Property) && !Property.bReadOnly && HasPropertyFlag(Property.Attributes.Flags, PropertyFlags::EditorEditable);
}

static Json ConvertTransport(const TypeId& Type, const Json& Value, bool bDecode)
{
    if (Type.Value == "engine.int64")
    {
        if (!bDecode)
        {
            return Value.dump();
        }
        if (!Value.is_string())
        {
            throw std::runtime_error("Integer values require decimal strings");
        }
        const auto Text = Value.get<std::string>();
        int64_t Number = 0;
        const auto Parsed = std::from_chars(Text.data(), Text.data() + Text.size(), Number);
        if (Parsed.ec != std::errc{} || Parsed.ptr != Text.data() + Text.size())
        {
            throw std::runtime_error("Invalid integer value");
        }
        return Number;
    }
    if (bDecode && Type.Value == "engine.float" && Value.is_number()
        && std::abs(Value.get<double>()) > std::numeric_limits<float>::max())
    {
        throw std::runtime_error("Value is outside float range");
    }
    auto& Reflection = ReflectionSubsystem::Get();
    const auto* Descriptor = Reflection.FindType(Type);
    Json Result = Value;
    if (Descriptor && Descriptor->Kind == TypeKind::Array && Value.is_array())
    {
        for (auto& Element : Result)
        {
            Element = ConvertTransport(Descriptor->ElementTypeId, Element, bDecode);
        }
    }
    else if (Descriptor && Descriptor->Kind == TypeKind::Struct && Value.is_object())
    {
        if (const auto* Fields = Reflection.FindStructProperties(Type))
        {
            for (const auto& Field : *Fields)
            {
                if (Result.contains(Field.Id.Value))
                {
                    Result[Field.Id.Value] = ConvertTransport(Field.ValueTypeId, Result.at(Field.Id.Value), bDecode);
                }
            }
        }
    }
    return Result;
}

static Json DescribeValue(const TypeId& Type, size_t Depth)
{
    Json Result = {{"typeId", Type.Value}, {"kind", "value"}};
    auto& Reflection = ReflectionSubsystem::Get();
    if (Depth > 12)
    {
        Result["error"] = "Reflection nesting limit reached";
        return Result;
    }
    const auto* Descriptor = Reflection.FindType(Type);
    if (!Descriptor)
    {
        Result["error"] = "Type metadata is unavailable";
        return Result;
    }
    if (const auto* Enumeration = Reflection.FindEnum(Type))
    {
        Result["enumValues"] = Json::array();
        for (const auto& Entry : Enumeration->Values)
        {
            Result["enumValues"].push_back(Entry.Name);
        }
    }
    if (Descriptor->Kind == TypeKind::Struct)
    {
        Result["kind"] = "struct";
        Result["fields"] = Json::array();
        if (const auto* Properties = Reflection.FindStructProperties(Type))
        {
            for (const auto& Property : *Properties)
            {
                if (!IsVisible(Property))
                {
                    continue;
                }
                Json Field = DescribeValue(Property.ValueTypeId, Depth + 1);
                Field["id"] = Property.Id.Value;
                Field["label"] = Property.Id.Value;
                Field["category"] = "";
                Field["description"] = "";
                Field["localizationKey"] = "Editor.Reflection." + Type.Value + "." + Property.Id.Value + ".DisplayName";
                Field["editable"] = IsEditable(Property);
                if (Property.Attributes.DisplayName)
                {
                    Field["label"] = Property.Attributes.DisplayName;
                }
                if (Property.Attributes.Category)
                {
                    Field["category"] = Property.Attributes.Category;
                }
                if (Property.Attributes.Description)
                {
                    Field["description"] = Property.Attributes.Description;
                }
                if (Property.Attributes.bHasRange)
                {
                    Field["range"] = {Property.Attributes.RangeMinimum, Property.Attributes.RangeMaximum};
                }
                auto AssetTypes = Property.Attributes.AllowedAssetTypes;
                if (Property.Attributes.AssetTypeFilter)
                {
                    AssetTypes.push_back(Property.Attributes.AssetTypeFilter);
                }
                if (!AssetTypes.empty())
                {
                    Field["assetTypes"] = AssetTypes;
                }
                Result["fields"].push_back(std::move(Field));
            }
        }
    }
    else if (Descriptor->Kind == TypeKind::Array)
    {
        Result["kind"] = "array";
        Result["fixedSize"] = Descriptor->FixedElementCount;
        Result["element"] = DescribeValue(Descriptor->ElementTypeId, Depth + 1);
        ReflectedValue Default;
        Json DefaultJson;
        if (MakeDefaultReflectionValue(Descriptor->ElementTypeId, Default).bOk
            && ReflectionJson::ReflectedValueToJson(Descriptor->ElementTypeId, Default, DefaultJson).bOk)
        {
            Result["elementDefault"] = ConvertTransport(Descriptor->ElementTypeId, DefaultJson, false);
        }
    }
    return Result;
}

Json ReflectedAssetDocument::GetSchema() const
{
    return DescribeValue(GetRootType(), 0);
}

Json ReflectedAssetDocument::GetValues() const
{
    ReflectedValue Value;
    CheckReflection(ReadValue(Value));
    Json Result;
    CheckReflection(ReflectionJson::ReflectedValueToJson(GetRootType(), Value, Result));
    return ConvertTransport(GetRootType(), Result, false);
}

static void ValidateNewValue(const TypeId& Type, const Json& Candidate, const Json& Baseline)
{
    auto& Reflection = ReflectionSubsystem::Get();
    const auto* Descriptor = Reflection.FindType(Type);
    if (!Descriptor)
    {
        throw std::runtime_error("Unknown reflected type");
    }
    ReflectedValue Converted;
    CheckReflection(ReflectionJson::JsonToReflectedValue(Type, Candidate, Converted));
    if (Descriptor->Kind == TypeKind::Struct)
    {
        if (!Candidate.is_object() || !Baseline.is_object() || Candidate.size() != Baseline.size())
        {
            throw std::runtime_error("Struct fields cannot be removed or added");
        }
        for (auto Entry = Candidate.begin(); Entry != Candidate.end(); ++Entry)
        {
            const auto* Property = Reflection.FindStructProperty(Type, PropertyId{Entry.key()});
            if (!Property)
            {
                throw std::runtime_error("Unknown reflected property");
            }
            if (!IsEditable(*Property) && (!Baseline.contains(Entry.key()) || Baseline.at(Entry.key()) != Entry.value()))
            {
                throw std::runtime_error("Property is not editable");
            }
            const auto* Value = Converted.FindField(Entry.key());
            if (Value)
            {
                CheckReflection(PropertyAccess::ValidatePropertyValue(*Property, *Value));
            }
            ValidateNewValue(Property->ValueTypeId, Entry.value(), Baseline.value(Entry.key(), Json{}));
        }
    }
    else if (Descriptor->Kind == TypeKind::Array)
    {
        if (Descriptor->FixedElementCount && Candidate.size() != Descriptor->FixedElementCount)
        {
            throw std::runtime_error("Fixed array length cannot change");
        }
        for (size_t Index = 0; Index < Candidate.size(); ++Index)
        {
            Json Previous;
            if (Baseline.is_array() && Index < Baseline.size())
            {
                Previous = Baseline.at(Index);
            }
            ValidateNewValue(Descriptor->ElementTypeId, Candidate.at(Index), Previous);
        }
    }
}

bool ReflectedAssetDocument::Edit(const Json& Change, std::string& Error)
{
    try
    {
        if (IsReadOnly())
        {
            throw std::runtime_error("Asset is read-only");
        }
        Json Candidate = ConvertTransport(GetRootType(), GetValues(), true);
        Json* Target = &Candidate;
        TypeId Type = GetRootType();
        const PropertyDescriptor* TargetProperty = nullptr;
        const auto& Path = Change.at("path");
        if (!Path.is_array() || Path.empty() || Path.size() > 12)
        {
            throw std::runtime_error("Expected a bounded property path");
        }
        auto& Reflection = ReflectionSubsystem::Get();
        for (const auto& Segment : Path)
        {
            const auto* Descriptor = Reflection.FindType(Type);
            if (Descriptor && Descriptor->Kind == TypeKind::Array && Segment.is_number_unsigned())
            {
                Target = &Target->at(Segment.get<size_t>());
                Type = Descriptor->ElementTypeId;
                TargetProperty = nullptr;
            }
            else if (Segment.is_string())
            {
                TargetProperty = Reflection.FindStructProperty(Type, PropertyId{Segment.get<std::string>()});
                if (!TargetProperty || !IsEditable(*TargetProperty))
                {
                    throw std::runtime_error("Property is not editable");
                }
                Target = &Target->at(TargetProperty->Id.Value);
                Type = TargetProperty->ValueTypeId;
            }
            else
            {
                throw std::runtime_error("Invalid property path");
            }
        }
        const std::string Operation = Change.value("operation", "set");
        const auto* Descriptor = Reflection.FindType(Type);
        if (Operation == "set")
        {
            const auto Value = ConvertTransport(Type, Change.at("value"), true);
            ValidateNewValue(Type, Value, *Target);
            ReflectedValue Converted;
            CheckReflection(ReflectionJson::JsonToReflectedValue(Type, Value, Converted));
            if (TargetProperty)
            {
                CheckReflection(PropertyAccess::ValidatePropertyValue(*TargetProperty, Converted));
            }
            *Target = Value;
        }
        else
        {
            if (!Descriptor || Descriptor->Kind != TypeKind::Array || Descriptor->FixedElementCount)
            {
                throw std::runtime_error("Expected a resizable collection");
            }
            if (Operation == "insert")
            {
                ReflectedValue Default;
                CheckReflection(MakeDefaultReflectionValue(Descriptor->ElementTypeId, Default));
                Json DefaultJson;
                CheckReflection(ReflectionJson::ReflectedValueToJson(Descriptor->ElementTypeId, Default, DefaultJson));
                const auto Value = ConvertTransport(Descriptor->ElementTypeId, Change.at("value"), true);
                ValidateNewValue(Descriptor->ElementTypeId, Value, DefaultJson);
                Target->push_back(Value);
            }
            else
            {
                const size_t Index = Change.at("index").get<size_t>();
                Target->at(Index);
                if (Operation == "remove")
                {
                    Target->erase(Target->begin() + Index);
                }
                else if (Operation == "move")
                {
                    const size_t Destination = Change.at("destination").get<size_t>();
                    Target->at(Destination);
                    Json Element = Target->at(Index);
                    Target->erase(Target->begin() + Index);
                    Target->insert(Target->begin() + Destination, std::move(Element));
                }
                else
                {
                    throw std::runtime_error("Unknown collection operation");
                }
            }
        }
        ReflectedValue Value;
        CheckReflection(ReflectionJson::JsonToReflectedValue(GetRootType(), Candidate, Value));
        return ApplyValue(Value, Error);
    }
    catch (const std::exception& Exception)
    {
        Error = Exception.what();
        return false;
    }
}

static std::map<AssetType, ReflectedAssetDocument::Factory>& DocumentFactories()
{
    static std::map<AssetType, ReflectedAssetDocument::Factory> Factories = {
        {InputActionAssetType, [] { return std::make_unique<InputAssetDocument>(); }},
        {InputMappingContextAssetType, [] { return std::make_unique<InputAssetDocument>(); }}
    };
    return Factories;
}

void ReflectedAssetDocument::Register(const AssetType& Type, Factory Create)
{
    DocumentFactories()[Type] = std::move(Create);
}

std::unique_ptr<ReflectedAssetDocument> ReflectedAssetDocument::Create(const AssetType& Type)
{
    const auto Found = DocumentFactories().find(Type);
    if (Found != DocumentFactories().end())
    {
        return Found->second();
    }
    return {};
}
