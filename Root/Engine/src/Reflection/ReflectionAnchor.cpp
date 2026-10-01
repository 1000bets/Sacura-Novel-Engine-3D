#include "World/Components/CameraComponent.h"
#include "World/Components/ArrowComponent.h"
#include "World/Components/LightComponent.h"
#include "World/Components/LightSettings.h"
#include "World/Components/MeshRendererComponent.h"
#include "World/Components/ScriptComponent.h"
#include "Reflection/ReflectionSubsystem.h"

void ForceTouchRegistrars()
{
    (void)CameraComponent::StaticReflectionTypeId();
    (void)ArrowComponent::StaticReflectionTypeId();
    (void)&ArrowComponent::s_ReflectionClassRegistrar;
    (void)LightComponent::StaticReflectionTypeId();
    (void)MeshRendererComponent::StaticReflectionTypeId();
    (void)ScriptComponent::StaticReflectionTypeId();
    (void)LightSettings::StaticReflectionTypeId();
    (void)&CameraComponent::s_ReflectionClassRegistrar;
    (void)&LightComponent::s_ReflectionClassRegistrar;
    (void)&MeshRendererComponent::s_ReflectionClassRegistrar;
    (void)&ScriptComponent::s_ReflectionClassRegistrar;
    (void)&LightSettings::s_ReflectionStructRegistrar;
}
