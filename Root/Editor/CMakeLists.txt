# ---------------------------------------------------------------
# Editor dependencies (owned by Editor)
# ---------------------------------------------------------------
add_subdirectory(ThirdParty)

# ---------------------------------------------------------------
# EditorCore — application-independent import, document and undo operations
# Logical layer: EditorCore. CMake target: EditorCore.
# ---------------------------------------------------------------
set(EDITOR_CORE_SRC
    src/AssetTools/AssetImporter.cpp
    src/AssetTools/FbxToGlbConverter.cpp
    src/AssetTools/GlbImporter.cpp
    src/AssetTools/ImageImporter.cpp
    src/EditorCommandStack.cpp
    src/EditorCommands.cpp
    src/SceneDocument.cpp
)

set(EDITOR_CORE_HDR
    inc/AssetTools/AssetImporter.h
    inc/AssetTools/FbxToGlbConverter.h
    inc/AssetTools/GlbImporter.h
    inc/AssetTools/ImageImporter.h
    inc/AssetTools/ImportRequest.h
    inc/AssetTools/ImportResult.h
    inc/EditorCommandStack.h
    inc/EditorCommands.h
    inc/SceneDocument.h
)

add_library(EditorCore STATIC ${EDITOR_CORE_SRC} ${EDITOR_CORE_HDR})
add_library(Sakura::EditorCore ALIAS EditorCore)

target_include_directories(EditorCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_link_libraries(EditorCore PUBLIC
    Engine
    Sakura::EditorCoreThirdParty
)

# ---------------------------------------------------------------
# Editor (Qt UI layer) — QWidget shell, adapters, viewport host
# Logical layer: QtEditorFramework. CMake target: Editor.
# ---------------------------------------------------------------
if(NOT SAKURA_BUILD_EDITOR)
    return()
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(EDITOR_SRC
    src/Editor.cpp
    src/AssetImportDialog.cpp
    src/ContentBrowserWidget.cpp
    src/EditorMainWindow.cpp
    src/EditorViewportWidget.cpp
    src/ProjectBrowserDialog.cpp
    src/ReflectionInspector.cpp
    src/TemplateCardWidget.cpp
)

set(EDITOR_HDR
    inc/Editor.h
    inc/AssetImportDialog.h
    inc/ContentBrowserWidget.h
    inc/EditorAssetMime.h
    inc/EditorMainWindow.h
    inc/EditorTheme.h
    inc/EditorViewportWidget.h
    inc/ProjectBrowserDialog.h
    inc/ReflectionInspector.h
    inc/TemplateCardWidget.h
)

set(EDITOR_RESOURCES
    resources/studio.qrc
)

add_library(Editor STATIC ${EDITOR_SRC} ${EDITOR_HDR} ${EDITOR_RESOURCES})
add_library(Sakura::Editor ALIAS Editor)

target_include_directories(Editor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_link_libraries(Editor PUBLIC
    EditorCore
    Sakura::EditorThirdParty
)

target_compile_definitions(Editor PUBLIC
    SAKURA_EDITOR_RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources/studio"
)
