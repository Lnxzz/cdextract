# cdextract - cd audio extraction utilities
cdextract aims to be a simple but complete solution to extract audio from CD's

cdextract has the following features:
  - extract audio tracks from CD's
  - calculate the disc id
  - querying the musicbrainz and gnudb services for retrieving disc information usinf the calculated disc id
  - downloading coverart from the cover art archive
  - support for flac encoding the audio data
  - expose the functions through a REST/json API
  - keep track of extracted CD's and query a local cddb database

This application does not have all the advanced audio extraction features tools like cdparanoia and cdrtools have. 

cdextract uses the interface and paranoia libraries part of cdparanoia III (release 10.2) by Monty <monty@xiph.org>.

Before installing this application, make sure you installed cdparanoia III and the other dependencies first.

# Usage
The are 3 possible ways to use cdextract:
1. as server application exposing a rest/json api
2. as standalone command line utility
3. as C/C++ library to integrate within other applications

The command line utility is mainly used to test the audio meta information gathering (disc, track information and covers) and
the audio extraction process. Normal usage would be through the server application exposing and API and keeping track of discs 
extracted.

# Dependencies
1. c/c++ compiler and cmake
2. cdparanoia III (release 10.2)
3. libflac
4. libcurl
5. sqlite3
6. libmicrohttpd

For Debian/Ubuntu based systems you can use the following to install all the dependencies
except for the libcdparanoia library:
```
# sudo apt install build-essential cmake
# sudo apt install flac libflac-dev
# sudo apt install libcurl4 libcurl4-openssl-dev
# sudo apt install libsqlite3-0 libsqlite3-dev
# sudo apt install libmicrohttpd-dev
```
libcdparanoia is patched and compiled from source.
The patches improve the interoperability with C++ (private is used as variable name, while in c++ private is a reserved keyword) and
better frees allocated resources.
The script [cdparanoia/apply_patch.sh](cdparanoia/apply_patch.sh) takes care of downloading and patching the library source code.


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
cdextract includes a server application which exposes a REST/json API which allows for easy integration within
other applications.
The specification of the API can be found in [doc/cdextract_server_api.md](doc/cdextract_server_api.md).

---

# COPYING
This software is released under the LGPL v3 license.
See LICENSE for more information.



