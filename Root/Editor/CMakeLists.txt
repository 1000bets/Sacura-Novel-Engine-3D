# ---------------------------------------------------------------
# Editor (Qt UI layer) РІР‚вЂќ QWidget shell, adapters, viewport host
# Logical layer: QtEditorFramework. CMake target: Editor.
# ---------------------------------------------------------------
if(NOT SAKURA_BUILD_EDITOR)
    return()
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(EDITOR_SRC
    src/Editor.cpp
    src/Panels/AssetImportDialog.cpp
    src/Panels/ContentBrowserWidget.cpp
    src/Shell/EditorMainWindow.cpp
    src/Shell/EditorMainWindowSupport.cpp
    src/Shell/EditorMainWindowLayout.cpp
    src/Shell/EditorMainWindowWorkspace.cpp
    src/Shell/EditorMainWindowSelection.cpp
    src/Shell/EditorMainWindowClipboard.cpp
    src/Shell/EditorMainWindowActions.cpp
    src/Shell/EditorMainWindowSimulation.cpp
    src/Shell/EditorMainWindowViewport.cpp
    src/Shell/EditorMainWindowAssets.cpp
    src/Panels/HierarchyTreeWidget.cpp
    src/EditorReflectionAnchor.cpp
    src/Viewport/EditorViewportWidget.cpp
    src/Launcher/ProjectBrowserDialog.cpp
    src/Panels/ReflectionInspector.cpp
    src/Launcher/TemplateCardWidget.cpp
)

set(EDITOR_HDR
    inc/Editor.h
    inc/Actions/BuiltinEditorActions.h
    inc/Panels/AssetImportDialog.h
    inc/Panels/HierarchyTreeWidget.h
    inc/Panels/ContentBrowserWidget.h
    inc/Panels/EditorAssetMime.h
    inc/Shell/EditorClipboard.h
    inc/Shell/EditorMainWindow.h
    inc/EditorReflectionAnchor.h
    inc/Shell/EditorTheme.h
    inc/Viewport/EditorViewportWidget.h
    inc/Launcher/ProjectBrowserDialog.h
    inc/Panels/ReflectionInspector.h
    inc/Launcher/TemplateCardWidget.h
)

set(EDITOR_RESOURCES
    resources/studio.qrc
)

add_library(Editor STATIC ${EDITOR_SRC} ${EDITOR_HDR} ${EDITOR_RESOURCES})
add_library(Sakura::Editor ALIAS Editor)

target_include_directories(Editor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_include_directories(Editor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_link_libraries(Editor PUBLIC
    EditorCore
)
target_link_libraries(Editor PRIVATE
    Sakura::EditorThirdParty
)

target_compile_definitions(Editor PUBLIC
    SAKURA_EDITOR_RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources/studio"
)

target_sources(Editor PRIVATE src/Panels/MaterialBindingsDialog.cpp)
