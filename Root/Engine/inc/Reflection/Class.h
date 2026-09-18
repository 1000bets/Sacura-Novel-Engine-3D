#pragma once

#include "Gameplay/Object.h"
#include "Reflection/PropertyDescriptor.h"
#include "Reflection/TypeId.h"

#include <vector>

class Class : public Object
{
    SAKURA_OBJECT(Class)

public:
    ~Class() override;

    const TypeId& GetTypeId() const { return Id; }
    uint32_t GetSchemaVersion() const { return SchemaVersion; }
    ReflectionOrigin GetOrigin() const { return Origin; }
    CreationPolicy GetCreationPolicy() const { return Policy; }
    Class* GetBaseClass() const { return BaseClass; }
    const TypeId& GetNativeBackingTypeId() const { return NativeBackingTypeId; }
    Object* GetClassDefaultObject() const { return ClassDefaultObject; }
    const std::vector<PropertyDescriptor>& GetProperties() const { return Properties; }

    bool IsA(const TypeId& OtherTypeId) const;
    bool IsA(const Class* OtherClass) const;
    bool IsAbstract() const { return Policy == CreationPolicy::Abstract; }
    bool IsConcrete() const { return Policy == CreationPolicy::Concrete; }

    PropertyDescriptor* FindProperty(const PropertyId& Property);
    const PropertyDescriptor* FindProperty(const PropertyId& Property) const;

    using FactoryFunction = Object* (*)(Class* OwningClass);

    TypeId Id{};
    uint32_t SchemaVersion = 1;
    ReflectionOrigin Origin = ReflectionOrigin::Native;
    CreationPolicy Policy = CreationPolicy::Concrete;
    Class* BaseClass = nullptr;
    const char* PendingBaseTypeIdString = nullptr;
    TypeId NativeBackingTypeId{};
    Object* ClassDefaultObject = nullptr;
    bool bClassDefaultObjectOwned = false;
    FactoryFunction Factory = nullptr;
    std::vector<PropertyDescriptor> Properties;

protected:
    Class();
};
