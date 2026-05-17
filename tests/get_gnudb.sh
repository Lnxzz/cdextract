
#!/bin/bash

BASE_URL="https://gnudb.gnudb.org/~cddb/cddb.cgi"
HELLO="pi+cdplayer+cddb-tool+0.4.7"
PROTO="5"
TRACK_INFO="92093e0a+10+175+18469+34444+51154+70524+88841+104824+124686+140966+159454+2368"
QUERYDB="?cmd=cddb+query+$TRACK_INFO&hello=$HELLO&proto=$PROTO"
READDB="?cmd=cddb+read+data+92093e0a&hello=$HELLO&proto=$PROTO"

#wget -q -nv -e timestamping=off -O - 
#https://gnudb.gnudb.org/~cddb/cddb.cgi?cmd=cddb+query+92093e0a+10+175+18469+34444+51154+70524+88841+104824+124686+140966+159454+2368&hello=pi+cdplayer+cddb-tool+0.4.7&proto=5
echo "Quering: $BASE_URL$QUERYDB"
wget -q -nv -e timestamping=off -O cddb_query_response.txt $BASE_URL$QUERYDB


#wget -q -nv -e timestamping=off -O - 
#https://gnudb.gnudb.org/~cddb/cddb.cgi?cmd=cddb+read+data+92093e0a&hello=pi+cdplayer+cddb-tool+0.4.7&proto=5
echo "Quering: $BASE_URL$READDB"
wget -q -nv -e timestamping=off -O cddb_read_response.txt $BASE_URL$READDB




