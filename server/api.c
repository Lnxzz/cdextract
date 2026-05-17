/**************************************************************************

  cdextract - server API functions

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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <linux/limits.h>

#include "libcdextract.h"
#include "libcdextract_types.h"
#include "string_utils.h"
#include "file_utils.h"
#include "json_utils.h"
#include "json_file.h"
#include "wav_writer.h"
#include "wav_reader.h"
#include "flac_writer.h"
#include "flac_reader.h"
#include "report.h"
#include "timer.h"
#include "cue_sheet.h"
#include "cddb.h"
#include "mb_api.h"
#include "db.h"
#include "scan.h"
#include "api.h"


#define MAX_DRIVE_RESPONSE 4096                       // maximum size of a json formatted drive response object
#define MAX_DISC_LIST_RESPONSE 262144                 // maximum size of a json formatted disc response object
#define MAX_DISC_RESPONSE 65536                       // maximum size of a json formatted disc response object
#define MAX_TRACK_RESPONSE 8192                       // maximum size of a json formatted disc response object
#define MAX_STATUS_RESPONSE 256                       // maximum size of a json formatted status response object
#define MAX_BLOCK_RESPONSE 524288                     // maximum size of a data block to be returned by the streaming callback (512kB or ~3 seconds of audio data)
#define SIZE_UNKNOWN_RESPONSE 0xFFFFFFFFFFFFFFFFULL   // use this value to indicate that the size of the data is unknown (MHD_SIZE_UNKNOWN)
#define DEFAULT_FRONT_COVER_FILENAME "cover.png"      // name of default front cover image to return when cover is not found
#define DEFAULT_BACK_COVER_FILENAME "cover-back.png"  // name of default back cover image to return when cover is not found


/* default error response messages */
const char *json_response_400 = "{\"code\": 400, \"message\": \"Bad Request\"}";
const char *json_response_403 = "{\"code\": 403, \"message\": \"Forbidden\"}";
const char *json_response_404 = "{\"code\": 404, \"message\": \"Not Found\"}";
const char *json_response_405 = "{\"code\": 405, \"message\": \"Method Not Allowed\"}";
const char *json_response_409 = "{\"code\": 409, \"message\": \"Conflict. Resource state conflict\"}";
const char *json_response_423 = "{\"code\": 423, \"message\": \"Locked. Resource is locked\"}";
const char *json_response_500 = "{\"code\": 500, \"message\": \"Internal Server Error\"}";
const char *json_response_501 = "{\"code\": 501, \"message\": \"Not Implemented\"}";
const char *json_response_503 = "{\"code\": 503, \"message\": \"Service Unavailable\"}";


/**
 * @brief lookup table containing all supported endpoints
 *        request methods and the associated function calls.
 */
const struct operation_path operations[] = {
    {"/v1/drive", {{GET, api_get_drive_info}, {POST, api_open_drive}, {DELETE, api_close_drive}, {INVALID, api_default_response}}},
    {"/v1/disc", {{GET, api_get_disc_info}, {POST, api_update_disc_info}, {PUT, api_insert_disc}, {DELETE, api_eject_disc}}},
    {"/v1/disc/extract", {{GET, api_get_extract_disc_progress}, {POST, api_extract_disc}, {DELETE, api_cancel_disc}, {INVALID, api_default_response}}},
    {"/v1/disc/front", {{GET, api_get_disc_front_cover}, {POST, api_update_disc_front_cover}, {INVALID, api_default_response}, {INVALID, api_default_response}}},
    {"/v1/disc/back", {{GET, api_get_disc_back_cover}, {POST, api_update_disc_back_cover}, {INVALID, api_default_response}, {INVALID, api_default_response}}},
    {"/v1/disc/audio", {{GET, api_get_audio}, {INVALID, api_default_response}, {INVALID, api_default_response}, {INVALID, api_default_response}}},
    {"/v1/discs", {{GET, api_list_discs}, {INVALID, api_default_response}, {INVALID, api_default_response}, {INVALID, api_default_response}}},
    {"/v1/discs/rescan", {{GET, api_rescan_status}, {POST, api_rescan_discs}, {INVALID, api_default_response}, {INVALID, api_default_response}}},
    {"/v1/discs/rebuild", {{GET, api_rebuild_status}, {POST, api_rebuild_discs}, {INVALID, api_default_response}, {INVALID, api_default_response}}},
    {"/v1/discs/backup", {{GET, api_backup_status}, {POST, api_backup_discs}, {INVALID, api_default_response}, {INVALID, api_default_response}}},
    {"/v1/discs/*/front", {{GET, api_get_disc_front_cover_by_id}, {POST, api_update_disc_front_cover_by_id}, {INVALID, api_default_response}, {INVALID, api_default_response}}},
    {"/v1/discs/*/back", {{GET, api_get_disc_back_cover_by_id}, {POST, api_update_disc_back_cover_by_id}, {INVALID, api_default_response}, {INVALID, api_default_response}}},
    {"/v1/discs/*/audio", {{GET, api_get_audio_by_id}, {INVALID, api_default_response}, {INVALID, api_default_response}, {INVALID, api_default_response}}},
    {"/v1/discs/*", {{GET, api_get_disc_by_id}, {POST, api_update_disc_info_by_id}, {INVALID, api_default_response}, {INVALID, api_default_response}}},
    {"/", {{GET, api_file_response}, {INVALID, api_default_response}, {INVALID, api_default_response}, {INVALID, api_default_response}}},
    {"/index.html", {{GET, api_file_response}, {INVALID, api_default_response}, {INVALID, api_default_response}, {INVALID, api_default_response}}},
    {"/favicon.ico", {{GET, api_file_response}, {INVALID, api_default_response}, {INVALID, api_default_response}, {INVALID, api_default_response}}}
};


// cdextract state structure which references the drive, disc and tracks
cde_state *cde = NULL;

// database state structure
sql_db *db = NULL;

// structure to handle the extraction progress state
progress_state progress;


//
// internal functions
//

/**
 * @brief set the status message response body as json 
 *        from the current database mode
 */
void set_body_from_database_mode(http_response *response) {
  char *json_response = calloc(MAX_STATUS_RESPONSE+1, sizeof(char));
  if (db->mode == DB_BACKUP) {
    snprintf(json_response, MAX_STATUS_RESPONSE, "{\"status\": \"backup\", \"message\": \"Database backup in progress\"}");
  } else if (db->mode == DB_NORMAL) {
    snprintf(json_response, MAX_STATUS_RESPONSE, "{\"status\": \"normal\", \"message\": \"Database in normal operation mode\"}");
  } else if (db->mode == DB_RESCAN) {
    snprintf(json_response, MAX_STATUS_RESPONSE, "{\"status\": \"rescan\", \"message\": \"Database rescan in progress\"}");
  } else {
    snprintf(json_response, MAX_STATUS_RESPONSE, "{\"status\": \"closed\", \"message\": \"No database connection available\"}");
  }
  response->code = OK;
  response->mime_type = MIME_TYPE_JSON;
  response->content_type = get_content_type(response->mime_type);
  response->size = strlen(json_response);
  response->body = calloc(response->size+1, sizeof(char));
  strcpy(response->body, json_response);
  free(json_response);
}

/**
 * @brief set the drive information response body as json f
 *        rom the given drive information
 */
void set_body_from_drive_info(cde_state *cde, http_response *response) {
  char *json_response = calloc(MAX_DRIVE_RESPONSE+1, sizeof(char));
  snprintf(json_response, MAX_DRIVE_RESPONSE,
           "{\"device\": \"%s\", \"status\": %d, \"root\": \"%s\", \"folder\": \"%s\", \"verbose\": %d, \"output_type\": %d, \"download_coverart\": %d, \"search_drive\": %d, \"cd_speed\": %d," \
           " \"max_retries\": %d, \"abort_on_skip\": %d, \"eject_when_done\": %d, \"write_json\": %d, \"write_cue_sheet\": %d, \"show_disc_info\": %d, \"virtual_drive\": %d, \"has_drive\": %d, \"opened\": %d}",
           cde->cdrom_device,
           cde->status,
           cde->root_folder,
           cde->folder,
           cde->verbose,
           cde->output_type,
           cde->download_coverart,
           cde->search_drive,
           cde->cd_speed,
           cde->max_retries,
           cde->abort_on_skip,
           cde->eject_when_done,
           cde->write_json,
           cde->write_cue_sheet,
           cde->show_disc_info,
           cde->virtual_drive,
           cde->drv == NULL ? 0 : 1,
           cde->drv == NULL ? 0 : cde->drv->opened
           );
  response->code = OK;
  response->mime_type = MIME_TYPE_JSON;
  response->content_type = get_content_type(response->mime_type);
  response->size = strlen(json_response);
  response->body = calloc(response->size+1, sizeof(char));
  strcpy(response->body, json_response);
  free(json_response);
}

/**
 * @brief set the disc information response body as json 
 *        from the given disc information
 * @param disc_info
 * @param response 
 */
void set_body_from_disc_info(disc *disc_info, http_response *response) {
    char *json_response = calloc(MAX_DISC_RESPONSE+1, sizeof(char));
    char *json_tracks = calloc(MAX_DISC_RESPONSE+1, sizeof(char));
    char *json_track = calloc(MAX_TRACK_RESPONSE+1, sizeof(char));
    for (int i=0; i<disc_info->d_tracks; i++) {
      snprintf(json_track, MAX_TRACK_RESPONSE,
              ",{\"num\": %d, \"length\": %d, \"title\": \"%s\", \"artist\": \"%s\", \"album\": \"%s\", \"genre\": \"%s\", \"year\": %d, \"extended\": \"%s\", \"filename\": \"%s\", \"skipped\":%d}",
              disc_info->tracks[i].t_num,
              disc_info->tracks[i].t_length,
              disc_info->tracks[i].t_title,
              disc_info->tracks[i].t_artist,
              disc_info->tracks[i].t_album,
              disc_info->tracks[i].t_genre,
              disc_info->tracks[i].t_year,
              disc_info->tracks[i].t_extended,
              disc_info->tracks[i].t_filename,
              disc_info->tracks[i].t_skipped);
      if (i==0) {
        strcpy(json_tracks, &json_track[1]);
      } else {
        strcat(json_tracks, json_track);
      }
    }
    snprintf(json_response, MAX_DISC_RESPONSE,
              "{\"id\": %lu, \"disc_id\": \"%08x\", \"length\": %d, \"lookup\": \"%016lx\", \"artist\": \"%s\", \"title\": \"%s\", \"genre\": \"%s\", \"year\": %d, \"extended\": \"%s\"," \
              " \"cddb_query\": \"%s\", \"cddb_category\": \"%s\", \"cddb_entry_id\": \"%08x\", \"cddb_disc_id\": \"%08x\", \"cddb_revision\":%d, \"cddb_complete\": %d," \
              " \"mb_query\": \"%s\", \"mb_fuzzy_lookup\": \"%s\", \"mb_disc_id\": \"%s\", \"mb_release_id\": \"%s\"," \
              " \"mb_front_cover_size\": %d, \"mb_back_cover_size\": %d, \"mb_complete\": %d, \"extracted\": %d, \"track_count\": %d, \"tracks\": [%s]}",
              disc_info->db_id,          
              disc_info->d_id,
              disc_info->d_length,
              disc_info->d_lookup, 
              disc_info->d_artist,
              disc_info->d_title,
              disc_info->d_genre,
              disc_info->d_year,
              disc_info->d_extended,
              disc_info->cddb_query,
              disc_info->cddb_category,
              disc_info->cddb_e_id,
              disc_info->cddb_d_id,
              disc_info->cddb_revision,
              disc_info->cddb_complete,
              disc_info->mb_query,
              disc_info->mb_fuzzy_lookup,
              disc_info->mb_disc_id,
              disc_info->mb_release_id,
              disc_info->mb_front_cover_size,
              disc_info->mb_back_cover_size,
              disc_info->mb_complete,
              disc_info->d_extracted,
              disc_info->d_tracks,
              json_tracks);
    response->code = OK;
    response->mime_type = MIME_TYPE_JSON;
    response->content_type = get_content_type(response->mime_type);
    response->size = strlen(json_response);
    response->body = calloc(response->size+1, sizeof(char));
    strcpy(response->body, json_response);
    free(json_track);
    free(json_tracks);
    free(json_response);
}

/**
 * @brief set the response body as json from the given disc list information
 * @param disc_info_list 
 * @param response 
 */
void set_body_from_disc_list(disc_list **disc_info_list, http_response *response) {
    char *json_response = calloc(MAX_DISC_LIST_RESPONSE+1, sizeof(char));
    char *json_disc = calloc(MAX_DISC_RESPONSE+1, sizeof(char));
    disc *disc_info = pop_disc_list(disc_info_list);
    int cnt = 0;
    while (disc_info != NULL) {
      snprintf(json_disc, MAX_DISC_RESPONSE,
        ",{\"id\": %lu, \"disc_id\": \"%08x\", \"length\": %d, \"lookup\": \"%016lx\", \"artist\": \"%s\", \"title\": \"%s\", \"genre\": \"%s\", \"year\": %d, \"extended\": \"%s\"," \
        " \"cddb_query\": \"%s\", \"cddb_category\": \"%s\", \"cddb_entry_id\": \"%08x\", \"cddb_disc_id\": \"%08x\", \"cddb_revision\":%d, \"cddb_complete\": %d," \
        " \"mb_query\": \"%s\", \"mb_fuzzy_lookup\": \"%s\", \"mb_disc_id\": \"%s\", \"mb_release_id\": \"%s\"," \
        " \"mb_front_cover_size\": %d, \"mb_back_cover_size\": %d, \"mb_complete\": %d, \"extracted\": %d, \"track_count\": %d}",
        disc_info->db_id,
        disc_info->d_id, 
        disc_info->d_length,
        disc_info->d_lookup,
        disc_info->d_artist,
        disc_info->d_title,
        disc_info->d_genre,
        disc_info->d_year,
        disc_info->d_extended,
        disc_info->cddb_query,
        disc_info->cddb_category,
        disc_info->cddb_e_id,
        disc_info->cddb_d_id,
        disc_info->cddb_revision,
        disc_info->cddb_complete,
        disc_info->mb_query,
        disc_info->mb_fuzzy_lookup,
        disc_info->mb_disc_id,
        disc_info->mb_release_id,
        disc_info->mb_front_cover_size,
        disc_info->mb_back_cover_size,
        disc_info->mb_complete,
        disc_info->d_extracted,
        disc_info->d_tracks);
      strcat(json_response, json_disc);
      cde_free_disc(&disc_info, -1);
      disc_info = pop_disc_list(disc_info_list);
      cnt++;
    }
    if (cnt==0) {
      sprintf(json_response, "[]");
    } else {
      strcat(json_response, "]");
      json_response[0] = '[';
    }
    response->code = OK;
    response->mime_type = MIME_TYPE_JSON;
    response->content_type = get_content_type(response->mime_type);
    response->size = strlen(json_response);
    response->body = calloc(response->size+1, sizeof(char));
    strcpy(response->body, json_response);
    free(json_disc);
    free(json_response);
}

/**
 * @brief check if both disc information structures match a cddb entry on
 *        entry id, disc id, artist, title, genre, year, length, revision and track count
 * @param disc_a 
 * @param disc_b 
 * @return 1 if they match, 0 otherwise
 */
int match_cddb_disc_info(disc *disc_a, disc *disc_b) {
  if (disc_a == NULL || disc_b == NULL) {
    return 0;
  }
  return (
    strcmp(disc_a->d_artist, disc_b->d_artist) == 0 &&
    strcmp(disc_a->d_title, disc_b->d_title) == 0 &&
    strcmp(disc_a->d_genre, disc_b->d_genre) == 0 &&
    disc_a->d_year == disc_b->d_year &&
    strcmp(disc_a->cddb_category, disc_b->cddb_category) == 0 &&
    disc_a->cddb_e_id == disc_b->cddb_e_id &&
    disc_a->cddb_d_id == disc_b->cddb_d_id &&
    disc_a->cddb_revision == disc_b->cddb_revision &&
    disc_a->d_tracks == disc_b->d_tracks);
}

/**
 * @brief rescan the filesystem and update the sqlite3 database
 * @param request_ptr pointer to the request structure
 * @return 0 if successful; another value indicates an error
 */
void *rescan_database_t(void *request_ptr) {
  if (cde==NULL || db==NULL || db->mode != DB_NORMAL) {
    return (void*) (size_t)DB_ERROR;
  }
  
  // set database operation mode back to rescan
  db->mode = DB_RESCAN;

  // get the request pointer and cast it to http_request
  http_request *request = (http_request *)request_ptr; 

  // purge all disc entries from the database when requested
  int res = DB_OK;
  if (request->purge == 1) {
    res = purge_discs_from_database(db);
    if (res != DB_OK) {
      cde_report(CDE_MSG_TYPE_ERROR, "rebuild_database: failed to purge disc information during database rebuild: %d", res);
    }
  }

  // rescan the filesystem and update the database
  scan_audio_folder(cde->root_folder, db, cde->download_coverart, cde->write_json, cde->verbose);

  // set database operation mode back to normal
  db->mode = DB_NORMAL;

  // terminate thread
  if (request->terminate_thread != 0) {
    pthread_exit(NULL);
  }
  return (void*) (size_t)res;
}

/**
 * @brief rescan the filesystem with cddb and extracted audio files to update the sqlite3 database
 * @param request_ptr pointer to the request structure
 * @return 0 if successful; another value indicates an error
 */
void *rebuild_database_t(void *request_ptr) {
  if (cde==NULL || request_ptr==NULL || db==NULL || db->mode != DB_NORMAL) {
    return (void*) (size_t)DB_ERROR;
  }

  // set database operation mode back to rescan
  db->mode = DB_REBUILD;

  // get the request pointer and cast it to http_request
  http_request *request = (http_request *)request_ptr;

  int res = rebuild_cddb_pre(db);
  if (res != DB_OK) {
    cde_report(CDE_MSG_TYPE_ERROR, "rebuild_database: failed to prepare the database for rebuild: %d", res);
    goto rebuild_finalize;
  }

  // rescan the filesystem for cddb files and update the database
  scan_cddb_folder(cde->cddb_folder, db);

  res = rebuild_cddb_post(db);
  if (res != DB_OK) {
    cde_report(CDE_MSG_TYPE_ERROR, "rebuild_database: failed to finish the database rebuild: %d", res);
    goto rebuild_finalize;
  }

  // purge all disc entries from the database when requested
  if (request->purge == 1) {
    res = purge_discs_from_database(db);
    if (res != DB_OK) {
      cde_report(CDE_MSG_TYPE_ERROR, "rebuild_database: failed to purge disc information during database rebuild: %d", res);
      goto rebuild_finalize;
    }
  }

  // rescan the filesystem for extracted audio files and update the database
  scan_audio_folder(cde->root_folder, db, cde->download_coverart, cde->write_json, cde->verbose);
  
rebuild_finalize:
  // set database operation mode back to normal
  db->mode = DB_NORMAL;

  // terminate thread
  if (request->terminate_thread != 0) {
    pthread_exit(NULL);
  }
  return (void*) (size_t)res;
}

/**
 * @brief callback for handling error, warning, info and debug messages
 */
static void api_report_callback(int rpt_type, char *rpt_msg) {
  char msg_type[10];
  switch (rpt_type) {
  case CDE_MSG_TYPE_ERROR:
    strcpy(msg_type, "ERROR");
    break;
  case CDE_MSG_TYPE_WARNING:
    strcpy(msg_type, "WARNING");
    break;
  case CDE_MSG_TYPE_INFO:
    strcpy(msg_type, "INFO");
    break;
  case CDE_MSG_TYPE_DEBUG:
    strcpy(msg_type, "DEBUG");
    break;
  case CDE_MSG_TYPE_PROGRESS:
    strcpy(msg_type, "PROGRESS");
    break; 
  default:
    strcpy(msg_type, "");
    break;
  }
  fprintf(stdout, "%s: %s\n", msg_type, rpt_msg);
}


//
// public functions 
//

/**
 * @brief initialize the api including the cdextract library  
 *        context and the database connection
 */
void api_init(char *device_name, char *root_folder, char *cddb_folder, char *db_filename, int db_backup) {
  // initialize the cdextract library
  if (cde == NULL) {
    cde = calloc(1, sizeof(cde_state));
    cde_initialize(cde, device_name, root_folder, cddb_folder, api_report_callback, api_set_extract_disc_progress);
  }
  // open the database connection
  if (db == NULL) {
    db = calloc(1, sizeof(sql_db));
    int res = open_database(db, db_filename);
    cde_report(CDE_MSG_TYPE_INFO, "open_database: %s; result: %d; message: %s", db->filename, res, res == DB_OK ? sqlite3_version : db->msg);
    if (res == DB_OK && db_backup == CDE_BACKUP_ON) {
      // start database backup in seperate thread
      res = backup_database(db);
      if (res == DB_OK) {
        cde_report(CDE_MSG_TYPE_INFO, "backup_database: starting database backup");
      } else {
        cde_report(CDE_MSG_TYPE_ERROR, "backup_database: unable to start database backup (%d): %s", res, db->msg);
      }
    }
  }
  // reset progress status
  api_set_extract_disc_progress(0, 0, 0, 0, 0);
}

/**
 * @brief clean the api context including the cdextract   
 *        library and the database connection
 */
void api_cleanup() {
  if (cde) {
    cde_cleanup(cde);
    free(cde);
    cde = NULL;
  }
  if (db) {
    close_database(db);
    free(db);
    db = NULL;
  }
}

/***
 * @brief set the given option
 */
void api_set_option(int option, int varg) {
  cde_set_option(cde, option, varg);
}

/**
 * @brief get information of the connected cdrom drive
 */
void api_get_drive_info(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_get_drive_info: [%s][%s]", cde->root_folder, request->path);
  if (cde == NULL || cde->status == CDE_STATUS_UNINITIALIZED) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  set_body_from_drive_info(cde, response);
}

/**
 * @brief open the connection with cdrom drive
 */
void api_open_drive(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_open_drive: [%s][%s]", cde->root_folder, request->path);
  // check for cdextract context
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  if (cde->status == CDE_STATUS_INITIALIZED) {
    int res;
    if ((res = cde_open_drive(cde)) != 0) {
      // unable to open drive
      response->code = NOT_FOUND;
      api_default_response(request, response);
    } else {
      // wait until the drive becomes available
      msleep(500);
    }
  }
  // drive connection opened or was already opened: respond with the drive information
  set_body_from_drive_info(cde, response);
}

/**
 * @brief close the connection with cdrom drive
 */
void api_close_drive(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_close_drive: [%s][%s]", cde->root_folder, request->path);
  // check for cdextract context
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  // close connection and cleanup
  if (cde->status == CDE_STATUS_IDLE || cde->status == CDE_STATUS_INITIALIZED) {
    if (cde_close_drive(cde) == CDE_OK) {
      // drive connection closed
      response->code = NO_CONTENT;
    } else {
      // drive connection already closed
      response->code = NOT_FOUND;
    }
    return;
  } else {
    // drive busy and locked by an operation
    response->code = RESOURCE_LOCKED;
  }
  api_default_response(request, response);
}

/**
 * @brief get information of the disc in the cdrom drive
 */
void api_get_disc_info(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_get_disc_info: [%s][%s]", cde->root_folder, request->path);
  int res = CDE_OK;

  // check for cdextract context
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  
  disc *cddb_disc_info = NULL;

  if (cde->status == CDE_STATUS_IDLE && (cde->disc_info == NULL || cde->disc_info->cddb_complete == 0 || cde->disc_info->mb_complete == 0)) {
    // prepare disc_info structure by using the toc and calculating the cddb and musicbrainz disc id hashes
    cde->status = CDE_STATUS_PREPARING;
    // extract toc information from the disc and cddb/musicbrains discid's
    if (cde_prepare_disc_info(cde) == 0) {

      // check if we already have the full disc information in the database
      res = get_disc_from_database_by_toc(db, cde->download_coverart, cde->disc_info);
      if (res == DB_OK && cde->disc_info->cddb_complete == 1 && cde->disc_info->mb_complete == 1) {
        // return disc information found in the database
        cde->status = CDE_STATUS_IDLE;
        set_body_from_disc_info(cde->disc_info, response);
        return;
      }

      // check if we already have the cddb disc information in the database
      long cddb_id = -1;
      
      if (get_cddb_entry_from_database_by_toc(db,  cde->disc_info, &cddb_id, &cddb_disc_info) == DB_ERROR) {
        cde_report(CDE_MSG_TYPE_ERROR, "api_get_disc_info: unable to check for cddb entry in the database");
        res = CDE_ERROR_CDDB_DATA;
        goto get_disc_info_error;
      }

      if (cddb_id > 0) {
        cde_report(CDE_MSG_TYPE_DEBUG, "api_get_disc_info: found cddb entry: %ld; revision: %d", cddb_id, cddb_disc_info->cddb_revision);
      }

      // check if cancellation requested before starting the download of cddb disc information
      if (cde->status == CDE_STATUS_CANCEL) {
        cde_report(CDE_MSG_TYPE_INFO, "api_get_disc_info: aborting download of cddb disc information");
        // return to 'idle' state and cleanup
        res = CDE_STATUS_CANCEL;
        goto get_disc_info_error;
      }

      // query the online cddb service to check if cddb information is available or if there is a newer revision
      res = cddb_get_disc_info(cde->disc_info, cde->drv, cde->verbose);
      cde_report(CDE_MSG_TYPE_DEBUG, "api_get_disc_info: cddb_get_disc_info: %d", res);

      if (cddb_id < 0 || match_cddb_disc_info(cde->disc_info, cddb_disc_info) == 0) {
        // get the cddb category id
        int category_id = get_category_id(db, cde->disc_info->cddb_category);
        if (category_id < 0) {
          cde_report(CDE_MSG_TYPE_ERROR, "api_get_disc_info: unable to store cddb disc information: disc category %s unknown", cde->disc_info->cddb_category);
          goto get_disc_info_error;
        }
        if (cddb_id < 0) {
          // store the downloaded disc information in the database
          if (store_cddb_entry_in_database(db, cde->disc_info, category_id, DB_DUPLICATE_CHECK_LOOKUP) == DB_ERROR) {
            cde_report(CDE_MSG_TYPE_ERROR, "api_get_disc_info: unable to store cddb disc information in the database");
            res = CDE_ERROR_CDDB_DATA;
            goto get_disc_info_error;
          }
        } else {
          // update the existing disc information in the database
          if (update_cddb_entry_in_database(db, cde->disc_info, cddb_id, category_id) == DB_ERROR) {
            cde_report(CDE_MSG_TYPE_ERROR, "api_get_disc_info: unable to update cddb disc information in the database");
            goto get_disc_info_error;
          } 
          cde_report(CDE_MSG_TYPE_DEBUG, "api_get_disc_info: updated cddb entry: %ld; revision: %d", cddb_id, cde->disc_info->cddb_revision);
        }        
      }

      // check if cancellation requested before starting the download of musicbrainz disc information
      if (cde->status == CDE_STATUS_CANCEL) {
        cde_report(CDE_MSG_TYPE_INFO, "api_get_disc_info: aborting download of musicbrainz disc information");
        // return to 'idle' state and cleanup
        res = CDE_STATUS_CANCEL;
        goto get_disc_info_error;
      }

      // try to get the the musicbrainz release id to download the cover images(s) and
      // add the missing disc information in case the cddb query failed
      res &= mb_get_disc_info(cde->disc_info, request->fuzzy_lookup, cde->verbose);

      // set the filename suffix (default set to flac)
      char *file_suffix = calloc(5, sizeof(char));
      if (cde->output_type==CDE_OUTPUT_TYPE_WAV) { 
        strcpy(file_suffix, "wav");
      } else {
        strcpy(file_suffix, "flac");
      }

      // set track output filenames
      for (int i = 0; i < cde->disc_info->d_tracks; i++) {
        if (cde_set_track_filename(cde->disc_info, i, file_suffix) != 0) {
           free(file_suffix);
          goto get_disc_info_error;
        }
      }
      free(file_suffix);
      
      // set the output folder: {ROOT_FOLDER}/{ARTIST}/{ALBUM_TITLE}/
      if (cde_set_create_output_path(cde, 0) != 0) {
        cde_report(CDE_MSG_TYPE_DEBUG, "api_get_disc_info: unable to set output path");
        res = CDE_ERROR_OUTPUT_PATH;
        goto get_disc_info_error;
      }
      
      if (cde->download_coverart != CDE_COVERART_OFF) {
        
        // check if cancellation requested before starting the download of disc covers
        if (cde->status == CDE_STATUS_CANCEL) {
          cde_report(CDE_MSG_TYPE_INFO, "api_get_disc_info: aborting download of disc covers");
          // return to 'idle' state and cleanup
          res = CDE_STATUS_CANCEL;
          goto get_disc_info_error;
        }

        // try to get front cover
        struct stat st = {0};
        char *front_cover_file = calloc(strlen(cde->folder)+strlen(CDE_COVER_FRONT)+2, sizeof(char));
        sprintf(front_cover_file, "%s/%s", cde->folder, CDE_COVER_FRONT);
        if (stat(front_cover_file, &st) == 0) {
          // file available: try to load the front cover from file
          cde_report(CDE_MSG_TYPE_DEBUG, "api_get_disc_info: loading front cover");
          cde->disc_info->mb_front_cover_size = read_file(&cde->disc_info->mb_front_cover, front_cover_file);
        } 
        if (cde->disc_info->mb_front_cover_size <= 0) {
          if (strlen(cde->disc_info->mb_release_id) > 0) {
            // front cover not loaded from file: try to download front cover from the online coverart service
            cde_report(CDE_MSG_TYPE_DEBUG, "api_get_disc_info: get front cover for release id: %s", cde->disc_info->mb_release_id);
            // note: we are only downloading the cover art to memory in this step with MB_COVERART_MEM_ONLY
            //       Covers are only written to file as part of the audio extraction process
            int r = mb_caa_get_front_cover(cde->disc_info, cde->download_coverart, cde->folder, cde->verbose);
            cde->disc_info->mb_complete = (r == 0 ? 1 : 0);
            res &= r;
          } else {
            cde_report(CDE_MSG_TYPE_WARNING, "api_get_disc_info: unable to download cover: mb release id unavailable");
          }
        }
        free(front_cover_file);

        // try to get back cover
        char *back_cover_file = calloc(strlen(cde->folder)+strlen(CDE_COVER_BACK)+2, sizeof(char));
        sprintf(back_cover_file, "%s/%s", cde->folder, CDE_COVER_BACK);
        if (stat(back_cover_file, &st) == 0) {
          // file available: try to load the back cover from file
          cde_report(CDE_MSG_TYPE_DEBUG, "api_get_disc_info: loading back cover");
          cde->disc_info->mb_back_cover_size = read_file(&cde->disc_info->mb_back_cover, back_cover_file);
        }
        if (cde->disc_info->mb_back_cover_size <= 0) {
          if (strlen(cde->disc_info->mb_release_id) > 0) {
            // back cover not loaded from file: try to download back cover from the online coverart service
            cde_report(CDE_MSG_TYPE_DEBUG, "api_get_disc_info: get back cover for release id: %s", cde->disc_info->mb_release_id);
            // note: we are only downloading the cover art to memory in this step with MB_COVERART_MEM_ONLY
            //       Covers are only written to file as part of the audio extraction process
            int r = mb_caa_get_back_cover(cde->disc_info, cde->download_coverart, cde->folder, cde->verbose);
            res &= r;
          }
        }
        free(back_cover_file);

      } else {
        // do not download coverart: we are done now
        cde_report(CDE_MSG_TYPE_DEBUG, "api_get_disc_info: skipping cover art");
        cde->disc_info->mb_complete = 1;
      }

      // store the downloaded disc information in the database
      if (store_disc_in_database(db, cde->disc_info, request->overwrite) > DB_OK) {
        cde_report(CDE_MSG_TYPE_ERROR, "api_get_disc_info: unable to store disc information: (%d) %s", db->status, db->msg);
      }
    } 
    cde->status = CDE_STATUS_IDLE;
  }

  // clean up
  if (cddb_disc_info != NULL) {
    cde_free_disc(&cddb_disc_info, -1);
  }

  // return downloaded or cached disc information
  if (cde->status >= CDE_STATUS_IDLE && cde->disc_info != NULL) {
    set_body_from_disc_info(cde->disc_info, response);
    return;
  }

  // drive connection closed or no disc information available
  response->code = NOT_FOUND;
  api_default_response(request, response);

  // disc information request cancelled or an error occurred
get_disc_info_error:
  if (cddb_disc_info != NULL) {
    cde_free_disc(&cddb_disc_info, -1);
  }
  cde->status = CDE_STATUS_IDLE;
  if (res == CDE_STATUS_CANCEL) {
    response->code = CONFLICT;
  } else {
    response->code = INTERNAL_SERVER_ERROR;
  }
  api_default_response(request, response);
}

/**
 * @brief edit the information of the disc in the cdrom drive
 */
void api_update_disc_info(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info: [%s][%s]", cde->root_folder, request->path);
  // check for cdextract context
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  // check for not downloading or extracting data
  if (cde->status > CDE_STATUS_IDLE) {
    response->code = RESOURCE_LOCKED;
    api_default_response(request, response);
    return;
  }
  // check if request data is available
  if (request->size <= 0 || request->data == NULL) {
    response->code = BAD_REQUEST;
    api_default_response(request, response);
  }
  // check if drive connection opened and disc information is available
  if (cde->status != CDE_STATUS_IDLE || cde->disc_info == NULL) {
    response->code = NOT_FOUND;
    api_default_response(request, response);
  }
  // parse json data
  json_token token;
  if (json_init(&token, request->data, request->size) == json_ok) {
    cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info: parsed json data");

    int validation_status = 0;
    char *value = calloc(JSON_UTILS_MAX_TOKEN_STR_SIZE, sizeof(char));
    char *db_id = calloc(JSON_UTILS_MAX_TOKEN_STR_SIZE, sizeof(char));
    char *disc_id = NULL;
    char *d_id_str = calloc(10, sizeof(char));
    
    int d_length = 0;
    char *d_artist = NULL;
    char *d_title = NULL;
    char *d_genre = NULL;
    int d_year = 0;
    char *d_extended = NULL;
    char *path = calloc(41, sizeof(char));
    track *tracks = calloc(cde->disc_info->d_tracks, sizeof(track));

    if (json_select_member(&token, "$.id", db_id, JSON_UTILS_MAX_TOKEN_STR_SIZE, request->data) > 0) {
      
      // check if disc id of the posted data matches the disc id of the cached disc info
      json_get_string(&token, "disc_id", &disc_id, request->data);
      sprintf(d_id_str, "%08x", cde->disc_info->d_id);
      if (strcasecmp(d_id_str, disc_id)==0) {

        // get disc information
        json_get_integer(&token, "length", &d_length, request->data);
        if (d_length <= 0) {
          cde_report(CDE_MSG_TYPE_ERROR, "api_update_disc_info: invalid disc length");
          validation_status = 3;            
        }

        if (validation_status == 0) {
          json_get_string(&token, "artist", &d_artist, request->data);
          json_get_string(&token, "title", &d_title, request->data);
          json_get_string(&token, "genre", &d_genre, request->data);
          json_get_integer(&token, "year", &d_year, request->data);  
          json_get_string(&token, "extended", &d_extended, request->data);
          
          // get track information
          for (int i=0; i < cde->disc_info->d_tracks; i++) {

            snprintf(path, 40, "$.tracks.[%d].num", i);
            if (json_select_member(&token, path, value, JSON_UTILS_MAX_TOKEN_STR_SIZE, request->data) > 0) {
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info: path: %s; value: %s", path, value);

              tracks[i].t_num = 0;
              if (sscanf(value, "%d", &(tracks[i].t_num)) != 1) {
                cde_report(CDE_MSG_TYPE_ERROR, "api_update_disc_info: invalid track number");
                validation_status = 4;
                break;
              }

              if (tracks[i].t_num < 1 || tracks[i].t_num > cde->disc_info->d_tracks) {
                cde_report(CDE_MSG_TYPE_ERROR, "api_update_disc_info: path: %s; invalid track number: %d", path, tracks[i].t_num);
                validation_status = 5;
                break;
              }
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info: path: %s; track number: %d", path, tracks[i].t_num);

              json_get_integer(&token, "length", &(tracks[i].t_length), request->data);
              
              if (tracks[i].t_length < 0 || tracks[i].t_length > cde->disc_info->d_length) {
                cde_report(CDE_MSG_TYPE_ERROR, "api_update_disc_info: path: %s; invalid track length: %d frames; %d seconds", path, tracks[i].t_length, tracks[i].t_length / CDE_CD_FRAMES);
                validation_status = 6;
                break;
              }
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info: path: %s; track length: %d", path, tracks[i].t_length);

              json_get_string(&token, "title", &(tracks[i].t_title), request->data);
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info: path: %s; track title: %s", path, tracks[i].t_title);

              json_get_string(&token, "artist", &(tracks[i].t_artist), request->data);
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info: path: %s; track artist: %s", path, tracks[i].t_artist);

              json_get_string(&token, "album", &(tracks[i].t_album), request->data);
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info: path: %s; track album: %s", path, tracks[i].t_album);

              json_get_string(&token, "genre", &(tracks[i].t_genre), request->data);
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info: path: %s; track genre: %s", path, tracks[i].t_genre);

              json_get_integer(&token, "year", &(tracks[i].t_year), request->data);
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info: path: %s; track year: %d", path, tracks[i].t_year);

              json_get_string(&token, "extended", &(tracks[i].t_extended), request->data);
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info: path: %s; track extended: %s", path, tracks[i].t_extended);

              json_get_string(&token, "filename", &(tracks[i].t_filename), request->data);
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info: path: %s; track filename: %s", path, tracks[i].t_filename);
            }
          }
        }
      } else {
        cde_report(CDE_MSG_TYPE_ERROR, "api_update_disc_info: no match on disc id");
        validation_status = 2;
      }

    } else {
      cde_report(CDE_MSG_TYPE_ERROR, "api_update_disc_info: invalid json structure; id not found");
      validation_status = 1;
    }

    if (validation_status == 0) {
      // validation passed: update disc information
      if (d_artist != NULL && strlen(d_artist) > 0) {
        char *tmp = realloc(cde->disc_info->d_artist, (strlen(d_artist) + 1) * sizeof(char));
        if (tmp != NULL) {
          cde->disc_info->d_artist = tmp;
          strcpy(cde->disc_info->d_artist, d_artist);
        }
      }
      if (d_title != NULL && strlen(d_title) > 0) {
        char *tmp = realloc(cde->disc_info->d_title, (strlen(d_title) + 1) * sizeof(char));
        if (tmp != NULL) {
          cde->disc_info->d_title = tmp;
          strcpy(cde->disc_info->d_title, d_title);
        }
      }
      if (d_genre != NULL && strlen(d_genre) > 0) {
        char *tmp = realloc(cde->disc_info->d_genre, (strlen(d_genre) + 1) * sizeof(char));
        if (tmp != NULL) {
          cde->disc_info->d_genre = tmp;
          strcpy(cde->disc_info->d_genre, d_genre);
        }
      }
      if (d_year > CDE_MIN_YEAR && d_year < CDE_MAX_YEAR) {
        cde->disc_info->d_year = d_year;
      }
      if (d_extended != NULL && strlen(d_extended) > 0) {
        char *tmp = realloc(cde->disc_info->d_extended, (strlen(d_extended) + 1) * sizeof(char));
        if (tmp != NULL) {
          cde->disc_info->d_extended = tmp;
          strcpy(cde->disc_info->d_extended, d_extended);
        }
      }
      // validation passed: update track information
      for (int i=0; i < cde->disc_info->d_tracks; i++) {
        if (tracks[i].t_num != 0) {
          for (int j=0; j < cde->disc_info->d_tracks; j++) {
            if (tracks[i].t_num == cde->disc_info->tracks[j].t_num) {

              if (tracks[i].t_title != NULL && strlen(tracks[i].t_title) > 0) {
                char *tmp = realloc(cde->disc_info->tracks[j].t_title, (strlen(tracks[i].t_title) + 1) * sizeof(char));
                if (tmp != NULL) {
                  cde->disc_info->tracks[j].t_title = tmp;
                  strcpy(cde->disc_info->tracks[j].t_title, tracks[i].t_title);
                }
              }
              if (tracks[i].t_artist != NULL && strlen(tracks[i].t_artist) > 0) {
                char *tmp = realloc(cde->disc_info->tracks[j].t_artist, (strlen(tracks[i].t_artist) + 1) * sizeof(char));
                if (tmp != NULL) {
                  cde->disc_info->tracks[j].t_artist = tmp;
                  strcpy(cde->disc_info->tracks[j].t_artist, tracks[i].t_artist);
                }
              }
              if (tracks[i].t_album != NULL && strlen(tracks[i].t_album) > 0) {
                char *tmp = realloc(cde->disc_info->tracks[j].t_album, (strlen(tracks[i].t_album) + 1) * sizeof(char));
                if (tmp != NULL) {
                  cde->disc_info->tracks[j].t_album = tmp;
                  strcpy(cde->disc_info->tracks[j].t_album, tracks[i].t_album);
                }
              }
              if (tracks[i].t_genre != NULL && strlen(tracks[i].t_genre) > 0) {
                char *tmp = realloc(cde->disc_info->tracks[j].t_genre, (strlen(tracks[i].t_genre) + 1) * sizeof(char));
                if (tmp != NULL) {
                  cde->disc_info->tracks[j].t_genre = tmp;
                  strcpy(cde->disc_info->tracks[j].t_genre, tracks[i].t_genre);
                }
              }
              if (tracks[i].t_year > CDE_MIN_YEAR && d_year < CDE_MAX_YEAR) {
                cde->disc_info->tracks[j].t_year = tracks[i].t_year;
              }
              if (tracks[i].t_extended != NULL && strlen(tracks[i].t_extended) > 0) {
                char *tmp = realloc(cde->disc_info->tracks[j].t_extended, (strlen(tracks[i].t_extended) + 1) * sizeof(char));
                if (tmp != NULL) {
                  cde->disc_info->tracks[j].t_extended = tmp;
                  strcpy(cde->disc_info->tracks[j].t_extended, tracks[i].t_extended);
                }
              }
              if (tracks[i].t_filename != NULL && strlen(tracks[i].t_filename) > 0) {
                char *tmp = realloc(cde->disc_info->tracks[j].t_filename, (strlen(tracks[i].t_filename) + 1) * sizeof(char));
                if (tmp != NULL) {
                  cde->disc_info->tracks[j].t_filename = tmp;
                  strcpy(cde->disc_info->tracks[j].t_filename, tracks[i].t_filename);
                }
              }
              break;
            }
          }
        }
      }
      // validation passed: store the downloaded disc information in the database, update if already present
      if (store_disc_in_database(db, cde->disc_info, 1) != 0) {
        cde_report(CDE_MSG_TYPE_ERROR, "api_update_disc_info: unable to store disc information: (%d) %s", db->status, db->msg);
      }
      // return updated disc information
      api_get_disc_info(request, response);
    } else {
      // validation failed
      response->code = BAD_REQUEST;
      api_default_response(request, response);
    }

    // cleanup
    for (int i=0; i < cde->disc_info->d_tracks; i++) {
      if (tracks[i].t_title != NULL) {
        free(tracks[i].t_title);
      }
      if (tracks[i].t_artist != NULL) {
        free(tracks[i].t_artist);
      }
      if (tracks[i].t_album != NULL) {
        free(tracks[i].t_album);
      }
      if (tracks[i].t_genre != NULL) {
        free(tracks[i].t_genre);
      }
      if (tracks[i].t_extended != NULL) {
        free(tracks[i].t_extended);
      }
      if (tracks[i].t_filename != NULL) {
        free(tracks[i].t_filename);
      }      
    }
    free(tracks);
    free(path);
    if (d_extended != NULL) {
      free(d_extended);
    }
    if (d_genre != NULL) {
      free(d_genre);
    }
    if (d_title != NULL) {
      free(d_title);
    }
    if (d_artist != NULL) {
      free(d_artist);
    }
    if (disc_id != NULL) {
      free(disc_id);
    }
    free(db_id);
    free(d_id_str);
    free(value);
    return;
  }
  // unable to parse json data
  response->code = BAD_REQUEST;
  api_default_response(request, response);
}

/**
 * @brief insert a disc into the cdrom drive
 */
void api_insert_disc(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_insert_disc: [%s][%s]", cde->root_folder, request->path);
  // check for cdextract context
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  // check for not downloading or extracting data
  if (cde->status > CDE_STATUS_IDLE) {
    response->code = RESOURCE_LOCKED;
    api_default_response(request, response);
    return;
  }
  // close drive tray
  int res = cde_close_tray(cde);
  if (res == CDE_OK) {
    // drive tray closed
    response->code = NO_CONTENT;
  } else if (res == CDE_ERROR_NO_DISC) {
    // no disc found
    response->code = NOT_FOUND;
  } else { 
    // drive connection closed or not idle
    response->code = SERVICE_UNAVAILABLE;
  }
  api_default_response(request, response);
}

/**
 * @brief eject the disc from the cdrom drive
 */
void api_eject_disc(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_eject_disc: [%s][%s]", cde->root_folder, request->path);
  // check for cdextract context
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  // check for not downloading or extracting data
  if (cde->status > CDE_STATUS_IDLE) {
    response->code = RESOURCE_LOCKED;
    api_default_response(request, response);
    return;
  }
  // open/eject drive tray
  int res = cde_eject(cde);
  if (res == CDE_OK) {
    // drive tray opened / disc ejected
    response->code = NO_CONTENT;
  } else if (res == CDE_ERROR_NOT_IDLE) {
    // drive not in idle state
    response->code = RESOURCE_LOCKED;
  } else if (res == CDE_ERROR_NO_DRIVE) {
    // drive not found
    response->code = NOT_FOUND;
  } else {
    // unable to (re)open drive
    response->code = SERVICE_UNAVAILABLE;
  }
  api_default_response(request, response);
}

/**
 * @brief get the audio extraction progress status
 */
void api_get_extract_disc_progress(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_get_extract_disc_progress: [%s][%s]", cde->root_folder, request->path);
  // check for cdextract context
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  // check if connection opened with drive and disc
  if (cde->status < CDE_STATUS_IDLE || cde->drv == NULL || cde->drv->opened == 0) {
    response->code = NOT_FOUND;
    api_default_response(request, response);
    return;
  }
  response->code = OK;
  response->mime_type = MIME_TYPE_JSON;
  response->content_type = get_content_type(response->mime_type);

  if (request->stream == 1) {
    // create response by calling the streaming_callback
    response->callback = api_callback_extract_disc_progress;
    response->callback_context = NULL;
    response->callback_cleanup = NULL;
    response->callback_block_size = MAX_STATUS_RESPONSE;
    response->size = SIZE_UNKNOWN_RESPONSE;
  } else {
    // create json response by returning the last available audio extraction progress status
    char *json_response = calloc(MAX_STATUS_RESPONSE+1, sizeof(char));
    snprintf(json_response, MAX_STATUS_RESPONSE,
            "{\"function\": \"%s\", \"track\": %d, \"sector\": %ld, \"percentage\": %.1f, \"message\": \"\"}",
            progress.function_str,
            progress.track,
            progress.sector,
            progress.percentage);
    response->size = strlen(json_response);
    response->body = calloc(response->size+1, sizeof(char));
    strcpy(response->body, json_response);
    free(json_response);
  }
}

/**
 * @brief callback to get the audio extraction progress status
 * @return length of the returned data
 */
ssize_t api_callback_extract_disc_progress(void *cls, uint64_t pos, char *buf, size_t max) {
  ssize_t len = -1;

  if (cde->status < CDE_STATUS_IDLE) {
    // not connected: no progress update available
    return len;
  }
  if (cde->status == CDE_STATUS_IDLE && progress.last_request == progress.last_update) {
    // idle, but no new progress update available
    return len;
  }
  if (progress.rpt_type == EXTRACT_CB_END_OF_DISC && progress.last_request == progress.last_update) {
    // reached the end of the audio extraction process
    return len;
  }

  // status is CDE_STATUS_IDLE and last update not yet returned, 
  // or the status is CDE_STATUS_PREPARING, CDE_STATUS_EXTRACTING or CDE_STATUS_CANCEL

  // proceed if new update available or after 10 seconds
  clock_t start = clock();
  clock_t current = start;
  while (progress.last_request == progress.last_update && current-start < 1000) {
    msleep(100);
    current = clock();
  }
  fprintf(stdout, "%ld -> %ld (%ld)\n", start, current, current-start);
  
  // prepare response
  char *json_response = calloc(MAX_STATUS_RESPONSE+1, sizeof(char));
  snprintf(json_response, MAX_STATUS_RESPONSE,
          "{\"function\": \"%s\", \"track\": %d, \"sector\": %ld, \"percentage\": %.1f, \"message\": \"\"}\n",
          progress.function_str,
          progress.track,
          progress.sector,
          progress.percentage);
  len = strlen(json_response);       
  if (len < max && len < MAX_STATUS_RESPONSE) {
    strncpy(buf, json_response, len);
    buf[len] = '\0';
  } else {
    len = 0;
  }
  free(json_response);

  // update last request
  if (progress.last_request < progress.last_update) {
    progress.last_request = progress.last_update;
  }
  
  return len;
}

/**
 * @brief helper function to set the audio extraction progress status
 */
void api_set_extract_disc_progress(int rpt_type, int function, int track, long sector, float percentage) {

  if (rpt_type == 0) {
    // reset
    progress.rpt_type_str[0] = '\0';
    progress.function_str[0] = '\0';
    progress.last_update = 0;
    progress.last_request = 0;

  } else {
    // set report and function
    switch (rpt_type) {
    case CDE_MSG_TYPE_ERROR:
      strcpy(progress.rpt_type_str, "error");
      break;
    case CDE_MSG_TYPE_WARNING:
      strcpy(progress.rpt_type_str, "warning");
      break;
    case CDE_MSG_TYPE_INFO:
      strcpy(progress.rpt_type_str, "info");
      break;
    case CDE_MSG_TYPE_DEBUG:
      strcpy(progress.rpt_type_str, "debug");
      break;
    case CDE_MSG_TYPE_PROGRESS:
      strcpy(progress.rpt_type_str, "progress");
      break; 
    default:
      strcpy(progress.rpt_type_str, "unknown");
      rpt_type = CDE_MSG_TYPE_ERROR;
      break;
    }

    switch (function) {
    // normal events
    case EXTRACT_CB_READ:
      strcpy(progress.function_str, "read");
      break;
    case EXTRACT_CB_VERIFY:
      strcpy(progress.function_str, "verify");
      break;
    case EXTRACT_CB_WRITE_FILE:
      strcpy(progress.function_str, "write");
      break;
    case EXTRACT_CB_END_OF_FILE:
      strcpy(progress.function_str, "end");
      percentage = 100;
      break;
    case EXTRACT_CB_END_OF_DISC:
      strcpy(progress.function_str, "done");
      track = 0;
      sector = 0;
      percentage = 100;
      break;
    // paranoia specific warnings and errors
    case EXTRACT_CB_FIXUP_EDGE:
      strcpy(progress.function_str, "fixup edge");
      break;
    case EXTRACT_CB_FIXUP_ATOM:
      strcpy(progress.function_str, "fixup atom");
      break;
    case EXTRACT_CB_SCRATCH:
      strcpy(progress.function_str, "scratch");
      break;
    case EXTRACT_CB_REPAIR:
      strcpy(progress.function_str, "repair");
      break;
    case EXTRACT_CB_SKIP:
      strcpy(progress.function_str, "skip");
      break;
    case EXTRACT_CB_DRIFT:
      strcpy(progress.function_str, "drift");
      break;
    case EXTRACT_CB_BACKOFF:
      strcpy(progress.function_str, "backoff");
      break;
    case EXTRACT_CB_OVERLAP:
      strcpy(progress.function_str, "overlap");
      break;
    case EXTRACT_CB_FIXUP_DROPPED:
      strcpy(progress.function_str, "fixup dropped");
      break;
    case EXTRACT_CB_FIXUP_DUPED:
      strcpy(progress.function_str, "fixup duped");
      break;
    case EXTRACT_CB_READERR:
      strcpy(progress.function_str, "read error");
      break;
    case EXTRACT_CB_CACHEERR:
      strcpy(progress.function_str, "cache error");
      break;
    default:
      // unsupported callback message type
      snprintf(progress.function_str, 15, "%d", function);
      rpt_type = CDE_MSG_TYPE_WARNING;
      break;
    }
  }

  progress.rpt_type = rpt_type;
  progress.function = function;
  progress.track = track;
  progress.sector = sector;
  progress.percentage = percentage;
  progress.last_update = clock();
}

/**
 * @brief extract the audio from the disc in the cdrom drive
 */
void api_extract_disc(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_extract_disc: [%s][%s]", cde->root_folder, request->path);
  // check for cdextract context
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }

  // check if connection opened with drive and disc
  if (cde->status < CDE_STATUS_IDLE || cde->drv == NULL || cde->drv->opened == 0) {
    response->code = NOT_FOUND;
    api_default_response(request, response);
    return;
  }

  // check for not downloading or extracting data
  if (cde->status > CDE_STATUS_IDLE) {
    response->code = RESOURCE_LOCKED;
    api_default_response(request, response);
    return;
  }

  // set and create the output folder: {ROOT_FOLDER}/{ARTIST}/{ALBUM_TITLE}/
  if (cde_set_create_output_path(cde, 1) != 0) {
    cde_report(CDE_MSG_TYPE_ERROR, "api_extract_disc: unable to set/create output path");
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }

  // if configured, write the gathered disc information to a json file
  if (cde->write_json == CDE_WRITE_JSON_ON) {
    cde_report(CDE_MSG_TYPE_INFO, "api_extract_disc: writing disc information");
    json_write_disc_info(cde->disc_info, cde->folder, request->overwrite, cde->verbose);
  }

  // if configured, write the gathered disc information to a cue sheet
  if (cde->write_cue_sheet == CDE_WRITE_CUE_SHEET_ON) {
    cde_report(CDE_MSG_TYPE_INFO, "api_extract_disc: writing cue sheet");
    write_cue_sheet(cde->disc_info, cde->folder, request->overwrite, cde->verbose);
  }

  // if configured, write the gathered disc information to a cddb entry in xmcd format
  if (cde->write_cddb == CDE_WRITE_CDDB_ON) {
    cde_report(CDE_MSG_TYPE_INFO, "api_extract_disc: writing cddb entry");
    cddb_write_entry(cde->disc_info, cde->folder, request->overwrite, cde->verbose);
  }

  // if configured, write the json formatted release information from the coverartarchive (CAA) to file
  if (cde->download_coverart >= MB_COVERART_FULL) {
    cde_report(CDE_MSG_TYPE_INFO, "api_extract_disc: writing coverartarchive release information");
    mb_caa_get_release_info(cde->disc_info, cde->download_coverart,cde->folder, cde->verbose);
  }

  // if configured, write the front and back cover images to file
  if (cde->download_coverart != MB_COVERART_OFF && cde->download_coverart != MB_COVERART_MEM_ONLY) {
    if (cde->disc_info->mb_front_cover_size > 0) {
      int front_cover_path_len = snprintf(NULL, 0, "%s/%s", cde->folder, CDE_COVER_FRONT);
      char *front_cover_path = malloc((front_cover_path_len + 1) * sizeof(char));
      sprintf(front_cover_path, "%s/%s", cde->folder, CDE_COVER_FRONT);
      FILE *front_fp = fopen(front_cover_path, "wb");
      if (front_fp) {
        cde_report(CDE_MSG_TYPE_INFO, "api_extract_disc: writing front cover to: '%s'", front_cover_path);
        fwrite(cde->disc_info->mb_front_cover, cde->disc_info->mb_front_cover_size, 1, front_fp);
        fclose(front_fp);
      }
      free(front_cover_path);
    }
    if (cde->disc_info->mb_back_cover_size > 0) {
      int back_cover_path_len = snprintf(NULL, 0, "%s/%s", cde->folder, CDE_COVER_BACK);
      char *back_cover_path = malloc((back_cover_path_len + 1) * sizeof(char));
      sprintf(back_cover_path, "%s/%s", cde->folder, CDE_COVER_BACK);
      FILE *back_fp = fopen(back_cover_path, "wb");
      if (back_fp) {
        cde_report(CDE_MSG_TYPE_INFO, "api_extract_disc: writing back cover to: '%s'", back_cover_path);
        fwrite(cde->disc_info->mb_front_cover, cde->disc_info->mb_front_cover_size, 1, back_fp);
        fclose(back_fp);
      }
      free(back_cover_path);    
    }
  }

  // reset progress status
  api_set_extract_disc_progress(0, 0, 0, 0, 0);

  // extract audio cd
  int res = cde_extract_audio(cde);
  if (res == CDE_OK) {
    // extraction process started: return progress status
    api_get_extract_disc_progress(request, response);
    return;
  }

  // drive not in idle state after call to extract audio function
  response->code = SERVICE_UNAVAILABLE;
  api_default_response(request, response);
}

/**
 * @brief cancel the audio extraction from disc
 */
void api_cancel_disc(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_cancel_disc: [%s][%s]", cde->root_folder, request->path);
  // check for cdextract context
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  // check if connection opened with drive and disc
  if (cde->status < CDE_STATUS_IDLE || cde->drv == NULL || cde->drv->opened == 0) {
    response->code = NOT_FOUND;
    api_default_response(request, response);
    return;
  }
  // check for downloading or extracting data
  if (cde->status != CDE_STATUS_PREPARING && cde->status != CDE_STATUS_EXTRACTING) {
    response->code = RESOURCE_LOCKED;
    api_default_response(request, response);
    return;
  }
  // cancel cd audio extraction
  int res = cde_cancel_extract(cde, 1);
  if (res == CDE_OK) {
    // cd audio extraction cancelled
    response->code = NO_CONTENT;
    // reset progress status
    api_set_extract_disc_progress(0, 0, 0, 0, 0);
  } else {
    // currently not extracting audio data or preparing to extract (downloading disc info)
    response->code = SERVICE_UNAVAILABLE;
  }
  api_default_response(request, response);
}

/**
 * @brief get the front cover of the disc in the cdrom drive
 */
void api_get_disc_front_cover(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_get_disc_front_cover: [%s][%s]", cde->root_folder, request->path);
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  if (cde->status >= CDE_STATUS_IDLE && cde->disc_info!=NULL && cde->disc_info->mb_front_cover_size>0) {
    data_response(cde->disc_info->mb_front_cover, cde->disc_info->mb_front_cover_size, MIME_TYPE_JPEG, response);
    return;
  }
  if (request->return_default == 1) {
    file_response(DEFAULT_FRONT_COVER_FILENAME, cde->root_folder, response);
    return;
  }
  response->code = NOT_FOUND;
  api_default_response(request, response);
}

/**
 * @brief update the front cover of the disc in the cdrom drive
 */
void api_update_disc_front_cover(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_front_cover: [%s][%s]", cde->root_folder, request->path);
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  // check if request data is available
  if (request->size < 140 || request->data == NULL) {
    response->code = BAD_REQUEST;
    api_default_response(request, response);
    return;
  }
  // check if drive connection opened and disc information is available
  if (cde->status < CDE_STATUS_IDLE || cde->disc_info == NULL) {
    response->code = NOT_FOUND;
    api_default_response(request, response);
    return;
  }
  // reallocate memory for the cover and copy data
  if (cde->disc_info->mb_front_cover != NULL) {
    free(cde->disc_info->mb_front_cover);
  }      
  cde->disc_info->mb_front_cover = malloc(request->size * sizeof(char));
  if (cde->disc_info->mb_front_cover != NULL) {
    memcpy(cde->disc_info->mb_front_cover, request->data, request->size);
    cde->disc_info->mb_front_cover_size = request->size;
    // update front cover in database
    if (cde->disc_info->db_id > 0) {
      // try to get the disc_id to check if the disc is already stored
      long disc_id = get_disc_id(db, cde->disc_info);
      if (disc_id >= 0 && disc_id == cde->disc_info->db_id) {
        if (update_cover_in_database(db, disc_id, 0, request->data, request->size) != 0) {
          cde_report(CDE_MSG_TYPE_ERROR, "api_update_disc_front_cover: unable to update cover in database for id [%ld] with data size[%d]\n", disc_id, request->size);
        }
      } else {
        cde_report(CDE_MSG_TYPE_ERROR, "api_update_disc_front_cover: unable to update cover in database due to id mismatch between [%ld] and stored id [%d]\n", cde->disc_info->db_id, disc_id);
      }
    }
    response->code = NO_CONTENT;
    api_default_response(request, response);
    return;
  }
  // unable to allocate memory for the cover
  response->code = INTERNAL_SERVER_ERROR;
  api_default_response(request, response);
}

/**
 * @brief get the back cover of the disc in the cdrom drive
 */
void api_get_disc_back_cover(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_get_disc_back_cover: [%s][%s]", cde->root_folder, request->path);
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  if (cde->status >= CDE_STATUS_IDLE && cde->disc_info!=NULL && cde->disc_info->mb_back_cover_size>0) {
    data_response(cde->disc_info->mb_back_cover, cde->disc_info->mb_back_cover_size, MIME_TYPE_JPEG, response);
    return;
  }
  if (request->return_default == 1) {
    file_response(DEFAULT_BACK_COVER_FILENAME, cde->root_folder, response);
    return;
  }
  response->code = NOT_FOUND;
  api_default_response(request, response);
}

/**
 * @brief update the back cover of the disc in the cdrom drive
 */
void api_update_disc_back_cover(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_back_cover: [%s][%s]", cde->root_folder, request->path);
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  // check if request data is available
  if (request->size < 140 || request->data == NULL) {
    response->code = BAD_REQUEST;
    api_default_response(request, response);
    return;
  }
  // check if drive connection opened and disc information is available
  if (cde->status < CDE_STATUS_IDLE || cde->disc_info == NULL) {
    response->code = NOT_FOUND;
    api_default_response(request, response);
    return;
  }
  // reallocate memory for the cover and copy data
  if (cde->disc_info->mb_back_cover != NULL) {
    free(cde->disc_info->mb_back_cover);
  }      
  cde->disc_info->mb_back_cover = malloc(request->size * sizeof(char));
  if (cde->disc_info->mb_back_cover != NULL) {
    memcpy(cde->disc_info->mb_back_cover, request->data, request->size);
    cde->disc_info->mb_back_cover_size = request->size;
    // update back cover in database
    if (cde->disc_info->db_id > 0) {
      // try to get the disc_id to check if the disc is already stored
      long disc_id = get_disc_id(db, cde->disc_info);
      if (disc_id >= 0 && disc_id == cde->disc_info->db_id) {
        if (update_cover_in_database(db, disc_id, 1, request->data, request->size) != 0) {
          cde_report(CDE_MSG_TYPE_ERROR, "api_update_disc_back_cover: unable to update cover in database for id [%ld] with data size[%d]", disc_id, request->size);
        }
      } else {
        cde_report(CDE_MSG_TYPE_ERROR, "api_update_disc_back_cover: unable to update cover in database due to id mismatch between [%ld] and stored id [%d]", cde->disc_info->db_id, disc_id);
      }
    }
    response->code = NO_CONTENT;
    api_default_response(request, response);
    return;
  }
  // unable to allocate memory for the cover
  response->code = INTERNAL_SERVER_ERROR;
  api_default_response(request, response);
}

/**
 * @brief get the audio data of the disc (and track) in the cdrom drive
 */
void api_get_audio(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_get_audio: [%s][%s][%s][%d][%d]", cde->root_folder, request->path, request->resource_id, request->track, request->format);
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  // check if current disc has a filename for the requested track
  if (cde->disc_info != NULL && cde->disc_info->d_tracks >= request->track &&
      request->track > 0 && cde->disc_info->tracks != NULL &&
      cde->disc_info->tracks[request->track - 1].t_filename != NULL) {

      // return the audio data of the requested track
      api_audio_response(request, response, &(cde->disc_info->tracks[request->track-1]));

  } else {
    // no audio tracks available or invalid track number
    response->code = BAD_REQUEST;
    api_default_response(request, response);
  }
}

/**
 * @brief list all stored discs
 */
void api_list_discs(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_list_discs: [%s][%s]", cde->root_folder, request->path);
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  if (request->limit > 0 && request->limit <= MAX_REQUEST_LIMIT && request->offset >= 0) {
    disc_list *disc_info_list = NULL;
    if (get_disc_list_from_database(db, request->limit, request->offset, &disc_info_list) == 0) {
      set_body_from_disc_list(&disc_info_list, response);
      free_disc_list(&disc_info_list);
      return;
    }
    cde_report(CDE_MSG_TYPE_ERROR, "api_list_discs: unable to get disc list from database: (%d) %s", db->status, db->msg);
    if (disc_info_list != NULL) {
      // free the disc list if it was allocated
      free_disc_list(&disc_info_list);
    }
  }
  response->code = BAD_REQUEST;
  api_default_response(request, response);
}

/**
 * @brief get the rescan status
 */
void api_rescan_status(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_rescan_status: [%s][%s]", cde->root_folder, request->path);
  if (cde == NULL || db == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  // create json response by returning the database mode (including the rescan status)
  set_body_from_database_mode(response);
}
 
/**
 * @brief rescan the stored discs on the filesystem to update the database
 */
void api_rescan_discs(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_rescan_discs: [%s][%s][%d]", cde->root_folder, request->path, request->purge);
  if (cde == NULL || db == NULL || db->mode == DB_CLOSED) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  if (db->mode == DB_RESCAN) {
    // rescan already in progress
    response->code = NO_CONTENT;
    api_default_response(request, response);
    return;   
  }
  if (db->mode == DB_NORMAL) {
    // start the rescan in a separate thread, kill the thread when the request is processed
    request->terminate_thread = 1;
    pthread_create(&db->thread, NULL, rescan_database_t, (void *)request);

    // rescan started
    response->code = NO_CONTENT;
    api_default_response(request, response);
    return;
  }
  response->code = RESOURCE_LOCKED;
  api_default_response(request, response);
}

/**
 * @brief get the discs database rebuild status
 */
void api_rebuild_status(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_rebuild_status: [%s][%s]", cde->root_folder, request->path);
  if (cde == NULL || db == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  // create json response by returning the database mode (including the rebuild status)
  set_body_from_database_mode(response);
}
 
/**
 * @brief rebuild the database with stored discs
 */
void api_rebuild_discs(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_rebuild_discs: [%s][%s][%d]", cde->root_folder, request->path, request->purge);
  if (cde == NULL || db == NULL || db->mode == DB_CLOSED) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  if (db->mode == DB_REBUILD) {
    // rescan already in progress
    response->code = NO_CONTENT;
    api_default_response(request, response);
    return;   
  }
  if (db->mode == DB_NORMAL) {
    // start the rebuild in a separate thread, kill the thread when the request is processed
    request->terminate_thread = 1;
    pthread_create(&db->thread, NULL, rebuild_database_t, (void *)request);

    // rebuild started
    response->code = NO_CONTENT;
    api_default_response(request, response);
    return;
  }
  response->code = RESOURCE_LOCKED;
  api_default_response(request, response);
}

/**
 * @brief get the database backup status
 */
void api_backup_status(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_backup_status: [%s][%s]", cde->root_folder, request->path);
  if (cde == NULL || db == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  // create json response by returning the database mode (including the backup status)
  set_body_from_database_mode(response);
}

/**
 * @brief backup the database with stored discs
 */
void api_backup_discs(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_backup_discs: [%s][%s]\n", cde->root_folder, request->path);
  if (cde == NULL || db == NULL || db->mode == DB_CLOSED) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  if (db->mode == DB_BACKUP) {
    // backup already in progress
    response->code = NO_CONTENT;
    api_default_response(request, response);
    return;   
  }
  if (db->mode == DB_NORMAL && backup_database(db) == 0) {
    // backup started
    response->code = NO_CONTENT;
    api_default_response(request, response);
    return;
  }
  response->code = RESOURCE_LOCKED;
  api_default_response(request, response);
}

/**
 * @brief get information of the specified stored disc
 */
void api_get_disc_by_id(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_get_disc_by_id: [%s][%s][%s]\n", cde->root_folder, request->path, request->resource_id);
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  disc *disc_info = NULL;
  if (get_disc_from_database(db, request->resource_id, 0, &disc_info) == 0) {
    set_body_from_disc_info(disc_info, response);
    cde_free_disc(&disc_info, -1);
    return;
  }
  if (disc_info != NULL) {
    cde_free_disc(&disc_info, -1);
  }
  response->code = NOT_FOUND;
  api_default_response(request, response);
}

/**
 * @brief edit the information of the specified disc
 */
void api_update_disc_info_by_id(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info_by_id: [%s][%s][%s]", cde->root_folder, request->path, request->resource_id);
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  // check if request data is available
  if (request->size <= 0 || request->data == NULL) {
    response->code = BAD_REQUEST;
    api_default_response(request, response);
  }
  // try to get the existing information from the specified disc
  disc *disc_info = NULL;
  if (get_disc_from_database(db, request->resource_id, 0, &disc_info) != 0) {
    cde_free_disc(&disc_info, -1);
    response->code = NOT_FOUND;
    api_default_response(request, response);  
    return;
  }
  // parse json data
  json_token token;
  if (json_init(&token, request->data, request->size) == json_ok) {
    cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info_by_id: parsed json data");

    int validation_status = 0;
    char *value = calloc(JSON_UTILS_MAX_TOKEN_STR_SIZE, sizeof(char));
    char *db_id = calloc(JSON_UTILS_MAX_TOKEN_STR_SIZE, sizeof(char));
    char *disc_id = NULL;
    char *d_id_str = calloc(10, sizeof(char));
    
    int d_length = 0;
    char *d_artist = NULL;
    char *d_title = NULL;
    char *d_genre = NULL;
    int d_year = 0;
    char *d_extended = NULL;
    char *path = calloc(41, sizeof(char));
    track *tracks = calloc(disc_info->d_tracks, sizeof(track));

    if (json_select_member(&token, "$.id", db_id, JSON_UTILS_MAX_TOKEN_STR_SIZE, request->data) > 0) {
      
      // check if disc id of the posted data matches the disc id of the cached disc info
      json_get_string(&token, "disc_id", &disc_id, request->data);
      sprintf(d_id_str, "%08x", disc_info->d_id);
      if (strcasecmp(d_id_str, disc_id)==0) {

        // get disc information
        json_get_integer(&token, "length", &d_length, request->data);
        if (d_length <= 0) {
          cde_report(CDE_MSG_TYPE_ERROR, "api_update_disc_info_by_id: invalid disc length");
          validation_status = 3;            
        }

        if (validation_status == 0) {
          json_get_string(&token, "artist", &d_artist, request->data);
          json_get_string(&token, "title", &d_title, request->data);
          json_get_string(&token, "genre", &d_genre, request->data);
          json_get_integer(&token, "year", &d_year, request->data);  
          json_get_string(&token, "extended", &d_extended, request->data);
          
          // get track information
          for (int i=0; i < disc_info->d_tracks; i++) {

            snprintf(path, 40, "$.tracks.[%d].num", i);
            if (json_select_member(&token, path, value, JSON_UTILS_MAX_TOKEN_STR_SIZE, request->data) > 0) {
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info_by_id: path: %s; value: %s", path, value);

              tracks[i].t_num = 0;
              if (sscanf(value, "%d", &(tracks[i].t_num)) != 1) {
                cde_report(CDE_MSG_TYPE_ERROR, "api_update_disc_info_by_id: invalid track number");
                validation_status = 4;
                break;
              }

              if (tracks[i].t_num < 1 || tracks[i].t_num > disc_info->d_tracks) {
                cde_report(CDE_MSG_TYPE_ERROR, "api_update_disc_info_by_id: path: %s; invalid track number: %d", path, tracks[i].t_num);
                validation_status = 5;
                break;
              }
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info_by_id: path: %s; track number: %d", path, tracks[i].t_num);

              json_get_integer(&token, "length", &(tracks[i].t_length), request->data);
              
              if (tracks[i].t_length < 0 || tracks[i].t_length > disc_info->d_length) {
                cde_report(CDE_MSG_TYPE_ERROR, "api_update_disc_info_by_id: path: %s; invalid track length: %d frames; %d seconds", path, tracks[i].t_length, tracks[i].t_length / CDE_CD_FRAMES);
                validation_status = 6;
                break;
              }
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info_by_id: path: %s; track length: %d", path, tracks[i].t_length);

              json_get_string(&token, "title", &(tracks[i].t_title), request->data);
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info_by_id: path: %s; track title: %s", path, tracks[i].t_title);

              json_get_string(&token, "artist", &(tracks[i].t_artist), request->data);
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info_by_id: path: %s; track artist: %s", path, tracks[i].t_artist);

              json_get_string(&token, "album", &(tracks[i].t_album), request->data);
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info_by_id: path: %s; track album: %s", path, tracks[i].t_album);

              json_get_string(&token, "genre", &(tracks[i].t_genre), request->data);
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info_by_id: path: %s; track genre: %s", path, tracks[i].t_genre);

              json_get_integer(&token, "year", &(tracks[i].t_year), request->data);
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info_by_id: path: %s; track year: %d", path, tracks[i].t_year);

              json_get_string(&token, "extended", &(tracks[i].t_extended), request->data);
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info_by_id: path: %s; track extended: %s", path, tracks[i].t_extended);

              json_get_string(&token, "filename", &(tracks[i].t_filename), request->data);
              //cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_info_by_id: path: %s; track filename: %s", path, tracks[i].t_filename);
            }
          }
        }
      } else {
        cde_report(CDE_MSG_TYPE_ERROR, "api_update_disc_info_by_id: no match on disc id");
        validation_status = 2;
      }

    } else {
      cde_report(CDE_MSG_TYPE_ERROR, "api_update_disc_info_by_id: invalid json structure; id not found");
      validation_status = 1;
    }

    if (validation_status == 0) {
      // validation passed: update disc information
      if (d_artist != NULL && strlen(d_artist) > 0) {
        char *tmp = realloc(disc_info->d_artist, (strlen(d_artist) + 1) * sizeof(char));
        if (tmp != NULL) {
          disc_info->d_artist = tmp;
          strcpy(disc_info->d_artist, d_artist);
        }
      }
      if (d_title != NULL && strlen(d_title) > 0) {
        char *tmp = realloc(disc_info->d_title, (strlen(d_title) + 1) * sizeof(char));
        if (tmp != NULL) {
          disc_info->d_title = tmp;
          strcpy(disc_info->d_title, d_title);
        }
      }
      if (d_genre != NULL && strlen(d_genre) > 0) {
        char *tmp = realloc(disc_info->d_genre, (strlen(d_genre) + 1) * sizeof(char));
        if (tmp != NULL) {
          disc_info->d_genre = tmp;
          strcpy(disc_info->d_genre, d_genre);
        }
      }
      if (d_year > CDE_MIN_YEAR && d_year < CDE_MAX_YEAR) {
        disc_info->d_year = d_year;
      }
      if (d_extended != NULL && strlen(d_extended) > 0) {
        char *tmp = realloc(disc_info->d_extended, (strlen(d_extended) + 1) * sizeof(char));
        if (tmp != NULL) {
          disc_info->d_extended = tmp;
          strcpy(disc_info->d_extended, d_extended);
        }
      }
      // validation passed: update track information
      for (int i=0; i < disc_info->d_tracks; i++) {
        if (tracks[i].t_num != 0) {
          for (int j=0; j < disc_info->d_tracks; j++) {
            if (tracks[i].t_num == disc_info->tracks[j].t_num) {

              if (tracks[i].t_title != NULL && strlen(tracks[i].t_title) > 0) {
                char *tmp = realloc(disc_info->tracks[j].t_title, (strlen(tracks[i].t_title) + 1) * sizeof(char));
                if (tmp != NULL) {
                  disc_info->tracks[j].t_title = tmp;
                  strcpy(disc_info->tracks[j].t_title, tracks[i].t_title);
                }
              }
              if (tracks[i].t_artist != NULL && strlen(tracks[i].t_artist) > 0) {
                char *tmp = realloc(disc_info->tracks[j].t_artist, (strlen(tracks[i].t_artist) + 1) * sizeof(char));
                if (tmp != NULL) {
                  disc_info->tracks[j].t_artist = tmp;
                  strcpy(disc_info->tracks[j].t_artist, tracks[i].t_artist);
                }
              }
              if (tracks[i].t_album != NULL && strlen(tracks[i].t_album) > 0) {
                char *tmp = realloc(disc_info->tracks[j].t_album, (strlen(tracks[i].t_album) + 1) * sizeof(char));
                if (tmp != NULL) {
                  disc_info->tracks[j].t_album = tmp;
                  strcpy(disc_info->tracks[j].t_album, tracks[i].t_album);
                }
              }
              if (tracks[i].t_genre != NULL && strlen(tracks[i].t_genre) > 0) {
                char *tmp = realloc(disc_info->tracks[j].t_genre, (strlen(tracks[i].t_genre) + 1) * sizeof(char));
                if (tmp != NULL) {
                  disc_info->tracks[j].t_genre = tmp;
                  strcpy(disc_info->tracks[j].t_genre, tracks[i].t_genre);
                }
              }
              if (tracks[i].t_year > CDE_MIN_YEAR && d_year < CDE_MAX_YEAR) {
                disc_info->tracks[j].t_year = tracks[i].t_year;
              }
              if (tracks[i].t_extended != NULL && strlen(tracks[i].t_extended) > 0) {
                char *tmp = realloc(disc_info->tracks[j].t_extended, (strlen(tracks[i].t_extended) + 1) * sizeof(char));
                if (tmp != NULL) {
                  disc_info->tracks[j].t_extended = tmp;
                  strcpy(disc_info->tracks[j].t_extended, tracks[i].t_extended);
                }
              }
              if (tracks[i].t_filename != NULL && strlen(tracks[i].t_filename) > 0) {
                char *tmp = realloc(disc_info->tracks[j].t_filename, (strlen(tracks[i].t_filename) + 1) * sizeof(char));
                if (tmp != NULL) {
                  disc_info->tracks[j].t_filename = tmp;
                  strcpy(disc_info->tracks[j].t_filename, tracks[i].t_filename);
                }
              }
              break;
            }
          }
        }
      }
      // validation passed: update the disc information in the database
      if (store_disc_in_database(db, disc_info, 1) != 0) {
        cde_report(CDE_MSG_TYPE_ERROR, "api_update_disc_info_by_id: unable to update disc information: (%d) %s", db->status, db->msg);
      }
      // return updated disc information
      set_body_from_disc_info(disc_info, response);
    } else {
      // validation failed
      response->code = BAD_REQUEST;
      api_default_response(request, response);
    }

    // cleanup
    for (int i=0; i < disc_info->d_tracks; i++) {
      if (tracks[i].t_title != NULL) {
        free(tracks[i].t_title);
      }
      if (tracks[i].t_artist != NULL) {
        free(tracks[i].t_artist);
      }
      if (tracks[i].t_album != NULL) {
        free(tracks[i].t_album);
      }
      if (tracks[i].t_genre != NULL) {
        free(tracks[i].t_genre);
      }
      if (tracks[i].t_extended != NULL) {
        free(tracks[i].t_extended);
      }
      if (tracks[i].t_filename != NULL) {
        free(tracks[i].t_filename);
      }      
    }
    free(tracks);
    free(path);
    if (d_extended != NULL) {
      free(d_extended);
    }
    if (d_genre != NULL) {
      free(d_genre);
    }
    if (d_title != NULL) {
      free(d_title);
    }
    if (d_artist != NULL) {
      free(d_artist);
    }
    if (disc_id != NULL) {
      free(disc_id);
    }
    if (db_id != NULL) {
      free(db_id);
    }
    free(d_id_str);
    free(value);
    cde_free_disc(&disc_info, -1);
    return;
  }
  // unable to parse json data
  cde_free_disc(&disc_info, -1);
  response->code = BAD_REQUEST;
  api_default_response(request, response);
}

/**
 * @brief get the front cover of the specified disc
 */
void api_get_disc_front_cover_by_id(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_get_disc_front_cover_by_id: [%s][%s][%s]", cde->root_folder, request->path, request->resource_id);
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  char *cover_data = NULL;
  int cover_size = 0;
  if (get_cover_from_database(db, request->resource_id, 0, &cover_data, &cover_size) == 0 && cover_size > 0) {
    data_response(cover_data, cover_size, MIME_TYPE_JPEG, response);
    if (cover_data != NULL) {
      free(cover_data);
    }
    return;
  }
  if (cover_data != NULL) {
    free(cover_data);
  }
  if (request->return_default == 1) {
    file_response(DEFAULT_FRONT_COVER_FILENAME, cde->root_folder, response);
    return;
  }
  response->code = NOT_FOUND;
  api_default_response(request, response);
}

/**
 * @brief update the front cover of the specified disc
 */
void api_update_disc_front_cover_by_id(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_front_cover_by_id: [%s][%s][%s]", cde->root_folder, request->path, request->resource_id);
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  // check if request data is available
  if (request->size < 140 || request->data == NULL || request->resource_id == NULL) {
    response->code = BAD_REQUEST;
    api_default_response(request, response);
    return;
  }
  // check if the disc identified by the resource id is already stored
  long disc_id = exists_disc_id(db, request->resource_id);
  // try to update the front cover linked to given disc information
  if (disc_id > 0 && update_cover_in_database(db, disc_id, 0, request->data, (int)request->size) == 0) {
    response->code = NO_CONTENT;
    api_default_response(request, response);
    return;
  }
  response->code = NOT_FOUND;
  api_default_response(request, response);
}

/**
 * @brief get the back cover of the specified disc
 */
void api_get_disc_back_cover_by_id(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_get_disc_back_cover_by_id: [%s][%s][%s]", cde->root_folder, request->path, request->resource_id);
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  char *cover_data = NULL;
  int cover_size = 0;
  if (get_cover_from_database(db, request->resource_id, 1, &cover_data, &cover_size) == 0 && cover_size > 0) {
    data_response(cover_data, cover_size, MIME_TYPE_JPEG, response);
    if (cover_data != NULL) {
      free(cover_data);
    }
    return;
  }
  if (cover_data != NULL) {
    free(cover_data);
  }
  if (request->return_default == 1) {
    file_response(DEFAULT_BACK_COVER_FILENAME, cde->root_folder, response);
    return;
  }
  response->code = NOT_FOUND;
  api_default_response(request, response);
}

/**
 * @brief update the back cover of the specified disc
 */
void api_update_disc_back_cover_by_id(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_update_disc_back_cover_by_id: [%s][%s][%s]", cde->root_folder, request->path, request->resource_id);
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  // check if request data is available
  if (request->size < 140 || request->data == NULL || request->resource_id == NULL) {
    response->code = BAD_REQUEST;
    api_default_response(request, response);
    return;
  }
  // check if the disc identified by the resource id is already stored
  long disc_id = exists_disc_id(db, request->resource_id);
  // try to update the back cover linked to given disc information
  if (disc_id > 0 && update_cover_in_database(db, disc_id, 1, request->data, (int)request->size) == 0) {
    response->code = NO_CONTENT;
    api_default_response(request, response);
    return;
  }
  response->code = NOT_FOUND;
  api_default_response(request, response);
}

/**
 * @brief get the audio data of the specified disc (and track)
 */
void api_get_audio_by_id(http_request *request, http_response *response) {
  cde_report(CDE_MSG_TYPE_DEBUG, "api_get_audio_by_id: [%s][%s][%s][%d][%d]", cde->root_folder, request->path, request->resource_id, request->track, request->format);
  if (cde == NULL) {
    response->code = SERVICE_UNAVAILABLE;
    api_default_response(request, response);
    return;
  }
  disc *disc_info = NULL;
  if (get_disc_from_database(db, request->resource_id, 0, &disc_info) == 0) {
    // check if retrieved disc information has a filename for the requested track
    if (disc_info->d_tracks >= request->track && request->track > 0 &&
        disc_info->tracks != NULL &&
        disc_info->tracks[request->track - 1].t_filename != NULL) {

      // return the audio data of the requested track
      api_audio_response(request, response, &(disc_info->tracks[request->track-1]));

    } else {
      // no audio tracks available or invalid track number
      response->code = BAD_REQUEST;
      api_default_response(request, response);
    }
  } else {
    // unable to retrieve disc information from database
    response->code = NOT_FOUND;
    api_default_response(request, response);
  }
  if (disc_info != NULL) {
    cde_free_disc(&disc_info, -1);
  }
}

/**
 * @brief callback to get chunks of flac encoded audio data
 * 
 *        The callback has to copy at most max bytes of content into buf. 
 *        The total number of bytes that has been placed into buf should be returned.
 *        Note that returning zero will cause MHD to try again. 
 *        Thus, returning zero should only be used in conjunction with MHD_suspend_connection() to avoid busy waiting.
 *        While usually the callback simply returns the number of bytes written into buf, there are two special return values:
 *
 *        MHD_CONTENT_READER_END_OF_STREAM (-1) should be returned for the regular end of transmission (with chunked encoding, 
 *        MHD will then terminate the chunk and send any HTTP footers that might be present; without chunked encoding and given an unknown response size, 
 *        MHD will simply close the connection; note that while returning MHD_CONTENT_READER_END_OF_STREAM is not technically legal if a response size was 
 *        specified, MHD accepts this and treats it just as MHD_CONTENT_READER_END_WITH_ERROR.
 *
 *        MHD_CONTENT_READER_END_WITH_ERROR (-2) is used to indicate a server error generating the response; this will cause MHD to simply close the connection immediately. 
 *        If a response size was given or if chunked encoding is in use, this will indicate an error to the client. 
 *        Note, however, that if the client does not know a response size and chunked encoding is not in use,
 *        then clients will not be able to tell the difference between MHD_CONTENT_READER_END_WITH_ERROR and MHD_CONTENT_READER_END_OF_STREAM. 
 *        This is not a limitation of MHD but rather of the HTTP protocol.
 *
 * @param cls the context of the callback
 * @param pos the position in the data stream to access (MHD_Response is not re-used, MHD guarantees that pos will be the sum of all non-negative return values obtained from the content reader so far)
 * @param buf the buffer to write the data to
 * @param max the maximum number of bytes to read
 * @return length of the returned flac encoded audio data, -1 at end of stream and -2 if an error occured
 */
ssize_t api_callback_flac_encoder(void *cls, uint64_t pos, char *buf, size_t max) {
  if (cls != NULL) {
    response_data_context *rdc = (response_data_context *)cls;
    if (rdc->fp != NULL && rdc->codec_context != NULL) {
      flac_encoder_context *encoder_context = (flac_encoder_context *)rdc->codec_context;
      return write_flac_to_buffer(encoder_context, rdc->fp, buf, max);
    }
  }
  return -2; // no file pointer or flac encoder available
}

/**
 * @brief cleanup the resources used by the flac encoder callback
 * @param cls the context of the callback
 */
void api_callback_flac_encoder_cleanup(void *cls) {
  if (cls != NULL) {
    response_data_context *rdc = (response_data_context *)cls;
    if (rdc->fp != NULL) {
        // close the wav file
        wav_close(rdc->fp);
        rdc->fp = NULL;
    }
    if (rdc->codec_context != NULL) {
      flac_encoder_context *encoder_context = (flac_encoder_context *)rdc->codec_context;
      end_flac(encoder_context);
      free(encoder_context);
    }
    free(rdc);
    cls = NULL;
  }
}

/**
 * @brief callback to get chunks of flac dencoded audio data
 * @param cls the context of the callback
 * @param pos the position in the file to start reading from
 * @param buf the buffer to write the data to
 * @param max the maximum number of bytes to read
 * @return length of the returned data
 */
ssize_t api_callback_flac_decoder(void *cls, uint64_t pos, char *buf, size_t max) {
  if (cls != NULL) {
    response_data_context *rdc = (response_data_context *)cls;
    if (rdc->codec_context != NULL) {
      return flac_read((flac_decoder_context *)rdc->codec_context, buf, max);
    }
  }
  return -1; // no file pointer or flac decoder available
}

/**
 * @brief cleanup the resources used by the flac decoder callback
 * @param cls the context of the callback
 */
void api_callback_flac_decoder_cleanup(void *cls) {
  if (cls != NULL) {
    response_data_context *rdc = (response_data_context *)cls;
    if (rdc->codec_context != NULL) {
      flac_decoder_context *decoder_context = (flac_decoder_context *)rdc->codec_context;
      flac_close(decoder_context);
      free(decoder_context);
    }
    free(rdc);
    cls = NULL;
  }
}

/**
 * @brief callback to get chunks of (audio) data from a file
 * @param cls the context of the callback
 * @param pos the position in the file to start reading from
 * @param buf the buffer to write the data to
 * @param max the maximum number of bytes to read
 * @return length of the returned data
 */
ssize_t api_callback_file_reader(void *cls, uint64_t pos, char *buf, size_t max) {
  if (cls != NULL) {
    response_data_context *rdc = (response_data_context *)cls;
    if (rdc->fp != NULL) {
      (void)  fseek(rdc->fp, pos + rdc->skip_bytes, SEEK_SET);
      size_t bytes_read = fread(buf, 1, max, rdc->fp);
      if (bytes_read == 0 && ferror(rdc->fp)) {
        return -1; // error reading from file
      }
      rdc->total_read += bytes_read;
      return bytes_read;
    }
  }
  return -1; // no file pointer available
}

/**
 * @brief cleanup the resources used by the file data callback
 * @param cls the context of the callback, which is the file
 */
void api_callback_file_reader_cleanup(void *cls) {
  if (cls != NULL) {
    response_data_context *rdc = (response_data_context *)cls;
    if (rdc->fp != NULL) {
      fclose(rdc->fp);
    }
    free(rdc);
    cls = NULL;
  }
}

/**
 * @brief helper function to initialize the response_data_context used by the reader callback functions.
 *        This function will open the input file, check the file size and prepare the response for streaming the file data.
 * @param response 
 * @param filename
 * @param mime_type
 * @return 0 on success, non-zero on failure
 */
int api_init_response_data_context(http_response *response, char *filename, int file_format, http_mime_type mime_type) {
  
  // create absolute path by concatenating the root folder and the filename (including the relative path)
  char *absolute_path = get_full_path(cde->root_folder, filename);
  if (absolute_path == NULL) {
    // unable to allocate memory for absolute path
    response->code = INTERNAL_SERVER_ERROR;
    return -1;
  }

  // create a response data context for the file reader
  response_data_context *rdc = calloc(1, sizeof(response_data_context));
  if (rdc == NULL) {
    // unable to allocate memory for response data context
    response->code = INTERNAL_SERVER_ERROR;
    // cleanup path strings
    free(absolute_path);
    return -1;
  }
  size_t min_file_size = 0;
  if (file_format == CDE_OUTPUT_TYPE_WAV) {
    // minimum file size for wav files is the 44 bytes header
    min_file_size = WAV_HEADER_SIZE;
  }
  if (file_format == CDE_OUTPUT_TYPE_FLAC) {
    // minimum file size for flac files is 42 bytes
    min_file_size = 42;
  }
  rdc->total_size = file_size(absolute_path);
  rdc->total_read = 0;
  rdc->skip_bytes = 0;
  if (mime_type == MIME_TYPE_PCM) {
    // pcm files have no header, so we skip the first 44 bytes from the wav file
    rdc->skip_bytes = WAV_HEADER_SIZE;
  }
  if (rdc->total_size <= min_file_size) {
    // file not found or too small
    response->code = NOT_FOUND;
    // cleanup response data context
    free(rdc);
    free(absolute_path);
    return -1;
  }

  // not using any encoder/decoder for the file data
  rdc->codec_context = NULL; 

  // open the input file
  rdc->fp = fopen(absolute_path, "rb");

  if (rdc->fp == NULL) {
    // unable to open the file
    response->code = NOT_FOUND;
    // cleanup response data context
    free(rdc);
    free(absolute_path);
    return -1;
  }

  // set the callback for streaming the file data
  response->callback_context = rdc;
  response->callback_block_size = MAX_BLOCK_RESPONSE;
  response->size = rdc->total_size;
  response->mime_type = mime_type;
  response->content_type = get_content_type(response->mime_type);
  response->code = OK;

  // cleanup path string
  free(absolute_path);

  return 0;
}

/**
 * @brief get a response message by returning the requested audio data
 */
void api_audio_response(http_request *request, http_response *response, track* track_info) {
  if (request->format == 1) {

    // request for audio data in flac format
    if (ends_with(".flac", track_info->t_filename)) {
      // file already in flac format

      if (api_init_response_data_context(response, track_info->t_filename, CDE_OUTPUT_TYPE_FLAC, MIME_TYPE_FLAC) != 0) {
        goto finalize_response;
      }
      response->callback = api_callback_file_reader;
      response->callback_cleanup = api_callback_file_reader_cleanup;

    } else if (ends_with(".wav", track_info->t_filename)) {
      // convert wav to flac

      if (api_init_response_data_context(response, track_info->t_filename, CDE_OUTPUT_TYPE_WAV, MIME_TYPE_FLAC) != 0) {
        goto finalize_response;
      }

      // create the flac_encoder_context
      response_data_context *rdc = (response_data_context *)response->callback_context;
      flac_encoder_context *encoder_context = calloc(1, sizeof(flac_encoder_context));
      if (encoder_context == NULL) {
        // unable to allocate memory for the flac_decoder_context
        response->code = INTERNAL_SERVER_ERROR;
        // cleanup response data context
        free(rdc);
        response->callback_context = NULL;
        goto finalize_response;
      }
      encoder_context->format = CDE_OUTPUT_TYPE_FLAC; // request flac format

      // read the wav header to get the audio data length
      int length_in_frames = 0;
      long length_in_bytes = 0;
      if (wav_open(rdc->fp, &length_in_frames, &length_in_bytes, NULL, track_info) != 0) {
        // invalid flac file
        response->code = INTERNAL_SERVER_ERROR;
        // close the wav file
        wav_close(rdc->fp);
        // cleanup response data and encoder context
        free(encoder_context);
        free(rdc);
        goto finalize_response;
      }

      cde_report(CDE_MSG_TYPE_DEBUG, "api_audio_response: convert wav to flac: frames:[%d] bytes:[%ld] file:[%ld]\n", length_in_frames, length_in_bytes, rdc->total_size-WAV_HEADER_SIZE);

      // start flac encoder to create a stream with encoded audio data
      if (start_flac(NULL, rdc->total_size-WAV_HEADER_SIZE, track_info, encoder_context) != 0) { 
        // invalid wav file
        response->code = INTERNAL_SERVER_ERROR;
        // close the flac file
        end_flac(encoder_context);
        // close the wav file
        wav_close(rdc->fp);
        // cleanup response data and encoder context
        free(encoder_context);
        free(rdc);
        goto finalize_response;
      }

      // set the callback for encoding the wav file to flac data
      rdc->codec_context = (void*)encoder_context;
      response->callback = api_callback_flac_encoder;
      response->callback_cleanup = api_callback_flac_encoder_cleanup;
      response->size = SIZE_UNKNOWN_RESPONSE;

    } else {
      // other audio source formats are not supported
      response->code = NOT_IMPLEMENTED;
    }

  } else if (request->format == 0) {

    // request for audio data in wav format
    if (ends_with(".wav", track_info->t_filename)) {
      // file already in wav format

      if (api_init_response_data_context(response, track_info->t_filename, CDE_OUTPUT_TYPE_WAV, MIME_TYPE_WAV) != 0) {
        goto finalize_response;
      }
      response->callback = api_callback_file_reader;
      response->callback_cleanup = api_callback_file_reader_cleanup;

    } else if (ends_with(".flac", track_info->t_filename)) {
      // convert flac to wav
      if (api_init_response_data_context(response, track_info->t_filename, CDE_OUTPUT_TYPE_FLAC, MIME_TYPE_WAV) != 0) {
        goto finalize_response;
      }

      // create the flac_decoder_context
      response_data_context *rdc = (response_data_context *)response->callback_context;
      flac_decoder_context *decoder_context = calloc(1, sizeof(flac_decoder_context));
      if (decoder_context == NULL) {
        // unable to allocate memory for the flac_decoder_context
        response->code = INTERNAL_SERVER_ERROR;
        // cleanup response data context
        free(rdc);
        response->callback_context = NULL;
        goto finalize_response;
      }
      decoder_context->format = CDE_OUTPUT_TYPE_WAV; // request wav format including wav header

      // read the flac metadata to get the audio data length
      if (flac_open(rdc->fp, NULL, track_info, decoder_context) != 0) {
        // invalid flac file
        response->code = INTERNAL_SERVER_ERROR;
        // close the flac file
        flac_close((flac_decoder_context *)rdc->codec_context);
        // cleanup response data and decoder context
        free(decoder_context);
        free(rdc);
        goto finalize_response;
      }
          
      // set the callbacks for decoding the flac data
      rdc->codec_context = (void*)decoder_context;
      response->callback = api_callback_flac_decoder;
      response->callback_cleanup = api_callback_flac_decoder_cleanup;
      response->size = decoder_context->length_in_bytes + WAV_HEADER_SIZE;

    } else {
      // other audio source formats are not supported
      response->code = NOT_IMPLEMENTED;
    }
    
  } else if (request->format == 2) {

    // request for audio data in pcm format
    if (ends_with(".wav", track_info->t_filename)) {
      // no real coversion needed as wav files are already in pcm format, we only need to skip the wav header
      if (api_init_response_data_context(response, track_info->t_filename, CDE_OUTPUT_TYPE_WAV, MIME_TYPE_PCM) != 0) {
        goto finalize_response;
      }
      response->callback = api_callback_file_reader;
      response->callback_cleanup = api_callback_file_reader_cleanup;

    } else if (ends_with(".flac", track_info->t_filename)) {

      // convert flac to pcm
      if (api_init_response_data_context(response, track_info->t_filename,CDE_OUTPUT_TYPE_FLAC, MIME_TYPE_PCM) != 0) {
        goto finalize_response;
      }

      // create the flac_decoder_context
      response_data_context *rdc = (response_data_context *)response->callback_context;
      flac_decoder_context *decoder_context = calloc(1, sizeof(flac_decoder_context));
      if (decoder_context == NULL) {
        // unable to allocate memory for the flac_decoder_context
        response->code = INTERNAL_SERVER_ERROR;
        // cleanup response data context
        free(rdc);
        response->callback_context = NULL;
        goto finalize_response;
      }
      decoder_context->format = 2; // request pcm format

      // read the flac metadata to get the audio data length
      if (flac_open(rdc->fp, NULL, track_info, decoder_context) != 0) {
        // invalid flac file
        response->code = INTERNAL_SERVER_ERROR;
        // close the flac file
        flac_close((flac_decoder_context *)rdc->codec_context);
        // cleanup response data and decoder context
        free(decoder_context);
        free(rdc);
        goto finalize_response;
      }
          
      // set the callbacks for decoding the flac data
      rdc->codec_context = (void*)decoder_context;
      response->callback = api_callback_flac_decoder;
      response->callback_cleanup = api_callback_flac_decoder_cleanup;
      response->size = decoder_context->length_in_bytes + WAV_HEADER_SIZE;

    } else {
      // other audio source formats are not supported
      response->code = NOT_IMPLEMENTED;
    }

  } else {

    // no specific format requested. return flac or wav audio data without conversion
    if (ends_with(".flac", track_info->t_filename)) {
      // return flac file
      if (api_init_response_data_context(response, track_info->t_filename, CDE_OUTPUT_TYPE_FLAC, MIME_TYPE_FLAC) != 0) {
        goto finalize_response;
      }
      response->callback = api_callback_file_reader;
      response->callback_cleanup = api_callback_file_reader_cleanup;

    } else if (ends_with(".wav", track_info->t_filename)) {
      // return wav file
      if (api_init_response_data_context(response, track_info->t_filename, CDE_OUTPUT_TYPE_WAV, MIME_TYPE_WAV) != 0) {
        goto finalize_response;
      }
      response->callback = api_callback_file_reader;
      response->callback_cleanup = api_callback_file_reader_cleanup;

    } else {
      // other audio source formats are not supported
      response->code = NOT_IMPLEMENTED;
    }

  }
finalize_response:
  if (response->code != OK) {
    // audio file could not be read or converted
    api_default_response(request, response);
  }
}

/**
 * @brief get a response message by returning a file
 */
void api_file_response(http_request *request, http_response *response) {
  if (cde == NULL) {
    default_response(SERVICE_UNAVAILABLE, NULL, response);
    return;
  }
  // read file and return it as response
  file_response(request->path, cde->root_folder, response);
  if (response->code != OK) {
    // file could not be read and response code already set (Not Found)
    api_default_response(request, response);
  }
}

/**
 * @brief get an API default response message (json)
 */
void api_default_response(http_request *request, http_response *response) {
  response->mime_type = MIME_TYPE_JSON;
  response->content_type = get_content_type(response->mime_type);
  if (response->code == NO_CONTENT) {
    response->size = 0;
    response->body = calloc(1, sizeof(char));
    return;
  }
  if (response->code == BAD_REQUEST) {
    response->size = strlen(json_response_400);
    response->body = calloc(response->size+1, sizeof(char));
    strcpy(response->body, json_response_400);
    return;
  }
  if (response->code == FORBIDDEN) {
    response->size = strlen(json_response_403);
    response->body = calloc(response->size+1, sizeof(char));
    strcpy(response->body, json_response_403);
    return;
  }
  if (response->code == NOT_FOUND) {
    response->size = strlen(json_response_404);
    response->body = calloc(response->size+1, sizeof(char));
    strcpy(response->body, json_response_404);
    return;
  }
  if (response->code == METHOD_NOT_ALLOWED) {
    response->size = strlen(json_response_405);
    response->body = calloc(response->size+1, sizeof(char));
    strcpy(response->body, json_response_405);
    return;
  }
  if (response->code == CONFLICT) {
    response->size = strlen(json_response_409);
    response->body = calloc(response->size+1, sizeof(char));
    strcpy(response->body, json_response_409);
    return;
  }
  if (response->code == RESOURCE_LOCKED) {
    response->size = strlen(json_response_423);
    response->body = calloc(response->size+1, sizeof(char));
    strcpy(response->body, json_response_423);
    return;
  }
  if (response->code == NOT_IMPLEMENTED) {
    response->size = strlen(json_response_501);
    response->body = calloc(response->size+1, sizeof(char));
    strcpy(response->body, json_response_501);
    return;
  }
  if (response->code == SERVICE_UNAVAILABLE) {
    response->size = strlen(json_response_503);
    response->body = calloc(response->size+1, sizeof(char));
    strcpy(response->body, json_response_503);
    return;
  }
  // unspecified behaviour: respond with Internal Server Error
  response->code = INTERNAL_SERVER_ERROR;
  response->size = strlen(json_response_500);
  response->body = calloc(response->size+1, sizeof(char));
  strcpy(response->body, json_response_500);
}

/**
 * @brief get the API response message based on the path and request method
 */
void api_response(http_request *request, http_response *response) {
  // get the request method
  int method_id = get_request_method(request->method);
  // try to find a match on the path
  for (int i=0; i<OPERATIONS_PATH_COUNT; i++) {
    if (strcmp(operations[i].path, request->path) == 0) {
      // path match: try to match the request method
      for (int j=0; j<OPERATIONS_METHOD_COUNT && operations[i].method_functions[j].method!=INVALID; j++) {
        if (operations[i].method_functions[j].method == method_id) {
          // match on path and request method: call operation to get the response message
          (operations[i].method_functions[j].func)(request, response);
          return;
        }
      }
      // match on path, but not on request method: return 405 Method Not Allowed
      response->code = METHOD_NOT_ALLOWED;
      api_default_response(request, response);
      return;
    } else if (strchr(operations[i].path, '*') != NULL) {
      // no direct match: try to find a match on wildcard
      char *head = calloc(20, sizeof(char));
      char *tail = calloc(20, sizeof(char));
      int has_split = split(&head, &tail, operations[i].path, '*');
      int has_start = starts_with(head, request->path);
      int has_end = ends_with(tail, request->path);
      free(head);
      free(tail);
      if (has_split && has_start && has_end) {
        // path match on wild card: try to match the request method
        for (int j=0; j<OPERATIONS_METHOD_COUNT && operations[i].method_functions[j].method!=INVALID; j++) {
          if (operations[i].method_functions[j].method == method_id) {
            // match on path and request method: call operation to get the response message
            (operations[i].method_functions[j].func)(request, response);
            return;
          }
        }
        // match on path with wildcard, but not on request method: return 405 Method Not Allowed
        response->code = METHOD_NOT_ALLOWED;
        api_default_response(request, response);
        return;
      }
    }
  }
  // no match on API path: return 404 Not Found html message
  default_response(NOT_FOUND, NULL, response);
}
