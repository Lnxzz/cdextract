#!/bin/bash

BASE_URL="http://musicbrainz.org/ws/2"
QUERY_DISCID="/discid/7AST_ULsIJtCcxLWk8dR6fqY_8k-?toc=1+11+150+23467+37970+60127+80385+105650+130527+149445+168272+190252+212805+243406"
QUERY_RELEASE="/release/a0691875-152e-45a6-a30d-ab2b57c0648e?nc=recordings+artists"
FMT_JSON="&fmt=json"

echo "Quering: $BASE_URL$QUERY_DISCID"
wget -q -nv -e timestamping=off -O mb_discid_response.xml $BASE_URL$QUERY_DISCID

echo "Quering: $BASE_URL$QUERY_DISCID$FMT_JSON"
wget -q -nv -e timestamping=off -O mb_discid_response.json $BASE_URL$QUERY_DISCID$FMT_JSON

echo "Quering: $BASE_URL$QUERY_RELEASE"
wget -q -nv -e timestamping=off -O mb_release_response.xml $BASE_URL$QUERY_RELEASE

echo "Quering: $BASE_URL$QUERY_RELEASE$FMT_JSON"
wget -q -nv -e timestamping=off -O mb_release_response.json $BASE_URL$QUERY_RELEASE$FMT_JSON
