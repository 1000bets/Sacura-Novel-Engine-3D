#include "Gameplay/CameraComponent.h"
#include "Gameplay/LightSettings.h"
#include "Gameplay/ScriptComponent.h"
#include "Reflection/ReflectionSubsystem.h"

void ForceTouchRegistrars()
{
    (void)CameraComponent::StaticReflectionTypeId();
    (void)ScriptComponent::StaticReflectionTypeId();
    (void)LightSettings::StaticReflectionTypeId();
    (void)&CameraComponent::s_ReflectionClassRegistrar;
    (void)&ScriptComponent::s_ReflectionClassRegistrar;
    (void)&LightSettings::s_ReflectionStructRegistrar;
}
