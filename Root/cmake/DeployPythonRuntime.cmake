if(NOT IS_DIRECTORY "${PYTHON_ROOT}/Lib/encodings" OR NOT EXISTS "${PYTHON_ROOT}/python${PYTHON_VERSION}.dll")
    message(FATAL_ERROR "A complete Windows Python installation is required for runtime deployment: ${PYTHON_ROOT}")
endif()
file(MAKE_DIRECTORY "${DEST_DIR}/Python")
file(COPY "${PYTHON_ROOT}/Lib" DESTINATION "${DEST_DIR}/Python"
    PATTERN "site-packages" EXCLUDE PATTERN "__pycache__" EXCLUDE
    PATTERN "test" EXCLUDE PATTERN "tests" EXCLUDE PATTERN "idlelib" EXCLUDE PATTERN "tkinter" EXCLUDE)
file(COPY "${PYTHON_ROOT}/DLLs" DESTINATION "${DEST_DIR}/Python"
    PATTERN "*.pdb" EXCLUDE PATTERN "*.lib" EXCLUDE PATTERN "*.ico" EXCLUDE
    PATTERN "_tkinter.pyd" EXCLUDE PATTERN "tcl*.dll" EXCLUDE PATTERN "tk*.dll" EXCLUDE)
file(GLOB PythonRuntimeLibraries "${PYTHON_ROOT}/*.dll")
file(COPY ${PythonRuntimeLibraries} DESTINATION "${DEST_DIR}")
if(EXISTS "${PYTHON_ROOT}/LICENSE.txt")
    file(COPY_FILE "${PYTHON_ROOT}/LICENSE.txt" "${DEST_DIR}/Python/LICENSE.txt" ONLY_IF_DIFFERENT)
endif()
file(WRITE "${DEST_DIR}/python${PYTHON_VERSION}._pth" "Python/Lib\nPython/DLLs\n.\n")
