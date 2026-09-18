#pragma once

#include <cstdint>
#include <string>

// Stable string identity for types (e.g. "engine.CameraComponent").
// Not typeid hash, not registration index, not file path.
struct TypeId
{
    std::string Value;

    bool IsValid() const { return !Value.empty(); }

    bool operator==(const TypeId& Other) const { return Value == Other.Value; }
    bool operator!=(const TypeId& Other) const { return Value != Other.Value; }
};

struct TypeIdHash
{
    size_t operator()(const TypeId& Id) const
    {
        return std::hash<std::string>{}(Id.Value);
    }
};

// Stable property key inside a declaring type (e.g. "field_of_view").
struct PropertyId
{
    std::string Value;

    bool IsValid() const { return !Value.empty(); }

    bool operator==(const PropertyId& Other) const { return Value == Other.Value; }
    bool operator!=(const PropertyId& Other) const { return Value != Other.Value; }
};

struct PropertyIdHash
{
    size_t operator()(const PropertyId& Id) const
    {
        return std::hash<std::string>{}(Id.Value);
    }
};

enum class ReflectionOrigin : uint8_t
{
    Native = 0,
    Python = 1
};

enum class TypeKind : uint8_t
{
    Unknown = 0,
    Value,
    Enum,
    Class,
    Reference
};

enum class CreationPolicy : uint8_t
{
    Abstract = 0,
    Concrete,
    SystemOnly
};
