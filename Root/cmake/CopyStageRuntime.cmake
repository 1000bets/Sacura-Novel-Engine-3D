if(NOT DEFINED SOURCE_DIR OR NOT DEFINED DEST_DIR)
    message(FATAL_ERROR "CopyStageRuntime: SOURCE_DIR and DEST_DIR are required")
endif()

file(MAKE_DIRECTORY "${DEST_DIR}")

file(GLOB RuntimeDlls "${SOURCE_DIR}/*.dll")
foreach(RuntimeDll IN LISTS RuntimeDlls)
    file(COPY "${RuntimeDll}" DESTINATION "${DEST_DIR}")
endforeach()

if(EXISTS "${SOURCE_DIR}/platforms")
    file(COPY "${SOURCE_DIR}/platforms" DESTINATION "${DEST_DIR}")
endif()
