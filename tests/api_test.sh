#!/bin/sh
echo "connecting to drive"
wget -nv --method=POST http://127.0.0.1:8001/v1/drive --output-document=drive.json
wget -nv --method=GET -O - http://127.0.0.1:8001/v1/drive 2>&1

echo "getting disc information"
wget -nv --method=GET http://127.0.0.1:8001/v1/disc --output-document=disc.json
wget -nv --method=GET -O - http://127.0.0.1:8001/v1/disc 2>&1

echo "get front and back cover"
wget -nv --method=GET http://127.0.0.1:8001/v1/disc/front --header='Content-Type:image/jpg' --output-document=cover-front.jpg
wget -nv --method=GET http://127.0.0.1:8001/v1/disc/back --header='Content-Type:image/jpg' --output-document=cover-back.jpg

echo "update front and back cover"
wget -nv http://127.0.0.1:8001/v1/disc/front --header='Content-Type:image/jpg' --post-file=cover-front-test.jpg
wget -nv http://127.0.0.1:8001/v1/disc/back --header='Content-Type:image/jpg' --post-file=cover-back-test.jpg
wget -nv --method=GET http://127.0.0.1:8001/v1/disc/front --header='Content-Type:image/jpg' --output-document=cover-front-out.jpg
wget -nv --method=GET http://127.0.0.1:8001/v1/disc/back --header='Content-Type:image/jpg' --output-document=cover-back-out.jpg

echo "updating disc information"
wget -nv http://127.0.0.1:8001/v1/disc --header='Content-Type:application/json' --post-file=disc_body.json --output-document=disc_body_out.json
wget -nv http://127.0.0.1:8001/v1/disc --header='Content-Type:application/json' --post-file=disc_body_part.json --output-document=disc_body_part_out.json
wget -nv http://127.0.0.1:8001/v1/disc --header='Content-Type:application/json' --post-file=disc_body_notracks.json --output-document=disc_body_notracks_out.json

echo "extracting audio..."
wget -nv --method=POST -O - http://127.0.0.1:8001/v1/disc/extract

echo "getting progress.. (single response)"
wget -nv --timeout=60 --wait=10 -S -O - http://127.0.0.1:8001/v1/disc/extract 2>&1

echo "getting progress.. (streaming response)"
wget -nv --timeout=60 --wait=10 http://127.0.0.1:8001/v1/disc/extract?stream=1 --output-document=progress.json &

echo "sleeping for 20 seconds..."
sleep 5
wget -nv --timeout=60 --wait=10 -S -O - http://127.0.0.1:8001/v1/disc/extract 2>&1
sleep 5
wget -nv --timeout=60 --wait=10 -S -O - http://127.0.0.1:8001/v1/disc/extract 2>&1
sleep 5
wget -nv --timeout=60 --wait=10 -S -O - http://127.0.0.1:8001/v1/disc/extract 2>&1
sleep 5
wget -nv --timeout=60 --wait=10 -S -O - http://127.0.0.1:8001/v1/disc/extract 2>&1

echo "cancel audio extraction"
wget -nv --method=DELETE -O - http://127.0.0.1:8001/v1/disc/extract 2>&1

echo "drive status"
wget -nv --method=GET -O - http://127.0.0.1:8001/v1/drive 2>&1

echo "disc status"
wget -nv --method=GET -O - http://127.0.0.1:8001/v1/disc 2>&1

echo "progress status"
wget -nv --timeout=60 --wait=10 -S -O - http://127.0.0.1:8001/v1/disc/extract 2>&1

echo "get disc metadata from database"
wget -nv --method=GET -O - http://127.0.0.1:8001/v1/discs/meta 2>&1

echo "get disc list from database"
wget -nv --method=GET -O - http://127.0.0.1:8001/v1/discs 2>&1

echo "get disc information from database"
wget -nv --method=GET -O - http://127.0.0.1:8001/v1/discs/1 2>&1

echo "get front and back cover from database"
wget -nv --method=GET http://127.0.0.1:8001/v1/discs/1/front --header='Content-Type:image/jpg' --output-document=cover-front-from-db.jpg
wget -nv --method=GET http://127.0.0.1:8001/v1/discs/1/back --header='Content-Type:image/jpg' --output-document=cover-back-from-db.jpg

echo "update front and back cover"
wget -nv http://127.0.0.1:8001/v1/discs/1/front --header='Content-Type:image/jpg' --post-file=cover-front-test.jpg
wget -nv http://127.0.0.1:8001/v1/discs/1/back --header='Content-Type:image/jpg' --post-file=cover-back-test.jpg

echo "get audio track in wav and flac format"
wget --method=GET "http://127.0.0.1:8001/v1/discs/1/audio?track=1&format=wav" --output-document=track1.wav
wget --method=GET "http://127.0.0.1:8001/v1/discs/1/audio?track=1&format=flac" --output-document=track1.flac

echo "backup database"
wget -nv --method=POST -O - http://127.0.0.1:8001/v1/discs/backup 2>&1
wget -nv --method=GET -O - http://127.0.0.1:8001/v1/discs/backup 2>&1

echo "rescan database"
wget -nv --method=POST -O - http://127.0.0.1:8001/v1/discs/rescan 2>&1
wget -nv --method=GET -O - http://127.0.0.1:8001/v1/discs/rescan 2>&1

echo "rebuild database"
wget -nv --method=POST -O - http://127.0.0.1:8001/v1/discs/rebuild 2>&1
wget -nv --method=GET -O - http://127.0.0.1:8001/v1/discs/rebuild 2>&1

echo "done"
