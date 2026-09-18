add_subdirectory(Engine)
add_subdirectory(Editor)

add_executable(SakuraTest Tests/Main_Test.cpp)
target_link_libraries(SakuraTest PRIVATE Engine Editor)
sakura_copy_engine_runtime_dlls(SakuraTest)
