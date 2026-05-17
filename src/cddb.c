/**************************************************************************

  libcdextract - cddb client

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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>

#include "report.h"
#include "http_client.h"
#include "file_utils.h"
#include "string_utils.h"
#include "libcdextract_types.h"
#include "cddb.h"


static const char *cddb_categories[] = {
  "data",
  "rock",
  "folk",
  "jazz",
  "blues",
  "classical",
  "country",
  "newage",
  "reggae",
  "soundtrack",
  "misc", 
  NULL
};

/**
 * @brief get the category enum value from the given string
 */
int cddb_get_enum_category(char *category_str) {
  int i = 0;
  char *str = calloc(1, sizeof(char));
  if (to_lower(&str, category_str) == 0) {
    while (cddb_categories[i] != NULL) {
      if (strcmp(str, cddb_categories[i]) == 0) {
        return i;
      }
      i++;
    }
  }
  // return default category
  return data;
}

/**
 * @brief get the category string value from the given enum value
 */
const char *cddb_get_string_category(cddb_category category_val) {
  if (category_val >= data && category_val <= misc) {
    return cddb_categories[category_val];
  }
  return cddb_categories[0];
}

/**
 * @brief get the sanitized genre from the given string
 * @return 0 if successful
 */
int cddb_get_string_genre(char **genre, char *genre_str) {
  size_t genre_len = 0;
  // check input string
  if (genre_str != NULL) {
    genre_len = strlen(genre_str);
  }
  // allocate output string
  if (*genre == NULL) {
    *genre = malloc(genre_len + 1);
    if (*genre == NULL) {
      return CDE_ERROR_CDDB_DATA;
    }
  } else {
    char *tmp = realloc(*genre, (genre_len + 1) * sizeof(char));
    if (tmp == NULL) {
      return CDE_ERROR_CDDB_DATA;
    }
    *genre = tmp;
  }
  int numeric_count = 0;
  int char_count = 0;
  int invalid_count = 0;
  int word_count = 0;
  int max_pos = 0;
  int in_pos = 0;
  int out_pos = 0;
  int has_trailing = 0;
  // skip leading whitespace
  while (in_pos < genre_len && (genre_str[in_pos] == ' ' || genre_str[in_pos] == '\t' || genre_str[in_pos] == '\r' || genre_str[in_pos] == '\n')) {
    in_pos++;
  }
  while (in_pos < genre_len) {
    // keep the number of processed words
    word_count++;
    if (word_count == 5) {
      // max. 4 words allowed in output string
      max_pos = out_pos - 1;
    }
    // convert first character of word to upper case
    if (in_pos < genre_len) {
      if (genre_str[in_pos] >= '0' && genre_str[in_pos] <= '9') {
        // numeric character
        (*genre)[out_pos++] = genre_str[in_pos++];
        numeric_count++;
      } else if (genre_str[in_pos] >= 0x00 && genre_str[in_pos] <= 0x20) {
        // invalid character
        invalid_count++;
        out_pos++;
      } else {
        // convert lower case character to upper case
        (*genre)[out_pos++] = (char)toupper(genre_str[in_pos++]);
        char_count++;
      }
    }
    // process rest of the word in lower case
    while (in_pos < genre_len && genre_str[in_pos] != ' ' &&
           genre_str[in_pos] != '\t' && genre_str[in_pos] != '\r' &&
           genre_str[in_pos] != '\n' && genre_str[in_pos] != ',' &&
           genre_str[in_pos] != '/' && genre_str[in_pos] != '&' &&
           genre_str[in_pos] != ';' && genre_str[in_pos] != '-' &&
           genre_str[in_pos] != '+' && genre_str[in_pos] != ':' &&
           (genre_str[in_pos] != '\\' && genre_str[in_pos] != '|')) {
      if (genre_str[in_pos] >= '0' && genre_str[in_pos] <= '9') {
        // numeric character
        (*genre)[out_pos++] = genre_str[in_pos++];
        numeric_count++;
      } else if ((genre_str[in_pos] >= 0x00 && genre_str[in_pos] <= 0x20) ||
                 genre_str[in_pos] == '*') {
        // invalid character
        invalid_count++;
        in_pos++;
      } else {
        // convert upper case character to lower case
        (*genre)[out_pos++] = (char)tolower(genre_str[in_pos++]);
        char_count++;
      }
    }
    // skip trailing whitespace and separator characters
    while (in_pos < genre_len &&
           (genre_str[in_pos] == ' ' || genre_str[in_pos] == '\t' ||
            genre_str[in_pos] == '\r' || genre_str[in_pos] == '\n' ||
            genre_str[in_pos] == ',' || genre_str[in_pos] == '/' ||
            genre_str[in_pos] == '&' || genre_str[in_pos] == ';' ||
            genre_str[in_pos] == '-' || genre_str[in_pos] == '+' ||
            genre_str[in_pos] == ':' || genre_str[in_pos] == '\\' ||
            genre_str[in_pos] == '|')) {
      in_pos++;
      has_trailing = 1;
    }
    if (has_trailing == 1) {
      has_trailing = 0;
      if (in_pos < genre_len) {
        (*genre)[out_pos++] = ' ';
      }
    }
  }
  // end string
  (*genre)[out_pos] = '\0';
  // check if we have to reduce the number of words
  if (max_pos > 0) {  
    (*genre)[max_pos] = '\0';
  }
  // check and perform special mappings
  int i=0;
  int and_match = 1;
  while (genre_mapping[i].map_type != CDBB_GENRE_MAP_TYPE_END) { 
    switch (genre_mapping[i].map_type) {
      case CDBB_GENRE_MAP_TYPE_EQUAL_OR:
        // try to find exact match on any of the elements in the list
        for (const char **from_genre = genre_mapping[i].from_genre_str; *from_genre != NULL; from_genre++) {
          if (strcmp(*genre, *from_genre) == 0) {
            size_t to_genre_len = strlen(genre_mapping[i].to_genre_str);
            char *mapped_genre = realloc(*genre, (to_genre_len + 1) * sizeof(char));
            if (mapped_genre == NULL) {
              return CDE_ERROR_CDDB_DATA;
            }
            strcpy(mapped_genre, genre_mapping[i].to_genre_str);
            *genre = mapped_genre;
            return CDE_OK;
          }
        }
        break;
      case CDBB_GENRE_MAP_TYPE_EQUAL_AND:
        // try to find exact match on all elements in the list
        for (const char **from_genre = genre_mapping[i].from_genre_str; *from_genre != NULL; from_genre++) {
          if (strstr(*genre, *from_genre) == NULL) {
            and_match = 0;
            break;
          }
        }
        if (and_match == 1) {
          size_t to_genre_len = strlen(genre_mapping[i].to_genre_str);
          char *mapped_genre = realloc(*genre, (to_genre_len + 1) * sizeof(char));
          if (mapped_genre == NULL) {
            return CDE_ERROR_CDDB_DATA;
          }
          strcpy(mapped_genre, genre_mapping[i].to_genre_str);
          *genre = mapped_genre;
          return CDE_OK;
        }
        break;
      case CDBB_GENRE_MAP_TYPE_BEGINS_OR:
        // try to find exact match on the beginning of any of the elements in the list
        for (const char **from_genre = genre_mapping[i].from_genre_str; *from_genre != NULL; from_genre++) {
          if (strstr(*genre, *from_genre) == *genre) {
            size_t to_genre_len = strlen(genre_mapping[i].to_genre_str);
            char *mapped_genre = realloc(*genre, (to_genre_len + 1) * sizeof(char));
            if (mapped_genre == NULL) {
              return CDE_ERROR_CDDB_DATA;
            }
            strcpy(mapped_genre, genre_mapping[i].to_genre_str);
            *genre = mapped_genre;
            return CDE_OK;
          }
        }
        break;
      case CDBB_GENRE_MAP_TYPE_CONTAINS:
        // try to find matching substring on any of the elements in the list
        for (const char **from_genre = genre_mapping[i].from_genre_str; *from_genre != NULL; from_genre++) {
          if (strstr(*genre, *from_genre) != NULL) {
            size_t to_genre_len = strlen(genre_mapping[i].to_genre_str);
            char *mapped_genre = realloc(*genre, (to_genre_len + 1) * sizeof(char));
            if (mapped_genre == NULL) {
              return CDE_ERROR_CDDB_DATA;
            }
            strcpy(mapped_genre, genre_mapping[i].to_genre_str);
            *genre = mapped_genre;
            return CDE_OK;
          }
        }
        break;
      default:
        // unknown map type
        break;
    }
    i++;
  }
  // check conversion
  if (char_count == 0 && numeric_count > 0) {
    // genre cannot be a single numeric value
    (*genre)[0] = '\0';
    return CDE_ERROR_CDDB_DATA;
  } else if (invalid_count > char_count / 3) {
    // genre cannot contain more invalid/special characters than valid ones
    (*genre)[0] = '\0';
    return CDE_ERROR_CDDB_DATA;
  } else if (char_count == 1) {
    // genre cannot have a single character
    (*genre)[0] = '\0';
    return CDE_ERROR_CDDB_DATA;
  }
  // conversion ok
  return CDE_OK;
}

/**
 * @brief calculate cddb checksum
 *        a number like 2344 becomes 2+3+4+4 (13)
 */
int cddb_sum(int n) {
  int ret = 0;

  while (n > 0) {
    ret = ret + (n % 10);
    n = n / 10;
  }

  return ret;
}

/**
 * @brief try to reconstruct cddb/musicbrainz query strings from the disc_info
 *        if they are not yet present
 * @param disc_info the disc information structure
 * @param track_frame_offsets the offsets for each track (use NULL to fully reconstruct the frame offset)
 * @param verbose print detailed output
 * @return 0 if successful
 */
int cddb_reconstruct_query_strings(disc *disc_info, char *track_frame_offsets, int verbose) {
  
  if (strlen(disc_info->cddb_query) > 0 &&
      strlen(disc_info->mb_query) > 0 &&
      strlen(disc_info->mb_fuzzy_lookup) > 0) {
    // query information already present
    return CDE_OK;
  }
  
  char tmp[32];
  char cddb_query_str[2048];
  char mb_query_str[2048];
  char mb_fuzzy_str[2048];

  // the cddb query string starts with the disc id
  sprintf((char *)cddb_query_str, "%08x", disc_info->d_id);

  // the musicbrainz query string starts with the first track
  sprintf((char *)mb_query_str, "%d", 1);

  // the musicbrainz fuzzy toc lookup starts with the first track
  sprintf((char *)mb_fuzzy_str, "%d", 1);

  // add the number of tracks to the query strings
  sprintf((char *)tmp, "+%d", disc_info->d_tracks);
  strcat((char *)cddb_query_str, (char *)tmp);
  strcat((char *)mb_query_str, (char *)tmp);
  strcat((char *)mb_fuzzy_str, (char *)tmp);

  // add frame offsets of all tracks
  if (track_frame_offsets == NULL) {
    long frame_offset = CDE_CD_MSF_OFFSET;
    sprintf((char *)tmp, "+%ld", frame_offset);
    strcat((char *)cddb_query_str, (char *)tmp);
    strcat((char *)mb_query_str, (char *)tmp);
    strcat((char *)mb_fuzzy_str, (char *)tmp);
    for (int i = 0; i < disc_info->d_tracks-1; i++) {
      frame_offset += disc_info->tracks[i].t_length;
      sprintf((char *)tmp, "+%ld", frame_offset);
      strcat((char *)cddb_query_str, (char *)tmp);
      strcat((char *)mb_query_str, (char *)tmp);
      strcat((char *)mb_fuzzy_str, (char *)tmp);
    }
  } else {
    strcat((char *)cddb_query_str, track_frame_offsets);
    strcat((char *)mb_query_str, track_frame_offsets);
    strcat((char *)mb_fuzzy_str, track_frame_offsets);
  }

  // add length of disc in seconds to the cddb query 
  sprintf((char *)tmp, "+%d", disc_info->d_length / CDE_CD_FRAMES);
  strcat((char *)cddb_query_str, tmp);

  // add length of disc in frames for the musicbrainz query
  sprintf((char *)tmp, "+%ld", (long)(disc_info->d_length + CDE_CD_MSF_OFFSET));
  strcat((char *)mb_query_str, (char *)tmp);
  
  // if no cddb query string present: use reconstructed cddb query string
  if (strlen(disc_info->cddb_query)==0) {
    disc_info->cddb_query = realloc(disc_info->cddb_query, (strlen((char *)cddb_query_str)+1) * sizeof(char));
    strcpy(disc_info->cddb_query, (char *)cddb_query_str);
    if (verbose) {
      cde_report(CDE_MSG_TYPE_INFO, "cddb_reconstruct_query_strings: using reconstructed cddb query: %s", disc_info->cddb_query);
    } 
  }

  // if no mb query string present: use reconstructed mb query string
  if (strlen(disc_info->mb_query)==0) {
    disc_info->mb_query = realloc(disc_info->mb_query, (strlen((char *)mb_query_str)+1) * sizeof(char));
    strcpy(disc_info->mb_query, (char *)mb_query_str);
    if (verbose) {
      cde_report(CDE_MSG_TYPE_INFO, "cddb_reconstruct_query_strings: using reconstructed musicbrainz query: %s", disc_info->mb_query);
    } 
  }

  // if no mb fuzzy lookup string present: use reconstructed mb fuzzy toc lookup string
  if (strlen(disc_info->mb_fuzzy_lookup)==0) {
    disc_info->mb_fuzzy_lookup = realloc(disc_info->mb_fuzzy_lookup, (strlen((char *)mb_fuzzy_str)+1) * sizeof(char));
    strcpy(disc_info->mb_fuzzy_lookup, (char *)mb_fuzzy_str);
    if (verbose) {
      cde_report(CDE_MSG_TYPE_DEBUG, "cddb_reconstruct_query_strings: using reconstructed musicbrainz fuzzy lookup:%s", disc_info->mb_fuzzy_lookup);
    }
  }

  return 0;
}

/**
 * @brief get cddb token from data and update position in data to
 *        the contents of this token.
 */
int cddb_get_token(char *data, int *position, int length, int *title_nr) {
  cddb_keyword result = UNKNOWN;
  *title_nr = -1; // no keyword title nr available 
  char *word = calloc(1, sizeof(char));
  
  // get first word from data 
  if (get_word(&word, data, position, length) > 0) {

    // get location of '=' when available
    char *token_data_start = strchr(word, '=');
    int pos_from_start = 0;
    if (token_data_start != NULL) {
      pos_from_start = (int)(token_data_start - word) + 1;
    }

    // determine token type and set position to the first character after the '=' seperator
    if (starts_with("DISCID", word)) {
      result = DISCID;
      *position = pos_from_start;
    } else if (starts_with("DTITLE", word)) {
      result = DTITLE;
      *position = pos_from_start;
    } else if (starts_with("DYEAR", word)) {
      result = DYEAR;
      *position = pos_from_start;
    } else if (starts_with("DGENRE", word)) {
      result = DGENRE;
      *position = pos_from_start;
    } else if (starts_with("TTITLE", word)) {
      // note: track title number between 'TTITLE' and '='
       int title_pos = 6;
      *title_nr = get_signed_int(word, &title_pos, pos_from_start-1);
      result = TTITLEN;
      *position = pos_from_start;
    } else if (starts_with("EXTD", word)) {
      result = EXTD;
      *position = pos_from_start;
    } else if (starts_with("EXTT", word)) {
      // note: extended track information between 'EXTT' and '='
      int title_pos = 4;
      *title_nr = get_signed_int(word, &title_pos, pos_from_start-1);
      result = EXTTN;
      *position = pos_from_start;
    } else if (starts_with("PLAYORDER", word)) {
      result = PLAYORDER;
      *position = pos_from_start;
    } else if (starts_with("#", word)) {
      result = COMMENT;
      *position = 0;
    } else if (starts_with(".", word)) {
      result = DOT;
      *position = 0;
    }
  }
  free(word);
  return (int)result;
}

/**
 * @brief parse query response data and store the results in disc_info
 */
int cddb_parse_query_response(disc *disc_info, char* cddb_data, int verbose) {
  char *line = calloc(1, sizeof(char));
  int result = 0;
  int res = 0;
  int pos = 0;
  int len = strlen(cddb_data);

  if (get_line(&line, cddb_data, &pos, len) > 0) {
    int pos_code = 0;
    int status_code = get_signed_int(cddb_data, &pos_code, len);
    if (status_code >= 200 && status_code <300) {
      // response = OK
      if (status_code == 200) {
        // in case of an exact match, only one line with all info is returned
        pos = 0;
      }
      // handle exact match (200), multiple exact matches (210) or close matches (211)
      if (verbose) {
        cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_query_response: code: %d", status_code);
      }
      // interpret result from next line; for example: "e0aabb78 data Various / Hit Songs"
      if (get_line(&line, cddb_data, &pos, len) > 0) {
        int posw = 0;
        char *word = calloc(1, sizeof(char));
        if (status_code == 200) {
          res = get_word(&word, line, &posw, len);
        }
        if ((res = get_word(&word, line, &posw, len)) > 0) {
          // first word contains the category
          if (verbose) {
            cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_query_response: category: %s", word);
          }
          disc_info->cddb_category = realloc(disc_info->cddb_category, (strlen(word)+1) * sizeof(char));
          strcpy(disc_info->cddb_category, word);
        }
        if ((res = get_word(&word, line, &posw, len)) > 0) {
          // second word contains discid
          if (verbose) {
            cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_query_response: cddb entry id: %s", word);
          }
          unsigned int entry_id = 0;
          if (uint_from_hex(&entry_id, word) == 0) {
            disc_info->cddb_e_id = entry_id;
          } else {
            if (verbose) {
              cde_report(CDE_MSG_TYPE_ERROR, "cddb_parse_query_response: conversion of [%s] to cddb entry id failed", word);
            }
            result = 4;
          }
        }
        // the rest of the line contains the artist and album name
        char *remainder = calloc(1, sizeof(char));
        if ((res = get_line(&remainder, line, &posw, len)) > 0) {
          if (verbose) {
            cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_query_response: artist/album: %s", remainder);
          }
          char *artist = calloc(1, sizeof(char));
          char *title = calloc(1, sizeof(char));
          if (split(&artist, &title, remainder, '/')) {
            disc_info->d_artist = realloc(disc_info->d_artist, (strlen(artist) + 1) * sizeof(char));
            strcpy(disc_info->d_artist, artist);
            disc_info->d_title = realloc(disc_info->d_title, (strlen(title) + 1) * sizeof(char));
            strcpy(disc_info->d_title, title);
          }
          free(title);
          free(artist);
        }
        free(remainder);
        free(word);      
      } else {
        if (verbose) {
          cde_report(CDE_MSG_TYPE_ERROR, "cddb_parse_query_response: status code (%d); unable to process result from line", status_code);
        }
        result = 3;
      }
    } else {
      if (verbose) {
        cde_report(CDE_MSG_TYPE_ERROR, "cddb_parse_query_response: status code (%d)", status_code);
      }
      result = 2;
    }
  } else {
    if (verbose) {
      cde_report(CDE_MSG_TYPE_ERROR, "cddb_parse_query_response: unable to proces returned data");
    }
    result = 1;
  }

  free(line);
  return result;
}

/**
 * @brief parse read response data and store the results in disc_info
 *        PRE: disc_info must be initialized
 */
int cddb_parse_data(disc *disc_info, cdrom_drive *drive, char* cddb_data, int pos, int reconstruct_queries, int verbose) {

  int track_frame_offset_mode = 0;
  long prev_track_frame_offset = CDE_CD_MSF_OFFSET;
  long curr_track_frame_offset = 0;
  int track_frame_nr = 0;

  char tmp[32];
  char query_str[2048];
  query_str[0] = '\0';

  int encoding = ENCODING_UNDEFINED;
  char *line = calloc(1, sizeof(char));
  int len = strlen(cddb_data);

  int token_data_pos = 0;
  int title_nr = -1;
  int old_title_nr = -1;

  // parse cddb data
  int proceed = 1;
  while (proceed) {
    // iterate over the returned document
    // get next line
    if (get_line(&line, cddb_data, &pos, len) > 0) {
      token_data_pos = 0;
      title_nr = -1;
      int token_type = cddb_get_token(line, &token_data_pos, len, &title_nr);
      char *token_data_raw = calloc(1, sizeof(char));
      get_line(&token_data_raw, line, &token_data_pos, len);
      // convert raw data to utf-8
      char *token_data = to_utf8(token_data_raw, 0, &encoding);
      if (encoding != ENCODING_UNDEFINED) {
        unsigned int disc_id = 0;
        switch (token_type) {
        case DISCID:
          if (strchr(token_data, ',') != NULL) {
            // multiple discid's are present
            char *next_id = strtok(token_data, ",");
            while (next_id != NULL) {
              if (uint_from_hex(&disc_id, next_id) == 0) {
                if (disc_info->cddb_d_id == disc_id) {
                  // found matching disc id already set
                  break;
                } else if (disc_info->d_id == disc_id) {
                  // found matching disc id for current disc info
                  disc_info->cddb_d_id = disc_id;
                  break;
                } else {
                  // update cddb disc id with parsed value
                  disc_info->cddb_d_id = disc_id;
                }
                if (verbose) {
                  cde_report(CDE_MSG_TYPE_WARNING, "cddb_parse_data: cddb disc id (multi): %s", next_id);
                }
              }
              next_id = strtok(NULL, ",");
            }
          } else {
            // normal case: 'single discid': check the disc id against the read disc id
            if (uint_from_hex(&disc_id, token_data) == 0) {
              if (disc_info->d_id != disc_id || disc_info->cddb_d_id != disc_id) {
                // disc id from query result does not match stored disc id
                if (verbose) {
                  cde_report(CDE_MSG_TYPE_WARNING, "cddb_parse_data: returned cddb disc id [%08x] differs from read disc id: [%08x] or id from cddb query: [%08x]", disc_id, disc_info->d_id, disc_info->cddb_d_id);
                }
              }
              if (disc_info->cddb_d_id == 0) {
                disc_info->cddb_d_id = disc_id;
              }
            }
          }
          // set read disc id if not already set
          if (disc_info->d_id == 0) {
            disc_info->d_id = disc_info->cddb_d_id;
          }
          break;
        case DTITLE:
          // set disc title
          if (verbose) {
            if (strcmp(token_data, token_data_raw) == 0) {
              cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_data: disc title: %s", token_data);
            } else {
              cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_data: converted disc title: [%s] to: [%s] (utf-8)", token_data_raw, token_data);
            }
          }
          char *artist = calloc(1, sizeof(char));
          char *title = calloc(1, sizeof(char));
          if (split(&artist, &title, token_data, '/')) {
            // artist and disc title separated by a '/'
            if (strlen(disc_info->d_artist)==0 || strcmp(disc_info->d_artist, artist)!=0) {
              char *tmp = realloc(disc_info->d_artist, (strlen(artist) + 1) * sizeof(char));
              if (tmp != NULL) {
                disc_info->d_artist = tmp;
                strcpy(disc_info->d_artist, artist);
              }
              if (verbose) {
                cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_data: disc artist changed to:[%s]", disc_info->d_artist);
              }
            } else if (verbose) {
              cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_data: disc artist:[%s]", disc_info->d_artist);
            }

            if (strlen(disc_info->d_title)==0 || strcmp(disc_info->d_title, title)!=0) {
              char *tmp = realloc(disc_info->d_title, (strlen(title) + 1) * sizeof(char));
              if (tmp != NULL) {
                disc_info->d_title = tmp;
                strcpy(disc_info->d_title, title);
              }
              if (verbose) {
                cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_data: disc title changed to:[%s]", disc_info->d_title);
              }
            } else if (verbose) {
              cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_data: disc title:[%s]", disc_info->d_title);
            }
          } else {
            // no '/' present, artist and disc title are the same
            if (strlen(disc_info->d_artist)==0 || strcmp(disc_info->d_artist, token_data)!=0) {
              char *tmp = realloc(disc_info->d_artist, (strlen(token_data) + 1) * sizeof(char));
              if (tmp != NULL) {
                disc_info->d_artist = tmp;
                strcpy(disc_info->d_artist, token_data);
              }
              if (verbose) {
                cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_data: disc artist changed to:[%s]", disc_info->d_artist);
              }
            } else if (verbose) {
              cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_data: disc artist:[%s]", disc_info->d_artist);
            }

            if (strlen(disc_info->d_title)==0 || strcmp(disc_info->d_title, token_data)!=0) {
              char *tmp = realloc(disc_info->d_title, (strlen(token_data) + 1) * sizeof(char));
              if (tmp != NULL) {
                disc_info->d_title = tmp;
                strcpy(disc_info->d_title, token_data);
              }
              if (verbose) {
                cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_data: disc title changed to:[%s]", disc_info->d_title);
              }
            } else if (verbose) {
              cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_data: disc title:[%s]", disc_info->d_title);
            }
          }
          free(title);
          free(artist);
          break;
        case DYEAR:
          // set disc year
          if (verbose) {
            cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_data: disc year:%s", token_data);
          }
          int year = 0;
          if (sscanf(token_data, "%d", &year) == 1) {
            disc_info->d_year = year;
          } else if (verbose) {
            // invalid year
            cde_report(CDE_MSG_TYPE_WARNING, "cddb_parse_data: invalid disc year:%s", token_data);
          }
          break;            
        case DGENRE:
          // set disc genre
          if (cddb_get_string_genre(&(disc_info->d_genre), token_data) == CDE_OK) {
            if (verbose) {
              cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_data: disc genre:[%s] mapped to:[%s]", token_data, disc_info->d_genre);
            }
          } else {
            // failed to set genre
            if (verbose) {
              cde_report(CDE_MSG_TYPE_WARNING, "cddb_parse_data: failed to map genre:[%s]", token_data);
            }
          }
          break;
        case TTITLEN:
          // set track title
          if (old_title_nr != title_nr) {
            // copy new track title
            char *tmp = realloc(disc_info->tracks[title_nr].t_title, (strlen(token_data) + 1) * sizeof(char));
            if (tmp != NULL) {
              disc_info->tracks[title_nr].t_title = tmp;
              strcpy(disc_info->tracks[title_nr].t_title, token_data);
            }
          } else {
            // add additional track title information
            char *tmp = calloc(strlen(disc_info->tracks[title_nr].t_title) + strlen(token_data) +1, sizeof(char));
            if (tmp != NULL) {
              strcpy(tmp, disc_info->tracks[title_nr].t_title);
              strcat(tmp, token_data);
              free(disc_info->tracks[title_nr].t_title);
              disc_info->tracks[title_nr].t_title = tmp;
            }
          }
          old_title_nr = title_nr;
          if (disc_info->d_tracks < title_nr + 1 && title_nr < CDE_MAX_TRACKS) {
            disc_info->d_tracks = title_nr + 1;
          }
          if (verbose) {
            cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_data: track title %d:%s", title_nr, disc_info->tracks[title_nr].t_title);
          }
          break;
        case EXTD:
          // set extended disc data
          if (verbose) {
            cde_report(CDE_MSG_TYPE_DEBUG, "cddb_parse_data: extended disc data:%s", token_data);
          }
          break;  
        case EXTTN:
          // set extended track data
          if (verbose) {
            cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_data: extended data track %d:%s", title_nr, token_data);
          }
          disc_info->tracks[title_nr].t_extended = realloc(disc_info->tracks[title_nr].t_extended, (strlen(token_data) + 1) * sizeof(char));
          strcpy(disc_info->tracks[title_nr].t_extended, token_data);
          break;
        case PLAYORDER:
          // set playorder
          if (verbose) {
            cde_report(CDE_MSG_TYPE_DEBUG, "cddb_parse_data: playorder: %s", token_data);
          }
          break;  
        case DOT:
          // last line contains '.' to indicate end of the document
          if (verbose) {
            cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_data: finished: %s", line);
          }
          proceed = 0;
          break;
        case COMMENT:
          // set comment
          if (verbose) {
            cde_report(CDE_MSG_TYPE_DEBUG, "cddb_parse_data: comment: %s", line);
          }
          // try to set virtual drive characteristics from the comments in the data

          // process track frame offsets
          if (track_frame_offset_mode == 0 && contains(token_data, "Track frame offsets")) {
            track_frame_offset_mode = 1; // get frame offsets from the next lines
          } else if (track_frame_offset_mode == 1) {

            if (contains(token_data, "Disc length")) {

              // last track frame offset has been processed: get disc length
              int p=14;
              int disc_length = get_signed_int(token_data, &p, strlen(token_data));
              if (verbose) {
                cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_data: disc length from disc: %d seconds; disc length from cddb response: %d seconds", disc_info->d_length / CDE_CD_FRAMES, disc_length);
                cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_data: last track frame offset: %ld; number of parsed tracks: %d", curr_track_frame_offset, track_frame_nr);
              }

              // set length of the disc (if not already done)
              if (disc_info->d_length == 0) {
                disc_info->d_length = disc_length * CDE_CD_FRAMES;
              }

              // set the number of tracks (if not already done)
              if (disc_info->d_tracks != track_frame_nr && track_frame_nr <= CDE_MAX_TRACKS) {
                disc_info->d_tracks = track_frame_nr;
                if (verbose) {
                  cde_report(CDE_MSG_TYPE_DEBUG, "cddb_parse_data: new track count after disc length: %d", disc_info->d_tracks);
                }
              }

              // set length of last track (if not already done)
              if (track_frame_nr > 0 && disc_info->tracks[track_frame_nr-1].t_length==0) {
                disc_info->tracks[track_frame_nr-1].t_length = (disc_length * CDE_CD_FRAMES) - curr_track_frame_offset;           
              }
              if (verbose) {
                cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_data: length of last track (%d): %d", track_frame_nr, disc_info->tracks[track_frame_nr-1].t_length);
              }

              // only for a virtual drive: we use the cdrom_drive structure to store the frames
              if (drive != NULL && drive->cdda_device_name != NULL && strcmp(drive->cdda_device_name, CDE_VIRTUAL_DRIVE)==0) {
                drive->disc_toc[track_frame_nr].dwStartSector = (disc_length * CDE_CD_FRAMES) - CDE_CD_MSF_OFFSET + 1;
              }
        
              // try to reconstruct cddb/musicbrainz query strings when they are not present yet
              if (reconstruct_queries == 1) {
                cddb_reconstruct_query_strings(disc_info, (char *)query_str, verbose);
              }

              track_frame_offset_mode = 2; // done
            } else {
              // get track frame offset (normal case)
              int p=2;
              long new_track_frame_offset=get_signed_long(token_data, &p, strlen(token_data));

              if (new_track_frame_offset != 0) {
                curr_track_frame_offset = new_track_frame_offset;

                // only for a virtual drive: we use the cdrom_drive structure to store the frames
                if (drive != NULL && drive->cdda_device_name != NULL &&
                    strcmp(drive->cdda_device_name, CDE_VIRTUAL_DRIVE) == 0 &&
                    drive->disc_toc[track_frame_nr].dwStartSector == 0) {
                  drive->disc_toc[track_frame_nr].dwStartSector = curr_track_frame_offset - CDE_CD_MSF_OFFSET;
                  drive->tracks = track_frame_nr + 1;
                }

                // set track length (if not already done)
                if (track_frame_nr > 0 && track_frame_nr <= CDE_MAX_TRACKS) {
                  if (disc_info->tracks[track_frame_nr-1].t_length==0) {       
                    disc_info->tracks[track_frame_nr-1].t_length = curr_track_frame_offset - prev_track_frame_offset;
                    if (verbose) {
                      cde_report(CDE_MSG_TYPE_DEBUG, "cddb_parse_data: set track length for track nr:%d; prev:%ld; cur:%ld; length:%d", track_frame_nr, prev_track_frame_offset, curr_track_frame_offset, disc_info->tracks[track_frame_nr-1].t_length);
                    }
                  }
                } else if (track_frame_nr > 0 && verbose) {
                  cde_report(CDE_MSG_TYPE_WARNING, "cddb_parse_data: track_frame_nr %d exceeds the number of tracks allowed", track_frame_nr);
                }

                // add current track frame offset to build the cddb/mb query
                tmp[0]='\0';
                sprintf((char *)tmp, "+%ld", curr_track_frame_offset);
                strcat((char *)query_str, (char *)tmp);

                prev_track_frame_offset = curr_track_frame_offset;
                track_frame_nr++;
              }
            }
          } else if (track_frame_offset_mode == 2) {
            if (contains(token_data, "Revision")) {
              // get revision number
              int p=11;
              disc_info->cddb_revision = get_signed_int(token_data, &p, strlen(token_data));
              if (verbose) {
                cde_report(CDE_MSG_TYPE_INFO, "cddb_parse_data: revision: %d", disc_info->cddb_revision);
              }
              if (disc_info->cddb_revision < 0) {
                // invalid revision number
                disc_info->cddb_revision = 0;
              }
              track_frame_offset_mode = 3; // done
            }
          }
          break;
        default:
          // unsupported or unknown
          if (verbose) {
            cde_report(CDE_MSG_TYPE_WARNING, "cddb_parse_data: unknown: %s", line);
          }
          break;
        }

      } else {
        // invalid data
        if (verbose) {
          cde_report(CDE_MSG_TYPE_ERROR, "cddb_parse_data: unable to convert [%s] to utf-8: [%s]", token_data_raw, token_data);
        }
        // clean up before return
        free(token_data);
        free(token_data_raw);
        free(line);
        return CDE_ERROR_CDDB_ENCODING;
      }

      // clean up token
      free(token_data);
      free(token_data_raw);

    } else {
      // no more data
      proceed = 0;
    }
  } // end while

  // cleanup line
  free(line);

  // try to set artist name for each track
  if (strcmp(disc_info->d_artist, "Various") == 0 ||
      strcmp(disc_info->d_artist, "Various Artists") == 0) {
    // disc artist is 'Various' or 'Various Artists'
    for (int i=0; i<disc_info->d_tracks; i++) {
      // try to get artist name from track title
      char *artist = calloc(1, sizeof(char));
      char *title = calloc(1, sizeof(char));
      if (split(&artist, &title, disc_info->tracks[i].t_title, '/')) {
        if (strlen(disc_info->tracks[i].t_artist)==0 || strcmp(disc_info->tracks[i].t_artist, artist)!=0) {
          char *tmp = realloc(disc_info->tracks[i].t_artist, (strlen(artist) + 1) * sizeof(char));
          if (tmp != NULL) {
            // set artist
            disc_info->tracks[i].t_artist = tmp;
            strcpy(disc_info->tracks[i].t_artist, artist);
          }
        }
      }
      free(title);
      free(artist);
    }
  } else {
    // normal album: copy artist to each track
    for (int i=0; i<disc_info->d_tracks; i++) {
      // set artist
      disc_info->tracks[i].t_artist = realloc(disc_info->tracks[i].t_artist, (strlen(disc_info->d_artist) + 1) * sizeof(char));
      strcpy(disc_info->tracks[i].t_artist, disc_info->d_artist);
    }
  }

  // copy other disc information to each track
  for (int i=0; i<disc_info->d_tracks; i++) {
    // set album
    disc_info->tracks[i].t_album = realloc(disc_info->tracks[i].t_album, (strlen(disc_info->d_title) + 1) * sizeof(char));
    strcpy(disc_info->tracks[i].t_album, disc_info->d_title);
    // set genre
    disc_info->tracks[i].t_genre = realloc(disc_info->tracks[i].t_genre, (strlen(disc_info->d_genre) + 1) * sizeof(char));
    strcpy(disc_info->tracks[i].t_genre, disc_info->d_genre);
    // set year
    disc_info->tracks[i].t_year = disc_info->d_year;
  }
  
  // check if parsed record is really ok
  if (disc_info->d_tracks <= 0 || disc_info->d_tracks > CDE_MAX_TRACKS) {
    if (verbose) {
      cde_report(CDE_MSG_TYPE_ERROR, "cddb_parse_data: invalid track count: %d", disc_info->d_tracks);
    }
    return CDE_ERROR_CDDB_DATA;
  }
  if (disc_info->cddb_d_id == 0) {
    if (verbose) {
      cde_report(CDE_MSG_TYPE_ERROR, "cddb_parse_data: missing cddb disc id");
    }
    return CDE_ERROR_CDDB_DATA;
  }
  if (disc_info->d_length <= 0 || disc_info->d_length > 100 * 60 * CDE_CD_FRAMES) {
    if (verbose) {
      cde_report(CDE_MSG_TYPE_ERROR, "cddb_parse_data: invalid disc length: %d", disc_info->d_length);
    }
    return CDE_ERROR_CDDB_DATA;
  }
  if (starts_with("Unknown", disc_info->d_title) == 1 && starts_with("Unknown ", disc_info->d_artist) == 1) {
    if (verbose) {
      cde_report(CDE_MSG_TYPE_ERROR, "cddb_parse_data: invalid title or artist: [%s][%s]", disc_info->d_title, disc_info->d_artist);
    }
    return CDE_ERROR_CDDB_DATA;
  }

  return CDE_OK;
}

/**
 * @brief query the online cddb service and parse the response
 *        Example url:
 *        https://gnudb.gnudb.org/~cddb/cddb.cgi?cmd=cddb+query+92093e0a+10+175+18469+34444+51154+70524+88841+104824+124686+140966+159454+2368&hello=pi+cdplayer+cddb-tool+0.4.7&proto=6
 */
int cddb_query(disc *disc_info, const char *end_point, int verbose) {
  int res = CDE_OK;
  char url[512];
  ssize_t size = -1;
  char *response = calloc(1, sizeof(char));
  
  sprintf(url, "%s?cmd=cddb+query+%s&hello=%s&proto=6", end_point, disc_info->cddb_query, CDDB_HELLO);
  size = http_get_data(url, &response, verbose);
  if (size > 0) {
    res = cddb_parse_query_response(disc_info, response, verbose);
  } else {
    res = CDE_ERROR_CDDB_DATA;
  }
  
  free(response);
  return res;
}

/**
 * @brief read from the online cddb service and parse the response
 *        Example url:
 *        https://gnudb.gnudb.org/~cddb/cddb.cgi?cmd=cddb+read+data+92093e0a&hello=pi+cdplayer+cddb-tool+0.4.7&proto=6
 */
int cddb_read(disc *disc_info, cdrom_drive *drive, const char *end_point, int verbose) {  
  int res = CDE_OK;
  char url[512];
  ssize_t size = -1;
  char *response = calloc(1, sizeof(char));

  sprintf(url, "%s?cmd=cddb+read+%s+%08x&hello=%s&proto=6", end_point, disc_info->cddb_category, disc_info->cddb_e_id, CDDB_HELLO);
  size = http_get_data(url, &response, verbose);
  if (size > 0) {
    char *line = calloc(1, sizeof(char));
    int pos = 0;
    int len = strlen(response);
  
    if (get_line(&line, response, &pos, len) > 0) {
      int pos_code = 0;
      int status_code = get_signed_int(line, &pos_code, len);
      if (status_code >= 200 && status_code < 300) {
        // response = OK
        // note: exact match (200), multiple exact matches (210) or close matches (211)
        if (verbose) {
          cde_report(CDE_MSG_TYPE_INFO, "cddb_read: status code: %d", status_code);
        }
        // parse cddb data
        res = cddb_parse_data(disc_info, drive, response, pos, 1, verbose);
      } else {
        if (verbose) {
          cde_report(CDE_MSG_TYPE_ERROR, "cddb_read: received status (%d); expected code: exact match (200), multiple exact matches (210) or close matches (211)", status_code);
        }
        res = CDE_ERROR_CDDB_DATA;
      }
    }
    free(line);
  } else {
    res = CDE_ERROR_CDDB_DATA;
  }
  
  free(response);
  return res;
}

/**
 * @brief query and read from the online cddb service
 *        returned cddb information will be parsed and stored in disc_info
 */
int cddb_get_disc_info(disc *disc_info, cdrom_drive *drive, int verbose) {
  int res = CDE_OK;
  disc_info->cddb_complete = 0;
  disc_info->mb_complete = 0;
  if ((res = cddb_query(disc_info, CDDB_REMOTE_ENDPOINT, verbose)) == CDE_OK) {
    res &= cddb_read(disc_info, drive, CDDB_REMOTE_ENDPOINT, verbose);
  }
  disc_info->cddb_complete = res == CDE_OK ? 1 : 0;
  return res;
}

/**
 * @brief writes a cddb entry from the gathered disc information to a file in xmcd format
 *        PRE: prepared disc information structure
 * @param disc_info the disc information structure
 * @param folder folder to store the file
 * @param overwrite overwrite the file if it exists.
 * @param verbose print detailed output
 * @return 0 if successful
 */
int cddb_write_entry(disc *disc_info, const char *folder, int overwrite, int verbose) {

  if (disc_info == NULL || folder == NULL) {
    return -2;
  }

  char *artist = replace_chars(disc_info->d_artist, FILENAME_CHAR_FILTER, '-');
  char *title = replace_chars(disc_info->d_title, FILENAME_CHAR_FILTER, '-');

  int len = strlen(folder) + strlen(artist) + strlen(title) + 8;
  char *cddb_file = calloc(len, sizeof(char));
  snprintf(cddb_file, len, "%s/%s-%s.cddb", folder, artist, title);
  
  free(title);
  free(artist);

  struct stat st = {0};
  if (overwrite == 0 && stat(cddb_file, &st) != -1) {
    if (verbose) {
      cde_report(CDE_MSG_TYPE_INFO, "not overwriting cddb entry: %s", cddb_file);
    }
    free(cddb_file);
    return 0;
  }

  if (verbose) {
    cde_report(CDE_MSG_TYPE_INFO, "writing cddb entry: %s", cddb_file);
  }
  
  FILE *f = fopen(cddb_file, "w");
  if (f == NULL) {
    free(cddb_file);
    return -1;
  }
  
  fprintf(f, "# xmcd\n");
  fprintf(f, "#\n");
  fprintf(f, "# Track frame offsets:\n");
  int frame_offset = 150;
  for (int i = 0; i < disc_info->d_tracks; i++) {
    fprintf(f, "#       %d\n", frame_offset);
    frame_offset += disc_info->tracks[i].t_length;
  }
  fprintf(f, "#\n");
  fprintf(f, "# Disc length: %d\n", disc_info->d_length / CDE_CD_FRAMES);
  fprintf(f, "#\n");
  fprintf(f, "# Revision: %d\n", disc_info->cddb_revision);
  fprintf(f, "#\n");
  fprintf(f, "# Submitted via: xmcd 2.0\n");
  fprintf(f, "#\n");
  fprintf(f, "DISCID=%08x\n", disc_info->d_id);
  fprintf(f, "DTITLE=%s / %s\n", disc_info->d_artist, disc_info->d_title);
  if (disc_info->d_year > 0) {
    fprintf(f, "DYEAR=%d\n", disc_info->d_year);
  } else {
    fprintf(f, "DYEAR=\n");
  }
  fprintf(f, "DGENRE=%s\n", disc_info->d_genre);
  for (int i = 0; i < disc_info->d_tracks; i++) {
    fprintf(f, "TTITLE%d=%s\n", i+1, disc_info->tracks[i].t_title);
  }
  fprintf(f, "EXTD=%s\n", disc_info->d_extended);
  for (int i = 0; i < disc_info->d_tracks; i++) {
     fprintf(f, "EXTT%d=%s\n", i+1, disc_info->tracks[i].t_extended);
  }
  fprintf(f, "PLAYORDER=\n");
  
  free(cddb_file);
  return fclose(f);
}