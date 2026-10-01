# ---------------------------------------------------------------
# Engine-owned third-party (SDL3, Diligent, SimpleMath, РІР‚В¦)
# ---------------------------------------------------------------
add_subdirectory(ThirdParty)

# ---------------------------------------------------------------
# Engine library (explicit sources РІР‚вЂќ do not use file(GLOB) for compilation units)
# ---------------------------------------------------------------
set(ENGINE_SRC
    src/Rendering/Assets/AssetGpuUploader.cpp
    src/Assets/AssetManager.cpp
    src/Assets/AssetMetadata.cpp
    src/Assets/AssetPath.cpp
    src/Assets/AssetRegistry.cpp
    src/Assets/AssetTypes.cpp
    src/Assets/ContentHash.cpp
    src/Assets/Guid.cpp
    src/World/Resources/SceneAssetResolver.cpp
    src/Assets/SurfaceVertices.cpp
    src/Assets/Loaders/MaterialLoader.cpp
    src/Assets/Loaders/ModelLoader.cpp
    src/Assets/Loaders/SkeletalLoader.cpp
    src/Assets/Loaders/TextureLoader.cpp
    src/Core/IO/AtomicFileWriter.cpp
    src/Core/EnginePaths.cpp
    src/Core/Object/MemorySubsystem.cpp
    src/Core/Subsystem.cpp
    src/Core/Threading/JobSystem.cpp
    src/Rendering/Threading/RenderCommandQueue.cpp
    src/Rendering/Threading/RenderFrameQueue.cpp
    src/Rendering/Threading/RenderThread.cpp
    src/Core/Threading/ThreadContext.cpp
    src/Engine.cpp
    src/World/Simulation/PlaySession.cpp
    src/World/Scene.cpp
    src/World/Serialization/SceneSerializer.cpp
    src/World/Components/CameraComponent.cpp
    src/World/Components/Component.cpp
    src/World/GameObject.cpp
    src/World/Components/LightComponent.cpp
    src/World/Components/MeshRendererComponent.cpp
    src/Core/Object/Object.cpp
    src/World/Components/ScriptComponent.cpp
    src/Materials/MaterialBindings.cpp
    src/Materials/MaterialDefinition.cpp
    src/Materials/MaterialDocumentIO.cpp
    src/Materials/DynamicMaterialInstance.cpp
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
    src/Rendering/PresentationController.cpp
    src/Rendering/RenderView.cpp
    src/Rendering/RHI/GpuBuffer.cpp
    src/Rendering/RHI/RenderMesh.cpp
    src/Rendering/RHI/RenderResourceManager.cpp
    src/Rendering/SceneExtractor.cpp
    src/Scripting/PythonBindings.cpp
    src/Scripting/ScriptLifecycle.cpp
    src/Scripting/ScriptingSubsystem.cpp
    src/Story/StoryDocumentIO.cpp
    src/Story/StoryRuntime.cpp
)

set(ENGINE_HDR
    inc/Rendering/Assets/AssetGpuUploader.h
    inc/Assets/AssetHandle.h
    inc/Assets/AssetManager.h
    inc/Assets/AssetMetadata.h
    inc/Assets/AssetPath.h
    inc/Assets/AssetRef.h
    inc/Assets/AssetRegistry.h
    inc/Assets/AssetTypes.h
    inc/Assets/ContentHash.h
    inc/Assets/Guid.h
    inc/World/Resources/SceneAssetResolver.h
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
    inc/Core/IO/AtomicFileWriter.h
    inc/Core/EnginePaths.h
    inc/Core/Object/MemorySubsystem.h
    inc/Core/Subsystem.h
    inc/Core/Threading/JobSystem.h
    inc/Rendering/Threading/RenderCommandQueue.h
    inc/Rendering/Frame/RenderFrameData.h
    inc/Rendering/Threading/RenderFrameQueue.h
    inc/Rendering/Threading/RenderThread.h
    inc/Core/Threading/ThreadContext.h
    inc/Core/Threading/ThreadRole.h
    inc/Core/Transform.h
    inc/Core/Types.h
    inc/Engine.h
    inc/World/Simulation/PlaySession.h
    inc/World/Scene.h
    inc/World/Serialization/SceneSerializer.h
    inc/World/Components/CameraComponent.h
    inc/World/Components/Component.h
    inc/World/GameObject.h
    inc/World/Components/LightComponent.h
    inc/World/Components/LightSettings.h
    inc/World/Components/MeshRendererComponent.h
    inc/Core/Object/Object.h
    inc/Core/Object/ObjectHandle.h
    inc/World/Components/ScriptComponent.h
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
    inc/Rendering/PresentationController.h
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
    inc/UI/MaterialWidget.h
    src/UI/MaterialWidget.cpp
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

target_link_libraries(Engine
    PUBLIC Sakura::EngineQtThirdParty nlohmann_json::nlohmann_json ozz_animation
    PRIVATE Sakura::EngineThirdParty
)

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

    add_custom_target(${TargetName}RuntimeAssets
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${SakuraStageRoot}/Engine/Content"
            "${SakuraStageRoot}/Engine/Shaders"
            "${SakuraStageRoot}/Engine/Config"
            "$<TARGET_FILE_DIR:${TargetName}>/Engine/Content"
            "$<TARGET_FILE_DIR:${TargetName}>/Engine/Shaders"
            "$<TARGET_FILE_DIR:${TargetName}>/Engine/Config"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SakuraEngineSourceShaders}"
            "${SakuraStageRoot}/Engine/Shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SakuraEngineSourceContent}"
            "${SakuraStageRoot}/Engine/Content"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SakuraEngineSourceConfig}"
            "${SakuraStageRoot}/Engine/Config"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SakuraEngineSourceShaders}"
            "$<TARGET_FILE_DIR:${TargetName}>/Engine/Shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SakuraEngineSourceContent}"
            "$<TARGET_FILE_DIR:${TargetName}>/Engine/Content"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SakuraEngineSourceConfig}"
            "$<TARGET_FILE_DIR:${TargetName}>/Engine/Config"
        COMMENT "Deploy engine assets for ${TargetName}"
        VERBATIM
    )
    add_dependencies(${TargetName} ${TargetName}RuntimeAssets)

    add_custom_command(TARGET ${TargetName} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SakuraStageRoot}/Bin"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${TargetName}>"
            "${SakuraStageRoot}/Bin/"
        COMMENT "Stage engine executable for ${TargetName}"
        VERBATIM
    )
endfunction()

if(WIN32)
    target_compile_definitions(Engine PRIVATE PLATFORM_WIN32=1 D3D12_SUPPORTED=1)
endif()
target_compile_definitions(Engine PRIVATE ENGINE_DLL=1 VULKAN_SUPPORTED=1 GL_SUPPORTED=1)

target_sources(Engine PRIVATE src/Rendering/RHI/MaterialShaderCompiler.cpp)
target_link_libraries(Engine PRIVATE Diligent-ShaderTools spirv-cross-glsl)
