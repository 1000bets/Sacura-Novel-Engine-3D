foreach(Required ENGINE_SOURCE_ROOT STAGE_ROOT TARGET_DIRECTORY)
    if(NOT DEFINED ${Required})
        message(FATAL_ERROR "DeployEngineAssets requires ${Required}")
    endif()
endforeach()

# Editor and Player can share both staging and executable directories.
# Serialize their always-run deployment jobs when built in parallel.
file(MAKE_DIRECTORY "${STAGE_ROOT}")
file(LOCK "${STAGE_ROOT}/.assets-deploy.lock" GUARD PROCESS TIMEOUT 120)
foreach(EngineDestination "${STAGE_ROOT}/Engine" "${TARGET_DIRECTORY}/Engine")
    file(MAKE_DIRECTORY "${EngineDestination}/Content" "${EngineDestination}/Shaders" "${EngineDestination}/Config")
    file(COPY "${ENGINE_SOURCE_ROOT}/Content/" DESTINATION "${EngineDestination}/Content")
    file(COPY "${ENGINE_SOURCE_ROOT}/shaders/" DESTINATION "${EngineDestination}/Shaders")
    file(COPY "${ENGINE_SOURCE_ROOT}/Config/" DESTINATION "${EngineDestination}/Config")
endforeach()
