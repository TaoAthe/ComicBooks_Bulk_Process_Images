cmake_minimum_required(VERSION 3.16)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql Test)

# Locate windeployqt6 so test executables can run standalone on Windows.
if(WIN32)
    find_program(WINDEPLOYQT windeployqt6 windeployqt
        HINTS "${Qt6_DIR}/../../../bin"
              "$ENV{QTDIR}/bin"
    )
endif()

# Helper macro: deploy Qt DLLs next to a test target after every build.
macro(deploy_qt_for_test TARGET)
    if(WIN32 AND WINDEPLOYQT)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND "${WINDEPLOYQT}"
                    --no-compiler-runtime
                    --no-translations
                    "$<TARGET_FILE:${TARGET}>"
            COMMENT "Running windeployqt for ${TARGET}"
        )
    endif()
endmacro()

# ── ComicProcessor unit tests ──────────────────────────────────────────────
qt_add_executable(TestComicProcessor
    test_comicprocessor.cpp
    ../src/comicprocessor.cpp
    ../src/comicprocessor.h
)
target_include_directories(TestComicProcessor PRIVATE ../src)
target_link_libraries(TestComicProcessor PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Sql Qt6::Test)
deploy_qt_for_test(TestComicProcessor)
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
deploy_qt_for_test(TestEbayCsv)
add_test(NAME EbayCsvTests COMMAND TestEbayCsv)
