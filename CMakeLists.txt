cmake_minimum_required(VERSION 3.25)
project(SakuraNovel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SAKURA_BUILD_EDITOR
    "Build the Qt editor application (Engine and EditorCore remain available)"
    ON)

include(CTest)

add_subdirectory(Root)
