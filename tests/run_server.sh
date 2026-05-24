#!/bin/sh
# PRE: server is built and installed
DIR_PREFIX=/usr/local
$DIR_PREFIX/bin/cdextract-server -c$DIR_PREFIX/var/cdda -db$DIR_PREFIX/var/cdextract/cdextract.db -a$DIR_PREFIX/var/cdextract/data -i3 -v >/tmp/cdextract.log
