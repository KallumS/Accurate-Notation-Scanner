# MuPDF, the PDF library (decision 0003).
#
# By default it is built from its own source, pinned to one release, as two
# static libraries - so the Mac app carries it inside and needs nothing
# installed. The source comes from GitHub with its third-party libraries.
#
# -DANS_SYSTEM_MUPDF=ON uses an installed MuPDF instead (Ubuntu's
# libmupdf-dev), which is quicker for working on Linux.

set(ANS_MUPDF_TAG "1.26.9" CACHE STRING "MuPDF release to build")

if(ANS_SYSTEM_MUPDF)
    find_path(MUPDF_INCLUDE_DIR mupdf/fitz.h REQUIRED)
    find_library(MUPDF_LIBRARY NAMES libmupdf.a mupdf REQUIRED)
    find_library(MUPDF_THIRD_LIBRARY NAMES libmupdf-third.a mupdf-third)
    add_library(mupdf INTERFACE)
    target_include_directories(mupdf INTERFACE ${MUPDF_INCLUDE_DIR})
    target_link_libraries(mupdf INTERFACE ${MUPDF_LIBRARY})
    if(MUPDF_THIRD_LIBRARY)
        target_link_libraries(mupdf INTERFACE ${MUPDF_THIRD_LIBRARY})
    endif()
    # Ubuntu's MuPDF links its third-party libraries from the system.
    foreach(lib harfbuzz freetype jbig2dec openjp2 jpeg lcms2 gumbo mujs z m)
        find_library(MUPDF_DEP_${lib} NAMES ${lib})
        if(MUPDF_DEP_${lib})
            target_link_libraries(mupdf INTERFACE ${MUPDF_DEP_${lib}})
        endif()
    endforeach()
    return()
endif()

include(ExternalProject)

set(MUPDF_SRC ${CMAKE_BINARY_DIR}/mupdf-src)
set(MUPDF_OUT ${MUPDF_SRC}/build/release)

# Only what reading PDFs needs: no other document types, no JavaScript,
# no OCR, and none of the large fallback fonts (a score embeds its own).
set(MUPDF_XCFLAGS "-DTOFU -DTOFU_CJK -DFZ_ENABLE_JS=0 -DFZ_ENABLE_HTML=0 -DFZ_ENABLE_EPUB=0 -DFZ_ENABLE_XPS=0 -DFZ_ENABLE_SVG=0 -DFZ_ENABLE_CBZ=0 -DFZ_ENABLE_IMG=0 -DFZ_ENABLE_FB2=0 -DFZ_ENABLE_MOBI=0 -DFZ_ENABLE_OFFICE=0 -DFZ_ENABLE_DOCX_OUTPUT=0 -DFZ_ENABLE_ODT_OUTPUT=0 -DFZ_ENABLE_BROTLI=0")
if(APPLE)
    string(APPEND MUPDF_XCFLAGS " -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET} -arch arm64")
else()
    string(APPEND MUPDF_XCFLAGS " -fPIC")
endif()

include(ProcessorCount)
ProcessorCount(NCPU)
if(NCPU EQUAL 0)
    set(NCPU 4)
endif()

ExternalProject_Add(mupdf_build
    PREFIX ${CMAKE_BINARY_DIR}/mupdf
    SOURCE_DIR ${MUPDF_SRC}
    DOWNLOAD_COMMAND
        sh -c "[ -f '${MUPDF_SRC}/Makefile' ] || git clone --quiet --depth 1 --branch ${ANS_MUPDF_TAG} --recurse-submodules --shallow-submodules https://github.com/ArtifexSoftware/mupdf.git '${MUPDF_SRC}'"
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE 1
    BUILD_COMMAND
        make -j${NCPU} build=release HAVE_X11=no HAVE_GLUT=no HAVE_CURL=no HAVE_TESSERACT=no HAVE_LEPTONICA=no
             USE_SYSTEM_LIBS=no "XCFLAGS=${MUPDF_XCFLAGS}" libs
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${MUPDF_OUT}/libmupdf.a ${MUPDF_OUT}/libmupdf-third.a
    LOG_DOWNLOAD ON
    LOG_BUILD ON
    LOG_OUTPUT_ON_FAILURE ON)

file(MAKE_DIRECTORY ${MUPDF_SRC}/include)
add_library(mupdf INTERFACE)
add_dependencies(mupdf mupdf_build)
target_include_directories(mupdf INTERFACE ${MUPDF_SRC}/include)
target_link_libraries(mupdf INTERFACE ${MUPDF_OUT}/libmupdf.a ${MUPDF_OUT}/libmupdf-third.a m)
