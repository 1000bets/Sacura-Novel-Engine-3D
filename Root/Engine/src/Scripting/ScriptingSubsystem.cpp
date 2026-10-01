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

#if defined(SAKURA_ENABLE_PYTHON)
std::unique_ptr<py::scoped_interpreter> EmbeddedInterpreter;
std::deque<std::string> OwnedAttributeStrings;
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
    OwnedAttributeStrings.clear();
    ClearRegisteredPythonTypes();
    EmbeddedInterpreter.reset();
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
        RegisterEmbeddedEnginePythonModule();
        if (EmbeddedInterpreter == nullptr)
        {
            EmbeddedInterpreter = std::make_unique<py::scoped_interpreter>();
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

void ScriptingSubsystem::Shutdown()
{
    AssertGameThread();
    if (!bInitialized)
    {
        return;
    }

    DestroyAllScriptInstances();

#if defined(SAKURA_ENABLE_PYTHON)
    OwnedAttributeStrings.clear();
    ClearRegisteredPythonTypes();
    EmbeddedInterpreter.reset();
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
    ImportDiagnostics.clear();
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

    for (fs::recursive_directory_iterator Iterator(ScriptsRoot, ErrorCode), End; Iterator != End; Iterator.increment(ErrorCode))
    {
        const fs::directory_entry& Entry = *Iterator;
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
            ImportDiagnostics.push_back(Imported);
        }
    }
    if (ErrorCode)
    {
        ImportDiagnostics.push_back(ReflectionDiagnostic::Fail("Failed to enumerate project scripts: " + ErrorCode.message()));
    }
    if (!ImportDiagnostics.empty())
    {
        return ImportDiagnostics.front();
    }
    return ReflectionDiagnostic::Ok();
}
