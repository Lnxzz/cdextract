#!/bin/sh
# PRE: server is built and installed
DIR_PREFIX=/usr/local
$DIR_PREFIX/bin/cdextract-server -c$DIR_PREFIX/var/cdda -db$DIR_PREFIX/var/cdextract/cdextract.db -f$DIR_PREFIX/var/cdextract/data -a3 -v >/tmp/cdextract.log
