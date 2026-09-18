#include "Reflection/PendingRegistry.h"
#include "Reflection/ReflectionSubsystem.h"

namespace
{
void BindObjectClass()
{
    Class* Target = ReflectionSubsystem::Get().GetOrCreateClassDuringBind("engine.Object");
    ReflectionSubsystem::Get().SetClassBindData(
        Target,
        nullptr,
        1,
        CreationPolicy::Abstract,
        nullptr);
}

void BindComponentClass()
{
    Class* Target = ReflectionSubsystem::Get().GetOrCreateClassDuringBind("engine.Component");
    ReflectionSubsystem::Get().SetClassBindData(
        Target,
        "engine.Object",
        1,
        CreationPolicy::Abstract,
        nullptr);
}
}

void RegisterNativeReflectionBootstrap()
{
    static bool bRegistered = false;
    if (bRegistered)
    {
        return;
    }
    bRegistered = true;

    static ClassRecipe ObjectRecipe{};
    ObjectRecipe.TypeIdString = "engine.Object";
    ObjectRecipe.BaseTypeIdString = nullptr;
    ObjectRecipe.SchemaVersion = 1;
    ObjectRecipe.Policy = CreationPolicy::Abstract;
    ObjectRecipe.Origin = ReflectionOrigin::Native;
    ObjectRecipe.Bind = &BindObjectClass;
    PendingRegistry::Get().AddClassRecipe(ObjectRecipe);

    static ClassRecipe ComponentRecipe{};
    ComponentRecipe.TypeIdString = "engine.Component";
    ComponentRecipe.BaseTypeIdString = "engine.Object";
    ComponentRecipe.SchemaVersion = 1;
    ComponentRecipe.Policy = CreationPolicy::Abstract;
    ComponentRecipe.Origin = ReflectionOrigin::Native;
    ComponentRecipe.Bind = &BindComponentClass;
    PendingRegistry::Get().AddClassRecipe(ComponentRecipe);
}
