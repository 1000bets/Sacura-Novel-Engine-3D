# ---------------------------------------------------------------
# ufbx — FBX Import (Editor). Single-source C library.
# ---------------------------------------------------------------

include(FetchContent)

set(SAKURA_UFBX_GIT_TAG "v0.9.0" CACHE STRING "ufbx git tag")

FetchContent_Declare(
    ufbx
    GIT_REPOSITORY https://github.com/ufbx/ufbx.git
    GIT_TAG        ${SAKURA_UFBX_GIT_TAG}
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/_src
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(ufbx)

add_library(SakuraUfbx STATIC
    ${ufbx_SOURCE_DIR}/ufbx.c
)
add_library(Sakura::Ufbx ALIAS SakuraUfbx)

target_include_directories(SakuraUfbx PUBLIC
    ${ufbx_SOURCE_DIR}
)

set_target_properties(SakuraUfbx PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

if(MSVC)
    target_compile_options(SakuraUfbx PRIVATE /W3)
endif()

message(STATUS "Sakura Editor third-party: ufbx ${SAKURA_UFBX_GIT_TAG}")
