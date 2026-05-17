/**************************************************************************

  libcdextract - cue sheet writer / parser

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

#include <linux/limits.h>
#include <stdlib.h>
#include <stdio.h>

#include "config.h"
#include "report.h"
#include "string_utils.h"
#include "file_utils.h"
#include "libcdextract_types.h"
#include "cue_sheet.h"


static const char *cue_keywords[] = {
  "CATALOG",
  "CDTEXTFILE",
  "FILE",
  "FLAGS",
  "DCP",
  "4CH",
  "PRE",
  "SCMS",
  "INDEX",
  "ISRC",
  "PERFORMER",
  "POSTGAP",
  "PREGAP",
  "SONGWRITER",
  "TITLE",
  "TRACK",
  "REM",
  "GENRE",
  "CATEGORY",
  "DATE",
  "DISCID",
  "LOOKUP",
  "COMMENT",
  "LENGTH",
  "CDDBQUERY",
  "CDDBCATEGORY",
  "CDDBENTRYID",
  "CDDBDISCID",
  "CDDBREVISION",
  "MBQUERY",
  "MBFUZZYLOOKUP",
  "MBDISCID", 
  "MBRELEASEID",
  "EXTENDED",
  "REPLAYGAIN_ALBUM_GAIN",
  "REPLAYGAIN_ALBUM_PEAK",
  "REPLAYGAIN_TRACK_GAIN",
  "REPLAYGAIN_TRACK_PEAK",
  "FILE",
  NULL
};

/**
 * @brief get the cue sheet keyword and set position to the first character after the '=' separator
 * 
 * @return int the keyword (enum)
 */
int get_cue_sheet_keyword(char *data, int *position, int length) {
  cue_keyword keyword = CUE_UNKNOWN;
  char *key = calloc(1, sizeof(char));

  if (get_word(&key, data, position, length)) {
    // try to get the key word
    int i = 0;
    while (cue_keywords[i] != NULL) {
      if (starts_with(cue_keywords[i], key)) {
        keyword = (cue_keyword)i;
        break;
      }
      i++;
    }
    // try to get a specific comment keyword
    if (keyword == CUE_REM) {
      char* remark_key = calloc(1, sizeof(char));
      if (get_word(&remark_key, data, position, length)) {
        int j = (int)CUE_REM_GENRE;
        while (cue_keywords[j] != NULL) {
          if (starts_with(cue_keywords[j], remark_key)) {
            keyword = (cue_keyword)j;
            break;
          }
          j++;
        }
        free(remark_key);
      }
    }
  }

  free(key);
  return (int)keyword;
}

/**
 * @brief get the number of frames from a timestamp string formatted as: mm::ss:ff
 * 
 * @return int the number of frames
 */
int get_cue_frames(char *data, int *position, int length) {
  int cue_frames = 0;
  int len = length - *position;
  char *length_str = calloc(len + 1, sizeof(char));
  get_word(&length_str, data, position, length);
  if (len >= 8) {
    char *minutes_str = calloc(3, sizeof(char));
    char *seconds_str = calloc(3, sizeof(char));
    char *frames_str = calloc(3, sizeof(char));
    int tmp = 0;
    substring(&minutes_str, length_str, &tmp, 2);
    tmp++;
    substring(&seconds_str, length_str, &tmp, 2);
    tmp++;
    substring(&frames_str, length_str, &tmp, 2);
    int minutes = 0;
    sscanf(minutes_str, "%d", &minutes);
    int seconds = 0;
    sscanf(seconds_str, "%d", &seconds);
    int frames = 0;
    sscanf(frames_str, "%d", &frames);
    cue_frames = (minutes * 60 * CDE_CD_FRAMES) + (seconds * CDE_CD_FRAMES) + frames;
    free(minutes_str);
    free(seconds_str);
    free(frames_str);
  }
  free(length_str);
  return cue_frames;
}

/**
 * @brief parses a cue sheet and stores the results in the disc information structure
 *        PRE: allocated disc information structure for all possible tracks
 * @param disc_info the disc information structure
 * @param cue_data the cue sheet data to parse
 * @param verbose print detailed output
 * @return 0 on success, non-zero on failure
 */
int parse_cue_sheet(disc *disc_info, const char *cue_data, int verbose) {

  char *line = calloc(1, sizeof(char));
  
  int tmp = 0;
  int data_pos = 0;
  int data_len = strlen(cue_data);

  char *filename = calloc(1, sizeof(char));
  int file_num = 0;
  int track_num = 0;
  int pregap = 0;
  int postgap = 0;
  int prev_frames = CDE_CD_MSF_OFFSET;

  // set default category to 'data'
  disc_info->cddb_category = realloc(disc_info->cddb_category, 5 * sizeof(char));
  strcpy(disc_info->cddb_category, "data");
  // no tracks found yet
  disc_info->d_tracks = 0;

  while (get_line(&line, (char *)cue_data, &data_pos, data_len) > 0) {
    int pos_in_line = 0;
    int cue_keyword = get_cue_sheet_keyword(line, &pos_in_line, strlen(line));
    
    char *keyword_raw = calloc(strlen(line) + 1, sizeof(char));
    get_line(&keyword_raw, line, &pos_in_line, strlen(line));
    
    int s_pos = 0;
    int e_pos = strlen(keyword_raw);

    if (s_pos < e_pos && keyword_raw[0] == '\"') {
      s_pos = 1;
      while (e_pos > s_pos && keyword_raw[e_pos] != '\"') {
        e_pos--;
      }
    }

    char *keyword_data = calloc(e_pos - s_pos + 1, sizeof(char));
    strncpy(keyword_data, &keyword_raw[s_pos], e_pos - s_pos);
    keyword_data[e_pos - s_pos] = '\0';

    if (verbose) {
      cde_report(CDE_MSG_TYPE_DEBUG, "parse_cue_sheet: line: [%s]; keyword: [%d]; raw: [%s]; data: [%s]", line, cue_keyword, keyword_raw, keyword_data);
    }
    
    free(keyword_raw);
    
    // hierarchy: DISC > FILE > TRACK > INDEX
    switch (cue_keyword) {
      case CUE_CATALOG:
        // A 13-digit UPC/EAN code, also referred to as the Media Catolog Number (MCN)
        // 12-digit UPC codes should be prefixed with a "0"
        disc_info->d_extended = realloc(disc_info->d_extended, (strlen(keyword_data) + 1) * sizeof(char));
        strcpy(disc_info->d_extended, keyword_data);
        break;
      case CUE_CDTEXTFILE:       
        // A path to a file containing CD-Text info (ignored)
        break;
      case CUE_FILE:
        // A path to a file containing audio data, and to which subsequent commands apply
        filename = realloc(filename, (strlen(keyword_data) + 1) * sizeof(char));
        strcpy(filename, keyword_data);
        file_num++;
        break;  
      case CUE_FLAGS:
        // Per-track subcode flag(s) (ignored)
        break;
      case CUE_DCP:
        // Digital copy permitted (ignored)
        break;
      case CUE_4CH:
        // Four channel audio (ignored)
        break;
      case CUE_PRE:
        // Pre-emphasis enabled (audio tracks only) (ignored)
        break;
      case CUE_SCMS:
        // Serial Copy Management System (ignored)
        break;
      case CUE_INDEX:
        // Per-track index(es)
        {
          // get index number (expect 0 or 1)
          tmp = 0;
          int index_num = get_signed_int(keyword_data, &tmp, strlen(keyword_data));
          // split index in minutes, seconds, frames
          int frames = get_cue_frames(keyword_data, &tmp, strlen(keyword_data));

          // handle possible styles of cue sheets:
          // * standard single file cue sheet
          // * multi file cue sheet with corrected gaps
          // * multi file cue sheet with left out gaps (PREGAP)
          // * multi file cue sheet with gaps (non-compliant used by EAC)
          if (verbose) {
            cde_report(CDE_MSG_TYPE_DEBUG, "parse_cue_sheet: line: [%s]; keyword: [%d]; data: [%s]; files: [%d]; tracks: [%d]; index: [%d]; frames: [%d]; pregap: [%d]; postgap: [%d]", line, cue_keyword, keyword_data, file_num, track_num, index_num, frames, pregap, postgap);
          }

          if (index_num == 0) {
            // index = 0: pre gap of track

            if (file_num > 1 && file_num == track_num && frames > 0) {
              // multi file cue sheet, one file for each track; append gaps to end of previous track (non-compliant)
              disc_info->tracks[track_num - 2].t_length = frames;
            }

          } else if (index_num == 1) {
            // index = 1: start of track 

            if (file_num == 1 && track_num == 1) {
              // first track of the first file
              disc_info->tracks[track_num - 1].t_length = 0;

            } else if (file_num == 1 && track_num > 1) {
              // standard single file cue sheet

              // set track length in frames
              if (track_num <= disc_info->d_tracks && disc_info->tracks[track_num - 2].t_length == 0) {
                disc_info->tracks[track_num - 2].t_length = frames - prev_frames;

                // store frames to calculate length
                prev_frames = frames;
              }

            } else if (file_num > 1 && file_num == track_num) {
              // multi file cue sheet, one file for each track; start of track normally at 00:00:00
              disc_info->tracks[track_num - 1].t_length = 0;

            } else {
              //  unexpected structure
              if (verbose) {
                cde_report(CDE_MSG_TYPE_WARNING, "parse_cue_sheet: line: [%s]; keyword: [%d]; data: [%s]; unexpected structure: [%d] files with [%d] tracks", line, cue_keyword, keyword_data, file_num, track_num);
              }
            }

            // store the filename for the current track
            if (track_num <= disc_info->d_tracks && strlen(filename) > 0) { 
              disc_info->tracks[track_num - 1].t_filename = realloc(disc_info->tracks[track_num - 1].t_filename, (strlen(filename) + 1) * sizeof(char));
              strcpy(disc_info->tracks[track_num - 1].t_filename, filename);           
            }

            // reset gaps for next track
            pregap = 0;
            postgap = 0;
          } else {
            // index >1: start of next track / pregap for next track
            if (verbose) {
              cde_report(CDE_MSG_TYPE_WARNING, "parse_cue_sheet: line: [%s]; keyword: [%d]; data: [%s]; invalid index: [%d]", line, cue_keyword, keyword_data, index_num);
            }
          }
        }
        break;
      case CUE_ISRC:
        // Per-track ISRC(s) (ignored)
        break;
      case CUE_PERFORMER:
        // Per-disc or per-track performer name for CD-Text data
        // printf("Performer name: %s\n", keyword_data);
        if (track_num < 1) {
          disc_info->d_artist = realloc(disc_info->d_artist, (strlen(keyword_data) + 1) * sizeof(char));
          strcpy(disc_info->d_artist, keyword_data);
        } else if (track_num <= disc_info->d_tracks) {
          disc_info->tracks[track_num-1].t_artist = realloc(disc_info->tracks[track_num-1].t_artist, (strlen(keyword_data) + 1) * sizeof(char));
          strcpy(disc_info->tracks[track_num-1].t_artist, keyword_data);
        }
        break;
      case CUE_POSTGAP:
        // Amount of post-track silence to add (ignored)
        tmp = 0;
        postgap = get_cue_frames(keyword_data, &tmp, strlen(keyword_data));
        break;
      case CUE_PREGAP:
        // Amount of pre-track silence to add (ignored)
        tmp = 0;
        pregap = get_cue_frames(keyword_data, &tmp, strlen(keyword_data));
        break;
      case CUE_SONGWRITER:
        // Per-disc or per-track songwriter name for CD-Text data (ignored)
        break;
      case CUE_TITLE:
        // Per-disc or per-track title for CD-Text data
        if (track_num < 1) {
          disc_info->d_title = realloc(disc_info->d_title, (strlen(keyword_data) + 1) * sizeof(char));
          strcpy(disc_info->d_title, keyword_data);
        } else if (track_num <= disc_info->d_tracks) {
          disc_info->tracks[track_num-1].t_title = realloc(disc_info->tracks[track_num-1].t_title, (strlen(keyword_data) + 1) * sizeof(char));
          strcpy(disc_info->tracks[track_num-1].t_title, keyword_data);
        }
        break;
      case CUE_TRACK:
        // Type of track to create, and to which subsequent commands apply
        // set the track number
        {
          tmp = 0;
          int num = get_signed_int(keyword_data, &tmp, strlen(keyword_data));
          if (num > 0 && num < CDE_MAX_TRACKS) {
            track_num = num;

            // update the number of audio tracks on the disc; do not increase the number of tracks for data tracks (MODEx/2xxx)
            if (num > disc_info->d_tracks && strstr(keyword_data, "AUDIO") != NULL) {
              disc_info->d_tracks = num;
            }
          }
        }
        break;
      case CUE_REM:
        // An unspecified remark/comment (ignored)
        break;
      case CUE_REM_GENRE:
        if (track_num < 1) {
          disc_info->d_genre = realloc(disc_info->d_genre, (strlen(keyword_data) + 1) * sizeof(char));
          strcpy(disc_info->d_genre, keyword_data);
        } else if (track_num <= disc_info->d_tracks) {
          disc_info->tracks[track_num-1].t_genre = realloc(disc_info->tracks[track_num-1].t_genre, (strlen(keyword_data) + 1) * sizeof(char));
          strcpy(disc_info->tracks[track_num-1].t_genre, keyword_data);
        }
        break;
      case CUE_REM_CATEGORY:
        // Disc category
        disc_info->cddb_category = realloc(disc_info->cddb_category, (strlen(keyword_data) + 1) * sizeof(char));
        strcpy(disc_info->cddb_category, keyword_data);
        break;
      case CUE_REM_DATE:
        // Release date
        {
          int year = 0;
          if (track_num < 1) {
            if (sscanf(keyword_data, "%d", &year) == 1) {
              disc_info->d_year = year;
            }
          } else if (track_num <= disc_info->d_tracks) {
            if (sscanf(keyword_data, "%d", &year) == 1) {
              disc_info->tracks[track_num-1].t_year = year;
            }
          }
        }
        break;
      case CUE_REM_DISCID:
        // Read disc id
        {
          unsigned d_id = 0;
          if (uint_from_hex(&d_id, keyword_data) == 0) {
            disc_info->d_id = d_id;
            if (disc_info->cddb_d_id == 0) {
              disc_info->cddb_d_id = d_id;
            }
          }
        }
        break;
      case CUE_REM_LOOKUP:
        // Read 64-bit internal hash value
        {
          uint64_t d_lookup = 0;
          if (uint64_from_hex(&d_lookup, keyword_data) == 0) {
            disc_info->d_lookup = d_lookup;
          }
        }
        break;
      case CUE_REM_COMMENT:
        // Comment (ignored)
        break;
      case CUE_REM_LENGTH:
        // Length of disc in frames
        {
          int length = 0;
          if (sscanf(keyword_data, "%d", &length) == 1) {
            if (track_num < 1) {
              disc_info->d_length = length;
            } else if (track_num <= disc_info->d_tracks) {
              disc_info->tracks[track_num-1].t_length = length;
            }
          }
        }
        break;
      case CUE_REM_CDDBQUERY:
        // CDDB query
        disc_info->cddb_query = realloc(disc_info->cddb_query, (strlen(keyword_data) + 1) * sizeof(char));
        strcpy(disc_info->cddb_query, keyword_data);
        break;
      case CUE_REM_CDDBCATEGORY:
        // CDDB category
        disc_info->cddb_category = realloc(disc_info->cddb_category, (strlen(keyword_data) + 1) * sizeof(char));
        strcpy(disc_info->cddb_category, keyword_data);
        break;
      case CUE_REM_CDDBENTRYID:
        // CDDB entry id
        {
          unsigned cddb_e_id = 0;
          if (uint_from_hex(&cddb_e_id, keyword_data) == 0) {
            disc_info->cddb_e_id = cddb_e_id;
          }
        }
        break;
      case CUE_REM_CDDBDISCID:
        // CDDB disc id
        {
          unsigned cddb_d_id = 0;
          if (uint_from_hex(&cddb_d_id, keyword_data) == 0) {
            disc_info->cddb_d_id = cddb_d_id;
          }
        }
        break;
      case CUE_REM_CDDBREVISION:
        // CDDB entry revision
        {
          int cddb_revision = 0;
          if (sscanf(keyword_data, "%d", &cddb_revision) == 1) {
            disc_info->cddb_revision = cddb_revision;
          }
        }
        break;
      case CUE_REM_MBQUERY:
        // Musicbrainz query
        disc_info->mb_query = realloc(disc_info->mb_query, (strlen(keyword_data) + 1) * sizeof(char));
        strcpy(disc_info->mb_query, keyword_data);
        break;
      case CUE_REM_MBFUZZYLOOKUP:
        // Musicbrainz fuzzy TOC search
        disc_info->mb_fuzzy_lookup = realloc(disc_info->mb_fuzzy_lookup, (strlen(keyword_data) + 1) * sizeof(char));
        strcpy(disc_info->mb_fuzzy_lookup, keyword_data);
        break;
      case CUE_REM_MBDISCID:
        // Musicbrainz disc id
        disc_info->mb_disc_id = realloc(disc_info->mb_disc_id, (strlen(keyword_data) + 1) * sizeof(char));
        strcpy(disc_info->mb_disc_id, keyword_data);
        break;
      case CUE_REM_MBRELEASEID:
        // Musicbrainz release id
        disc_info->mb_release_id = realloc(disc_info->mb_release_id, (strlen(keyword_data) + 1) * sizeof(char));
        strcpy(disc_info->mb_release_id, keyword_data);
        break;
      case CUE_REM_EXTENDED:
        // Extended disc information
        if (track_num < 1) {
          disc_info->d_extended = realloc(disc_info->d_extended, (strlen(keyword_data) + 1) * sizeof(char));
          strcpy(disc_info->d_extended, keyword_data);
        } else if (track_num <= disc_info->d_tracks) {
          disc_info->tracks[track_num-1].t_extended = realloc(disc_info->tracks[track_num-1].t_extended, (strlen(keyword_data) + 1) * sizeof(char));
          strcpy(disc_info->tracks[track_num-1].t_extended, keyword_data);
        }
        break;
      case CUE_REM_REPLAYGAIN_ALBUM_GAIN:
        // Album gain (ignored)
        break;
      case CUE_REM_REPLAYGAIN_ALBUM_PEAK:
        // Album peak (ignored)
        break;
      case CUE_REM_REPLAYGAIN_TRACK_GAIN:
        // Track gain (ignored)
        break;
      case CUE_REM_REPLAYGAIN_TRACK_PEAK:
        // Track peak (ignored)
        break;
      case CUE_REM_FILE:
        // Path to a file containing track data
        filename = realloc(filename, (strlen(keyword_data) + 1) * sizeof(char));
        strcpy(filename, keyword_data);
        break;
      default:
        // Unknown keyword (ignored)
        break;
    }
    
    free(keyword_data);
  }
  // cleanup
  free(filename);
  free(line);
  if (disc_info->d_id > 0 && disc_info->d_tracks > 0 && disc_info->d_tracks <= CDE_MAX_TRACKS) {
    // ok, as it seems we stored disc information including track data 
    return (int)CDE_OK;
  }
  return (int)CDE_ERROR_CUE_SHEET;
}

/**
 * @brief writes a cue sheet from the gathered disc information to a file
 *        PRE: prepared disc information structure
 * @param disc_info the disc information structure
 * @param folder folder to store the file
 * @param overwrite overwrite the file if it exists
 * @param verbose print detailed output
 */
int write_cue_sheet(disc *disc_info, const char *folder, int overwrite, int verbose) {

  if (disc_info == NULL || folder == NULL) {
    return -2;
  }

  char *artist = replace_chars(disc_info->d_artist, FILENAME_CHAR_FILTER, '-');
  char *title = replace_chars(disc_info->d_title, FILENAME_CHAR_FILTER, '-');

  int len = strlen(folder) + strlen(artist) + strlen(title) + 7;
  char *cue_file = calloc(len, sizeof(char));
  snprintf(cue_file, len, "%s/%s-%s.cue", folder, artist, title);
  
  free(title);
  free(artist);

  struct stat st = {0};
  if (overwrite == 0 && stat(cue_file, &st) != -1) {
    if (verbose) {
      cde_report(CDE_MSG_TYPE_INFO, "not overwriting cue sheet: %s", cue_file);
    }
    free(cue_file);
    return 0;
  }

  if (verbose) {
    cde_report(CDE_MSG_TYPE_INFO, "writing cue sheet: %s", cue_file);
  }
  
  FILE *f = fopen(cue_file, "w");
  if (f == NULL) {
    free(cue_file);
    return -1;
  }
  
  //fprintf(f, "REM DBID %lu\n", disc_info->db_id);
  fprintf(f, "REM DISCID %08x\n", disc_info->d_id);
  if (disc_info->d_lookup > 0) {
    fprintf(f, "REM LOOKUP %016lx\n", disc_info->d_lookup);
  }
  fprintf(f, "REM LENGTH %d\n", disc_info->d_length);
  fprintf(f, "REM GENRE \"%s\"\n", disc_info->d_genre);
  if (disc_info->d_year > 0) {
    fprintf(f, "REM DATE %d\n", disc_info->d_year);
  }
  fprintf(f, "REM EXTENDED \"%s\"\n", disc_info->d_extended);
  fprintf(f, "REM CDDBQUERY %s\n", disc_info->cddb_query);
  fprintf(f, "REM CDDBCATEGORY %s\n", disc_info->cddb_category);
  fprintf(f, "REM CDDBENTRYID %08x\n", disc_info->cddb_d_id);
  fprintf(f, "REM CDDBDISCID %08x\n", disc_info->cddb_d_id);
  fprintf(f, "REM CDDBREVISION %08x\n", disc_info->cddb_d_id);
  fprintf(f, "REM MBQUERY %s\n", disc_info->mb_query);
  fprintf(f, "REM MBFUZZYLOOKUP %s\n", disc_info->mb_fuzzy_lookup);
  fprintf(f, "REM MBDISCID %s\n", disc_info->mb_disc_id);
  fprintf(f, "REM MBRELEASEID %s\n", disc_info->mb_release_id);
  fprintf(f, "REM COMMENT \"%s v%d.%d\"\n", CDEXTRACT_NAME, CDEXTRACT_VERSION_MAJOR, CDEXTRACT_VERSION_MINOR);

  fprintf(f, "PERFORMER \"%s\"\n", disc_info->d_artist);
  fprintf(f, "TITLE \"%s\"\n", disc_info->d_title);
  fprintf(f, "FILE \"%016lx.bin\" WAVE\n", disc_info->d_lookup);

  long offset = 0;
  for (int i = 0; i < disc_info->d_tracks; i++) {
    fprintf(f, "  TRACK %d AUDIO\n", disc_info->tracks[i].t_num);
    fprintf(f, "    TITLE \"%s\"\n", disc_info->tracks[i].t_title);
    fprintf(f, "    PERFORMER \"%s\"\n", disc_info->tracks[i].t_artist);
    fprintf(f, "    REM LENGTH %d\n", disc_info->tracks[i].t_length);
    if (disc_info->tracks[i].t_year > 0) {
      fprintf(f, "    REM DATE %d\n", disc_info->tracks[i].t_year);
    }
    fprintf(f, "    REM FILE \"%s\"\n", disc_info->tracks[i].t_filename);
    fprintf(f, "    INDEX 01 %02d:%02d:%02d\n", (int)(offset / (60 * CDE_CD_FRAMES)), (int)((offset / CDE_CD_FRAMES) % 60), (int)(offset % CDE_CD_FRAMES));

    offset += disc_info->tracks[i].t_length;
  }

  free(cue_file);
  return fclose(f);
}
