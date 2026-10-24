set(SakuraBundledPythonVersion "3.12.10")
FetchContent_Declare(
    SakuraBundledPython
    URL "https://api.nuget.org/v3-flatcontainer/python/${SakuraBundledPythonVersion}/python.${SakuraBundledPythonVersion}.nupkg"
    URL_HASH SHA256=0eb85c2dfccccf1b17352de4c397f69194035b7d37149eacc16f1147d93de3b8
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/python/_src"
    DOWNLOAD_NAME "python-${SakuraBundledPythonVersion}.zip"
)
FetchContent_MakeAvailable(SakuraBundledPython)
set(Python3_ROOT_DIR "${sakurabundledpython_SOURCE_DIR}/tools")
if(NOT "${_Python3_RUNTIME_LIBRARY_RELEASE}" STREQUAL "${Python3_ROOT_DIR}/python312.dll")
    get_cmake_property(SakuraPythonCacheVariables CACHE_VARIABLES)
    foreach(SakuraPythonCacheVariable IN LISTS SakuraPythonCacheVariables)
        if(SakuraPythonCacheVariable MATCHES "^_Python3_")
            unset(${SakuraPythonCacheVariable} CACHE)
        endif()
    endforeach()
endif()
set(Python3_EXECUTABLE "${Python3_ROOT_DIR}/python.exe" CACHE FILEPATH "Bundled Python interpreter" FORCE)
set(Python3_INCLUDE_DIR "${Python3_ROOT_DIR}/include" CACHE PATH "Bundled Python headers" FORCE)
set(Python3_LIBRARY "${Python3_ROOT_DIR}/libs/python312.lib" CACHE FILEPATH "Bundled Python import library" FORCE)
set(Python3_FIND_REGISTRY NEVER)
