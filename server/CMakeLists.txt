cmake_minimum_required(VERSION 3.28)

# set the project name
project(cdextract VERSION 1.0)

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

# add the executable
add_executable(cdextract-server 
  ../src/flac_reader.c
  ../src/wav_reader.c
  timer.c
  db.c
  scan.c
  request.c
  response.c
  api.c
  handler.c 
  server.c)

# configure a header file to pass the version number to the source
configure_file(../src/config.h.in config.h)

# ensure the required libraries/packages are available
set(FLAC_LIBRARY "-lFLAC") 
find_library(FLAC_LIBRARY REQUIRED NAMES FLAC)

# check if the sqlite3 package is available
find_package(SQLite3 REQUIRED)

# ensure the microhttpd library is available
set(MHTD_LIBRARY "-lmicrohttpd") 
find_library(MHTD_LIBRARY REQUIRED NAMES microhttpd)

# add compile definition for debug build (use: cmake -DCMAKE_BUILD_TYPE=Debug ..)
target_compile_definitions(cdextract-server PRIVATE $<$<CONFIG:Debug>: DEBUG=1>)

# add cdextract library to link with the server application
target_link_libraries(cdextract-server PUBLIC cdextract::cdextract SQLite::SQLite3 "${MHTD_LIBRARY}")

# add source directory in which the linker should search for libraries when linking
target_link_directories(cdextract-server PUBLIC "/usr/local/lib")

# installation
install(TARGETS cdextract-server DESTINATION bin)
install(FILES "${PROJECT_SOURCE_DIR}/../doc/favicon.ico" DESTINATION var/cdextract)
install(FILES "${PROJECT_SOURCE_DIR}/../doc/cover.png" DESTINATION var/cdextract)
install(FILES "${PROJECT_SOURCE_DIR}/../doc/cover-back.png" DESTINATION var/cdextract)
install(FILES "${PROJECT_SOURCE_DIR}/../doc/cdextract_server_api.html" DESTINATION var/cdextract RENAME index.html)
install(FILES "${PROJECT_SOURCE_DIR}/../sample/cdextract.db" DESTINATION var/cdextract)
install(DIRECTORY DESTINATION var/cdextract/data DIRECTORY_PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_WRITE GROUP_READ GROUP_EXECUTE WORLD_WRITE WORLD_READ WORLD_EXECUTE)
install(DIRECTORY DESTINATION var/cdda DIRECTORY_PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_WRITE GROUP_READ GROUP_EXECUTE WORLD_WRITE WORLD_READ WORLD_EXECUTE)
install(FILES "${PROJECT_SOURCE_DIR}/../doc/cdextract_server.service" DESTINATION /usr/lib/systemd/system)

# enable test support
enable_testing()

# does the application run
add_test(NAME test_command COMMAND cdextract-server -v -t)

# git auto ignore build directory
if(NOT EXISTS ${PROJECT_BINARY_DIR}/.gitignore)
  file(WRITE ${PROJECT_BINARY_DIR}/.gitignore "*")
endif()
