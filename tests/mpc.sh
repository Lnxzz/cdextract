#!/bin/bash
mpdhost=192.168.3.241
mpdfolder=/mnt/NAS/audio/music/flac/Carl_Orff/Carmina_Burana/
mpc -h ${mpdhost} clear
mpc -h ${mpdhost} add ${mpdfolder}
mpc -h ${mpdhost} play
