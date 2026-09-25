#include "Actions/BuiltinEditorActions.h"
#include "EditorAction.h"

void ForceTouchEditorRegistrars()
{
    (void)EditorAction::StaticReflectionTypeId();
    (void)CreateEmptyObjectAction::StaticReflectionTypeId();
    (void)CreateCameraObjectAction::StaticReflectionTypeId();
    (void)CreateStaticMeshObjectAction::StaticReflectionTypeId();
    (void)CreateDirectionalLightObjectAction::StaticReflectionTypeId();
    (void)CreatePointLightObjectAction::StaticReflectionTypeId();
    (void)CreateSpotLightObjectAction::StaticReflectionTypeId();
    (void)AddCameraComponentAction::StaticReflectionTypeId();
    (void)AddLightComponentAction::StaticReflectionTypeId();
    (void)AddMeshRendererComponentAction::StaticReflectionTypeId();
    (void)&EditorAction::s_ReflectionClassRegistrar;
    (void)&CreateEmptyObjectAction::s_ReflectionClassRegistrar;
    (void)&CreateCameraObjectAction::s_ReflectionClassRegistrar;
    (void)&CreateStaticMeshObjectAction::s_ReflectionClassRegistrar;
    (void)&CreateDirectionalLightObjectAction::s_ReflectionClassRegistrar;
    (void)&CreatePointLightObjectAction::s_ReflectionClassRegistrar;
    (void)&CreateSpotLightObjectAction::s_ReflectionClassRegistrar;
    (void)&AddCameraComponentAction::s_ReflectionClassRegistrar;
    (void)&AddLightComponentAction::s_ReflectionClassRegistrar;
    (void)&AddMeshRendererComponentAction::s_ReflectionClassRegistrar;
}
