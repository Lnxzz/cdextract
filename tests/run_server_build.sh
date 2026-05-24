#!/bin/sh
DIR_DATA=/tmp/cdextract
DIR_CDDA=/tmp/cdda

mkdir -p $DIR_DATA
mkdir -p $DIR_CDDA
cp ../doc/cdextract_server_api.html $DIR_DATA/index.html
cp ../doc/favicon.ico $DIR_DATA/
../build/server/cdextract-server -v -i3 >/tmp/cdextract.log
