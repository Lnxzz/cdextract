/**************************************************************************

  libcdextract - musicbrainz api / coverart archive client

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

#include "mb_api.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#include "coverart/caa_c.h"
#include "report.h"
#include "http_client.h"
#include "json_utils.h"
#include "string_utils.h"
#include "libcdextract_types.h"


#define MB_API_ENDPOINT "https://musicbrainz.org/ws/2"
#define MB_DISCID_LOOKUP "%s/discid/%s?toc=%s&fmt=json"
#define MB_RELEASE_LOOKUP "%s/release/%s?inc=recordings+artists+discids&fmt=json"
#define MB_RELEASE_QUERY_ARTYF "%s/release/?query=artist:%%22%s%%22%%20AND%%20release:%%22%s%%22%%20AND%%20tracks:%d%%20AND%%20date:%d%%20AND%%20format:%%22CD%%22&inc=recordings+artists&fmt=json"
#define MB_RELEASE_QUERY_ARTF "%s/release/?query=artist:%%22%s%%22%%20AND%%20release:%%22%s%%22%%20AND%%20tracks:%d%%20AND%%20format:%%22CD%%22&inc=recordings+artists&fmt=json"
#define MB_RELEASE_QUERY_ARF "%s/release/?query=artist:%%22%s%%22%%20AND%%20release:%%22%s%%22%%20AND%%20format:%%22CD%%22&inc=recordings+artists&fmt=json"
#define MB_CAA_ENDPOINT "https://coverartarchive.org/release"
#define MB_CAA_RELEASE_INFO "%s/%s"
#define MB_CAA_FRONT_COVER "%s/%s/front"
#define MB_CAA_BACK_COVER "%s/%s/back"
#define MB_MAX_URL_LENGTH 512


/**
 * @brief download the json formatted release information from the coverartarchive (CAA) 
 *        using the specified musicbrainz release id
 *        PRE: output folder must exist when writing to file
 */
int mb_caa_get_release_info(disc *disc_info, int download_coverart, const char *folder, int verbose) {
  int res = 0;
  char url[MB_MAX_URL_LENGTH];
  char *info_response = calloc(1, sizeof(char));
  ssize_t info_response_size;

  if (download_coverart >= MB_COVERART_FULL) {
    snprintf(url, MB_MAX_URL_LENGTH, MB_CAA_RELEASE_INFO, MB_CAA_ENDPOINT, disc_info->mb_release_id);
    info_response_size = http_get_data(url, &info_response, verbose);
    if (info_response_size > 0) {
      int file_name_size;
      char *file_name;
      FILE *fptr;
      file_name_size = snprintf(NULL, 0, "%s/%s", folder, CDE_COVER_INFO);
      file_name = malloc((file_name_size + 1) * sizeof(char));
      sprintf(file_name, "%s/%s", folder, CDE_COVER_INFO);
      fptr = fopen(file_name, "wb");
      if (fptr) {
        fwrite(info_response, info_response_size, 1, fptr);
        fclose(fptr);
        if (verbose) {
          cde_report(CDE_MSG_TYPE_INFO, "saved cover art archive release information to: '%s'", file_name);
        }
      } else {
        cde_report(CDE_MSG_TYPE_WARNING, "unable to save cover art archive release information to: '%s'", file_name);
        res = 1;
      }
      free(file_name);
    } else {
      cde_report(CDE_MSG_TYPE_INFO, "no cover art archive release information available");
      res = 0;
    }
  }
  free(info_response);
  return res;
}

/**
 * @brief download the front cover from the coverartarchive (CAA) 
 *        using the specified musicbrainz release id
 *        PRE: output folder must exist when writing to file
 */
int mb_caa_get_front_cover(disc *disc_info, int download_coverart, const char *folder, int verbose) {
  int res = 0;
  char url[MB_MAX_URL_LENGTH];
  char *cover_response = calloc(1, sizeof(char));
  ssize_t cover_response_size;

  snprintf(url, MB_MAX_URL_LENGTH, MB_CAA_FRONT_COVER, MB_CAA_ENDPOINT, disc_info->mb_release_id);
  cover_response_size = http_get_data(url, &cover_response, verbose);
  if (cover_response_size > 0) {
    // malloc & copy
    char *tmp_front = realloc(disc_info->mb_front_cover, cover_response_size * sizeof(char));
    if (tmp_front != NULL) {
      disc_info->mb_front_cover = tmp_front;
      memcpy(disc_info->mb_front_cover, cover_response, cover_response_size);
      disc_info->mb_front_cover_size = cover_response_size;
    }

    // write cover to file when requested
    if (download_coverart >= MB_COVERART_COVER_ONLY) {
      int file_name_size;
      char *file_name;
      FILE *fptr;
      file_name_size = snprintf(NULL, 0, "%s/%s", folder, CDE_COVER_FRONT);
      file_name = malloc((file_name_size + 1) * sizeof(char));
      sprintf(file_name, "%s/%s", folder, CDE_COVER_FRONT);
      fptr = fopen(file_name, "wb");
      if (fptr) {
        fwrite(cover_response, cover_response_size, 1, fptr);
        fclose(fptr);
        if (verbose) {
          cde_report(CDE_MSG_TYPE_INFO, "saved cover art archive front cover to: '%s'", file_name);
        }
      } else {
        cde_report(CDE_MSG_TYPE_WARNING, "unable to save cover art archive front cover to: '%s'", file_name);
        res = 1;        
      }
      free(file_name);
    }
  } else {
    cde_report(CDE_MSG_TYPE_WARNING, "no cover art archive front cover available");
    res = 1;
  }
  free(cover_response);
  return res;
}

/**
 * @brief download the back cover from the coverartarchive (CAA) 
 *        using the specified musicbrainz release id
 *        PRE: output folder must exist when writing to file
 */
int mb_caa_get_back_cover(disc *disc_info, int download_coverart, const char *folder, int verbose) {
  int res = 0;
  char url[MB_MAX_URL_LENGTH];
  char *cover_response = calloc(1, sizeof(char));
  ssize_t cover_response_size;

  snprintf(url, MB_MAX_URL_LENGTH, MB_CAA_BACK_COVER, MB_CAA_ENDPOINT, disc_info->mb_release_id);
  cover_response_size = http_get_data(url, &cover_response, verbose);
  if (cover_response_size > 0) {
    // malloc & copy
    char *tmp_back = realloc(disc_info->mb_back_cover, cover_response_size * sizeof(char));
    if (tmp_back != NULL) {
      disc_info->mb_back_cover = tmp_back;
      memcpy(disc_info->mb_back_cover, cover_response, cover_response_size);
      disc_info->mb_back_cover_size = cover_response_size;
    }

    // write cover to file when requested
    if (download_coverart >= MB_COVERART_COVER_ONLY) {
      int file_name_size;
      char *file_name;
      FILE *fptr;
      file_name_size = snprintf(NULL, 0, "%s/%s", folder, CDE_COVER_BACK);
      file_name = malloc((file_name_size + 1) * sizeof(char));
      sprintf(file_name, "%s/%s", folder, CDE_COVER_BACK);
      fptr = fopen(file_name, "wb");
      if (fptr) {
        fwrite(cover_response, cover_response_size, 1, fptr);
        fclose(fptr);
        if (verbose) {
          cde_report(CDE_MSG_TYPE_INFO, "saved cover art archive back cover to: '%s'", file_name);
        }
      } else {
        cde_report(CDE_MSG_TYPE_WARNING, "unable to save cover art archive back cover to: '%s'", file_name);
        res = 1;
      }
      free(file_name);
    }
  } else {
    cde_report(CDE_MSG_TYPE_INFO, "no cover art archive back cover available");
    res = 0;
  }
  free(cover_response);
  return res;
}

/**
 * @brief parse disc id lookup response data and store the musicbrainz release id
 * @return 0 on success, >=1 on error
 */
int mb_parse_discid_lookup_response(disc *disc_info, char *data, int verbose) {
  int res = 0;
  int len = strlen(data);

  // parse json data
  json_token token;
  char *value = calloc(JSON_UTILS_MAX_TOKEN_STR_SIZE, sizeof(char));
  if (json_init(&token, data, len) == json_ok) {
    if (json_select_member(&token, "$.releases[0].id", value, JSON_UTILS_MAX_TOKEN_STR_SIZE, data) > 0) {

      // allocate new string and copy data
      size_t val_len = strlen(value);
      char *tmp = realloc(disc_info->mb_release_id, (val_len + 1) * sizeof(char));
      if (tmp == NULL) {
          // handle memory allocation failure
          free(value);
          return 3;
      }
      disc_info->mb_release_id = tmp;
      strcpy(disc_info->mb_release_id, value);

      if (verbose) { 
        cde_report(CDE_MSG_TYPE_INFO, "mb_parse_discid_lookup_response: musicbrainz release id:[%s]", disc_info->mb_release_id);
      }
    } else {
      if (verbose) {
        cde_report(CDE_MSG_TYPE_ERROR, "mb_parse_discid_lookup_response: member $.releases[0].id NOT found");
      }
      res = 1;
    }
  } else {
    if (verbose) {
      cde_report(CDE_MSG_TYPE_ERROR, "mb_parse_discid_lookup_response: unable to parse response");
    }
    res = 1;
  }

  free(value);
  return res;
}

/**
 * @brief parse release lookup response data and store the results in disc_info
 * @return 0 on success, >=1 on error
 */
int mb_parse_release_lookup_response(disc *disc_info, char *data, int verbose) {
  int res = 0;
  int len = strlen(data);

  // parse json data
  json_token token;
  char *value = calloc(JSON_UTILS_MAX_TOKEN_STR_SIZE, sizeof(char));
  if (verbose) {
    cde_report(CDE_MSG_TYPE_INFO, "parsing release lookup response for musicbrainz release id:[%s]", disc_info->mb_release_id);
  }

  if (json_init(&token, data, (int)len) == json_ok) {
    if (strcmp(disc_info->d_title, CDE_UNKNOWN_ALBUM) == 0) {
      // no disc title yet, try to extract it from the returned json data
      json_select_member(&token, "$.title", value, JSON_UTILS_MAX_TOKEN_STR_SIZE, data);

      // allocate new string and copy data
      size_t val_len = strlen(value);
      char *tmp = realloc(disc_info->d_title, (val_len + 1) * sizeof(char));
      if (tmp == NULL) {
          // handle memory allocation failure
          free(value);
          return 3;
      }
      disc_info->d_title = tmp;
      strcpy(disc_info->d_title, value);
  
      if (verbose) {
        cde_report(CDE_MSG_TYPE_INFO, "disc title:[%s]", disc_info->d_title);
      }
    }

    if (strcmp(disc_info->d_artist, CDE_UNKNOWN_ARTIST) == 0) {
      // no artist yet, try to extract it from the returned json data
      json_select_member(&token, "$.artist-credit.[0].name", value, JSON_UTILS_MAX_TOKEN_STR_SIZE, data);

      // allocate new string and copy data
      size_t val_len = strlen(value);
      char *tmp = realloc(disc_info->d_artist, (val_len + 1) * sizeof(char));
      if (tmp == NULL) {
          // handle memory allocation failure
          free(value);
          return 3;
      }
      disc_info->d_artist = tmp;
      strcpy(disc_info->d_artist, value);

      if (verbose) {
        cde_report(CDE_MSG_TYPE_INFO, "disc artist:[%s]", disc_info->d_artist);
      }
    }

    if (disc_info->d_year == 0) {
      // no year yet, try to extract it from the returned json data
      // json path: $['date']" --> $.date
      json_select_member(&token, "$.date", value, JSON_UTILS_MAX_TOKEN_STR_SIZE, data);
      if (verbose) {
        cde_report(CDE_MSG_TYPE_INFO, "disc date:[%s]", value);
      }
      int d_year = 0;
      if (strlen(value) == 4) {
        if (sscanf(value, "%d", &d_year) == 1 && d_year > CDE_MIN_YEAR && d_year < CDE_MAX_YEAR) {
          disc_info->d_year = d_year;
        }
      } else {
        char *left = calloc(16, sizeof(char));
        char *mid = calloc(16, sizeof(char));
        char *right = calloc(16, sizeof(char));
        if (split(&left, &mid, value, '-')) {
          if (strlen(left) == 4) {
            if (sscanf(left, "%d", &d_year) == 1 && d_year > CDE_MIN_YEAR && d_year < CDE_MAX_YEAR) {
              disc_info->d_year = d_year;
            }              
          } else if (strlen(mid) > 4) {
            if (split(&left, &right, mid, '-')) {
              if (strlen(right) == 4) {
                if (sscanf(right, "%d", &d_year) == 1 && d_year > CDE_MIN_YEAR && d_year < CDE_MAX_YEAR) {
                  disc_info->d_year = d_year;
                }   
              }
            }
          }
        }
        free(right);
        free(mid);
        free(left);
      }
    }

    // set track information, if not already done
    // $['media'][0]['track-count']
    // $.media.[0].track-count
    // $..track-count
    json_select_member(&token, "$..track-count", value, JSON_UTILS_MAX_TOKEN_STR_SIZE, data);
    int track_cnt = 0;
    char *path = calloc(41, sizeof(char));
    if (sscanf(value, "%d", &track_cnt) == 1) {
      // track_cnt should match disc->d_tracks
      if (track_cnt == disc_info->d_tracks) {
        for (int i = 0; i < track_cnt; i++) {
          // set track number, if not already done: $..tracks.[i].number
          snprintf(path, 40, "$..tracks.[%d].number", i);
          json_select_member(&token, path, value, JSON_UTILS_MAX_TOKEN_STR_SIZE, data);
          int track_nr = i;
          disc_info->tracks[i].t_num = i + 1;
          if (sscanf(value, "%d", &track_nr) == 1) {
            track_nr--;
          }

          // set track title, if not already done: $..tracks.[i].title
          if (starts_with("Track ", disc_info->tracks[track_nr].t_title) == 1) {
            snprintf(path, 40, "$..tracks.[%d].title", i);
            json_select_member(&token, path, value, JSON_UTILS_MAX_TOKEN_STR_SIZE, data);
            if (strlen(value) > 0) {
              char *tmp_tt = realloc(disc_info->tracks[track_nr].t_title, (strlen(value) + 1) * sizeof(char));
              if (tmp_tt != NULL) {
                disc_info->tracks[track_nr].t_title = tmp_tt;
                strcpy(disc_info->tracks[track_nr].t_title, value);
              }
            }
          }

          // set album title, if not already done
          if (strlen(disc_info->tracks[track_nr].t_album) == 0) {
            char *tmp_ta = realloc(disc_info->tracks[track_nr].t_album, (strlen(disc_info->d_title) + 1) * sizeof(char));
            if (tmp_ta != NULL) {
              disc_info->tracks[track_nr].t_album = tmp_ta;
              strcpy(disc_info->tracks[track_nr].t_album, disc_info->d_title);
            }
          }
          
          // set artist, if not already done
          if (strlen(disc_info->tracks[track_nr].t_artist) == 0) {
            char *tmp_tr = realloc(disc_info->tracks[track_nr].t_artist, (strlen(disc_info->d_artist) + 1) * sizeof(char));
            if (tmp_tr != NULL) {
              disc_info->tracks[track_nr].t_artist = tmp_tr;
              strcpy(disc_info->tracks[track_nr].t_artist, disc_info->d_artist);
            }
          }

          // set year, if not already done
          if (disc_info->tracks[track_nr].t_year == 0) {
            disc_info->tracks[track_nr].t_year = disc_info->d_year;
          }

          // set genre, if not already done
          if (strlen(disc_info->tracks[track_nr].t_genre) == 0) {
            char *tmp_tg = realloc(disc_info->tracks[track_nr].t_genre, (strlen(disc_info->d_genre) + 1) * sizeof(char));
            if (tmp_tg != NULL) {
              disc_info->tracks[track_nr].t_genre = tmp_tg;
              strcpy(disc_info->tracks[track_nr].t_genre, disc_info->d_genre);
            }
          }
        }
      } else {
        if (verbose) {
          cde_report(CDE_MSG_TYPE_ERROR, 
              "mb_parse_release_lookup_response: number of tracks in "
              "response (%d) does not match number of tracks on disc (%d)",
              track_cnt, disc_info->d_tracks);
        }
        res = 2;
      }
    }
    
    // set musicbrainz discid, if not already done
    // if set, the musicbrainz query and fuzzy lookup are reconstructed as well
    if (strlen(disc_info->mb_disc_id) == 0) {
      json_select_member(&token, "$..discs.[0].id", value, JSON_UTILS_MAX_TOKEN_STR_SIZE, data);

      // allocate new string and copy data
      size_t val_len = strlen(value);
      char *tmp = realloc(disc_info->mb_disc_id, (val_len + 1) * sizeof(char));
      if (tmp == NULL) {
          // handle memory allocation failure
          free(value);
          return 3;
      }
      disc_info->mb_disc_id = tmp;
      strcpy(disc_info->mb_disc_id, value);
    
      // reconstruct the musicbrainz query and fuzzy lookup
      json_select_member(&token, "$..discs.[0].offset-count", value, JSON_UTILS_MAX_TOKEN_STR_SIZE, data);
      int offset_cnt = 0;
      if (sscanf(value, "%d", &offset_cnt) == 1 && offset_cnt == disc_info->d_tracks) {
        
        char *tmp = calloc(32, sizeof(char));
        char *mb_query = calloc(2048, sizeof(char));
        char *mb_fuzzy = calloc(2048, sizeof(char));

        // set the length of disc in sectors
        int sectors = 0;
        json_select_member(&token, "$..discs.[0].sectors", value, JSON_UTILS_MAX_TOKEN_STR_SIZE, data);
        if (sscanf(value, "%d", &sectors) == 1 && sectors > 0) {
          disc_info->d_length = sectors;
        }

        // the musicbrainz query string starts with the first track
        sprintf(mb_query, "%d", 1);

        // the musicbrainz fuzzy toc lookup starts with the first track
        sprintf(mb_fuzzy, "%d", 1);

        // add the number of tracks to the query strings
        sprintf(tmp, "+%d", offset_cnt);
        strcat(mb_query, tmp);
        strcat(mb_fuzzy, tmp);

        // add the disc length in frames for the musicbrainz fuzzy toc lookup
        sprintf(tmp, "+%d", sectors);
        strcat(mb_fuzzy, tmp);

        // add frame offsets of all tracks
        int prev_offset, offset = 0;
        for (int i = 0; i < offset_cnt; i++) {
          snprintf(path, 40, "$..discs.[0].offsets.[%d]", i);
          json_select_member(&token, path, value, JSON_UTILS_MAX_TOKEN_STR_SIZE, data);
          prev_offset = offset;
          if (sscanf(value, "%d", &offset) == 1 && offset > 0) {
            // set length of track
            if (i > 1) {
              disc_info->tracks[i-1].t_length = offset - prev_offset;
            }
          }
          sprintf(tmp, "+%s", value);
          strcat(mb_query, tmp);
          strcat(mb_fuzzy, tmp);
        }
        // set length of last track
        if (sectors - offset > 0) {
          disc_info->tracks[offset_cnt-1].t_length = sectors - offset;
        }

        // add length of disc in frames for the musicbrainz query service
        sprintf(tmp, "+%d", sectors);
        strcat(mb_query, tmp);

        // set the musicbrainz query string
        disc_info->mb_query = realloc(disc_info->mb_query, (strlen(mb_query)+1) * sizeof(char));
        strcpy(disc_info->mb_query, mb_query);
        cde_report(CDE_MSG_TYPE_DEBUG, "reconstructed musicbrainz query:%s", disc_info->mb_query);

        // set the mb fuzzy toc lookup string
        disc_info->mb_fuzzy_lookup = realloc(disc_info->mb_fuzzy_lookup, (strlen(mb_fuzzy)+1) * sizeof(char));
        strcpy(disc_info->mb_fuzzy_lookup, mb_fuzzy);
        cde_report(CDE_MSG_TYPE_DEBUG, "reconstructed musicbrainz fuzzy lookup:%s", disc_info->mb_fuzzy_lookup);

        free(mb_fuzzy);
        free(mb_query);
        free(tmp);
      }
    }

    free(path);
  }
  
  free(value);
  return res;
}

/**
 * @brief query and read from the online musicbrainz api
 *        returned information will be parsed and stored in disc_info
 * @return 0 on success, >=1 on error
 */
int mb_get_disc_info(disc *disc_info, int query_method, int verbose) {
  int res = 0;
  char url[MB_MAX_URL_LENGTH];
  
  if (query_method == MB_QUERY_DISCID) {
    // prepare disc id lookup
    snprintf(url, MB_MAX_URL_LENGTH, MB_DISCID_LOOKUP, MB_API_ENDPOINT, disc_info->mb_disc_id, disc_info->mb_query);
  } else if (query_method == MB_QUERY_FUZZY) {
    // prepare fuzzy lookup
    snprintf(url, MB_MAX_URL_LENGTH, MB_DISCID_LOOKUP, MB_API_ENDPOINT, "-", disc_info->mb_fuzzy_lookup);
  } else {
    // remove any trailing specializations from the title between [] and ()
    char *filtered_title = calloc(strlen(disc_info->d_title) + 1, sizeof(char));
    for (int i=0; i<strlen(disc_info->d_title); i++) {
      if (disc_info->d_title[i] == '[' || disc_info->d_title[i] == '(') {
        // skip all characters until the matching closing bracket
        while (i < strlen(disc_info->d_title) && disc_info->d_title[i] != ']' && disc_info->d_title[i] != ')') {
          i++;
        }
      } else {
        strncat(filtered_title, &disc_info->d_title[i], 1);
      }
    }
    // url encode the artist and title
    char *encoded_title = url_encode(filtered_title);
    char *encoded_artist = url_encode(disc_info->d_artist);
    switch (query_method) {
      case MB_QUERY_RELEASE_FULL:
        if (disc_info->d_year > 0 && disc_info->d_tracks > 0) {
          // query by artist, release title, track count, release year and CD format
          snprintf(url, MB_MAX_URL_LENGTH, MB_RELEASE_QUERY_ARTYF, MB_API_ENDPOINT, encoded_artist, encoded_title, disc_info->d_tracks, disc_info->d_year);
        } else if (disc_info->d_tracks > 0) {
          // query by artist, release title, track count and CD format
          snprintf(url, MB_MAX_URL_LENGTH, MB_RELEASE_QUERY_ARTF, MB_API_ENDPOINT, encoded_artist, encoded_title, disc_info->d_tracks);
        } else {
          // query by artist, release title and CD format
          snprintf(url, MB_MAX_URL_LENGTH, MB_RELEASE_QUERY_ARF, MB_API_ENDPOINT, encoded_artist, encoded_title);
        }
        break;
      case MB_QUERY_RELEASE_PARTIAL:
        if (disc_info->d_tracks > 0) {
          // query by artist, release title, track count and CD format
          snprintf(url, MB_MAX_URL_LENGTH, MB_RELEASE_QUERY_ARTF, MB_API_ENDPOINT, encoded_artist, encoded_title, disc_info->d_tracks);
        } else {
          // query by artist, release title and CD format
          snprintf(url, MB_MAX_URL_LENGTH, MB_RELEASE_QUERY_ARF, MB_API_ENDPOINT, encoded_artist, encoded_title);
        }
        break;
      case MB_QUERY_RELEASE_LIMITED:
        // query by artist, release title and CD format
        snprintf(url, MB_MAX_URL_LENGTH, MB_RELEASE_QUERY_ARF, MB_API_ENDPOINT, encoded_artist, encoded_title);
        break;
      default:
        break;
    }
    free(filtered_title);
    free(encoded_title);
    free(encoded_artist);
  }

  char *diskid_response = calloc(1, sizeof(char));
  cde_report(CDE_MSG_TYPE_DEBUG, "mb_get_disc_info: calling: %s", url);
  if (http_get_data(url, &diskid_response, verbose) > 0) {
    if ((res = mb_parse_discid_lookup_response(disc_info, diskid_response, verbose)) == 0) {
      // prepare the release lookup. we have the release id as result from the disc id lookup
      char *lookup_response = calloc(1, sizeof(char));
      snprintf(url, MB_MAX_URL_LENGTH, MB_RELEASE_LOOKUP, MB_API_ENDPOINT, disc_info->mb_release_id);
      cde_report(CDE_MSG_TYPE_DEBUG, "mb_get_disc_info: calling: %s", url);
      if (http_get_data(url, &lookup_response, verbose) > 0) {
        res = mb_parse_release_lookup_response(disc_info, lookup_response, verbose);
      }
      free(lookup_response);
    }
  }

  free(diskid_response);
  return res;
}