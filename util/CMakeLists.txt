cmake_minimum_required(VERSION 3.28)

# set the project name
project(cdextract VERSION 0.1)

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

# add gdb debugging information
add_compile_options(-g)

# add the executable
add_executable(cdextract-util cdextract.c)

# configure a header file to pass the version number to the source
configure_file(../src/config.h.in config.h)

# add compile definition for debug build (use: cmake -DCMAKE_BUILD_TYPE=Debug ..)
target_compile_definitions(cdextract-util PRIVATE $<$<CONFIG:Debug>: DEBUG=1>)

# add cdextract library to link with the test utility
target_link_libraries(cdextract-util PUBLIC cdextract::cdextract)

# add source directory to include path (to find headers)
target_include_directories(cdextract PUBLIC "../src/")

# installation
install(TARGETS cdextract-util DESTINATION bin)

# enable test support
enable_testing()

# does the application run
add_test(NAME test_command COMMAND cdextract-util -v -t)

# git auto ignore build directory
if(NOT EXISTS ${PROJECT_BINARY_DIR}/.gitignore)
  file(WRITE ${PROJECT_BINARY_DIR}/.gitignore "*")
endif()
