cmake_minimum_required(VERSION 3.16)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql Test)

# ── ComicProcessor unit tests ──────────────────────────────────────────────
qt_add_executable(TestComicProcessor
    test_comicprocessor.cpp
    ../src/comicprocessor.cpp
    ../src/comicprocessor.h
)
target_include_directories(TestComicProcessor PRIVATE ../src)
target_link_libraries(TestComicProcessor PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Sql Qt6::Test)

add_test(NAME ComicProcessorTests COMMAND TestComicProcessor)

# ── eBay CSV export tests ──────────────────────────────────────────────────
qt_add_executable(TestEbayCsv
    test_ebaycsv.cpp
    ../src/comicprocessor.cpp
    ../src/comicprocessor.h
)
target_include_directories(TestEbayCsv PRIVATE ../src)
target_link_libraries(TestEbayCsv PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Sql Qt6::Test)

add_test(NAME EbayCsvTests COMMAND TestEbayCsv)
