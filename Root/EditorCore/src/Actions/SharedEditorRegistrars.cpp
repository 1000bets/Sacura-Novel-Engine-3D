#include "Actions/SharedBuiltinEditorActions.h"
#include "Actions/SharedPhysicsNavigationEditorActions.h"
#include "Actions/EditorAction.h"
#include "Assets/AssetCreationFactory.h"

void ForceTouchSharedEditorRegistrars()
{
    ForceTouchBuiltinAssetCreationFactories();
    (void)&CreateSkySphereObjectAction::s_ReflectionClassRegistrar;
    (void)&CreateSkyLightObjectAction::s_ReflectionClassRegistrar;
    (void)&CreatePhysicsBodyObjectAction::s_ReflectionClassRegistrar;
    (void)&CreateCharacterObjectAction::s_ReflectionClassRegistrar;
    (void)&CreateNavMeshBoundsObjectAction::s_ReflectionClassRegistrar;
    (void)&CreateNavigationModifierObjectAction::s_ReflectionClassRegistrar;
    (void)&CreateNavigationLinkObjectAction::s_ReflectionClassRegistrar;
    (void)&AddPhysicsBodyComponentAction::s_ReflectionClassRegistrar;
    (void)&AddCharacterControllerComponentAction::s_ReflectionClassRegistrar;
    (void)&AddNavMeshBoundsComponentAction::s_ReflectionClassRegistrar;
    (void)&AddNavigationModifierComponentAction::s_ReflectionClassRegistrar;
    (void)&AddNavigationLinkComponentAction::s_ReflectionClassRegistrar;
    (void)&AddNavigationAgentComponentAction::s_ReflectionClassRegistrar;
    (void)EditorAction::StaticReflectionTypeId();
    (void)CreateEmptyObjectAction::StaticReflectionTypeId();
    (void)CreateCameraObjectAction::StaticReflectionTypeId();
    (void)CreateStaticMeshObjectAction::StaticReflectionTypeId();
    (void)CreateDirectionalLightObjectAction::StaticReflectionTypeId();
    (void)CreatePointLightObjectAction::StaticReflectionTypeId();
    (void)CreateSpotLightObjectAction::StaticReflectionTypeId();
    (void)AddCameraComponentAction::StaticReflectionTypeId();
    (void)AddLightComponentAction::StaticReflectionTypeId();
    (void)AddArrowComponentAction::StaticReflectionTypeId();
    (void)&AddArrowComponentAction::s_ReflectionClassRegistrar;
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
