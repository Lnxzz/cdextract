# cdextract - cd audio extraction library and application
===============================================================================

cdextract aims to be a simple but complete solution to extract audio from CD's

cdextract has the following features:
  - extract audio tracks from CD's
  - support to calculate the disc id
  - support for querying the musicbrainz and gnudb services for retrieving disc information
  - support for downloading coverart
  - support for flac encoding

This application does not have all the advanced features tools
like cdparanoia and cdrtools have. 

cdextract uses the interface and paranoia libraries part of 
cdparanoia III (release 10.2) by Monty <monty@xiph.org>.

Before installing this application, make sure you installed cdparanoia III and 
the other dependencies (libflac, libcurl) first.


# Dependencies
===============================================================================

1. cdparanoia III (release 10.2)
2. libflac
3. libcurl

For Debian/Ubuntu based systems you can use:
...
# sudo apt install libcdparanoia0 libcdparanoia-dev
# sudo apt install flac libflac-dev
# sudo apt install libcurl4 libcurl4-openssl-dev
...

# Building
===============================================================================

This project uses cmake to build the library and the application:
...
  mkdir build
  cd build/
  cmake ../
  make
  sudo make install
...

# COPYING
===============================================================================
This software is released under the LGPL v3 license.
See LICENSE for more information.

