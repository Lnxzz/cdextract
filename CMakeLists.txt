cmake_minimum_required(VERSION 3.28)

# set the project name
project(cdextract VERSION 0.1)

# add the cddparanoia III library
add_subdirectory(cdparanoia)

# add the cdextractlibrary
add_subdirectory(lib)

# add the (cpp) test application, the commandline utility and the server application
add_subdirectory(test)
add_subdirectory(util)
add_subdirectory(server)

# git auto ignore build directory
if(NOT EXISTS ${PROJECT_BINARY_DIR}/.gitignore)
  file(WRITE ${PROJECT_BINARY_DIR}/.gitignore "*")
endif()
