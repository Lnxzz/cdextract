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

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/limits.h>
#include <linux/cdrom.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#include <scsi/scsi_ioctl.h>
#include <unistd.h>
#include <pthread.h>

#include "config.h"
#include "sha1.h"
#include "hash.h"
#include "base64.h"
#include "file_utils.h"
#include "string_utils.h"
#include "wav_writer.h"
#include "wav_reader.h"
#include "flac_writer.h"
#include "flac_reader.h"
#include "cddb.h"
#include "mb_api.h"
#include "cdda_interface.h"
#include "cdda_paranoia.h"
#include "json_file.h"
#include "cue_sheet.h"
#include "report.h"
#include "libcdextract_types.h"
#include "libcdextract.h"
 

// internal variables
static pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
long track_firstsector = 0;
long track_lastsector = 0;
int current_track = 0;


/**
 * @brief handles the callback used by the cdda_paranoia library
 *        calls the configured  external callback (if available)
 */
static void cde_progress_callback(long inpos, int function) {
  int rpt_type = CDE_MSG_TYPE_INFO;
  
  long sector = inpos / CD_FRAMEWORDS;
  float percentage = ((float)(sector - track_firstsector) / (track_lastsector - track_firstsector)) * 100;
  if (percentage > 100) {
    percentage = 100;
  }

  // set report type
  if (function <= EXTRACT_CB_VERIFY) {
    rpt_type = CDE_MSG_TYPE_INFO;
  } else if (function == PARANOIA_CB_FIXUP_EDGE || function == PARANOIA_CB_FIXUP_ATOM || function == PARANOIA_CB_DRIFT) {
    rpt_type = CDE_MSG_TYPE_WARNING;
  } else if (function == PARANOIA_CB_SCRATCH || function == PARANOIA_CB_REPAIR || function == PARANOIA_CB_SKIP || 
      function == PARANOIA_CB_FIXUP_DROPPED || function == PARANOIA_CB_FIXUP_DUPED || function == PARANOIA_CB_READERR || 
      function == PARANOIA_CB_CACHEERR) {
    rpt_type = CDE_MSG_TYPE_ERROR;
  }

  // call external progress callback if available
  if (external_progress_callback_ptr != NULL) {
    (*external_progress_callback_ptr)(rpt_type, function, current_track, sector, percentage);
    return;
  }

  char function_str[16];
  switch (function)
  {
  // normal events
  case EXTRACT_CB_READ:
    strcpy(function_str, "read");
    break;
  case EXTRACT_CB_VERIFY:
    strcpy(function_str, "verify");
    break;
  case EXTRACT_CB_WRITE_FILE:
    strcpy(function_str, "write");
    break;
  case EXTRACT_CB_END_OF_FILE:
    strcpy(function_str, "end");
    break;
  case EXTRACT_CB_END_OF_DISC:
    strcpy(function_str, "done");
    break; 
  // paranoia specific warnings and errors
  case EXTRACT_CB_FIXUP_EDGE:
    strcpy(function_str, "fixup edge");
    break;
  case EXTRACT_CB_FIXUP_ATOM:
    strcpy(function_str, "fixup atom");
    break;
  case EXTRACT_CB_SCRATCH:
    strcpy(function_str, "scratch");
    break;
  case EXTRACT_CB_REPAIR:
    strcpy(function_str, "repair");
    break;
  case EXTRACT_CB_SKIP:
    strcpy(function_str, "skip");
    break;
  case EXTRACT_CB_DRIFT:
    strcpy(function_str, "drift");
    break;
  case EXTRACT_CB_BACKOFF:
    strcpy(function_str, "backoff");
    break;
  case EXTRACT_CB_OVERLAP:
    strcpy(function_str, "overlap");
    break;
  case EXTRACT_CB_FIXUP_DROPPED:
    strcpy(function_str, "fixup dropped");
    break;
  case EXTRACT_CB_FIXUP_DUPED:
    strcpy(function_str, "fixup duped");
    break;
  case EXTRACT_CB_READERR:
    strcpy(function_str, "read error");
    break;
  case EXTRACT_CB_CACHEERR:
    strcpy(function_str, "cache error");
    break;
  default:
    // unsupported callback message type
    sprintf(function_str, "unknown: %d", function);
    rpt_type = CDE_MSG_TYPE_WARNING;
    break;
  }

  // use cde_report to print buffer
  cde_report(CDE_MSG_TYPE_PROGRESS, "[%d] [%s] [%d] [%d] [%d] [%.1f%%]     ", rpt_type, function_str, function, inpos, sector, percentage);
}

/**
 * virtual drive dummy hook
 */
static int dummy_hook(cdrom_drive *d, int v) {
  return(0);
}


//
// internal functions
//

/**
 * @brief set internal status
 */
static inline void cde_set_status(cde_state *cde, int cde_state) {
  pthread_mutex_lock(&state_mutex);
  cde->status = cde_state;
  pthread_mutex_unlock(&state_mutex);
}

/**
 * @brief get internal status
 */
static inline int cde_get_status(cde_state *cde) {
  int cde_status = 0;
  pthread_mutex_lock(&state_mutex);
  cde_status = cde->status;
  pthread_mutex_unlock(&state_mutex);
  return cde_status;
}

//
// public functions
//

/**
 * @brief show cd extract, cdda and paranoia library versions
 */
void cde_version() {
  cde_report(CDE_MSG_TYPE_INFO, "cdextract library version: %s %d.%d", CDEXTRACT_NAME, CDEXTRACT_VERSION_MAJOR, CDEXTRACT_VERSION_MINOR);
  cde_report(CDE_MSG_TYPE_INFO, "cdda library version: %s", cdda_version());
  cde_report(CDE_MSG_TYPE_INFO, "paranoia library version: %s", paranoia_version());
}

/**
 * @brief intialize the cdextraction library
 */
void cde_initialize(cde_state *cde, char *cde_device_name, char *cde_audio_folder, char *cde_cddb_folder, char *cde_web_folder, void(*rpt_callback)(int, char*), void(*progress_callback)(int, int, int, long, float)) {
  
  if (cde==NULL) {
    return;
  }

  if (cde_get_status(cde) != CDE_STATUS_UNINITIALIZED) {
    return;
  }

  // prepare cde state structure
  cde->cdrom_device = NULL;
  cde->drv = NULL;
  cde->disc_info = NULL;
  cde->folder = calloc(PATH_MAX+1, sizeof(char));

  if (cde_device_name != NULL) {
    cde->cdrom_device = calloc(strlen(cde_device_name) + 1, sizeof(char));
    strcpy(cde->cdrom_device, cde_device_name);
  } else {
    cde->cdrom_device = calloc(1, sizeof(char));
  }

  if (cde_audio_folder != NULL) {
    cde->audio_folder = calloc(strlen(cde_audio_folder) + 1, sizeof(char));
    strcpy(cde->audio_folder, cde_audio_folder);
  } else {
    cde->audio_folder = calloc(5, sizeof(char));
    strcpy(cde->audio_folder, "/tmp");
  }

  if (cde_cddb_folder != NULL) {
    cde->cddb_folder = calloc(strlen(cde_cddb_folder) + 1, sizeof(char));
    strcpy(cde->cddb_folder, cde_cddb_folder);
  } else {
    cde->cddb_folder = calloc(10, sizeof(char));
    strcpy(cde->cddb_folder, "/tmp/cddb");
  }

  if (cde_web_folder != NULL) {
    cde->web_folder = calloc(strlen(cde_web_folder) + 1, sizeof(char));
    strcpy(cde->web_folder, cde_web_folder);
  } else {
    cde->web_folder = calloc(5, sizeof(char));
    strcpy(cde->web_folder, "/tmp");
  }

  external_rpt_callback_ptr = rpt_callback;
  external_progress_callback_ptr = progress_callback;

  cde->verbose = CDE_VERBOSE_OFF;
  cde->output_type = CDE_OUTPUT_TYPE_FLAC;
  cde->download_coverart = CDE_COVERART_OFF;
  cde->search_drive = CDE_SEARCH_DRIVE_OFF;
  cde->cd_speed = DEFAULT_CD_SPEED;
  cde->max_retries = DEFAULT_MAX_RETRIES;
  cde->abort_on_skip = CDE_ABORT_ON_SKIP_OFF;
  cde->eject_when_done = CDE_EJECT_WHEN_DONE_OFF;
  cde->write_json = CDE_WRITE_JSON_OFF;
  cde->write_cue_sheet = CDE_WRITE_CUE_SHEET_OFF;
  cde->write_cddb = CDE_WRITE_CDDB_OFF;
  cde->show_disc_info = CDE_SHOW_DISC_INFO_OFF;
  cde->virtual_drive = CDE_VIRTUAL_DRIVE_OFF;

  cde_set_status(cde, CDE_STATUS_INITIALIZED);
}

/***
 * @brief intialize the cdextraction library with the given option
 */
void cde_set_option(cde_state *cde, int option, int varg) {
  if (cde_get_status(cde) != CDE_STATUS_UNINITIALIZED && cde_get_status(cde) != CDE_STATUS_INITIALIZED && cde_get_status(cde) != CDE_STATUS_IDLE) {
    return;
  }
  switch (option) {
    case CDE_OPTION_VERBOSE:
      cde->verbose = (int)varg ? 1 : 0;
      break;
    case CDE_OPTION_OUTPUT_TYPE:   
      if ((int)varg==CDE_OUTPUT_TYPE_FLAC) { 
        cde->output_type = CDE_OUTPUT_TYPE_FLAC;
      } else {
        cde->output_type = CDE_OUTPUT_TYPE_WAV;
      }
      break;
    case CDE_OPTION_COVERART:
      if ((int)varg==CDE_COVERART_COVER_ONLY) { 
        cde->download_coverart = CDE_COVERART_COVER_ONLY; 
      } else if ((int)varg==CDE_COVERART_FULL) { 
        cde->download_coverart = CDE_COVERART_FULL; 
      } else {
        cde->download_coverart = CDE_COVERART_OFF;
      }
      break;
    case CDE_OPTION_SEARCH_DRIVE:
      cde->search_drive = (int)varg ? 1 : 0;
      break;
    case CDE_OPTION_ABORT_ON_SKIP:
      cde->abort_on_skip = (int)varg ? 1 : 0;
      break;
    case CDE_OPTION_EJECT_WHEN_DONE:
      cde->eject_when_done = (int)varg ? 1 : 0;
      break;
    case CDE_OPTION_WRITE_JSON:
      cde->write_json = (int)varg ? 1 : 0;
      break;
    case CDE_OPTION_WRITE_CUE_SHEET:
      cde->write_cue_sheet = (int)varg ? 1 : 0;
      break;
    case CDE_OPTION_WRITE_CDDB:
      cde->write_cddb = (int)varg ? 1 : 0;
      break;
    case CDE_OPTION_SHOW_DISC_INFO:
      cde->show_disc_info = (int)varg ? 1 : 0;
      break;
    case CDE_OPTION_VIRTUAL_DRIVE:
      cde->virtual_drive = (int)varg ? 1 : 0;
    case CDE_OPTION_CD_SPEED:
      cde->cd_speed = (int)varg > 0 ? (int)varg : 0;
      break;
    case CDE_OPTION_MAX_RETRIES:
      cde->max_retries = (int)varg >= 0 ? (int)varg : DEFAULT_MAX_RETRIES;
      break;
    default:
      cde_report(CDE_MSG_TYPE_WARNING, "unkown option: %d", option);
      break;
  }
}

/**
 * @brief cleanup and reset state
 */
void cde_cleanup(cde_state *cde) {
  if (cde) {
    if (cde->cdrom_device) {
      free(cde->cdrom_device);
      cde->cdrom_device = NULL;
    }
    if (cde->audio_folder) {
      free(cde->audio_folder);
      cde->audio_folder = NULL;
    }
    if (cde->cddb_folder) {
      free(cde->cddb_folder);
      cde->cddb_folder = NULL;
    }
    if (cde->web_folder) {
      free(cde->web_folder);
      cde->web_folder = NULL;
    }
    if (cde->folder) {
      free(cde->folder);
      cde->folder = NULL;
    }
    if (cde->drv) {
      cde_close_drive(cde);
    }
    if (cde->disc_info) {
      cde_free_disc(&cde->disc_info, -1);
    }
  }

  cde_set_status(cde, CDE_STATUS_UNINITIALIZED);
}

/**
 * @brief set and (optionally) create the output path
 *        the output path uses the following structure:
 *        {ROOT_FOLDER}/{ARTIST}/{ALBUM_TITLE} ({ALBUM_YEAR})/
 * @param cde cdextract state
 * @param create_path indicator to create output path (1) or only set the folder in cde (0)
 * @return 0 on success, non-zero on failure
 */
int cde_set_create_output_path(cde_state *cde, int create_output_path) {
  if (cde->disc_info == NULL) {
    return (int)CDE_ERROR_NO_DISC;
  }
  struct stat st = {0};
  if (cde->folder) {
    cde->folder = realloc(cde->folder, (PATH_MAX+1) * sizeof(char));
  } else {
    cde->folder = calloc(PATH_MAX+1, sizeof(char));
  }
  char *artist_folder = replace_chars(cde->disc_info->d_artist, FILENAME_CHAR_FILTER, '-');
  char *title_folder = replace_chars(cde->disc_info->d_title, FILENAME_CHAR_FILTER, '-');
  if (cde->disc_info->d_year > 0) {
    snprintf(cde->folder, PATH_MAX, "%s/%s/%s (%d)", cde->audio_folder, artist_folder, title_folder, cde->disc_info->d_year);
  } else {
    snprintf(cde->folder, PATH_MAX, "%s/%s/%s", cde->audio_folder, artist_folder, title_folder);
  }
  free(title_folder);
  free(artist_folder);
  cde_report(CDE_MSG_TYPE_INFO, "cde_set_create_output_path: using output folder: %s", cde->folder);
  if (create_output_path == 1) {
    if (stat(cde->folder, &st) == -1) {
        // create output folder
        mode_t dir_mode = (S_IRWXU | S_IRWXG | S_IRWXO); // S_IRWXU
        if (create_path(cde->folder, dir_mode) != 0) {
          cde_report(CDE_MSG_TYPE_ERROR, "cde_set_create_output_path: unable to create output folder: %s", cde->folder);
          return (int)1;
        }
    } else {
      cde_report(CDE_MSG_TYPE_WARNING, "cde_set_create_output_path: output folder: %s already exists", cde->folder);
    }
  }
  return (int)0;
}

/**
 * @brief set the filename for a track
 *        we use the following output format:
 *        {TRACKNUM}.{TRACK_TITLE}.{FILE_SUFFIX}
 *        for a disc containing 'Various' artists and an artist name for the track we use:
 *        {TRACKNUM}.{TRACK_ARTIST}-{TRACK_TITLE}.{FILE_SUFFIX}
 * @param disc_info pointer to the disc information
 * @param num track number
 * @param file_suffix suffix to append to the filename
 * @return 0 on success, non-zero on failure
 */
int cde_set_track_filename(disc *disc_info, int num, const char *file_suffix) {
  if (disc_info == NULL || num < 0 || num >= disc_info->d_tracks) {
    return -1;
  }

  char *artist_folder = replace_chars(disc_info->d_artist, FILENAME_CHAR_FILTER, '-');
  char *album_folder = replace_chars(disc_info->d_title, FILENAME_CHAR_FILTER, '-');

  char *track_raw = calloc(PATH_MAX+1, sizeof(char));
  if (strcmp(disc_info->d_artist, "Various") == 0) {
    // for a disc with 'Various' artists
    if (strlen(disc_info->tracks[num].t_artist) > 0) {
      // with artist name: {TRACKNUM}.{TRACK_ARTIST}-{TRACK_TITLE}.{FILE_SUFFIX}
      snprintf(track_raw, PATH_MAX, "%02d.%s-%s.%s", disc_info->tracks[num].t_num, disc_info->tracks[num].t_artist, disc_info->tracks[num].t_title, file_suffix);
    } else {
      // no artist name: {TRACKNUM}.{TRACK_TITLE}.{FILE_SUFFIX}
      snprintf(track_raw, PATH_MAX, "%02d.%s.%s", disc_info->tracks[num].t_num, disc_info->tracks[num].t_title, file_suffix);
    }
  } else {
    // default: {TRACKNUM}.{TRACK_TITLE}.{FILE_SUFFIX}
    snprintf(track_raw, PATH_MAX, "%02d.%s.%s", disc_info->tracks[num].t_num, disc_info->tracks[num].t_title, file_suffix);
  }
  
  // replace characters not allowed in filename (linux and windows) and set the track filename
  char *track_filename = replace_chars(track_raw, FILENAME_CHAR_FILTER, '-');
  if (disc_info->tracks[num].t_filename != NULL) {
    free(disc_info->tracks[num].t_filename);
  }
  if (disc_info->d_year > 0) {
    disc_info->tracks[num].t_filename = calloc(strlen(artist_folder) + strlen(album_folder) + strlen(track_filename) + 10, sizeof(char));
    if (disc_info->tracks[num].t_filename == NULL) {
      free(track_filename);
      free(track_raw);
      free(album_folder);
      free(artist_folder);
      return -1;
    }
    sprintf(disc_info->tracks[num].t_filename, "%s/%s (%d)/%s", artist_folder, album_folder, disc_info->d_year, track_filename);
  } else {
    disc_info->tracks[num].t_filename = calloc(strlen(artist_folder) + strlen(album_folder) + strlen(track_filename) + 3, sizeof(char));
    if (disc_info->tracks[num].t_filename == NULL) {
      free(track_filename);
      free(track_raw);
      free(album_folder);
      free(artist_folder);
      return -1;
    }
    sprintf(disc_info->tracks[num].t_filename, "%s/%s/%s", artist_folder, album_folder, track_filename);
  }

  free(track_filename);
  free(track_raw);
  free(album_folder);
  free(artist_folder);
  return 0;
}

/**
 * @brief dynamically allocate a disc information structure
 */
disc *cde_alloc_disc(int nr_of_tracks) {
  // allocate disc information structure
  disc *d = malloc(sizeof(disc));
  
  // set default attributes (can be overridden later)
  d->db_id = 0;
  d->d_lookup = 0;
  d->d_id = 0;
  d->d_length = 0;
 
  d->d_artist = calloc(15, sizeof(char));
  strcpy(d->d_artist, CDE_UNKNOWN_ARTIST);
  d->d_title = calloc(14, sizeof(char));
  strcpy(d->d_title, CDE_UNKNOWN_ALBUM);
  d->d_genre = calloc(5, sizeof(char));
  strcpy(d->d_genre, CDE_UNKNOWN_GENRE);
  d->d_year = 0;
  d->d_extended = calloc(1, sizeof(char));
  d->cddb_query = calloc(1, sizeof(char));
  d->cddb_category = calloc(1, sizeof(char));
  d->cddb_e_id = 0;
  d->cddb_d_id = 0;
  d->cddb_revision = 0;
  d->cddb_complete = 0;
  d->mb_query = calloc(1, sizeof(char));
  d->mb_fuzzy_lookup = calloc(1, sizeof(char));
  d->mb_disc_id = calloc(1, sizeof(char));
  d->mb_release_id = calloc(1, sizeof(char));
  d->mb_front_cover = NULL;
  d->mb_front_cover_size = 0;
  d->mb_back_cover = NULL;
  d->mb_back_cover_size = 0;
  d->mb_complete = 0;
  d->d_extracted = 0;
  // note: memory allocation only, actual number of tracks to be set by caller
  d->d_tracks = 0; 
  d->tracks = malloc(nr_of_tracks * sizeof(track));
  // add basic track information (can be overridden later)
  for (int i = 0; i < nr_of_tracks; i++) {
    d->tracks[i].t_num = i+1;                                        
    d->tracks[i].t_length = 0;                                    
    d->tracks[i].t_title = calloc(10, sizeof(char));                
    sprintf(d->tracks[i].t_title, "Track %02d", (unsigned char)i+1);
    d->tracks[i].t_artist = calloc(1, sizeof(char));	
    d->tracks[i].t_album = calloc(1, sizeof(char));			
    d->tracks[i].t_genre = calloc(1, sizeof(char));			
    d->tracks[i].t_year = 0;
    d->tracks[i].t_extended = calloc(1, sizeof(char));
    d->tracks[i].t_filename = calloc(1, sizeof(char));
    d->tracks[i].t_skipped = 0;
  }

  return d;
}

/**
 * @brief free a dynamically allocated disc information structure
 * @param ppdisc pointer to the pointer of the disc structure to be freed
 * @param tracks_allocated number of tracks allocated in the disc structure; -1 = use d_tracks from the disc structure
 */
void cde_free_disc(disc **ppdisc, int tracks_allocated) {
  if (*ppdisc == NULL) {
    return;
  }
  
  // determine amount of tracks to free
  if ((*ppdisc)->tracks == NULL) {
    tracks_allocated = 0;
  } else if (tracks_allocated < 0) {
    tracks_allocated = (*ppdisc)->d_tracks;
  }

  // free track information
  for (int i = 0; i < tracks_allocated; i++) {                             
    free((*ppdisc)->tracks[i].t_title);              
    free((*ppdisc)->tracks[i].t_artist);
    free((*ppdisc)->tracks[i].t_album);		
    free((*ppdisc)->tracks[i].t_genre);	
    free((*ppdisc)->tracks[i].t_extended);
    free((*ppdisc)->tracks[i].t_filename);
  }

  // free disc information
  free((*ppdisc)->d_artist);
  free((*ppdisc)->d_title);
  free((*ppdisc)->d_genre); 
  free((*ppdisc)->d_extended);
  free((*ppdisc)->cddb_query);
  free((*ppdisc)->cddb_category);
  free((*ppdisc)->mb_query);
  free((*ppdisc)->mb_fuzzy_lookup);
  free((*ppdisc)->mb_disc_id);
  free((*ppdisc)->mb_release_id);
  free((*ppdisc)->mb_front_cover);
  free((*ppdisc)->mb_back_cover);

  free((*ppdisc)->tracks);
  free(*ppdisc);
  *ppdisc = NULL;
}

/**
 * @brief find and access the cdrom drive
 */
int cde_get_drive(cde_state *cde) {

  int result = CDE_OK;

  if (cde == NULL) {
    return CDE_ERROR_NO_DRIVE;
  }

  if (cde->drv != NULL && cde->drv->opened==1) {
    // drive already opened
    return result;
  }

  if (cde->virtual_drive == CDE_VIRTUAL_DRIVE_OFF) {

    // get physical cdrom device
    if (cde->cdrom_device && strlen(cde->cdrom_device) > 0) {
      cde->drv = cdda_identify(cde->cdrom_device, CDDA_MESSAGE_FORGETIT, NULL);
    } else if (cde->search_drive) {
      cde->drv = cdda_find_a_cdrom(CDDA_MESSAGE_FORGETIT, NULL);
    } else {
      // check for /dev/cdrom
      struct stat s;
      if (lstat("/dev/cdrom", &s)) {
        // not found: search
        cde->drv = cdda_find_a_cdrom(CDDA_MESSAGE_FORGETIT, NULL);
      } else {
        // found: use /dev/cdrom
        cde->drv = cdda_identify("/dev/cdrom", CDDA_MESSAGE_FORGETIT, NULL);
      }
    }

    if (cde->drv==NULL) {
      cde_report(CDE_MSG_TYPE_ERROR, "cde_get_drive: unable to open drive");
      return CDE_ERROR_NO_DRIVE;
    }

    if (cde->verbose) {
      cdda_verbose_set(cde->drv, CDDA_MESSAGE_PRINTIT, CDDA_MESSAGE_PRINTIT);
    } else {
      cdda_verbose_set(cde->drv, CDDA_MESSAGE_PRINTIT, CDDA_MESSAGE_FORGETIT);
    }

    result = cdda_open(cde->drv);
    switch (result) {
    case -2:
    case -3:
    case -4:
    case -5:
      cde_report(CDE_MSG_TYPE_WARNING, "cde_get_drive: unable to access the disc - no audio cd present?");
      break;
    case -6:
      cde_report(CDE_MSG_TYPE_ERROR, "cde_get_drive: could not find a way to read audio from the drive");
      break;
    case 0:
      cde_report(CDE_MSG_TYPE_INFO, "cde_get_drive: disc opened");
      break;
    default:
      cde_report(CDE_MSG_TYPE_WARNING, "cde_get_drive: unable to open disc");
      break;
    }
    
  } else {

    // get 'virtual' drive
    if (cde->drv == NULL) {
      cde->drv = calloc(1, sizeof(cdrom_drive));
      if (cde->cdrom_device && strlen(cde->cdrom_device) > 0) {
        cde->drv->cdda_device_name = copy_string(cde->cdrom_device);
        cde->drv->ioctl_device_name = copy_string(cde->cdrom_device);
      } else {
        cde->drv->cdda_device_name = copy_string(CDE_VIRTUAL_DRIVE);
        cde->drv->ioctl_device_name = copy_string(CDE_VIRTUAL_DRIVE);
      }
      cde->drv->drive_model = copy_string("virtual cdrom device");
      cde->drv->drive_type = 0;
      cde->drv->cdda_fd = 0;
      cde->drv->ioctl_fd = 0;
      cde->drv->interface = 1;
      cde->drv->bigendianp = -1;
      cde->drv->nsectors = -1;
      cde->drv->opened = 1;
      cde->drv->set_speed = dummy_hook;
      cde->drv->enable_cdda = dummy_hook;
      for (int i=0; i<CDE_MAX_TRACKS; i++) {
        cde->drv->disc_toc[i].bFlags = 0x00;
        cde->drv->disc_toc[i].bTrack = 0;
        cde->drv->disc_toc[i].dwStartSector = 0;
      }
    }
    cde_report(CDE_MSG_TYPE_INFO, "cde_get_drive: virtual disc opened");
  }

  return result;
}

/**
 * @brief open the cdrom drive
 */
int cde_open_drive(cde_state *cde) {
  int res;
  // open physical or virtual cdrom device
  res = cde_get_drive(cde);
  if (res == CDE_OK) {
    cde_set_status(cde, CDE_STATUS_IDLE);
  }
  // copy real device name used if available
  if (cde->drv) {
    if (cde->cdrom_device) {
      cde->cdrom_device = realloc(cde->cdrom_device, (strlen(cde->drv->ioctl_device_name)+1) * sizeof(char));
    } else {
      cde->cdrom_device = calloc(strlen(cde->drv->ioctl_device_name) + 1, sizeof(char));
    }
    strcpy(cde->cdrom_device, cde->drv->ioctl_device_name);
  }
  return res;
}

/***
 * @brief close the cdrom drive
 */
int cde_close_drive(cde_state *cde) {
  if (cde->drv) {
    if (cde->virtual_drive == CDE_VIRTUAL_DRIVE_OFF) {
      cdda_close(cde->drv);
    } else {
      if (cde->drv->cdda_device_name) {
        free(cde->drv->cdda_device_name);
      }
      if (cde->drv->ioctl_device_name) {
        free(cde->drv->ioctl_device_name);
      }
      if (cde->drv->drive_model) {
        free(cde->drv->drive_model);
      }
      free(cde->drv);
    }
    cde->drv = NULL;
    cde_set_status(cde, CDE_STATUS_INITIALIZED);
    return (int)0;
  }
  return (int)1;
}

/**
 * @brief calculate and set the 64-bit internal hash value of the disc information to enable fast lookups
 */
void cde_set_hash(disc *disc_info) {
  // prepare the internal 64-bit hash to identify the disc with the disc length and number of tracks
  hash_init(disc_info->d_length, disc_info->d_tracks, &(disc_info->d_lookup));
  // add last track length in seconds
  if (disc_info->d_tracks > 0) {
    hash_update(disc_info->tracks[disc_info->d_tracks-1].t_length / CDE_CD_FRAMES, &(disc_info->d_lookup));
  }
  // add track length in frames for each track
  for (int i = disc_info->d_tracks-2; i >= 0; i--) {
    hash_update(disc_info->tracks[i].t_length, &(disc_info->d_lookup));
  }
}

/**
 * @brief prepare disc_info structure by reading the toc and 
 *        calculating the cddb and musicbrainz disc id hashes
 */
int cde_prepare_disc_info(cde_state *cde) {
  SHA_INFO	sha;
  unsigned long	size;
  long audiolen = 0;
  unsigned char digest[20];
  char tmp[32];
  char cddb_query[2048];
  char mb_query[2048];
  char mb_fuzzy[2048];

  if (cde==NULL || cde->drv==NULL || cde->drv->tracks<=0) {
    return 1;
  }

  if (cde->disc_info == NULL) {
    // allocate the disc information structure for the given number of tracks
    cde->disc_info = cde_alloc_disc(cde->drv->tracks);
  }

  // gather basic track information and calculate the cddb disc id
  int csum = 0;
  for (int i = 1; i <= cde->drv->tracks; i++) {
    if (cdda_track_audiop(cde->drv, i) > 0) {
      long sec = cdda_track_firstsector(cde->drv, i);
      long len = cdda_track_lastsector(cde->drv, i) - sec + 1;
      audiolen += len;
      // add basic track information with track number and track length in frames
      cde->disc_info->tracks[i-1].t_num = i;
      cde->disc_info->tracks[i-1].t_length = len;
      // calculate checksum for cddb disc id
      csum += cddb_sum((sec + CD_MSF_OFFSET) / CD_FRAMES);
    }
  }
  cde->disc_info->d_id = (csum % 0xff) << 24 | (int)((audiolen / CD_FRAMES) << 8) | (unsigned char)(cde->drv->tracks % 0xff);
  cde->disc_info->d_length = (int)audiolen;
  cde->disc_info->d_tracks = cde->drv->tracks;

  // the cddb query string starts with the disc id
  sprintf((char *)cddb_query, "%08x", cde->disc_info->d_id);

  // the musicbrainz query string starts with the first track
  sprintf((char *)mb_query, "%d", 1);

  // the musicbrainz fuzzy toc lookup starts with the first track
  sprintf((char *)mb_fuzzy, "%d", 1);

  // add the number of tracks to the query strings
  sprintf((char *)tmp, "+%d", cde->disc_info->d_tracks);
  strcat((char *)cddb_query, (char *)tmp);
  strcat((char *)mb_query, (char *)tmp);
  strcat((char *)mb_fuzzy, (char *)tmp);

  // add the disc length in frames for the musicbrainz fuzzy toc lookup
  sprintf((char *)tmp, "+%ld", cdda_track_lastsector(cde->drv, cde->drv->tracks) + CD_MSF_OFFSET);
  strcat((char *)mb_fuzzy, (char *)tmp);

  // add frame offsets of all tracks
  for (int i = 1; i <= cde->drv->tracks; i++) {
    sprintf((char *)tmp, "+%ld", cdda_track_firstsector(cde->drv, i) + CD_MSF_OFFSET);
    strcat((char *)cddb_query, (char *)tmp);
    strcat((char *)mb_query, (char *)tmp);
    strcat((char *)mb_fuzzy, (char *)tmp);
  }

  // add length of disc in seconds to the cddb query
  sprintf((char *)tmp, "+%ld", (cdda_track_lastsector(cde->drv, cde->drv->tracks) + CD_MSF_OFFSET) / CD_FRAMES);
  strcat((char *)cddb_query, (char *)tmp);

  // set the cddb query string
  cde->disc_info->cddb_query = realloc(cde->disc_info->cddb_query, (strlen((char *)cddb_query)+1) * sizeof(char));
  strcpy(cde->disc_info->cddb_query, (char *)cddb_query);
  cde_report(CDE_MSG_TYPE_DEBUG, "cddb query:%s", cde->disc_info->cddb_query);

  // add length of disc in frames for the musicbrainz query service
  sprintf((char *)tmp, "+%ld", cdda_track_lastsector(cde->drv, cde->drv->tracks) + CD_MSF_OFFSET);
  strcat((char *)mb_query, (char *)tmp);

  // set the musicbrainz query string
  cde->disc_info->mb_query = realloc(cde->disc_info->mb_query, (strlen((char *)mb_query)+1) * sizeof(char));
  strcpy(cde->disc_info->mb_query, (char *)mb_query);
  cde_report(CDE_MSG_TYPE_DEBUG, "musicbrainz query:%s", cde->disc_info->mb_query);

  // set the mb fuzzy toc lookup string
  cde->disc_info->mb_fuzzy_lookup = realloc(cde->disc_info->mb_fuzzy_lookup, (strlen((char *)mb_fuzzy)+1) * sizeof(char));
  strcpy(cde->disc_info->mb_fuzzy_lookup, (char *)mb_fuzzy);
  cde_report(CDE_MSG_TYPE_DEBUG, "musicbrainz fuzzy lookup:%s", cde->disc_info->mb_fuzzy_lookup);

  // prepare the musicbrainz disc id by creating a sha1 hash of the toc data
  sha_init(&sha);
  // add first track
	sprintf((char *)tmp, "%02X", 1);
	sha_update(&sha, (SHA_BYTE *) tmp, strlen((char *)tmp));
  // add number of tracks
	sprintf((char *)tmp, "%02X", cde->drv->tracks);
	sha_update(&sha, (SHA_BYTE *) tmp, strlen((char *)tmp));
  // add disc length
  sprintf((char *)tmp, "%08X", cde->drv->disc_toc[cde->drv->tracks].dwStartSector + CD_MSF_OFFSET);
	sha_update(&sha, (SHA_BYTE *) tmp, strlen((char *)tmp));
  // add track offsets
  for (int i = 1; i < 100; i++) {
    if (i <= cde->drv->tracks) {
      sprintf((char *)tmp, "%08X", cde->drv->disc_toc[i-1].dwStartSector + CD_MSF_OFFSET);
    } else {
      sprintf((char *)tmp, "%08X", 0);
    }
		sha_update(&sha, (SHA_BYTE *)tmp, strlen((char *)tmp));
	}
	sha_final(&digest[0], &sha);
  // base64 encode and store the musicbrainz disc id
  free(cde->disc_info->mb_disc_id);
	cde->disc_info->mb_disc_id = (char *)rfc822_binary(&digest[0], sizeof(digest), &size);
  cde_report(CDE_MSG_TYPE_DEBUG, "musicbrainz disc id:%s", cde->disc_info->mb_disc_id);

  // set the 64-bit internal hash to identify and lookup the disc
  cde_set_hash(cde->disc_info);
  cde_report(CDE_MSG_TYPE_DEBUG, "internal disc lookup hash:%016llx", cde->disc_info->d_lookup);

  return CDE_OK;
}

/**
 * @brief displays the table of contents
 */
void cde_display_toc(cdrom_drive *drv) {
  long audiolen = 0;
  int csum = 0;

  if (drv == NULL) {
    cde_report(CDE_MSG_TYPE_WARNING, "cde_display_toc: no table of contents (TOC) information");
    return;
  }

  cde_report(CDE_MSG_TYPE_INFO, "table of contents (TOC):");
  cde_report(CDE_MSG_TYPE_INFO, "track  first sector length   begin time   length");

  for (int i = 1; i <= drv->tracks; i++) {
    if (cdda_track_audiop(drv, i) > 0) {

      long sec = cdda_track_firstsector(drv, i);
      long off = cdda_track_lastsector(drv, i) - sec + 1;

      cde_report(CDE_MSG_TYPE_INFO, "%3d.   %7ld    %7ld    [%02d:%02d.%02d]   [%02d:%02d.%02d]", 
             i, sec, off, 
             (int)(sec / (60 * CD_FRAMES)), (int)((sec / CD_FRAMES) % 60), (int)(sec % CD_FRAMES),
             (int)(off / (60 * CD_FRAMES)), (int)((off / CD_FRAMES) % 60), (int)(off % CD_FRAMES));
      audiolen += off;

      // calculate checksum for disc id
      csum += cddb_sum((sec + CD_MSF_OFFSET) / CD_FRAMES);
    }
  }

  // print total number of tracks, frames and duration
  cde_report(CDE_MSG_TYPE_INFO, "total: %d tracks %7ld [%02ld:%02ld.%02ld]", drv->tracks, audiolen, (audiolen / (60 * CD_FRAMES)), (audiolen / CD_FRAMES) % 60, audiolen % CD_FRAMES);

  // print discid
  cde_report(CDE_MSG_TYPE_DEBUG, "disc id: %08x ", (csum % 0xff) << 24 | (int)((audiolen / CD_FRAMES) << 8) | (unsigned char)(drv->tracks % 0xff));
  
  // print cddb sum
  cde_report(CDE_MSG_TYPE_DEBUG, "cddb sum: %d", (int)csum);
}

/**
 * @brief displays the gathered disc information
 */
void cde_display_disc_info(disc *disc_info) {

  if (disc_info == NULL) {
    cde_report(CDE_MSG_TYPE_WARNING, "cde_display_disc_info: no disc information");
    return;
  }
  
  // print discid
  cde_report(CDE_MSG_TYPE_INFO, "cd with disc id: %08x and length in frames: %d", disc_info->d_id, disc_info->d_length);
  
  // print cddb information
  cde_report(CDE_MSG_TYPE_INFO, "cddb category %s, entry id: %08x and disc id: %08x", disc_info->cddb_category, disc_info->cddb_e_id, disc_info->cddb_d_id);

  // print musicbrainz discid
  cde_report(CDE_MSG_TYPE_INFO, "musicbrainz disc id: %s and release id: %s", disc_info->mb_disc_id, disc_info->mb_release_id);

  // print title and artist
  cde_report(CDE_MSG_TYPE_INFO, "cd title: %s, cd artist: %s, cd genre: %s, cd year: %d", disc_info->d_title, disc_info->d_artist, disc_info->d_genre, disc_info->d_year);
  cde_report(CDE_MSG_TYPE_INFO, "table of contents:");
  cde_report(CDE_MSG_TYPE_INFO, "track  length   title");
  
  // print tracks
  for (int i = 1; i <= disc_info->d_tracks && i <= CDE_MAX_TRACKS; i++) {
      cde_report(CDE_MSG_TYPE_INFO, "%3d.   [%02d:%02d]  %s - %s - %s [%d] %s", 
        disc_info->tracks[i-1].t_num, (int)((disc_info->tracks[i-1].t_length/CDE_CD_FRAMES)/60), (int)((disc_info->tracks[i-1].t_length/CDE_CD_FRAMES)%60), disc_info->tracks[i-1].t_title,
        disc_info->tracks[i-1].t_artist, disc_info->tracks[i-1].t_album, disc_info->tracks[i-1].t_year, disc_info->tracks[i-1].t_extended);
  }

  cde_report(CDE_MSG_TYPE_INFO, "total: %d tracks [%02d:%02d:%02d.%02d]", disc_info->d_tracks, (disc_info->d_length / CDE_CD_FRAMES) / 3600, ((disc_info->d_length / CDE_CD_FRAMES) / 60) % 60, (disc_info->d_length / CDE_CD_FRAMES) % 60, disc_info->d_length % CD_FRAMES);

}

/**
 * @brief writes the gathered disc information to a file
 */
int cde_write_disc_info(cde_state *cde, int overwrite) {
  return json_write_disc_info(cde->disc_info, cde->audio_folder, overwrite, cde->verbose);
}

/**
 * @brief download cddb/musicbrainz disc information and covers
 *        pre: cde_initialize has been called to set the right settings
 */
int cde_download_disc_info(cde_state *cde, int fuzzy_lookup, int overwrite, int cleanup) {
  int res = CDE_OK;

  if (cde_get_status(cde) != CDE_STATUS_IDLE) {
    cde_report(CDE_MSG_TYPE_ERROR, "cde_download_disc_info: cannot get disc information unless in idle state");
    return CDE_ERROR_NOT_IDLE;
  }

  cde_set_status(cde, CDE_STATUS_PREPARING);

  // open the drive
  if (cde->drv == NULL) {
    res = cde_open_drive(cde);
    if (res != 0) {
      cde_report(CDE_MSG_TYPE_ERROR, "cde_download_disc_info: unable to open drive");
      // return to 'idle' state and cleanup
      res = CDE_ERROR_NO_DRIVE;
      goto cleanup;
    }
  } else {
    cde_report(CDE_MSG_TYPE_INFO, "cde_download_disc_info: using drive %s", cde->drv->ioctl_device_name);
  }

  // show the table of contents
  if (cde->verbose == CDE_VERBOSE_ON) {
    cde_display_toc(cde->drv);
  }
  
  // extract toc information and cddb/musicbrains discid's
  if (cde_prepare_disc_info(cde) == 0) {

    // check if cancellation requested before starting the download of cddb disc information
    if (cde_get_status(cde) == CDE_STATUS_CANCEL) {
      cde_report(CDE_MSG_TYPE_INFO, "cde_download_disc_info: aborting download of cddb disc information");
      // return to 'idle' state and cleanup
      res = CDE_STATUS_CANCEL;
      goto cleanup;
    }

    // query the online cddb service
    int res = cddb_get_disc_info(cde->disc_info, cde->drv, cde->verbose);
    cde_report(CDE_MSG_TYPE_DEBUG, "cde_download_disc_info: cddb_get_disc_info: %d", res);

    // check if cancellation requested before starting the download of musicbrainz disc information
    if (cde_get_status(cde) == CDE_STATUS_CANCEL) {
      cde_report(CDE_MSG_TYPE_INFO, "cde_download_disc_info: aborting download of musicbrainz disc information");
      // return to 'idle' state and cleanup
      res = CDE_STATUS_CANCEL;
      goto cleanup;
    }

    // try to get the the musicbrainz release id to download the cover images(s) and
    // add the missing disc information in case the cddb query failed
    res &= mb_get_disc_info(cde->disc_info, fuzzy_lookup, cde->verbose);

    // set and create the output folder: {ROOT_FOLDER}/{ARTIST}/{ALBUM_TITLE}/
    if (cde_set_create_output_path(cde, 1) != 0) {
      cde_report(CDE_MSG_TYPE_DEBUG, "cde_download_disc_info: unable to set/create output path");
      res = CDE_ERROR_OUTPUT_PATH;
      goto cleanup;
    }

    // set the filename suffix (default set to flac)
    char *file_suffix = "flac";
    if (cde->output_type==CDE_OUTPUT_TYPE_WAV) { 
      strcpy(file_suffix, "wav");
    }

    // set track output filenames
    for (int i = 0; i < cde->disc_info->d_tracks; i++) {
      if (cde_set_track_filename(cde->disc_info, i, file_suffix) != 0) {
        goto cleanup;
      }
    }

    if (cde->download_coverart != CDE_COVERART_OFF) {

      // check if cancellation requested before starting the download of disc covers
      if (cde_get_status(cde) == CDE_STATUS_CANCEL) {
        cde_report(CDE_MSG_TYPE_INFO, "cde_download_disc_info: aborting download of disc covers");
        // return to 'idle' state and cleanup
        res = CDE_STATUS_CANCEL;
        goto cleanup;
      }

      // try to get front cover
      struct stat st = {0};
      char *front_cover_file = calloc(strlen(cde->folder)+strlen(CDE_COVER_FRONT)+2, sizeof(char));
      sprintf(front_cover_file, "%s/%s", cde->folder, CDE_COVER_FRONT);
      if (stat(front_cover_file, &st) == 0) {
        // file available: try to load the front cover from file
        cde_report(CDE_MSG_TYPE_DEBUG, "cde_download_disc_info: loading front cover");
        cde->disc_info->mb_front_cover_size = read_file(&cde->disc_info->mb_front_cover, front_cover_file);
      } 
      if (cde->disc_info->mb_front_cover_size <= 0) {
        if (strlen(cde->disc_info->mb_release_id) > 0) {
          // front cover not loaded from file: try to download front cover from the online coverart service
          cde_report(CDE_MSG_TYPE_DEBUG, "cde_download_disc_info: get front cover for release id: %s", cde->disc_info->mb_release_id);
          // note: we are only downloading the cover art to memory in this step with MB_COVERART_MEM_ONLY
          //       Covers are only written to file as part of the audio extraction process
          int r = mb_caa_get_front_cover(cde->disc_info, cde->download_coverart, cde->folder, cde->verbose);
          cde->disc_info->mb_complete = (r == 0 ? 1 : 0);
          res &= r;
        } else {
          cde_report(CDE_MSG_TYPE_WARNING, "cde_download_disc_info: unable to download cover: mb release id unavailable");
        }
      }
      free(front_cover_file);

      // try to get back cover
      char *back_cover_file = calloc(strlen(cde->folder)+strlen(CDE_COVER_BACK)+2, sizeof(char));
      sprintf(back_cover_file, "%s/%s", cde->folder, CDE_COVER_BACK);
      if (stat(back_cover_file, &st) == 0) {
        // file available: try to load the back cover from file
        cde_report(CDE_MSG_TYPE_DEBUG, "cde_download_disc_info: loading back cover");
        cde->disc_info->mb_back_cover_size = read_file(&cde->disc_info->mb_back_cover, back_cover_file);
      }
      if (cde->disc_info->mb_back_cover_size <= 0) {
        if (strlen(cde->disc_info->mb_release_id) > 0) {
          // back cover not loaded from file: try to download back cover from the online coverart service
          cde_report(CDE_MSG_TYPE_DEBUG, "cde_download_disc_info: get back cover for release id: %s", cde->disc_info->mb_release_id);
          // note: we are only downloading the cover art to memory in this step with MB_COVERART_MEM_ONLY
          //       Covers are only written to file as part of the audio extraction process
          int r = mb_caa_get_back_cover(cde->disc_info, cde->download_coverart, cde->folder, cde->verbose);
          res &= r;
        }
      }
      free(back_cover_file);

    } else {
      // do not download coverart: we are done now
      cde_report(CDE_MSG_TYPE_DEBUG, "cde_download_disc_info: skipping cover art");
		  cde->disc_info->mb_complete = 1;
    }

    if (cde->write_json == CDE_WRITE_JSON_ON) {
      // write the gathered disc information to a json file
      cde_report(CDE_MSG_TYPE_DEBUG, "cde_download_disc_info: writing disc information");
      json_write_disc_info(cde->disc_info, cde->folder, overwrite, cde->verbose);
    }

    if (cde->write_cue_sheet == CDE_WRITE_CUE_SHEET_ON) {
      // write the gathered disc information to a cue sheet
      cde_report(CDE_MSG_TYPE_DEBUG, "cde_download_disc_info: writing cue sheet");
      write_cue_sheet(cde->disc_info, cde->folder, overwrite, cde->verbose);
    }

    if (cde->write_cddb == CDE_WRITE_CDDB_ON) {
      // write the gathered disc information to a cddb entry in xmcd format
      cde_report(CDE_MSG_TYPE_DEBUG, "cde_download_disc_info: writing cddb entry");
      cddb_write_entry(cde->disc_info, cde->folder, overwrite, cde->verbose);
    }
  }
 
  if (cde->show_disc_info == CDE_SHOW_DISC_INFO_ON) {
    // show the gathered disc information
    cde_display_disc_info(cde->disc_info);
  }

  // download disc information completed
  cde_report(CDE_MSG_TYPE_DEBUG, "cde_download_disc_info: completed");

cleanup:
  if (cleanup) {
    if (cde->drv) {
      cde_close_drive(cde);
    }
    if (cde->disc_info) {
      cde_free_disc(&cde->disc_info, -1);
    }
    if (cde->folder) {
      free(cde->folder);
      cde->folder = NULL;
    }
  }

  cde_set_status(cde, CDE_STATUS_IDLE);

  return res;
}

/**
 * @brief extract audio cd - actual cd audio extraction
 *        pre: cde_initialize has been called to set the right settings
 */
void *cde_extract_audio_t(void *state) {
  int res = 1;
  FILE *out;                                         // audio file to write
  cdrom_paranoia *p = NULL;                          // cdda paranoia

  // get cde_state pointer
  cde_state *cde = (cde_state*)state;

  if (cde == NULL) {
    // no cde_state available
    goto cleanup;
  }

  // get disc information
  if (cde->drv == NULL || cde->disc_info == NULL) {
    if (cde_download_disc_info(cde, 0, 0, 0) != CDE_OK) {
      // no drive and or disc information available
      goto cleanup;
    }
  }

  // set the filename suffix (default set to flac)
  char *file_suffix = calloc(5, sizeof(char));
  if (cde->output_type==CDE_OUTPUT_TYPE_WAV) { 
    strcpy(file_suffix, "wav");
  } else {
    strcpy(file_suffix, "flac");
  }

  // check if cancellation requested before starting the actual data extraction
  if (cde_get_status(cde) == CDE_STATUS_CANCEL) {
    cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: aborting data extraction: extraction cancelled");
    // return to 'idle' state and cleanup
    res = CDE_STATUS_CANCEL;
    goto cleanup;
  }

  // preparation done, update status to extracting
  cde_set_status(cde, CDE_STATUS_EXTRACTING);

  if (cde->drv->nsectors == 1) {
    cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: aborting data extraction: the autosensed/selected sectors per read value is one sector");
    res = 1;
    goto cleanup;
  }

  // set cdrom speed
  if (cdda_speed_set(cde->drv, cde->cd_speed)) {
    if (cde->verbose || cde->cd_speed != -1) {
      cde_report(CDE_MSG_TYPE_WARNING, "cde_extract_audio: attempt to set cdrom speed failed");
    }
  } else {
    if (cde->verbose) {
      cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: set cdrom speed: drive returned ok");
    }
  }

  // set up begin and end sectors of data to extract
  long first_sector = cdda_track_firstsector(cde->drv, 1);
  long last_sector = cdda_disc_lastsector(cde->drv);
  int first_track = cdda_sector_gettrack(cde->drv, first_sector);
  int last_track = cdda_sector_gettrack(cde->drv, last_sector);
  
  long cursor;
  int16_t offset_buffer[1176];
  int offset_buffer_used = 0;
  
  // unable to handle non-audio tracks
  for (int i = first_track; i <= last_track; i++) {
    if (!cdda_track_audiop(cde->drv, i)) {
      cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: disc contains non audio tracks; aborting data extraction");
      res = 1;
      goto cleanup;
    }
  }

  cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: extracting data from sector %7ld (track %2d) to sector %7ld (track %2d)", first_sector, first_track, last_sector, last_track);

  // set the tracks to skipped, when actually extracted skipped will be set to not skipped
  for (int i = 0; i < cde->disc_info->d_tracks; i++) {
    cde->disc_info->tracks[i].t_skipped = 1;
  }

  // set full paranoia mode, but allow skipping
  int paranoia_mode = PARANOIA_MODE_FULL ^ PARANOIA_MODE_NEVERSKIP;

  // initialize libparanoia
  p = paranoia_init(cde->drv);
  paranoia_modeset(p, paranoia_mode);

  if (cde->verbose) {
    cdda_verbose_set(cde->drv, CDDA_MESSAGE_LOGIT, CDDA_MESSAGE_LOGIT);
  } else {
    cdda_verbose_set(cde->drv, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);
  }
  paranoia_seek(p, cursor = first_sector, SEEK_SET);

  current_track = 0;
  // keep extracting till we reach the end (and don't reach the track extraction limit)
  while (cursor <= last_sector && current_track < CDE_MAX_TRACKS) {
   
    current_track = cdda_sector_gettrack(cde->drv, cursor);
    if (current_track < 1) {
      cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: invalid track (%d) for sector: %ld", current_track, cursor);
      // return to 'idle' state and cleanup
      res = 1;
      goto cleanup;
    }
    track_firstsector = cursor;
    track_lastsector = cdda_track_lastsector(cde->drv, current_track);
    if (track_lastsector > last_sector) {
      track_lastsector = last_sector;
    }

    // set full output filename
    cde_set_track_filename(cde->disc_info, current_track-1, file_suffix);
    char *output_filename = calloc(sizeof(char), PATH_MAX+1);
    snprintf(output_filename, PATH_MAX, "%s/%s", cde->audio_folder, cde->disc_info->tracks[current_track-1].t_filename);
    cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: output file %s", output_filename);

    // open file as binary write
    out = fopen(output_filename, "wb");
    if (out == NULL) {
      cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: cannot open output file %s: %s", output_filename, strerror(errno));
      // return to 'idle' state and cleanup
      res = 1;
      goto cleanup;
    }
    cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: track: %d; first sector: %ld; last sector: %ld", current_track, track_firstsector, track_lastsector);


    // start output by writing header (and metadata)
    flac_encoder_context *encoder_context = calloc(1, sizeof(flac_encoder_context));
    switch (cde->output_type) {
    case CDE_OUTPUT_TYPE_WAV:
      start_wav(out, (track_lastsector - track_firstsector + 1) * CD_FRAMESIZE_RAW);
      break;
    case CDE_OUTPUT_TYPE_FLAC:
      if (start_flac(out, (track_lastsector - track_firstsector + 1) * CD_FRAMESIZE_RAW, &cde->disc_info->tracks[current_track-1], encoder_context)) {
        cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: error starting flac");
        // return to 'idle' state and cleanup
        res = 1;
        goto cleanup;
      }
      break;
    }

    // write buffer to file
    if (offset_buffer_used) {
      cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: offset_buffer_used=true");
      // partial sector from previous batch read
      cursor++;
      if (cde->output_type == CDE_OUTPUT_TYPE_FLAC) {
        // write flac
        if (write_flac(encoder_context, ((char *)offset_buffer) + offset_buffer_used, CD_FRAMESIZE_RAW - offset_buffer_used)) {
          cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: error writing output: %s", strerror(errno));
          // return to 'idle' state and cleanup
          res = 1;
          goto cleanup;
        }
      } else {
        // write raw or wav
        if (write_wav(out, ((char *)offset_buffer) + offset_buffer_used, CD_FRAMESIZE_RAW - offset_buffer_used)) {
          cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: error writing output: %s", strerror(errno));
          // return to 'idle' state and cleanup
          res = 1;
          goto cleanup;
        } else {
          cde_report(CDE_MSG_TYPE_DEBUG, "cde_extract_audio: noffset_buffer_used=true; write_wav=ok;");
        }
      }
    }

    int skipped_flag = 0;
    while (cursor <= track_lastsector) {
      // use paranoia to read data
      int16_t *readbuf = paranoia_read_limited(p, cde_progress_callback, cde->max_retries);
      char *err = cdda_errors(cde->drv);
      char *mes = cdda_messages(cde->drv);

      if (mes || err) {
        cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: paranoia_read: %s%s", mes ? mes : "", err ? err : "");
      }
      if (err) {
        free(err);
      }
      if (mes) {
        free(mes);
      }
      if (readbuf == NULL) {
        if (errno == EBADF || errno == ENOMEDIUM) {
          cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: paranoia_read: drive unavailable");
          res = 1;
          goto cleanup;
        }
        skipped_flag = 1;
        cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: paranoia_read: unrecoverable error");
        break;
      }
      if (skipped_flag && cde->abort_on_skip) {
        cursor = track_lastsector + 1;
        break;
      }

      skipped_flag = 0;
      cursor++;

      cde_progress_callback(cursor * (CD_FRAMEWORDS)-1, EXTRACT_CB_WRITE_FILE);

      if (cde->output_type == CDE_OUTPUT_TYPE_FLAC) {
        // write flac
        res = write_flac(encoder_context, ((char *)readbuf), CD_FRAMESIZE_RAW);
        if (res) {
          cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: error writing output: %s", strerror(errno));
          goto cleanup;
        }
      } else {
        // write wav
        res = write_wav(out, ((char *)readbuf), CD_FRAMESIZE_RAW);
        if (res) {
          cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: error writing output: %s", strerror(errno));
          goto cleanup;
        }
      }

      // check if extraction has been cancelled
      if (cde_get_status(cde) == CDE_STATUS_CANCEL) {
        cde_report(CDE_MSG_TYPE_INFO, "aborting data extraction: extraction cancelled");
        skipped_flag = 1;
        break;
      }

    } // while read sector

    // end of file
    cde_progress_callback(cursor * (CD_FRAMESIZE_RAW / 2) - 1, EXTRACT_CB_END_OF_FILE);
    if (cde->output_type == CDE_OUTPUT_TYPE_FLAC) {
      cde_report(CDE_MSG_TYPE_DEBUG, "end_flac");
      res = end_flac(encoder_context);
      free(encoder_context);
      if (res) {
        goto cleanup;
      }
    } else {
      cde_report(CDE_MSG_TYPE_DEBUG, "end_wav");
      res = end_wav(out);
      if (res) {
        goto cleanup;
      }
    }

    // set the skipped indicator for this track
    cde->disc_info->tracks[current_track-1].t_skipped = skipped_flag;

    if (skipped_flag) {
      // remove the file
      cde_report(CDE_MSG_TYPE_INFO, "removing aborted file: %s", output_filename);
      unlink(output_filename);
      // check for cancellation
      if (cde_get_status(cde) == CDE_STATUS_CANCEL) {
        free(output_filename);
        goto cleanup;
      }
      // make the cursor correct if we have another track
      if (current_track != -1) {
        current_track++;
        cursor = cdda_track_firstsector(cde->drv, current_track);
        paranoia_seek(p, cursor, SEEK_SET);
        offset_buffer_used = 0;
      }
    }
    free(output_filename);
    cde_report(CDE_MSG_TYPE_INFO, "end of track");
  } // while extracting

  // we are done: set the extraction completed indicator 
  cde->disc_info->d_extracted = 1;

  // extraction done progress callback and status report
  cde_progress_callback(cursor * (CD_FRAMEWORDS)-1, EXTRACT_CB_END_OF_DISC);
  cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: done");

  // cleanup
cleanup:
  free(file_suffix);
  if (p) {
    paranoia_free(p);
    p = NULL;
  }

  // return to idle state
  cde_set_status(cde, CDE_STATUS_IDLE);

  // eject disc if audio extraction completed and requested to do so
  if (cde->eject_when_done == CDE_EJECT_WHEN_DONE_ON && cde->disc_info->d_extracted == 1) {
    cde_eject(cde);
  }

  // terminate thread
  pthread_exit(NULL);

  return (void*) (size_t)res;
}

/**
 * @brief extract audio cd - 'virtual' cd audio extraction
 *        pre: cde_initialize has been called to set the right settings
 */
void *cde_extract_audio_v(void *state) {
  int res = 1;
  //FILE *out;                                         // audio file to write
  cdrom_paranoia *p = NULL;                          // cdda paranoia

  // get cde_state pointer
  cde_state *cde = (cde_state*)state;

  if (cde == NULL) {
    // no cde_state available
    goto cleanup;
  }

  // get disc information
  if (cde->drv == NULL || cde->disc_info == NULL) {
    if (cde_download_disc_info(cde, 0, 0, 0) != CDE_OK) {
      // no drive and or disc information available
      goto cleanup;
    }
  }

  // set the filename suffix (default set to flac)
  char *file_suffix = "flac";
  if (cde->output_type==CDE_OUTPUT_TYPE_WAV) { 
    strcpy(file_suffix, "wav");
  }

  // check if cancellation requested before starting the actual data extraction
  if (cde_get_status(cde) == CDE_STATUS_CANCEL) {
    cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: aborting data extraction: extraction cancelled");
    // return to 'idle' state and cleanup
    res = CDE_STATUS_CANCEL;
    goto cleanup;
  }

  // preparation done, update status to extracting
  cde_set_status(cde, CDE_STATUS_EXTRACTING);

  if (cde->drv->nsectors == 1) {
    cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: aborting data extraction: the autosensed/selected sectors per read value is one sector");
    res = 1;
    goto cleanup;
  }

  // set cdrom speed
  if (cdda_speed_set(cde->drv, cde->cd_speed)) {
    if (cde->verbose || cde->cd_speed != -1) {
      cde_report(CDE_MSG_TYPE_WARNING, "cde_extract_audio: attempt to set cdrom speed failed");
    }
  } else {
    if (cde->verbose) {
      cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: set cdrom speed: drive returned ok");
    }
  }

  // set up begin and end sectors of data to extract
  long first_sector = cdda_track_firstsector(cde->drv, 1);
  long last_sector = cdda_disc_lastsector(cde->drv);
  int first_track = cdda_sector_gettrack(cde->drv, first_sector);
  int last_track = cdda_sector_gettrack(cde->drv, last_sector);
  
  long cursor;
  //int16_t offset_buffer[1176];
  //int offset_buffer_used = 0;
  
  // unable to handle non-audio tracks
  for (int i = first_track; i <= last_track; i++) {
    if (!cdda_track_audiop(cde->drv, i)) {
      cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: disc contains non audio tracks. Aborting data extraction.");
      res = 1;
      goto cleanup;
    }
  }

  cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: extracting data from sector %7ld (track %2d) to sector %7ld (track %2d)", first_sector, first_track, last_sector, last_track);

  // set full paranoia mode, but allow skipping
  int paranoia_mode = PARANOIA_MODE_FULL ^ PARANOIA_MODE_NEVERSKIP;

  // initialize libparanoia
  p = paranoia_init(cde->drv);
  paranoia_modeset(p, paranoia_mode);

  if (cde->verbose) {
    cdda_verbose_set(cde->drv, CDDA_MESSAGE_LOGIT, CDDA_MESSAGE_LOGIT);
  } else {
    cdda_verbose_set(cde->drv, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);
  }
  paranoia_seek(p, cursor = first_sector, SEEK_SET);

  current_track = 0;
  // keep extracting till we reach the end (and don't reach the track extraction limit)
  while (cursor <= last_sector && current_track < CDE_MAX_TRACKS) {
   
    current_track = cdda_sector_gettrack(cde->drv, cursor);
    if (current_track < 1) {
      cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: invalid track (%d) for sector: %ld", current_track, cursor);
      // return to 'idle' state and cleanup
      res = 1;
      goto cleanup;
    }
    track_firstsector = cursor;
    track_lastsector = cdda_track_lastsector(cde->drv, current_track);
    if (track_lastsector > last_sector) {
      track_lastsector = last_sector;
    }

    // set full output filename
    cde_set_track_filename(cde->disc_info, current_track-1, file_suffix);
    char *output_filename = calloc(sizeof(char), PATH_MAX+1);
    snprintf(output_filename, PATH_MAX, "%s/%s", cde->audio_folder, cde->disc_info->tracks[current_track-1].t_filename);
    cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: output file %s", output_filename);

    // open file as binary write
    cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: track: %d; first sector: %ld; last sector: %ld", current_track, track_firstsector, track_lastsector);

    // start output by writing header (and metadata)
    // ..

    // write buffer to file
    //if (offset_buffer_used) {
    //  cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: offset_buffer_used=true");
    //  // partial sector from previous batch read
    //  cursor++;
    //  // ..
    //}

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 20000000;
    int ret;
    int skipped_flag = 0;
    while (cursor <= track_lastsector) {
      // use paranoia to read data
      // int16_t *readbuf = paranoia_read_limited(p, cde_progress_callback, cde->max_retries);
      // ..
      // 'virtual' read: simulate read time: 20ms per sector
      do {
        ret = nanosleep(&ts, &ts);
      } while (ret && errno == EINTR);
      // 'virtual' read: update read progress
      cde_progress_callback(cursor * (CD_FRAMEWORDS)-1, EXTRACT_CB_READ);

      skipped_flag = 0;
      cursor++;

      cde_progress_callback(cursor * (CD_FRAMEWORDS)-1, EXTRACT_CB_WRITE_FILE);

      // write data: write(out, ((char *)readbuf), CD_FRAMESIZE_RAW)

      // check if extraction has been cancelled
      if (cde_get_status(cde) == CDE_STATUS_CANCEL) {
        cde_report(CDE_MSG_TYPE_INFO, "aborting data extraction: extraction cancelled");
        skipped_flag = 1;
        break;
      }

    } // while read sector

    // end of file
    cde_progress_callback(cursor * (CD_FRAMESIZE_RAW / 2) - 1, EXTRACT_CB_END_OF_FILE);
    if (cde->output_type == CDE_OUTPUT_TYPE_FLAC) {
      cde_report(CDE_MSG_TYPE_DEBUG, "end_flac");
      // ..
    } else {
      cde_report(CDE_MSG_TYPE_DEBUG, "end_wav");
      // ..
    }

    if (skipped_flag) {
      // remove the file
      cde_report(CDE_MSG_TYPE_INFO, "removing aborted file: %s", output_filename);
      //unlink(output_filename);
      // check for cancellation
      if (cde_get_status(cde) == CDE_STATUS_CANCEL) {
        free(output_filename);
        goto cleanup;
      }
      // set the skipped indicator for this track
      cde->disc_info->tracks[current_track-1].t_skipped = 1;
      // make the cursor correct if we have another track
      if (current_track != -1) {
        current_track++;
        cursor = cdda_track_firstsector(cde->drv, current_track);
        paranoia_seek(p, cursor, SEEK_SET);
        //offset_buffer_used = 0;
      }
    }
    free(output_filename);
    cde_report(CDE_MSG_TYPE_INFO, "end of track");
  } // while extracting

  // we are done: set the extraction completed indicator 
  cde->disc_info->d_extracted = 1;

  // extraction done progress callback and status report
  cde_progress_callback(cursor * (CD_FRAMEWORDS)-1, EXTRACT_CB_END_OF_DISC);
  cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: done");

  // cleanup
cleanup:
  if (p) {
    paranoia_free(p);
    p = NULL;
  }

  // return to idle state
  cde_set_status(cde, CDE_STATUS_IDLE);

  // eject disc if audio extraction completed and requested to do so
  if (cde->eject_when_done == CDE_EJECT_WHEN_DONE_ON && cde->disc_info->d_extracted == 1) {
    cde_eject(cde);
  }

  // terminate thread
  pthread_exit(NULL);

  return (void*) (size_t)res;
}

/**
 * @brief extract audio cd
 *        uses a separate thread for extraction
 *        pre: cde_initialize has been called to set the right settings
 * @returns 0 if successful
 */
int cde_extract_audio(cde_state *cde) {
  int res = -1;
  if (cde_get_status(cde) == CDE_STATUS_IDLE) {
    if (cde->virtual_drive == CDE_VIRTUAL_DRIVE_OFF) {
      // actual audio extraction from physical drive
      pthread_create(&cde->thread, NULL, cde_extract_audio_t, (void*)cde);
    } else {
      // 'virtual' audio extraction
      pthread_create(&cde->thread, NULL, cde_extract_audio_v, (void*)cde);
    }
    res = 0;
  }
  return res;
}

/**
 * @brief cancel audio cd extraction
 * @return returns 0 if successful
 */
int cde_cancel_extract(cde_state *cde, int wait) {
  int res = -1;
  pthread_mutex_lock(&state_mutex);
  if (cde->status == CDE_STATUS_EXTRACTING || cde->status == CDE_STATUS_PREPARING) {
    // current extracting audio data or preparing to extract: request cancellation
    cde->status = CDE_STATUS_CANCEL;
    res = 0;
  }
  pthread_mutex_unlock(&state_mutex);
  if (wait) {
    void *thr_return;
    pthread_join(cde->thread, &thr_return);
  }
  return res;
}

/**
 * @brief close drive tray
 * @returns 0 if successful
 */
int cde_close_tray(cde_state *cde) {
  int fd = -1;
  int res = CDE_ERROR_NO_DRIVE;

  if (cde_get_status(cde) == CDE_STATUS_IDLE) {
    if (cde && cde->cdrom_device) {
      cde_report(CDE_MSG_TYPE_DEBUG, "cde_close_tray: %s; state:%d", cde->cdrom_device, cde_get_status(cde));
      if (cde->virtual_drive != CDE_VIRTUAL_DRIVE_ON) {
        // open the drive
        fd = open(cde->cdrom_device, O_RDWR | O_NONBLOCK);
        if (fd<0) {
          fd = open(cde->cdrom_device, O_RDONLY | O_NONBLOCK);
        }
        if (fd < 0) {
          cde_report(CDE_MSG_TYPE_ERROR, "cde_close_tray: unable to open drive");
          res = CDE_ERROR_DRIVE_NOT_OPEN; 
        } else {
          res = ioctl(fd, CDROMCLOSETRAY);
          if (fd >= 0) {
            close(fd);
          }
        }
      } else {
        // we are using a virtual drive
        res = 0;
        cde_report(CDE_MSG_TYPE_DEBUG, "cde_close_tray: close (virtual drive): %d", res);
      }
    } else {
      cde_report(CDE_MSG_TYPE_ERROR, "cde_close_tray: no drive available");
    }
  } else {
    cde_report(CDE_MSG_TYPE_ERROR, "cde_close_tray: cannot close tray unless in idle state");
    res = CDE_ERROR_NOT_IDLE;
  }
  return res;
}

/**
 * @brief eject disc from the drive
 * @returns 0 if successful
 */
int cde_eject(cde_state *cde) {
  int fd = -1;
  int res = CDE_ERROR_NO_DRIVE;
  int version;
	sg_io_hdr_t *sg_io_hdr = calloc(1, sizeof(sg_io_hdr_t));
	unsigned char cmd_data[6] = {ALLOW_MEDIUM_REMOVAL, 0, 0, 0, 0, 0};
	unsigned char inq_buf[2];
	unsigned char sense_buffer[32];

  if (cde_get_status(cde) == CDE_STATUS_IDLE) {

    if (cde && cde->cdrom_device) {
      // close (cdda) connection with drive drive 
      if (cde->drv) {
        cde_close_drive(cde);
      }
      cde_report(CDE_MSG_TYPE_DEBUG, "cde_eject: %s; state:%d", cde->cdrom_device, cde_get_status(cde));
      if (cde->virtual_drive != CDE_VIRTUAL_DRIVE_ON) {
        // open the drive
        fd = open(cde->cdrom_device, O_RDWR | O_NONBLOCK);
        if (fd < 0) {
          fd = open(cde->cdrom_device, O_RDONLY | O_NONBLOCK);
          cde_report(CDE_MSG_TYPE_DEBUG, "cde_eject: opening read only, non blocking");
        }
        if (fd < 0) {
          cde_report(CDE_MSG_TYPE_ERROR, "cde_eject: unable to open drive");
          res = CDE_ERROR_DRIVE_NOT_OPEN; 
        } else {
          res = ioctl(fd, CDROM_LOCKDOOR, 0);
          cde_report(CDE_MSG_TYPE_DEBUG, "cde_eject: lockdoor: %d", res);
          if (res >= 0) {
            res = ioctl(fd, CDROMEJECT);
            cde_report(CDE_MSG_TYPE_DEBUG, "cde_eject: eject: %d", res);
          } else {
            // retry using scsi commands
            if ((ioctl(fd, SG_GET_VERSION_NUM, &version) >= 0) && (version >= 30527)) {

                sg_io_hdr->interface_id = 'S';
                sg_io_hdr->cmd_len = 6;
                sg_io_hdr->mx_sb_len = sizeof(sense_buffer);
                sg_io_hdr->dxfer_direction = SG_DXFER_NONE;
                sg_io_hdr->dxfer_len = 0;
                sg_io_hdr->dxferp = inq_buf;
                sg_io_hdr->sbp = sense_buffer;
                sg_io_hdr->timeout = 15000;
                sg_io_hdr->cmdp = cmd_data;

                // allow media removal and eject
                if ((res = ioctl(fd, SG_IO, (void *)sg_io_hdr)) >= 0 && sg_io_hdr->host_status == 0 && sg_io_hdr->driver_status == 0) {
                  cmd_data[0] = START_STOP;
                  cmd_data[4] = 0x01;
                  if ((res = ioctl(fd, SG_IO, (void *)sg_io_hdr)) >= 0 && sg_io_hdr->host_status == 0 && 
                    (sg_io_hdr->driver_status == 0 || (sg_io_hdr->driver_status == 0x08 && sg_io_hdr->sbp && sg_io_hdr->sbp[12] == 0x3a))) {
                    // ignore medium not present
                    cmd_data[4] = 0x02;
                    if ((res = ioctl(fd, SG_IO, (void *)sg_io_hdr)) >= 0 && sg_io_hdr->host_status && sg_io_hdr->driver_status) {
                      // all ok, force rereading partition table when disc is inserted
                      res = ioctl(fd, _IO(0x12,95));
                    }
                  }
                }
                cde_report(CDE_MSG_TYPE_DEBUG, "cde_eject: scsi driver version:%d state:%d", version, res);
              } 
          }
          if (fd >= 0) {
            close(fd);
          }
        }
      } else {
        // we are using a virtual drive
        res = 0;
        cde_report(CDE_MSG_TYPE_DEBUG, "cde_eject: eject (virtual drive): %d", res);
      }
      // clean disc information
      if (cde->disc_info) {
        cde_free_disc(&cde->disc_info, -1);
      }
      if (cde->folder) {
        free(cde->folder);
        cde->folder = NULL;
      }
    } else {
      cde_report(CDE_MSG_TYPE_ERROR, "cde_eject: no drive available");
      res = CDE_ERROR_NO_DRIVE;
    }
  } else {
    cde_report(CDE_MSG_TYPE_ERROR, "cde_eject: cannot get eject disc unless in idle state");
    res = CDE_ERROR_NOT_IDLE;
  }

  free(sg_io_hdr);
  return res;
}

/**
 * @brief writes a cue sheet from the gathered disc information to a file
 * PRE: cde_state contains a prepared disc information structure
 */
int cde_write_cue_sheet(cde_state *cde, int overwrite) {
  return write_cue_sheet(cde->disc_info, cde->audio_folder, overwrite, cde->verbose);
}

/**
 * @brief parses a cue sheet and stores the results in the disc information structure
 * PRE: cde_state state initialized and we must be using a 'virtual' cdrom drive
 */
int cde_parse_cue_sheet(cde_state *cde, const char *cue, bool download_disc_info) {
  if (cde == NULL) {
    return CDE_ERROR_NO_DRIVE;
  }
  if (cde->virtual_drive != CDE_VIRTUAL_DRIVE_ON) {
    return CDE_ERROR_NO_VIRTUAL_DRIVE;
  }
  if (cde->drv == NULL) {
    cde_get_drive(cde);
  }

  // allocate a disc information structure for holding the track information
  if (cde->disc_info != NULL) {
    cde_free_disc(&(cde->disc_info), -1);
  }
  cde->disc_info = cde_alloc_disc(CDE_MAX_TRACKS);
  
  int result = parse_cue_sheet(cde->disc_info, cue, cde->verbose);
  if (result != CDE_OK) {
    cde_free_disc(&(cde->disc_info), CDE_MAX_TRACKS);
    return result;
  } 

  if (cde->verbose) {
    cde_report(CDE_MSG_TYPE_DEBUG, "disc info from cue sheet:");
    cde_display_disc_info(cde->disc_info);
  }
  
  // set tracks available on the 'virtual' device
  cde->drv->tracks = cde->disc_info->d_tracks;
  

  if (download_disc_info) {
    // see if we can improve/complete the disc information

    // try to read the entry from cddb to extract the correct frame information
    // the extended version parses the comments containing the track frames as well
    result = cddb_read(cde->disc_info, cde->drv, CDDB_REMOTE_ENDPOINT, cde->verbose);

    if (cde->verbose) {
      cde_report(CDE_MSG_TYPE_DEBUG, "cddb_read result: %d", result);
      cde_report(CDE_MSG_TYPE_DEBUG, "disc info from cddb_read:");
      cde_display_disc_info(cde->disc_info);

      for (int i=0; i<=cde->drv->tracks; i++) {
        cde_report(CDE_MSG_TYPE_DEBUG, "---- %2d: %d", i, cde->drv->disc_toc[i].dwStartSector);
      }
    }
  }

  // free unused tracks
  for (int i = cde->disc_info->d_tracks; i < CDE_MAX_TRACKS; i++) {                             
    free(cde->disc_info->tracks[i].t_title);              
    free(cde->disc_info->tracks[i].t_artist);
    free(cde->disc_info->tracks[i].t_album);		
    free(cde->disc_info->tracks[i].t_genre);	
    free(cde->disc_info->tracks[i].t_extended);
    free(cde->disc_info->tracks[i].t_filename);
  }

  return result;
}

/**
 * @brief writes a cddb entry from the gathered disc information to a file in xmcd format
 *        pre: cde_state contains a prepared disc information structure
 */
int cde_cddb_write_entry(cde_state *cde, int overwrite) {
  return cddb_write_entry(cde->disc_info, cde->audio_folder, overwrite, cde->verbose);
}
