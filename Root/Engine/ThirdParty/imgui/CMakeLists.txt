# ---------------------------------------------------------------
# Dear ImGui (official docking line) — Sakura's copy for Engine UI.
# Target must NOT be named "imgui" so DiligentTools can use its
# bundled ImGui for Diligent-Imgui without API conflicts.
# ---------------------------------------------------------------

include(FetchContent)

set(SAKURA_IMGUI_GIT_TAG "v1.92.9b-docking" CACHE STRING "Dear ImGui git tag (docking)")

FetchContent_Declare(
    SakuraImGuiSources
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        ${SAKURA_IMGUI_GIT_TAG}
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/_src
)

FetchContent_MakeAvailable(SakuraImGuiSources)

add_library(SakuraImGui STATIC
    ${sakuraimguisources_SOURCE_DIR}/imgui.cpp
    ${sakuraimguisources_SOURCE_DIR}/imgui_draw.cpp
    ${sakuraimguisources_SOURCE_DIR}/imgui_tables.cpp
    ${sakuraimguisources_SOURCE_DIR}/imgui_widgets.cpp
    ${sakuraimguisources_SOURCE_DIR}/imgui_demo.cpp
)

add_library(Sakura::ImGui ALIAS SakuraImGui)

target_include_directories(SakuraImGui PUBLIC
    ${sakuraimguisources_SOURCE_DIR}
)

target_compile_features(SakuraImGui PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(SakuraImGui PRIVATE /W3)
endif()
