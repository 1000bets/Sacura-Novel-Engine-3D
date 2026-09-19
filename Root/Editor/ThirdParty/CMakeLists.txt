# ---------------------------------------------------------------
# Editor-owned third-party dependencies.
# Do not re-add Engine deps (SDL3, Diligent, asset runtime) here.
# ---------------------------------------------------------------

add_subdirectory(ufbx)

# ---- Qt (Core / Gui / Widgets) — Editor UI ----
# Prefer an existing Qt6 install. Optional local aqt tree:
#   Root/Editor/ThirdParty/Qt/<version>/<arch>
set(SAKURA_QT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Qt" CACHE PATH
    "Qt install root (aqt layout: <root>/<version>/<arch>)")

if(DEFINED ENV{SAKURA_QT_ROOT} AND NOT "$ENV{SAKURA_QT_ROOT}" STREQUAL "")
    set(SAKURA_QT_ROOT "$ENV{SAKURA_QT_ROOT}" CACHE PATH "Qt install root" FORCE)
endif()

if(NOT Qt6_DIR)
    file(GLOB SAKURA_QT_CMAKE_CANDIDATES
        "${SAKURA_QT_ROOT}/*/msvc*_64/lib/cmake/Qt6"
        "${SAKURA_QT_ROOT}/*/mingw_64/lib/cmake/Qt6"
        "${SAKURA_QT_ROOT}/lib/cmake/Qt6"
    )
    if(SAKURA_QT_CMAKE_CANDIDATES)
        list(GET SAKURA_QT_CMAKE_CANDIDATES 0 SAKURA_QT_CMAKE_DIR)
        set(Qt6_DIR "${SAKURA_QT_CMAKE_DIR}" CACHE PATH "Qt6 CMake package dir" FORCE)
        get_filename_component(SAKURA_QT_PREFIX "${SAKURA_QT_CMAKE_DIR}/../../.." ABSOLUTE)
        list(PREPEND CMAKE_PREFIX_PATH "${SAKURA_QT_PREFIX}")
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# find_package(Qt6) creates directory-scoped imported targets; promote so
# Root executables can use sakura_deploy_qt_runtime() after add_subdirectory(Editor).
foreach(SakuraQtModule Core Gui Widgets)
    if(TARGET Qt6::${SakuraQtModule})
        get_target_property(SakuraQtIsGlobal Qt6::${SakuraQtModule} IMPORTED_GLOBAL)
        if(NOT SakuraQtIsGlobal)
            set_target_properties(Qt6::${SakuraQtModule} PROPERTIES IMPORTED_GLOBAL TRUE)
        endif()
    endif()
endforeach()

add_library(SakuraEditorThirdParty INTERFACE)
add_library(Sakura::EditorThirdParty ALIAS SakuraEditorThirdParty)

target_link_libraries(SakuraEditorThirdParty INTERFACE
    Sakura::Ufbx
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

function(sakura_deploy_qt_runtime TargetName)
    if(NOT WIN32)
        return()
    endif()
    if(NOT TARGET Qt6::Core OR NOT TARGET Qt6::Gui OR NOT TARGET Qt6::Widgets)
        message(WARNING "sakura_deploy_qt_runtime(${TargetName}): Qt6 Core/Gui/Widgets targets missing")
        return()
    endif()
    if(NOT Qt6_DIR)
        message(WARNING "sakura_deploy_qt_runtime(${TargetName}): Qt6_DIR is empty")
        return()
    endif()

    get_filename_component(SAKURA_QT_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)
    set(SAKURA_QT_PLATFORMS_DIR "${SAKURA_QT_PREFIX}/plugins/platforms")
    set(SAKURA_QT_IMAGEFORMATS_DIR "${SAKURA_QT_PREFIX}/plugins/imageformats")

    add_custom_command(TARGET ${TargetName} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${TargetName}>/platforms"
            "$<TARGET_FILE_DIR:${TargetName}>/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::Core>"
            "$<TARGET_FILE:Qt6::Gui>"
            "$<TARGET_FILE:Qt6::Widgets>"
            "$<TARGET_FILE_DIR:${TargetName}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SAKURA_QT_PLATFORMS_DIR}/$<IF:$<CONFIG:Debug>,qwindowsd.dll,qwindows.dll>"
            "$<TARGET_FILE_DIR:${TargetName}>/platforms/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SAKURA_QT_IMAGEFORMATS_DIR}/$<IF:$<CONFIG:Debug>,qjpegd.dll,qjpeg.dll>"
            "$<TARGET_FILE_DIR:${TargetName}>/imageformats/"
        COMMENT "Copy Qt Core/Gui/Widgets runtime next to ${TargetName}"
        VERBATIM
    )
endfunction()
