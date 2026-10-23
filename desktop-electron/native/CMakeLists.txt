if(NOT TARGET Sakura::ImGuizmo)
    message(FATAL_ERROR "Interactive Electron viewport requires SAKURA_BUILD_EDITOR=ON for existing ImGuizmo integration")
endif()
if(BUILD_TESTING)
    add_executable(SakuraEditorCompatibilityTest QtCompatibilityTest.cpp)
    target_link_libraries(SakuraEditorCompatibilityTest PRIVATE Editor)
    sakura_link_whole_archive_engine(SakuraEditorCompatibilityTest)
    sakura_copy_engine_runtime_dlls(SakuraEditorCompatibilityTest)
    sakura_stage_engine_runtime(SakuraEditorCompatibilityTest)
endif()
add_executable(SakuraElectronHost Host.cpp ProjectCli.cpp ViewportController.cpp NativeBridge.cpp)
set_property(TARGET SakuraElectronHost APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../bridge/contract.json")
target_link_libraries(SakuraElectronHost PRIVATE Engine EditorCore Sakura::ImGuizmo user32 gdi32)
target_compile_definitions(SakuraElectronHost PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
sakura_link_whole_archive_engine(SakuraElectronHost)
sakura_copy_engine_runtime_dlls(SakuraElectronHost)
sakura_stage_engine_runtime(SakuraElectronHost)
if(MSVC)
    # App-local redistributable CRT: a clean Windows machine need not have VS.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)
    if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        add_custom_command(TARGET SakuraElectronHost POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} "$<TARGET_FILE_DIR:SakuraElectronHost>"
            COMMAND_EXPAND_LISTS VERBATIM)
    endif()
endif()
add_custom_command(TARGET SakuraElectronHost POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../bridge/contract.json" "$<TARGET_FILE_DIR:SakuraElectronHost>/BridgeContract.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../bridge/contract.json" "${CMAKE_BINARY_DIR}/Stage/Bin/BridgeContract.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SDL3::SDL3>" "$<TARGET_FILE_DIR:SakuraElectronHost>"
    VERBATIM)
if(SAKURA_PYTHON_AVAILABLE)
    find_package(Python3 COMPONENTS Interpreter Development.Embed REQUIRED)
    get_filename_component(SakuraPythonHome "${Python3_EXECUTABLE}" DIRECTORY)
    add_custom_command(TARGET SakuraElectronHost POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Python3::Python>" "$<TARGET_FILE_DIR:SakuraElectronHost>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SakuraPythonHome}/Lib" "$<TARGET_FILE_DIR:SakuraElectronHost>/Python/Lib"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SakuraPythonHome}/DLLs" "$<TARGET_FILE_DIR:SakuraElectronHost>/Python/DLLs"
        COMMENT "Deploy embedded Python standard library for the Electron host"
        VERBATIM)
endif()
