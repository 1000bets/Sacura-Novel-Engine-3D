#include "Scripting/ScriptingSubsystem.h"
#include "Engine.h"
#include "Materials/MaterialDocumentIO.h"
#include "World/Components/MeshRendererComponent.h"
#include "World/Components/CameraComponent.h"

#include "Core/Object/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Core/Transform.h"
#include "World/GameObject.h"
#include "World/Components/ScriptComponent.h"
#include "Reflection/PropertyAccess.h"
#include "Reflection/ReflectionValueCodec.h"
#include "Reflection/Json/ReflectionJson.h"
#include "Reflection/ReflectionSubsystem.h"

#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "Scripting/PythonRuntimeInternal.h"

#if defined(SAKURA_ENABLE_PYTHON)
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include "UI/MaterialWidget.h"
#include <QApplication>



std::unordered_map<std::string, py::object> PythonTypesByTypeId;

void RegisterPythonType(const std::string& TypeIdString, const py::object& PythonType)
{
    PythonTypesByTypeId[TypeIdString] = PythonType;
}

py::object FindRegisteredPythonType(const std::string& TypeIdString)
{
    auto Found = PythonTypesByTypeId.find(TypeIdString);
    if (Found == PythonTypesByTypeId.end())
    {
        return py::none();
    }
    return Found->second;
}

void ClearRegisteredPythonTypes()
{
    PythonTypesByTypeId.clear();
}

const char* InternAttributeString(const std::string& Text)
{
    OwnedAttributeStrings.push_back(Text);
    return OwnedAttributeStrings.back().c_str();
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
        const TypeId Candidate{TypeObject.cast<std::string>()};
        if (ReflectionSubsystem::Get().FindType(Candidate) != nullptr)
        {
            return Candidate;
        }
        const std::string Prefix = "engine.ClassRef<";
        if (Candidate.Value.find(Prefix) == 0 && Candidate.Value.back() == '>')
        {
            const TypeId BaseType{Candidate.Value.substr(Prefix.size(), Candidate.Value.size() - Prefix.size() - 1)};
            if (ReflectionSubsystem::Get().FindClass(BaseType) != nullptr)
            {
                return RegisterReflectionClassType(BaseType);
            }
        }
        return {};
    }
    return {};
}

ReflectedValue PythonToReflected(const py::object& Value, const TypeId& ValueTypeId);

ReflectedValue MakeDefaultFromPython(const TypeId& ValueTypeId, const py::object& DefaultObject)
{
    if (!DefaultObject.is_none())
    {
        return PythonToReflected(DefaultObject, ValueTypeId);
    }
    ReflectedValue Result;
    ReflectionDiagnostic Converted = MakeDefaultReflectionValue(ValueTypeId, Result);
    if (!Converted.bOk)
    {
        throw py::value_error(Converted.Message);
    }
    return Result;
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
    case ReflectedValue::Kind::TypeId:
        return py::str(Value.TypeIdValue.Value);
    case ReflectedValue::Kind::ObjectReference:
        if (Value.ObjectValue.IsValid())
        {
            return py::module_::import("engine").attr("Object")(Value.ObjectValue.Id, Value.ObjectValue.Generation);
        }
        return py::none();
    case ReflectedValue::Kind::AssetReference:
    {
        py::dict Result;
        Result["assetId"] = Value.AssetValue.Asset.ToString();
        Result["subAssetId"] = py::none();
        if (Value.AssetValue.SubAsset.has_value())
        {
            Result["subAssetId"] = py::str(Value.AssetValue.SubAsset->ToString());
        }
        return Result;
    }
    case ReflectedValue::Kind::Struct:
    {
        py::dict Result;
        for (size_t FieldIndex = 0; FieldIndex < Value.Values.size(); ++FieldIndex)
        {
            Result[py::str(Value.FieldNames[FieldIndex])] = ReflectedToPython(Value.Values[FieldIndex]);
        }
        return Result;
    }
    case ReflectedValue::Kind::Array:
    {
        py::list Result;
        for (const ReflectedValue& Element : Value.Values)
        {
            Result.append(ReflectedToPython(Element));
        }
        return Result;
    }
    case ReflectedValue::Kind::Enum:
        for (const auto& Entry : ReflectionSubsystem::Get().FindEnum(Value.TypeIdValue)->Values)
        {
            if (Entry.Value == Value.Int64Value)
            {
                return py::str(Entry.Name);
            }
        }
        throw py::value_error("Unknown enum value");
    case ReflectedValue::Kind::Bytes:
    {
        const size_t ChannelCount = Value.BytesValue.size() / sizeof(float);
        py::tuple Result(ChannelCount);
        for (size_t ChannelIndex = 0; ChannelIndex < ChannelCount; ++ChannelIndex)
        {
            float Channel = 0.f;
            std::memcpy(&Channel, Value.BytesValue.data() + ChannelIndex * sizeof(float), sizeof(float));
            Result[ChannelIndex] = Channel;
        }
        if (ChannelCount == 3 && Value.TypeIdValue.Value != "engine.EulerAngles")
        {
            return py::module_::import("engine").attr("Vector3")(Result[0], Result[1], Result[2]);
        }
        return Result;
    }
    default:
        return py::none();
    }
}

ReflectedValue PythonToReflected(const py::object& Value, const TypeId& ValueTypeId)
{
    const TypeDescriptor* Type = ReflectionSubsystem::Get().FindType(ValueTypeId);
    if (Type != nullptr && Type->Kind == TypeKind::Struct)
    {
        ReflectedValue Result;
        ReflectionDiagnostic Defaulted = MakeDefaultReflectionValue(ValueTypeId, Result);
        if (!Defaulted.bOk)
        {
            throw py::value_error(Defaulted.Message);
        }
        const py::dict Fields = Value.cast<py::dict>();
        for (auto Entry : Fields)
        {
            const std::string Name = py::str(Entry.first);
            const PropertyDescriptor* Field = ReflectionSubsystem::Get().FindStructProperty(ValueTypeId, PropertyId{Name});
            if (Field == nullptr)
            {
                throw py::key_error(Name);
            }
            for (size_t FieldIndex = 0; FieldIndex < Result.FieldNames.size(); ++FieldIndex)
            {
                if (Result.FieldNames[FieldIndex] == Name)
                {
                    Result.Values[FieldIndex] = PythonToReflected(py::reinterpret_borrow<py::object>(Entry.second), Field->ValueTypeId);
                }
            }
        }
        return Result;
    }
    if (Type != nullptr && Type->Kind == TypeKind::Array)
    {
        ReflectedValue Result;
        Result.ValueKind = ReflectedValue::Kind::Array;
        Result.TypeIdValue = Type->ElementTypeId;
        for (auto Element : Value.cast<py::sequence>())
        {
            Result.Values.push_back(PythonToReflected(py::reinterpret_borrow<py::object>(Element), Type->ElementTypeId));
        }
        return Result;
    }
    if (ValueTypeId.Value == "engine.ObjectHandle")
    {
        ObjectHandle Identity;
        if (!Value.is_none())
        {
            Identity = {Value.attr("_id").cast<uint64_t>(), Value.attr("_generation").cast<uint32_t>()};
        }
        ReflectedValue Result;
        ReadReflectionObject(Identity, Result);
        if (Identity.IsValid() && !Result.ObjectValue.IsValid())
        {
            throw py::value_error("Object reference is stale");
        }
        return Result;
    }
    if (ValueTypeId.Value == "engine.Vector3" || ValueTypeId.Value == "engine.EulerAngles" || ValueTypeId.Value == "engine.Quaternion")
    {
        size_t ChannelCount = 3;
        if (ValueTypeId.Value == "engine.Quaternion")
        {
            ChannelCount = 4;
        }
        ReflectedValue Result;
        Result.ValueKind = ReflectedValue::Kind::Bytes;
        Result.TypeIdValue = ValueTypeId;
        Result.BytesValue.resize(ChannelCount * sizeof(float));
        const char* Names[] = {"x", "y", "z", "w"};
        for (size_t ChannelIndex = 0; ChannelIndex < ChannelCount; ++ChannelIndex)
        {
            float Channel = 0.f;
            if (py::isinstance<py::sequence>(Value))
            {
                Channel = Value.cast<py::sequence>()[ChannelIndex].cast<float>();
            }
            else
            {
                Channel = Value.attr(Names[ChannelIndex]).cast<float>();
            }
            std::memcpy(Result.BytesValue.data() + ChannelIndex * sizeof(float), &Channel, sizeof(float));
        }
        return Result;
    }
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
        return ReflectedValue::MakeFloat64(Value.cast<double>());
    }
    if (ValueTypeId.Value == "engine.string")
    {
        return ReflectedValue::MakeString(Value.cast<std::string>());
    }
    const std::string Json = py::module_::import("json").attr("dumps")(Value).cast<std::string>();
    ReflectedValue Result;
    ReflectionDiagnostic Converted = ReflectionJson::JsonToReflectedValue(ValueTypeId, nlohmann::json::parse(Json), Result);
    if (!Converted.bOk)
    {
        throw py::value_error(Converted.Message);
    }
    const std::string ClassPrefix = "engine.ClassRef<";
    if (ValueTypeId.Value.find(ClassPrefix) == 0)
    {
        ReflectionDiagnostic Validated = ValidateReflectionClass(Result.TypeIdValue,
            TypeId{ValueTypeId.Value.substr(ClassPrefix.size(), ValueTypeId.Value.size() - ClassPrefix.size() - 1)});
        if (!Validated.bOk)
        {
            throw py::value_error(Validated.Message);
        }
    }
    return Result;
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

void RegisterEmbeddedEnginePythonModule()
{
}

PYBIND11_EMBEDDED_MODULE(engine, Module)
{
    Module.doc() = "Sacura Novel Engine scripting bindings";

    py::class_<DynamicMaterialInstance, std::shared_ptr<DynamicMaterialInstance>>(Module, "DynamicMaterialInstance")
        .def("get_parameter", [](const DynamicMaterialInstance& Material, const std::string& Identifier)
        {
            MaterialParameterValue Value;
            if (!Material.GetParameter(Identifier, Value))
            {
                throw py::key_error(Identifier);
            }
            return py::module_::import("json").attr("loads")(MaterialDocumentIO::WriteValue(Value).dump());
        })
        .def("set_parameter", [](DynamicMaterialInstance& Material, const std::string& Identifier, const py::object& Value)
        {
            MaterialParameterValue Previous;
            if (!Material.GetParameter(Identifier, Previous))
            {
                throw py::key_error(Identifier);
            }
            try
            {
                const auto Json = nlohmann::json::parse(py::module_::import("json").attr("dumps")(Value).cast<std::string>());
                const auto Result = Material.SetParameter(Identifier, MaterialDocumentIO::ReadValue(Previous.Type, Json));
                if (Result.HasError())
                {
                    throw py::value_error(Result.Message);
                }
            }
            catch (const nlohmann::json::exception& Exception)
            {
                throw py::value_error(Exception.what());
            }
        })
        .def("reset_parameter", &DynamicMaterialInstance::ResetParameter);
    py::class_<MaterialWidget>(Module, "MaterialWidget")
        .def(py::init([]()
        {
            AssertGameThread();
            Engine* Runtime = ScriptingSubsystem::Get().GetMaterialEngine();
            if (Runtime == nullptr || qobject_cast<QApplication*>(QCoreApplication::instance()) == nullptr)
            {
                throw py::value_error("MaterialWidget requires an active UI runtime");
            }
            return std::make_unique<MaterialWidget>(*Runtime);
        }))
        .def("set_material", [](MaterialWidget& Widget, const std::string& AssetPath)
        {
            Engine* Runtime = ScriptingSubsystem::Get().GetMaterialEngine();
            AssetRegistryEntry Entry;
            if (!Runtime->GetAssetRegistry().TryGetByPath(AssetPath, Entry)
                || (Entry.Metadata.Type != MaterialAssetType && Entry.Metadata.Type != MaterialInstanceAssetType))
            {
                throw py::key_error(AssetPath);
            }
            Widget.SetMaterial(AssetKey{Entry.Metadata.Guid, {}});
        })
        .def("set_dynamic_material", &MaterialWidget::SetDynamicMaterial)
        .def("resize", [](MaterialWidget& Widget, int Width, int Height) { Widget.resize(Width, Height); })
        .def("show", &MaterialWidget::show)
        .def("hide", &MaterialWidget::hide);
    Module.def("assign_ui_material", [](MaterialWidget& Widget, std::shared_ptr<DynamicMaterialInstance> Material)
    {
        if (Material && Material->GetResolvedMaterial().Definition.Domain != MaterialDomain::UserInterface)
        {
            throw py::value_error("UI widget requires a UserInterface material");
        }
        Widget.SetDynamicMaterial(std::move(Material));
    });
    Module.def("create_dynamic_material_instance", [](const std::string& AssetPath)
    {
        Engine* Runtime = ScriptingSubsystem::Get().GetMaterialEngine();
        if (Runtime == nullptr)
        {
            throw py::value_error("Material runtime is unavailable");
        }
        AssetRegistryEntry Entry;
        if (!Runtime->GetAssetRegistry().TryGetByPath(AssetPath, Entry))
        {
            throw py::key_error(AssetPath);
        }
        return Runtime->CreateDynamicMaterialInstance(AssetKey{Entry.Metadata.Guid, {}});
    });
    Module.def("assign_dynamic_material", [](uint64_t ObjectId, uint32_t Generation,
        std::shared_ptr<DynamicMaterialInstance> Material, int32_t Slot)
    {
        GameObject* Owner = dynamic_cast<GameObject*>(ResolveObjectOrThrow(ObjectId, Generation));
        if (Owner == nullptr || Owner->GetComponent<MeshRendererComponent>() == nullptr)
        {
            throw py::type_error("Object has no MeshRendererComponent");
        }
        const auto Result = Owner->GetComponent<MeshRendererComponent>()->SetDynamicMaterial(std::move(Material), Slot);
        if (Result.HasError())
        {
            throw py::value_error(Result.Message);
        }
    }, py::arg("object_id"), py::arg("generation"), py::arg("material"), py::arg("slot") = -1);
    Module.def("assign_postprocess_material", [](uint64_t ObjectId, uint32_t Generation,
        std::shared_ptr<DynamicMaterialInstance> Material, size_t EffectIndex)
    {
        GameObject* Owner = dynamic_cast<GameObject*>(ResolveObjectOrThrow(ObjectId, Generation));
        if (Owner == nullptr || Owner->GetComponent<CameraComponent>() == nullptr
            || EffectIndex >= Owner->GetComponent<CameraComponent>()->PostProcessEffects.Entries.size()
            || (Material && Material->GetResolvedMaterial().Definition.Domain != MaterialDomain::PostProcess))
        {
            throw py::value_error("A postprocess material and existing camera effect are required");
        }
        auto& Effect = Owner->GetComponent<CameraComponent>()->PostProcessEffects.Entries[EffectIndex];
        Effect.DynamicMaterial = std::move(Material);
        Effect.Snapshot.reset();
    });


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

    def set_dynamic_material(self, material, slot=-1):
        assign_dynamic_material(self._id, self._generation, material, slot)

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
    RegisterPhysicsNavigationBindings(Module);
}
#endif

