# ---------------------------------------------------------------
# Editor dependencies (owned by Editor)
# ---------------------------------------------------------------
add_subdirectory(ThirdParty)

# ---------------------------------------------------------------
# Editor library
# ---------------------------------------------------------------
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

file(GLOB_RECURSE EDITOR_SRC src/*.cpp)
file(GLOB_RECURSE EDITOR_HDR inc/*.h)
set(EDITOR_RESOURCES
    resources/studio.qrc
)

add_library(Editor STATIC ${EDITOR_SRC} ${EDITOR_HDR} ${EDITOR_RESOURCES})

target_include_directories(Editor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_link_libraries(Editor PUBLIC
    Engine
    Sakura::EditorThirdParty
)

target_compile_definitions(Editor PUBLIC
    SAKURA_EDITOR_RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources/studio"
)
