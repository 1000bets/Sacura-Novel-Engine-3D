#include "Reflection/Class.h"
#include "Core/MemorySubsystem.h"

Class::Class()
    : Object("Class")
{
}

Class::~Class()
{
    if (bClassDefaultObjectOwned && ClassDefaultObject != nullptr)
    {
        if (MemorySubsystem* Memory = MemorySubsystem::Get())
        {
            Memory->DestroyObject(ClassDefaultObject);
        }
        else
        {
            delete ClassDefaultObject;
        }
        ClassDefaultObject = nullptr;
        bClassDefaultObjectOwned = false;
    }
}

bool Class::IsA(const TypeId& OtherTypeId) const
{
    const Class* Current = this;
    while (Current != nullptr)
    {
        if (Current->Id == OtherTypeId)
        {
            return true;
        }
        Current = Current->BaseClass;
    }
    return false;
}

bool Class::IsA(const Class* OtherClass) const
{
    if (OtherClass == nullptr)
    {
        return false;
    }
    return IsA(OtherClass->Id);
}

PropertyDescriptor* Class::FindProperty(const PropertyId& Property)
{
    for (PropertyDescriptor& Entry : Properties)
    {
        if (Entry.Id == Property)
        {
            return &Entry;
        }
    }
    return nullptr;
}

const PropertyDescriptor* Class::FindProperty(const PropertyId& Property) const
{
    for (const PropertyDescriptor& Entry : Properties)
    {
        if (Entry.Id == Property)
        {
            return &Entry;
        }
    }
    return nullptr;
}
