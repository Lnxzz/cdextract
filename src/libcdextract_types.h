/**************************************************************************

  libcdextract - disc information structures

  Copyright (C) 2021-2026 E. Heerschop (github@heerschop.frl)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published 
  by the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program. If not, see <https://www.gnu.org/licenses/>.

 **************************************************************************/

#ifndef LIBCDEXTRACT_TYPES_H
#define LIBCDEXTRACT_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <pthread.h>
#include "cdda_interface.h"


#define CDE_OK 0                              // operation successful
#define CDE_ERROR 1                           // generic error
#define CDE_ERROR_NOT_IDLE -1                 // operation can only be done when in idle mode
#define CDE_ERROR_NO_DISC -4                  // no disc present in drive
#define CDE_ERROR_OUTPUT_PATH -93             // unable to set or create output path
#define CDE_ERROR_CDDB_ENCODING -94           // encoding error in CDDB data (expect utf-8 or iso-8859-1)
#define CDE_ERROR_CDDB_DATA -95               // no or invalid cddb data
#define CDE_ERROR_MB_DATA -96                 // no or invalid musicbrainz data
#define CDE_ERROR_CUE_SHEET -97               // no or invalid cue sheet
#define CDE_ERROR_NO_VIRTUAL_DRIVE -98        // drive is not a virtual drive
#define CDE_ERROR_DRIVE_NOT_OPEN -99          // no drive found to read audio cd from
#define CDE_ERROR_NO_DRIVE -100               // no drive found to read audio cd from

#define CDE_MAX_TRACKS 100                    // maximum number of tracks to extract
#define CDE_CD_MSF_OFFSET 150                 // MSF offset of first frame
#define CDE_CD_FRAMES 75                      // frames per second
#define CDE_VIRTUAL_DRIVE "<virtual>"         // virtual cdrom drive name
#define CDE_COVER_FRONT "cover.jpg"           // front cover filename
#define CDE_COVER_BACK "cover-back.jpg"       // back cover filename
#define CDE_COVER_INFO "cover-info.json"      // coverartarchive release information
#define CDE_UNKNOWN_ARTIST "Unknown Artist"   // unknown artist
#define CDE_UNKNOWN_ALBUM "Unknown Album"     // unknown album
#define CDE_UNKNOWN_GENRE "rock"              // unknown/default genre
#define CDE_MIN_YEAR 1560                     // minimum album year
#define CDE_MAX_YEAR 2100                     // maximum album year


typedef struct track {
  int t_num;               // track number
  int t_length;            // track length in frames (divide by 75 to get the track length in seconds)
  char *t_title;           // track title
  char *t_artist;          // track artist
  char *t_album;           // album the track belongs to
  char *t_genre;           // genre
  int t_year;              // year
  char *t_extended;        // extended track information
  char *t_filename;        // track output filename
  int  t_skipped;          // indicator stating the audio extraction for this track has been skipped
} track;

typedef struct disc {
  uint64_t db_id;          // 64-bit bit database id for the disc
  unsigned int d_id;       // calculated cddb disc id from the track information read from the disc. Example: f50a3b13
  int d_length;            // total length of the disc in frames (divide by 75 to get the overall length in seconds)
  uint64_t d_lookup;       // 64-bit internal hash value of the disc information to enable fast lookups
  char *d_artist;          // artist. Example: Various
  char *d_title;           // title. Example: Various Live
  char *d_genre;           // genre. Example: jazz
  int d_year;              // year. Example: 1994
  char *d_extended;        // extended data
  char *cddb_query;        // cddb query string. Example: f50a3b13+10+175+18469+34444+51154+70524+88841+104824+124686+140966+159454
  char *cddb_category;     // cddb category. Example: rock
  unsigned int cddb_e_id;  // cddb entry id as returned by the query service and used to read a specific entry. Example: f50a3b14
  unsigned int cddb_d_id;  // cddb disc id as returned by the cddb service. Example: f50a3b15
  int cddb_revision;       // cddb revision number. Example: 1
  int cddb_complete;       // indicator stating cddb information gathering is complete
  char *mb_query;          // musicbrainz query string. Example: 10+175+18469+34444+51154+70524+88841+104824+124686+140966+159454+177668
  char *mb_fuzzy_lookup;   // musicbrainz fuzzy TOC search. Example: 1+10+177668+175+18469+34444+51154+70524+88841+104824+124686+140966+159454
  char *mb_disc_id;        // musicbrainz disc id. Example: BM0fleBGaH5TzPGp1jBh4s.VwpU-
  char *mb_release_id;     // musicbrainz release id. Example: a0691875-152e-45a6-a30d-ab2b57c0648e
  char *mb_front_cover;    // disc front cover image data
  int mb_front_cover_size; // disc front cover image size
  char *mb_back_cover;     // disc back cover image data
  int mb_back_cover_size;  // disc back cover image size
  int mb_complete;         // indicator stating musicbrainz data gathering is complete
  int d_extracted;         // indicator stating the audio has been extracted from this disc
  int d_tracks;            // number of tracks on the disc
  track *tracks;           // pointer to the list of track details
} disc;

typedef struct cde_state {
  char *cdrom_device;      // cdrom device name. Example: /dev/cdrom
  char *audio_folder;      // base folder to store audio data
  char *cddb_folder;       // folder to read cddb data from
  char *web_folder;        // folder to read web data from (default covers, index.html, etc.)
  char *folder;            // folder to store the current disc
  cdrom_drive *drv;        // cdda cdrom device structure
  disc *disc_info;         // disc information structure
  pthread_t thread;        // thread used by the audio extraction process
  int status;              // audio extraction status (CDE_STATUS_*)
  /* cdextract options: */
  int verbose;             // use verbose output
  int output_type;         // audio output type (0=wav; 1=flac)
  int download_coverart;   // download cd cover art
  int search_drive;        // search for cdrom device
  int cd_speed;            // cdrom device read speed
  int max_retries;         // max. number of retries to read audio data from disc
  int abort_on_skip;       // abort audio extraction when data could not be read (0=off; 1=on)
  int eject_when_done;     // eject disc when audio extraction process is finished
  int write_json;          // write disc information as json structure to disc (1=write)
  int write_cue_sheet;     // write cue sheet to disc (1=write)
  int write_cddb;          // write cddb information in xmcd format to disc (1=write)
  int show_disc_info;      // show disc information structure
  int virtual_drive;       // use 'virtual' drive instead of a physical device (1=virtual)
} cde_state;


#ifdef __cplusplus
}
#endif

#endif /* LIBCDEXTRACT_TYPES_H */
