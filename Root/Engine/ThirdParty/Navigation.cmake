set(SAKURA_RECAST_GIT_TAG "v1.6.0" CACHE STRING "Recast Navigation git tag")
set(RECASTNAVIGATION_DEMO OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_TESTS OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_DT_POLYREF64 ON CACHE BOOL "" FORCE)
FetchContent_Declare(RecastNavigation
    GIT_REPOSITORY https://github.com/recastnavigation/recastnavigation.git
    GIT_TAG ${SAKURA_RECAST_GIT_TAG}
    GIT_SHALLOW TRUE
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/navigation/_src
)
FetchContent_MakeAvailable(RecastNavigation)
add_library(SakuraRecastGeometry STATIC ${recastnavigation_SOURCE_DIR}/RecastDemo/Source/ChunkyTriMesh.cpp)
target_include_directories(SakuraRecastGeometry PRIVATE ${recastnavigation_SOURCE_DIR}/RecastDemo/Include)
add_library(SakuraNavigation INTERFACE)
add_library(Sakura::Navigation ALIAS SakuraNavigation)
target_include_directories(SakuraNavigation INTERFACE ${recastnavigation_SOURCE_DIR})
target_link_libraries(SakuraNavigation INTERFACE Recast Detour DetourCrowd SakuraRecastGeometry)
