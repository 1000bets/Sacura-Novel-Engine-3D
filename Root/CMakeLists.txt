add_subdirectory(Engine)

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

# ---------------------------------------------------------------
# Tests
# ---------------------------------------------------------------

# Headless reflection (no QApplication required)
add_executable(SakuraReflectionTest
    Tests/Reflection_Test.cpp
    Tests/Reflection_Test_TU2.cpp
)
target_link_libraries(SakuraReflectionTest PRIVATE Engine)
sakura_copy_engine_runtime_dlls(SakuraReflectionTest)
sakura_link_whole_archive_engine(SakuraReflectionTest)

add_executable(SakuraSceneTest
    Tests/Scene_Test.cpp
)
target_link_libraries(SakuraSceneTest PRIVATE Engine)
sakura_copy_engine_runtime_dlls(SakuraSceneTest)
sakura_link_whole_archive_engine(SakuraSceneTest)

add_executable(SakuraStoryRuntimeTest
    Tests/StoryRuntime_Test.cpp
)
target_link_libraries(SakuraStoryRuntimeTest PRIVATE Engine)
sakura_copy_engine_runtime_dlls(SakuraStoryRuntimeTest)
sakura_link_whole_archive_engine(SakuraStoryRuntimeTest)

add_executable(SakuraPlaySessionTest
    Tests/PlaySession_Test.cpp
)
target_link_libraries(SakuraPlaySessionTest PRIVATE Engine)
sakura_copy_engine_runtime_dlls(SakuraPlaySessionTest)
sakura_link_whole_archive_engine(SakuraPlaySessionTest)

add_executable(SakuraRenderLifetimeTest
    Tests/RenderLifetime_Test.cpp
    Engine/src/Core/Threading/RenderCommandQueue.cpp
)
target_include_directories(SakuraRenderLifetimeTest PRIVATE
    ${CMAKE_SOURCE_DIR}/Root/Engine/inc
)

add_executable(SakuraRenderViewTest
    Tests/RenderView_Test.cpp
)
target_link_libraries(SakuraRenderViewTest PRIVATE Engine)
sakura_copy_engine_runtime_dlls(SakuraRenderViewTest)
sakura_link_whole_archive_engine(SakuraRenderViewTest)

# Headless Python reflection without Editor linkage.
add_executable(SakuraReflectionPythonTest
    Tests/ReflectionPython_Test.cpp
)
target_link_libraries(SakuraReflectionPythonTest PRIVATE Engine)
sakura_copy_engine_runtime_dlls(SakuraReflectionPythonTest)
sakura_link_whole_archive_engine(SakuraReflectionPythonTest)
target_compile_definitions(SakuraReflectionPythonTest PRIVATE
    SAKURA_SAMPLE_PROJECT_DIR="${CMAKE_SOURCE_DIR}/Samples/SampleProject"
)

if(SAKURA_BUILD_EDITOR)
    # GPU + window smoke (still uses Editor stub)
    add_executable(SakuraTest Tests/Main_Test.cpp)
    target_link_libraries(SakuraTest PRIVATE Engine Editor)
    sakura_copy_engine_runtime_dlls(SakuraTest)
    sakura_stage_engine_runtime(SakuraTest)
    sakura_link_whole_archive_engine(SakuraTest)

    add_custom_command(TARGET SakuraTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>"
            "$<TARGET_FILE_DIR:SakuraTest>"
        COMMENT "Copy SDL3 next to SakuraTest"
        VERBATIM
    )

endif()

    # Asset pipeline + EditorCore AssetImporter
    add_executable(SakuraAssetTest Tests/AssetSystem_Test.cpp)
    target_link_libraries(SakuraAssetTest PRIVATE Engine EditorCore)
    sakura_copy_engine_runtime_dlls(SakuraAssetTest)
    sakura_link_whole_archive_engine(SakuraAssetTest)

    add_executable(SakuraProjectSessionTest Tests/ProjectSession_Test.cpp)
    target_link_libraries(SakuraProjectSessionTest PRIVATE Engine)
    sakura_copy_engine_runtime_dlls(SakuraProjectSessionTest)
    sakura_link_whole_archive_engine(SakuraProjectSessionTest)

    add_executable(SakuraSceneSerializerTest Tests/SceneSerializer_Test.cpp)
    target_link_libraries(SakuraSceneSerializerTest PRIVATE Engine EditorCore)
    sakura_copy_engine_runtime_dlls(SakuraSceneSerializerTest)
    sakura_link_whole_archive_engine(SakuraSceneSerializerTest)

    add_executable(SakuraEditorCommandTest Tests/EditorCommand_Test.cpp)
    target_link_libraries(SakuraEditorCommandTest PRIVATE Engine EditorCore)
    sakura_copy_engine_runtime_dlls(SakuraEditorCommandTest)
    sakura_link_whole_archive_engine(SakuraEditorCommandTest)

add_executable(SakuraStoryWidgetTest Tests/StoryWidget_Test.cpp)
target_link_libraries(SakuraStoryWidgetTest PRIVATE Engine)
sakura_copy_engine_runtime_dlls(SakuraStoryWidgetTest)
sakura_link_whole_archive_engine(SakuraStoryWidgetTest)

add_executable(SakuraForwardRendererTest Tests/ForwardRenderer_Test.cpp)
target_link_libraries(SakuraForwardRendererTest PRIVATE Engine)
sakura_copy_engine_runtime_dlls(SakuraForwardRendererTest)
sakura_link_whole_archive_engine(SakuraForwardRendererTest)
target_compile_definitions(SakuraForwardRendererTest PRIVATE
    SAKURA_RENDER_SHADER_DIRECTORY="${CMAKE_SOURCE_DIR}/Root/Engine/shaders"
    SAKURA_RENDER_VALIDATION_PROJECT="${CMAKE_SOURCE_DIR}/Samples/RenderValidation/RenderValidation.project"
)

# ---------------------------------------------------------------
# CTest registration
# Labels:
#   headless  — no window / no GPU required for success criteria of the test itself
#   python    — needs embedded Python
#   editor    — links Editor / Qt
#   gpu       — opens a window / exercises render thread + GPU
#   probe     — intentionally failing; excluded from default preset runs via -LE probe
# ---------------------------------------------------------------
if(BUILD_TESTING)
    add_test(NAME Engine.Render.Forward COMMAND SakuraForwardRendererTest)
    set_tests_properties(Engine.Render.Forward PROPERTIES
        LABELS "gpu;engine;render"
        TIMEOUT 40
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:SakuraForwardRendererTest>"
    )
    get_filename_component(SakuraQtPrefix "${Qt6_DIR}/../../.." ABSOLUTE)
    add_test(NAME Engine.UI.Story COMMAND SakuraStoryWidgetTest)
    set_tests_properties(Engine.UI.Story PROPERTIES
        LABELS "headless;engine;ui"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${SakuraQtPrefix}/plugins"
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:SakuraStoryWidgetTest>"
    )
    add_test(NAME Engine.Reflection.Native COMMAND SakuraReflectionTest)
    set_tests_properties(Engine.Reflection.Native PROPERTIES
        LABELS "headless;engine"
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:SakuraReflectionTest>"
    )

    add_test(NAME Engine.Scene.Ownership COMMAND SakuraSceneTest)
    set_tests_properties(Engine.Scene.Ownership PROPERTIES
        LABELS "headless;engine"
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:SakuraSceneTest>"
    )

    add_test(NAME Engine.Story.Runtime COMMAND SakuraStoryRuntimeTest)
    set_tests_properties(Engine.Story.Runtime PROPERTIES
        LABELS "headless;engine"
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:SakuraStoryRuntimeTest>"
    )

    add_test(NAME Engine.Play.Session COMMAND SakuraPlaySessionTest)
    set_tests_properties(Engine.Play.Session PROPERTIES
        LABELS "headless;engine"
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:SakuraPlaySessionTest>"
    )

    add_test(NAME Engine.Render.CommandLifetime COMMAND SakuraRenderLifetimeTest)
    set_tests_properties(Engine.Render.CommandLifetime PROPERTIES
        LABELS "headless;engine"
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:SakuraRenderLifetimeTest>"
    )

    add_test(NAME Engine.Render.ViewCamera COMMAND SakuraRenderViewTest)
    set_tests_properties(Engine.Render.ViewCamera PROPERTIES
        LABELS "headless;engine"
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:SakuraRenderViewTest>"
    )

    add_test(NAME Engine.Reflection.Python COMMAND SakuraReflectionPythonTest)
    set_tests_properties(Engine.Reflection.Python PROPERTIES
        LABELS "headless;engine;python"
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:SakuraReflectionPythonTest>"
    )

        add_test(NAME Engine.Assets.WithEditorImport COMMAND SakuraAssetTest)
        set_tests_properties(Engine.Assets.WithEditorImport PROPERTIES
            LABELS "headless;engine;editor"
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:SakuraAssetTest>"
        )

        add_test(NAME Engine.Project.Session COMMAND SakuraProjectSessionTest)
        set_tests_properties(Engine.Project.Session PROPERTIES
            LABELS "headless;engine"
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:SakuraProjectSessionTest>"
        )

        add_test(NAME Engine.Scene.Serializer COMMAND SakuraSceneSerializerTest)
        set_tests_properties(Engine.Scene.Serializer PROPERTIES
            LABELS "headless;engine;editor"
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:SakuraSceneSerializerTest>"
        )

        add_test(NAME Editor.Commands.Undo COMMAND SakuraEditorCommandTest)
        set_tests_properties(Editor.Commands.Undo PROPERTIES
            LABELS "headless;engine;editor"
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:SakuraEditorCommandTest>"
        )

    if(SAKURA_BUILD_EDITOR)
        add_test(NAME Engine.Gpu.MainSmoke COMMAND SakuraTest)
        set_tests_properties(Engine.Gpu.MainSmoke PROPERTIES
            LABELS "gpu;editor"
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:SakuraTest>"
        )
    endif()

    # Intentionally failing probe — proves ctest reports non-zero when a check fails.
    # Default presets use `ctest -LE probe` so this is excluded from green runs.
    add_test(NAME Engine.Probe.MustFail COMMAND ${CMAKE_COMMAND} -E false)
    set_tests_properties(Engine.Probe.MustFail PROPERTIES
        LABELS "probe"
    )
endif()
