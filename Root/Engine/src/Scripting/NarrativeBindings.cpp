#include "Story/Narrative.h"
#include "Story/StoryTypes.h"
#include "PythonRuntimeInternal.h"
#include "Reflection/ReflectionSubsystem.h"
#include "Reflection/Json/ReflectionJson.h"
#include "Reflection/PropertyAccess.h"
#include "Core/Object/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "World/Scene.h"
#include "World/Components/ScriptComponent.h"
#include <algorithm>
#include <cmath>
#include <set>

static std::map<std::string, NarrativeNodeDefinition> NarrativeDefinitions;

#if defined(SAKURA_ENABLE_PYTHON)
static std::map<std::string, py::object> NarrativeClasses;
struct NarrativeInvocation
{
    py::object Generator;
    py::object Context;
    py::object Instance;
    double WaitSeconds = 0;
    NarrativeNodeDefinition Definition;
    std::string NodeId;
};
static std::map<uint64_t, NarrativeInvocation> NarrativeInvocations;
static uint64_t NextNarrativeInvocation = 1;

static void CancelNarrativeInvocation(uint64_t Identifier)
{
    const auto Found = NarrativeInvocations.find(Identifier);
    if (Found == NarrativeInvocations.end())
    {
        return;
    }
    py::gil_scoped_acquire Gil;
    NarrativeInvocation Invocation = std::move(Found->second);
    NarrativeInvocations.erase(Found);
    Invocation.Context.attr("_Active") = false;
    if (!Invocation.Generator.is_none())
    {
        try
        {
            Invocation.Generator.attr("close")();
        }
        catch (const py::error_already_set& Error)
        {
            PrintString(std::string("Narrative cancellation failed: ") + Error.what());
        }
    }
}

void ClearNarrativeBindings()
{
    while (!NarrativeInvocations.empty())
    {
        CancelNarrativeInvocation(NarrativeInvocations.begin()->first);
    }
    NarrativeClasses.clear();
    NarrativeDefinitions.clear();
}

void RegisterNarrativeBindings(py::module_& Module)
{
    Module.def("_register_narrative", [](const py::object& PythonType, const std::string& Type,
        const std::string& Label, bool bPure, const py::dict& Inputs, const py::dict& Outputs,
        const std::vector<std::string>& Exits)
    {
        if (Type.empty() || Type.rfind("component:", 0) == 0 || NarrativeDefinitions.count(Type))
        {
            throw py::value_error("Invalid or duplicate Narrative type: " + Type);
        }
        NarrativeNodeDefinition Definition;
        Definition.Type = Type;
        Definition.Label = Label;
        Definition.bPure = bPure;
        Definition.Exits = Exits;
        std::set<std::string> UniqueExits;
        for (const std::string& Exit : Exits)
        {
            if (Exit.empty() || !UniqueExits.insert(Exit).second)
            {
                throw py::value_error("Narrative exits must be unique nonempty names");
            }
        }
        if ((bPure && !Exits.empty()) || (!bPure && Exits.empty()))
        {
            throw py::value_error("Actions require execution exits; pure nodes must not have them");
        }
        auto ReadPins = [&](const py::dict& Pins, std::vector<NarrativePin>& Destination)
        {
            for (auto Entry : Pins)
            {
                NarrativePin Pin;
                Pin.Property.Id = PropertyId{py::str(Entry.first).cast<std::string>()};
                py::object Declaration = py::reinterpret_borrow<py::object>(Entry.second);
                Pin.Property.ValueTypeId = ResolvePythonValueTypeId(Declaration.attr("Type"));
                if (Pin.Property.Id.Value.empty() || !Pin.Property.ValueTypeId.IsValid())
                {
                    throw py::value_error("Unsupported Narrative pin: " + Pin.Property.Id.Value);
                }
                Pin.Property.Attributes.DisplayName = InternAttributeString(Declaration.attr("Label").cast<std::string>());
                Pin.bHasDefault = Declaration.attr("HasDefault").cast<bool>();
                if (Pin.bHasDefault)
                {
                    ReflectedValue Value = PythonToReflected(Declaration.attr("Default"), Pin.Property.ValueTypeId);
                    auto Validated = PropertyAccess::ValidatePropertyValue(Pin.Property, Value);
                    if (!Validated.bOk)
                    {
                        throw py::value_error(Validated.Message);
                    }
                    auto Converted = ReflectionJson::ReflectedValueToJson(Pin.Property.ValueTypeId, Value, Pin.Default);
                    if (!Converted.bOk)
                    {
                        throw py::value_error(Converted.Message);
                    }
                }
                Destination.push_back(std::move(Pin));
            }
        };
        ReadPins(Inputs, Definition.Inputs);
        ReadPins(Outputs, Definition.Outputs);
        NarrativeClasses.emplace(Type, PythonType);
        NarrativeDefinitions.emplace(Type, std::move(Definition));
    });
    py::exec(R"PY(
class NarrativeNode:
    pass

_MissingNarrativeDefault = object()

class pin:
    def __init__(self, Type, *, Default=_MissingNarrativeDefault, Label=""):
        self.Type = Type
        self.Default = Default
        self.HasDefault = Default is not _MissingNarrativeDefault
        self.Label = Label

class NarrativeResult:
    def __init__(self, Exit="Done", **Values):
        self.Exit = Exit
        self.Values = Values

class _NarrativeWait:
    def __init__(self, Seconds):
        import math
        if not math.isfinite(Seconds) or Seconds < 0:
            raise ValueError("Wait duration must be finite and nonnegative")
        self.Seconds = float(Seconds)

class _NarrativeContext:
    def __init__(self, Pure, Read, Write):
        self._Active = True
        self._Pure = Pure
        self._Read = Read
        self._Write = Write

    def _Check(self, Effects=False):
        if not self._Active:
            raise RuntimeError("Narrative invocation is no longer active")
        if Effects and self._Pure:
            raise RuntimeError("Pure Narrative nodes cannot perform actions")

    def GetVariable(self, Name):
        self._Check()
        return self._Read(Name)

    def SetVariable(self, Name, Value):
        self._Check(True)
        self._Write(Name, Value)

    def Invoke(self, Target, MethodName, **Arguments):
        self._Check(True)
        return Target.Invoke(MethodName, **Arguments)

    def Wait(self, Seconds):
        self._Check(True)
        return _NarrativeWait(Seconds)

def register_node(*, type_id, label="", pure=False):
    import inspect
    def Decorate(NodeType):
        if not issubclass(NodeType, NarrativeNode):
            raise TypeError("Narrative class must inherit NarrativeNode")
        Inputs = dict(getattr(NodeType, "Inputs", {}))
        Outputs = dict(getattr(NodeType, "Outputs", {}))
        Exits = tuple(getattr(NodeType, "Exits", ()))
        CallbackName = "Evaluate"
        if not pure:
            CallbackName = "Execute"
        Callback = getattr(NodeType, CallbackName, None)
        if Callback is None or inspect.iscoroutinefunction(Callback) or inspect.isasyncgenfunction(Callback):
            raise TypeError("Narrative nodes require Evaluate or Execute; use yield Context.Wait for waiting")
        if pure and inspect.isgeneratorfunction(Callback):
            raise TypeError("Pure nodes cannot yield")
        Signature = list(inspect.signature(Callback).parameters.values())
        if len(Signature) < 2 or {Parameter.name for Parameter in Signature[2:]} != set(Inputs):
            raise TypeError("Narrative callback arguments must match Inputs after self and Context")
        for Parameter in Signature:
            if Parameter.kind not in (inspect.Parameter.POSITIONAL_OR_KEYWORD, inspect.Parameter.KEYWORD_ONLY):
                raise TypeError("Narrative callback requires explicit named arguments")
        if not all(isinstance(Declaration, pin) for Declaration in list(Inputs.values()) + list(Outputs.values())):
            raise TypeError("Narrative inputs and outputs must use pin declarations")
        _register_narrative(NodeType, type_id, label or NodeType.__name__, pure, Inputs, Outputs, Exits)
        return NodeType
    return Decorate
)PY", Module.attr("__dict__"));
}
#endif

std::vector<NarrativeNodeDefinition> GetNarrativeDefinitions()
{
    std::vector<NarrativeNodeDefinition> Result;
    for (const auto& Entry : NarrativeDefinitions)
    {
        Result.push_back(Entry.second);
    }
    for (const Class* ComponentClass : ReflectionSubsystem::Get().GetPublishedCatalog().Classes)
    {
        for (const MethodDescriptor& Method : ComponentClass->Methods)
        {
            NarrativeNodeDefinition Definition;
            Definition.Type = "component:" + ComponentClass->GetTypeId().Value + ":" + Method.Name;
            Definition.Label = ComponentClass->GetTypeId().Value + " / " + Method.Name;
            Definition.ComponentType = ComponentClass->GetTypeId().Value;
            Definition.ComponentMethod = Method.Name;
            Definition.Exits = {"Done"};
            NarrativePin Target;
            Definition.ComponentTargetInput = "Target";
            while (std::any_of(Method.Parameters.begin(), Method.Parameters.end(), [&](const PropertyDescriptor& Parameter)
            {
                return Parameter.Id.Value == Definition.ComponentTargetInput;
            }))
            {
                Definition.ComponentTargetInput = "Component" + Definition.ComponentTargetInput;
            }
            Target.Property.Id = PropertyId{Definition.ComponentTargetInput};
            Target.Property.ValueTypeId = TypeId{"engine.ObjectHandle"};
            Definition.Inputs.push_back(Target);
            for (const auto& Parameter : Method.Parameters)
            {
                NarrativePin Pin;
                Pin.Property = Parameter;
                Definition.Inputs.push_back(std::move(Pin));
            }
            if (Method.ResultTypeId.IsValid())
            {
                NarrativePin ResultPin;
                ResultPin.Property.Id = PropertyId{"Result"};
                ResultPin.Property.ValueTypeId = Method.ResultTypeId;
                Definition.Outputs.push_back(ResultPin);
            }
            Result.push_back(std::move(Definition));
        }
    }
    return Result;
}

bool FindNarrativeDefinition(const std::string& Type, NarrativeNodeDefinition& Definition)
{
    for (const auto& Candidate : GetNarrativeDefinitions())
    {
        if (Candidate.Type == Type)
        {
            Definition = Candidate;
            return true;
        }
    }
    return false;
}

struct NarrativeRuntime::State
{
    ObjectHandle World;
    uint64_t Invocation = 0;
    bool bActive = true;
    std::map<std::string, nlohmann::json> Variables;
    std::map<std::string, std::map<std::string, ReflectedValue>> Results;
    std::string CurrentNode;
    std::string Exit;
    bool bCompleted = false;
};

NarrativeRuntime::NarrativeRuntime() : Session(std::make_shared<State>()) {}
NarrativeRuntime::~NarrativeRuntime() { Stop(); }
void NarrativeRuntime::Stop()
{
    CancelAction();
    Session->bActive = false;
}
void NarrativeRuntime::CancelAction()
{
#if defined(SAKURA_ENABLE_PYTHON)
    CancelNarrativeInvocation(Session->Invocation);
#endif
    Session->Invocation = 0;
    Session->bCompleted = false;
    Session->Exit.clear();
}
void NarrativeRuntime::Reset(ObjectHandle World)
{
    Stop();
    Session = std::make_shared<State>();
    Session->World = World;
}
void NarrativeRuntime::BeginTransition()
{
    CancelAction();
    Session->bActive = true;
}
const std::map<std::string, nlohmann::json>& NarrativeRuntime::GetVariables() const { return Session->Variables; }

#if defined(SAKURA_ENABLE_PYTHON)
static std::map<std::string, ReflectedValue> ReadNarrativeResults(const NarrativeNodeDefinition& Definition,
    const py::object& Returned, std::string& Exit)
{
    py::dict Values;
    if (Definition.bPure)
    {
        Values = Returned.cast<py::dict>();
    }
    else
    {
        Exit = Returned.attr("Exit").cast<std::string>();
        if (std::find(Definition.Exits.begin(), Definition.Exits.end(), Exit) == Definition.Exits.end())
        {
            throw py::value_error("Unknown execution exit: " + Exit);
        }
        Values = Returned.attr("Values").cast<py::dict>();
    }
    if (Values.size() != Definition.Outputs.size())
    {
        throw py::value_error("Narrative output count mismatch");
    }
    std::map<std::string, ReflectedValue> Result;
    for (const NarrativePin& Pin : Definition.Outputs)
    {
        const py::str Name(Pin.Property.Id.Value);
        if (!Values.contains(Name))
        {
            throw py::value_error("Missing Narrative output: " + Pin.Property.Id.Value);
        }
        ReflectedValue Value = PythonToReflected(Values[Name], Pin.Property.ValueTypeId);
        auto Validated = PropertyAccess::ValidatePropertyValue(Pin.Property, Value);
        if (!Validated.bOk)
        {
            throw py::value_error(Validated.Message);
        }
        Result.emplace(Pin.Property.Id.Value, std::move(Value));
    }
    return Result;
}
#endif

bool NarrativeRuntime::Start(const StoryDocument& Document, const StoryNode& Node, std::string& Error)
{
    AssertGameThread();
    if (!Session->bActive || Session->Invocation != 0)
    {
        Error = "Narrative session is stopped or already executing";
        return false;
    }
    Session->CurrentNode = Node.Id;
    Session->bCompleted = false;
#if !defined(SAKURA_ENABLE_PYTHON)
    Error = "Narrative requires Python support";
    return false;
#else
    try
    {
        py::gil_scoped_acquire Gil;
        std::set<std::string> Visiting;
        std::map<std::string, std::map<std::string, ReflectedValue>> Evaluated;
        std::function<std::map<std::string, ReflectedValue>(const StoryNode&, bool)> Execute;
        Execute = [&](const StoryNode& Current, bool bDependency)
        {
            if (Visiting.size() >= 128 || !Visiting.insert(Current.Id).second)
            {
                throw py::value_error("Narrative data cycle or depth limit at " + Current.Id);
            }
            NarrativeNodeDefinition Definition;
            if (!FindNarrativeDefinition(Current.NarrativeType, Definition))
            {
                throw py::value_error("Narrative type not found: " + Current.NarrativeType);
            }
            if (bDependency && !Definition.bPure)
            {
                Visiting.erase(Current.Id);
                if (!Session->Results.count(Current.Id))
                {
                    throw py::value_error("Action output is not available before execution: " + Current.Id);
                }
                return Session->Results.at(Current.Id);
            }
            py::dict Arguments;
            std::unordered_map<std::string, ReflectedValue> NativeArguments;
            Scene* World = MemorySubsystem::Get()->ResolveHandle<Scene>(Session->World);
            ReflectionJson::ObjectResolver Resolver;
            if (World != nullptr)
            {
                Resolver = ReflectionJson::MakeSceneResolver(*World);
            }
            for (const NarrativePin& Pin : Definition.Inputs)
            {
                ReflectedValue Value;
                const std::string& Name = Pin.Property.Id.Value;
                const auto Source = Current.NarrativeSources.find(Name);
                if (Source != Current.NarrativeSources.end())
                {
                    const StoryNode* Producer = nullptr;
                    for (const StoryNode& Candidate : Document.Nodes)
                    {
                        if (Candidate.Id == Source->second.first)
                        {
                            Producer = &Candidate;
                            break;
                        }
                    }
                    if (Producer == nullptr)
                    {
                        throw py::value_error("Missing data source node");
                    }
                    if (!Evaluated.count(Producer->Id))
                    {
                        Evaluated[Producer->Id] = Execute(*Producer, true);
                    }
                    const auto Found = Evaluated.at(Producer->Id).find(Source->second.second);
                    if (Found == Evaluated.at(Producer->Id).end())
                    {
                        throw py::value_error("Missing data output");
                    }
                    Value = Found->second;
                }
                else
                {
                    auto Literal = Current.NarrativeInputs.find(Name);
                    if (Literal == Current.NarrativeInputs.end() && !Pin.bHasDefault)
                    {
                        throw py::value_error("Missing Narrative input: " + Name);
                    }
                    nlohmann::json Input = Pin.Default;
                    if (Literal != Current.NarrativeInputs.end())
                    {
                        Input = Literal.value();
                    }
                    auto Converted = ReflectionJson::JsonToReflectedValue(Pin.Property.ValueTypeId, Input, Value, Resolver);
                    if (!Converted.bOk)
                    {
                        throw py::value_error(Converted.Message);
                    }
                }
                auto Validated = PropertyAccess::ValidatePropertyValue(Pin.Property, Value);
                if (!Validated.bOk)
                {
                    throw py::value_error(Validated.Message);
                }
                NativeArguments.emplace(Name, Value);
                Arguments[py::str(Name)] = ReflectedToPython(Value);
            }
            std::map<std::string, ReflectedValue> Result;
            if (!Definition.ComponentType.empty())
            {
                ScriptComponent* Target = MemorySubsystem::Get()->ResolveHandle<ScriptComponent>(NativeArguments.at(Definition.ComponentTargetInput).ObjectValue);
                if (Target == nullptr || !Target->GetClass()->IsA(TypeId{Definition.ComponentType}))
                {
                    throw py::value_error("Component target is missing or incompatible");
                }
                NativeArguments.erase(Definition.ComponentTargetInput);
                ReflectedValue Returned;
                auto Invoked = ScriptingSubsystem::Get().InvokeMethod(*Target, Definition.ComponentMethod, NativeArguments, Returned);
                if (!Invoked.bOk)
                {
                    throw py::value_error(Invoked.Message);
                }
                if (!Definition.Outputs.empty())
                {
                    Result.emplace("Result", std::move(Returned));
                }
                Session->Exit = "Done";
                Session->bCompleted = true;
            }
            else
            {
                py::module_ Module = py::module_::import("engine");
                std::weak_ptr<State> WeakSession = Session;
                py::object Read = py::cpp_function([WeakSession](const std::string& Name)
                {
                    auto State = WeakSession.lock();
                    if (!State || !State->bActive || !State->Variables.count(Name))
                    {
                        throw py::value_error("Narrative variable not available: " + Name);
                    }
                    return py::module_::import("json").attr("loads")(State->Variables.at(Name).dump());
                });
                py::object Write = py::cpp_function([WeakSession](const std::string& Name, const py::object& Value)
                {
                    auto State = WeakSession.lock();
                    if (!State || !State->bActive)
                    {
                        throw py::value_error("Narrative session stopped");
                    }
                    State->Variables[Name] = nlohmann::json::parse(py::module_::import("json").attr("dumps")(Value).cast<std::string>());
                });
                py::object Context = Module.attr("_NarrativeContext")(Definition.bPure, Read, Write);
                py::object Instance = NarrativeClasses.at(Definition.Type)();
                const char* Callback = "Execute";
                if (Definition.bPure)
                {
                    Callback = "Evaluate";
                }
                py::object Returned;
                try
                {
                    Returned = Instance.attr(Callback)(Context, **Arguments);
                }
                catch (...)
                {
                    Context.attr("_Active") = false;
                    throw;
                }
                if (PyGen_Check(Returned.ptr()))
                {
                    if (Definition.bPure)
                    {
                        Context.attr("_Active") = false;
                        throw py::value_error("Pure nodes cannot return generators");
                    }
                    Session->Invocation = NextNarrativeInvocation++;
                    NarrativeInvocations.emplace(Session->Invocation,
                        NarrativeInvocation{Returned, Context, Instance, 0, Definition, Current.Id});
                }
                else
                {
                    Context.attr("_Active") = false;
                    Result = ReadNarrativeResults(Definition, Returned, Session->Exit);
                    if (!bDependency)
                    {
                        Session->bCompleted = true;
                    }
                }
            }
            Visiting.erase(Current.Id);
            return Result;
        };
        Session->Results[Node.Id] = Execute(Node, false);
        return true;
    }
    catch (const std::exception& Failure)
    {
        Error = "Narrative node " + Node.Id + ": " + Failure.what();
        Stop();
        return false;
    }
#endif
}

bool NarrativeRuntime::Tick(float DeltaTime, bool& Completed, std::string& Exit, std::string& Error)
{
    AssertGameThread();
    Completed = Session->bCompleted;
    Exit = Session->Exit;
#if defined(SAKURA_ENABLE_PYTHON)
    if (Session->Invocation == 0)
    {
        return true;
    }
    try
    {
        py::gil_scoped_acquire Gil;
        auto Found = NarrativeInvocations.find(Session->Invocation);
        if (Found == NarrativeInvocations.end())
        {
            throw py::value_error("Narrative execution has been cancelled");
        }
        auto& Invocation = Found->second;
        Invocation.WaitSeconds -= DeltaTime;
        if (Invocation.WaitSeconds > 0)
        {
            return true;
        }
        PyObject* Raw = PyObject_CallMethod(Invocation.Generator.ptr(), "__next__", nullptr);
        if (Raw == nullptr)
        {
            if (!PyErr_ExceptionMatches(PyExc_StopIteration))
            {
                throw py::error_already_set();
            }
            PyObject* ExceptionType = nullptr;
            PyObject* ExceptionValue = nullptr;
            PyObject* Traceback = nullptr;
            PyErr_Fetch(&ExceptionType, &ExceptionValue, &Traceback);
            PyErr_NormalizeException(&ExceptionType, &ExceptionValue, &Traceback);
            py::object OwnedType = py::reinterpret_steal<py::object>(ExceptionType);
            py::object OwnedValue = py::reinterpret_steal<py::object>(ExceptionValue);
            py::object OwnedTraceback = py::reinterpret_steal<py::object>(Traceback);
            Session->Results[Invocation.NodeId] = ReadNarrativeResults(Invocation.Definition, OwnedValue.attr("value"), Exit);
            Invocation.Context.attr("_Active") = false;
            NarrativeInvocations.erase(Found);
            Session->Invocation = 0;
            Session->bCompleted = true;
            Session->Exit = Exit;
            Completed = true;
            return true;
        }
        py::object Yielded = py::reinterpret_steal<py::object>(Raw);
        if (!py::isinstance(Yielded, py::module_::import("engine").attr("_NarrativeWait")))
        {
            throw py::value_error("Narrative actions may only yield Context.Wait");
        }
        Invocation.WaitSeconds = Yielded.attr("Seconds").cast<double>();
        if (!std::isfinite(Invocation.WaitSeconds) || Invocation.WaitSeconds < 0)
        {
            throw py::value_error("Wait duration must be finite and nonnegative");
        }
        return true;
    }
    catch (const std::exception& Failure)
    {
        Error = "Narrative node " + Session->CurrentNode + ": " + Failure.what();
        Stop();
        return false;
    }
#else
    Error = "Narrative requires Python support";
    return false;
#endif
}


bool ValidateNarrativeDocument(const StoryDocument& Document, std::string& Error)
{
    std::map<std::string, const StoryNode*> Nodes;
    std::map<std::string, NarrativeNodeDefinition> Definitions;
    for (const auto& Node : Document.Nodes)
    {
        Nodes.emplace(Node.Id, &Node);
        if (Node.Kind == StoryNodeKind::Narrative)
        {
            NarrativeNodeDefinition Definition;
            if (!FindNarrativeDefinition(Node.NarrativeType, Definition))
            {
                Error = "Unknown Narrative type: " + Node.NarrativeType;
                return false;
            }
            Definitions.emplace(Node.Id, std::move(Definition));
        }
    }
    auto ValidateTarget = [&](const std::string& Target)
    {
        const auto Found = Definitions.find(Target);
        if (Found != Definitions.end() && Found->second.bPure)
        {
            Error = "Pure Narrative node cannot receive execution: " + Target;
            return false;
        }
        return true;
    };
    if (!ValidateTarget(Document.StartNodeId))
    {
        return false;
    }
    for (const auto& Node : Document.Nodes)
    {
        if (!ValidateTarget(Node.NextNodeId))
        {
            return false;
        }
        for (const auto& Branch : Node.Choices)
        {
            if (!ValidateTarget(Branch.NextNodeId))
            {
                return false;
            }
        }
        if (Node.Kind != StoryNodeKind::Narrative)
        {
            continue;
        }
        const auto& Definition = Definitions.at(Node.Id);
        std::set<std::string> Exits;
        for (const auto& Branch : Node.Choices)
        {
            if (!Exits.insert(Branch.Id).second || std::find(Definition.Exits.begin(), Definition.Exits.end(), Branch.Id) == Definition.Exits.end())
            {
                Error = "Invalid Narrative execution exit: " + Node.Id + "." + Branch.Id;
                return false;
            }
        }
        auto HasInput = [&](const std::string& Name)
        {
            return std::any_of(Definition.Inputs.begin(), Definition.Inputs.end(), [&](const NarrativePin& Pin)
            {
                return Pin.Property.Id.Value == Name;
            });
        };
        for (const auto& Entry : Node.NarrativeInputs.items())
        {
            if (!HasInput(Entry.key()))
            {
                Error = "Unknown Narrative input: " + Entry.key();
                return false;
            }
        }
        for (const auto& Source : Node.NarrativeSources)
        {
            if (!HasInput(Source.first))
            {
                Error = "Unknown Narrative input: " + Source.first;
                return false;
            }
        }
        for (const auto& Input : Definition.Inputs)
        {
            const auto Source = Node.NarrativeSources.find(Input.Property.Id.Value);
            if (Source == Node.NarrativeSources.end())
            {
                if (!Input.bHasDefault && !Node.NarrativeInputs.contains(Input.Property.Id.Value))
                {
                    Error = "Missing Narrative input: " + Node.Id + "." + Input.Property.Id.Value;
                    return false;
                }
                continue;
            }
            const auto Producer = Definitions.find(Source->second.first);
            if (Producer == Definitions.end())
            {
                Error = "Missing Narrative source: " + Source->second.first;
                return false;
            }
            const auto Output = std::find_if(Producer->second.Outputs.begin(), Producer->second.Outputs.end(), [&](const NarrativePin& Pin)
            {
                return Pin.Property.Id.Value == Source->second.second;
            });
            if (Output == Producer->second.Outputs.end() || Output->Property.ValueTypeId != Input.Property.ValueTypeId)
            {
                Error = "Incompatible Narrative data connection at " + Node.Id + "." + Input.Property.Id.Value;
                return false;
            }
        }
    }
    std::set<std::string> Visiting;
    std::set<std::string> Visited;
    std::function<bool(const StoryNode&)> Visit = [&](const StoryNode& Node)
    {
        if (Visited.count(Node.Id))
        {
            return true;
        }
        if (Visiting.size() >= 128 || !Visiting.insert(Node.Id).second)
        {
            Error = "Narrative data cycle or depth limit: " + Node.Id;
            return false;
        }
        for (const auto& Source : Node.NarrativeSources)
        {
            if (!Visit(*Nodes.at(Source.second.first)))
            {
                return false;
            }
        }
        Visiting.erase(Node.Id);
        Visited.insert(Node.Id);
        return true;
    };
    for (const auto& Entry : Definitions)
    {
        if (!Visit(*Nodes.at(Entry.first)))
        {
            return false;
        }
    }
    return true;
}
