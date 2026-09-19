#include "Core/MemorySubsystem.h"
#include "Core/Transform.h"
#include "Engine.h"
#include "Gameplay/CameraComponent.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/ScriptComponent.h"
#include "Reflection/Json/ReflectionJson.h"
#include "Reflection/PropertyAccess.h"
#include "Reflection/PropertyFlags.h"
#include "Reflection/ReflectionSubsystem.h"
#include "Scripting/ScriptingSubsystem.h"

#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(SAKURA_ENABLE_PYTHON)
#include <pybind11/embed.h>
namespace py = pybind11;
#endif

namespace
{
int FailureCount = 0;

void Expect(bool Condition, const char* Message)
{
    if (!Condition)
    {
        ++FailureCount;
        std::cout << "FAIL: " << Message << std::endl;
    }
    else
    {
        std::cout << "  OK: " << Message << std::endl;
    }
}

std::filesystem::path FindSampleProjectRoot()
{
#if defined(SAKURA_SAMPLE_PROJECT_DIR)
    const std::filesystem::path Configured = std::filesystem::path(SAKURA_SAMPLE_PROJECT_DIR);
    if (std::filesystem::exists(Configured / "Scripts" / "door_controller.py"))
    {
        return std::filesystem::weakly_canonical(Configured);
    }
#endif

    const std::filesystem::path Candidates[] = {
        std::filesystem::current_path() / "Samples" / "SampleProject",
        std::filesystem::current_path() / ".." / "Samples" / "SampleProject",
        std::filesystem::current_path() / ".." / ".." / "Samples" / "SampleProject",
    };
    for (const std::filesystem::path& Candidate : Candidates)
    {
        if (std::filesystem::exists(Candidate / "Scripts" / "door_controller.py"))
        {
            return std::filesystem::weakly_canonical(Candidate);
        }
    }
    return {};
}
}

int main()
{
    const std::filesystem::path SampleProjectRoot = FindSampleProjectRoot();
    Engine Application;
    if (!SampleProjectRoot.empty())
    {
        Application.SetScriptsRoot(SampleProjectRoot / "Scripts");
    }
    Application.InitializeHeadless(SampleProjectRoot.empty() ? std::filesystem::path{} : SampleProjectRoot / "Content");

    ReflectionSubsystem& Reflection = ReflectionSubsystem::Get();
    Expect(Reflection.IsInitialized(), "Reflection initialized");

    Object* CameraObject = Reflection.CreateInstance(TypeId{"engine.CameraComponent"});
    Expect(CameraObject != nullptr, "Create CameraComponent");
    CameraComponent* Camera = dynamic_cast<CameraComponent*>(CameraObject);
    Expect(Camera != nullptr, "Camera cast");

    ReflectionDiagnostic SetFov = PropertyAccess::SetProperty(
        Camera,
        PropertyId{"field_of_view"},
        ReflectedValue::MakeFloat(72.f));
    Expect(SetFov.bOk, "Set camera FOV");

    std::string CameraJson;
    ReflectionDiagnostic Serialized = ReflectionJson::SerializeObject(Camera, CameraJson);
    Expect(Serialized.bOk, "Serialize CameraComponent JSON");

    Object* CameraRoundTrip = Reflection.CreateInstance(TypeId{"engine.CameraComponent"});
    Expect(CameraRoundTrip != nullptr, "Create Camera round-trip");
    ReflectionDiagnostic Deserialized = ReflectionJson::DeserializeInto(CameraRoundTrip, CameraJson);
    Expect(Deserialized.bOk, "Deserialize CameraComponent JSON");
    ReflectedValue RoundTripFov;
    PropertyAccess::GetProperty(CameraRoundTrip, PropertyId{"field_of_view"}, RoundTripFov);
    Expect(RoundTripFov.Float64Value == 72.0, "Camera FOV round-trip");

    ReflectionJson::RegisterMigration(
        TypeId{"test.MigratedComponent"},
        1,
        2,
        [](uint32_t, uint32_t, nlohmann::json& Properties) -> ReflectionDiagnostic
        {
            if (Properties.contains("velocity"))
            {
                Properties["speed"] = Properties["velocity"];
                Properties.erase("velocity");
            }
            return ReflectionDiagnostic::Ok();
        });

    {
        std::vector<PropertyDescriptor> Properties;
        PropertyDescriptor SpeedProperty;
        SpeedProperty.Id = PropertyId{"speed"};
        SpeedProperty.ValueTypeId = TypeId{"engine.float"};
        SpeedProperty.Attributes.Flags = PropertyFlags::Serializable | PropertyFlags::EditorEditable
            | PropertyFlags::EditorVisible | PropertyFlags::ScriptReadable | PropertyFlags::ScriptWritable;
        SpeedProperty.ReadObject = &ScriptComponent::ReadManagedProperty;
        SpeedProperty.WriteObject = &ScriptComponent::WriteManagedProperty;
        Properties.push_back(SpeedProperty);

        std::unordered_map<PropertyId, ReflectedValue, PropertyIdHash> Defaults;
        Defaults.emplace(PropertyId{"speed"}, ReflectedValue::MakeFloat(1.f));

        ReflectionDiagnostic Published = Reflection.PublishPythonClass(
            TypeId{"test.MigratedComponent"},
            2,
            TypeId{"engine.ScriptComponent"},
            TypeId{"engine.ScriptComponent"},
            std::move(Properties),
            Defaults);
        Expect(Published.bOk, "Publish migrated test class schema 2");

        Object* MigratedObject = Reflection.CreateInstance(TypeId{"test.MigratedComponent"});
        Expect(MigratedObject != nullptr, "Create migrated test instance");
        const std::string MigratedJson =
            R"({"type":"test.MigratedComponent","typeVersion":1,"properties":{"velocity":4.5}})";
        ReflectionDiagnostic Migrated = ReflectionJson::DeserializeInto(MigratedObject, MigratedJson);
        Expect(Migrated.bOk, "JSON migration 1->2");
        ReflectedValue MigratedSpeed;
        PropertyAccess::GetProperty(MigratedObject, PropertyId{"speed"}, MigratedSpeed);
        Expect(MigratedSpeed.Float64Value == 4.5, "Migrated velocity->speed value");
        if (MemorySubsystem* Memory = MemorySubsystem::Get())
        {
            if (MigratedObject != nullptr)
            {
                Memory->DestroyObject(MigratedObject);
            }
        }
    }

#if defined(SAKURA_ENABLE_PYTHON)
    Expect(Application.GetScripting().IsPythonEnabled(), "Python enabled");
    Expect(!SampleProjectRoot.empty(), "Sample project with Scripts found");

    if (!SampleProjectRoot.empty())
    {
        Class* DoorClass = Reflection.FindClass("game.DoorController");
        Expect(DoorClass != nullptr, "FindClass game.DoorController");
        if (DoorClass != nullptr)
        {
            Expect(DoorClass->GetOrigin() == ReflectionOrigin::Python, "DoorController origin Python");
            Expect(DoorClass->GetNativeBackingTypeId().Value == "engine.ScriptComponent", "Native backing ScriptComponent");
            Expect(DoorClass->FindProperty(PropertyId{"speed"}) != nullptr, "DoorController speed property");
        }

        MemorySubsystem* Memory = MemorySubsystem::Get();
        Expect(Memory != nullptr, "MemorySubsystem available");
        if (Memory != nullptr && DoorClass != nullptr)
        {
            GameObject* Owner = Memory->NewObject<GameObject>("DoorOwner");
            Owner->GetTransform().Position = Vector3(0.f, 0.f, 0.f);
            Component* Attached = Reflection.CreateComponent(*Owner, TypeId{"game.DoorController"});
            ScriptComponent* Script = dynamic_cast<ScriptComponent*>(Attached);
            Expect(Script != nullptr, "Create DoorController ScriptComponent");
            Expect(Script != nullptr && Script->GetClass() == DoorClass, "Script Class is DoorController");

            ReflectionDiagnostic SetSpeed = PropertyAccess::SetProperty(
                Script,
                PropertyId{"speed"},
                ReflectedValue::MakeFloat(3.f));
            Expect(SetSpeed.bOk, "Set DoorController speed");

            std::string DoorJson;
            Expect(ReflectionJson::SerializeObject(Script, DoorJson).bOk, "Serialize DoorController JSON");
            ScriptComponent* DoorClone = dynamic_cast<ScriptComponent*>(
                Reflection.CreateInstance(TypeId{"game.DoorController"}));
            Expect(DoorClone != nullptr, "Create DoorController clone");
            Expect(ReflectionJson::DeserializeInto(DoorClone, DoorJson).bOk, "Deserialize DoorController JSON");
            ReflectedValue SpeedValue;
            PropertyAccess::GetProperty(DoorClone, PropertyId{"speed"}, SpeedValue);
            Expect(SpeedValue.Float64Value == 3.0, "DoorController speed round-trip");
            Memory->DestroyObject(DoorClone);

            const float BeforeX = Owner->GetTransform().Position.x;
            Script->Tick(1.0f);
            const float AfterX = Owner->GetTransform().Position.x;
            Expect(AfterX > BeforeX, "DoorController Tick moves transform");

            const ObjectHandle OwnerHandle = Owner->GetObjectHandle();
            Memory->DestroyObject(Owner);

            bool bStaleSafe = false;
            try
            {
                py::gil_scoped_acquire Gil;
                py::module_ EngineModule = py::module_::import("engine");
                py::object Facade = EngineModule.attr("Object")(OwnerHandle.Id, OwnerHandle.Generation);
                Expect(Facade.attr("is_valid").cast<bool>() == false, "Stale Object.is_valid is false");
                try
                {
                    Facade.attr("get_local_transform")();
                    bStaleSafe = false;
                }
                catch (const py::error_already_set&)
                {
                    bStaleSafe = true;
                    PyErr_Clear();
                }
            }
            catch (const py::error_already_set& Error)
            {
                std::cout << "Python stale-handle check error: " << Error.what() << std::endl;
                PyErr_Clear();
            }
            Expect(bStaleSafe, "Stale Python Object handle throws safely");
        }
    }
#else
    Expect(!Application.GetScripting().IsPythonEnabled(), "Python disabled build");
    std::cout << "  SKIP: Python DoorController tests (SAKURA_ENABLE_PYTHON=OFF)\n";
#endif

    if (CameraObject != nullptr)
    {
        std::vector<const PropertyDescriptor*> Editable;
        if (Class* CameraClass = CameraObject->GetClass())
        {
            for (const PropertyDescriptor& Descriptor : CameraClass->GetProperties())
            {
                if (HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::EditorEditable))
                {
                    Editable.push_back(&Descriptor);
                }
            }
        }
        Expect(!Editable.empty(), "Inspector property list API non-empty for Camera");
        ReflectionDiagnostic Reset = PropertyAccess::SetProperty(
            CameraObject,
            PropertyId{"field_of_view"},
            ReflectedValue::MakeFloat(60.f));
        Expect(Reset.bOk, "Inspector-style reset FOV");
    }

    if (MemorySubsystem* Memory = MemorySubsystem::Get())
    {
        if (CameraObject != nullptr)
        {
            Memory->DestroyObject(CameraObject);
        }
        if (CameraRoundTrip != nullptr)
        {
            Memory->DestroyObject(CameraRoundTrip);
        }
    }

    ReflectionJson::ClearMigrations();
    Application.Shutdown();

    if (FailureCount == 0)
    {
        std::cout << "SakuraReflectionPythonTest passed\n";
        return 0;
    }

    std::cout << "SakuraReflectionPythonTest failed: " << FailureCount << " checks\n";
    return 1;
}
