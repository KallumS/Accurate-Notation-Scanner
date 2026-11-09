# Clones MuPDF's pinned release with its third-party libraries, once.
#   cmake -DSRC=<folder> -DTAG=<release> -P FetchMuPDF.cmake
if(EXISTS "${SRC}/Makefile")
    return()
endif()
file(REMOVE_RECURSE "${SRC}")
execute_process(
    COMMAND git clone --quiet --depth 1 --branch ${TAG} --recurse-submodules --shallow-submodules
            https://github.com/ArtifexSoftware/mupdf.git "${SRC}"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Could not fetch MuPDF ${TAG} from GitHub (is the computer online?)")
endif()
