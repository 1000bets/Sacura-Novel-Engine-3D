# ---------------------------------------------------------------
# Asset pipeline runtime libs: fastgltf, ozz-animation, stb, json
# ---------------------------------------------------------------

include(FetchContent)

set(SAKURA_FASTGLTF_GIT_TAG "v0.9.0" CACHE STRING "fastgltf git tag")
set(SAKURA_OZZ_GIT_TAG "0.16.0" CACHE STRING "ozz-animation git tag")
set(SAKURA_NLOHMANN_JSON_GIT_TAG "v3.12.0" CACHE STRING "nlohmann/json git tag")
set(SAKURA_STB_GIT_TAG "2c980bb59875b0d32144a71867fbdebb2f77cd20" CACHE STRING "stb git commit")

# ---- fastgltf (glTF/GLB parse + export) ----
set(FASTGLTF_COMPILE_AS_CPP20 OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_DOCS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    fastgltf
    GIT_REPOSITORY https://github.com/spnda/fastgltf.git
    GIT_TAG        ${SAKURA_FASTGLTF_GIT_TAG}
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/fastgltf/_src
    EXCLUDE_FROM_ALL
)

# ---- ozz-animation (skeletal runtime) ----
set(ozz_build_tools OFF CACHE BOOL "" FORCE)
set(ozz_build_fbx OFF CACHE BOOL "" FORCE)
set(ozz_build_gltf OFF CACHE BOOL "" FORCE)
set(ozz_build_data OFF CACHE BOOL "" FORCE)
set(ozz_build_samples OFF CACHE BOOL "" FORCE)
set(ozz_build_howtos OFF CACHE BOOL "" FORCE)
set(ozz_build_tests OFF CACHE BOOL "" FORCE)
set(ozz_build_msvc_rt_dll ON CACHE BOOL "" FORCE)
set(ozz_build_postfix OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    ozz_animation
    GIT_REPOSITORY https://github.com/guillaumeblanc/ozz-animation.git
    GIT_TAG        ${SAKURA_OZZ_GIT_TAG}
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/ozz-animation/_src
    EXCLUDE_FROM_ALL
)

# ---- nlohmann/json (.meta / JSON documents) ----
set(JSON_BuildTests OFF CACHE INTERNAL "" FORCE)
set(JSON_Install OFF CACHE INTERNAL "" FORCE)
set(JSON_MultipleHeaders OFF CACHE INTERNAL "" FORCE)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        ${SAKURA_NLOHMANN_JSON_GIT_TAG}
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/nlohmann_json/_src
    EXCLUDE_FROM_ALL
)

# ---- stb (image decode headers; implementation compiled once) ----
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        ${SAKURA_STB_GIT_TAG}
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/stb/_src
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(fastgltf ozz_animation nlohmann_json stb)

# stb_image as a real compile unit (STB_IMAGE_IMPLEMENTATION once)
add_library(SakuraStbImage STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/stb/stb_image_impl.cpp
)
add_library(Sakura::StbImage ALIAS SakuraStbImage)

target_include_directories(SakuraStbImage PUBLIC
    ${stb_SOURCE_DIR}
)

if(MSVC)
    target_compile_options(SakuraStbImage PRIVATE /W3)
endif()

# Aggregate: Engine asset I/O + animation runtime
add_library(SakuraAssetThirdParty INTERFACE)
add_library(Sakura::AssetThirdParty ALIAS SakuraAssetThirdParty)

target_link_libraries(SakuraAssetThirdParty INTERFACE
    fastgltf::fastgltf
    ozz_animation
    ozz_animation_offline
    nlohmann_json::nlohmann_json
    Sakura::StbImage
)

message(STATUS "Sakura asset third-party: fastgltf ${SAKURA_FASTGLTF_GIT_TAG}, ozz ${SAKURA_OZZ_GIT_TAG}, json ${SAKURA_NLOHMANN_JSON_GIT_TAG}, stb ${SAKURA_STB_GIT_TAG}")
