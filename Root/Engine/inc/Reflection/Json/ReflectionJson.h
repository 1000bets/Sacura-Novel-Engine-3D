#pragma once

#include "Reflection/ReflectedValue.h"
#include "Reflection/TypeId.h"

#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

class Object;
class Scene;

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
    using ObjectResolver = std::function<ObjectHandle(const std::string&)>;
    static ObjectResolver MakeSceneResolver(const Scene& World);
    static ReflectionDiagnostic SerializeObject(const Object* Instance, std::string& OutJsonText);
    static ReflectionDiagnostic SerializeObject(const Object* Instance, ReflectionJsonDocument& OutDocument);

    static ReflectionDiagnostic DeserializeInto(
        Object* Instance,
        const std::string& JsonText,
        bool bAllowMigration = true,
        const ObjectResolver& ResolveObject = {});

    static ReflectionDiagnostic DeserializeInto(
        Object* Instance,
        const ReflectionJsonDocument& Document,
        bool bAllowMigration = true,
        const ObjectResolver& ResolveObject = {});

    static ReflectionDiagnostic ParseDocument(const std::string& JsonText, ReflectionJsonDocument& OutDocument);

    static void RegisterMigration(
        const TypeId& Type,
        uint32_t FromVersion,
        uint32_t ToVersion,
        ReflectionJsonMigrationFunction Migration);

    static void ClearMigrations();

    static ReflectionDiagnostic ReflectedValueToJson(
        const TypeId& ValueTypeId,
        const ReflectedValue& Value,
        nlohmann::json& OutJson);

    static ReflectionDiagnostic JsonToReflectedValue(
        const TypeId& ValueTypeId,
        const nlohmann::json& JsonValue,
        ReflectedValue& OutValue,
        const ObjectResolver& ResolveObject = {});

private:
    static ReflectionDiagnostic ApplyMigrations(
        const TypeId& Type,
        uint32_t& TypeVersion,
        uint32_t TargetVersion,
        nlohmann::json& Properties);

    static std::unordered_map<std::string, ReflectionJsonMigrationFunction>& MigrationTable();
};
