cmake_minimum_required(VERSION 3.28)
project(cdextractpp)

# set C standard
set(CMAKE_C_STANDARD 11)

# set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# build shared libraries
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# set rpath at installation
set(CMAKE_INSTALL_RPATH "/usr/local/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# add gdb debugging information
add_compile_options(-g)

# configure a header file to pass the version number to the source
configure_file(../src/config.h.in config.h)

## add the c++ wrapper for the cdextract library
add_library(cdextract-libpp STATIC
        ../src/libcdextractpp.cpp)

# add compile definition for debug build (use: cmake -DCMAKE_BUILD_TYPE=Debug ..)
target_compile_definitions(cdextract-libpp PRIVATE $<$<CONFIG:Debug>: DEBUG=1>)

# add the c library to link with the c++ wrapper library
target_link_libraries(cdextract-libpp PUBLIC cdextract::cdextract)

## add executables
add_executable(cdextract-cpp cdextract.cpp)

# add compile definition for debug build (use: cmake -DCMAKE_BUILD_TYPE=Debug ..)
target_compile_definitions(cdextract-cpp PRIVATE $<$<CONFIG:Debug>: DEBUG=1>)

# add cdextract c++ wrapper library to link with the c++ test application
target_link_libraries(cdextract-cpp cdextract-libpp)

# add source directory to include path (to find headers)
target_include_directories(cdextract-cpp PUBLIC "../src/")

# installation
install(TARGETS cdextract-libpp DESTINATION lib)
install(TARGETS cdextract-cpp DESTINATION bin)
