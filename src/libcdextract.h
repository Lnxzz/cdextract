/********************************************************************

  libcdextract - cd audio data extraction library 

  Copyright (C) 2021-2026 E. Heerschop (github@heerschop.frl)

  This library uses:
  * the cdda-interface and cdda-paranoia libraries for the audio
    extraction which are part of the cdparanoia III software distribution.
    both libraries are licensed under GNU LGPL v2.1 (or any later version).
    Copyright (C) 2008 Monty monty@xiph.org
  * the CURL library for downloading disc information
    libCURL - Copyright (c) 1996 - 2024, Daniel Stenberg, <daniel@haxx.se>,
    and many contributors
  * libFLAC for encoding the audio data in the lossless FLAC format
    licensed under Xiph.Org's BSD-like license
    Copyright (C) 2000-2009  Josh Coalson
    Copyright (C) 2011-2016  Xiph.Org Foundation
  * libcoverart for downloading cover images from the musicbrainz service
    This library is licensed under GNU LGPL v2.1 (or any later version).
    Copyright (C) 2012 Andrew Hawkins


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

#ifndef LIBCDEXTRACT_H
#define LIBCDEXTRACT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "libcdextract_types.h"


#define CDE_OPTION_VERBOSE 1
#define CDE_OPTION_OUTPUT_TYPE 2
#define CDE_OPTION_COVERART 3
#define CDE_OPTION_SEARCH_DRIVE 4
#define CDE_OPTION_ABORT_ON_SKIP 5
#define CDE_OPTION_CD_SPEED 6
#define CDE_OPTION_MAX_RETRIES 7
#define CDE_OPTION_EJECT_WHEN_DONE 8
#define CDE_OPTION_WRITE_JSON 9
#define CDE_OPTION_WRITE_CUE_SHEET 10
#define CDE_OPTION_WRITE_CDDB 11
#define CDE_OPTION_SHOW_DISC_INFO 12
#define CDE_OPTION_VIRTUAL_DRIVE 13

#define CDE_VERBOSE_OFF 0               // no verbose messaging, only show errors
#define CDE_VERBOSE_ON 1                // use verbose messaging
#define CDE_OUTPUT_TYPE_WAV 0           // write audio as a wav file
#define CDE_OUTPUT_TYPE_FLAC 1          // write audio as a flac file
#define CDE_COVERART_OFF 0              // do not download disc coverart
#define CDE_COVERART_MEM_ONLY 1         // only get front and back cover and keep the result in memory
#define CDE_COVERART_COVER_ONLY 2       // only download the front and back cover
#define CDE_COVERART_FULL 3             // full download of covers and metadata
#define CDE_SEARCH_DRIVE_OFF 0          // do not search drive
#define CDE_SEARCH_DRIVE_ON 1           // search drive
#define CDE_ABORT_ON_SKIP_OFF 0         // do not abort on skip
#define CDE_ABORT_ON_SKIP_ON 1          // abort on skip
#define CDE_EJECT_WHEN_DONE_OFF 0       // do not eject/open tray when extraction done
#define CDE_EJECT_WHEN_DONE_ON 1        // eject/open tray when extraction done
#define CDE_WRITE_JSON_OFF 0            // do not write json disc information to disk
#define CDE_WRITE_JSON_ON 1             // write json disc information to disk
#define CDE_WRITE_CUE_SHEET_OFF 0       // do not write cue sheet to disk
#define CDE_WRITE_CUE_SHEET_ON 1        // write cue sheet to disk
#define CDE_WRITE_CDDB_OFF 0            // do not write cddb information in xmcd format to disk
#define CDE_WRITE_CDDB_ON 1             // write cddb information in xmcd format to disk
#define CDE_SHOW_DISC_INFO_OFF 0        // do not show gathered disc information
#define CDE_SHOW_DISC_INFO_ON 1         // show gathered disc information
#define CDE_VIRTUAL_DRIVE_OFF 0         // cdrom drive is a physical drive
#define CDE_VIRTUAL_DRIVE_ON 1          // cdrom drive is a virtual drive
#define CDE_BACKUP_OFF 0                // do not backup database at startup
#define CDE_BACKUP_ON 1                 // backup database at startup

#define CDE_STATUS_UNINITIALIZED 0      // uninitialized state
#define CDE_STATUS_INITIALIZED 1        // initialized state
#define CDE_STATUS_IDLE 2               // idle state
#define CDE_STATUS_PREPARING 3          // preparing for audio extraction
#define CDE_STATUS_EXTRACTING 4         // currently extracting audio
#define CDE_STATUS_CANCEL 5             // request for cancellation

#define EXTRACT_CB_READ 0               // callback identifier for reading data
#define EXTRACT_CB_VERIFY 1             // callback identifier for verifying data
#define EXTRACT_CB_END_OF_FILE -1       // callback identifier for end of file
#define EXTRACT_CB_WRITE_FILE -2        // callback identifier for writing data
#define EXTRACT_CB_END_OF_DISC -3       // callback identifier for end of disc
#define EXTRACT_CB_FIXUP_EDGE     2
#define EXTRACT_CB_FIXUP_ATOM     3
#define EXTRACT_CB_SCRATCH        4
#define EXTRACT_CB_REPAIR         5
#define EXTRACT_CB_SKIP           6
#define EXTRACT_CB_DRIFT          7
#define EXTRACT_CB_BACKOFF        8
#define EXTRACT_CB_OVERLAP        9
#define EXTRACT_CB_FIXUP_DROPPED 10
#define EXTRACT_CB_FIXUP_DUPED   11
#define EXTRACT_CB_READERR       12
#define EXTRACT_CB_CACHEERR      13

#define DEFAULT_MAX_RETRIES 20          // default maximum number of retries
#define DEFAULT_CD_SPEED 0              // default cd read speed (set to max)


/**
 * @brief show cd extract, cdda and paranoia library versions
 */
extern void cde_version();

/**
 * @brief intialize the cdextraction library
 */
extern void cde_initialize(cde_state *cde, char *cde_device_name, char *cde_root_folder, char *cde_cddb_folder, void(*rpt_callback)(int, char*), void(*progress_callback)(int, int, int, long, float));

/**
 * @brief intialize the cdextraction library with the given option
 */
extern void cde_set_option(cde_state *cde, int option, int varg);

/**
 * @brief cleanup and reset state
 */
extern void cde_cleanup(cde_state *cde);

/**
 * @brief set and (optionally) create the output path
 *        the output path uses the following structure:
 *        {ROOT_FOLDER}/{ARTIST}/{ALBUM_TITLE}
 * @param cde cdextract state
 * @param create_path indicator to create output path (1) or only set the folder in cde (0)
 * @return 0 on success, non-zero on failure
 */
extern int cde_set_create_output_path(cde_state *cde, int create_output_path);

/**
 * @brief set the filename for a track
 * @param disc_info pointer to the disc information
 * @param num track number
 * @param file_suffix suffix to append to the filename
 * @return 0 on success, non-zero on failure
 */
extern int cde_set_track_filename(disc *disc_info, int num, const char *file_suffix);

/**
 * @brief dynamically allocate a disc information structure
 */
extern disc *cde_alloc_disc(int nr_of_tracks);

/**
 * @brief free a dynamically allocated disc information structure
 * @param ppdisc pointer to the pointer of the disc structure to be freed
 * @param tracks_allocated number of tracks allocated in the disc structure; -1 = use d_tracks from the disc structure
 */
extern void cde_free_disc(disc **ppdisc, int tracks_allocated);

/**
 * @brief open the cdrom drive
 */
extern int cde_open_drive(cde_state *cde);

/**
 * @brief close the cdrom drive
 */
extern int cde_close_drive(cde_state *cde);

/**
 * @brief calculate and set the 64-bit internal hash value of the disc information to enable fast lookups
 */
extern void cde_set_hash(disc *disc_info);

/**
 * @brief prepare disc_info structure by reading the toc and 
 *        calculating the cddb and musicbrainz disc id hashes
 */
extern int cde_prepare_disc_info(cde_state *cde);

/**
 * @brief displays the table of contents (toc)
 */
extern void cde_display_toc(cdrom_drive *drv);

/**
 * @brief displays the gathered disc information
 */
extern void cde_display_disc_info(disc *disc_info);

/**
 * @brief download cddb/musicbrainz disc information and covers
 *        pre: cde_initialize has been called to set the right settings
 */
extern int cde_download_disc_info(cde_state *cde, int fuzzy_lookup, int overwrite, int cleanup);

/**
 * @brief writes the gathered disc information to a file
 */
extern int cde_write_disc_info(cde_state *cde, int overwrite);

/**
 * @brief extract audio cd - actual cd audio extraction
 *        pre: cde_initialize has been called to set the right settings
 */
extern void *cde_extract_audio_t(void *state);

/**
 * @brief extract audio cd - 'virtual' cd audio extraction
 *        pre: cde_initialize has been called to set the right settings
 */
extern void *cde_extract_audio_v(void *state);

/**
 * @brief extract audio cd
 *        uses a separate thread for extraction
 *        pre: cde_initialize has been called to set the right settings
 * @returns 0 if successful
 */
extern int cde_extract_audio(cde_state *cde);

/**
 * @brief cancel audio cd extraction
 * @return returns 0 if successful
 */
extern int cde_cancel_extract(cde_state *cde, int wait);

/**
 * @brief close drive tray
 * @returns 0 if successful
 */
extern int cde_close_tray(cde_state *cde);

/**
 * @brief eject disc from the drive
 * @returns 0 if successful
 */
extern int cde_eject(cde_state *cde);

/**
 * @brief writes a cue sheet from the gathered disc information to a file
 *        pre: cde_state contains a prepared disc information structure
 */
extern int cde_write_cue_sheet(cde_state *cde, int overwrite);

/**
 * @brief parses a cue sheet and stores the results in the disc information structure
 *        pre: cde_state state initialized and we must be using a 'virtual' cdrom drive
 */
extern int cde_parse_cue_sheet(cde_state *cde, const char *cue, bool download_disc_info);

/**
 * @brief writes a cddb entry from the gathered disc information to a file in xmcd format
 *        pre: cde_state contains a prepared disc information structure
 */
extern int cde_cddb_write_entry(cde_state *cde, int overwrite);

#ifdef __cplusplus
}
#endif

#endif
