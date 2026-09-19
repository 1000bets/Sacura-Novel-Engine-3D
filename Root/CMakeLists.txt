add_subdirectory(Engine)
add_subdirectory(Editor)

# ---------------------------------------------------------------
# Staging helper (defined in Engine/CMakeLists.txt)
# ---------------------------------------------------------------
# sakura_stage_engine_runtime, sakura_copy_engine_runtime_dlls,
# sakura_deploy_qt_runtime

# ---------------------------------------------------------------
# Editor application
# ---------------------------------------------------------------
add_executable(SakuraEditor Editor/apps/SakuraEditorMain.cpp)
target_link_libraries(SakuraEditor PRIVATE Engine Editor)
target_compile_definitions(SakuraEditor PRIVATE
    SAKURA_SAMPLE_PROJECT_FILE="${CMAKE_SOURCE_DIR}/Samples/SampleProject/SampleProject.project"
)
sakura_copy_engine_runtime_dlls(SakuraEditor)
sakura_deploy_qt_runtime(SakuraEditor)
sakura_stage_engine_runtime(SakuraEditor)

add_custom_command(TARGET SakuraEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SDL3::SDL3>"
        "$<TARGET_FILE_DIR:SakuraEditor>"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:SakuraEditor>/studio"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Root/Editor/resources/studio"
        "$<TARGET_FILE_DIR:SakuraEditor>/studio"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SakuraEditor>"
        "${CMAKE_BINARY_DIR}/Stage/Bin/"
    COMMAND ${CMAKE_COMMAND}
        "-DSOURCE_DIR=$<TARGET_FILE_DIR:SakuraEditor>"
        "-DDEST_DIR=${CMAKE_BINARY_DIR}/Stage/Bin"
        -P "${CMAKE_SOURCE_DIR}/Root/cmake/CopyStageRuntime.cmake"
    COMMENT "Sync SakuraEditor runtime into Stage/Bin"
    VERBATIM
)

# ---------------------------------------------------------------
# Tests
# ---------------------------------------------------------------
add_executable(SakuraTest Tests/Main_Test.cpp)
target_link_libraries(SakuraTest PRIVATE Engine Editor)
sakura_copy_engine_runtime_dlls(SakuraTest)
sakura_deploy_qt_runtime(SakuraTest)
sakura_stage_engine_runtime(SakuraTest)

add_custom_command(TARGET SakuraTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SDL3::SDL3>"
        "$<TARGET_FILE_DIR:SakuraTest>"
    COMMENT "Copy SDL3 next to SakuraTest"
    VERBATIM
)

add_executable(SakuraAssetTest Tests/AssetSystem_Test.cpp)
target_link_libraries(SakuraAssetTest PRIVATE Engine Editor)
sakura_deploy_qt_runtime(SakuraAssetTest)

add_executable(SakuraReflectionTest
    Tests/Reflection_Test.cpp
    Tests/Reflection_Test_TU2.cpp
)
target_link_libraries(SakuraReflectionTest PRIVATE Engine)

add_executable(SakuraReflectionPythonTest
    Tests/ReflectionPython_Test.cpp
)
target_link_libraries(SakuraReflectionPythonTest PRIVATE Engine Editor)
sakura_deploy_qt_runtime(SakuraReflectionPythonTest)

target_compile_definitions(SakuraReflectionPythonTest PRIVATE
    SAKURA_SAMPLE_PROJECT_DIR="${CMAKE_SOURCE_DIR}/Samples/SampleProject"
)

if(MSVC)
    target_link_options(SakuraReflectionTest PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:Engine>")
    target_link_options(SakuraReflectionPythonTest PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:Engine>")
    target_link_options(SakuraAssetTest PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:Engine>")
    target_link_options(SakuraTest PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:Engine>")
    target_link_options(SakuraEditor PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:Engine>")
endif()
