# cdextract - cd audio extraction library and utilities
cdextract aims to be a simple but complete solution to extract audio from CD's

cdextract has the following features:
  - extract audio tracks from CD's
  - support to calculate the disc id
  - support for querying the musicbrainz and gnudb services for retrieving disc information
  - support for downloading coverart
  - support for flac encoding

This application does not have all the advanced features tools like cdparanoia and cdrtools have. 

cdextract uses the interface and paranoia libraries part of cdparanoia III (release 10.2) by Monty <monty@xiph.org>.

Before installing this application, make sure you installed cdparanoia III and 
the other dependencies (libflac, libcurl) first.

# Usage
The are 3 possible ways to use cdextract:
1. as standalone command line utility
2. as server application exposing a rest/json api
3. as C/C++ library to integrate within other applications


# Dependencies
1. c/c++ compiler and cmake
2. cdparanoia III (release 10.2)
3. libflac
4. libcurl
5. sqlite3

For Debian/Ubuntu based systems you can use:
```
# sudo apt install build-essential cmake
# sudo apt install libcdparanoia0 libcdparanoia-dev
# sudo apt install flac libflac-dev
# sudo apt install libcurl4 libcurl4-openssl-dev
# sudo apt install libsqlite3-0 libsqlite3-dev
```

# Building
This project uses cmake to build the library, the cli and server applications.
Use the standard cmake approach for building and installing:
```
  mkdir build
  cd build/
  cmake ../
  make
  sudo make install
```

# Server application
cdextract includes a server application which exposes a rest/json api which allows for easy integration within
other applications.
The specification of the API can be found in [doc/cdextract_server_api.md](doc/cdextract_server_api.md).

---

# COPYING
This software is released under the LGPL v3 license.
See LICENSE for more information.



