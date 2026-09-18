#pragma once

#include <cstdint>

enum class PropertyFlags : uint32_t
{
    None = 0,
    Serializable = 1u << 0,
    Transient = 1u << 1,
    EditorVisible = 1u << 2,
    EditorEditable = 1u << 3,
    EditorReadOnly = 1u << 4,
    ScriptReadable = 1u << 5,
    ScriptWritable = 1u << 6,
};

constexpr PropertyFlags operator|(PropertyFlags Left, PropertyFlags Right)
{
    return static_cast<PropertyFlags>(static_cast<uint32_t>(Left) | static_cast<uint32_t>(Right));
}

constexpr PropertyFlags operator&(PropertyFlags Left, PropertyFlags Right)
{
    return static_cast<PropertyFlags>(static_cast<uint32_t>(Left) & static_cast<uint32_t>(Right));
}

constexpr bool HasPropertyFlag(PropertyFlags Flags, PropertyFlags Flag)
{
    return (static_cast<uint32_t>(Flags) & static_cast<uint32_t>(Flag)) != 0;
}

constexpr PropertyFlags RF_SERIALIZABLE = PropertyFlags::Serializable;
constexpr PropertyFlags RF_TRANSIENT = PropertyFlags::Transient;
constexpr PropertyFlags RF_EDITOR_VISIBLE = PropertyFlags::EditorVisible;
constexpr PropertyFlags RF_EDITOR_EDITABLE = PropertyFlags::EditorEditable | PropertyFlags::EditorVisible;
constexpr PropertyFlags RF_EDITOR_READONLY = PropertyFlags::EditorReadOnly | PropertyFlags::EditorVisible;
constexpr PropertyFlags RF_SCRIPT_READ = PropertyFlags::ScriptReadable;
constexpr PropertyFlags RF_SCRIPT_WRITE = PropertyFlags::ScriptWritable;
constexpr PropertyFlags RF_SCRIPT_READ_WRITE = PropertyFlags::ScriptReadable | PropertyFlags::ScriptWritable;

struct PropertyAttributes
{
    PropertyFlags Flags = PropertyFlags::None;
    const char* DisplayName = nullptr;
    const char* Category = nullptr;
    const char* Description = nullptr;
    bool bHasRange = false;
    double RangeMinimum = 0.0;
    double RangeMaximum = 0.0;
};
