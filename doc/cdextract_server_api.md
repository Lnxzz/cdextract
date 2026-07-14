# cdextract server API
The server application of cdextract exposes its functionality via a REST based API. 
Resources are available via the following structure:

`http://<host>:<port>/<version>/<resource>/[id]`

## port

The server can be made available on any port.
The default port is **8001**.

## version

The current supported version is **v1**.

## resources

The supported resources are:
* **drive**
* **disc**
* **discs**

More information can be found in the API definition [cdextract_server_api.yaml](cdextract_server_api.yaml) and its [html documentation](cdextract_server_api.html).


## drive
The drive endpoint controls the connection with cdrom drive.
Supported drive operations are:
* **GET /drive** - get the status of the cdrom drive
* **POST /drive** - open the connection with the cdrom drive
* **DELETE /drive** - close the connection with the cdrom drive

### Get status
Get the status of the cdrom drive

#### end point: `http://<host>:<port>/v1/drive`
#### request method: **GET**

#### example:
**request**

``
wget --method=GET http://localhost:8001/v1/drive
``

**response**

Success
```
HTTP/1.1 200 OK
Server: cdextract-server
Content-Length: 363
Content-Type: application/json

{
  "device": "/dev/cdrom",
  "status": 2,
  "root": "/tmp/cdextract",
  "folder": "/tmp/cdextract/Artist/Album",
  "verbose": 0,
  "output_type": 1,
  "download_coverart": 2,
  "search_drive": 0,
  "cd_speed": 0,
  "max_retries": 20,
  "abort_on_skip": 0,
  "eject_when_done": 0,
  "write_json": 1,
  "write_cue_sheet": 1,
  "show_disc_info": 1,
  "virtual_drive": 0,
  "has_drive": 1,
  "opened": 1
}
```

Failure
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### Open connection
Open the connection with the cdrom drive.

#### end point: `http://<host>:<port>/v1/drive`
#### request method: **POST**

#### example:
**request**

``
wget --method=POST http://localhost:8001/v1/drive
``

**response**

Success
```
HTTP/1.1 200 OK
Server: cdextract-server
Content-Length: 363
Content-Type: application/json

{
  "device": "/dev/cdrom",
  "status": 2,
  "root": "/tmp/cdextract",
  "folder": "/tmp/cdextract/Artist/Album",
  "verbose": 0,
  "output_type": 1,
  "download_coverart": 2,
  "search_drive": 0,
  "cd_speed": 0,
  "max_retries": 20,
  "abort_on_skip": 0,
  "eject_when_done": 0,
  "write_json": 1,
  "write_cue_sheet": 1,
  "show_disc_info": 1,
  "virtual_drive": 0,
  "has_drive": 1,
  "opened": 1
}
```

Failure
```
HTTP/1.1 404 Not Found
Server: cdextract-server
Content-Length: 51
Content-Type: application/json

{
  "code": 404
  "message": "Resource not found"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### Close connection
Close the connection with the cdrom drive.

#### end point: `http://<host>:<port>/v1/drive`
#### request method: **DELETE**

#### example:
**request**

``
wget --method=DELETE http://localhost:8001/v1/drive
``

**response**

Success
```
HTTP/1.1 204 No Content
Server: cdextract-server
Content-Length: 0

```

Failure
```
HTTP/1.1 404 Not Found
Server: cdextract-server
Content-Length: 51
Content-Type: application/json


{
  "code": 404
  "message": "Resource not found"
}
```
```
HTTP/1.1 423 Locked
Server: cdextract-server
Content-Length: 59
Content-Type: application/json

{
  "code": 423
  "message": "Locked. Resource is locked"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

## disc
The disc endpoint controls the current disc in the cdrom drive
Supported disc operations are:
* **GET /disc** - get the information of the disc in the cdrom drive
* **POST /disc** - edit the disc information
* **PUT /disc** - insert a disc into the cdrom drive
* **DELETE /disc** - eject the disc into the cdrom drive
* **GET /disc/extract** - get the disc extraction progress status
* **POST /disc/extract** - extract audio from the disc
* **DELETE /disc/extract** - cancel the extraction of audio from the disc
* **GET /disc/front** - get the front cover of the disc in the cdrom drive
* **POST /disc/front** - update the front cover of the disc in the cdrom 
* **GET /disc/back** - get the back cover of the disc in the cdrom drive
* **POST /disc/back** - update the back cover of the disc in the cdrom drive
* **GET /disc/audio** - get the audio data of the disc in the cdrom drive

### get disc information
Get the information of the disc in the cdrom drive.

#### end point: `http://<host>:<port>/v1/disc`
#### request method: **GET**

#### example:
**request**

``
wget --method=GET http://localhost:8001/v1/disc?overwrite=1&fuzzy=0
``

**response**

Success
```
HTTP/1.1 200 OK
Server: cdextract-server
Content-Length: 4050
Content-Type: application/json

{
  "id": 1,
  "disc_id": "92093e0a",
  "length": 177493,
  "lookup": "0ab5552c1aad7fa1",
  "artist": "James Blunt",
  "title": "Back To Bedlam",
  "genre": "Singer Songwriter",
  "year": 2005,
  "extended": "",
  "cddb_query": "92093e0a+10+175+18469+34444+51154+70524+88841+104824+124686+140966+159454+2368",
  "cddb_category": "data",
  "cddb_entry_id": "96093e87",
  "cddb_disc_id": "96093e0a",
  "cddb_revision": 0,
  "cddb_complete": 1,
  "mb_query": "1+10+175+18469+34444+51154+70524+88841+104824+124686+140966+159454+177667",
  "mb_fuzzy_lookup": "1+10+177667+175+18469+34444+51154+70524+88841+104824+124686+140966+159454",
  "mb_disc_id": "BM0fleBGaH5TzPGp1jBh4s.VwpU-",
  "mb_release_id": "54dca467-331b-4df9-9ff0-97a9d970cfd2",
  "mb_front_cover_size": 162064,
  "mb_back_cover_size": 219025,
  "mb_complete": 1,
  "extracted": 0,
  "track_count": 10,
  "tracks": [
    {
      "num": 1,
      "length": 18294,
      "title": "High",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/01.High.flac",
      "skipped": 0
    },
    {
      "num": 2,
      "length": 15975,
      "title": "You're Beautiful",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/02.You're Beautiful.flac",
      "skipped": 0
    },
    {
      "num": 3,
      "length": 16710,
      "title": "Wisemen",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer/Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "/James Blunt/Back To Bedlam (2005)/03.Wisemen.flac",
      "skipped": 0
    },
    {
      "num": 4,
      "length": 19370,
      "title": "Goodbye My Lover",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/04.Goodbye My Lover.flac",
      "skipped": 0
    },
    {
      "num": 5,
      "length": 18317,
      "title": "Tears And Rain",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer/Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "/James Blunt/Back To Bedlam (2005)/05.Tears And Rain.flac",
      "skipped": 0
    },
    {
      "num": 6,
      "length": 15983,
      "title": "Out Of My Mind",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/06.Out Of My Mind.flac",
      "skipped": 0
    },
    {
      "num": 7,
      "length": 19862,
      "title": "So Long, Jimmy",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/07.So Long, Jimmy.flac",
      "skipped": 0
    },
    {
      "num": 8,
      "length": 16280,
      "title": "Billy",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/08.Billy.flac",
      "skipped": 0
    },
    {
      "num": 9,
      "length": 18488,
      "title": "Cry",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer/Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "/James Blunt/Back To Bedlam (2005)/09.Cry.flac",
      "skipped": 0
    },
    {
      "num": 10,
      "length": 18214,
      "title": "No Bravery",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/10.No Bravery.flac",
      "skipped": 0
    }
  ]
}
```

Failure
```
HTTP/1.1 404 Not Found
Server: cdextract-server
Content-Length: 51
Content-Type: application/json

{
  "code": 404
  "message": "Resource not found"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### edit disc information
Edit the information of the current disc in the cdrom drive.
Note: ensure all the audio files of the disc are stored in the same folder.

#### end point: `http://<host>:<port>/v1/disc`
#### request method: **POST**

#### example:
**request**

``
wget --method=POST --body-file=disc_body.json http://localhost:8001/v1/disc
``
```
{
  "id": 1,
  "disc_id": "92093e0a",
  "length": 177493,
  "lookup": "0ab5552c1aad7fa1",
  "artist": "James Blunt",
  "title": "Back To Bedlam",
  "genre": "Singer Songwriter",
  "year": 2005,
  "extended": "",
  "tracks": [
    {
      "num": 9,
      "length": 246,
      "title": "Cry",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer/Songwriter",
      "year": "2005",
      "extended": "",
      "filename": "/James Blunt/Back To Bedlam (2005)/09.Cry.flac"
    },
    {
      "num": 3,
      "length": 222,
      "title": "Wisemen",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer/Songwriter",
      "year": "2005",
      "extended": "",
      "filename": "/James Blunt/Back To Bedlam (2005)/03.Wisemen.flac"
    },
    {
      "num": 5,
      "length": 244,
      "title": "Tears And Rain",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer/Songwriter",
      "year": "2005",
      "extended": "",
      "filename": "/James Blunt/Back To Bedlam (2005)/05.Tears And Rain.flac"
    }
  ]
}
```

**response**

Success
```
HTTP/1.1 200 OK
Server: cdextract-server
Content-Length: 4050
Content-Type: application/json

{
  "id": 1,
  "disc_id": "92093e0a",
  "length": 177493,
  "lookup": "0ab5552c1aad7fa1",
  "artist": "James Blunt",
  "title": "Back To Bedlam",
  "genre": "Singer Songwriter",
  "year": 2005,
  "extended": "",
  "cddb_query": "92093e0a+10+175+18469+34444+51154+70524+88841+104824+124686+140966+159454+2368",
  "cddb_category": "data",
  "cddb_entry_id": "96093e87",
  "cddb_disc_id": "96093e0a",
  "cddb_revision": 0,
  "cddb_complete": 1,
  "mb_query": "1+10+175+18469+34444+51154+70524+88841+104824+124686+140966+159454+177667",
  "mb_fuzzy_lookup": "1+10+177667+175+18469+34444+51154+70524+88841+104824+124686+140966+159454",
  "mb_disc_id": "BM0fleBGaH5TzPGp1jBh4s.VwpU-",
  "mb_release_id": "54dca467-331b-4df9-9ff0-97a9d970cfd2",
  "mb_front_cover_size": 162064,
  "mb_back_cover_size": 219025,
  "mb_complete": 1,
  "extracted": 0,
  "track_count": 10,
  "tracks": [
    {
      "num": 1,
      "length": 18294,
      "title": "High",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/01.High.flac",
      "skipped": 0
    },
    {
      "num": 2,
      "length": 15975,
      "title": "You're Beautiful",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/02.You're Beautiful.flac",
      "skipped": 0
    },
    {
      "num": 3,
      "length": 16710,
      "title": "Wisemen",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer/Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "/James Blunt/Back To Bedlam (2005)/03.Wisemen.flac",
      "skipped": 0
    },
    {
      "num": 4,
      "length": 19370,
      "title": "Goodbye My Lover",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/04.Goodbye My Lover.flac",
      "skipped": 0
    },
    {
      "num": 5,
      "length": 18317,
      "title": "Tears And Rain",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer/Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "/James Blunt/Back To Bedlam (2005)/05.Tears And Rain.flac",
      "skipped": 0
    },
    {
      "num": 6,
      "length": 15983,
      "title": "Out Of My Mind",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/06.Out Of My Mind.flac",
      "skipped": 0
    },
    {
      "num": 7,
      "length": 19862,
      "title": "So Long, Jimmy",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/07.So Long, Jimmy.flac",
      "skipped": 0
    },
    {
      "num": 8,
      "length": 16280,
      "title": "Billy",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/08.Billy.flac",
      "skipped": 0
    },
    {
      "num": 9,
      "length": 18488,
      "title": "Cry",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer/Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "/James Blunt/Back To Bedlam (2005)/09.Cry.flac",
      "skipped": 0
    },
    {
      "num": 10,
      "length": 18214,
      "title": "No Bravery",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer Songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/10.No Bravery.flac",
      "skipped": 0
    }
  ]
}
```

Failure
```
HTTP/1.1 404 Not Found
Server: cdextract-server
Content-Length: 51
Content-Type: application/json

{
  "code": 404
  "message": "Resource not found"
}
```
```
HTTP/1.1 409 Conflict
Server: cdextract-server
Content-Length: 66
Content-Type: application/json

{
  "code": 409
  "message": "Conflict. Resource state conflict"
}
```
```
HTTP/1.1 423 Locked
Server: cdextract-server
Content-Length: 59
Content-Type: application/json

{
  "code": 423
  "message": "Locked. Resource is locked"
}
```
```
HTTP/1.1 500 Internal Server Error
Server: cdextract-server
Content-Length: 54
Content-Type: application/json

{
  "code": 500
  "message": "Internal Server Error"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### insert disc
Insert a disc into the cdrom drive.

#### end point: `http://<host>:<port>/v1/disc`
#### request method: **PUT**

#### example:
**request**

``
wget --method=PUT http://localhost:8001/v1/disc
``

**response**

Success
```
HTTP/1.1 204 No Content
Server: cdextract-server
Content-Length: 0

```

Failure
```
HTTP/1.1 404 Not Found
Server: cdextract-server
Content-Length: 51
Content-Type: application/json

{
  "code": 404
  "message": "Resource not found"
}
```
```
HTTP/1.1 423 Locked
Server: cdextract-server
Content-Length: 59
Content-Type: application/json

{
  "code": 423
  "message": "Locked. Resource is locked"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### eject disc
Eject the current disc from the cdrom drive.

#### end point: `http://<host>:<port>/v1/disc`
#### request method: **DELETE**

#### example:
**request**

``
wget --method=DELETE http://localhost:8001/v1/disc
``

**response**

Success
```
HTTP/1.1 204 No Content
Server: cdextract-server
Content-Length: 0

```

Failure
```
HTTP/1.1 404 Not Found
Server: cdextract-server
Content-Length: 51
Content-Type: application/json

{
  "code": 404
  "message": "Resource not found"
}
```
```
HTTP/1.1 423 Locked
Server: cdextract-server
Content-Length: 59
Content-Type: application/json

{
  "code": 423
  "message": "Locked. Resource is locked"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### audio extraction progress
Get the disc extraction progress status.

#### end point: `http://<host>:<port>/v1/disc/extract`
#### request method: **GET**

#### example:
**request**

For a single response:
``
wget --method=GET http://localhost:8001/v1/disc/extract
``

For a streaming response with multiple updates use:
``
wget --method=GET http://localhost:8001/v1/disc/extract?stream=1
``

**response**

Success
```
HTTP/1.1 200 OK
Server: cdextract-server
Content-Length: 4467
Content-Type: application/json

{"function": "0", "track": 1, "sector": 5444, "percentage": 29.6, "message": ""}
```

Failure
```
HTTP/1.1 404 Not Found
Server: cdextract-server
Content-Length: 51
Content-Type: application/json

{
  "code": 404
  "message": "Resource not found"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### extract audio from the disc
Extract the audio from the current disc in the cdrom drive.

#### end point: `http://<host>:<port>/v1/disc/extract`
#### request method: **POST**

#### example:
**request**

``
wget --method=POST http://localhost:8001/v1/disc/extract
``

**response**

Success
```
HTTP/1.1 200 OK
Server: cdextract-server
Content-Length: 4467
Content-Type: application/json

{"function": "0", "track": 1, "sector": 5444, "percentage": 29.6, "message": ""}
```

Failure
```
HTTP/1.1 404 Not Found
Server: cdextract-server
Content-Length: 51
Content-Type: application/json

{
  "code": 404
  "message": "Resource not found"
}
```
```
HTTP/1.1 423 Locked
Server: cdextract-server
Content-Length: 59
Content-Type: application/json

{
  "code": 423
  "message": "Locked. Resource is locked"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### cancel disc extract
Cancel the extraction of audio from the current disc in the cdrom drive.

#### end point: `http://<host>:<port>/v1/disc/extract`
#### request method: **DELETE**

#### example:
**request**

``
wget --method=DELETE http://localhost:8001/v1/disc/extract
``

**response**

Success
```
HTTP/1.1 201 Created
Server: cdextract-server
Content-Length: 0
Content-Type: application/json

```

Failure
```
HTTP/1.1 404 Not Found
Server: cdextract-server
Content-Length: 51
Content-Type: application/json

{
  "code": 404
  "message": "Resource not found"
}
```
```
HTTP/1.1 423 Locked
Server: cdextract-server
Content-Length: 59
Content-Type: application/json

{
  "code": 423
  "message": "Locked. Resource is locked"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### get front cover
Get the front cover of the current disc in the cdrom drive.

#### end point: `http://<host>:<port>/v1/disc/front`
#### request method: **GET**

#### example:
**request**

``
wget --method=GET http://localhost:8001/v1/disc/front
``

**response**

Success
```
<binary data>

```

Failure
```
HTTP/1.1 404 Not Found
Server: cdextract-server
Content-Length: 51
Content-Type: application/json

{
  "code": 404
  "message": "Resource not found"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### update front cover
Update the front cover of the current disc in the cdrom drive.

#### end point: `http://<host>:<port>/v1/disc/front`
#### request method: **POST**

#### example:
**request**

``
wget --method=POST --body-file=cover.jpg http://localhost:8001/v1/disc/front
``

**response**

Success
```
HTTP/1.1 204 No Content
Server: cdextract-server
Content-Length: 0

```

Failure
```
HTTP/1.1 400 Bad Request
Server: cdextract-server
Content-Length: 59
Content-Type: application/json

{
  "code": 400
  "message": "Bad Request"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### get back cover
Get the back cover of the current disc in the cdrom drive.

#### end point: `http://<host>:<port>/v1/disc/back`
#### request method: **GET**

#### example:
**request**

``
wget --method=GET http://localhost:8001/v1/disc/back
``

**response**

Success
```
<binary data>

```

Failure
```
HTTP/1.1 404 Not Found
Server: cdextract-server
Content-Length: 51
Content-Type: application/json

{
  "code": 404
  "message": "Resource not found"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### update back cover
Update the back cover of the current disc in the cdrom drive.

#### end point: `http://<host>:<port>/v1/disc/back`
#### request method: **POST**

#### example:
**request**

``
wget --method=POST --body-file=back-cover.jpg http://localhost:8001/v1/disc/back
``

**response**

Success
```
HTTP/1.1 204 No Content
Server: cdextract-server
Content-Length: 0

```

Failure
```
HTTP/1.1 400 Bad Request
Server: cdextract-server
Content-Length: 59
Content-Type: application/json

{
  "code": 400
  "message": "Bad Request"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### get audio data
Get the audio data of the disc in the cdrom drive.

#### end point: `http://<host>:<port>/v1/disc/audio`
#### request method: **GET**

#### example:
**request**

``
wget --method=GET http://localhost:8001/v1/disc/audio?track=1&format=flac
``

**response**

Success
```
<binary data>

```

Failure
```
HTTP/1.1 404 Not Found
Server: cdextract-server
Content-Length: 51
Content-Type: application/json

{
  "code": 404
  "message": "Resource not found"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```


## discs
The discs endpoint controls stored disc information
Supported disc operations are:
* **GET /discs** - get a (filtered) list of stored discs
* **GET /discs/meta** - get the metadata for the stored discs
* **GET /discs/rescan** - get the discs database rescan status
* **POST /discs/rescan** - rescan the stored discs on the filesystem to update the database
* **GET /discs/backup** - get the discs database backup status
* **POST /discs/backup** - backup the database with stored discs
* **GET /discs/{discId}** - get the information of the specified disc
* **POST /discs/{discId}** - edit the information of the specified disc
* **GET /discs/{discId}/front** - get the front cover of the specified disc
* **POST /discs/{discId}/front** - update the front cover of the specified disc
* **GET /discs/{discId}/back** - get the back cover of the specified disc
* **POST /discs/{discId}/back** - update the back cover of the specified disc
* **GET /discs/{discId}/audio** - get the audio data of the specified disc (and track)

### get list of stored discs
Get a (filtered) list of stored discs.

#### end point: `http://<host>:<port>/v1/discs`
#### request method: **GET**
#### query parameters: 
* limit - The number of items to return at one time (max. 100)
* offset - Where to start with returning items (default 0)
* search - Search term for filtering discs
* tag - The tag to filter the discs by (disc, track, artist, genre, year)
* format - The format of the stored disc information list; disc=disc information, track=disc and track information (default disc information only)

#### example:
**request**

``
wget --method=GET http://localhost:8001/v1/discs?limit=100&offset=0&format=disc
``

**response**

Success
```
HTTP/1.1 200 OK
Server: cdextract-server
Content-Length: 43782
Content-Type: application/json

	
Response body

[
  {
    "id": 1,
    "disc_id": "92093e0a",
    "length": 177493,
      ..
  },
  {
    "id": 2,
    "disc_id": "c6d3a5cb",
    "length": 187254,
      ..
  },
  ..
]
```

Failure
```
HTTP/1.1 404 Not Found
Server: cdextract-server
Content-Length: 51
Content-Type: application/json

{
  "code": 404
  "message": "Resource not found"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### get disc metadata
Get the metadata for the stored discs including the total number of discs, tracks and artists.

#### end point: `http://<host>:<port>/v1/discs/meta`
#### request method: **GET**

#### example:
**request**

``
wget --method=GET http://localhost:8001/v1/discs/meta
``

**response**

Success
```
HTTP/1.1 200 OK
Server: cdextract-server
Content-Length: 46
Content-Type: application/json

{
  "discs": 207, 
  "tracks": 2417, 
  "artists": 108
}
```

Failure
```
HTTP/1.1 500 Internal Server Error
Server: cdextract-server
Content-Length: 54
Content-Type: application/json

{
  "code": 500
  "message": "Internal Server Error"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### get rescan status
Get the discs database rescan status.

#### end point: `http://<host>:<port>/v1/discs/rescan`
#### request method: **GET**

#### example:
**request**

``
wget --method=GET http://localhost:8001/v1/discs/rescan
``

**response**

Success
```
HTTP/1.1 200 OK
Server: cdextract-server
Content-Length: 65
Content-Type: application/json

{
    "status": "pending",
    "message": "rescan in progress",
}
```

Failure
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### rescan and update database
Rescan the stored discs on the filesystem to update the database.

#### end point: `http://<host>:<port>/v1/discs/rescan`
#### request method: **POST**

#### example:
**request**

``
wget --method=POST http://localhost:8001/v1/discs/rescan
``

**response**

Success
```
HTTP/1.1 204 No Content
Server: cdextract-server
Content-Length: 0

```

Failure
```
HTTP/1.1 403 Forbidden
Server: cdextract-server
Content-Length: 42
Content-Type: application/json

{
  "code": 403
  "message": "Forbidden"
}
```
```
HTTP/1.1 423 Locked
Server: cdextract-server
Content-Length: 59
Content-Type: application/json

{
  "code": 423
  "message": "Locked. Resource is locked"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### get rebuild status
Get the discs database rebuild status.

#### end point: `http://<host>:<port>/v1/discs/rebuild`
#### request method: **GET**

#### example:
**request**

``
wget --method=GET http://localhost:8001/v1/discs/rebuild
``

**response**

Success
```
HTTP/1.1 200 OK
Server: cdextract-server
Content-Length: 65
Content-Type: application/json

{
    "status": "pending",
    "message": "rebuild in progress",
}
```

Failure
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### rescan and update database
Rescan the stored discs on the filesystem to update the database.

#### end point: `http://<host>:<port>/v1/discs/rebuild`
#### request method: **POST**

#### example:
**request**

``
wget --method=POST http://localhost:8001/v1/discs/rebuild
``

**response**

Success
```
HTTP/1.1 204 No Content
Server: cdextract-server
Content-Length: 0

```

Failure
```
HTTP/1.1 403 Forbidden
Server: cdextract-server
Content-Length: 42
Content-Type: application/json

{
  "code": 403
  "message": "Forbidden"
}
```
```
HTTP/1.1 423 Locked
Server: cdextract-server
Content-Length: 59
Content-Type: application/json

{
  "code": 423
  "message": "Locked. Resource is locked"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### get backup status
Get the discs database backup status.

#### end point: `http://<host>:<port>/v1/discs/backup`
#### request method: **GET**

#### example:
**request**

``
wget --method=GET http://localhost:8001/v1/discs/backup
``

**response**

Success
```
HTTP/1.1 200 OK
Server: cdextract-server
Content-Length: 65
Content-Type: application/json

{
    "status": "pending",
    "message": "backup in progress",
}
```

Failure
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### backup database
Backup the database with stored discs.

#### end point: `http://<host>:<port>/v1/discs/backup`
#### request method: **POST**

#### example:
**request**

``
wget --method=POST http://localhost:8001/v1/discs/backup
``

**response**

Success
```
HTTP/1.1 204 No Content
Server: cdextract-server
Content-Length: 0

```

Failure
```
HTTP/1.1 403 Forbidden
Server: cdextract-server
Content-Length: 42
Content-Type: application/json

{
  "code": 403
  "message": "Forbidden"
}
```
```
HTTP/1.1 423 Locked
Server: cdextract-server
Content-Length: 59
Content-Type: application/json

{
  "code": 423
  "message": "Locked. Resource is locked"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### get disc information
Get the information of the specified disc.

#### end point: `http://<host>:<port>/v1/discs/{discId}`
#### request method: **GET**

#### example:
**request**

``
wget --method=GET http://localhost:8001/v1/discs/92093e0a
``

**response**

Success
```
HTTP/1.1 200 OK
Server: cdextract-server
Content-Length: 4050
Content-Type: application/json

{
  "id": 1,
  "disc_id": "92093e0a",
  "length": 177493,
  "lookup": "0ab5552c1aad7fa1",
  "artist": "James Blunt",
  "title": "Back To Bedlam",
  "genre": "singer songwriter",
  "year": 2005,
  "extended": "",
  "cddb_query": "92093e0a+10+175+18469+34444+51154+70524+88841+104824+124686+140966+159454+2368",
  "cddb_category": "data",
  "cddb_entry_id": "96093e87",
  "cddb_disc_id": "96093e0a",
  "cddb_revision": 0,
  "cddb_complete": 1,
  "mb_query": "1+10+175+18469+34444+51154+70524+88841+104824+124686+140966+159454+177667",
  "mb_fuzzy_lookup": "1+10+177667+175+18469+34444+51154+70524+88841+104824+124686+140966+159454",
  "mb_disc_id": "BM0fleBGaH5TzPGp1jBh4s.VwpU-",
  "mb_release_id": "54dca467-331b-4df9-9ff0-97a9d970cfd2",
  "mb_front_cover_size": 162064,
  "mb_back_cover_size": 219025,
  "mb_complete": 1,
  "extracted": 0,
  "track_count": 10,
  "tracks": [
    {
      "num": 1,
      "length": 18294,
      "title": "High",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/01.High.flac",
      "skipped": 0
    },
    {
      "num": 2,
      "length": 15975,
      "title": "You're Beautiful",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/02.You're Beautiful.flac",
      "skipped": 0
    },
    {
      "num": 3,
      "length": 16710,
      "title": "Wisemen",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer/songwriter",
      "year": 2005,
      "extended": "",
      "filename": "/James Blunt/Back To Bedlam (2005)/03.Wisemen.flac",
      "skipped": 0
    },
    {
      "num": 4,
      "length": 19370,
      "title": "Goodbye My Lover",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/04.Goodbye My Lover.flac",
      "skipped": 0
    },
    {
      "num": 5,
      "length": 18317,
      "title": "Tears And Rain",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer/songwriter",
      "year": 2005,
      "extended": "",
      "filename": "/James Blunt/Back To Bedlam (2005)/05.Tears And Rain.flac",
      "skipped": 0
    },
    {
      "num": 6,
      "length": 15983,
      "title": "Out Of My Mind",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/06.Out Of My Mind.flac",
      "skipped": 0
    },
    {
      "num": 7,
      "length": 19862,
      "title": "So Long, Jimmy",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/07.So Long, Jimmy.flac",
      "skipped": 0
    },
    {
      "num": 8,
      "length": 16280,
      "title": "Billy",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/08.Billy.flac",
      "skipped": 0
    },
    {
      "num": 9,
      "length": 18488,
      "title": "Cry",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer/songwriter",
      "year": 2005,
      "extended": "",
      "filename": "/James Blunt/Back To Bedlam (2005)/09.Cry.flac",
      "skipped": 0
    },
    {
      "num": 10,
      "length": 18214,
      "title": "No Bravery",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/10.No Bravery.flac",
      "skipped": 0
    }
  ]
}
```

Failure
```
HTTP/1.1 404 Not Found
Server: cdextract-server
Content-Length: 51
Content-Type: application/json

{
  "code": 404
  "message": "Resource not found"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### edit disc information
Edit the information of the specified disc.

#### end point: `http://<host>:<port>/v1/discs/{discId}`
#### request method: **POST**

#### example:
**request**

``
wget --method=POST --body-file=disc_body.json http://localhost:8001/v1/discs/92093e0a
``
```
{
  "id": 1,
  "disc_id": "92093e0a",
  "length": 177493,
  "lookup": "0ab5552c1aad7fa1",
  "artist": "James Blunt",
  "title": "Back To Bedlam",
  "genre": "Singer Songwriter",
  "year": 2005,
  "extended": "",
  "tracks": [
    {
      "num": 9,
      "length": 246,
      "title": "Cry",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer/Songwriter",
      "year": "2005",
      "extended": "",
      "filename": "/James Blunt/Back To Bedlam (2005)/09.Cry.flac"
    },
    {
      "num": 3,
      "length": 222,
      "title": "Wisemen",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer/Songwriter",
      "year": "2005",
      "extended": "",
      "filename": "/James Blunt/Back To Bedlam (2005)/03.Wisemen.flac"
    },
    {
      "num": 5,
      "length": 244,
      "title": "Tears And Rain",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "Singer/Songwriter",
      "year": "2005",
      "extended": "",
      "filename": "/James Blunt/Back To Bedlam (2005)/05.Tears And Rain.flac"
    }
  ]
}
```

**response**

Success
```
HTTP/1.1 200 OK
Server: cdextract-server
Content-Length: 4050
Content-Type: application/json

{
  "id": 1,
  "disc_id": "92093e0a",
  "length": 177493,
  "lookup": "0ab5552c1aad7fa1",
  "artist": "James Blunt",
  "title": "Back To Bedlam",
  "genre": "singer songwriter",
  "year": 2005,
  "extended": "",
  "cddb_query": "92093e0a+10+175+18469+34444+51154+70524+88841+104824+124686+140966+159454+2368",
  "cddb_category": "data",
  "cddb_entry_id": "96093e87",
  "cddb_disc_id": "96093e0a",
  "cddb_revision": 0,
  "cddb_complete": 1,
  "mb_query": "1+10+175+18469+34444+51154+70524+88841+104824+124686+140966+159454+177667",
  "mb_fuzzy_lookup": "1+10+177667+175+18469+34444+51154+70524+88841+104824+124686+140966+159454",
  "mb_disc_id": "BM0fleBGaH5TzPGp1jBh4s.VwpU-",
  "mb_release_id": "54dca467-331b-4df9-9ff0-97a9d970cfd2",
  "mb_front_cover_size": 162064,
  "mb_back_cover_size": 219025,
  "mb_complete": 1,
  "extracted": 0,
  "track_count": 10,
  "tracks": [
    {
      "num": 1,
      "length": 18294,
      "title": "High",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/01.High.flac",
      "skipped": 0
    },
    {
      "num": 2,
      "length": 15975,
      "title": "You're Beautiful",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/02.You're Beautiful.flac",
      "skipped": 0
    },
    {
      "num": 3,
      "length": 16710,
      "title": "Wisemen",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer/songwriter",
      "year": 2005,
      "extended": "",
      "filename": "/James Blunt/Back To Bedlam (2005)/03.Wisemen.flac",
      "skipped": 0
    },
    {
      "num": 4,
      "length": 19370,
      "title": "Goodbye My Lover",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/04.Goodbye My Lover.flac",
      "skipped": 0
    },
    {
      "num": 5,
      "length": 18317,
      "title": "Tears And Rain",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer/songwriter",
      "year": 2005,
      "extended": "",
      "filename": "/James Blunt/Back To Bedlam (2005)/05.Tears And Rain.flac",
      "skipped": 0
    },
    {
      "num": 6,
      "length": 15983,
      "title": "Out Of My Mind",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/06.Out Of My Mind.flac",
      "skipped": 0
    },
    {
      "num": 7,
      "length": 19862,
      "title": "So Long, Jimmy",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/07.So Long, Jimmy.flac",
      "skipped": 0
    },
    {
      "num": 8,
      "length": 16280,
      "title": "Billy",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/08.Billy.flac",
      "skipped": 0
    },
    {
      "num": 9,
      "length": 18488,
      "title": "Cry",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer/songwriter",
      "year": 2005,
      "extended": "",
      "filename": "/James Blunt/Back To Bedlam (2005)/09.Cry.flac",
      "skipped": 0
    },
    {
      "num": 10,
      "length": 18214,
      "title": "No Bravery",
      "artist": "James Blunt",
      "album": "Back To Bedlam",
      "genre": "singer songwriter",
      "year": 2005,
      "extended": "",
      "filename": "James Blunt/Back To Bedlam (2005)/10.No Bravery.flac",
      "skipped": 0
    }
  ]
}
```

Failure
```
HTTP/1.1 403 Forbidden
Server: cdextract-server
Content-Length: 42
Content-Type: application/json

{
  "code": 403
  "message": "Forbidden"
}
```
```
HTTP/1.1 404 Not Found
Server: cdextract-server
Content-Length: 51
Content-Type: application/json

{
  "code": 404
  "message": "Resource not found"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### get front cover
Get the front cover of the specified disc.

#### end point: `http://<host>:<port>/v1/discs/{discId}/front`
#### request method: **GET**

#### example:
**request**

``
wget --method=GET http://localhost:8001/v1/discs/01dfab06/front
``

**response**

Success
```
<binary data>

```

Failure
```
HTTP/1.1 404 Not Found
Server: cdextract-server
Content-Length: 51
Content-Type: application/json

{
  "code": 404
  "message": "Resource not found"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### update front cover
Update the front cover of the specified disc.

#### end point: `http://<host>:<port>/v1/discs/{discId}/front`
#### request method: **POST**

#### example:
**request**

``
wget --method=POST --body-file=cover.jpg http://localhost:8001/v1/discs/01dfab06/front
``

**response**

Success
```
HTTP/1.1 204 No Content
Server: cdextract-server
Content-Length: 0

```

Failure
```
HTTP/1.1 423 Locked
Server: cdextract-server
Content-Length: 59
Content-Type: application/json

{
  "code": 423
  "message": "Locked. Resource is locked"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### get back cover
Get the back cover of the specified disc.

#### end point: `http://<host>:<port>/v1/discs/{discId}/back`
#### request method: **GET**

#### example:
**request**

``
wget --method=GET http://localhost:8001/v1/discs/01dfab06/back
``

**response**

Success
```
<binary data>

```

Failure
```
HTTP/1.1 404 Not Found
Server: cdextract-server
Content-Length: 51
Content-Type: application/json

{
  "code": 404
  "message": "Resource not found"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### update back cover
Update the back cover of the specified disc.

#### end point: `http://<host>:<port>/v1/discs/{discId}/back`
#### request method: **POST**

#### example:
**request**

``
wget --method=POST --body-file=back-cover.jpg http://localhost:8001/v1/discs/01dfab06/back
``

**response**

Success
```
HTTP/1.1 204 No Content
Server: cdextract-server
Content-Length: 0

```

Failure
```
HTTP/1.1 423 Locked
Server: cdextract-server
Content-Length: 59
Content-Type: application/json

{
  "code": 423
  "message": "Locked. Resource is locked"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```

### get audio data
Get the audio data of the specified disc (and track).

#### end point: `http://<host>:<port>/v1/discs/{discId}/audio`
#### request method: **GET**

#### example:
**request**

``
wget --method=GET http://localhost:8001/v1/discs/01dfab06/audio?track=1&format=flac
``

**response**

Success
```
<binary data>

```

Failure
```
HTTP/1.1 404 Not Found
Server: cdextract-server
Content-Length: 51
Content-Type: application/json

{
  "code": 404
  "message": "Resource not found"
}
```
```
HTTP/1.1 503 Service Unavailable
Server: cdextract-server
Content-Length: 100
Content-Type: application/json

{
  "code": 503
  "message": "Service Unavailable. The server is not ready to handle the request."
}
```


## HTTP status codes

### Successful responses

#### 200 OK
The request succeeded. 
The result and meaning of "success" depends on the HTTP method:

**GET**: The resource has been fetched and transmitted in the message body.

**HEAD**: Representation headers are included in the response without any message body.

**POST**: The resource describing the result of the action is transmitted in the message body.

#### 201 Created
The request succeeded, and a new resource was created as a result.

#### 202 Accepted
The request has been received but not yet acted upon. 

#### 204 No Content
The request has been processed, but there is no content to return.

### Client error responses

#### 400 Bad Request
The server cannot or will not process the request due to something that is perceived to be a client error

#### 404 Not Found
The server cannot find the requested resource.

#### 405 Method Not Allowed
The request method is known by the server but is not supported by the target resource.

### Server error responses

#### 500 Internal Server Error
The server has encountered a situation it does not know how to handle.

#### 503 Service Unavailable
The server is not ready to handle the request.


## HTTP request methods

### GET
The GET method requests a representation of the specified resource. 
Requests using GET only retrieve data.

### HEAD
The HEAD method asks for a response identical to a GET request, but without a response body.
This method is not supported by the API and the server will respond with a [405 Method Not Allowed](#405-method-not-allowed) response.

### POST
The POST method submits an entity to the specified resource, often causing a change in state or side effects on the server.

### PUT
The PUT method replaces all current representations of the target resource with the request content.

### DELETE
The DELETE method deletes the specified resource.

### CONNECT
The CONNECT method establishes a tunnel to the server identified by the target resource.
This method is not supported by the API and the server will respond with a [405 Method Not Allowed](#405-method-not-allowed) response.

### OPTIONS
The OPTIONS method describes the communication options for the target resource.
This method is not supported by the API and the server will respond with a [405 Method Not Allowed](#405-method-not-allowed) response.

### TRACE
The TRACE method performs a message loop-back test along the path to the target resource.
This method is not supported by the API and the server will respond with a [405 Method Not Allowed](#405-method-not-allowed) response.

### PATCH
The PATCH method applies partial modifications to a resource.
This method is not supported by the API and the server will respond with a [405 Method Not Allowed](#405-method-not-allowed) response.


#### Safe, idempotent, and cacheable request methods
The following table lists HTTP request methods and their categorization in terms of safety, cacheability, and idempotency.

| Method  | Safe | Idempotent | Cacheable       |
| ------- | ---- | ---------- | --------------- |
| GET     | Yes  | Yes        | Yes             |
| HEAD    | Yes  | Yes        | Yes             |
| OPTIONS | Yes  | Yes        | No              |
| TRACE   | Yes  | Yes        | No              |
| PUT     | No   | Yes        | No              |
| DELETE  | No   | Yes        | No              |
| POST    | No   | No	        | Conditional[^1] |
| PATCH   | No   | No         | Conditional[^1] |
| CONNECT | No   | No         | No              |

[^1]: *POST and PATCH are cacheable when responses explicitly include freshness information and a matching Content-Location header.*
