#include "Scripting/ScriptingSubsystem.h"

#include "Core/Object/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Core/Transform.h"
#include "World/GameObject.h"
#include "World/Components/ScriptComponent.h"
#include "Reflection/PropertyAccess.h"
#include "Reflection/ReflectionSubsystem.h"

#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "Scripting/PythonRuntimeInternal.h"

void ScriptingSubsystem::DestroyAllScriptInstances()
{
    AssertGameThread();
#if defined(SAKURA_ENABLE_PYTHON)
    if (EmbeddedInterpreter == nullptr)
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

void ScriptingSubsystem::BindScriptComponent(ScriptComponent& Component)
{
    AssertGameThread();
#if !defined(SAKURA_ENABLE_PYTHON)
    return;
#else
    if (!bInitialized || EmbeddedInterpreter == nullptr)
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
        if (EmbeddedInterpreter != nullptr)
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
    if (Component.IsScriptFailed() || EmbeddedInterpreter == nullptr)
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
