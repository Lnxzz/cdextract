#!/bin/sh
cp ../doc/cdextract_server_api.html /tmp/cdextract/index.html
cp ../doc/favicon.ico /tmp/cdextract/
../build/server/cdextract-server >/tmp/cdextract.log
