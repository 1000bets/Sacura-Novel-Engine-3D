# ---------------------------------------------------------------
# Engine-owned third-party (SDL3, Diligent, SimpleMath, …)
# ---------------------------------------------------------------
add_subdirectory(ThirdParty)

# ---------------------------------------------------------------
# Engine library
# ---------------------------------------------------------------
file(GLOB_RECURSE ENGINE_SRC src/*.cpp)
file(GLOB_RECURSE ENGINE_HDR inc/*.h)

add_library(Engine STATIC ${ENGINE_SRC} ${ENGINE_HDR})

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
    PLATFORM_WIN32=1
    ENGINE_DLL=1
    D3D12_SUPPORTED=1
    VULKAN_SUPPORTED=1
    GL_SUPPORTED=1
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
