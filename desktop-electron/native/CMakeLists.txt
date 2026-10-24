if(NOT TARGET Sakura::ImGuizmo)
    message(FATAL_ERROR "Interactive Electron viewport requires SAKURA_BUILD_EDITOR=ON for existing ImGuizmo integration")
endif()
if(BUILD_TESTING)
    add_executable(SakuraAssetCreationFactoryTest AssetCreationFactoryTest.cpp)
    target_link_libraries(SakuraAssetCreationFactoryTest PRIVATE EditorCore)
    sakura_link_whole_archive_engine(SakuraAssetCreationFactoryTest)
    sakura_copy_engine_runtime_dlls(SakuraAssetCreationFactoryTest)
    add_test(NAME Editor.AssetCreationFactories COMMAND SakuraAssetCreationFactoryTest "${CMAKE_BINARY_DIR}/asset-factory-test")

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
    get_property(SakuraPythonHome GLOBAL PROPERTY SAKURA_DISTRIBUTION_PYTHON_ROOT)
    get_property(SakuraPythonVersion GLOBAL PROPERTY SAKURA_DISTRIBUTION_PYTHON_VERSION)
    add_custom_command(TARGET SakuraElectronHost POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DPYTHON_ROOT=${SakuraPythonHome}"
            "-DPYTHON_VERSION=${SakuraPythonVersion}"
            "-DDEST_DIR=$<TARGET_FILE_DIR:SakuraElectronHost>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/../../Root/cmake/DeployPythonRuntime.cmake"
        COMMENT "Deploy bundled Python for the Electron host"
        VERBATIM)
endif()


add_custom_command(TARGET SakuraElectronHost POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Root/EditorCore/resources/Localization/Editor.json"
        "$<TARGET_FILE_DIR:SakuraElectronHost>/Engine/Config/EditorLocalization.json"
    VERBATIM)
