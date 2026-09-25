# ---------------------------------------------------------------
# Engine-owned third-party (SDL3, Diligent, SimpleMath, …)
# ---------------------------------------------------------------
add_subdirectory(ThirdParty)

# ---------------------------------------------------------------
# Engine library (explicit sources — do not use file(GLOB) for compilation units)
# ---------------------------------------------------------------
set(ENGINE_SRC
    src/Assets/AssetGpuUploader.cpp
    src/Assets/AssetManager.cpp
    src/Assets/AssetMetadata.cpp
    src/Assets/AssetPath.cpp
    src/Assets/AssetRegistry.cpp
    src/Assets/AssetTypes.cpp
    src/Assets/ContentHash.cpp
    src/Assets/Guid.cpp
    src/Assets/SceneAssetResolver.cpp
    src/Assets/Loaders/MaterialLoader.cpp
    src/Assets/Loaders/ModelLoader.cpp
    src/Assets/Loaders/SkeletalLoader.cpp
    src/Assets/Loaders/TextureLoader.cpp
    src/Core/EnginePaths.cpp
    src/Core/MemorySubsystem.cpp
    src/Core/Subsystem.cpp
    src/Core/Threading/JobSystem.cpp
    src/Core/Threading/RenderCommandQueue.cpp
    src/Core/Threading/RenderFrameQueue.cpp
    src/Core/Threading/RenderThread.cpp
    src/Core/Threading/ThreadContext.cpp
    src/Engine.cpp
    src/Game/PlaySession.cpp
    src/Game/Scene.cpp
    src/Game/SceneSerializer.cpp
    src/Gameplay/CameraComponent.cpp
    src/Gameplay/Component.cpp
    src/Gameplay/GameObject.cpp
    src/Gameplay/LightComponent.cpp
    src/Gameplay/MeshRendererComponent.cpp
    src/Gameplay/Object.cpp
    src/Gameplay/ScriptComponent.cpp
    src/ISystem.cpp
    src/Platform/WindowSubsystem.cpp
    src/Project/ProjectDescriptor.cpp
    src/Project/ProjectGenerator.cpp
    src/Project/ProjectPaths.cpp
    src/Project/ProjectSession.cpp
    src/Reflection/Class.cpp
    src/Reflection/Json/ReflectionJson.cpp
    src/Reflection/NativeReflectionBootstrap.cpp
    src/Reflection/PendingRegistry.cpp
    src/Reflection/PropertyAccess.cpp
    src/Reflection/ReflectedValue.cpp
    src/Reflection/ReflectionAnchor.cpp
    src/Reflection/ReflectionSubsystem.cpp
    src/Rendering/RHI/RenderDevice.cpp
    src/Rendering/RHI/Renderer.cpp
    src/Rendering/RHI/ImGuiOverlayPass.cpp
    src/Rendering/RHI/EnvironmentLighting.cpp
    src/Rendering/RenderView.cpp
    src/Rendering/RHI/GpuBuffer.cpp
    src/Rendering/RHI/RenderMesh.cpp
    src/Rendering/RHI/RenderResourceManager.cpp
    src/Rendering/SceneExtractor.cpp
    src/Scripting/ScriptingSubsystem.cpp
    src/Story/StoryDocumentIO.cpp
    src/Story/StoryRuntime.cpp
)

set(ENGINE_HDR
    inc/Assets/AssetGpuUploader.h
    inc/Assets/AssetHandle.h
    inc/Assets/AssetManager.h
    inc/Assets/AssetMetadata.h
    inc/Assets/AssetPath.h
    inc/Assets/AssetRef.h
    inc/Assets/AssetRegistry.h
    inc/Assets/AssetTypes.h
    inc/Assets/ContentHash.h
    inc/Assets/Guid.h
    inc/Assets/SceneAssetResolver.h
    inc/Assets/Loaders/IAssetLoader.h
    inc/Assets/Loaders/MaterialLoader.h
    inc/Assets/Loaders/ModelLoader.h
    inc/Assets/Loaders/SkeletalLoader.h
    inc/Assets/Loaders/TextureLoader.h
    inc/Assets/Resources/AnimationClipResource.h
    inc/Assets/Resources/MaterialResource.h
    inc/Assets/Resources/ModelResource.h
    inc/Assets/Resources/SkeletalMeshResource.h
    inc/Assets/Resources/SkeletonResource.h
    inc/Assets/Resources/SkinBinding.h
    inc/Assets/Resources/StaticMeshResource.h
    inc/Assets/Resources/TextureResource.h
    inc/Core/EnginePaths.h
    inc/Core/MemorySubsystem.h
    inc/Core/Subsystem.h
    inc/Core/Threading/JobSystem.h
    inc/Core/Threading/RenderCommandQueue.h
    inc/Core/Threading/RenderFrameData.h
    inc/Core/Threading/RenderFrameQueue.h
    inc/Core/Threading/RenderThread.h
    inc/Core/Threading/ThreadContext.h
    inc/Core/Threading/ThreadRole.h
    inc/Core/Transform.h
    inc/Core/Types.h
    inc/Engine.h
    inc/Game/PlaySession.h
    inc/Game/Scene.h
    inc/Game/SceneSerializer.h
    inc/Gameplay/CameraComponent.h
    inc/Gameplay/Component.h
    inc/Gameplay/GameObject.h
    inc/Gameplay/LightComponent.h
    inc/Gameplay/LightSettings.h
    inc/Gameplay/MeshRendererComponent.h
    inc/Gameplay/Object.h
    inc/Gameplay/ObjectHandle.h
    inc/Gameplay/ScriptComponent.h
    inc/ISystem.h
    inc/Platform/GraphicsBackend.h
    inc/Platform/NativeWindowInfo.h
    inc/Platform/WindowSubsystem.h
    inc/Project/ProjectDescriptor.h
    inc/Project/ProjectGenerator.h
    inc/Project/ProjectPaths.h
    inc/Project/ProjectSession.h
    inc/Reflection/Class.h
    inc/Reflection/ClassRef.h
    inc/Reflection/EnumDescriptor.h
    inc/Reflection/Json/ReflectionJson.h
    inc/Reflection/PendingRegistry.h
    inc/Reflection/PropertyAccess.h
    inc/Reflection/PropertyDescriptor.h
    inc/Reflection/PropertyFlags.h
    inc/Reflection/ReflectedValue.h
    inc/Reflection/ReflectionMacros.h
    inc/Reflection/ReflectionSubsystem.h
    inc/Reflection/ReflectionValueCodec.h
    inc/Reflection/TypeDescriptor.h
    inc/Reflection/TypeId.h
    src/Rendering/RHI/EnvironmentLighting.h
    inc/Rendering/RenderMaterial.h
    inc/Rendering/RenderSettings.h
    inc/Rendering/Visibility.h
    inc/Rendering/AxisAlignedBounds.h
    inc/Rendering/RenderCamera.h
    src/Rendering/RHI/RenderContext.h
    src/Rendering/RHI/RenderDevice.h
    src/Rendering/RHI/Renderer.h
    inc/Rendering/RenderView.h
    src/Rendering/RHI/RenderSurface.h
    inc/Rendering/RenderLight.h
    inc/Rendering/RenderObject.h
    inc/Rendering/RenderResourceHandles.h
    inc/Rendering/RenderScene.h
    src/Rendering/RHI/GpuBuffer.h
    src/Rendering/RHI/RenderMesh.h
    src/Rendering/RHI/RenderResourceManager.h
    inc/Rendering/Resources/RenderVertex.h
    inc/Rendering/SceneExtractor.h
    inc/Scripting/ScriptingSubsystem.h
    inc/Story/StoryDocumentIO.h
    inc/Story/StoryRuntime.h
    inc/Story/StoryTypes.h
)

add_library(Engine STATIC ${ENGINE_SRC} ${ENGINE_HDR}
    inc/UI/RenderViewportWidget.h
    src/UI/RenderViewportWidget.cpp
    inc/UI/StoryWidget.h
    src/UI/StoryWidget.cpp
)
set_target_properties(Engine PROPERTIES AUTOMOC ON)

target_include_directories(Engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_include_directories(Engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_sources(Engine PRIVATE ThirdParty/SimpleMath/SimpleMath.cpp)
target_include_directories(Engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/SimpleMath
)

if(NOT WIN32)
    target_include_directories(Engine PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/DirectXMath
    )
endif()

target_link_libraries(Engine PUBLIC Sakura::EngineThirdParty)

target_compile_definitions(Engine
PUBLIC
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(SAKURA_PYTHON_AVAILABLE)
    target_compile_definitions(Engine PUBLIC SAKURA_ENABLE_PYTHON=1)
endif()

# ---------------------------------------------------------------
# Stage engine runtime assets next to an executable / into Stage/
# Layout:
#   <StageRoot>/Bin/<exe>
#   <StageRoot>/Engine/Content
#   <StageRoot>/Engine/Shaders
#   <StageRoot>/Engine/Config
# ---------------------------------------------------------------
function(sakura_stage_engine_runtime TargetName)
    set(SakuraStageRoot "${CMAKE_BINARY_DIR}/Stage")
    set(SakuraEngineSourceContent "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Content")
    set(SakuraEngineSourceShaders "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/shaders")
    set(SakuraEngineSourceConfig "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Config")

    add_custom_command(TARGET ${TargetName} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${SakuraStageRoot}/Bin"
            "${SakuraStageRoot}/Engine/Content"
            "${SakuraStageRoot}/Engine/Shaders"
            "${SakuraStageRoot}/Engine/Config"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SakuraEngineSourceShaders}"
            "${SakuraStageRoot}/Engine/Shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SakuraEngineSourceContent}"
            "${SakuraStageRoot}/Engine/Content"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SakuraEngineSourceConfig}"
            "${SakuraStageRoot}/Engine/Config"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${TargetName}>"
            "${SakuraStageRoot}/Bin/"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${TargetName}>/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SakuraEngineSourceShaders}"
            "$<TARGET_FILE_DIR:${TargetName}>/shaders"
        COMMENT "Stage engine runtime for ${TargetName}"
        VERBATIM
    )
endfunction()

if(WIN32)
    target_compile_definitions(Engine PRIVATE PLATFORM_WIN32=1 D3D12_SUPPORTED=1)
endif()
target_compile_definitions(Engine PRIVATE ENGINE_DLL=1 VULKAN_SUPPORTED=1 GL_SUPPORTED=1)
