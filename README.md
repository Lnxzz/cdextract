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

# Server
cdextract includes a server application which exposes a REST/json API which allows for easy integration within
other applications.
The specification of the API can be found in [doc/cdextract_server_api.md](doc/cdextract_server_api.md).
The server application uses a sqlite3 database to keep track of the disc information and audio tracks extracted.
It is also able to import and query a local cddb database such as the old Freedb database.
Configuration options can be supplied when starting the server. The following options are supported:

```
cdextract-server [option]...
options:
 -b                 background; start server as daemon process and run in the background
 -c<ccddb folder>   folder to import cddb data; default: '/tmp/cdda'
 -db<database file> database file; default: '/tmp/cdextract.db'
 -d<drive name>     cd-rom drive name; default auto detect
 -a<root folder>    root folder for extracted audio data; default: '/tmp/cdextract'
 -l<log file>       log file; default log to standard output
 -pid<pid file>     pid file; default: '/tmp/cdextract.pid'
 -p<port>           server port; default port: 8001
 -s                 backup database at startup
 -t<flac|wav>       flac or wav audio output; default: flac
 -i<0|1|2|3>        download cover images; 0=off;1=not to file;2=front and back;3=full default: 2
 -v                 verbose; use verbose messaging
 -h                 help; show command line options
```

---

# COPYING
This software is released under the LGPL v3 license.
See LICENSE for more information.



