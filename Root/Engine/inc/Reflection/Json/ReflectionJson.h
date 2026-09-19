#pragma once

#include "Reflection/ReflectedValue.h"
#include "Reflection/TypeId.h"

#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

class Object;

struct ReflectionJsonDocument
{
    TypeId Type{};
    uint32_t TypeVersion = 1;
    nlohmann::json Properties = nlohmann::json::object();
};

using ReflectionJsonMigrationFunction = std::function<ReflectionDiagnostic(
    uint32_t FromVersion,
    uint32_t ToVersion,
    nlohmann::json& Properties)>;

class ReflectionJson
{
public:
    static ReflectionDiagnostic SerializeObject(const Object* Instance, std::string& OutJsonText);
    static ReflectionDiagnostic SerializeObject(const Object* Instance, ReflectionJsonDocument& OutDocument);

    static ReflectionDiagnostic DeserializeInto(
        Object* Instance,
        const std::string& JsonText,
        bool bAllowMigration = true);

    static ReflectionDiagnostic DeserializeInto(
        Object* Instance,
        const ReflectionJsonDocument& Document,
        bool bAllowMigration = true);

    static ReflectionDiagnostic ParseDocument(const std::string& JsonText, ReflectionJsonDocument& OutDocument);

    static void RegisterMigration(
        const TypeId& Type,
        uint32_t FromVersion,
        uint32_t ToVersion,
        ReflectionJsonMigrationFunction Migration);

    static void ClearMigrations();

private:
    static ReflectionDiagnostic ReflectedValueToJson(
        const TypeId& ValueTypeId,
        const ReflectedValue& Value,
        nlohmann::json& OutJson);

    static ReflectionDiagnostic JsonToReflectedValue(
        const TypeId& ValueTypeId,
        const nlohmann::json& JsonValue,
        ReflectedValue& OutValue);

    static ReflectionDiagnostic ApplyMigrations(
        const TypeId& Type,
        uint32_t& TypeVersion,
        uint32_t TargetVersion,
        nlohmann::json& Properties);

    static std::unordered_map<std::string, ReflectionJsonMigrationFunction>& MigrationTable();
};
