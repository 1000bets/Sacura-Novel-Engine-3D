# ---------------------------------------------------------------
# Engine
# ---------------------------------------------------------------
file(GLOB_RECURSE ENGINE_SRC Engine/src/*.cpp)
file(GLOB_RECURSE ENGINE_HDR Engine/inc/*.h)
 
add_library(Engine STATIC ${ENGINE_SRC} ${ENGINE_HDR})
 
target_include_directories(Engine PUBLIC Engine/inc)
 
# SimpleMath
target_sources(Engine PRIVATE ThirdParty/SimpleMath/SimpleMath.cpp)
target_include_directories(Engine PUBLIC ThirdParty/SimpleMath)
 
if(NOT WIN32)
    target_include_directories(Engine PUBLIC ThirdParty/DirectXMath)
endif()
 
# ---------------------------------------------------------------
# Editor
# ---------------------------------------------------------------
file(GLOB_RECURSE EDITOR_SRC Editor/src/*.cpp)
file(GLOB_RECURSE EDITOR_HDR Editor/inc/*.h)
 
add_library(Editor STATIC ${EDITOR_SRC} ${EDITOR_HDR})
 
target_include_directories(Editor PUBLIC Editor/inc)
target_link_libraries(Editor PUBLIC Engine)
 
# ---------------------------------------------------------------
# Tests
# ---------------------------------------------------------------
add_executable(SakuraTest Tests/Main_Test.cpp)
target_link_libraries(SakuraTest PRIVATE Engine Editor)