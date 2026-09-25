#include "Gameplay/CameraComponent.h"
#include "Gameplay/LightComponent.h"
#include "Gameplay/LightSettings.h"
#include "Gameplay/MeshRendererComponent.h"
#include "Gameplay/ScriptComponent.h"
#include "Reflection/ReflectionSubsystem.h"

void ForceTouchRegistrars()
{
    (void)CameraComponent::StaticReflectionTypeId();
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
