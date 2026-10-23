#include "Reflection/Class.h"
#include "Core/Object/MemorySubsystem.h"
#include <cctype>

std::string Class::GetDisplayName() const
{
    if (!DisplayName.empty())
    {
        return DisplayName;
    }
    std::string Name = Id.Value.substr(Id.Value.find_last_of('.') + 1);
    if (Name.size() > 9 && Name.compare(Name.size() - 9, 9, "Component") == 0)
    {
        Name.resize(Name.size() - 9);
    }
    std::string Caption;
    for (size_t Index = 0; Index < Name.size(); ++Index)
    {
        if (Index > 0 && std::isupper(static_cast<unsigned char>(Name[Index]))
            && (std::islower(static_cast<unsigned char>(Name[Index - 1]))
                || std::isdigit(static_cast<unsigned char>(Name[Index - 1]))
                || (Index + 1 < Name.size() && std::islower(static_cast<unsigned char>(Name[Index + 1])))))
        {
            Caption += ' ';
        }
        Caption += Name[Index];
    }
    return Caption;
}

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
    if (BaseClass != nullptr)
    {
        return BaseClass->FindProperty(Property);
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
    if (BaseClass != nullptr)
    {
        return BaseClass->FindProperty(Property);
    }
    return nullptr;
}

std::vector<const PropertyDescriptor*> Class::GetAllProperties() const
{
    std::vector<const PropertyDescriptor*> Result;
    for (const Class* Current = this; Current != nullptr; Current = Current->GetBaseClass())
    {
        for (const PropertyDescriptor& Descriptor : Current->GetProperties())
        {
            bool bExists = false;
            for (const PropertyDescriptor* Added : Result)
            {
                bExists = bExists || Added->Id == Descriptor.Id;
            }
            if (!bExists)
            {
                Result.push_back(&Descriptor);
            }
        }
    }
    return Result;
}
