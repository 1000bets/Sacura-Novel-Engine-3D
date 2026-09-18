# ---------------------------------------------------------------
# Editor dependencies (owned by Editor)
# ---------------------------------------------------------------
add_subdirectory(ThirdParty)

# ---------------------------------------------------------------
# Editor library
# ---------------------------------------------------------------
file(GLOB_RECURSE EDITOR_SRC src/*.cpp)
file(GLOB_RECURSE EDITOR_HDR inc/*.h)

add_library(Editor STATIC ${EDITOR_SRC} ${EDITOR_HDR})

target_include_directories(Editor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_link_libraries(Editor PUBLIC
    Engine
    Sakura::EditorThirdParty
)
