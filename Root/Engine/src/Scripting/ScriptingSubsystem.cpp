#include "Scripting/ScriptingSubsystem.h"

#include "Core/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Core/Transform.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/ScriptComponent.h"
#include "Reflection/PropertyAccess.h"
#include "Reflection/ReflectionSubsystem.h"

#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#if defined(SAKURA_ENABLE_PYTHON)
#include <pybind11/embed.h>
#include <pybind11/stl.h>

namespace py = pybind11;

struct ScriptingSubsystem::ScriptInstanceRecord
{
    ScriptComponent* Component = nullptr;
    py::object Instance;
    py::object OnCreate;
    py::object OnUpdate;
    py::object OnDestroy;
    bool bDestroyInvoked = false;
};

namespace
{
std::unique_ptr<py::scoped_interpreter> g_Interpreter;
std::vector<std::string> g_OwnedAttributeStrings;
std::unordered_map<std::string, py::object> g_PythonTypesByTypeId;

void RegisterPythonType(const std::string& TypeIdString, const py::object& PythonType)
{
    g_PythonTypesByTypeId[TypeIdString] = PythonType;
}

py::object FindRegisteredPythonType(const std::string& TypeIdString)
{
    auto Found = g_PythonTypesByTypeId.find(TypeIdString);
    if (Found == g_PythonTypesByTypeId.end())
    {
        return py::none();
    }
    return Found->second;
}

void ClearRegisteredPythonTypes()
{
    g_PythonTypesByTypeId.clear();
}

const char* InternAttributeString(const std::string& Text)
{
    g_OwnedAttributeStrings.push_back(Text);
    return g_OwnedAttributeStrings.back().c_str();
}

TypeId ResolvePythonValueTypeId(const py::object& TypeObject)
{
    if (py::isinstance<py::type>(TypeObject))
    {
        const py::type AsType = py::reinterpret_borrow<py::type>(TypeObject);
        if (AsType.is(py::type::of(py::bool_(false))))
        {
            return TypeId{"engine.bool"};
        }
        if (AsType.is(py::type::of(py::int_(0))))
        {
            return TypeId{"engine.int64"};
        }
        if (AsType.is(py::type::of(py::float_(0.0))))
        {
            return TypeId{"engine.float"};
        }
        if (AsType.is(py::type::of(py::str(""))))
        {
            return TypeId{"engine.string"};
        }
    }
    if (py::isinstance<py::str>(TypeObject))
    {
        return TypeId{TypeObject.cast<std::string>()};
    }
    return {};
}

ReflectedValue MakeDefaultFromPython(const TypeId& ValueTypeId, const py::object& DefaultObject)
{
    if (DefaultObject.is_none())
    {
        if (ValueTypeId.Value == "engine.bool")
        {
            return ReflectedValue::MakeBool(false);
        }
        if (ValueTypeId.Value == "engine.int64")
        {
            return ReflectedValue::MakeInt64(0);
        }
        if (ValueTypeId.Value == "engine.float" || ValueTypeId.Value == "engine.double")
        {
            return ReflectedValue::MakeFloat(0.f);
        }
        if (ValueTypeId.Value == "engine.string")
        {
            return ReflectedValue::MakeString("");
        }
        return ReflectedValue::MakeEmpty();
    }

    if (ValueTypeId.Value == "engine.bool")
    {
        return ReflectedValue::MakeBool(DefaultObject.cast<bool>());
    }
    if (ValueTypeId.Value == "engine.int64")
    {
        return ReflectedValue::MakeInt64(DefaultObject.cast<int64_t>());
    }
    if (ValueTypeId.Value == "engine.float" || ValueTypeId.Value == "engine.double")
    {
        return ReflectedValue::MakeFloat(static_cast<float>(DefaultObject.cast<double>()));
    }
    if (ValueTypeId.Value == "engine.string")
    {
        return ReflectedValue::MakeString(DefaultObject.cast<std::string>());
    }
    if (ValueTypeId.Value == "engine.Vector3")
    {
        const float X = DefaultObject.attr("x").cast<float>();
        const float Y = DefaultObject.attr("y").cast<float>();
        const float Z = DefaultObject.attr("z").cast<float>();
        ReflectedValue Value = ReflectedValue::MakeEmpty();
        Value.ValueKind = ReflectedValue::Kind::Bytes;
        Value.BytesValue.resize(sizeof(float) * 3);
        float Values[3] = {X, Y, Z};
        std::memcpy(Value.BytesValue.data(), Values, sizeof(Values));
        return Value;
    }
    return ReflectedValue::MakeEmpty();
}

py::object ReflectedToPython(const ReflectedValue& Value)
{
    switch (Value.ValueKind)
    {
    case ReflectedValue::Kind::Bool:
        return py::bool_(Value.BoolValue);
    case ReflectedValue::Kind::Int64:
        return py::int_(Value.Int64Value);
    case ReflectedValue::Kind::Float64:
        return py::float_(Value.Float64Value);
    case ReflectedValue::Kind::String:
        return py::str(Value.StringValue);
    case ReflectedValue::Kind::Bytes:
        if (Value.BytesValue.size() == sizeof(float) * 3)
        {
            float Values[3] = {};
            std::memcpy(Values, Value.BytesValue.data(), sizeof(Values));
            py::module_ EngineModule = py::module_::import("engine");
            return EngineModule.attr("Vector3")(Values[0], Values[1], Values[2]);
        }
        break;
    default:
        break;
    }
    return py::none();
}

ReflectedValue PythonToReflected(const py::object& Value, const TypeId& ValueTypeId)
{
    if (ValueTypeId.Value == "engine.bool")
    {
        return ReflectedValue::MakeBool(Value.cast<bool>());
    }
    if (ValueTypeId.Value == "engine.int64")
    {
        return ReflectedValue::MakeInt64(Value.cast<int64_t>());
    }
    if (ValueTypeId.Value == "engine.float" || ValueTypeId.Value == "engine.double")
    {
        return ReflectedValue::MakeFloat(static_cast<float>(Value.cast<double>()));
    }
    if (ValueTypeId.Value == "engine.string")
    {
        return ReflectedValue::MakeString(Value.cast<std::string>());
    }
    if (ValueTypeId.Value == "engine.Vector3")
    {
        const float X = Value.attr("x").cast<float>();
        const float Y = Value.attr("y").cast<float>();
        const float Z = Value.attr("z").cast<float>();
        ReflectedValue Out = ReflectedValue::MakeEmpty();
        Out.ValueKind = ReflectedValue::Kind::Bytes;
        Out.BytesValue.resize(sizeof(float) * 3);
        float Values[3] = {X, Y, Z};
        std::memcpy(Out.BytesValue.data(), Values, sizeof(Values));
        return Out;
    }
    return ReflectedValue::MakeEmpty();
}

Object* ResolveObjectOrThrow(uint64_t Id, uint32_t Generation)
{
    MemorySubsystem* Memory = MemorySubsystem::Get();
    if (Memory == nullptr)
    {
        throw py::value_error("MemorySubsystem is not available");
    }
    Object* Found = Memory->ResolveHandle(ObjectHandle{Id, Generation});
    if (Found == nullptr)
    {
        throw py::value_error("Object handle is stale or destroyed");
    }
    return Found;
}

ScriptComponent* ResolveScriptCarrier(uint64_t Id, uint32_t Generation)
{
    Object* Found = ResolveObjectOrThrow(Id, Generation);
    ScriptComponent* Script = dynamic_cast<ScriptComponent*>(Found);
    if (Script == nullptr)
    {
        throw py::type_error("Handle does not refer to ScriptComponent");
    }
    return Script;
}

ReflectionDiagnostic PublishRegisteredPythonClass(
    const py::object& PythonType,
    const std::string& TypeIdString,
    uint32_t SchemaVersion)
{
    if (PythonType.attr("__dict__").contains("__init__"))
    {
        return ReflectionDiagnostic::Fail(
            "Registered ScriptComponent subclasses must not define __init__",
            TypeId{TypeIdString});
    }

    PythonClassPublishRequest Request;
    Request.Id = TypeId{TypeIdString};
    Request.SchemaVersion = SchemaVersion;
    Request.BaseTypeId = TypeId{"engine.ScriptComponent"};

    py::dict ClassDict = PythonType.attr("__dict__");
    for (auto Item : ClassDict)
    {
        py::object Value = py::reinterpret_borrow<py::object>(Item.second);
        if (!py::hasattr(Value, "_sakura_field_marker"))
        {
            continue;
        }

        PythonFieldDeclaration Field;
        std::string PropertyIdValue = Value.attr("property_id").cast<std::string>();
        if (PropertyIdValue.empty())
        {
            PropertyIdValue = py::str(Item.first).cast<std::string>();
        }
        Field.Id = PropertyId{PropertyIdValue};
        Field.ValueTypeId = ResolvePythonValueTypeId(Value.attr("value_type"));
        if (!Field.ValueTypeId.IsValid())
        {
            return ReflectionDiagnostic::Fail("Unsupported Python field type", Request.Id, Field.Id);
        }

        PropertyAttributes Attributes{};
        if (Value.attr("serializable").cast<bool>())
        {
            Attributes.Flags = Attributes.Flags | PropertyFlags::Serializable;
        }
        if (Value.attr("editable").cast<bool>())
        {
            Attributes.Flags = Attributes.Flags | PropertyFlags::EditorEditable | PropertyFlags::EditorVisible
                | PropertyFlags::ScriptReadable | PropertyFlags::ScriptWritable;
        }
        else
        {
            Attributes.Flags = Attributes.Flags | PropertyFlags::EditorVisible | PropertyFlags::ScriptReadable;
        }

        if (!Value.attr("display_name").is_none())
        {
            Attributes.DisplayName = InternAttributeString(Value.attr("display_name").cast<std::string>());
        }
        if (!Value.attr("ui_range").is_none())
        {
            py::tuple Range = Value.attr("ui_range").cast<py::tuple>();
            Attributes.bHasRange = true;
            Attributes.RangeMinimum = Range[0].cast<double>();
            Attributes.RangeMaximum = Range[1].cast<double>();
        }

        Field.Attributes = Attributes;
        Field.DefaultValue = MakeDefaultFromPython(Field.ValueTypeId, Value.attr("default"));
        Request.Fields.push_back(std::move(Field));
    }

    ReflectionDiagnostic Published = ScriptingSubsystem::Get().PublishPythonClass(Request);
    if (Published.bOk)
    {
        RegisterPythonType(TypeIdString, PythonType);
    }
    return Published;
}
}

void RegisterEmbeddedEnginePythonModule()
{
}

PYBIND11_EMBEDDED_MODULE(engine, Module)
{
    Module.doc() = "Sacura Novel Engine scripting bindings";

    Module.def(
        "resolve_object_valid",
        [](uint64_t Id, uint32_t Generation)
        {
            MemorySubsystem* Memory = MemorySubsystem::Get();
            return Memory != nullptr && Memory->ResolveHandle(ObjectHandle{Id, Generation}) != nullptr;
        });

    Module.def(
        "get_local_transform",
        [](uint64_t Id, uint32_t Generation)
        {
            Object* Found = ResolveObjectOrThrow(Id, Generation);
            GameObject* Owner = dynamic_cast<GameObject*>(Found);
            if (Owner == nullptr)
            {
                throw py::type_error("Object is not a GameObject");
            }
            const Transform& Source = Owner->GetTransform();
            return py::make_tuple(
                Source.Position.x,
                Source.Position.y,
                Source.Position.z,
                Source.Scale.x,
                Source.Scale.y,
                Source.Scale.z);
        });

    Module.def(
        "set_local_transform",
        [](uint64_t Id, uint32_t Generation, float PosX, float PosY, float PosZ, float ScaleX, float ScaleY, float ScaleZ)
        {
            Object* Found = ResolveObjectOrThrow(Id, Generation);
            GameObject* Owner = dynamic_cast<GameObject*>(Found);
            if (Owner == nullptr)
            {
                throw py::type_error("Object is not a GameObject");
            }
            Transform Updated = Owner->GetTransform();
            Updated.Position = Vector3(PosX, PosY, PosZ);
            Updated.Scale = Vector3(ScaleX, ScaleY, ScaleZ);
            Owner->SetTransform(Updated);
        });

    Module.def(
        "get_owner_handle",
        [](uint64_t CarrierId, uint32_t CarrierGeneration)
        {
            ScriptComponent* Script = ResolveScriptCarrier(CarrierId, CarrierGeneration);
            GameObject* Owner = Script->GetGameObject();
            if (Owner == nullptr)
            {
                return py::make_tuple(static_cast<uint64_t>(0), static_cast<uint32_t>(0));
            }
            return py::make_tuple(Owner->GetID(), Owner->GetGeneration());
        });

    Module.def(
        "get_managed_property",
        [](uint64_t CarrierId, uint32_t CarrierGeneration, const std::string& PropertyName)
        {
            ScriptComponent* Script = ResolveScriptCarrier(CarrierId, CarrierGeneration);
            ReflectedValue Value;
            if (!Script->TryGetManagedProperty(PropertyId{PropertyName}, Value))
            {
                return py::object(py::none());
            }
            return ReflectedToPython(Value);
        });

    Module.def(
        "set_managed_property",
        [](uint64_t CarrierId, uint32_t CarrierGeneration, const std::string& PropertyName, const py::object& Value)
        {
            ScriptComponent* Script = ResolveScriptCarrier(CarrierId, CarrierGeneration);
            Class* ObjectClass = Script->GetClass();
            if (ObjectClass == nullptr)
            {
                throw py::value_error("ScriptComponent has no Class");
            }
            const PropertyDescriptor* Descriptor = ObjectClass->FindProperty(PropertyId{PropertyName});
            if (Descriptor == nullptr)
            {
                throw py::key_error(PropertyName);
            }
            ReflectedValue Converted = PythonToReflected(Value, Descriptor->ValueTypeId);
            ReflectionDiagnostic Result = PropertyAccess::SetProperty(
                Script,
                *Descriptor,
                Converted,
                PropertyAccessContext::Script);
            if (!Result.bOk)
            {
                throw py::value_error(Result.Message);
            }
        });

    Module.def(
        "_publish_registered_class",
        [](const py::object& PythonType, const std::string& TypeIdString, uint32_t SchemaVersion)
        {
            ReflectionDiagnostic Published = PublishRegisteredPythonClass(PythonType, TypeIdString, SchemaVersion);
            if (!Published.bOk)
            {
                throw py::value_error(Published.Message);
            }
        });

    py::exec(R"PY(
class Vector3:
    def __init__(self, x=0.0, y=0.0, z=0.0):
        self.x = float(x)
        self.y = float(y)
        self.z = float(z)

class Transform:
    def __init__(self, position=None, scale=None):
        self.position = position if position is not None else Vector3()
        self.scale = scale if scale is not None else Vector3(1.0, 1.0, 1.0)

class Object:
    def __init__(self, object_id, generation):
        self._id = int(object_id)
        self._generation = int(generation)

    @property
    def is_valid(self):
        return bool(resolve_object_valid(self._id, self._generation))

    def get_local_transform(self):
        values = get_local_transform(self._id, self._generation)
        result = Transform()
        result.position = Vector3(values[0], values[1], values[2])
        result.scale = Vector3(values[3], values[4], values[5])
        return result

    def set_local_transform(self, transform):
        set_local_transform(
            self._id,
            self._generation,
            float(transform.position.x),
            float(transform.position.y),
            float(transform.position.z),
            float(transform.scale.x),
            float(transform.scale.y),
            float(transform.scale.z))

class _FieldDescriptor:
    def __init__(self, value_type, property_id, default, serializable, editable, display_name, ui_range):
        self.value_type = value_type
        self.property_id = property_id or ""
        self.default = default
        self.serializable = bool(serializable)
        self.editable = bool(editable)
        self.display_name = display_name
        self.ui_range = ui_range
        self._sakura_field_marker = True

    def __set_name__(self, owner, name):
        if not self.property_id:
            self.property_id = name

    def __get__(self, obj, objtype=None):
        if obj is None:
            return self
        return obj._get_managed(self.property_id)

    def __set__(self, obj, value):
        obj._set_managed(self.property_id, value)

def field(type, property_id="", default=None, serializable=True, editable=True, display_name=None, ui_range=None):
    return _FieldDescriptor(type, property_id, default, serializable, editable, display_name, ui_range)

class ScriptComponent:
    def _bind_carrier(self, object_id, generation):
        self._carrier_id = int(object_id)
        self._carrier_generation = int(generation)

    @property
    def owner(self):
        handles = get_owner_handle(self._carrier_id, self._carrier_generation)
        if handles[0] == 0:
            return None
        return Object(handles[0], handles[1])

    def _get_managed(self, property_id):
        return get_managed_property(self._carrier_id, self._carrier_generation, property_id)

    def _set_managed(self, property_id, value):
        set_managed_property(self._carrier_id, self._carrier_generation, property_id, value)

def register_class(type_id, schema_version=1):
    def decorator(python_type):
        _publish_registered_class(python_type, type_id, int(schema_version))
        python_type._sakura_type_id = type_id
        python_type._sakura_schema_version = int(schema_version)
        return python_type
    return decorator
)PY",
        Module.attr("__dict__"));
}
#endif

ScriptingSubsystem& ScriptingSubsystem::Get()
{
    static ScriptingSubsystem Instance;
    return Instance;
}

ScriptingSubsystem::ScriptingSubsystem() = default;

ScriptingSubsystem::~ScriptingSubsystem()
{
    if (!bInitialized)
    {
        return;
    }

#if defined(SAKURA_ENABLE_PYTHON)
    Instances.clear();
    g_OwnedAttributeStrings.clear();
    ClearRegisteredPythonTypes();
    g_Interpreter.reset();
#endif
    bInitialized = false;
}

bool ScriptingSubsystem::IsPythonEnabled() const
{
#if defined(SAKURA_ENABLE_PYTHON)
    return true;
#else
    return false;
#endif
}

void ScriptingSubsystem::SetScriptsRoot(const std::filesystem::path& InScriptsRoot)
{
    ScriptsRoot = InScriptsRoot;
}

ReflectionDiagnostic ScriptingSubsystem::Initialize()
{
    AssertGameThread();
    if (bInitialized)
    {
        return ReflectionDiagnostic::Ok();
    }

#if defined(SAKURA_ENABLE_PYTHON)
    try
    {
        if (g_Interpreter == nullptr)
        {
            g_Interpreter = std::make_unique<py::scoped_interpreter>();
        }
        py::module_::import("engine");
    }
    catch (const py::error_already_set& Error)
    {
        return ReflectionDiagnostic::Fail(std::string("Python initialize failed: ") + Error.what());
    }
#endif

    bInitialized = true;
    PrintString("Scripting: initialized");
    return ReflectionDiagnostic::Ok();
}

void ScriptingSubsystem::DestroyAllScriptInstances()
{
    AssertGameThread();
#if defined(SAKURA_ENABLE_PYTHON)
    if (g_Interpreter == nullptr)
    {
        Instances.clear();
        return;
    }

    py::gil_scoped_acquire Gil;
    for (auto& Pair : Instances)
    {
        ScriptInstanceRecord& Record = *Pair.second;
        if (Record.Component != nullptr && !Record.bDestroyInvoked)
        {
            if (!Record.OnDestroy.is_none())
            {
                try
                {
                    Record.OnDestroy();
                }
                catch (const py::error_already_set& Error)
                {
                    PrintString(std::string("Script on_destroy failed: ") + Error.what());
                    PyErr_Clear();
                }
            }
            Record.bDestroyInvoked = true;
            Record.Component->MarkDestroyCalled();
            Record.Component->SetScriptInstanceHandle({});
        }
        Record.OnCreate = py::none();
        Record.OnUpdate = py::none();
        Record.OnDestroy = py::none();
        Record.Instance = py::none();
        Record.Component = nullptr;
    }
    Instances.clear();
#endif
}

void ScriptingSubsystem::Shutdown()
{
    AssertGameThread();
    if (!bInitialized)
    {
        return;
    }

    DestroyAllScriptInstances();

#if defined(SAKURA_ENABLE_PYTHON)
    g_OwnedAttributeStrings.clear();
    ClearRegisteredPythonTypes();
    g_Interpreter.reset();
#endif

    bInitialized = false;
    PrintString("Scripting: shutdown complete");
}

ReflectionDiagnostic ScriptingSubsystem::PublishPythonClass(const PythonClassPublishRequest& Request)
{
    std::vector<PropertyDescriptor> Properties;
    std::unordered_map<PropertyId, ReflectedValue, PropertyIdHash> Defaults;
    Properties.reserve(Request.Fields.size());

    for (const PythonFieldDeclaration& Field : Request.Fields)
    {
        PropertyDescriptor Descriptor;
        Descriptor.DeclaringTypeId = Request.Id;
        Descriptor.Id = Field.Id;
        Descriptor.ValueTypeId = Field.ValueTypeId;
        Descriptor.Attributes = Field.Attributes;
        Descriptor.bReadOnly = false;
        Descriptor.bIsField = true;
        Descriptor.ReadObject = &ScriptComponent::ReadManagedProperty;
        Descriptor.WriteObject = &ScriptComponent::WriteManagedProperty;
        Properties.push_back(Descriptor);
        Defaults.emplace(Field.Id, Field.DefaultValue);
    }

    return ReflectionSubsystem::Get().PublishPythonClass(
        Request.Id,
        Request.SchemaVersion,
        Request.BaseTypeId,
        TypeId{"engine.ScriptComponent"},
        std::move(Properties),
        Defaults);
}

ReflectionDiagnostic ScriptingSubsystem::ImportModule(const std::string& ModuleName)
{
    AssertGameThread();
#if !defined(SAKURA_ENABLE_PYTHON)
    return ReflectionDiagnostic::Fail("Python is disabled");
#else
    if (!bInitialized)
    {
        return ReflectionDiagnostic::Fail("ScriptingSubsystem not initialized");
    }

    try
    {
        py::gil_scoped_acquire Gil;
        if (!ScriptsRoot.empty())
        {
            py::module_ Sys = py::module_::import("sys");
            py::list Path = Sys.attr("path");
            const std::string ScriptsRootString = ScriptsRoot.generic_string();
            bool bFound = false;
            for (auto Entry : Path)
            {
                if (py::str(Entry).cast<std::string>() == ScriptsRootString)
                {
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
            {
                Path.insert(0, ScriptsRootString);
            }
        }
        py::module_::import(ModuleName.c_str());
        PrintString(std::string("Scripting: imported module ") + ModuleName);
        return ReflectionDiagnostic::Ok();
    }
    catch (const py::error_already_set& Error)
    {
        return ReflectionDiagnostic::Fail(std::string("Failed to import module: ") + Error.what());
    }
#endif
}

ReflectionDiagnostic ScriptingSubsystem::ImportConfiguredModules()
{
    AssertGameThread();
    if (ScriptsRoot.empty())
    {
        return ReflectionDiagnostic::Ok();
    }

    namespace fs = std::filesystem;
    std::error_code ErrorCode;
    if (!fs::exists(ScriptsRoot, ErrorCode))
    {
        return ReflectionDiagnostic::Ok();
    }

    for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(ScriptsRoot, ErrorCode))
    {
        if (!Entry.is_regular_file(ErrorCode))
        {
            continue;
        }
        if (Entry.path().extension() != ".py")
        {
            continue;
        }

        const std::string Stem = Entry.path().stem().string();
        if (Stem == "__init__")
        {
            continue;
        }

        fs::path Relative = fs::relative(Entry.path(), ScriptsRoot, ErrorCode);
        if (ErrorCode)
        {
            continue;
        }
        Relative.replace_extension();
        std::string ModuleName = Relative.generic_string();
        for (char& Character : ModuleName)
        {
            if (Character == '/')
            {
                Character = '.';
            }
        }

        ReflectionDiagnostic Imported = ImportModule(ModuleName);
        if (!Imported.bOk)
        {
            return Imported;
        }
    }
    return ReflectionDiagnostic::Ok();
}

void ScriptingSubsystem::BindScriptComponent(ScriptComponent& Component)
{
    AssertGameThread();
#if !defined(SAKURA_ENABLE_PYTHON)
    return;
#else
    if (!bInitialized || g_Interpreter == nullptr)
    {
        return;
    }

    Class* ObjectClass = Component.GetClass();
    if (ObjectClass == nullptr || ObjectClass->GetOrigin() != ReflectionOrigin::Python)
    {
        return;
    }

    try
    {
        py::gil_scoped_acquire Gil;
        py::object PythonType = FindRegisteredPythonType(ObjectClass->GetTypeId().Value);
        if (PythonType.is_none())
        {
            PrintString(std::string("Scripting: Python type not found for ") + ObjectClass->GetTypeId().Value);
            Component.MarkScriptFailed();
            return;
        }

        py::object Instance = PythonType.attr("__new__")(PythonType);
        Instance.attr("_bind_carrier")(Component.GetID(), Component.GetGeneration());

        ScriptInstanceRecord Record;
        Record.Component = &Component;
        Record.Instance = Instance;
        Record.OnCreate = py::getattr(PythonType, "on_create", py::none());
        Record.OnUpdate = py::getattr(PythonType, "on_update", py::none());
        Record.OnDestroy = py::getattr(PythonType, "on_destroy", py::none());
        if (!Record.OnCreate.is_none())
        {
            Record.OnCreate = Record.OnCreate.attr("__get__")(Instance, PythonType);
        }
        if (!Record.OnUpdate.is_none())
        {
            Record.OnUpdate = Record.OnUpdate.attr("__get__")(Instance, PythonType);
        }
        if (!Record.OnDestroy.is_none())
        {
            Record.OnDestroy = Record.OnDestroy.attr("__get__")(Instance, PythonType);
        }

        const uint64_t InstanceId = NextScriptInstanceId++;
        auto Stored = std::make_unique<ScriptInstanceRecord>(std::move(Record));
        ScriptInstanceRecord* StoredRaw = Stored.get();
        Instances.emplace(InstanceId, std::move(Stored));
        Component.SetScriptInstanceHandle(ScriptInstanceHandle{InstanceId});

        if (StoredRaw != nullptr && !StoredRaw->OnCreate.is_none())
        {
            StoredRaw->OnCreate();
        }
    }
    catch (const py::error_already_set& Error)
    {
        PrintString(std::string("Script on_create failed: ") + Error.what());
        PyErr_Clear();
        Component.MarkScriptFailed();
    }
#endif
}

void ScriptingSubsystem::UnbindScriptComponent(ScriptComponent& Component)
{
    AssertGameThread();
#if !defined(SAKURA_ENABLE_PYTHON)
    return;
#else
    const ScriptInstanceHandle Handle = Component.GetScriptInstanceHandle();
    if (!Handle.IsValid())
    {
        return;
    }

    auto Found = Instances.find(Handle.Value);
    if (Found == Instances.end())
    {
        Component.SetScriptInstanceHandle({});
        return;
    }

    ScriptInstanceRecord& Record = *Found->second;
    if (!Record.bDestroyInvoked && !Component.HasCalledDestroy())
    {
        if (g_Interpreter != nullptr)
        {
            py::gil_scoped_acquire Gil;
            if (!Record.OnDestroy.is_none())
            {
                try
                {
                    Record.OnDestroy();
                }
                catch (const py::error_already_set& Error)
                {
                    PrintString(std::string("Script on_destroy failed: ") + Error.what());
                    PyErr_Clear();
                }
            }
            Record.OnCreate = py::none();
            Record.OnUpdate = py::none();
            Record.OnDestroy = py::none();
            Record.Instance = py::none();
        }
        Record.bDestroyInvoked = true;
        Component.MarkDestroyCalled();
    }

    Record.Component = nullptr;
    Instances.erase(Found);
    Component.SetScriptInstanceHandle({});
#endif
}

void ScriptingSubsystem::TickScriptComponent(ScriptComponent& Component, float DeltaTime)
{
    AssertGameThread();
#if !defined(SAKURA_ENABLE_PYTHON)
    return;
#else
    if (Component.IsScriptFailed() || g_Interpreter == nullptr)
    {
        return;
    }

    const ScriptInstanceHandle Handle = Component.GetScriptInstanceHandle();
    if (!Handle.IsValid())
    {
        return;
    }

    auto Found = Instances.find(Handle.Value);
    if (Found == Instances.end())
    {
        return;
    }

    ScriptInstanceRecord& Record = *Found->second;
    if (Record.OnUpdate.is_none())
    {
        return;
    }

    try
    {
        py::gil_scoped_acquire Gil;
        Record.OnUpdate(DeltaTime);
    }
    catch (const py::error_already_set& Error)
    {
        PrintString(std::string("ScriptFailed: on_update exception: ") + Error.what());
        PyErr_Clear();
        Component.MarkScriptFailed();
    }
#endif
}
