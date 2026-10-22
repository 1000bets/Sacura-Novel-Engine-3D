# Keep the pinned upstream generator, but publish a complete header only.
# Repeated WRITE/APPEND exposes a truncated header to a concurrent compiler or
# leaves one behind after interrupted configuration. CONFIGURE also preserves
# the timestamp when the generated list has not changed.
set(ShaderBuildUtils "${DILIGENT_FX_SOURCE_DIRECTORY}/BuildUtils.cmake")
file(READ "${ShaderBuildUtils}" ShaderBuildContents)
set(ShaderListMarker "# Sakura: publish the complete shader list with copy-if-different semantics.")
string(FIND "${ShaderBuildContents}" "${ShaderListMarker}" ShaderListPatchedPosition)
if(ShaderListPatchedPosition EQUAL -1)
    set(ShaderListOriginalWrite [=[file(WRITE ${_SHADERS_LIST_FILE}]=])
    set(ShaderListOriginalAppend [=[file(APPEND ${_SHADERS_LIST_FILE}]=])
    set(ShaderListOriginalPublish "        set_source_files_properties(")
    foreach(ShaderListOriginal IN ITEMS ShaderListOriginalWrite ShaderListOriginalAppend ShaderListOriginalPublish)
        string(FIND "${ShaderBuildContents}" "${${ShaderListOriginal}}" ShaderListOriginalPosition)
        if(ShaderListOriginalPosition EQUAL -1)
            message(FATAL_ERROR "DiligentFX shader list patch does not match the pinned source: ${ShaderListOriginal}")
        endif()
    endforeach()
    string(REPLACE "${ShaderListOriginalWrite}" "string(CONCAT SHADERS_LIST_CONTENT" ShaderBuildContents "${ShaderBuildContents}")
    string(REPLACE "${ShaderListOriginalAppend}" "string(APPEND SHADERS_LIST_CONTENT" ShaderBuildContents "${ShaderBuildContents}")
    set(ShaderListPublish [=[        # Sakura: publish the complete shader list with copy-if-different semantics.
        file(CONFIGURE OUTPUT "${_SHADERS_LIST_FILE}" CONTENT "${SHADERS_LIST_CONTENT}" @ONLY)

        set_source_files_properties(]=])
    string(REPLACE "${ShaderListOriginalPublish}" "${ShaderListPublish}" ShaderBuildContents "${ShaderBuildContents}")
    file(CONFIGURE OUTPUT "${ShaderBuildUtils}" CONTENT "${ShaderBuildContents}" @ONLY)
endif()
