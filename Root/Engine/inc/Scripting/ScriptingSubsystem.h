#pragma once

#include "Gameplay/ObjectHandle.h"
#include "Reflection/PropertyDescriptor.h"
#include "Reflection/ReflectedValue.h"
#include "Reflection/TypeId.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class ScriptComponent;
class Class;

struct PythonFieldDeclaration
{
    PropertyId Id{};
    TypeId ValueTypeId{};
    PropertyAttributes Attributes{};
    ReflectedValue DefaultValue{};
};

struct PythonClassPublishRequest
{
    TypeId Id{};
    uint32_t SchemaVersion = 1;
    TypeId BaseTypeId{"engine.ScriptComponent"};
    std::vector<PythonFieldDeclaration> Fields;
};

class ScriptingSubsystem
{
public:
    static ScriptingSubsystem& Get();

    ScriptingSubsystem(const ScriptingSubsystem&) = delete;
    ScriptingSubsystem& operator=(const ScriptingSubsystem&) = delete;

    ReflectionDiagnostic Initialize();
    void Shutdown();

    bool IsInitialized() const { return bInitialized; }
    bool IsPythonEnabled() const;

    void SetScriptsRoot(const std::filesystem::path& ScriptsRoot);
    const std::filesystem::path& GetScriptsRoot() const { return ScriptsRoot; }

    ReflectionDiagnostic ImportConfiguredModules();
    ReflectionDiagnostic ImportModule(const std::string& ModuleName);

    ReflectionDiagnostic PublishPythonClass(const PythonClassPublishRequest& Request);

    void BindScriptComponent(ScriptComponent& Component);
    void UnbindScriptComponent(ScriptComponent& Component);
    void TickScriptComponent(ScriptComponent& Component, float DeltaTime);
    void DestroyAllScriptInstances();

private:
    ScriptingSubsystem();
    ~ScriptingSubsystem();

    bool bInitialized = false;
    std::filesystem::path ScriptsRoot;
    uint64_t NextScriptInstanceId = 1;

#if defined(SAKURA_ENABLE_PYTHON)
    struct ScriptInstanceRecord;
    std::unordered_map<uint64_t, std::unique_ptr<ScriptInstanceRecord>> Instances;
#endif
};

void RegisterEmbeddedEnginePythonModule();
