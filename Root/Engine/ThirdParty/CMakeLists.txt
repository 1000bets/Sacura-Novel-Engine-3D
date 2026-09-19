# ---------------------------------------------------------------
# Engine third-party: SDL3, Diligent stack, ImGui, PhysX, asset I/O
# Fetched at configure time into this tree; pinned tags — bump deliberately.
# ---------------------------------------------------------------

include(FetchContent)

set(SAKURA_SDL3_GIT_TAG "release-3.4.16" CACHE STRING "SDL3 git tag")
set(SAKURA_DILIGENT_CORE_GIT_TAG "v2.5.6" CACHE STRING "DiligentCore git tag")
set(SAKURA_DILIGENT_TOOLS_GIT_TAG "v2.5.6" CACHE STRING "DiligentTools git tag")
set(SAKURA_DILIGENT_FX_GIT_TAG "v2.5.6" CACHE STRING "DiligentFX git tag")
set(SAKURA_DILIGENT_SAMPLES_GIT_TAG "v2.5.6" CACHE STRING "DiligentSamples git tag")
set(SAKURA_PHYSX_GIT_TAG "107.3-physx-5.6.1" CACHE STRING "NVIDIA PhysX git tag")
option(SAKURA_BUILD_DILIGENT_SAMPLES "Build Diligent sample/tutorial apps" OFF)

# ---- SDL3 ----
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        ${SAKURA_SDL3_GIT_TAG}
    GIT_SHALLOW    TRUE
    EXCLUDE_FROM_ALL
)

# ---- Diligent modules (SOURCE_DIR name must match module name) ----
set(DILIGENT_NO_FORMAT_VALIDATION ON CACHE BOOL "" FORCE)
set(DILIGENT_NO_RENDER_STATE_PACKAGER ON CACHE BOOL "" FORCE)
set(DILIGENT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DILIGENT_NO_DIRECT3D11 ON CACHE BOOL "" FORCE)
set(DILIGENT_NO_WEBGPU ON CACHE BOOL "" FORCE)
set(DILIGENT_INSTALL_CORE OFF CACHE BOOL "" FORCE)
set(DILIGENT_INSTALL_TOOLS OFF CACHE BOOL "" FORCE)
set(DILIGENT_INSTALL_FX OFF CACHE BOOL "" FORCE)
set(DILIGENT_INSTALL_SAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    DiligentCore
    GIT_REPOSITORY https://github.com/DiligentGraphics/DiligentCore.git
    GIT_TAG        ${SAKURA_DILIGENT_CORE_GIT_TAG}
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/DiligentCore
)

FetchContent_Declare(
    DiligentTools
    GIT_REPOSITORY https://github.com/DiligentGraphics/DiligentTools.git
    GIT_TAG        ${SAKURA_DILIGENT_TOOLS_GIT_TAG}
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/DiligentTools
)

FetchContent_Declare(
    DiligentFX
    GIT_REPOSITORY https://github.com/DiligentGraphics/DiligentFX.git
    GIT_TAG        ${SAKURA_DILIGENT_FX_GIT_TAG}
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/DiligentFX
)

FetchContent_Declare(
    DiligentSamples
    GIT_REPOSITORY https://github.com/DiligentGraphics/DiligentSamples.git
    GIT_TAG        ${SAKURA_DILIGENT_SAMPLES_GIT_TAG}
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/DiligentSamples
)

# ---- PhysX SDK (NVIDIA-Omniverse) ----
FetchContent_Declare(
    PhysX
    GIT_REPOSITORY https://github.com/NVIDIA-Omniverse/PhysX.git
    GIT_TAG        ${SAKURA_PHYSX_GIT_TAG}
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/PhysX
)

FetchContent_MakeAvailable(SDL3 DiligentCore)

# DiligentTools embeds its own ImGui into Diligent-Imgui when target "imgui" is absent.
FetchContent_MakeAvailable(DiligentTools DiligentFX)

add_subdirectory(imgui)

if(SAKURA_BUILD_DILIGENT_SAMPLES)
    FetchContent_MakeAvailable(DiligentSamples)
else()
    FetchContent_GetProperties(DiligentSamples)
    if(NOT diligentsamples_POPULATED)
        FetchContent_Populate(DiligentSamples)
    endif()
endif()

# ---- PhysX build (CPU SDK; GPU projects off) ----
FetchContent_GetProperties(PhysX)
if(NOT physx_POPULATED)
    FetchContent_Populate(PhysX)
endif()

set(PHYSX_ROOT_DIR "${physx_SOURCE_DIR}/physx" CACHE PATH "PhysX root" FORCE)
set(PX_OUTPUT_LIB_DIR "${CMAKE_BINARY_DIR}/PhysX" CACHE PATH "" FORCE)
set(PX_OUTPUT_BIN_DIR "${CMAKE_BINARY_DIR}/PhysX" CACHE PATH "" FORCE)
set(PX_BUILDSNIPPETS OFF CACHE BOOL "" FORCE)
set(PX_BUILDPVDRUNTIME OFF CACHE BOOL "" FORCE)
set(PX_GENERATE_STATIC_LIBRARIES ON CACHE BOOL "" FORCE)
set(PX_GENERATE_GPU_PROJECTS OFF CACHE BOOL "" FORCE)
set(PX_GENERATE_GPU_PROJECTS_ONLY OFF CACHE BOOL "" FORCE)
set(PX_COPY_EXTERNAL_DLL OFF CACHE BOOL "" FORCE)
set(NV_USE_STATIC_WINCRT OFF CACHE BOOL "" FORCE)
set(NV_USE_DEBUG_WINCRT ON CACHE BOOL "" FORCE)

# PhysX public CMake still tries to copy freeglut when PUBLIC_RELEASE=1.
# Snippets are disabled; placeholders satisfy the FILE(COPY) calls.
set(SAKURA_PHYSX_EXTERNAL_BIN "${CMAKE_BINARY_DIR}/PhysXExternal")
file(MAKE_DIRECTORY "${SAKURA_PHYSX_EXTERNAL_BIN}/win64")
file(TOUCH "${SAKURA_PHYSX_EXTERNAL_BIN}/win64/freeglut.dll")
file(TOUCH "${SAKURA_PHYSX_EXTERNAL_BIN}/win64/freeglutd.dll")
set(PHYSX_SLN_FREEGLUT_PATH "${SAKURA_PHYSX_EXTERNAL_BIN}" CACHE INTERNAL "PhysX freeglut copy path" FORCE)

if(WIN32)
    set(TARGET_BUILD_PLATFORM "windows" CACHE INTERNAL "" FORCE)
elseif(APPLE)
    set(TARGET_BUILD_PLATFORM "mac" CACHE INTERNAL "" FORCE)
else()
    set(TARGET_BUILD_PLATFORM "linux" CACHE INTERNAL "" FORCE)
endif()

if(NOT TARGET PhysX)
    add_subdirectory(
        ${PHYSX_ROOT_DIR}/compiler/public
        ${CMAKE_BINARY_DIR}/PhysXSDK
    )
endif()

add_library(SakuraPhysX INTERFACE)
add_library(Sakura::PhysX ALIAS SakuraPhysX)

set(SAKURA_PHYSX_TARGETS
    PhysX
    PhysXCommon
    PhysXFoundation
    PhysXCooking
    PhysXExtensions
    PhysXPvdSDK
    PhysXCharacterKinematic
    PhysXVehicle
)

foreach(PhysXTarget ${SAKURA_PHYSX_TARGETS})
    if(TARGET ${PhysXTarget})
        target_link_libraries(SakuraPhysX INTERFACE ${PhysXTarget})
    elseif(TARGET ${PhysXTarget}_static)
        target_link_libraries(SakuraPhysX INTERFACE ${PhysXTarget}_static)
    endif()
endforeach()

target_include_directories(SakuraPhysX INTERFACE
    ${PHYSX_ROOT_DIR}/include
)

# ---- Asset pipeline (fastgltf, ozz, stb_image, nlohmann/json) ----
add_subdirectory(asset)

# ---- Python scripting (pybind11 embed) ----
set(SAKURA_PYBIND11_GIT_TAG "v2.13.6" CACHE STRING "pybind11 git tag")
option(SAKURA_ENABLE_PYTHON "Enable Python scripting via pybind11" ON)
set(SAKURA_PYTHON_ROOT "" CACHE PATH "Optional Python root hint for Development/Embed")

set(SAKURA_PYTHON_AVAILABLE OFF CACHE INTERNAL "Python embed available for Sakura")
if(SAKURA_ENABLE_PYTHON)
    if(SAKURA_PYTHON_ROOT)
        set(Python3_ROOT_DIR "${SAKURA_PYTHON_ROOT}")
    elseif(DEFINED ENV{SAKURA_PYTHON_ROOT})
        set(Python3_ROOT_DIR "$ENV{SAKURA_PYTHON_ROOT}")
    endif()

    find_package(Python3 COMPONENTS Interpreter Development.Embed QUIET)
    if(NOT Python3_Development_Embed_FOUND)
        find_package(Python3 COMPONENTS Interpreter Development QUIET)
    endif()

    if(Python3_Development_Embed_FOUND OR Python3_Development_FOUND)
        set(SAKURA_PYTHON_AVAILABLE ON CACHE INTERNAL "Python embed available for Sakura" FORCE)
        message(STATUS "Sakura Python: ${Python3_EXECUTABLE} (${Python3_VERSION})")
    else()
        message(WARNING "SAKURA_ENABLE_PYTHON=ON but Python3 Development/Embed was not found; building without Python")
        set(SAKURA_ENABLE_PYTHON OFF CACHE BOOL "Enable Python scripting via pybind11" FORCE)
    endif()
endif()

if(SAKURA_PYTHON_AVAILABLE)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG        ${SAKURA_PYBIND11_GIT_TAG}
        GIT_SHALLOW    TRUE
        SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/pybind11/_src
    )
    set(PYBIND11_FINDPYTHON ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(pybind11)
endif()

# ---- Aggregate for Engine ----
add_library(SakuraEngineThirdParty INTERFACE)
add_library(Sakura::EngineThirdParty ALIAS SakuraEngineThirdParty)

target_link_libraries(SakuraEngineThirdParty INTERFACE
    SDL3::SDL3
    Diligent-BuildSettings
    Sakura::ImGui
    Sakura::PhysX
    Sakura::AssetThirdParty
)

if(SAKURA_PYTHON_AVAILABLE)
    if(TARGET Python3::Python)
        target_link_libraries(SakuraEngineThirdParty INTERFACE
            pybind11::embed
            Python3::Python
        )
    else()
        target_link_libraries(SakuraEngineThirdParty INTERFACE
            pybind11::embed
        )
    endif()
    target_compile_definitions(SakuraEngineThirdParty INTERFACE SAKURA_ENABLE_PYTHON=1)
endif()

if(TARGET Diligent-GraphicsEngineD3D12-shared)
    target_link_libraries(SakuraEngineThirdParty INTERFACE Diligent-GraphicsEngineD3D12-shared)
endif()
if(TARGET Diligent-GraphicsEngineVk-shared)
    target_link_libraries(SakuraEngineThirdParty INTERFACE Diligent-GraphicsEngineVk-shared)
endif()
if(TARGET Diligent-GraphicsEngineOpenGL-shared)
    target_link_libraries(SakuraEngineThirdParty INTERFACE Diligent-GraphicsEngineOpenGL-shared)
endif()
if(TARGET DiligentFX)
    target_link_libraries(SakuraEngineThirdParty INTERFACE DiligentFX)
endif()
if(TARGET Diligent-TextureLoader)
    target_link_libraries(SakuraEngineThirdParty INTERFACE Diligent-TextureLoader)
endif()
if(TARGET Diligent-AssetLoader)
    target_link_libraries(SakuraEngineThirdParty INTERFACE Diligent-AssetLoader)
endif()
if(TARGET Diligent-Imgui)
    target_link_libraries(SakuraEngineThirdParty INTERFACE Diligent-Imgui)
endif()
# Diligent-Imgui uses Tools' bundled ImGui (~1.89). Sakura::ImGui is docking 1.92+ for our UI.

target_include_directories(SakuraEngineThirdParty INTERFACE
    ${diligentcore_SOURCE_DIR}
    ${diligenttools_SOURCE_DIR}
    ${diligentfx_SOURCE_DIR}
)

function(sakura_copy_engine_runtime_dlls TargetName)
    if(COMMAND copy_required_dlls)
        copy_required_dlls(${TargetName})
    endif()
endfunction()
