add_subdirectory(Engine)

add_subdirectory(Editor/ThirdParty)
add_subdirectory(EditorCore)
add_subdirectory(Editor)

# ---------------------------------------------------------------
# Staging helper (defined in Engine/CMakeLists.txt)
# ---------------------------------------------------------------
# sakura_stage_engine_runtime, sakura_copy_engine_runtime_dlls,
# sakura_deploy_qt_runtime (Editor ThirdParty, only when SAKURA_BUILD_EDITOR=ON)

function(sakura_link_whole_archive_engine TargetName)
    if(MSVC)
        target_link_options(${TargetName} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:Engine>")
    endif()
endfunction()

# ---------------------------------------------------------------
# Standalone player (Engine runtime Qt UI)
# ---------------------------------------------------------------
add_executable(SakuraPlayer Engine/apps/SakuraPlayerMain.cpp)
target_link_libraries(SakuraPlayer PRIVATE Engine)
sakura_copy_engine_runtime_dlls(SakuraPlayer)
sakura_stage_engine_runtime(SakuraPlayer)
sakura_link_whole_archive_engine(SakuraPlayer)

add_custom_command(TARGET SakuraPlayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SDL3::SDL3>"
        "$<TARGET_FILE_DIR:SakuraPlayer>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SakuraPlayer>"
        "${CMAKE_BINARY_DIR}/Stage/Bin/"
    COMMAND ${CMAKE_COMMAND}
        "-DSOURCE_DIR=$<TARGET_FILE_DIR:SakuraPlayer>"
        "-DDEST_DIR=${CMAKE_BINARY_DIR}/Stage/Bin"
        -P "${CMAKE_SOURCE_DIR}/Root/cmake/CopyStageRuntime.cmake"
    COMMENT "Sync SakuraPlayer runtime into Stage/Bin"
    VERBATIM
)

# ---------------------------------------------------------------
# Editor application
# ---------------------------------------------------------------
if(SAKURA_BUILD_EDITOR)
    add_executable(SakuraEditor Editor/apps/SakuraEditorMain.cpp)
    target_link_libraries(SakuraEditor PRIVATE Engine Editor)
    target_compile_definitions(SakuraEditor PRIVATE
        SAKURA_SAMPLE_PROJECT_FILE="${CMAKE_SOURCE_DIR}/Samples/SampleProject/SampleProject.project"
    )
    sakura_copy_engine_runtime_dlls(SakuraEditor)
    sakura_stage_engine_runtime(SakuraEditor)
    sakura_link_whole_archive_engine(SakuraEditor)

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
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Tests/TestTargets.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/Tests/TestTargets.cmake")
endif()
