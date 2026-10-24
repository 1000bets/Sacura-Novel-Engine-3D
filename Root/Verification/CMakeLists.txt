add_executable(SakuraIoServicesTest IoServices.cpp
    ../Engine/src/Core/IO/AtomicFileWriter.cpp ../Engine/src/Core/IO/ImageBuffer.cpp ../Engine/src/Assets/Guid.cpp)
target_include_directories(SakuraIoServicesTest PRIVATE ../Engine/inc)
target_link_libraries(SakuraIoServicesTest PRIVATE Sakura::StbImage)
target_compile_definitions(SakuraIoServicesTest PRIVATE NOMINMAX)
if(MSVC)
    target_compile_options(SakuraIoServicesTest PRIVATE /utf-8)
endif()
add_test(NAME Engine.IO.Services COMMAND SakuraIoServicesTest)

add_executable(SakuraCharacterRuntimeTest CharacterRuntimeTest.cpp)
target_link_libraries(SakuraCharacterRuntimeTest PRIVATE Engine)
sakura_link_whole_archive_engine(SakuraCharacterRuntimeTest)
sakura_copy_engine_runtime_dlls(SakuraCharacterRuntimeTest)
sakura_stage_engine_runtime(SakuraCharacterRuntimeTest)
add_custom_command(TARGET SakuraCharacterRuntimeTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:SDL3::SDL3>" "$<TARGET_FILE_DIR:SakuraCharacterRuntimeTest>"
    VERBATIM)
if(SAKURA_PYTHON_AVAILABLE)
    get_property(CharacterTestPythonRoot GLOBAL PROPERTY SAKURA_DISTRIBUTION_PYTHON_ROOT)
    get_property(CharacterTestPythonVersion GLOBAL PROPERTY SAKURA_DISTRIBUTION_PYTHON_VERSION)
    add_custom_command(TARGET SakuraCharacterRuntimeTest POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DPYTHON_ROOT=${CharacterTestPythonRoot}"
            "-DPYTHON_VERSION=${CharacterTestPythonVersion}"
            "-DDEST_DIR=$<TARGET_FILE_DIR:SakuraCharacterRuntimeTest>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/DeployPythonRuntime.cmake"
        VERBATIM)
endif()
add_test(NAME Engine.Character.Runtime COMMAND SakuraCharacterRuntimeTest)
