#include "Core/MemorySubsystem.h"
#include "Engine.h"
#include "Gameplay/CameraComponent.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/LightSettings.h"
#include "Reflection/PendingRegistry.h"
#include "Reflection/PropertyAccess.h"
#include "Reflection/ReflectionSubsystem.h"

#include <iostream>
#include <string>
#include <vector>

void ReflectionTestTouchSecondaryTranslationUnit();

namespace
{
int FailureCount = 0;

void Expect(bool Condition, const char* Message)
{
    if (!Condition)
    {
        ++FailureCount;
        std::cout << "FAIL: " << Message << '\n';
    }
    else
    {
        std::cout << "  OK: " << Message << '\n';
    }
}

bool HasProperty(Class* Target, const char* PropertyName)
{
    if (Target == nullptr)
    {
        return false;
    }
    return Target->FindProperty(PropertyId{PropertyName}) != nullptr;
}

bool HasStructProperty(const TypeId& Id, const char* PropertyName)
{
    return ReflectionSubsystem::Get().FindStructProperty(Id, PropertyId{PropertyName}) != nullptr;
}
}

int main()
{
    ReflectionTestTouchSecondaryTranslationUnit();

    Engine Application;
    Application.InitializeHeadless({});

    ReflectionSubsystem& Reflection = ReflectionSubsystem::Get();
    Expect(Reflection.IsInitialized(), "Reflection initialized");

    Class* CameraClass = Reflection.FindClass("engine.CameraComponent");
    Expect(CameraClass != nullptr, "FindClass CameraComponent");
    if (CameraClass != nullptr)
    {
        Expect(HasProperty(CameraClass, "field_of_view"), "Camera field_of_view property");
        Expect(HasProperty(CameraClass, "near_plane"), "Camera near_plane field");
        Expect(HasProperty(CameraClass, "far_plane"), "Camera far_plane field");
        Expect(HasProperty(CameraClass, "aspect_ratio"), "Camera aspect_ratio field");
        Expect(HasProperty(CameraClass, "primary"), "Camera primary field");
        Expect(CameraClass->GetProperties().size() == 5, "Camera property count");
    }

    TypeDescriptor* LightType = Reflection.FindType("engine.LightSettings");
    Expect(LightType != nullptr, "FindType LightSettings");
    TypeId LightTypeId{"engine.LightSettings"};
    Expect(HasStructProperty(LightTypeId, "light_color"), "LightSettings light_color");
    Expect(HasStructProperty(LightTypeId, "intensity"), "LightSettings intensity");
    Expect(HasStructProperty(LightTypeId, "direction"), "LightSettings direction");
    Expect(HasStructProperty(LightTypeId, "cast_shadows"), "LightSettings cast_shadows");
    const std::vector<PropertyDescriptor>* LightProperties = Reflection.FindStructProperties(LightTypeId);
    Expect(LightProperties != nullptr && LightProperties->size() == 4, "LightSettings property count");

    Object* CreatedObject = Reflection.CreateInstance(TypeId{"engine.CameraComponent"});
    Expect(CreatedObject != nullptr, "CreateInstance CameraComponent");
    CameraComponent* Camera = dynamic_cast<CameraComponent*>(CreatedObject);
    Expect(Camera != nullptr, "CreateInstance type");
    Expect(Camera != nullptr && Camera->GetClass() == CameraClass, "Instance Class assigned");

    ReflectedValue FovValue;
    ReflectionDiagnostic GetFov = PropertyAccess::GetProperty(Camera, PropertyId{"field_of_view"}, FovValue);
    Expect(GetFov.bOk && FovValue.ValueKind == ReflectedValue::Kind::Float64, "Get field_of_view");
    Expect(GetFov.bOk && FovValue.Float64Value == 60.0, "Default field_of_view");

    ReflectionDiagnostic SetFov = PropertyAccess::SetProperty(
        Camera,
        PropertyId{"field_of_view"},
        ReflectedValue::MakeFloat(75.f));
    Expect(SetFov.bOk, "Set field_of_view valid");
    Expect(Camera->FieldOfViewDegrees == 75.f, "Member updated via property");

    ReflectionDiagnostic SetInvalid = PropertyAccess::SetProperty(
        Camera,
        PropertyId{"field_of_view"},
        ReflectedValue::MakeFloat(200.f));
    Expect(!SetInvalid.bOk, "Reject out-of-range field_of_view");
    Expect(Camera->FieldOfViewDegrees == 75.f, "Invalid set did not mutate");

    Object* ClassDefault = CameraClass != nullptr ? CameraClass->GetClassDefaultObject() : nullptr;
    Expect(ClassDefault != nullptr, "CDO exists");
    ReflectedValue CdoFov;
    ReflectionDiagnostic GetCdo = PropertyAccess::GetProperty(ClassDefault, PropertyId{"field_of_view"}, CdoFov);
    Expect(GetCdo.bOk && CdoFov.Float64Value == 60.0, "CDO keeps default FOV");
    ReflectionDiagnostic SetCdo = PropertyAccess::SetProperty(
        ClassDefault,
        PropertyId{"field_of_view"},
        ReflectedValue::MakeFloat(90.f));
    Expect(!SetCdo.bOk, "CDO is readonly for writes");

    if (MemorySubsystem* Memory = MemorySubsystem::Get())
    {
        if (CreatedObject != nullptr)
        {
            Memory->DestroyObject(CreatedObject);
            CreatedObject = nullptr;
            Camera = nullptr;
        }
    }

    LightSettings Settings{};
    ReflectionDiagnostic SetIntensity = PropertyAccess::SetProperty(
        &Settings,
        LightTypeId,
        PropertyId{"intensity"},
        ReflectedValue::MakeFloat(3.5f));
    Expect(SetIntensity.bOk, "Set LightSettings intensity");
    Expect(Settings.Intensity == 3.5f, "LightSettings member updated");

    if (MemorySubsystem* Memory = MemorySubsystem::Get())
    {
        GameObject* Owner = Memory->NewObject<GameObject>("ReflectionCameraOwner");
        Component* Attached = Reflection.CreateComponent(*Owner, TypeId{"engine.CameraComponent"});
        Expect(Attached != nullptr, "CreateComponent attaches");
        Expect(Owner->GetComponent<CameraComponent>() == Attached, "Component owned by GameObject");
        Memory->DestroyObject(Owner);
    }

    Reflection.Shutdown();
    static ClassRecipe DuplicateRecipe{};
    DuplicateRecipe.TypeIdString = "engine.CameraComponent";
    DuplicateRecipe.BaseTypeIdString = "engine.Component";
    DuplicateRecipe.SchemaVersion = 1;
    DuplicateRecipe.Policy = CreationPolicy::Concrete;
    DuplicateRecipe.Origin = ReflectionOrigin::Native;
    DuplicateRecipe.Bind = []() {};
    PendingRegistry::Get().AddClassRecipe(DuplicateRecipe);
    ReflectionDiagnostic DuplicateInit = Reflection.InitializeNative();
    Expect(!DuplicateInit.bOk, "Duplicate TypeId fails initialization");

    Application.Shutdown();

    if (FailureCount > 0)
    {
        std::cout << "Reflection tests failed: " << FailureCount << '\n';
        return 1;
    }

    std::cout << "Reflection tests passed\n";
    return 0;
}
