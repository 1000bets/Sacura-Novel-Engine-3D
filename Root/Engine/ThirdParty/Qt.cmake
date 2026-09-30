option(SAKURA_BUILD_QT_FROM_SOURCE "Download and build Qt from source" ON)
set(SAKURA_QT_VERSION "6.8.3" CACHE STRING "Qt Base source version")
set(SAKURA_QT_SOURCE_SHA256 "56001b905601bb9023d399f3ba780d7fa940f3e4861e496a7c490331f49e0b80" CACHE STRING "Qt Base source archive SHA256")
set(SAKURA_QT_BUILD_JOBS "4" CACHE STRING "Parallel jobs for the Qt source build")

if(SAKURA_BUILD_QT_FROM_SOURCE)
    include(FetchContent)
    string(REGEX MATCH "^[0-9]+\\.[0-9]+" SakuraQtVersionSeries "${SAKURA_QT_VERSION}")
    FetchContent_Declare(SakuraQtBase
        URL "https://download.qt.io/archive/qt/${SakuraQtVersionSeries}/${SAKURA_QT_VERSION}/submodules/qtbase-everywhere-src-${SAKURA_QT_VERSION}.tar.xz"
        URL_HASH "SHA256=${SAKURA_QT_SOURCE_SHA256}"
        TIMEOUT 300
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/Qt/Source/${SAKURA_QT_VERSION}"
    )
    FetchContent_GetProperties(SakuraQtBase)
    if(NOT sakuraqtbase_POPULATED)
        FetchContent_Populate(SakuraQtBase)
    endif()

    set(SakuraQtConfiguration "${CMAKE_BUILD_TYPE}")
    if(NOT SakuraQtConfiguration)
        set(SakuraQtConfiguration Debug)
    endif()
    if(CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_CONFIGURATION_TYPES "${SakuraQtConfiguration}" CACHE STRING "Configurations matching the source-built Qt" FORCE)
    endif()
    set(SakuraQtInstallDirectory "${CMAKE_BINARY_DIR}/Qt/${SAKURA_QT_VERSION}/${SakuraQtConfiguration}/Install")
    file(SHA256 "${CMAKE_CURRENT_LIST_DIR}/BuildQt.ps1" SakuraQtBuildScriptHash)
    string(SHA256 SakuraQtBuildIdentity "${SAKURA_QT_VERSION};${SAKURA_QT_SOURCE_SHA256};${SakuraQtConfiguration};${CMAKE_CXX_COMPILER};${CMAKE_CXX_COMPILER_VERSION};${SakuraQtBuildScriptHash}")
    string(SUBSTRING "${SakuraQtBuildIdentity}" 0 12 SakuraQtBuildKey)
    set(SakuraQtBuildDirectory "${CMAKE_BINARY_DIR}/Qt/${SAKURA_QT_VERSION}/${SakuraQtConfiguration}/Build-${SakuraQtBuildKey}")
    set(SakuraQtBuildStamp "${SakuraQtInstallDirectory}/.sakura-build-identity")
    set(SakuraQtCachedBuildIdentity "")
    if(EXISTS "${SakuraQtBuildStamp}")
        file(READ "${SakuraQtBuildStamp}" SakuraQtCachedBuildIdentity)
    endif()
    if(NOT SakuraQtCachedBuildIdentity STREQUAL SakuraQtBuildIdentity OR NOT EXISTS "${SakuraQtInstallDirectory}/lib/cmake/Qt6/Qt6Config.cmake")
        message(STATUS "Building Qt ${SAKURA_QT_VERSION} from source (${SakuraQtConfiguration})")
        if(WIN32)
            find_program(SakuraPowerShellExecutable NAMES powershell REQUIRED)
            set(SakuraQtBuildCommand
                "${SakuraPowerShellExecutable}" -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_LIST_DIR}/BuildQt.ps1"
                -SourceDirectory "${sakuraqtbase_SOURCE_DIR}"
                -BuildDirectory "${SakuraQtBuildDirectory}"
                -InstallDirectory "${SakuraQtInstallDirectory}"
                -Configuration "${SakuraQtConfiguration}"
                -CMakeExecutable "${CMAKE_COMMAND}"
                -ParallelJobs "${SAKURA_QT_BUILD_JOBS}"
            )
            if(CMAKE_GENERATOR_INSTANCE)
                list(APPEND SakuraQtBuildCommand -VisualStudioDirectory "${CMAKE_GENERATOR_INSTANCE}")
            endif()
            execute_process(COMMAND ${SakuraQtBuildCommand} RESULT_VARIABLE SakuraQtBuildResult)
        else()
            execute_process(COMMAND "${CMAKE_COMMAND}"
                -S "${sakuraqtbase_SOURCE_DIR}" -B "${SakuraQtBuildDirectory}" -G Ninja
                "-DCMAKE_BUILD_TYPE=${SakuraQtConfiguration}"
                "-DCMAKE_INSTALL_PREFIX=${SakuraQtInstallDirectory}"
                -DBUILD_SHARED_LIBS=ON -DQT_BUILD_TESTS=OFF -DQT_BUILD_EXAMPLES=OFF
                -DFEATURE_system_zlib=OFF -DFEATURE_system_pcre2=OFF
                -DFEATURE_system_jpeg=OFF -DFEATURE_system_png=OFF
                -DFEATURE_system_freetype=OFF -DFEATURE_system_harfbuzz=OFF
                RESULT_VARIABLE SakuraQtBuildResult
            )
            if(SakuraQtBuildResult EQUAL 0)
                execute_process(COMMAND "${CMAKE_COMMAND}" --build "${SakuraQtBuildDirectory}"
                    --parallel "${SAKURA_QT_BUILD_JOBS}" RESULT_VARIABLE SakuraQtBuildResult)
            endif()
            if(SakuraQtBuildResult EQUAL 0)
                execute_process(COMMAND "${CMAKE_COMMAND}" --install "${SakuraQtBuildDirectory}"
                    RESULT_VARIABLE SakuraQtBuildResult)
            endif()
        endif()
        if(NOT SakuraQtBuildResult STREQUAL "0")
            message(FATAL_ERROR "Qt source build failed: ${SakuraQtBuildResult}")
        endif()
        file(WRITE "${SakuraQtBuildStamp}" "${SakuraQtBuildIdentity}")
    endif()

    get_cmake_property(SakuraCacheVariables CACHE_VARIABLES)
    foreach(SakuraVariable IN LISTS SakuraCacheVariables)
        if(SakuraVariable MATCHES "^Qt6.*_DIR$")
            unset(${SakuraVariable} CACHE)
            unset(${SakuraVariable})
        endif()
    endforeach()
    set(Qt6_DIR "${SakuraQtInstallDirectory}/lib/cmake/Qt6" CACHE PATH "Source-built Qt package" FORCE)
    list(PREPEND CMAKE_PREFIX_PATH "${SakuraQtInstallDirectory}")
else()
    set(SakuraLegacyQtRoot "${CMAKE_SOURCE_DIR}/Root/Editor/ThirdParty/Qt")
    set(SakuraRuntimeQtRoot "${CMAKE_CURRENT_LIST_DIR}/Qt")
    get_cmake_property(SakuraCacheVariables CACHE_VARIABLES)
    foreach(SakuraVariable IN LISTS SakuraCacheVariables)
        if(SakuraVariable MATCHES "^Qt6.*_DIR$" OR SakuraVariable STREQUAL "SAKURA_QT_ROOT")
            string(REPLACE "${SakuraLegacyQtRoot}" "${SakuraRuntimeQtRoot}" SakuraUpdatedPath "${${SakuraVariable}}")
            if(NOT SakuraUpdatedPath STREQUAL "${${SakuraVariable}}" AND EXISTS "${SakuraUpdatedPath}")
                set(${SakuraVariable} "${SakuraUpdatedPath}" CACHE PATH "Qt runtime location" FORCE)
            endif()
        endif()
    endforeach()

    set(SAKURA_QT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Qt" CACHE PATH
        "Qt install root (aqt layout: <root>/<version>/<arch>)")

    if(DEFINED ENV{SAKURA_QT_ROOT} AND NOT "$ENV{SAKURA_QT_ROOT}" STREQUAL "")
        set(SAKURA_QT_ROOT "$ENV{SAKURA_QT_ROOT}" CACHE PATH "Qt install root" FORCE)
    endif()

    if(NOT Qt6_DIR)
        file(GLOB SAKURA_QT_CMAKE_CANDIDATES
            "${SAKURA_QT_ROOT}/*/msvc*_64/lib/cmake/Qt6"
            "${SAKURA_QT_ROOT}/*/mingw_64/lib/cmake/Qt6"
            "${SAKURA_QT_ROOT}/lib/cmake/Qt6"
        )
        if(SAKURA_QT_CMAKE_CANDIDATES)
            list(GET SAKURA_QT_CMAKE_CANDIDATES 0 SAKURA_QT_CMAKE_DIR)
            set(Qt6_DIR "${SAKURA_QT_CMAKE_DIR}" CACHE PATH "Qt6 CMake package dir" FORCE)
            get_filename_component(SAKURA_QT_PREFIX "${SAKURA_QT_CMAKE_DIR}/../../.." ABSOLUTE)
            list(PREPEND CMAKE_PREFIX_PATH "${SAKURA_QT_PREFIX}")
        endif()
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# find_package(Qt6) creates directory-scoped imported targets; promote so
# Runtime executables share the Qt targets discovered by Engine.
foreach(SakuraQtModule Core Gui Widgets QWindowsIntegrationPlugin QJpegPlugin)
    if(TARGET Qt6::${SakuraQtModule})
        get_target_property(SakuraQtIsGlobal Qt6::${SakuraQtModule} IMPORTED_GLOBAL)
        if(NOT SakuraQtIsGlobal)
            set_target_properties(Qt6::${SakuraQtModule} PROPERTIES IMPORTED_GLOBAL TRUE)
        endif()
    endif()
endforeach()

add_library(SakuraEngineQtThirdParty INTERFACE)
add_library(Sakura::EngineQtThirdParty ALIAS SakuraEngineQtThirdParty)
target_link_libraries(SakuraEngineQtThirdParty INTERFACE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

function(sakura_deploy_qt_runtime TargetName)
    if(NOT WIN32)
        return()
    endif()
    if(NOT TARGET Qt6::Core OR NOT TARGET Qt6::Gui OR NOT TARGET Qt6::Widgets)
        message(WARNING "sakura_deploy_qt_runtime(${TargetName}): Qt6 Core/Gui/Widgets targets missing")
        return()
    endif()
    if(NOT Qt6_DIR)
        message(WARNING "sakura_deploy_qt_runtime(${TargetName}): Qt6_DIR is empty")
        return()
    endif()

    add_custom_command(TARGET ${TargetName} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${TargetName}>/platforms"
            "$<TARGET_FILE_DIR:${TargetName}>/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::Core>"
            "$<TARGET_FILE:Qt6::Gui>"
            "$<TARGET_FILE:Qt6::Widgets>"
            "$<TARGET_FILE_DIR:${TargetName}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
            "$<TARGET_FILE_DIR:${TargetName}>/platforms/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::QJpegPlugin>"
            "$<TARGET_FILE_DIR:${TargetName}>/imageformats/"
        COMMENT "Copy Qt Core/Gui/Widgets runtime next to ${TargetName}"
        VERBATIM
    )
endfunction()
