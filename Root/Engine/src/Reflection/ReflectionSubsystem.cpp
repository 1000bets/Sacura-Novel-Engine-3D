#include "Reflection/ReflectionSubsystem.h"

#include "Core/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Gameplay/Component.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/ScriptComponent.h"
#include "Reflection/PendingRegistry.h"
#include "Reflection/PropertyAccess.h"
#include "Reflection/ReflectionValueCodec.h"

#include <SimpleMath.h>
#include <cstring>
#include <new>
#include <unordered_set>

using namespace DirectX::SimpleMath;

namespace
{
ReflectionDiagnostic ValidateFloatRange(const ReflectedValue& InValue, const PropertyAttributes* Attributes)
{
    if (InValue.ValueKind != ReflectedValue::Kind::Float64)
    {
        return ReflectionDiagnostic::Fail("Expected float64");
    }
    if (Attributes != nullptr && Attributes->bHasRange)
    {
        if (InValue.Float64Value < Attributes->RangeMinimum || InValue.Float64Value > Attributes->RangeMaximum)
        {
            return ReflectionDiagnostic::Fail("Value out of range");
        }
    }
    return ReflectionDiagnostic::Ok();
}

template <typename ValueType>
void FillBuiltinType(TypeDescriptor& Descriptor, const char* TypeIdString, size_t ByteSize)
{
    Descriptor.Kind = TypeKind::Value;
    Descriptor.Id = TypeId{TypeIdString};
    Descriptor.SchemaVersion = 1;
    Descriptor.ByteSize = ByteSize;
    Descriptor.Construct = [](void* Destination)
    {
        new (Destination) ValueType();
    };
    Descriptor.Destroy = [](void* Destination)
    {
        static_cast<ValueType*>(Destination)->~ValueType();
    };
    Descriptor.Copy = [](void* Destination, const void* Source)
    {
        *static_cast<ValueType*>(Destination) = *static_cast<const ValueType*>(Source);
    };
    Descriptor.Equals = [](const void* Left, const void* Right)
    {
        return *static_cast<const ValueType*>(Left) == *static_cast<const ValueType*>(Right);
    };
    Descriptor.ToReflected = [](const void* Source, ReflectedValue& OutValue)
    {
        return ReflectionValueCodec<ValueType>::ToReflected(*static_cast<const ValueType*>(Source), OutValue);
    };
    Descriptor.FromReflected = [](void* Destination, const ReflectedValue& InValue)
    {
        return ReflectionValueCodec<ValueType>::FromReflected(*static_cast<ValueType*>(Destination), InValue);
    };
}
}

ReflectionSubsystem& ReflectionSubsystem::Get()
{
    static ReflectionSubsystem Instance;
    return Instance;
}

void ReflectionSubsystem::RegisterBuiltinValueTypes()
{
    {
        TypeDescriptor Descriptor;
        FillBuiltinType<bool>(Descriptor, "engine.bool", sizeof(bool));
        RegisterTypeDescriptor(std::move(Descriptor));
    }
    {
        TypeDescriptor Descriptor;
        FillBuiltinType<int64_t>(Descriptor, "engine.int64", sizeof(int64_t));
        RegisterTypeDescriptor(std::move(Descriptor));
    }
    {
        TypeDescriptor Descriptor;
        FillBuiltinType<float>(Descriptor, "engine.float", sizeof(float));
        Descriptor.Validate = &ValidateFloatRange;
        RegisterTypeDescriptor(std::move(Descriptor));
    }
    {
        TypeDescriptor Descriptor;
        FillBuiltinType<double>(Descriptor, "engine.double", sizeof(double));
        Descriptor.Validate = &ValidateFloatRange;
        RegisterTypeDescriptor(std::move(Descriptor));
    }
    {
        TypeDescriptor Descriptor;
        FillBuiltinType<std::string>(Descriptor, "engine.string", sizeof(std::string));
        RegisterTypeDescriptor(std::move(Descriptor));
    }
    {
        TypeDescriptor Descriptor;
        FillBuiltinType<Vector3>(Descriptor, "engine.Vector3", sizeof(Vector3));
        RegisterTypeDescriptor(std::move(Descriptor));
    }
    {
        TypeDescriptor Descriptor;
        FillBuiltinType<Quaternion>(Descriptor, "engine.Quaternion", sizeof(Quaternion));
        RegisterTypeDescriptor(std::move(Descriptor));
    }
    {
        TypeDescriptor Descriptor;
        FillBuiltinType<Color>(Descriptor, "engine.Color", sizeof(Color));
        RegisterTypeDescriptor(std::move(Descriptor));
    }
}

void ReflectionSubsystem::RegisterTypeDescriptor(TypeDescriptor Descriptor)
{
    TypeId Id = Descriptor.Id;
    if (TypesById.find(Id) != TypesById.end())
    {
        return;
    }

    TypeDescriptor* Stored = new TypeDescriptor(std::move(Descriptor));
    TypesById.emplace(Id, Stored);
    Catalog.Types.push_back(Stored);
}

void ReflectionSubsystem::RegisterPendingProperty(PropertyDescriptor Descriptor)
{
    PendingProperties.push_back(std::move(Descriptor));
}

Class* ReflectionSubsystem::GetOrCreateClassDuringBind(const char* TypeIdString)
{
    TypeId Id{TypeIdString};
    auto Found = ClassesById.find(Id);
    if (Found != ClassesById.end())
    {
        return Found->second;
    }

    MemorySubsystem* Memory = MemorySubsystem::Get();
    if (Memory == nullptr)
    {
        PrintString("Reflection: MemorySubsystem required to create Class metadata");
        return nullptr;
    }

    Class* Created = Memory->NewObject<Class>();
    Created->Id = Id;
    Created->NativeBackingTypeId = Id;
    Created->AssignClass(nullptr);
    ClassesById.emplace(Id, Created);
    Catalog.Classes.push_back(Created);
    return Created;
}

void ReflectionSubsystem::SetClassBindData(
    Class* Target,
    const char* BaseTypeIdString,
    uint32_t SchemaVersion,
    CreationPolicy Policy,
    Class::FactoryFunction Factory)
{
    if (Target == nullptr)
    {
        return;
    }

    Target->SchemaVersion = SchemaVersion;
    Target->Policy = Policy;
    Target->Origin = ReflectionOrigin::Native;
    Target->PendingBaseTypeIdString = BaseTypeIdString;
    Target->Factory = Factory;
    Target->NativeBackingTypeId = Target->Id;
}

TypeDescriptor* ReflectionSubsystem::GetOrCreateStructDuringBind(const char* TypeIdString, uint32_t SchemaVersion)
{
    TypeId Id{TypeIdString};
    auto Found = TypesById.find(Id);
    if (Found != TypesById.end())
    {
        Found->second->SchemaVersion = SchemaVersion;
        return Found->second;
    }

    TypeDescriptor Descriptor;
    Descriptor.Kind = TypeKind::Value;
    Descriptor.Id = Id;
    Descriptor.SchemaVersion = SchemaVersion;
    TypeDescriptor* Stored = new TypeDescriptor(std::move(Descriptor));
    TypesById.emplace(Id, Stored);
    Catalog.Types.push_back(Stored);
    return Stored;
}

Class* ReflectionSubsystem::FindClass(const TypeId& Id) const
{
    auto Found = ClassesById.find(Id);
    if (Found == ClassesById.end())
    {
        return nullptr;
    }
    return Found->second;
}

Class* ReflectionSubsystem::FindClass(const char* TypeIdString) const
{
    return FindClass(TypeId{TypeIdString});
}

TypeDescriptor* ReflectionSubsystem::FindType(const TypeId& Id) const
{
    auto Found = TypesById.find(Id);
    if (Found == TypesById.end())
    {
        return nullptr;
    }
    return Found->second;
}

TypeDescriptor* ReflectionSubsystem::FindType(const char* TypeIdString) const
{
    return FindType(TypeId{TypeIdString});
}

EnumDescriptor* ReflectionSubsystem::FindEnum(const TypeId& Id) const
{
    auto Found = EnumsById.find(Id);
    if (Found == EnumsById.end())
    {
        return nullptr;
    }
    return Found->second;
}

EnumDescriptor* ReflectionSubsystem::FindEnum(const char* TypeIdString) const
{
    return FindEnum(TypeId{TypeIdString});
}

const PropertyDescriptor* ReflectionSubsystem::FindStructProperty(const TypeId& DeclaringTypeId, const PropertyId& Property) const
{
    auto Found = StructPropertiesById.find(DeclaringTypeId);
    if (Found == StructPropertiesById.end())
    {
        return nullptr;
    }
    for (const PropertyDescriptor& Entry : Found->second)
    {
        if (Entry.Id == Property)
        {
            return &Entry;
        }
    }
    return nullptr;
}

const std::vector<PropertyDescriptor>* ReflectionSubsystem::FindStructProperties(const TypeId& DeclaringTypeId) const
{
    auto Found = StructPropertiesById.find(DeclaringTypeId);
    if (Found == StructPropertiesById.end())
    {
        return nullptr;
    }
    return &Found->second;
}

ReflectionDiagnostic ReflectionSubsystem::DetectDuplicateTypeIds() const
{
    std::unordered_set<std::string> SeenClassIds;
    for (ClassRecipe* Recipe = PendingRegistry::Get().GetClassRecipes(); Recipe != nullptr; Recipe = Recipe->Next)
    {
        if (Recipe->TypeIdString == nullptr)
        {
            continue;
        }
        if (!SeenClassIds.insert(Recipe->TypeIdString).second)
        {
            return ReflectionDiagnostic::Fail(
                std::string("Duplicate Class TypeId: ") + Recipe->TypeIdString,
                TypeId{Recipe->TypeIdString});
        }
    }

    std::unordered_set<std::string> SeenTypeIds;
    for (StructRecipe* Recipe = PendingRegistry::Get().GetStructRecipes(); Recipe != nullptr; Recipe = Recipe->Next)
    {
        if (Recipe->TypeIdString == nullptr)
        {
            continue;
        }
        if (!SeenTypeIds.insert(Recipe->TypeIdString).second)
        {
            return ReflectionDiagnostic::Fail(
                std::string("Duplicate Struct TypeId: ") + Recipe->TypeIdString,
                TypeId{Recipe->TypeIdString});
        }
    }

    return ReflectionDiagnostic::Ok();
}

ReflectionDiagnostic ReflectionSubsystem::BuildFromPending()
{
    ReflectionDiagnostic DuplicateCheck = DetectDuplicateTypeIds();
    if (!DuplicateCheck.bOk)
    {
        return DuplicateCheck;
    }

    for (ClassRecipe* Recipe = PendingRegistry::Get().GetClassRecipes(); Recipe != nullptr; Recipe = Recipe->Next)
    {
        if (Recipe->Bind != nullptr)
        {
            Recipe->Bind();
        }
    }

    for (StructRecipe* Recipe = PendingRegistry::Get().GetStructRecipes(); Recipe != nullptr; Recipe = Recipe->Next)
    {
        if (Recipe->Bind != nullptr)
        {
            Recipe->Bind();
        }
    }

    for (EnumRecipe* Recipe = PendingRegistry::Get().GetEnumRecipes(); Recipe != nullptr; Recipe = Recipe->Next)
    {
        if (Recipe->Bind != nullptr)
        {
            Recipe->Bind();
        }
    }

    for (FieldRecipe* Recipe = PendingRegistry::Get().GetFieldRecipes(); Recipe != nullptr; Recipe = Recipe->Next)
    {
        if (Recipe->Bind != nullptr)
        {
            Recipe->Bind();
        }
    }

    for (PropertyRecipe* Recipe = PendingRegistry::Get().GetPropertyRecipes(); Recipe != nullptr; Recipe = Recipe->Next)
    {
        if (Recipe->Bind != nullptr)
        {
            Recipe->Bind();
        }
    }

    for (Class* Entry : Catalog.Classes)
    {
        if (Entry == nullptr)
        {
            continue;
        }
        if (Entry->PendingBaseTypeIdString != nullptr)
        {
            Entry->BaseClass = FindClass(Entry->PendingBaseTypeIdString);
            if (Entry->BaseClass == nullptr)
            {
                return ReflectionDiagnostic::Fail(
                    std::string("Missing base class: ") + Entry->PendingBaseTypeIdString,
                    Entry->Id);
            }
        }
        Entry->PendingBaseTypeIdString = nullptr;
    }

    for (PropertyDescriptor& Property : PendingProperties)
    {
        if (Property.ReadRaw != nullptr && Property.ReadObject == nullptr)
        {
            StructPropertiesById[Property.DeclaringTypeId].push_back(Property);
            continue;
        }

        Class* DeclaringClass = FindClass(Property.DeclaringTypeId);
        if (DeclaringClass == nullptr)
        {
            return ReflectionDiagnostic::Fail("Property declaring class missing", Property.DeclaringTypeId, Property.Id);
        }

        if (DeclaringClass->FindProperty(Property.Id) != nullptr)
        {
            return ReflectionDiagnostic::Fail("Duplicate property id", Property.DeclaringTypeId, Property.Id);
        }

        DeclaringClass->Properties.push_back(Property);
    }

    PendingProperties.clear();
    return ReflectionDiagnostic::Ok();
}

ReflectionDiagnostic ReflectionSubsystem::BuildClassDefaults()
{
    for (Class* Entry : Catalog.Classes)
    {
        if (Entry == nullptr || !Entry->IsConcrete() || Entry->Factory == nullptr)
        {
            continue;
        }
        if (Entry->ClassDefaultObject != nullptr)
        {
            continue;
        }

        Object* DefaultObject = Entry->Factory(Entry);
        if (DefaultObject == nullptr)
        {
            return ReflectionDiagnostic::Fail("Failed to build CDO", Entry->Id);
        }
        DefaultObject->AssignClass(Entry);
        Entry->ClassDefaultObject = DefaultObject;
        Entry->bClassDefaultObjectOwned = true;
    }

    return ReflectionDiagnostic::Ok();
}

ReflectionDiagnostic ReflectionSubsystem::Validate() const
{
    for (Class* Entry : Catalog.Classes)
    {
        if (Entry == nullptr || !Entry->Id.IsValid())
        {
            return ReflectionDiagnostic::Fail("Invalid class entry");
        }
        if (Entry->IsConcrete() && Entry->Factory == nullptr)
        {
            return ReflectionDiagnostic::Fail("Concrete class missing factory", Entry->Id);
        }
        if (Entry->IsConcrete() && Entry->ClassDefaultObject == nullptr)
        {
            return ReflectionDiagnostic::Fail("Concrete class missing CDO", Entry->Id);
        }
    }
    return ReflectionDiagnostic::Ok();
}

ReflectionDiagnostic ReflectionSubsystem::InitializeNative()
{
    if (bInitialized)
    {
        return ReflectionDiagnostic::Ok();
    }

    RegisterNativeReflectionBootstrap();
    ForceTouchRegistrars();
    RegisterBuiltinValueTypes();

    ReflectionDiagnostic Built = BuildFromPending();
    if (!Built.bOk)
    {
        PrintString(std::string("Reflection InitializeNative failed: ") + Built.Message);
        ClearMetadata();
        return Built;
    }

    ReflectionDiagnostic Defaults = BuildClassDefaults();
    if (!Defaults.bOk)
    {
        PrintString(std::string("Reflection CDO build failed: ") + Defaults.Message);
        ClearMetadata();
        return Defaults;
    }

    ReflectionDiagnostic Validated = Validate();
    if (!Validated.bOk)
    {
        PrintString(std::string("Reflection validate failed: ") + Validated.Message);
        ClearMetadata();
        return Validated;
    }

    bInitialized = true;
    PrintString("Reflection: InitializeNative complete");
    return ReflectionDiagnostic::Ok();
}

void ReflectionSubsystem::ClearMetadata()
{
    for (Class* Entry : Catalog.Classes)
    {
        if (Entry == nullptr)
        {
            continue;
        }
        if (Entry->bClassDefaultObjectOwned && Entry->ClassDefaultObject != nullptr)
        {
            if (MemorySubsystem* Memory = MemorySubsystem::Get())
            {
                Memory->DestroyObject(Entry->ClassDefaultObject);
            }
            else
            {
                delete Entry->ClassDefaultObject;
            }
            Entry->ClassDefaultObject = nullptr;
            Entry->bClassDefaultObjectOwned = false;
        }

        if (MemorySubsystem* Memory = MemorySubsystem::Get())
        {
            Memory->DestroyObject(Entry);
        }
        else
        {
            delete Entry;
        }
    }

    for (TypeDescriptor* Entry : Catalog.Types)
    {
        delete Entry;
    }

    for (EnumDescriptor* Entry : Catalog.Enums)
    {
        delete Entry;
    }

    Catalog = {};
    ClassesById.clear();
    TypesById.clear();
    EnumsById.clear();
    StructPropertiesById.clear();
    PendingProperties.clear();
    bInitialized = false;
}

void ReflectionSubsystem::Shutdown()
{
    if (!bInitialized && Catalog.Classes.empty() && Catalog.Types.empty())
    {
        return;
    }

    PrintString("Reflection: shutting down");
    ClearMetadata();
}

Object* ReflectionSubsystem::CreateInstance(const TypeId& Id)
{
    Class* Target = FindClass(Id);
    if (Target == nullptr || !Target->IsConcrete() || Target->Factory == nullptr)
    {
        return nullptr;
    }

    Object* Instance = Target->Factory(Target);
    if (Instance == nullptr)
    {
        return nullptr;
    }

    Instance->AssignClass(Target);

    Object* Defaults = Target->GetClassDefaultObject();
    if (Defaults != nullptr && Defaults != Instance)
    {
        PropertyAccess::CopyPropertiesFrom(Defaults, Instance, PropertyAccessContext::Deserialize);
    }

    return Instance;
}

Component* ReflectionSubsystem::CreateComponent(GameObject& Owner, const TypeId& Id)
{
    Object* Instance = CreateInstance(Id);
    Component* Created = dynamic_cast<Component*>(Instance);
    if (Created == nullptr)
    {
        if (Instance != nullptr)
        {
            if (MemorySubsystem* Memory = MemorySubsystem::Get())
            {
                Memory->DestroyObject(Instance);
            }
            else
            {
                delete Instance;
            }
        }
        return nullptr;
    }

    return Owner.AddExistingComponent(Created);
}

namespace
{
Object* CreatePythonBackedScriptComponent(Class* OwningClass)
{
    MemorySubsystem* Memory = MemorySubsystem::Get();
    if (Memory == nullptr || OwningClass == nullptr)
    {
        return nullptr;
    }

    ScriptComponent* Instance = Memory->NewObject<ScriptComponent>();
    Instance->AssignClass(OwningClass);

    if (Object* DefaultObject = OwningClass->GetClassDefaultObject())
    {
        if (ScriptComponent* DefaultScript = dynamic_cast<ScriptComponent*>(DefaultObject))
        {
            Instance->CopyManagedPropertiesFrom(*DefaultScript);
        }
    }

    return Instance;
}
}

ReflectionDiagnostic ReflectionSubsystem::PublishPythonClass(
    const TypeId& Id,
    uint32_t SchemaVersion,
    const TypeId& BaseTypeId,
    const TypeId& NativeBackingTypeId,
    std::vector<PropertyDescriptor> Properties,
    const std::unordered_map<PropertyId, ReflectedValue, PropertyIdHash>& DefaultValues)
{
    if (!bInitialized)
    {
        return ReflectionDiagnostic::Fail("Reflection not initialized", Id);
    }
    if (!Id.IsValid())
    {
        return ReflectionDiagnostic::Fail("Invalid Python class TypeId");
    }
    if (FindClass(Id) != nullptr)
    {
        return ReflectionDiagnostic::Fail("Python class TypeId already registered", Id);
    }

    Class* BaseClass = FindClass(BaseTypeId);
    if (BaseClass == nullptr)
    {
        return ReflectionDiagnostic::Fail("Missing Python class base", Id);
    }

    MemorySubsystem* Memory = MemorySubsystem::Get();
    if (Memory == nullptr)
    {
        return ReflectionDiagnostic::Fail("MemorySubsystem required", Id);
    }

    Class* Created = Memory->NewObject<Class>();
    Created->Id = Id;
    Created->SchemaVersion = SchemaVersion;
    Created->Origin = ReflectionOrigin::Python;
    Created->Policy = CreationPolicy::Concrete;
    Created->BaseClass = BaseClass;
    Created->NativeBackingTypeId = NativeBackingTypeId;
    Created->Factory = &CreatePythonBackedScriptComponent;
    Created->Properties = std::move(Properties);
    Created->AssignClass(nullptr);

    for (PropertyDescriptor& Property : Created->Properties)
    {
        Property.DeclaringTypeId = Id;
        if (Property.ReadObject == nullptr)
        {
            Property.ReadObject = &ScriptComponent::ReadManagedProperty;
        }
        if (!Property.bReadOnly && Property.WriteObject == nullptr)
        {
            Property.WriteObject = &ScriptComponent::WriteManagedProperty;
        }
    }

    Object* DefaultObject = Created->Factory(Created);
    if (DefaultObject == nullptr)
    {
        Memory->DestroyObject(Created);
        return ReflectionDiagnostic::Fail("Failed to build Python CDO", Id);
    }
    DefaultObject->AssignClass(Created);
    if (ScriptComponent* DefaultScript = dynamic_cast<ScriptComponent*>(DefaultObject))
    {
        for (const auto& Pair : DefaultValues)
        {
            DefaultScript->SetManagedProperty(Pair.first, Pair.second);
        }
    }
    Created->ClassDefaultObject = DefaultObject;
    Created->bClassDefaultObjectOwned = true;

    ClassesById.emplace(Id, Created);
    Catalog.Classes.push_back(Created);
    PrintString(std::string("Reflection: published Python class ") + Id.Value);
    return ReflectionDiagnostic::Ok();
}
