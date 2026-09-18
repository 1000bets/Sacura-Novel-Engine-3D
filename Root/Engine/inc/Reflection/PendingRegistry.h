#pragma once

#include "Reflection/PropertyFlags.h"
#include "Reflection/TypeId.h"

#include <cstdint>

struct ClassRecipe;
struct StructRecipe;
struct FieldRecipe;
struct PropertyRecipe;
struct EnumRecipe;

struct ClassRecipe
{
    ClassRecipe* Next = nullptr;
    const char* TypeIdString = nullptr;
    const char* BaseTypeIdString = nullptr;
    uint32_t SchemaVersion = 1;
    CreationPolicy Policy = CreationPolicy::Concrete;
    ReflectionOrigin Origin = ReflectionOrigin::Native;
    void (*Bind)() = nullptr;
};

struct StructRecipe
{
    StructRecipe* Next = nullptr;
    const char* TypeIdString = nullptr;
    uint32_t SchemaVersion = 1;
    void (*Bind)() = nullptr;
};

struct FieldRecipe
{
    FieldRecipe* Next = nullptr;
    const char* DeclaringTypeIdString = nullptr;
    const char* PropertyIdString = nullptr;
    const char* ValueTypeIdString = nullptr;
    PropertyAttributes Attributes{};
    void (*Bind)() = nullptr;
};

struct PropertyRecipe
{
    PropertyRecipe* Next = nullptr;
    const char* DeclaringTypeIdString = nullptr;
    const char* PropertyIdString = nullptr;
    const char* ValueTypeIdString = nullptr;
    PropertyAttributes Attributes{};
    bool bReadOnly = false;
    void (*Bind)() = nullptr;
};

struct EnumRecipe
{
    EnumRecipe* Next = nullptr;
    const char* TypeIdString = nullptr;
    uint32_t SchemaVersion = 1;
    void (*Bind)() = nullptr;
};

class PendingRegistry
{
public:
    static PendingRegistry& Get() noexcept;

    void AddClassRecipe(ClassRecipe& Recipe) noexcept;
    void AddStructRecipe(StructRecipe& Recipe) noexcept;
    void AddFieldRecipe(FieldRecipe& Recipe) noexcept;
    void AddPropertyRecipe(PropertyRecipe& Recipe) noexcept;
    void AddEnumRecipe(EnumRecipe& Recipe) noexcept;

    ClassRecipe* GetClassRecipes() const noexcept { return ClassHead; }
    StructRecipe* GetStructRecipes() const noexcept { return StructHead; }
    FieldRecipe* GetFieldRecipes() const noexcept { return FieldHead; }
    PropertyRecipe* GetPropertyRecipes() const noexcept { return PropertyHead; }
    EnumRecipe* GetEnumRecipes() const noexcept { return EnumHead; }

private:
    PendingRegistry() = default;

    ClassRecipe* ClassHead = nullptr;
    StructRecipe* StructHead = nullptr;
    FieldRecipe* FieldHead = nullptr;
    PropertyRecipe* PropertyHead = nullptr;
    EnumRecipe* EnumHead = nullptr;
};
