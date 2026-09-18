#pragma once

#include "Reflection/Class.h"
#include "Reflection/ClassRef.h"
#include "Reflection/EnumDescriptor.h"
#include "Reflection/PropertyDescriptor.h"
#include "Reflection/ReflectedValue.h"
#include "Reflection/TypeDescriptor.h"
#include "Reflection/TypeId.h"

#include <string>
#include <unordered_map>
#include <vector>

class GameObject;
class Component;

struct PublishedReflectionCatalog
{
    std::vector<Class*> Classes;
    std::vector<TypeDescriptor*> Types;
    std::vector<EnumDescriptor*> Enums;
};

class ReflectionSubsystem
{
public:
    static ReflectionSubsystem& Get();

    ReflectionDiagnostic InitializeNative();
    void Shutdown();

    bool IsInitialized() const { return bInitialized; }

    Class* FindClass(const TypeId& Id) const;
    Class* FindClass(const char* TypeIdString) const;
    TypeDescriptor* FindType(const TypeId& Id) const;
    TypeDescriptor* FindType(const char* TypeIdString) const;
    EnumDescriptor* FindEnum(const TypeId& Id) const;
    EnumDescriptor* FindEnum(const char* TypeIdString) const;
    const PropertyDescriptor* FindStructProperty(const TypeId& DeclaringTypeId, const PropertyId& Property) const;
    const std::vector<PropertyDescriptor>* FindStructProperties(const TypeId& DeclaringTypeId) const;

    const PublishedReflectionCatalog& GetPublishedCatalog() const { return Catalog; }

    ReflectionDiagnostic Validate() const;

    Object* CreateInstance(const TypeId& Id);
    Component* CreateComponent(GameObject& Owner, const TypeId& Id);

    void RegisterTypeDescriptor(TypeDescriptor Descriptor);
    void RegisterPendingProperty(PropertyDescriptor Descriptor);

    ReflectionDiagnostic PublishPythonClass(
        const TypeId& Id,
        uint32_t SchemaVersion,
        const TypeId& BaseTypeId,
        const TypeId& NativeBackingTypeId,
        std::vector<PropertyDescriptor> Properties,
        const std::unordered_map<PropertyId, ReflectedValue, PropertyIdHash>& DefaultValues);

    Class* GetOrCreateClassDuringBind(const char* TypeIdString);
    void SetClassBindData(
        Class* Target,
        const char* BaseTypeIdString,
        uint32_t SchemaVersion,
        CreationPolicy Policy,
        Class::FactoryFunction Factory);

    TypeDescriptor* GetOrCreateStructDuringBind(const char* TypeIdString, uint32_t SchemaVersion);

private:
    ReflectionSubsystem() = default;

    ReflectionDiagnostic BuildFromPending();
    ReflectionDiagnostic BuildClassDefaults();
    ReflectionDiagnostic DetectDuplicateTypeIds() const;
    void RegisterBuiltinValueTypes();
    void ClearMetadata();

    bool bInitialized = false;
    PublishedReflectionCatalog Catalog;

    std::unordered_map<TypeId, Class*, TypeIdHash> ClassesById;
    std::unordered_map<TypeId, TypeDescriptor*, TypeIdHash> TypesById;
    std::unordered_map<TypeId, EnumDescriptor*, TypeIdHash> EnumsById;
    std::unordered_map<TypeId, std::vector<PropertyDescriptor>, TypeIdHash> StructPropertiesById;

    std::vector<PropertyDescriptor> PendingProperties;
};

void ForceTouchRegistrars();
void RegisterNativeReflectionBootstrap();

template <typename TBase>
inline Class* ClassRef<TBase>::Resolve() const
{
    return ReflectionSubsystem::Get().FindClass(Id);
}
