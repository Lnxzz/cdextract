cmake_minimum_required(VERSION 3.28)

# set the project name
project(cdparanoia VERSION 10.2 DESCRIPTION "cdparanoia-III library")

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

# set library version
set(VERSION "10.2")

# add compile options (including option to add gdb debugging information)
add_compile_options(-O2 -fsigned-char -g -Wno-unused-result)

# add the cdda_interface library
add_library(cdda_interface STATIC
        cdparanoia-III-10.2/interface/common_interface.c
        cdparanoia-III-10.2/interface/cooked_interface.c  
        cdparanoia-III-10.2/interface/scan_devices.c
        cdparanoia-III-10.2/interface/scsi_interface.c
        cdparanoia-III-10.2/interface/smallft.c
        cdparanoia-III-10.2/interface/test_interface.c
        cdparanoia-III-10.2/interface/toc.c
        cdparanoia-III-10.2/interface/interface.c)

# add the cdda_paranoia library
add_library(cdda_paranoia STATIC
        cdparanoia-III-10.2/paranoia/gap.c
        cdparanoia-III-10.2/paranoia/isort.c
        cdparanoia-III-10.2/paranoia/overlap.c
        cdparanoia-III-10.2/paranoia/p_block.c
        cdparanoia-III-10.2/paranoia/paranoia.c)

# add libraries
list(APPEND EXTRA_LIBS m rt)
list(APPEND EXTRA_INCLUDES "/usr/local/include")

# add libraries to link with
target_link_libraries(cdda_interface PUBLIC ${EXTRA_LIBS})

# state that libcdextract need PIC when the default is shared libraries
set_target_properties(cdda_interface PROPERTIES POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})

# add compile definition for debug build (use: cmake -DCMAKE_BUILD_TYPE=Debug ..)
target_compile_definitions(cdda_interface PRIVATE $<$<CONFIG:Debug>: DEBUG=1>)

# add source directory to include path (to find headers)
target_include_directories(cdda_interface PUBLIC "cdparanoia-III-10.2/interface/")

# add libraries to link with
target_link_libraries(cdda_paranoia PUBLIC ${EXTRA_LIBS})

# state that libcdextract need PIC when the default is shared libraries
set_target_properties(cdda_paranoia PROPERTIES POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})

# add compile definition for debug build (use: cmake -DCMAKE_BUILD_TYPE=Debug ..)
target_compile_definitions(cdda_paranoia PRIVATE $<$<CONFIG:Debug>: DEBUG=1>)

# add source directory to include path (to find headers)
target_include_directories(cdda_paranoia PUBLIC "cdparanoia-III-10.2/interface/")

# add alias to link to libraries later to the cdextract library
add_library(cdparanoia::cdda_interface ALIAS cdda_interface)
add_library(cdparanoia::cdda_paranoia ALIAS cdda_paranoia)

# git auto ignore build directory
if(NOT EXISTS ${PROJECT_BINARY_DIR}/.gitignore)
  file(WRITE ${PROJECT_BINARY_DIR}/.gitignore "*")
endif()
