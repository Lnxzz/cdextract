/**************************************************************************

  cdextract - folder scanning functions

  Copyright (C) 2021-2025 E. Heerschop (github@heerschop.frl)

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

#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include "libcdextract.h"
#include "libcdextract_types.h"
#include "string_utils.h"
#include "file_utils.h"
#include "json_file.h"
#include "wav_reader.h"
#include "flac_reader.h"
#include "timer.h"
#include "cue_sheet.h"
#include "cddb.h"
#include "mb_api.h"
#include "report.h"
#include "scan.h"


#define CLOCKS_PER_MSEC (CLOCKS_PER_SEC/1000)


/**
 * @brief ensure the specified period passes to rate limit calls
 */
void rate_limit(clock_t *last_call, int min_delay_ms) {
  clock_t now = clock();
  long delay_ms = min_delay_ms - ((now - *last_call) / CLOCKS_PER_MSEC);
  if (delay_ms > 0) {
    msleep(delay_ms);
  }
  *last_call = now;
}

/**
 * @brief attempt to complete disc information in dest_disc
 *        using alt_disc and the cddb information we have in the database
 * @param dest_disc destination disc information to complete
 * @param alt_disc reference disc information used to complete dest_disc (optional: use NULL if not present)
 * @param db the database to store the found disc info
 * @param last_call last call to musicbrainz api used for rate limiting
 * @param verbose print detailed output
 */
int complete_disc_info(disc *dest_disc, disc *alt_disc, sql_db *db, clock_t *last_call, int verbose) {

  // try to add missing information from alt_disc when available
  if (alt_disc != NULL) {
    if (dest_disc->d_lookup == 0) {
      dest_disc->d_lookup = alt_disc->d_lookup;
    }
    if (dest_disc->d_id == 0) {
      dest_disc->d_id = alt_disc->d_id;
    }
    if (dest_disc->d_length == 0) {
      dest_disc->d_length = alt_disc->d_length;
    }
    if ((strlen(dest_disc->d_artist) == 0 || strcmp(dest_disc->d_artist, CDE_UNKNOWN_ARTIST) == 0) && strlen(alt_disc->d_artist) > 0) {
      set_string(&dest_disc->d_artist, alt_disc->d_artist);
    }
    if ((strlen(dest_disc->d_title) == 0 || strcmp(dest_disc->d_title, CDE_UNKNOWN_ALBUM) == 0) && strlen(alt_disc->d_title) > 0) {
      set_string(&dest_disc->d_title, alt_disc->d_title);
    }
    if ((strlen(dest_disc->d_genre) == 0 || strcmp(dest_disc->d_genre, CDE_UNKNOWN_GENRE) == 0) && strlen(alt_disc->d_genre) > 0) {
      set_string(&dest_disc->d_genre, alt_disc->d_genre);
    }
    if (dest_disc->d_year == 0) {
      dest_disc->d_year = alt_disc->d_year;
    }
    if (strlen(dest_disc->d_extended) == 0 && strlen(alt_disc->d_extended) > 0) {
      set_string(&dest_disc->d_extended, alt_disc->d_extended);
    }
    if (dest_disc->mb_front_cover_size == 0 && alt_disc->mb_front_cover_size > 0) {
      set_binary(&dest_disc->mb_front_cover, alt_disc->mb_front_cover, alt_disc->mb_front_cover_size);
      dest_disc->mb_front_cover_size = alt_disc->mb_front_cover_size;
    }
    if (dest_disc->mb_back_cover_size == 0 && alt_disc->mb_back_cover_size > 0) {
      set_binary(&dest_disc->mb_back_cover, alt_disc->mb_back_cover, alt_disc->mb_back_cover_size);
      dest_disc->mb_back_cover_size = alt_disc->mb_back_cover_size;
    }
    if (dest_disc->d_tracks != alt_disc->d_tracks) {
      if (dest_disc->d_tracks == 0) {    
        dest_disc->d_tracks = alt_disc->d_tracks;
        cde_report(CDE_MSG_TYPE_WARNING, "track count on destination disc information set to: %d", dest_disc->d_tracks);
      } else {
        cde_report(CDE_MSG_TYPE_WARNING, "track count mismatch: track count on destination disc information: %d; from file disc information: %d", dest_disc->d_tracks, alt_disc->d_tracks);
      }
    }
    for (int i=0; i < dest_disc->d_tracks && i < CDE_MAX_TRACKS; i++) {
      if (dest_disc->tracks[i].t_num == 0) {
        if (alt_disc->tracks[i].t_num > 0) {
          dest_disc->tracks[i].t_num = alt_disc->tracks[i].t_num;
        } else {
          // default: set track num to index starting with 1
          dest_disc->tracks[i].t_num = i + 1;
        }
      }
      if (dest_disc->tracks[i].t_length == 0) {
        dest_disc->tracks[i].t_length = alt_disc->tracks[i].t_length;
      }
      if (strlen(dest_disc->tracks[i].t_artist) == 0 && strlen(alt_disc->tracks[i].t_artist) > 0) {
        set_string(&dest_disc->tracks[i].t_artist, alt_disc->tracks[i].t_artist);
      }
      if (strlen(dest_disc->tracks[i].t_title) == 0 && strlen(alt_disc->tracks[i].t_title) > 0) {
        set_string(&dest_disc->tracks[i].t_title, alt_disc->tracks[i].t_title);
      }
      if (strlen(dest_disc->tracks[i].t_genre) == 0 && strlen(alt_disc->tracks[i].t_genre) > 0) {
        set_string(&dest_disc->tracks[i].t_genre, alt_disc->tracks[i].t_genre);
      }
      if (dest_disc->tracks[i].t_year == 0) {
        dest_disc->tracks[i].t_year = alt_disc->tracks[i].t_year;
      }
      if (strlen(dest_disc->tracks[i].t_extended) == 0 && strlen(alt_disc->tracks[i].t_extended) > 0) {
        set_string(&dest_disc->tracks[i].t_extended, alt_disc->tracks[i].t_extended);
      }
      if (strlen(alt_disc->tracks[i].t_filename) > 0) {
        // alwsays set the filename from the alt_disc, as it contain the current actual path
        set_string(&dest_disc->tracks[i].t_filename, alt_disc->tracks[i].t_filename);
      }
      if (dest_disc->tracks[i].t_skipped == 0) {
        dest_disc->tracks[i].t_skipped = alt_disc->tracks[i].t_skipped;
      }
    }
    // note: cddb/musicbrainz already present or can only be reconstructed 
    //       after getting cddb information from the database
  }

  //try to get the disc information from the local database
  if (verbose) {
    cde_report(CDE_MSG_TYPE_DEBUG, "complete_disc_info: calling search_cddb_entry_in_database: search cddb entry with: disc id: %08x; lookup: %lu; cddb_d_id: %08x; cddb_e_id: %08x; tracks: %d; artist: %s; title: %s", dest_disc->d_id, dest_disc->d_lookup, dest_disc->cddb_d_id, dest_disc->cddb_e_id, dest_disc->d_tracks, dest_disc->d_artist, dest_disc->d_title);
  }
  int res = 0;
  long cddb_id = 0;
  disc *cddb_info = NULL;
  if ((res = search_cddb_entry_in_database(db, dest_disc, &cddb_id, &cddb_info)) == CDE_OK) {
    if (cddb_info != NULL) {
      cddb_reconstruct_query_strings(cddb_info, NULL, verbose);

      if (verbose) {
        cde_report(CDE_MSG_TYPE_DEBUG, "cddb entry %ld found: %s - %s", cddb_id, cddb_info->d_artist, cddb_info->d_title);
        cde_report(CDE_MSG_TYPE_DEBUG, "cue sheet has the following query strings:   [%s] [%s]", dest_disc->cddb_query, dest_disc->mb_fuzzy_lookup);
        cde_report(CDE_MSG_TYPE_DEBUG, "cddb entry with reconstructed query strings: [%s] [%s]", cddb_info->cddb_query, cddb_info->mb_fuzzy_lookup);
      }
      
      // copy information from cddb result to the destination disc information
      if (dest_disc->d_id == 0) {
        if (cddb_info->cddb_d_id > 0) {
          dest_disc->d_id = cddb_info->cddb_d_id; // set the cddb disc id as disc id
        } else {
          dest_disc->d_id = cddb_info->cddb_e_id; // set the cddb entry id as disc id
        }
      }
      dest_disc->d_length = cddb_info->d_length;
      set_string(&dest_disc->d_artist, cddb_info->d_artist);
      set_string(&dest_disc->d_title, cddb_info->d_title);
      set_string(&dest_disc->d_genre, cddb_info->d_genre);
      if (cddb_info->d_year > 0) {
        dest_disc->d_year = cddb_info->d_year;
      }
      set_string(&dest_disc->cddb_query, cddb_info->cddb_query);
      set_string(&dest_disc->cddb_category, cddb_info->cddb_category);
      dest_disc->cddb_e_id = cddb_info->cddb_e_id;
      dest_disc->cddb_d_id = cddb_info->cddb_d_id;
      dest_disc->cddb_revision = cddb_info->cddb_revision;
      dest_disc->cddb_complete = cddb_info->cddb_complete;

      if (strlen(dest_disc->mb_query) == 0 && strlen(cddb_info->mb_query) > 0) {
        set_string(&dest_disc->mb_query, cddb_info->mb_query);
      }

      if (strlen(dest_disc->mb_fuzzy_lookup) == 0 && strlen(cddb_info->mb_fuzzy_lookup) > 0) {
        set_string(&dest_disc->mb_fuzzy_lookup, cddb_info->mb_fuzzy_lookup);
      }

      if (strlen(dest_disc->mb_disc_id) == 0 && strlen(cddb_info->mb_disc_id) > 0) {
        set_string(&dest_disc->mb_disc_id, cddb_info->mb_disc_id);
      }

      if (strlen(dest_disc->mb_release_id) == 0 && strlen(cddb_info->mb_release_id) > 0) {
        set_string(&dest_disc->mb_release_id, cddb_info->mb_release_id);
      }

      // copy track information
      for (int i=0; i < cddb_info->d_tracks; i++) {
        if (strlen(cddb_info->tracks[i].t_title) > 0) {
          set_string(&(dest_disc->tracks[i].t_title), cddb_info->tracks[i].t_title);
        }
        if (strlen(cddb_info->tracks[i].t_artist) > 0) {
          set_string(&(dest_disc->tracks[i].t_artist), cddb_info->tracks[i].t_artist);
        } else if (strlen(cddb_info->d_artist) > 0) {
          set_string(&(dest_disc->tracks[i].t_artist), cddb_info->d_artist);
        }
        if (strlen(cddb_info->tracks[i].t_album) > 0) {
          set_string(&(dest_disc->tracks[i].t_album), cddb_info->tracks[i].t_album);
        } else if (strlen(cddb_info->d_title) > 0) {
          set_string(&(dest_disc->tracks[i].t_album), cddb_info->d_title);
        }
        if (strlen(cddb_info->tracks[i].t_genre) > 0) {
          set_string(&(dest_disc->tracks[i].t_genre), cddb_info->tracks[i].t_genre);
        } else if (strlen(cddb_info->d_genre) > 0) {
          set_string(&(dest_disc->tracks[i].t_genre), cddb_info->d_genre);
        }
        if (cddb_info->tracks[i].t_year > 0) {
          dest_disc->tracks[i].t_year = cddb_info->tracks[i].t_year;
        } else if (cddb_info->d_year > 0) {
          dest_disc->tracks[i].t_year = cddb_info->d_year;
        }
        if (dest_disc->tracks[i].t_length == 0) {
          dest_disc->tracks[i].t_length = cddb_info->tracks[i].t_length;
        }
      }

      // try to get the disc information from the online cddb service
      //  if (cddb_query(cue_disc, CDDB_REMOTE_ENDPOINT, verbose) == CDE_OK &&
      //      cddb_read(cue_disc, NULL, CDDB_REMOTE_ENDPOINT, verbose) == CDE_OK) {
      // --> will call cddb_parse_data

      if (strlen(dest_disc->mb_release_id) > 0) {
        // musicbrainz release id available to download the cover images(s) if needed
        res = CDE_OK;
      } else {
        
        if (strlen(dest_disc->mb_query) > 0) {
          // rate limit to one call per second
          rate_limit(last_call, 1000);
          // try to get the musicbrainz release id by using a disc information query
          if ((res = mb_get_disc_info(dest_disc, MB_QUERY_DISCID, verbose)) == CDE_OK) {
            cde_report(CDE_MSG_TYPE_DEBUG, "successfully retrieved musicbrainz disc info using a disc information query");          
          }
        }
        
        if (res != CDE_OK && strlen(dest_disc->mb_fuzzy_lookup) > 0) {
          // rate limit to one call per 1,2 seconds
          rate_limit(last_call, 1200);
          // try to get the musicbrainz release id by using a fuzzy lookup
          if ((res = mb_get_disc_info(dest_disc, MB_QUERY_FUZZY, verbose)) == CDE_OK) {
            cde_report(CDE_MSG_TYPE_DEBUG, "successfully retrieved musicbrainz disc info using a fuzzy lookup");          
          }
        }

        if (res != CDE_OK && strlen(dest_disc->d_artist) > 0 && strlen(dest_disc->d_title) > 0 && dest_disc->d_year > 0 && dest_disc->d_tracks > 0) {
          // rate limit to one call per 1,5 seconds
          rate_limit(last_call, 1500);
          // try to get the musicbrainz release id by using a query based on title and artist
          if ((res = mb_get_disc_info(dest_disc, MB_QUERY_RELEASE_FULL, verbose)) == CDE_OK) {
            cde_report(CDE_MSG_TYPE_DEBUG, "successfully retrieved musicbrainz disc info using a query based on artist, release title, track count, release year and CD format");          
          }        
        }

        if (res != CDE_OK && strlen(dest_disc->d_artist) > 0 && strlen(dest_disc->d_title) > 0 && dest_disc->d_tracks > 0) {
          // rate limit to one call per 1,5 seconds
          rate_limit(last_call, 1500);
          // try to get the musicbrainz release id by using a query based on title and artist
          if ((res = mb_get_disc_info(dest_disc, MB_QUERY_RELEASE_PARTIAL, verbose)) == CDE_OK) {
            cde_report(CDE_MSG_TYPE_DEBUG, "successfully retrieved musicbrainz disc info using a query based on artist, release title, track count and CD format");          
          }      
        }

        if (res != CDE_OK && strlen(dest_disc->d_artist) > 0 && strlen(dest_disc->d_title) > 0) {
          // rate limit to one call per 1,5 seconds
          rate_limit(last_call, 1500);
          // try to get the musicbrainz release id by using a query based on title and artist
          if ((res = mb_get_disc_info(dest_disc, MB_QUERY_RELEASE_LIMITED, verbose)) == CDE_OK) {
            cde_report(CDE_MSG_TYPE_DEBUG, "successfully retrieved musicbrainz disc info using a query based on artist, release title and CD format");          
          }      
        }

        if (res != CDE_OK) {
          res = CDE_ERROR_MB_DATA;
          cde_report(CDE_MSG_TYPE_WARNING, "no musicbrainz disc information available using disc information, fuzzy lookup or release information query");
        }
      }

      // free allocated cddb disc information
      cde_free_disc(&cddb_info, -1);
    } else {
      res = CDE_ERROR_CDDB_DATA;
      cde_report(CDE_MSG_TYPE_DEBUG, "search for cddb entry in database returned no cddb entry");
    }
  } else if (res == DB_NO_RESULT) {
    cde_report(CDE_MSG_TYPE_DEBUG, "search for cddb entry in database returned no results");
  } else {
    cde_report(CDE_MSG_TYPE_ERROR, "search for cddb entry in database failed with error: %d", res);
  }

  if (verbose) {
    //cde_display_disc_info(dest_disc);
    cde_report(CDE_MSG_TYPE_DEBUG, "completed disc info processing for disc: %s - %s; result: %d", dest_disc->d_artist, dest_disc->d_title, res);
  }
  return res;
}

/**
 * @brief get the front and back covers from file or try to download and store them when requested
 * @param disc_info disc information structure
 * @param folder the output folder to read and store cover images
 * @param download_coverart download disc cover art
* @param verbose print detailed output
 * @return 0 if successful
 */
int get_coverart(disc* disc_info, const char *folder, int download_coverart, int verbose) {
  int res = CDE_OK;

  if (disc_info->mb_front_cover_size > 0 && disc_info->mb_front_cover != NULL) {
    // cover already available within the disc structure
    return res;
  }

  // try to get front cover
  struct stat st = {0};
  char *front_cover_file = calloc(strlen(folder)+strlen(CDE_COVER_FRONT)+2, sizeof(char));
  sprintf(front_cover_file, "%s/%s", folder, CDE_COVER_FRONT);
  if (stat(front_cover_file, &st) == 0) {
    // file available: try to load the front cover from file
    cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder: loading front cover");
    disc_info->mb_front_cover_size = read_file(&disc_info->mb_front_cover, front_cover_file);
  } 
  if (disc_info->mb_front_cover_size <= 0) {
    if (download_coverart) {
      if (strlen(disc_info->mb_release_id) > 0) {
        // front cover not loaded from file: try to download front cover from the online coverart service
        cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder: get front cover for release id: %s", disc_info->mb_release_id);
        // note: we are only downloading the cover art to memory in this step with MB_COVERART_MEM_ONLY
        //       Covers are only written to file as part of the audio extraction process
        int r = mb_caa_get_front_cover(disc_info, download_coverart, folder, verbose);
        disc_info->mb_complete = (r == 0 ? 1 : 0);
        res &= r;
      } else {
        cde_report(CDE_MSG_TYPE_WARNING, "scan audio folder: unable to download cover: mb release id unavailable");
      }
    } else {
      // do not download coverart: we are done now
      cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder: skipping cover art");
      disc_info->mb_complete = 1;      
    }
  }
  free(front_cover_file);

  // try to get back cover
  char *back_cover_file = calloc(strlen(folder)+strlen(CDE_COVER_BACK)+2, sizeof(char));
  sprintf(back_cover_file, "%s/%s", folder, CDE_COVER_BACK);
  if (stat(back_cover_file, &st) == 0) {
    // file available: try to load the back cover from file
    cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder: loading back cover");
    disc_info->mb_back_cover_size = read_file(&disc_info->mb_back_cover, back_cover_file);
  }
  if (disc_info->mb_back_cover_size <= 0) {
    if (download_coverart) {
      if (strlen(disc_info->mb_release_id) > 0) {
        // back cover not loaded from file: try to download back cover from the online coverart service
        cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder: get back cover for release id: %s", disc_info->mb_release_id);
        // note: we are only downloading the cover art to memory in this step with MB_COVERART_MEM_ONLY
        //       Covers are only written to file as part of the audio extraction process
        int r = mb_caa_get_back_cover(disc_info, download_coverart, folder, verbose);
        res &= r;
      }
    }
  }
  free(back_cover_file);

  return res;
}

/**
 * @brief read a m3u file
 * @param m3u_file the m3u file
 */
void process_m3u_file(char *m3u_file) {
  // note: not implemented
  cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder: processed m3u file: %s", m3u_file);
}

/**
 * @brief read a cddb file and store the cddb entry in the database
 * @param cddb_file the file containing the cddb entry
 */
void process_cddb_file(char *cddb_file, sql_db *db) {
  char* cddb_data = NULL;
  if (read_file(&cddb_data, cddb_file) > 0) {
    // try to parse the cddb data
    disc *disc_info = cde_alloc_disc(CDE_MAX_TRACKS);
    if (disc_info != NULL) {
      // parse the cddb data
      int res = cddb_parse_data(disc_info, NULL, cddb_data, 0, 1, 0);
      if (res == CDE_OK) {
        // set the 64-bit internal hash to identify and lookup the disc
        cde_set_hash(disc_info);
        // get the cddb category id
        int category_id = get_category_id(db, disc_info->cddb_category);
        if (category_id > 0 && disc_info->d_lookup > 0) {
          // store the disc information from the cddb data in the database
          int sres = store_cddb_entry_in_database(db, disc_info, category_id, DB_DUPLICATE_CHECK_LOOKUP);
          if (sres == DB_OK) {
            cde_report(CDE_MSG_TYPE_INFO, "scan audio folder: stored cddb data: %s", cddb_file);
          } else if (sres == DB_DUPLICATE) {
            cde_report(CDE_MSG_TYPE_INFO, "scan audio folder: cddb data already stored: %s", cddb_file);
          } else {
            cde_report(CDE_MSG_TYPE_ERROR, "scan audio folder: unable to store disc information from cddb data: %s (%d) %s", cddb_file, db->status, db->msg);
          }
        } else {
          cde_report(CDE_MSG_TYPE_ERROR, "scan audio folder: unable to store disc information from cddb data; no or invalid category id: %s", cddb_file);
        }
      } else if (res == CDE_ERROR_CDDB_ENCODING) {
        // encoding error
        cde_report(CDE_MSG_TYPE_ERROR, "scan audio folder: cddb entry has invalid encoding: %s", cddb_file);
      } else {
        // parse error
        cde_report(CDE_MSG_TYPE_ERROR, "scan audio folder: unable to parse cddb data: %s", cddb_file);
      }
      // free the disc information with the number of tracks we allocated memory for
      cde_free_disc(&disc_info, CDE_MAX_TRACKS);
    }
    if (cddb_data != NULL) {
      free(cddb_data);
    }
    cde_report(CDE_MSG_TYPE_INFO, "scan audio folder: processed cddb data: %s", cddb_file);
  } else {
    cde_report(CDE_MSG_TYPE_ERROR, "scan audio folder: unable to read cddb data: %s", cddb_file);
  }
}

 /**
 * @brief traverse the audio folder and scan for json disc info, audio files and cue sheets
 * @param audio_folder the audio folder to scan
 * @param db the database to store the found disc info
 * @param download_coverart download disc cover art
 * @param write_json write disc information to a json file
 * @param verbose print detailed output
 */
void scan_audio_folder(char *audio_folder, sql_db *db, int download_coverart, int write_json, int verbose) {
  FTS *ftsp;
  FTSENT *p;
  FTSENT *chp;
  int fts_options = FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOCHDIR;
  char *folders[] = {audio_folder, NULL};

  long folder_count = 0;              // total number of scanned audio folders
  long audio_files_count = 0;         // total number of audio files
  long stored_disc_count = 0;         // total number of stored discs
  long already_stored_disc_count = 0; // total number of already stored discs
  long discarded_disc_count = 0;      // total number of discarded discs
  long ignored_path_count = 0;        // total number of ignored paths

  clock_t last_call = 0;              // last api call (used for rate limiting)
  clock_t clock_start = clock();      // start of scanning process

  if ((ftsp = fts_open(folders, fts_options, NULL)) == NULL) {
    cde_report(CDE_MSG_TYPE_ERROR, "scan audio folder: unable to open folder: %s", audio_folder);
    return;
  }

  if ((p = fts_read(ftsp)) == NULL) {
    fts_close(ftsp);
    cde_report(CDE_MSG_TYPE_ERROR, "scan audio folder: unable to read from folder: %s", audio_folder);
    return;
  }
  
  // get linked list of structures which describe the files contained in the directory.
  chp = fts_children(ftsp, 0);
  if (chp == NULL) {
    // no files in folder
    return;               
  }
  
  cde_report(CDE_MSG_TYPE_INFO, "scan audio folder: path: %s; level: %d", chp->fts_path, chp->fts_level);

  int json_info = 0;      // 0=not found, 1=found, 2=processed
  int cue_sheet = 0;      // 0=not found, 1=found, 2=processed, 3=processed custom cdextract cuesheet
  int audio_info = 0;     // 0=not found, 1=found, 2=processed
  int audio_files = 0;    // number of audio files found for the disc
  int image_files = 0;    // number of image files found for the disc
  int front_cover = 0;    // 0=not found, 1=found
  int back_cover = 0;     // 0=not found, 1=found
  int cddb_files = 0;     // 0=not found, >=1=found
  int m3u_files = 0;      // 0=not found, >=1=found
  int disc_level = 0;     // folder level of the disc information

  disc *json_disc = NULL; // disc information from json file
  disc *cue_disc = NULL;  // disc information from cue sheet
  disc *file_disc = NULL; // disc information from audio files

  char *folder_artist = calloc(1, sizeof(char));  // artist name from folder
  char *folder_album = calloc(1, sizeof(char));   // album name from folder
  char *cddb_filename = calloc(1, sizeof(char));  // cddb filename and path
  char *m3u_filename = calloc(1, sizeof(char));   // m3u filename and path

  // scan the folder for files
  while ((p = fts_read(ftsp)) != NULL) {
    switch (p->fts_info) {
    case FTS_D:
      // entering path
      if (p->fts_level == 1 ) {
        // get artist from folder
        set_string(&folder_artist, p->fts_name);
        cde_report(CDE_MSG_TYPE_INFO, "scan audio folder: entering path: %s; artist: %s; level: %d", p->fts_path, folder_artist, p->fts_level);
      } else if (p->fts_level > 1 && disc_level == 0) {
        // get album from folder
        set_string(&folder_album, p->fts_name);
        cde_report(CDE_MSG_TYPE_INFO, "scan audio folder: entering path: %s; artist: %s; album: %s; level: %d", p->fts_path, folder_artist, folder_album, p->fts_level);
      } else {
        cde_report(CDE_MSG_TYPE_INFO, "scan audio folder: entering path: %s; level: %d; disc info level: %d", p->fts_path, p->fts_level, disc_level);
      }
      break;
    case FTS_DP:
      // leaving path: store the disc information from the cuesheet in the database if found and no json info is available
      cde_report(CDE_MSG_TYPE_INFO, "scan audio folder: leaving path: %s; level: %d", p->fts_path, p->fts_level);
      if (p->fts_level == disc_level) {
        // leaving album path: store disc information if found
        if (cddb_files > 0) {
          // read found cddb file and store entry if possibke
          process_cddb_file(cddb_filename, db);
        }
        if (json_info == 2) {
          // parsed json file containing all album information
          if (file_disc != NULL && json_disc->d_tracks == file_disc->d_tracks && json_disc->d_tracks < CDE_MAX_TRACKS) {
            // always set the filenames from the found audio files, as they contain the current actual paths
            for (int i=0; i < json_disc->d_tracks; i++) {
              if (strlen(file_disc->tracks[i].t_filename) > 0) {
                set_string(&json_disc->tracks[i].t_filename, file_disc->tracks[i].t_filename);
              }
            }
          }
          int res = store_disc_in_database(db, json_disc, 0);
          if (res == DB_OK) {
            stored_disc_count++;
            cde_report(CDE_MSG_TYPE_INFO, "scan audio folder: stored disc information: %s; level: %d", p->fts_path, p->fts_level);
          } else if (res > DB_OK) {
            discarded_disc_count++;
            cde_report(CDE_MSG_TYPE_ERROR, "scan audio folder: unable to store disc information: (%d) %s (%d) %s", p->fts_level, p->fts_path, db->status, db->msg);
          } else { // res < DB_OK
            already_stored_disc_count++;
            if (verbose) {
              cde_report(CDE_MSG_TYPE_INFO, "scan audio folder: disc information already stored: %s", p->fts_path);
            }
          }
        } else if (cue_sheet == 3) {
          // parsed custom cdextract cuesheet containing all album information
          if (file_disc != NULL  && cue_disc->d_tracks == file_disc->d_tracks && cue_disc->d_tracks < CDE_MAX_TRACKS) {
            // always set the filenames from the found audio files, as they contain the current actual paths
            for (int i=0; i < cue_disc->d_tracks; i++) {
              if (strlen(file_disc->tracks[i].t_filename) > 0) {
                set_string(&cue_disc->tracks[i].t_filename, file_disc->tracks[i].t_filename);
              }
            }
          }
          int res = store_disc_in_database(db, cue_disc, 0);
          if (res == DB_OK) {
            stored_disc_count++;
            cde_report(CDE_MSG_TYPE_INFO, "scan audio folder: stored disc information from cdextract cue sheet: %s; level: %d", p->fts_path, p->fts_level);
          } else if (res > DB_OK) {
            discarded_disc_count++;
            cde_report(CDE_MSG_TYPE_ERROR, "scan audio folder: unable to store disc information from cdextract cue sheet: (%d) %s (%d) %s", p->fts_level, p->fts_path, db->status, db->msg);
          } else { // res < DB_OK
            already_stored_disc_count++;
            if (verbose) {
              cde_report(CDE_MSG_TYPE_INFO, "scan audio folder: not storing disc information from cdextract cuesheet; disc information already stored: %s", p->fts_path);
            }
          }
          // write the gathered disc information to a json file if requested (assuming the information is complete)
          if (write_json == 1) {
            cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder: writing disc information using folder %s", p->fts_path);
            json_write_disc_info(cue_disc, p->fts_path, 0, verbose);
          }
        } else if (cue_sheet == 2 && cue_disc != NULL && cue_disc->d_tracks > 0) {
          // parsed cuesheet which we will use as basis to enrich with m3u / cddb / musicbrainz information
          if (m3u_files > 0) {
            process_m3u_file(m3u_filename);
          }
          if (cue_disc->d_tracks <= CDE_MAX_TRACKS && complete_disc_info(cue_disc, file_disc, db, &last_call, verbose) == CDE_OK) {
            if (get_coverart(cue_disc, p->fts_path, download_coverart, verbose) == CDE_OK) {
              // store the disc information in the database
              int res = store_disc_in_database(db, cue_disc, 0);
              if (res == DB_OK) {
                stored_disc_count++;
                cde_report(CDE_MSG_TYPE_INFO, "scan audio folder: stored disc information from cue sheet: %s; level: %d", p->fts_path, p->fts_level);
              } else if (res > DB_OK) {
                discarded_disc_count++;
                cde_report(CDE_MSG_TYPE_ERROR, "scan audio folder: unable to store disc information from cue sheet: (%d) %s (%d) %s", p->fts_level, p->fts_path, db->status, db->msg);
              } else { // res < DB_OK
                already_stored_disc_count++;
                if (verbose) {
                  cde_report(CDE_MSG_TYPE_INFO, "scan audio folder: not storing disc information from cuesheet; disc information already stored: %s", p->fts_path);
                }
              }
              // write the gathered disc information to a json file if requested and fully complete
              if (write_json == 1 && cue_disc->cddb_complete == 1 && cue_disc->mb_complete == 1) {
                cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder: writing disc information using folder %s", p->fts_path);
                json_write_disc_info(cue_disc, p->fts_path, 0, verbose);
              }
            } else { 
              cde_report(CDE_MSG_TYPE_WARNING, "scan audio folder: unable to get cover art for disc information from cue sheet: %s; path: %s; level: %d", p->fts_name, p->fts_path, p->fts_level);
            }
          } else {
            discarded_disc_count++;
            cde_report(CDE_MSG_TYPE_ERROR, "scan audio folder: unable to reconstruct disc information from cue sheet: %s; path: %s; level: %d", p->fts_name, p->fts_path, p->fts_level);
          }
        } else if (audio_info == 1 && file_disc != NULL && file_disc->d_tracks > 0) {
          // found audio files which we will use as basis to enrich with m3u / cddb / musicbrainz information
          if (m3u_files > 0) {
            process_m3u_file(m3u_filename);
          }
          if (file_disc->d_tracks <= CDE_MAX_TRACKS && complete_disc_info(file_disc, NULL, db, &last_call, verbose) == CDE_OK) {
            if (get_coverart(file_disc, p->fts_path, download_coverart, verbose) == CDE_OK) {
              // store the disc information in the database
              int res = store_disc_in_database(db, file_disc, 0);
              if (res == DB_OK) {
                stored_disc_count++;
                cde_report(CDE_MSG_TYPE_INFO, "scan audio folder: stored disc information from audio files: %s; level: %d", p->fts_path, p->fts_level);
              } else if (res > DB_OK) {
                discarded_disc_count++;
                cde_report(CDE_MSG_TYPE_ERROR, "scan audio folder: unable to store disc information from audio files: (%d) %s (%d) %s", p->fts_level, p->fts_path, db->status, db->msg);
              } else { // res < DB_OK
                already_stored_disc_count++;
                if (verbose) {
                  cde_report(CDE_MSG_TYPE_INFO, "scan audio folder: not storing disc information from audio files; disc information already stored: %s", p->fts_path);
                }
              }
              // write the gathered disc information to a json file if requested and fully complete
              if (write_json == 1 && file_disc->cddb_complete == 1 && file_disc->mb_complete == 1) {
                cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder: writing disc information using folder %s", p->fts_path);
                json_write_disc_info(file_disc, p->fts_path, 0, verbose);
              }
            } else {
              cde_report(CDE_MSG_TYPE_WARNING, "scan audio folder: unable to get cover art for disc information from audio files: %s; path: %s; level: %d; tracks: %s", p->fts_name, p->fts_path, p->fts_level, file_disc->d_tracks);
            }
          } else {
            discarded_disc_count++;
            cde_report(CDE_MSG_TYPE_ERROR, "scan audio folder: unable to reconstruct disc information from audio files: %s; path: %s; level: %d", p->fts_name, p->fts_path, p->fts_level);
          }
        } else {
          ignored_path_count++;
          cde_report(CDE_MSG_TYPE_INFO, "scan audio folder: no valid disc information found in path: (%d) %s", p->fts_level, p->fts_path);
        }
      
        // leaving album path: reset flags and counters
        if (verbose) {
          cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder: reset flags and counters while leaving path: %s; level: %d; disc level: %d", p->fts_path, p->fts_level, disc_level);
        }
        json_info = 0;
        cue_sheet = 0;
        audio_info = 0;
        audio_files = 0;
        image_files = 0;
        front_cover = 0;
        back_cover = 0;
        cddb_files = 0;
        m3u_files = 0;
        disc_level = 0;
        // cleanup
        if (json_disc != NULL) {
          cde_free_disc(&json_disc, -1);
          json_disc = NULL;
        }
        if (cue_disc != NULL) {
          cde_free_disc(&cue_disc, CDE_MAX_TRACKS);
          cue_disc = NULL;
        }
        if (file_disc != NULL) {
          cde_free_disc(&file_disc, CDE_MAX_TRACKS);
          file_disc = NULL;
        }
        // clear album name
        folder_album[0] = '\0';
        folder_count++;
      }
      if (p->fts_level == 1) {
        // leaving artist path: clear artist name
        folder_artist[0] = '\0';
      }
      if (verbose) {
        cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder: left path: %s; level: %d", p->fts_path, p->fts_level);
      }
      break;
    case FTS_SL:
    case FTS_F:
      if (p->fts_level > 2) {
        if (ends_with(".json", p->fts_name)) {
          cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder: found json disc info: %s; path: %s; level: %d", p->fts_name, p->fts_path, p->fts_level);
          json_info = 1;
          if (json_disc == NULL) {
            json_disc = calloc(1, sizeof(disc));
          }
          if (json_disc != NULL) {
            int res = CDE_OK;
            if ((res = json_read_disc_info(json_disc, p->fts_path, 0)) == CDE_OK) {
              // processed valid json disc info
              json_info = 2;
              // set disc_level
              disc_level = p->fts_level - 1;
            } else {
              // error parsing json disc info: ensure cleanup to prevent mismatch of disc information
              if (json_disc != NULL) {
                cde_free_disc(&json_disc, -1);
                json_disc = NULL;
              }
              cde_report(CDE_MSG_TYPE_ERROR, "scan audio folder: unable to read disc info (%d) from json file: %s", res, p->fts_path);
            }
          }    
        } else if (ends_with(".cue", p->fts_name) && json_info != 2) { 
          cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder: found cue sheet: %s; path: %s; level: %d", p->fts_name, p->fts_path, p->fts_level);
          cue_sheet = 1;
          char *cue_data = NULL;
          if (cue_disc == NULL && read_file(&cue_data, p->fts_path) > 0) {
            // note: only a single cue sheet is processed per folder
            cue_disc = cde_alloc_disc(CDE_MAX_TRACKS);
            cue_disc->d_tracks = 0; // reset number of tracks
            if (parse_cue_sheet(cue_disc, cue_data, verbose) == CDE_OK) {
              if (strlen(cue_disc->cddb_query) > 0 && strlen(cue_disc->mb_query) > 0 && strlen(cue_disc->mb_fuzzy_lookup) > 0) {
                // parsed custom cdextract cuesheet
                cue_sheet = 3;
                // set disc_level
                disc_level = p->fts_level - 1;
              } else {
                // processed valid cue sheet
                cue_sheet = 2;
                // set disc_level if not already done
                if (disc_level == 0) {
                  disc_level = p->fts_level - 1;
                }
              }
            } else {
              // error parsing cue sheet: ensure cleanup to prevent mismatch of disc information
              if (cue_disc != NULL) {
                cde_free_disc(&cue_disc, CDE_MAX_TRACKS);
                cue_disc = NULL;
              }
              cde_report(CDE_MSG_TYPE_ERROR, "scan audio folder: invalid cue sheet: %s", p->fts_path);
            }
          } else {
            cde_report(CDE_MSG_TYPE_WARNING, "scan audio folder: unable to read cue sheet: %s", p->fts_path);
          }
          if (cue_data != NULL) {
            free(cue_data);
          }
        } else if (ends_with(".flac", p->fts_name) || ends_with(".wav", p->fts_name)) {

          audio_files++; // increase number of audio files found for this disc
          audio_files_count++; // increase total number of audio files

          if (file_disc == NULL) {
            audio_info = 1;
            file_disc = cde_alloc_disc(CDE_MAX_TRACKS);
            file_disc->d_tracks = 0; // reset number of tracks

            // set disc category
            set_string(&(file_disc->cddb_category), "data");
            
            // set artist name
            char *artist_name = replace_chars(folder_artist, "_", ' ');
            set_string(&(file_disc->d_artist), artist_name);
            free(artist_name);

            // extract album name from album folder
            int artist_len = strlen(file_disc->d_artist);
            int folder_album_len = strlen(folder_album);
            int pos = 0;
            // skip leading 'metadata' between brackets from the beginning of the album folder name
            while (pos < folder_album_len && (folder_album[pos] == '[' || folder_album[pos] == '(')) {
              // find end bracket
              while (pos < folder_album_len && folder_album[pos] != ']' && folder_album[pos] != ')') {
                pos++;
              }
              pos++;
            }
            // skip leading whitespace and dashes
            while (pos < folder_album_len && (folder_album[pos] == ' ' || folder_album[pos] == '-' || folder_album[pos] == '_')) {
              pos++;
            }
            // skip artist name if present
            int artist_pos = pos;
            while (pos - artist_pos < artist_len && pos < folder_album_len && folder_album[pos] == folder_artist[pos - artist_pos]) {
              pos++;
            }
            if (pos - artist_pos != artist_len) {
              pos = 0;
            }
            // skip leading whitespace and dashes
            while (pos < folder_album_len && (folder_album[pos] == ' ' || folder_album[pos] == '-' || folder_album[pos] == '_')) {
              pos++;
            }
            // remove 'metadata' between brackets from the end of the album folder name
            int end_pos = folder_album_len;
            if (end_pos > pos && (folder_album[end_pos-1] == ']' || folder_album[end_pos-1] == ')')) {
              // find start bracket
              while (end_pos > pos && folder_album[end_pos] != '[' && folder_album[end_pos] != '(') {
                end_pos--;
              }
              if (end_pos <= pos) {
                // no start bracket found: reset endpos
                end_pos = folder_album_len-1;
              } else {
                // try to extract the album year
                int year_pos = end_pos + 1;
                int year_pos_end = year_pos;
                while (folder_album[year_pos_end] >= '0' && folder_album[year_pos_end] <= '9') {
                  year_pos_end++;
                }
                int d_year = 0;
                if (year_pos_end > year_pos) {
                  char *year_str = calloc((year_pos_end - year_pos + 1), sizeof(char));
                  if (year_str != NULL) {
                    strncpy(year_str, &(folder_album[year_pos]), year_pos_end - year_pos);
                    d_year = atoi(year_str);
                    free(year_str);
                    if (d_year > 1560 && d_year < 2100) {
                      file_disc->d_year = d_year;
                    }
                  }
                }
              }
              // skip trailing whitespace
              while (end_pos > pos && (folder_album[end_pos-1] == ' ')) {
                end_pos--;
              }
            }

            // set album name
            char *album_name_raw = calloc(end_pos - pos + 1, sizeof(char));
            strncpy(album_name_raw, &folder_album[pos], end_pos - pos);
            char *album_name = replace_chars(album_name_raw, "_", ' ');
            set_string(&(file_disc->d_title), album_name);

            free(album_name_raw);
            free(album_name);

            // initialize total audio length of the disc
            file_disc->d_length = 0;
          }

          // get length of filename to extract track number and title
          int fts_name_len = strlen(p->fts_name);
          // find file suffix location
          int fts_end = fts_name_len;
          while (fts_end > 0 && p->fts_name[fts_end] != '.') {
            --fts_end;
          }
          // get the track number 
          int fts_start = 0;
          // first skip leading characters until we find a digit
          while (fts_start < fts_end && (p->fts_name[fts_start] < '0' || p->fts_name[fts_start] > '9')) {
            fts_start++;
          }
          int fts_pos = 0;
          if (fts_start < fts_end) {
            // set fts_pos to the start of the track number
            fts_pos = fts_start;
          } else {
            fts_start = 0;
          }
          // try to get actual track number
          while (p->fts_name[fts_pos] >= '0' && p->fts_name[fts_pos] <= '9') {
            fts_pos++;
          }
          int tnum = 0;
          if (fts_pos > 0) {
            char *tnum_str = calloc((fts_pos - fts_start + 1), sizeof(char));
            if (tnum_str != NULL) {
              strncpy(tnum_str, &(p->fts_name[fts_start]), fts_pos - fts_start);
              tnum = atoi(tnum_str);
              free(tnum_str);
            }
          }
          if (tnum > 0 && tnum <= CDE_MAX_TRACKS) {
            // set track number
            file_disc->tracks[tnum-1].t_num = tnum;
            // set number of tracks
            if (tnum > file_disc->d_tracks) {
              file_disc->d_tracks = tnum;
            }
            // skip whitespace, dots and dashes
            while (p->fts_name[fts_pos] == ' ' || p->fts_name[fts_pos] == '.' || p->fts_name[fts_pos] == '-') {
              ++fts_pos;
            }
            // copy remaining part of filename as track title
            char *t_title_raw =  malloc((fts_end - fts_pos + 1) * sizeof(char));
            strncpy(t_title_raw, &(p->fts_name[fts_pos]), fts_end - fts_pos);
            t_title_raw[fts_end - fts_pos] = '\0';
            char *t_title = replace_chars(t_title_raw, "_", ' ');
            set_string(&(file_disc->tracks[tnum-1].t_title), t_title);
            free(t_title);
            free(t_title_raw);

            // set artist name of track
            set_string(&(file_disc->tracks[tnum-1].t_artist), file_disc->d_artist);
            
            // set album name of track
            set_string(&(file_disc->tracks[tnum-1].t_album), file_disc->d_title);
            
            // set filename of track
            int audio_folderlen = strlen(audio_folder);
            while (p->fts_path[audio_folderlen] == '/') {
              audio_folderlen++;
            }
            char *t_filename = calloc(p->fts_pathlen - audio_folderlen + 1, sizeof(char));
            strncpy(t_filename, &(p->fts_path[audio_folderlen]), p->fts_pathlen - audio_folderlen);
            set_string(&(file_disc->tracks[tnum-1].t_filename), t_filename);
            free(t_filename);

            // set year
            file_disc->tracks[tnum-1].t_year = file_disc->d_year;

            // check file contents
            if (json_info != 2 && cue_sheet != 3) { 
              // no need to check file contents if we already have a complete json or cuesheet file
              FILE *f;   
              int length_in_frames = 0;
              f = fopen(p->fts_path, "r");
              if (f != NULL) {
                if (ends_with(".wav", p->fts_name)) {
                  long length_in_bytes = 0;
                  wav_open(f, &length_in_frames, &length_in_bytes, file_disc, &(file_disc->tracks[tnum-1]));
                  wav_close(f);
                } else if (ends_with(".flac", p->fts_name)) {
                  flac_decoder_context decoder_context;
                  flac_open(f, file_disc, &(file_disc->tracks[tnum-1]), &decoder_context);
                  length_in_frames = decoder_context.length_in_frames;
                  flac_close(&decoder_context);
                } else {
                  // unsupported audio file format: ensure file is closed
                  fclose(f);
                }
                
                if (length_in_frames > 0) {
                  file_disc->tracks[tnum-1].t_length = length_in_frames;
                  file_disc->d_length = file_disc->d_length + length_in_frames;
                }

                if (verbose) {
                  cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder: processed track #:[%d]; title:[%s]; length:[%u]; filename:[%s];", file_disc->tracks[tnum-1].t_num, file_disc->tracks[tnum-1].t_title, file_disc->tracks[tnum-1].t_length, file_disc->tracks[tnum-1].t_filename);
                }
              } else {
                cde_report(CDE_MSG_TYPE_ERROR, "scan audio folder: unable to open audio file: %s", p->fts_path);
              }
            } else if (verbose) {
              cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder: found audio file: %s; path: %s; level: %d", p->fts_name, p->fts_path, p->fts_level);
            }

            // set disc_level if not already done
            if (disc_level == 0) {
              disc_level = p->fts_level - 1;
            }
            
          } else {
            cde_report(CDE_MSG_TYPE_WARNING, "scan audio folder: found audio file with invalid track number; filename:[%s]", p->fts_name);
          }
        } else if (ends_with(".jpg", p->fts_name) || ends_with(".png", p->fts_name)) {
          image_files++;
          if (strcmp(p->fts_name, CDE_COVER_FRONT) == 0) {
            // @todo: 
            //if (file_disc->mb_front_cover_size == 0) {
            //  file_disc->mb_front_cover_size = read_file(&file_disc->mb_front_cover, p->fts_path);
            //}
            front_cover = 1;
            if (verbose) {
              cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder audio: found image file: %s; path: %s; level: %d; front cover: %d", p->fts_name, p->fts_path, p->fts_level, front_cover);
            }
          } else if (strcmp(p->fts_name, CDE_COVER_BACK) == 0) {
            // @todo: 
            //if (file_disc->mb_back_cover_size == 0) {
            //  file_disc->mb_back_cover_size = read_file(&file_disc->mb_back_cover, p->fts_path);
            //}
            back_cover = 1;
            if (verbose) {
              cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder audio: found image file: %s; path: %s; level: %d; back cover: %d", p->fts_name, p->fts_path, p->fts_level, back_cover);
            }
          } else {
            if (verbose) {
              cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder audio: found image file: %s; path: %s; level: %d", p->fts_name, p->fts_path, p->fts_level);
            }
          }
        } else if (ends_with(".cddb", p->fts_name)) {
          cddb_files++;
          set_string(&cddb_filename, p->fts_path);
          if (verbose) {
            cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder audio: found cddb entry: %s; path: %s; level: %d", p->fts_name, p->fts_path, p->fts_level);
          }
        } else if (ends_with(".m3u", p->fts_name)) {
          m3u_files++;
          set_string(&m3u_filename, p->fts_path);
          if (verbose) {
            cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder audio: found m3u entry: %s; path: %s; level: %d", p->fts_name, p->fts_path, p->fts_level);
          }
        } else {
          if (verbose) {
            cde_report(CDE_MSG_TYPE_DEBUG, "scan audio folder: found unknown file: %s", p->fts_path);
          }
        }
      }
      break;
    default:
      break;
    }
  }

  // cleanup
  fts_close(ftsp);
  free(folder_album);
  free(folder_artist);
  free(cddb_filename);
  free(m3u_filename);

  // log the scan results
  clock_t clock_end = clock();
  char *unit;
  double period = elapsed_format(clock_start, clock_end, &unit);
  cde_report(CDE_MSG_TYPE_INFO, "scan audio folder: processed %ld audio folders and %ld tracks; stored %ld discs; already stored %ld discs; discarded %ld discs; ignored %ld paths; period:%.2f%s", folder_count, audio_files_count, stored_disc_count, already_stored_disc_count, discarded_disc_count, ignored_path_count, period, unit);
}

/**
 * @brief traverse the cddb folder and scan for disc info
 * @param cddb_folder the cddb folder to scan
 * @param db the database to store the found disc info
 */
void scan_cddb_folder(char *cddb_folder, sql_db *db) {
  FTS *ftsp;
  FTSENT *p;
  FTSENT *chp;
  int fts_options = FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOCHDIR;
  char *folders[] = {cddb_folder, NULL};
  long category_id = 0;

  long file_count = 0;
  long stored_count = 0;
  long duplicate_count = 0;
  long encoding_count = 0;
  long error_count = 0;

  char *category = calloc(16, sizeof(char));
  clock_t clock_start = clock();

  if ((ftsp = fts_open(folders, fts_options, NULL)) == NULL) {
    cde_report(CDE_MSG_TYPE_ERROR, "scan cddb folder: unable to open folder: %s", cddb_folder);
    return;
  }

  if ((p = fts_read(ftsp)) == NULL) {
    fts_close(ftsp);
    cde_report(CDE_MSG_TYPE_ERROR, "scan cddb folder: unable to read folder: %s", cddb_folder);
    return;
  }
 
  // get linked list of structures which describe the files contained in the directory.
  chp = fts_children(ftsp, 0);
  if (chp == NULL) {
    // no files in folder
    return;               
  }
 
  cde_report(CDE_MSG_TYPE_INFO, "scan cddb folder: path: %s; level: %d", chp->fts_path, chp->fts_level);

  // scan the folder for files
  while ((p = fts_read(ftsp)) != NULL) {
    switch (p->fts_info) {
    case FTS_D:
      // entering path
      cde_report(CDE_MSG_TYPE_INFO, "scan cddb folder: entering path: %s; level: %d", p->fts_path, p->fts_level);
      if (p->fts_level == 1) {
        // get category from folder
        char *tmp = realloc(category, (p->fts_namelen+1) * sizeof(char));
        if (tmp == NULL) {
          break;
        }
        category = tmp;
        strcpy(category, p->fts_name);
        // try to get the category
        category_id = get_category_id(db, category);
      }
      break;
    case FTS_DP:
      // leaving path: store the disc information from the cuesheet in the database if found and no json info is available
      cde_report(CDE_MSG_TYPE_INFO, "scan cddb folder: leaving path: %s; level: %d", p->fts_path, p->fts_level);
      break;
    case FTS_SL:
    case FTS_F:
      if (p->fts_level > 1) {
        char* cddb_data = NULL;
        if (read_file(&cddb_data, p->fts_path) > 0) {
          // try to parse the cddb data
          disc *disc_info = cde_alloc_disc(CDE_MAX_TRACKS);
          if (disc_info != NULL) {
            // set the disc id from the current file name
            unsigned int disc_id = 0;
            if (uint_from_hex(&disc_id, p->fts_name) == 0) {
              disc_info->d_id = disc_id;
            }
            // parse the cddb data
            int res = cddb_parse_data(disc_info, NULL, cddb_data, 0, 0, 0);
            if (res == CDE_OK) {
              // set the cddb disc category
              if (disc_info->cddb_category != NULL) {
                free(disc_info->cddb_category);
              }
              disc_info->cddb_category = calloc(strlen(category)+1, sizeof(char));
              if (disc_info->cddb_category != NULL) {
                strcpy(disc_info->cddb_category, category);
              }
              // set the cddb entry id from the current filename
              disc_info->cddb_e_id = disc_id;
              // set the 64-bit internal hash to identify and lookup the disc
              cde_set_hash(disc_info);
              // store the disc information from the cddb data in the database
              int sres = store_cddb_entry_in_database(db, disc_info, category_id, DB_DUPLICATE_CHECK_TEMP);
              if (sres == DB_OK) {
                stored_count++;
              } else if (sres == DB_DUPLICATE) {
                duplicate_count++;
              } else {
                cde_report(CDE_MSG_TYPE_ERROR, "scan cddb folder: unable to store disc information from cddb data: (%d) %s (%d) %s", p->fts_level, p->fts_path, db->status, db->msg);
                error_count++;
              }
            } else if (res == CDE_ERROR_CDDB_ENCODING) {
              // encoding error
              encoding_count++;
            } else {
              // parse error
              cde_report(CDE_MSG_TYPE_ERROR, "scan cddb folder: unable to parse cddb data: %s", p->fts_path);
              error_count++;
            }
            // free the disc information with the number of tracks we allocated memory for
            cde_free_disc(&disc_info, CDE_MAX_TRACKS);
          }
          if (cddb_data != NULL) {
            free(cddb_data);
          }
          file_count++;
          if (file_count % 10000 == 0) {
            cde_report(CDE_MSG_TYPE_INFO, "scan cddb folder: processed %ld files; stored %ld cddb entries in the database; encountered %ld duplicate entries, %ld encoding issues and %ld other errors", file_count, stored_count, duplicate_count, encoding_count, error_count);
          }
        } else {
          cde_report(CDE_MSG_TYPE_WARNING, "scan cddb folder: unable to read cddb data: %s", p->fts_path);
          error_count++;
        }
      } else {
        cde_report(CDE_MSG_TYPE_INFO, "scan cddb folder: discarding file: %s", p->fts_path);
      }
      break;
    default:
      break;
    }
  }

  // cleanup
  fts_close(ftsp);
  free(category);

  // log the scan results
  clock_t clock_end = clock();
  char *unit;
  double period = elapsed_format(clock_start, clock_end, &unit);
  cde_report(CDE_MSG_TYPE_INFO, "scan cddb folder: processed %ld files; stored %ld cddb entries in the database; encountered %ld duplicate entries, %ld encoding issues and %ld other errors; period:%.2f%s", file_count, stored_count, duplicate_count, encoding_count, error_count, period, unit);
}
