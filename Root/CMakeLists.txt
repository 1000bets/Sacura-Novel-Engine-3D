add_subdirectory(Engine)
add_subdirectory(Editor)

add_executable(SakuraTest Tests/Main_Test.cpp)
target_link_libraries(SakuraTest PRIVATE Engine Editor)
sakura_copy_engine_runtime_dlls(SakuraTest)

add_custom_command(TARGET SakuraTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:SakuraTest>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Engine/shaders"
        "$<TARGET_FILE_DIR:SakuraTest>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SDL3::SDL3>"
        "$<TARGET_FILE_DIR:SakuraTest>"
    COMMENT "Copy shaders and SDL3 next to SakuraTest"
    VERBATIM
)
