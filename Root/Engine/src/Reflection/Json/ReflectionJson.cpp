#include "Reflection/Json/ReflectionJson.h"

#include "Assets/Guid.h"
#include "Assets/AssetMetadata.h"
#include "Reflection/ReflectionValueCodec.h"
#include "World/Scene.h"
#include "World/GameObject.h"
#include "Core/Object/Object.h"
#include "Reflection/Class.h"
#include "Reflection/PropertyAccess.h"
#include "Reflection/ReflectionSubsystem.h"

#include <cstring>

ReflectionJson::ObjectResolver ReflectionJson::MakeSceneResolver(const Scene& World)
{
    return [&World](const std::string& Identity)
    {
        for (GameObject* Instance : World.GetAllObjects())
        {
            if (Instance->GetPersistentId() == Identity)
            {
                return Instance->GetObjectHandle();
            }
            for (Component* ComponentInstance : Instance->GetAllComponents())
            {
                if (ComponentInstance->GetPersistentId() == Identity)
                {
                    return ComponentInstance->GetObjectHandle();
                }
            }
        }
        return ObjectHandle{};
    };
}

namespace
{
std::string MakeMigrationKey(const TypeId& Type, uint32_t FromVersion, uint32_t ToVersion)
{
    return Type.Value + "#" + std::to_string(FromVersion) + "->" + std::to_string(ToVersion);
}
}

std::unordered_map<std::string, ReflectionJsonMigrationFunction>& ReflectionJson::MigrationTable()
{
    static std::unordered_map<std::string, ReflectionJsonMigrationFunction> Table;
    return Table;
}

void ReflectionJson::RegisterMigration(
    const TypeId& Type,
    uint32_t FromVersion,
    uint32_t ToVersion,
    ReflectionJsonMigrationFunction Migration)
{
    MigrationTable()[MakeMigrationKey(Type, FromVersion, ToVersion)] = std::move(Migration);
}

void ReflectionJson::ClearMigrations()
{
    MigrationTable().clear();
}

ReflectionDiagnostic ReflectionJson::ReflectedValueToJson(
    const TypeId& ValueTypeId,
    const ReflectedValue& Value,
    nlohmann::json& OutJson)
{
    if (Value.ValueKind == ReflectedValue::Kind::Struct)
    {
        if (Value.TypeIdValue != ValueTypeId || Value.FieldNames.size() != Value.Values.size())
        {
            return ReflectionDiagnostic::Fail("Invalid reflected struct", ValueTypeId);
        }
        OutJson = nlohmann::json::object();
        for (size_t FieldIndex = 0; FieldIndex < Value.Values.size(); ++FieldIndex)
        {
            const PropertyDescriptor* Field = ReflectionSubsystem::Get().FindStructProperty(
                ValueTypeId, PropertyId{Value.FieldNames[FieldIndex]});
            if (Field == nullptr)
            {
                return ReflectionDiagnostic::Fail("Unknown struct field", ValueTypeId);
            }
            ReflectionDiagnostic Converted = ReflectedValueToJson(
                Field->ValueTypeId, Value.Values[FieldIndex], OutJson[Field->Id.Value]);
            if (!Converted.bOk)
            {
                return Converted;
            }
        }
        return ReflectionDiagnostic::Ok();
    }
    if (Value.ValueKind == ReflectedValue::Kind::Array)
    {
        OutJson = nlohmann::json::array();
        for (const ReflectedValue& Element : Value.Values)
        {
            nlohmann::json JsonElement;
            ReflectionDiagnostic Converted = ReflectedValueToJson(Value.TypeIdValue, Element, JsonElement);
            if (!Converted.bOk)
            {
                return Converted;
            }
            OutJson.push_back(std::move(JsonElement));
        }
        return ReflectionDiagnostic::Ok();
    }
    if (Value.ValueKind == ReflectedValue::Kind::AssetReference)
    {
        OutJson = AssetMetadataIO::AssetRefToJson(Value.AssetValue);
        return ReflectionDiagnostic::Ok();
    }
    if (Value.ValueKind == ReflectedValue::Kind::ObjectReference)
    {
        OutJson = {{"object", Value.StringValue}};
        return ReflectionDiagnostic::Ok();
    }
    if (Value.ValueKind == ReflectedValue::Kind::Enum)
    {
        const EnumDescriptor* Descriptor = ReflectionSubsystem::Get().FindEnum(ValueTypeId);
        if (Descriptor != nullptr)
        {
            for (const EnumValueDescriptor& Entry : Descriptor->Values)
            {
                if (Entry.Value == Value.Int64Value)
                {
                    OutJson = Entry.Name;
                    return ReflectionDiagnostic::Ok();
                }
            }
        }
        return ReflectionDiagnostic::Fail("Unknown enum value", ValueTypeId);
    }
    if (ValueTypeId.Value == "engine.bool")
    {
        if (Value.ValueKind != ReflectedValue::Kind::Bool)
        {
            return ReflectionDiagnostic::Fail("Expected bool");
        }
        OutJson = Value.BoolValue;
        return ReflectionDiagnostic::Ok();
    }
    if (ValueTypeId.Value == "engine.int64")
    {
        if (Value.ValueKind != ReflectedValue::Kind::Int64)
        {
            return ReflectionDiagnostic::Fail("Expected int64");
        }
        OutJson = Value.Int64Value;
        return ReflectionDiagnostic::Ok();
    }
    if (ValueTypeId.Value == "engine.float" || ValueTypeId.Value == "engine.double")
    {
        if (Value.ValueKind != ReflectedValue::Kind::Float64)
        {
            return ReflectionDiagnostic::Fail("Expected float");
        }
        OutJson = Value.Float64Value;
        return ReflectionDiagnostic::Ok();
    }
    if (ValueTypeId.Value == "engine.string")
    {
        if (Value.ValueKind != ReflectedValue::Kind::String)
        {
            return ReflectionDiagnostic::Fail("Expected string");
        }
        OutJson = Value.StringValue;
        return ReflectionDiagnostic::Ok();
    }
    if ((ValueTypeId.Value == "engine.Vector3" || ValueTypeId.Value == "engine.EulerAngles"))
    {
        if (Value.ValueKind != ReflectedValue::Kind::Bytes || Value.BytesValue.size() != sizeof(float) * 3)
        {
            return ReflectionDiagnostic::Fail("Expected Vector3");
        }
        float Values[3] = {};
        std::memcpy(Values, Value.BytesValue.data(), sizeof(Values));
        OutJson = nlohmann::json::array({Values[0], Values[1], Values[2]});
        return ReflectionDiagnostic::Ok();
    }
    if (ValueTypeId.Value == "engine.Color" || ValueTypeId.Value == "engine.Quaternion")
    {
        if (Value.ValueKind != ReflectedValue::Kind::Bytes || Value.BytesValue.size() != sizeof(float) * 4)
        {
            return ReflectionDiagnostic::Fail("Expected Color/Quaternion");
        }
        float Values[4] = {};
        std::memcpy(Values, Value.BytesValue.data(), sizeof(Values));
        OutJson = nlohmann::json::array({Values[0], Values[1], Values[2], Values[3]});
        return ReflectionDiagnostic::Ok();
    }
    if (Value.ValueKind == ReflectedValue::Kind::TypeId)
    {
        OutJson = Value.TypeIdValue.Value;
        return ReflectionDiagnostic::Ok();
    }
    if (Value.ValueKind == ReflectedValue::Kind::String && ValueTypeId.Value.find("AssetRef") != std::string::npos)
    {
        OutJson = Value.StringValue;
        return ReflectionDiagnostic::Ok();
    }
    if (Value.ValueKind == ReflectedValue::Kind::String)
    {
        OutJson = Value.StringValue;
        return ReflectionDiagnostic::Ok();
    }

    return ReflectionDiagnostic::Fail("Unsupported value type for JSON: " + ValueTypeId.Value);
}

ReflectionDiagnostic ReflectionJson::JsonToReflectedValue(
    const TypeId& ValueTypeId,
    const nlohmann::json& JsonValue,
    ReflectedValue& OutValue,
    const ObjectResolver& ResolveObject)
{
    const TypeDescriptor* Type = ReflectionSubsystem::Get().FindType(ValueTypeId);
    if (Type != nullptr && Type->Kind == TypeKind::Struct)
    {
        nlohmann::json Fields = JsonValue;
        if (JsonValue.is_array() && ValueTypeId.Value == "engine.Color" && JsonValue.size() == 4)
        {
            Fields = {{"r", JsonValue[0]}, {"g", JsonValue[1]}, {"b", JsonValue[2]}, {"a", JsonValue[3]}};
        }
        else if (JsonValue.is_array()
            && (ValueTypeId.Value == "engine.MaterialSlots" || ValueTypeId.Value == "engine.PostProcessEffects"))
        {
            Fields = {{"entries", JsonValue}};
        }
        if (!Fields.is_object())
        {
            return ReflectionDiagnostic::Fail("Expected struct object", ValueTypeId);
        }
        ReflectedValue Result;
        Result.ValueKind = ReflectedValue::Kind::Struct;
        Result.TypeIdValue = ValueTypeId;
        for (auto Field = Fields.begin(); Field != Fields.end(); ++Field)
        {
            const PropertyDescriptor* Property = ReflectionSubsystem::Get().FindStructProperty(
                ValueTypeId, PropertyId{Field.key()});
            if (Property == nullptr)
            {
                return ReflectionDiagnostic::Fail("Unknown struct field", ValueTypeId, PropertyId{Field.key()});
            }
            ReflectedValue Value;
            ReflectionDiagnostic Converted = JsonToReflectedValue(Property->ValueTypeId, Field.value(), Value, ResolveObject);
            if (!Converted.bOk)
            {
                return Converted;
            }
            Result.FieldNames.push_back(Field.key());
            Result.Values.push_back(std::move(Value));
        }
        OutValue = std::move(Result);
        return ReflectionDiagnostic::Ok();
    }
    if (Type != nullptr && Type->Kind == TypeKind::Array)
    {
        if (!JsonValue.is_array())
        {
            return ReflectionDiagnostic::Fail("Expected array", ValueTypeId);
        }
        ReflectedValue Result;
        Result.ValueKind = ReflectedValue::Kind::Array;
        Result.TypeIdValue = Type->ElementTypeId;
        for (const nlohmann::json& Element : JsonValue)
        {
            ReflectedValue Converted;
            ReflectionDiagnostic Parsed = JsonToReflectedValue(Type->ElementTypeId, Element, Converted, ResolveObject);
            if (!Parsed.bOk)
            {
                return Parsed;
            }
            Result.Values.push_back(std::move(Converted));
        }
        OutValue = std::move(Result);
        return ReflectionDiagnostic::Ok();
    }
    if (ValueTypeId.Value == "engine.AssetRef")
    {
        AssetKey Key;
        AssetDiagnostic Diagnostic;
        if (!AssetMetadataIO::TryAssetRefFromJson(JsonValue, Key, Diagnostic))
        {
            return ReflectionDiagnostic::Fail(Diagnostic.Message, ValueTypeId);
        }
        return ReflectionValueCodec<AssetKey>::ToReflected(Key, OutValue);
    }
    if (ValueTypeId.Value == "engine.ObjectHandle")
    {
        if (!JsonValue.is_object() || !JsonValue.contains("object") || !JsonValue["object"].is_string())
        {
            return ReflectionDiagnostic::Fail("Expected persistent object reference", ValueTypeId);
        }
        const std::string Identity = JsonValue["object"].get<std::string>();
        ObjectHandle Target;
        if (!Identity.empty())
        {
            if (!ResolveObject)
            {
                return ReflectionDiagnostic::Fail("Object reference requires a scene resolver", ValueTypeId);
            }
            Target = ResolveObject(Identity);
        }
        return ReadReflectionObject(Target, OutValue);
    }
    if (const EnumDescriptor* Descriptor = ReflectionSubsystem::Get().FindEnum(ValueTypeId))
    {
        for (const EnumValueDescriptor& Entry : Descriptor->Values)
        {
            if ((JsonValue.is_string() && JsonValue.get<std::string>() == Entry.Name)
                || (JsonValue.is_number_integer() && JsonValue.get<int64_t>() == Entry.Value))
            {
                OutValue = ReflectedValue::MakeInt64(Entry.Value);
                OutValue.ValueKind = ReflectedValue::Kind::Enum;
                OutValue.TypeIdValue = ValueTypeId;
                return ReflectionDiagnostic::Ok();
            }
        }
        return ReflectionDiagnostic::Fail("Unknown enum value", ValueTypeId);
    }
    if (ValueTypeId.Value == "engine.bool")
    {
        if (!JsonValue.is_boolean())
        {
            return ReflectionDiagnostic::Fail("Expected bool JSON");
        }
        OutValue = ReflectedValue::MakeBool(JsonValue.get<bool>());
        return ReflectionDiagnostic::Ok();
    }
    if (ValueTypeId.Value == "engine.int64")
    {
        if (!JsonValue.is_number_integer())
        {
            return ReflectionDiagnostic::Fail("Expected int JSON");
        }
        OutValue = ReflectedValue::MakeInt64(JsonValue.get<int64_t>());
        return ReflectionDiagnostic::Ok();
    }
    if (ValueTypeId.Value == "engine.float" || ValueTypeId.Value == "engine.double")
    {
        if (!JsonValue.is_number())
        {
            return ReflectionDiagnostic::Fail("Expected number JSON");
        }
        OutValue = ReflectedValue::MakeFloat64(JsonValue.get<double>());
        return ReflectionDiagnostic::Ok();
    }
    if (ValueTypeId.Value == "engine.string")
    {
        if (!JsonValue.is_string())
        {
            return ReflectionDiagnostic::Fail("Expected string JSON");
        }
        OutValue = ReflectedValue::MakeString(JsonValue.get<std::string>());
        return ReflectionDiagnostic::Ok();
    }
    if ((ValueTypeId.Value == "engine.Vector3" || ValueTypeId.Value == "engine.EulerAngles"))
    {
        if (!JsonValue.is_array() || JsonValue.size() != 3)
        {
            return ReflectionDiagnostic::Fail("Expected Vector3 array");
        }
        float Values[3] = {
            JsonValue[0].get<float>(),
            JsonValue[1].get<float>(),
            JsonValue[2].get<float>()};
        OutValue = ReflectedValue::MakeEmpty();
        OutValue.ValueKind = ReflectedValue::Kind::Bytes;
        OutValue.BytesValue.resize(sizeof(Values));
        std::memcpy(OutValue.BytesValue.data(), Values, sizeof(Values));
        return ReflectionDiagnostic::Ok();
    }
    if (ValueTypeId.Value == "engine.Color" || ValueTypeId.Value == "engine.Quaternion")
    {
        if (!JsonValue.is_array() || JsonValue.size() != 4)
        {
            return ReflectionDiagnostic::Fail("Expected 4-float array");
        }
        float Values[4] = {
            JsonValue[0].get<float>(),
            JsonValue[1].get<float>(),
            JsonValue[2].get<float>(),
            JsonValue[3].get<float>()};
        OutValue = ReflectedValue::MakeEmpty();
        OutValue.ValueKind = ReflectedValue::Kind::Bytes;
        OutValue.BytesValue.resize(sizeof(Values));
        std::memcpy(OutValue.BytesValue.data(), Values, sizeof(Values));
        return ReflectionDiagnostic::Ok();
    }
    if (JsonValue.is_string())
    {
        OutValue = ReflectedValue::MakeString(JsonValue.get<std::string>());
        if (ValueTypeId.Value.find("ClassRef") != std::string::npos || ValueTypeId.Value == "engine.TypeId")
        {
            OutValue = ReflectedValue::MakeTypeId(TypeId{JsonValue.get<std::string>()});
        }
        return ReflectionDiagnostic::Ok();
    }

    return ReflectionDiagnostic::Fail("Unsupported JSON value for type: " + ValueTypeId.Value);
}

ReflectionDiagnostic ReflectionJson::ApplyMigrations(
    const TypeId& Type,
    uint32_t& TypeVersion,
    uint32_t TargetVersion,
    nlohmann::json& Properties)
{
    while (TypeVersion < TargetVersion)
    {
        const uint32_t NextVersion = TypeVersion + 1;
        auto Found = MigrationTable().find(MakeMigrationKey(Type, TypeVersion, NextVersion));
        if (Found == MigrationTable().end())
        {
            return ReflectionDiagnostic::Fail(
                "Missing migration " + std::to_string(TypeVersion) + "->" + std::to_string(NextVersion),
                Type);
        }
        ReflectionDiagnostic Migrated = Found->second(TypeVersion, NextVersion, Properties);
        if (!Migrated.bOk)
        {
            return Migrated;
        }
        TypeVersion = NextVersion;
    }
    return ReflectionDiagnostic::Ok();
}

ReflectionDiagnostic ReflectionJson::ParseDocument(const std::string& JsonText, ReflectionJsonDocument& OutDocument)
{
    try
    {
        nlohmann::json Root = nlohmann::json::parse(JsonText);
        if (!Root.is_object())
        {
            return ReflectionDiagnostic::Fail("JSON root must be object");
        }
        if (!Root.contains("type") || !Root["type"].is_string())
        {
            return ReflectionDiagnostic::Fail("JSON missing type");
        }
        if (!Root.contains("typeVersion") || !Root["typeVersion"].is_number_integer())
        {
            return ReflectionDiagnostic::Fail("JSON missing typeVersion");
        }
        if (!Root.contains("properties") || !Root["properties"].is_object())
        {
            return ReflectionDiagnostic::Fail("JSON missing properties object");
        }

        OutDocument.Type = TypeId{Root["type"].get<std::string>()};
        OutDocument.TypeVersion = Root["typeVersion"].get<uint32_t>();
        OutDocument.Properties = Root["properties"];
        return ReflectionDiagnostic::Ok();
    }
    catch (const nlohmann::json::exception& Error)
    {
        return ReflectionDiagnostic::Fail(std::string("JSON parse failed: ") + Error.what());
    }
}

ReflectionDiagnostic ReflectionJson::SerializeObject(const Object* Instance, ReflectionJsonDocument& OutDocument)
{
    if (Instance == nullptr)
    {
        return ReflectionDiagnostic::Fail("Instance is null");
    }
    Class* ObjectClass = Instance->GetClass();
    if (ObjectClass == nullptr)
    {
        return ReflectionDiagnostic::Fail("Object has no Class");
    }

    OutDocument.Type = ObjectClass->GetTypeId();
    OutDocument.TypeVersion = ObjectClass->GetSchemaVersion();
    OutDocument.Properties = nlohmann::json::object();

    for (const PropertyDescriptor* Property : ObjectClass->GetAllProperties())
    {
        const PropertyDescriptor& Descriptor = *Property;
        if (!HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::Serializable))
        {
            continue;
        }

        ReflectedValue Value;
        ReflectionDiagnostic Got = PropertyAccess::GetProperty(
            const_cast<Object*>(Instance),
            Descriptor,
            Value,
            PropertyAccessContext::Deserialize);
        if (!Got.bOk)
        {
            return Got;
        }

        nlohmann::json JsonValue;
        ReflectionDiagnostic Converted = ReflectedValueToJson(Descriptor.ValueTypeId, Value, JsonValue);
        if (!Converted.bOk)
        {
            Converted.Property = Descriptor.Id;
            Converted.Type = ObjectClass->GetTypeId();
            return Converted;
        }
        OutDocument.Properties[Descriptor.Id.Value] = std::move(JsonValue);
    }

    return ReflectionDiagnostic::Ok();
}

ReflectionDiagnostic ReflectionJson::SerializeObject(const Object* Instance, std::string& OutJsonText)
{
    ReflectionJsonDocument Document;
    ReflectionDiagnostic Serialized = SerializeObject(Instance, Document);
    if (!Serialized.bOk)
    {
        return Serialized;
    }

    nlohmann::json Root = nlohmann::json::object();
    Root["type"] = Document.Type.Value;
    Root["typeVersion"] = Document.TypeVersion;
    Root["properties"] = Document.Properties;
    OutJsonText = Root.dump(2);
    return ReflectionDiagnostic::Ok();
}

ReflectionDiagnostic ReflectionJson::DeserializeInto(
    Object* Instance,
    const ReflectionJsonDocument& Document,
    bool bAllowMigration,
    const ObjectResolver& ResolveObject)
{
    if (Instance == nullptr)
    {
        return ReflectionDiagnostic::Fail("Instance is null");
    }
    Class* ObjectClass = Instance->GetClass();
    if (ObjectClass == nullptr)
    {
        return ReflectionDiagnostic::Fail("Object has no Class");
    }
    if (ObjectClass->GetTypeId() != Document.Type)
    {
        return ReflectionDiagnostic::Fail("JSON type mismatch", Document.Type);
    }

    nlohmann::json Properties = Document.Properties;
    uint32_t TypeVersion = Document.TypeVersion;
    if (bAllowMigration && TypeVersion != ObjectClass->GetSchemaVersion())
    {
        if (TypeVersion > ObjectClass->GetSchemaVersion())
        {
            return ReflectionDiagnostic::Fail("JSON typeVersion newer than class schema", Document.Type);
        }
        ReflectionDiagnostic Migrated = ApplyMigrations(
            Document.Type,
            TypeVersion,
            ObjectClass->GetSchemaVersion(),
            Properties);
        if (!Migrated.bOk)
        {
            return Migrated;
        }
    }
    else if (TypeVersion != ObjectClass->GetSchemaVersion())
    {
        return ReflectionDiagnostic::Fail("JSON typeVersion mismatch", Document.Type);
    }

    std::vector<std::pair<const PropertyDescriptor*, ReflectedValue>> PendingWrites;
    PendingWrites.reserve(Properties.size());

    for (auto It = Properties.begin(); It != Properties.end(); ++It)
    {
        const PropertyId Property{It.key()};
        const PropertyDescriptor* Descriptor = ObjectClass->FindProperty(Property);
        if (Descriptor == nullptr)
        {
            return ReflectionDiagnostic::Fail("Unknown property in JSON", Document.Type, Property);
        }
        if (!HasPropertyFlag(Descriptor->Attributes.Flags, PropertyFlags::Serializable))
        {
            return ReflectionDiagnostic::Fail("Property is not serializable", Document.Type, Property);
        }

        ReflectedValue Converted;
        ReflectionDiagnostic Parsed = JsonToReflectedValue(Descriptor->ValueTypeId, It.value(), Converted, ResolveObject);
        if (!Parsed.bOk)
        {
            Parsed.Type = Document.Type;
            Parsed.Property = Property;
            return Parsed;
        }

        if (Descriptor->Attributes.bHasRange)
        {
            double NumericValue = 0.0;
            bool bNumeric = false;
            if (Converted.ValueKind == ReflectedValue::Kind::Float64)
            {
                NumericValue = Converted.Float64Value;
                bNumeric = true;
            }
            else if (Converted.ValueKind == ReflectedValue::Kind::Int64)
            {
                NumericValue = static_cast<double>(Converted.Int64Value);
                bNumeric = true;
            }
            if (bNumeric)
            {
                if (NumericValue < Descriptor->Attributes.RangeMinimum
                    || NumericValue > Descriptor->Attributes.RangeMaximum)
                {
                    return ReflectionDiagnostic::Fail("Value out of range", Document.Type, Property);
                }
            }
        }

        ReflectionDiagnostic Validated = PropertyAccess::ValidatePropertyValue(*Descriptor, Converted);
        if (!Validated.bOk)
        {
            return Validated;
        }
        PendingWrites.emplace_back(Descriptor, std::move(Converted));
    }

    for (const auto& Pair : PendingWrites)
    {
        ReflectionDiagnostic Written = PropertyAccess::SetProperty(
            Instance,
            *Pair.first,
            Pair.second,
            PropertyAccessContext::Deserialize);
        if (!Written.bOk)
        {
            return Written;
        }
    }

    return ReflectionDiagnostic::Ok();
}

ReflectionDiagnostic ReflectionJson::DeserializeInto(
    Object* Instance,
    const std::string& JsonText,
    bool bAllowMigration,
    const ObjectResolver& ResolveObject)
{
    ReflectionJsonDocument Document;
    ReflectionDiagnostic Parsed = ParseDocument(JsonText, Document);
    if (!Parsed.bOk)
    {
        return Parsed;
    }
    return DeserializeInto(Instance, Document, bAllowMigration, ResolveObject);
}
