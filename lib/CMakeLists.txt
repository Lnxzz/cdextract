cmake_minimum_required(VERSION 3.28)

# set the project name
project(cdextract VERSION 0.1 DESCRIPTION "cdextract library")

# set C standard
set(CMAKE_C_STANDARD 11)

# set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

# build shared libraries
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# set rpath at installation
set(CMAKE_INSTALL_RPATH "/usr/local/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# add gdb debugging information and turn on all compiler warnings
add_compile_options(-g -Wall)

# add the library
add_library(cdextract STATIC
        ../src/report.c
        ../src/wav_writer.c
        ../src/flac_writer.c
        ../src/cue_sheet.c
        ../src/file_utils.c
        ../src/string_utils.c
        ../src/json_utils.c
        ../src/json_file.c
        ../src/base64.c
        ../src/sha1.c
        ../src/http_client.c
        ../src/cddb.c
        ../src/mb_api.c
        ../src/libcdextract.c)

# configure a header file to pass the version number to the source
configure_file(../src/config.h.in config.h)

# ensure the required libraries/packages are available
set(FLAC_LIBRARY "-lFLAC") 
find_library(FLAC_LIBRARY REQUIRED NAMES FLAC)

set(CURL_LIBRARY "-lcurl") 
find_package(CURL REQUIRED)

#set(COVERART_LIBRARY "-lcoverart") 
#find_library(COVERART_LIBRARY REQUIRED NAMES coverart)

find_package(OpenSSL REQUIRED)

# add libraries
list(APPEND EXTRA_LIBS "${FLAC_LIBRARY}" "${CURL_LIBRARY}" "${OPENSSL_LIBRARIES}" pthread m rt)
#list(APPEND EXTRA_LIBS "${FLAC_LIBRARY}" "${CURL_LIBRARY}" "${COVERART_LIBRARY}" "${OPENSSL_LIBRARIES}" pthread m rt)
#list(APPEND EXTRA_INCLUDES "/usr/local/include")

# add libraries to link with
target_link_libraries(cdextract PUBLIC cdparanoia::cdda_interface cdparanoia::cdda_paranoia ${EXTRA_LIBS})

# state that libcdextract need PIC when the default is shared libraries
set_target_properties(cdextract PROPERTIES POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})

# add the bin directory to the list of paths to search for the config include file
target_include_directories(cdextract PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_BINARY_DIR}/cdparanoia" "${CURL_LIBRARIES}")

# add compile definition for debug build (use: cmake -DCMAKE_BUILD_TYPE=Debug ..)
target_compile_definitions(cdextract PRIVATE $<$<CONFIG:Debug>: DEBUG=1>)

# add source directory to include path (to find headers)
target_include_directories(cdextract PUBLIC "../src/" "../cdparanoia/cdparanoia-III-10.2/interface/" "../cdparanoia/cdparanoia-III-10.2/paranoia/")

#add alias to link to libraries later to the executables
add_library(cdextract::cdextract ALIAS cdextract)

# installation
install(TARGETS cdextract DESTINATION lib)
install(FILES "${PROJECT_SOURCE_DIR}/src/libcdextract_types.h" DESTINATION include)
install(FILES "${PROJECT_SOURCE_DIR}/src/libcdextract.h" DESTINATION include)

# git auto ignore build directory
if(NOT EXISTS ${PROJECT_BINARY_DIR}/.gitignore)
  file(WRITE ${PROJECT_BINARY_DIR}/.gitignore "*")
endif()
