cmake_minimum_required(VERSION 3.25)

if(WIN32)
    set(SakuraGitConfigurationCount 0)
    if(DEFINED ENV{GIT_CONFIG_COUNT})
        set(SakuraGitConfigurationCount "$ENV{GIT_CONFIG_COUNT}")
    endif()
    set(ENV{GIT_CONFIG_KEY_${SakuraGitConfigurationCount}} "core.longpaths")
    set(ENV{GIT_CONFIG_VALUE_${SakuraGitConfigurationCount}} "true")
    math(EXPR SakuraGitConfigurationCount "${SakuraGitConfigurationCount} + 1")
    set(ENV{GIT_CONFIG_COUNT} "${SakuraGitConfigurationCount}")
endif()

if(NOT WIN32)
    # Unix build scripts/data must retain LF even with autocrlf in user Git config.
    set(SakuraGitConfigurationCount 0)
    if(DEFINED ENV{GIT_CONFIG_COUNT})
        set(SakuraGitConfigurationCount "$ENV{GIT_CONFIG_COUNT}")
    endif()
    set(ENV{GIT_CONFIG_KEY_${SakuraGitConfigurationCount}} "core.autocrlf")
    set(ENV{GIT_CONFIG_VALUE_${SakuraGitConfigurationCount}} "false")
    math(EXPR SakuraGitConfigurationCount "${SakuraGitConfigurationCount} + 1")
    set(ENV{GIT_CONFIG_COUNT} "${SakuraGitConfigurationCount}")
endif()

project(SakuraNovel LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJC OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SAKURA_BUILD_EDITOR
    "Build the Qt editor application (Engine and EditorCore remain available)"
    ON)

include(CTest)

add_subdirectory(Root)
