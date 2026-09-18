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
