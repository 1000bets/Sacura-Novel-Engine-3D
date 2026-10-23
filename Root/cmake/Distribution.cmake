get_property(SakuraDistributionPythonRoot GLOBAL PROPERTY SAKURA_DISTRIBUTION_PYTHON_ROOT)
get_property(SakuraDistributionPythonVersion GLOBAL PROPERTY SAKURA_DISTRIBUTION_PYTHON_VERSION)

set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
set(CMAKE_INSTALL_DEBUG_LIBRARIES FALSE)
include(InstallRequiredSystemLibraries)
set(SakuraReleaseRuntimeLibraries "${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}")
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS "")
set(CMAKE_INSTALL_DEBUG_LIBRARIES TRUE)
set(CMAKE_INSTALL_DEBUG_LIBRARIES_ONLY TRUE)
include(InstallRequiredSystemLibraries)
set(SakuraDebugRuntimeLibraries "${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS};${SakuraReleaseRuntimeLibraries}")

function(sakura_deploy_distribution_runtime TargetName)
    if(NOT WIN32)
        return()
    endif()
    if(SAKURA_PYTHON_AVAILABLE)
        add_custom_command(TARGET ${TargetName} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                "-DPYTHON_ROOT=${SakuraDistributionPythonRoot}"
                "-DPYTHON_VERSION=${SakuraDistributionPythonVersion}"
                "-DDEST_DIR=$<TARGET_FILE_DIR:${TargetName}>"
                -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/DeployPythonRuntime.cmake"
            VERBATIM)
    endif()
    if(SakuraReleaseRuntimeLibraries)
        add_custom_command(TARGET ${TargetName} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                "-DRUNTIME_LIBRARIES=$<IF:$<CONFIG:Debug>,${SakuraDebugRuntimeLibraries},${SakuraReleaseRuntimeLibraries}>"
                "-DDEST_DIR=$<TARGET_FILE_DIR:${TargetName}>"
                -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/CopySystemRuntime.cmake"
            VERBATIM)
    endif()
    add_custom_command(TARGET ${TargetName} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DSOURCE_DIR=$<TARGET_FILE_DIR:${TargetName}>"
            "-DDEST_DIR=${CMAKE_BINARY_DIR}/Stage/Bin"
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/CopyStageRuntime.cmake"
        VERBATIM)
endfunction()

sakura_deploy_distribution_runtime(SakuraPlayer)
if(SAKURA_BUILD_EDITOR AND WIN32)
    sakura_deploy_distribution_runtime(SakuraEditor)
    add_dependencies(SakuraEditor SakuraPlayer)
    set(SAKURA_DISTRIBUTION_DIRECTORY "${CMAKE_BINARY_DIR}/Distribution/$<CONFIG>/SakuraEditor" CACHE STRING "New output directory for the portable Windows Editor")
    add_custom_target(SakuraEditorDistribution
        COMMAND ${CMAKE_COMMAND}
            "-DSOURCE_DIR=$<TARGET_FILE_DIR:SakuraEditor>"
            "-DDEST_DIR=${SAKURA_DISTRIBUTION_DIRECTORY}"
            "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_LIST_DIR}/EditorDistribution.cmake"
        DEPENDS SakuraEditor SakuraPlayer SakuraMcpBridge
        VERBATIM)
endif()
