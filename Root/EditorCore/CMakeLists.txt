set(EDITOR_CORE_SRC
    src/Assets/AssetOperations.cpp
    src/Assets/AssetImporter.cpp
    src/Assets/FbxToGlbConverter.cpp
    src/Assets/GlbImporter.cpp
    src/Assets/ImageImporter.cpp
    src/Actions/EditorAction.cpp
    src/Commands/EditorCommandStack.cpp
    src/Commands/EditorCommands.cpp
    src/Documents/MaterialDocument.cpp
    src/Documents/SceneDocument.cpp
    src/Documents/SceneWorkspace.cpp
)

set(EDITOR_CORE_HDR
    inc/Assets/AssetOperations.h
    inc/Assets/AssetImporter.h
    inc/Assets/FbxToGlbConverter.h
    inc/Assets/GlbImporter.h
    inc/Assets/ImageImporter.h
    inc/Assets/ImportRequest.h
    inc/Assets/ImportResult.h
    inc/Actions/EditorAction.h
    inc/Actions/EditorActionContext.h
    inc/Commands/EditorCommandStack.h
    inc/Commands/EditorCommands.h
    inc/Documents/MaterialDocument.h
    inc/Documents/SceneDocument.h
    inc/Documents/SceneWorkspace.h
)

add_library(EditorCore STATIC ${EDITOR_CORE_SRC} ${EDITOR_CORE_HDR})
add_library(Sakura::EditorCore ALIAS EditorCore)

target_include_directories(EditorCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_link_libraries(EditorCore PUBLIC
    Engine
)
target_link_libraries(EditorCore PRIVATE
    Sakura::EditorCoreThirdParty
    Sakura::AssetThirdParty
)


target_sources(EditorCore PRIVATE src/Assets/MaterialAssetCreation.cpp)

target_sources(EditorCore PRIVATE src/Assets/MaterialSourceRelocation.cpp)
