cmake_minimum_required(VERSION 3.22)

project(AccurateNotationScanner VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    # Apple silicon only, macOS 11 and later (decision 0004).
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Apple silicon")
endif()

option(ANS_BUILD_APP     "Build the app (downloads JUCE)" ON)
option(ANS_BUILD_TESTS   "Build the tests" ON)
option(ANS_SYSTEM_MUPDF  "Use an installed MuPDF instead of building one" OFF)

include(cmake/MuPDF.cmake)

#==============================================================================
# The core: PDF in, MusicXML out. No JUCE, so it builds and tests in seconds.
#==============================================================================
add_library(ScannerCore STATIC
    Source/Core/Glyphs.cpp
    Source/Core/Shapes.cpp
    Source/Core/PdfReader.cpp
    Source/Core/Layout.cpp
    Source/Core/Reader.cpp
    Source/Core/MusicXml.cpp
    Source/Core/Midi.cpp
    Source/Core/Convert.cpp
    Source/Core/Chords.cpp
    Source/Core/Xml.cpp)
target_include_directories(ScannerCore PUBLIC Source/Core)
target_link_libraries(ScannerCore PUBLIC mupdf)
set_target_properties(ScannerCore PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(ScannerCore PRIVATE -Wall -Wextra)
endif()

# The command-line converter: what the tests and the accuracy check drive.
add_executable(notation-scan tools/notation-scan.cpp)
target_link_libraries(notation-scan PRIVATE ScannerCore)

if(ANS_BUILD_TESTS)
    enable_testing()
    add_executable(ScannerTests
        Tests/TestMain.cpp
        Tests/TestGlyphs.cpp
        Tests/TestShapes.cpp
        Tests/TestFixtures.cpp
        Tests/TestMidi.cpp
        Tests/TestChords.cpp)
    target_link_libraries(ScannerTests PRIVATE ScannerCore)
    target_compile_definitions(ScannerTests PRIVATE ANS_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/Tests/fixtures")
    add_test(NAME Core COMMAND ScannerTests)
endif()

if(NOT ANS_BUILD_APP)
    return()
endif()

include(cmake/App.cmake)
