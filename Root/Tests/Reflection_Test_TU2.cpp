#include "Gameplay/CameraComponent.h"

void ReflectionTestTouchSecondaryTranslationUnit()
{
    (void)CameraComponent::StaticReflectionTypeId();
    (void)&CameraComponent::s_ReflectionClassRegistrar;
}
