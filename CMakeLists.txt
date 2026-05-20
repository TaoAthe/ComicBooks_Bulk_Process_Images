cmake_minimum_required(VERSION 3.16)
project(ComicVisionSorterStage1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql)

add_executable(ComicVisionSorterStage1
    src/main.cpp
    src/comicprocessor.cpp
    src/comicprocessor.h
    src/backendcontroller.cpp
    src/backendcontroller.h
    src/ebayexporter.cpp
    src/ebayexporter.h
    src/apiserver.cpp
    src/apiserver.h
    src/serviceapp.cpp
    src/serviceapp.h
)

target_link_libraries(ComicVisionSorterStage1 PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Sql)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)