/**************************************************************************

  libcdextract - json file reader / writer

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

#include <linux/limits.h>
#include <stdlib.h>
#include <stdio.h>

#include "config.h"
#include "report.h"
#include "string_utils.h"
#include "file_utils.h"
#include "json_utils.h"
#include "libcdextract_types.h"


/**
 * @brief writes the gathered disc information to a json file
 * @param disc_info the disc information structure
 * @param folder folder to store the file
 * @param overwrite overwrite the file if it exists
 * @param verbose print detailed output
 */
int json_write_disc_info(disc *disc_info, const char *folder, int overwrite, int verbose) {
  
  if (disc_info == NULL || folder == NULL) {
    return -2;
  }

  char *artist_folder = replace_chars(disc_info->d_artist, FILENAME_CHAR_FILTER, '-');
  char *album_folder = replace_chars(disc_info->d_title, FILENAME_CHAR_FILTER, '-');

  int json_file_len = strlen(folder) + strlen(artist_folder) + strlen(album_folder) + 8;
  char *json_file = calloc(json_file_len, sizeof(char));
  snprintf(json_file, json_file_len, "%s/%s-%s.json", folder, artist_folder, album_folder);
  
  struct stat st = {0};
  if (overwrite == 0 && stat(json_file, &st) != -1) {
    if (verbose) {
      cde_report(CDE_MSG_TYPE_INFO, "not overwriting disc information: %s", json_file);
    }
    free(album_folder);
    free(artist_folder);
    free(json_file);
    return 0;
  }
  
  if (verbose) {
    cde_report(CDE_MSG_TYPE_INFO, "writing disc information: %s", json_file);
  }

  FILE *f = fopen(json_file, "w");
  if (f == NULL) {
    free(album_folder);
    free(artist_folder);
    free(json_file);
    return -1;
  }

  // disc information
  fprintf(f, "{\n");
  // note: we discard the database id 'id'
  //fprintf(f, "  \"id\":%lu,\n", disc_info->db_id);
  fprintf(f, "  \"disc_id\":\"%08x\",\n", disc_info->d_id);
  fprintf(f, "  \"length\":%d,\n", disc_info->d_length);
  fprintf(f, "  \"lookup\":\"%016lx\",\n", disc_info->d_lookup);
  fprintf(f, "  \"artist\":\"%s\",\n", disc_info->d_artist);
  fprintf(f, "  \"title\":\"%s\",\n", disc_info->d_title);
  fprintf(f, "  \"genre\":\"%s\",\n", disc_info->d_genre);
  fprintf(f, "  \"year\":%d,\n", disc_info->d_year);
  fprintf(f, "  \"extended\":\"%s\",\n", disc_info->d_extended);
  fprintf(f, "  \"cddb_query\":\"%s\",\n", disc_info->cddb_query);
  fprintf(f, "  \"cddb_category\":\"%s\",\n", disc_info->cddb_category);
  fprintf(f, "  \"cddb_entry_id\":\"%08x\",\n", disc_info->cddb_e_id);
  fprintf(f, "  \"cddb_disc_id\":\"%08x\",\n", disc_info->cddb_d_id);
  fprintf(f, "  \"cddb_revision\":%d,\n", disc_info->cddb_revision);
  fprintf(f, "  \"cddb_complete\":%d,\n", disc_info->cddb_complete);
  fprintf(f, "  \"mb_query\":\"%s\",\n", disc_info->mb_query);
  fprintf(f, "  \"mb_fuzzy_lookup\":\"%s\",\n", disc_info->mb_fuzzy_lookup);
  fprintf(f, "  \"mb_disc_id\":\"%s\",\n", disc_info->mb_disc_id);
  fprintf(f, "  \"mb_release_id\":\"%s\",\n", disc_info->mb_release_id);
  // add cover information
  fprintf(f, "  \"mb_front_cover_size\":%d,\n", disc_info->mb_front_cover_size);
  if (disc_info->mb_front_cover_size > 0) {
    // ensure the image is stored on disk if not already present or overwrite is requested
    if (overwrite != 1 || stat(json_file, &st) == -1) {
      int cover_front_len = snprintf(NULL, 0, "%s/%s", folder, CDE_COVER_FRONT);
      char *cover_front_file = malloc((cover_front_len + 1) * sizeof(char));
      sprintf(cover_front_file, "%s/%s", folder, CDE_COVER_FRONT);
      if (write_file(disc_info->mb_front_cover, disc_info->mb_front_cover_size, cover_front_file) == -1) {
        cde_report(CDE_MSG_TYPE_ERROR, "unable to write front cover: %s", cover_front_file);
      }
      free(cover_front_file);
    }
    fprintf(f, "  \"mb_front_cover\":\"%s/%s/%s\",\n", artist_folder, album_folder, CDE_COVER_FRONT);
  } else {
    fprintf(f, "  \"mb_front_cover\":\"\",\n");
  }
  fprintf(f, "  \"mb_back_cover_size\":%d,\n", disc_info->mb_back_cover_size);
  if (disc_info->mb_back_cover_size > 0) {
    // ensure the image is stored on disk if not already present or overwrite is requested
    if (overwrite != 1 || stat(json_file, &st) == -1) {
      int cover_back_len = snprintf(NULL, 0, "%s/%s", folder, CDE_COVER_BACK);
      char *cover_back_file = malloc((cover_back_len + 1) * sizeof(char));
      sprintf(cover_back_file, "%s/%s", folder, CDE_COVER_BACK);
      if (write_file(disc_info->mb_back_cover, disc_info->mb_back_cover_size, cover_back_file) == -1) {
        cde_report(CDE_MSG_TYPE_ERROR, "unable to write back cover: %s", cover_back_file);
      }
      free(cover_back_file);
    }
    fprintf(f, "  \"mb_back_cover\":\"%s/%s/%s\",\n", artist_folder, album_folder, CDE_COVER_BACK);
  } else {
    fprintf(f, "  \"mb_back_cover\":\"\",\n");
  }
  fprintf(f, "  \"mb_complete\":%d,\n", disc_info->mb_complete);
  fprintf(f, "  \"extracted\":%d,\n", disc_info->d_extracted);
  fprintf(f, "  \"track_count\":%d,\n", disc_info->d_tracks);
  fprintf(f, "    \"tracks\": [\n");
  // add track information
  for (int i = 0; i < disc_info->d_tracks; i++) {
    fprintf(f, "      {\n");
    fprintf(f, "        \"num\":%d,\n", disc_info->tracks[i].t_num);
    fprintf(f, "        \"length\":%d,\n", disc_info->tracks[i].t_length);
    fprintf(f, "        \"title\":\"%s\",\n", disc_info->tracks[i].t_title);
    fprintf(f, "        \"artist\":\"%s\",\n", disc_info->tracks[i].t_artist);
    fprintf(f, "        \"album\":\"%s\",\n", disc_info->tracks[i].t_album);
    fprintf(f, "        \"genre\":\"%s\",\n", disc_info->tracks[i].t_genre);
    fprintf(f, "        \"year\":%d,\n", disc_info->tracks[i].t_year);
    fprintf(f, "        \"extended\":\"%s\",\n", disc_info->tracks[i].t_extended);
    fprintf(f, "        \"filename\":\"%s\",\n", disc_info->tracks[i].t_filename);
    fprintf(f, "        \"skipped\":%d\n", disc_info->tracks[i].t_skipped);
    if (i < disc_info->d_tracks - 1) {
      fprintf(f, "      },\n");
    } else {
      fprintf(f, "      }\n");
    }
  }
  fprintf(f, "  ]\n");
  fprintf(f, "}\n");
  
  // cleanup
  free(album_folder);
  free(artist_folder);
  free(json_file);
  return fclose(f);
}

/**
 * @brief reads the disc information from the specified json file
 * @param disc_info the disc information structure
 * @param file_path path to the json file
 * @param verbose print detailed output
 * @return 0 on success, negative value on error
 */
int json_read_disc_info(disc *disc_info, const char *file_path, int verbose) {
  
  // read file into memory
  FILE *f = fopen(file_path, "r");
  if (f == NULL) {
    return -1;
  }
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *data = malloc(size + 1);
  if (data == NULL) {
    fclose(f);
    return -1;
  }
  fread(data, 1, size, f);
  data[size] = '\0'; // null-terminate the string
  fclose(f);

  // parse json data
  int res = 0;
  json_token token;
  if (size > 0 && json_init(&token, data, size) == json_ok) {

    if (disc_info == NULL) {
      return -2;
    }

    disc_info->d_id = 0;
    disc_info->d_year = 0;
    disc_info->d_tracks = 0;
    char *value = calloc(JSON_UTILS_MAX_TOKEN_STR_SIZE, sizeof(char));
    char *path = calloc(41, sizeof(char));

    // get disc information
    // note: we discard the database id 'id'
    if (json_select_member(&token, "$.disc_id", value, JSON_UTILS_MAX_TOKEN_STR_SIZE, data) > 0) {
      
      if (sscanf(value, "%8x", &(disc_info->d_id)) != 1) {
        res = 2; // invalid disc id
      }
      if (res == 0) {
        json_get_integer(&token, "length", &(disc_info->d_length), data);
        if (disc_info->d_length <= 0) {
          res = 3; // invalid disc length
        }
      }
      if (res == 0) {
        json_get_hex64(&token, "lookup", &(disc_info->d_lookup), data);
        disc_info->d_artist = calloc(64, sizeof(char));
        json_get_string(&token, "artist", &(disc_info->d_artist), data);
        disc_info->d_title = calloc(64, sizeof(char));
        json_get_string(&token, "title", &(disc_info->d_title), data);
        disc_info->d_genre = calloc(16, sizeof(char));
        json_get_string(&token, "genre", &(disc_info->d_genre), data);
        json_get_integer(&token, "year", &(disc_info->d_year), data);
        disc_info->d_extended = calloc(64, sizeof(char));
        json_get_string(&token, "extended", &(disc_info->d_extended), data);
        disc_info->cddb_query = calloc(256, sizeof(char));
        json_get_string(&token, "cddb_query", &(disc_info->cddb_query), data);
        disc_info->cddb_category = calloc(16, sizeof(char));
        json_get_string(&token, "cddb_category", &(disc_info->cddb_category), data);
        json_get_hex32(&token, "cddb_entry_id", &(disc_info->cddb_e_id), data);
        json_get_hex32(&token, "cddb_disc_id", &(disc_info->cddb_d_id), data);
        json_get_integer(&token, "cddb_revision", &(disc_info->cddb_revision), data);
        json_get_integer(&token, "cddb_complete", &(disc_info->cddb_complete), data);
        disc_info->mb_query = calloc(256, sizeof(char));
        json_get_string(&token, "mb_query", &(disc_info->mb_query), data);
        disc_info->mb_fuzzy_lookup = calloc(256, sizeof(char));
        json_get_string(&token, "mb_fuzzy_lookup", &(disc_info->mb_fuzzy_lookup), data);
        disc_info->mb_disc_id = calloc(64, sizeof(char));
        json_get_string(&token, "mb_disc_id", &(disc_info->mb_disc_id), data);
        disc_info->mb_release_id = calloc(64, sizeof(char));
        json_get_string(&token, "mb_release_id", &(disc_info->mb_release_id), data);
        // load front cover from the specified file
        json_get_integer(&token, "mb_front_cover_size", &(disc_info->mb_front_cover_size), data);  
        char *front_cover_file = calloc(256, sizeof(char));
        json_get_string(&token, "mb_front_cover", &front_cover_file, data);
        if (strlen(front_cover_file) > 0) {
          // determine full path of the front cover

          // front_cover_file: a2/b2/cover.jpg
          // file_path: x/y/z/a1/b1/a-b.json
          // full path: x/y/z/a1/b1/cover.jpg

          // determine cover folder without trailing slash
          char *fpp = strrchr(file_path, '/');
          int fpp_len = strlen(file_path) - strlen(fpp);
          char *fc_p = calloc(fpp_len + 1, sizeof(char));
          strncpy(fc_p, file_path, fpp_len);

          // add the cover filename
          char *fcp = strrchr(front_cover_file, '/');
          char *fc_full_path;
          if (fcp != NULL) {
            // note: increase fcp to discard leading '/' from the cover filename
            int fc_full_len = snprintf(NULL, 0, "%s/%s", fc_p, fcp + 1);
            fc_full_path = malloc((fc_full_len + 1) * sizeof(char));
            sprintf(fc_full_path, "%s/%s", fc_p, fcp + 1);
          } else {
            fc_full_path = malloc((fpp_len + strlen(CDE_COVER_FRONT) + 2) * sizeof(char));
            sprintf(fc_full_path, "%s/%s", fc_p, CDE_COVER_FRONT);
          }

          long front_cover_size = read_file(&(disc_info->mb_front_cover), fc_full_path);
          if (front_cover_size >= 0) {
            if (front_cover_size != disc_info->mb_front_cover_size && disc_info->mb_front_cover_size > 0) {
              cde_report(CDE_MSG_TYPE_WARNING, "front cover: %s has different size: %d", fc_full_path, front_cover_size);
            }
            disc_info->mb_front_cover_size = (int)front_cover_size; // ensure correct size
          } else {
            cde_report(CDE_MSG_TYPE_ERROR, "unable to read front cover: %s", fc_full_path);
          }

          free(fc_full_path);
          free(fc_p);
        }
        free(front_cover_file);
        // load front cover from the specified file
        json_get_integer(&token, "mb_back_cover_size", &(disc_info->mb_back_cover_size), data);
        char *back_cover_file = calloc(256, sizeof(char));
        json_get_string(&token, "mb_back_cover", &back_cover_file, data);
        if (strlen(back_cover_file) > 0) {
          // determine full path of the back cover
          
          // determine cover folder without trailing slash
          char *fpp = strrchr(file_path, '/');
          int fpp_len = strlen(file_path) - strlen(fpp);
          char *bc_p = calloc(fpp_len + 1, sizeof(char));
          strncpy(bc_p, file_path, fpp_len);

          // add the cover filename
          char *bcp = strrchr(back_cover_file, '/');
          char *bc_full_path;
          if (bcp != NULL) {
            // note: increase bcp to discard leading '/' from the cover filename
            int bc_full_len = snprintf(NULL, 0, "%s/%s", bc_p, bcp + 1);
            bc_full_path = malloc((bc_full_len + 1) * sizeof(char));
            sprintf(bc_full_path, "%s/%s", bc_p, bcp + 1);
          } else {
            bc_full_path = malloc((fpp_len + strlen(CDE_COVER_BACK) + 2) * sizeof(char));
            sprintf(bc_full_path, "%s/%s", bc_p, CDE_COVER_BACK);
          }

          long back_cover_size = read_file(&(disc_info->mb_back_cover), bc_full_path);
          if (back_cover_size >= 0) {
            if (back_cover_size != disc_info->mb_back_cover_size && disc_info->mb_back_cover_size > 0) {
              cde_report(CDE_MSG_TYPE_WARNING, "back cover: %s has different size: %d", bc_full_path, back_cover_size);
            }
            disc_info->mb_back_cover_size = (int)back_cover_size; // ensure correct size
          } else {
            cde_report(CDE_MSG_TYPE_ERROR, "unable to read back cover: %s", bc_full_path);
          }

          free(bc_full_path);
          free(bc_p);
        }
        free(back_cover_file);
        json_get_integer(&token, "mb_complete", &(disc_info->mb_complete), data);
        if (json_get_integer(&token, "extracted", &(disc_info->d_extracted), data) == 0) {
          // backwards compatibility: if not present we assume the disc has been extracted
          disc_info->d_extracted = 1;
          // select previous element
          json_select_member(&token, "$.mb_complete", value, JSON_UTILS_MAX_TOKEN_STR_SIZE, data);
        }
        json_get_integer(&token, "track_count", &(disc_info->d_tracks), data);
        if (disc_info->d_tracks > 0 && disc_info->d_tracks < 100) {
          disc_info->tracks = calloc(disc_info->d_tracks, sizeof(track));

          // get track information
          for (int i=0; i < disc_info->d_tracks; i++) {

            snprintf(path, 40, "$.tracks.[%d].num", i);
            if (json_select_member(&token, path, value, JSON_UTILS_MAX_TOKEN_STR_SIZE, data) > 0) {

              disc_info->tracks[i].t_num = 0;
              if (sscanf(value, "%d", &(disc_info->tracks[i].t_num)) != 1) {
                res = 4; // invalid track number
                break;
              }
              if (disc_info->tracks[i].t_num < 1 || disc_info->tracks[i].t_num > disc_info->d_tracks) {
                res = 5; // invalid track number
                break;
              }

              json_get_integer(&token, "length", &(disc_info->tracks[i].t_length), data);
              if (disc_info->tracks[i].t_length < 0 || disc_info->tracks[i].t_length > disc_info->d_length) {
                res = 6; // invalid track length
                break;
              }
              disc_info->tracks[i].t_title = calloc(64, sizeof(char));
              json_get_string(&token, "title", &(disc_info->tracks[i].t_title), data);
              disc_info->tracks[i].t_artist = calloc(64, sizeof(char));
              json_get_string(&token, "artist", &(disc_info->tracks[i].t_artist), data);
              disc_info->tracks[i].t_album = calloc(64, sizeof(char));
              json_get_string(&token, "album", &(disc_info->tracks[i].t_album), data);
              disc_info->tracks[i].t_genre = calloc(16, sizeof(char));
              json_get_string(&token, "genre", &(disc_info->tracks[i].t_genre), data);
              disc_info->tracks[i].t_year = disc_info->d_year;
              json_get_integer(&token, "year", &(disc_info->tracks[i].t_year), data);
              disc_info->tracks[i].t_extended = calloc(64, sizeof(char));
              json_get_string(&token, "extended", &(disc_info->tracks[i].t_extended), data);
              disc_info->tracks[i].t_filename = calloc(PATH_MAX+1, sizeof(char));
              json_get_string(&token, "filename", &(disc_info->tracks[i].t_filename), data);
              if (json_get_integer(&token, "skipped", &(disc_info->tracks[i].t_skipped), data) != 1) {
                disc_info->tracks[i].t_skipped = 0;
              }
            }
          } // end for tracks
        } else {
          res = 7; // invalid number of tracks
        }
      }
    } else {   
      res = 1; // no disc id
    }
    free(value);
    free(path);
  }

  free(data);
  return res;
}