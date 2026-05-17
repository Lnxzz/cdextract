#!/bin/sh
#valgrind --leak-check=full --show-leak-kinds=all ../build/util/cdextract-util
valgrind --leak-check=full --show-leak-kinds=all ../build/server/cdextract-server
#valgrind --leak-check=full --show-leak-kinds=all ../build/server/cdextract-server -twav -f/tmp/cdextract -db/tmp/cdextract_wav.db

