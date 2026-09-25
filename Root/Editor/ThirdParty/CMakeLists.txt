add_subdirectory(ufbx)

add_library(SakuraEditorCoreThirdParty INTERFACE)
add_library(Sakura::EditorCoreThirdParty ALIAS SakuraEditorCoreThirdParty)
target_link_libraries(SakuraEditorCoreThirdParty INTERFACE Sakura::Ufbx)

if(SAKURA_BUILD_EDITOR)
    include(FetchContent)

    set(SAKURA_IMGUIZMO_GIT_TAG "1.83" CACHE STRING "ImGuizmo git tag")
    FetchContent_Declare(
        ImGuizmo
        GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
        GIT_TAG        ${SAKURA_IMGUIZMO_GIT_TAG}
        GIT_SHALLOW    TRUE
        SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/ImGuizmo
    )
    FetchContent_GetProperties(ImGuizmo)
    if(NOT imguizmo_POPULATED)
        FetchContent_Populate(ImGuizmo)
    endif()

    add_library(SakuraImGuizmo STATIC
        ${imguizmo_SOURCE_DIR}/ImGuizmo.cpp
    )
    add_library(Sakura::ImGuizmo ALIAS SakuraImGuizmo)
    target_include_directories(SakuraImGuizmo PUBLIC ${imguizmo_SOURCE_DIR})
    target_link_libraries(SakuraImGuizmo PUBLIC Diligent-Imgui)

    add_library(SakuraEditorThirdParty INTERFACE)
    add_library(Sakura::EditorThirdParty ALIAS SakuraEditorThirdParty)
    target_link_libraries(SakuraEditorThirdParty INTERFACE Sakura::ImGuizmo)
endif()
