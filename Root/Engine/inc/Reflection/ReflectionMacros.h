#pragma once

#include "Core/MemorySubsystem.h"
#include "Reflection/Class.h"
#include "Reflection/PendingRegistry.h"
#include "Reflection/PropertyDescriptor.h"
#include "Reflection/PropertyFlags.h"
#include "Reflection/ReflectionSubsystem.h"
#include "Reflection/ReflectionValueCodec.h"
#include "Reflection/ReflectedValue.h"
#include "Reflection/TypeId.h"

#include <type_traits>
#include <utility>

struct ReflectionDisplayNameTag
{
    const char* Text = nullptr;
};

struct ReflectionUiRangeTag
{
    double Minimum = 0.0;
    double Maximum = 0.0;
};

struct ReflectionCategoryTag
{
    const char* Text = nullptr;
};

inline void ApplyReflectArgument(PropertyAttributes& Attributes, PropertyFlags Flags)
{
    Attributes.Flags = Attributes.Flags | Flags;
}

inline void ApplyReflectArgument(PropertyAttributes& Attributes, ReflectionDisplayNameTag Tag)
{
    Attributes.DisplayName = Tag.Text;
}

inline void ApplyReflectArgument(PropertyAttributes& Attributes, ReflectionUiRangeTag Tag)
{
    Attributes.bHasRange = true;
    Attributes.RangeMinimum = Tag.Minimum;
    Attributes.RangeMaximum = Tag.Maximum;
}

inline void ApplyReflectArgument(PropertyAttributes& Attributes, ReflectionCategoryTag Tag)
{
    Attributes.Category = Tag.Text;
}

inline void BuildPropertyAttributes(PropertyAttributes&)
{
}

template <typename FirstArgument, typename... RemainingArguments>
inline void BuildPropertyAttributes(
    PropertyAttributes& Attributes,
    FirstArgument&& First,
    RemainingArguments&&... Remaining)
{
    ApplyReflectArgument(Attributes, std::forward<FirstArgument>(First));
    BuildPropertyAttributes(Attributes, std::forward<RemainingArguments>(Remaining)...);
}

template <typename... Arguments>
inline PropertyAttributes MakePropertyAttributes(Arguments&&... Values)
{
    PropertyAttributes Attributes{};
    BuildPropertyAttributes(Attributes, std::forward<Arguments>(Values)...);
    return Attributes;
}

#define RF_DISPLAY_NAME(TextLiteral) ReflectionDisplayNameTag{TextLiteral}
#define RF_UI_RANGE(MinimumValue, MaximumValue) ReflectionUiRangeTag{static_cast<double>(MinimumValue), static_cast<double>(MaximumValue)}
#define RF_CATEGORY(TextLiteral) ReflectionCategoryTag{TextLiteral}

template <typename ClassType, typename MemberType>
MemberType ReflectionMemberTypeProbe(MemberType ClassType::*);

template <typename OwnerType, typename MemberType, MemberType OwnerType::*MemberPointer>
struct ReflectionFieldAccess
{
    static ReflectionDiagnostic ReadObject(
        Object* Instance,
        const PropertyDescriptor&,
        ReflectedValue& OutValue)
    {
        if (Instance == nullptr)
        {
            return ReflectionDiagnostic::Fail("Instance is null");
        }
        OwnerType* Typed = dynamic_cast<OwnerType*>(Instance);
        if (Typed == nullptr)
        {
            return ReflectionDiagnostic::Fail("Instance type mismatch");
        }
        return ReflectionValueCodec<MemberType>::ToReflected(Typed->*MemberPointer, OutValue);
    }

    static ReflectionDiagnostic WriteObject(
        Object* Instance,
        const PropertyDescriptor&,
        const ReflectedValue& InValue)
    {
        if (Instance == nullptr)
        {
            return ReflectionDiagnostic::Fail("Instance is null");
        }
        OwnerType* Typed = dynamic_cast<OwnerType*>(Instance);
        if (Typed == nullptr)
        {
            return ReflectionDiagnostic::Fail("Instance type mismatch");
        }
        return ReflectionValueCodec<MemberType>::FromReflected(Typed->*MemberPointer, InValue);
    }

    static ReflectionDiagnostic ReadRaw(void* Instance, ReflectedValue& OutValue)
    {
        if (Instance == nullptr)
        {
            return ReflectionDiagnostic::Fail("Instance is null");
        }
        OwnerType* Typed = static_cast<OwnerType*>(Instance);
        return ReflectionValueCodec<MemberType>::ToReflected(Typed->*MemberPointer, OutValue);
    }

    static ReflectionDiagnostic WriteRaw(void* Instance, const ReflectedValue& InValue)
    {
        if (Instance == nullptr)
        {
            return ReflectionDiagnostic::Fail("Instance is null");
        }
        OwnerType* Typed = static_cast<OwnerType*>(Instance);
        return ReflectionValueCodec<MemberType>::FromReflected(Typed->*MemberPointer, InValue);
    }

    static void Publish(
        const char* DeclaringTypeIdString,
        const char* PropertyIdString,
        PropertyAttributes Attributes)
    {
        PropertyDescriptor Descriptor;
        Descriptor.DeclaringTypeId = TypeId{DeclaringTypeIdString};
        Descriptor.Id = PropertyId{PropertyIdString};
        Descriptor.ValueTypeId = ReflectionValueCodec<MemberType>::GetTypeId();
        Descriptor.Attributes = Attributes;
        Descriptor.bReadOnly = HasPropertyFlag(Attributes.Flags, PropertyFlags::EditorReadOnly)
            && !HasPropertyFlag(Attributes.Flags, PropertyFlags::EditorEditable);
        Descriptor.bIsField = true;
        if constexpr (std::is_base_of_v<Object, OwnerType>)
        {
            Descriptor.ReadObject = &ReadObject;
            Descriptor.WriteObject = &WriteObject;
        }
        else
        {
            Descriptor.ReadRaw = &ReadRaw;
            Descriptor.WriteRaw = &WriteRaw;
        }
        ReflectionSubsystem::Get().RegisterPendingProperty(std::move(Descriptor));
    }
};

template <typename OwnerType, typename ValueType, ValueType (OwnerType::*Getter)() const, void (OwnerType::*Setter)(ValueType)>
struct ReflectionPropertyAccess
{
    static ReflectionDiagnostic ReadObject(
        Object* Instance,
        const PropertyDescriptor&,
        ReflectedValue& OutValue)
    {
        if (Instance == nullptr)
        {
            return ReflectionDiagnostic::Fail("Instance is null");
        }
        OwnerType* Typed = dynamic_cast<OwnerType*>(Instance);
        if (Typed == nullptr)
        {
            return ReflectionDiagnostic::Fail("Instance type mismatch");
        }
        ValueType Value = (Typed->*Getter)();
        return ReflectionValueCodec<ValueType>::ToReflected(Value, OutValue);
    }

    static ReflectionDiagnostic WriteObject(
        Object* Instance,
        const PropertyDescriptor&,
        const ReflectedValue& InValue)
    {
        if (Instance == nullptr)
        {
            return ReflectionDiagnostic::Fail("Instance is null");
        }
        OwnerType* Typed = dynamic_cast<OwnerType*>(Instance);
        if (Typed == nullptr)
        {
            return ReflectionDiagnostic::Fail("Instance type mismatch");
        }
        ValueType Value{};
        ReflectionDiagnostic Converted = ReflectionValueCodec<ValueType>::FromReflected(Value, InValue);
        if (!Converted.bOk)
        {
            return Converted;
        }
        (Typed->*Setter)(Value);
        return ReflectionDiagnostic::Ok();
    }

    static void Publish(
        const char* DeclaringTypeIdString,
        const char* PropertyIdString,
        PropertyAttributes Attributes,
        bool bReadOnly)
    {
        PropertyDescriptor Descriptor;
        Descriptor.DeclaringTypeId = TypeId{DeclaringTypeIdString};
        Descriptor.Id = PropertyId{PropertyIdString};
        Descriptor.ValueTypeId = ReflectionValueCodec<ValueType>::GetTypeId();
        Descriptor.Attributes = Attributes;
        Descriptor.bReadOnly = bReadOnly;
        Descriptor.bIsField = false;
        Descriptor.ReadObject = &ReadObject;
        if (!bReadOnly)
        {
            Descriptor.WriteObject = &WriteObject;
        }
        ReflectionSubsystem::Get().RegisterPendingProperty(std::move(Descriptor));
    }
};

template <typename OwnerType, typename ValueType, ValueType (OwnerType::*Getter)() const>
struct ReflectionReadOnlyAccess
{
    static ReflectionDiagnostic ReadObject(
        Object* Instance,
        const PropertyDescriptor&,
        ReflectedValue& OutValue)
    {
        if (Instance == nullptr)
        {
            return ReflectionDiagnostic::Fail("Instance is null");
        }
        OwnerType* Typed = dynamic_cast<OwnerType*>(Instance);
        if (Typed == nullptr)
        {
            return ReflectionDiagnostic::Fail("Instance type mismatch");
        }
        ValueType Value = (Typed->*Getter)();
        return ReflectionValueCodec<ValueType>::ToReflected(Value, OutValue);
    }

    static void Publish(
        const char* DeclaringTypeIdString,
        const char* PropertyIdString,
        PropertyAttributes Attributes)
    {
        PropertyDescriptor Descriptor;
        Descriptor.DeclaringTypeId = TypeId{DeclaringTypeIdString};
        Descriptor.Id = PropertyId{PropertyIdString};
        Descriptor.ValueTypeId = ReflectionValueCodec<ValueType>::GetTypeId();
        Descriptor.Attributes = Attributes;
        Descriptor.bReadOnly = true;
        Descriptor.bIsField = false;
        Descriptor.ReadObject = &ReadObject;
        ReflectionSubsystem::Get().RegisterPendingProperty(std::move(Descriptor));
    }
};

#define ENGINE_CLASS_ROOT(ClassName, TypeIdLiteral, SchemaVersionValue) \
    SAKURA_OBJECT(ClassName) \
public: \
    static constexpr const char* StaticReflectionTypeId() { return TypeIdLiteral; } \
    static constexpr uint32_t StaticReflectionSchemaVersion() { return static_cast<uint32_t>(SchemaVersionValue); } \
    static constexpr const char* StaticReflectionBaseTypeId() { return nullptr; } \
    static constexpr CreationPolicy StaticReflectionCreationPolicy() { return CreationPolicy::Concrete; } \
    friend struct ClassName##_ReflectionClassRegistrar; \
    struct ClassName##_ReflectionClassRegistrar \
    { \
        ClassName##_ReflectionClassRegistrar() noexcept \
        { \
            static ClassRecipe Recipe{}; \
            Recipe.TypeIdString = TypeIdLiteral; \
            Recipe.BaseTypeIdString = nullptr; \
            Recipe.SchemaVersion = static_cast<uint32_t>(SchemaVersionValue); \
            Recipe.Policy = CreationPolicy::Concrete; \
            Recipe.Origin = ReflectionOrigin::Native; \
            Recipe.Bind = &ClassName##_ReflectionClassRegistrar::Bind; \
            PendingRegistry::Get().AddClassRecipe(Recipe); \
        } \
        static void Bind(); \
        static Object* CreateInstance(Class* OwningClass); \
    }; \
    static ClassName##_ReflectionClassRegistrar s_ReflectionClassRegistrar; \
private:

#define ENGINE_CLASS(ClassName, BaseClassName, TypeIdLiteral, SchemaVersionValue) \
    SAKURA_OBJECT(ClassName) \
public: \
    static constexpr const char* StaticReflectionTypeId() { return TypeIdLiteral; } \
    static constexpr uint32_t StaticReflectionSchemaVersion() { return static_cast<uint32_t>(SchemaVersionValue); } \
    static constexpr const char* StaticReflectionBaseTypeId() { return BaseClassName::StaticReflectionTypeId(); } \
    static constexpr CreationPolicy StaticReflectionCreationPolicy() { return CreationPolicy::Concrete; } \
    friend struct ClassName##_ReflectionClassRegistrar; \
    struct ClassName##_ReflectionClassRegistrar \
    { \
        ClassName##_ReflectionClassRegistrar() noexcept \
        { \
            static ClassRecipe Recipe{}; \
            Recipe.TypeIdString = TypeIdLiteral; \
            Recipe.BaseTypeIdString = BaseClassName::StaticReflectionTypeId(); \
            Recipe.SchemaVersion = static_cast<uint32_t>(SchemaVersionValue); \
            Recipe.Policy = CreationPolicy::Concrete; \
            Recipe.Origin = ReflectionOrigin::Native; \
            Recipe.Bind = &ClassName##_ReflectionClassRegistrar::Bind; \
            PendingRegistry::Get().AddClassRecipe(Recipe); \
        } \
        static void Bind(); \
        static Object* CreateInstance(Class* OwningClass); \
    }; \
    static ClassName##_ReflectionClassRegistrar s_ReflectionClassRegistrar; \
private:

#define ENGINE_ABSTRACT_CLASS(ClassName, BaseClassName, TypeIdLiteral, SchemaVersionValue) \
    SAKURA_OBJECT(ClassName) \
public: \
    static constexpr const char* StaticReflectionTypeId() { return TypeIdLiteral; } \
    static constexpr uint32_t StaticReflectionSchemaVersion() { return static_cast<uint32_t>(SchemaVersionValue); } \
    static constexpr const char* StaticReflectionBaseTypeId() { return BaseClassName::StaticReflectionTypeId(); } \
    static constexpr CreationPolicy StaticReflectionCreationPolicy() { return CreationPolicy::Abstract; } \
    friend struct ClassName##_ReflectionClassRegistrar; \
    struct ClassName##_ReflectionClassRegistrar \
    { \
        ClassName##_ReflectionClassRegistrar() noexcept \
        { \
            static ClassRecipe Recipe{}; \
            Recipe.TypeIdString = TypeIdLiteral; \
            Recipe.BaseTypeIdString = BaseClassName::StaticReflectionTypeId(); \
            Recipe.SchemaVersion = static_cast<uint32_t>(SchemaVersionValue); \
            Recipe.Policy = CreationPolicy::Abstract; \
            Recipe.Origin = ReflectionOrigin::Native; \
            Recipe.Bind = &ClassName##_ReflectionClassRegistrar::Bind; \
            PendingRegistry::Get().AddClassRecipe(Recipe); \
        } \
        static void Bind(); \
        static Object* CreateInstance(Class* OwningClass); \
    }; \
    static ClassName##_ReflectionClassRegistrar s_ReflectionClassRegistrar; \
private:

#define ENGINE_ABSTRACT_CLASS_ROOT(ClassName, TypeIdLiteral, SchemaVersionValue) \
    SAKURA_OBJECT(ClassName) \
public: \
    static constexpr const char* StaticReflectionTypeId() { return TypeIdLiteral; } \
    static constexpr uint32_t StaticReflectionSchemaVersion() { return static_cast<uint32_t>(SchemaVersionValue); } \
    static constexpr const char* StaticReflectionBaseTypeId() { return nullptr; } \
    static constexpr CreationPolicy StaticReflectionCreationPolicy() { return CreationPolicy::Abstract; } \
    friend struct ClassName##_ReflectionClassRegistrar; \
    struct ClassName##_ReflectionClassRegistrar \
    { \
        ClassName##_ReflectionClassRegistrar() noexcept \
        { \
            static ClassRecipe Recipe{}; \
            Recipe.TypeIdString = TypeIdLiteral; \
            Recipe.BaseTypeIdString = nullptr; \
            Recipe.SchemaVersion = static_cast<uint32_t>(SchemaVersionValue); \
            Recipe.Policy = CreationPolicy::Abstract; \
            Recipe.Origin = ReflectionOrigin::Native; \
            Recipe.Bind = &ClassName##_ReflectionClassRegistrar::Bind; \
            PendingRegistry::Get().AddClassRecipe(Recipe); \
        } \
        static void Bind(); \
        static Object* CreateInstance(Class* OwningClass); \
    }; \
    static ClassName##_ReflectionClassRegistrar s_ReflectionClassRegistrar; \
private:

#define ENGINE_CLASS_END(ClassName) \
    inline ClassName::ClassName##_ReflectionClassRegistrar ClassName::s_ReflectionClassRegistrar{}; \
    inline void ClassName::ClassName##_ReflectionClassRegistrar::Bind() \
    { \
        Class* Target = ReflectionSubsystem::Get().GetOrCreateClassDuringBind(ClassName::StaticReflectionTypeId()); \
        ReflectionSubsystem::Get().SetClassBindData( \
            Target, \
            ClassName::StaticReflectionBaseTypeId(), \
            ClassName::StaticReflectionSchemaVersion(), \
            ClassName::StaticReflectionCreationPolicy(), \
            &ClassName::ClassName##_ReflectionClassRegistrar::CreateInstance); \
    } \
    inline Object* ClassName::ClassName##_ReflectionClassRegistrar::CreateInstance(Class* OwningClass) \
    { \
        MemorySubsystem* Memory = MemorySubsystem::Get(); \
        if (Memory == nullptr) \
        { \
            return nullptr; \
        } \
        ClassName* Instance = Memory->NewObject<ClassName>(); \
        Instance->AssignClass(OwningClass); \
        return Instance; \
    }

#define ENGINE_STRUCT(StructName, TypeIdLiteral, SchemaVersionValue) \
public: \
    static constexpr const char* StaticReflectionTypeId() { return TypeIdLiteral; } \
    static constexpr uint32_t StaticReflectionSchemaVersion() { return static_cast<uint32_t>(SchemaVersionValue); } \
    friend struct StructName##_ReflectionStructRegistrar; \
    struct StructName##_ReflectionStructRegistrar \
    { \
        StructName##_ReflectionStructRegistrar() noexcept \
        { \
            static StructRecipe Recipe{}; \
            Recipe.TypeIdString = TypeIdLiteral; \
            Recipe.SchemaVersion = static_cast<uint32_t>(SchemaVersionValue); \
            Recipe.Bind = &StructName##_ReflectionStructRegistrar::Bind; \
            PendingRegistry::Get().AddStructRecipe(Recipe); \
        } \
        static void Bind(); \
    }; \
    static StructName##_ReflectionStructRegistrar s_ReflectionStructRegistrar; \
private:

#define ENGINE_STRUCT_END(StructName) \
    inline StructName::StructName##_ReflectionStructRegistrar StructName::s_ReflectionStructRegistrar{}; \
    inline void StructName::StructName##_ReflectionStructRegistrar::Bind() \
    { \
        ReflectionSubsystem::Get().GetOrCreateStructDuringBind( \
            StructName::StaticReflectionTypeId(), \
            StructName::StaticReflectionSchemaVersion()); \
    }

#define ENGINE_REFLECT_FIELD(OwnerType, MemberName, PropertyIdLiteral, ...) \
    struct ReflectionFieldRegistrar_##MemberName \
    { \
        static FieldRecipe& Recipe() noexcept; \
        ReflectionFieldRegistrar_##MemberName() noexcept \
        { \
            FieldRecipe& Entry = Recipe(); \
            Entry.DeclaringTypeIdString = OwnerType::StaticReflectionTypeId(); \
            Entry.PropertyIdString = PropertyIdLiteral; \
            Entry.Attributes = MakePropertyAttributes(__VA_ARGS__); \
            Entry.Bind = &Bind; \
            PendingRegistry::Get().AddFieldRecipe(Entry); \
        } \
        static void Bind(); \
    }; \
    friend struct ReflectionFieldRegistrar_##MemberName; \
    static inline ReflectionFieldRegistrar_##MemberName s_ReflectionFieldRegistrar_##MemberName{};

#define ENGINE_IMPLEMENT_FIELD(OwnerType, MemberName) \
    inline FieldRecipe& OwnerType::ReflectionFieldRegistrar_##MemberName::Recipe() noexcept \
    { \
        static FieldRecipe Instance{}; \
        return Instance; \
    } \
    inline void OwnerType::ReflectionFieldRegistrar_##MemberName::Bind() \
    { \
        FieldRecipe& Entry = Recipe(); \
        using MemberType = decltype(ReflectionMemberTypeProbe(&OwnerType::MemberName)); \
        ReflectionFieldAccess<OwnerType, MemberType, &OwnerType::MemberName>::Publish( \
            Entry.DeclaringTypeIdString, \
            Entry.PropertyIdString, \
            Entry.Attributes); \
    }

#define ENGINE_REFLECT_PROPERTY(OwnerType, PropertyToken, PropertyIdLiteral, ValueType, GetterName, SetterName, ...) \
    struct ReflectionPropertyRegistrar_##PropertyToken \
    { \
        static PropertyRecipe& Recipe() noexcept; \
        ReflectionPropertyRegistrar_##PropertyToken() noexcept \
        { \
            PropertyRecipe& Entry = Recipe(); \
            Entry.DeclaringTypeIdString = OwnerType::StaticReflectionTypeId(); \
            Entry.PropertyIdString = PropertyIdLiteral; \
            Entry.Attributes = MakePropertyAttributes(__VA_ARGS__); \
            Entry.bReadOnly = false; \
            Entry.Bind = &Bind; \
            PendingRegistry::Get().AddPropertyRecipe(Entry); \
        } \
        static void Bind(); \
    }; \
    friend struct ReflectionPropertyRegistrar_##PropertyToken; \
    static inline ReflectionPropertyRegistrar_##PropertyToken s_ReflectionPropertyRegistrar_##PropertyToken{};

#define ENGINE_IMPLEMENT_PROPERTY(OwnerType, PropertyToken, ValueType, GetterName, SetterName) \
    inline PropertyRecipe& OwnerType::ReflectionPropertyRegistrar_##PropertyToken::Recipe() noexcept \
    { \
        static PropertyRecipe Instance{}; \
        return Instance; \
    } \
    inline void OwnerType::ReflectionPropertyRegistrar_##PropertyToken::Bind() \
    { \
        PropertyRecipe& Entry = Recipe(); \
        ReflectionPropertyAccess<OwnerType, ValueType, &OwnerType::GetterName, &OwnerType::SetterName>::Publish( \
            Entry.DeclaringTypeIdString, \
            Entry.PropertyIdString, \
            Entry.Attributes, \
            false); \
    }

#define ENGINE_REFLECT_READONLY(OwnerType, PropertyToken, PropertyIdLiteral, ValueType, GetterName, ...) \
    struct ReflectionReadOnlyRegistrar_##PropertyToken \
    { \
        static PropertyRecipe& Recipe() noexcept; \
        ReflectionReadOnlyRegistrar_##PropertyToken() noexcept \
        { \
            PropertyRecipe& Entry = Recipe(); \
            Entry.DeclaringTypeIdString = OwnerType::StaticReflectionTypeId(); \
            Entry.PropertyIdString = PropertyIdLiteral; \
            Entry.Attributes = MakePropertyAttributes(__VA_ARGS__); \
            Entry.bReadOnly = true; \
            Entry.Bind = &Bind; \
            PendingRegistry::Get().AddPropertyRecipe(Entry); \
        } \
        static void Bind(); \
    }; \
    friend struct ReflectionReadOnlyRegistrar_##PropertyToken; \
    static inline ReflectionReadOnlyRegistrar_##PropertyToken s_ReflectionReadOnlyRegistrar_##PropertyToken{};

#define ENGINE_IMPLEMENT_READONLY(OwnerType, PropertyToken, ValueType, GetterName) \
    inline PropertyRecipe& OwnerType::ReflectionReadOnlyRegistrar_##PropertyToken::Recipe() noexcept \
    { \
        static PropertyRecipe Instance{}; \
        return Instance; \
    } \
    inline void OwnerType::ReflectionReadOnlyRegistrar_##PropertyToken::Bind() \
    { \
        PropertyRecipe& Entry = Recipe(); \
        ReflectionReadOnlyAccess<OwnerType, ValueType, &OwnerType::GetterName>::Publish( \
            Entry.DeclaringTypeIdString, \
            Entry.PropertyIdString, \
            Entry.Attributes); \
    }

#define ENGINE_REFLECT_ENUM(EnumType, TypeIdLiteral, SchemaVersionValue) \
    struct EnumType##_ReflectionEnumRegistrar \
    { \
        EnumType##_ReflectionEnumRegistrar() noexcept \
        { \
            static EnumRecipe Recipe{}; \
            Recipe.TypeIdString = TypeIdLiteral; \
            Recipe.SchemaVersion = static_cast<uint32_t>(SchemaVersionValue); \
            Recipe.Bind = &EnumType##_ReflectionEnumRegistrar::Bind; \
            PendingRegistry::Get().AddEnumRecipe(Recipe); \
        } \
        static void Bind(); \
    }; \
    static inline EnumType##_ReflectionEnumRegistrar s_ReflectionEnumRegistrar_##EnumType{}
