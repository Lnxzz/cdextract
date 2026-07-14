/**************************************************************************

  cdextract - server API functions

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

#ifndef CDE_API_H
#define CDE_API_H

#include <stdint.h>
#include <time.h>

#include "request.h"
#include "response.h"
#include "libcdextract_types.h"


#define OPERATIONS_METHOD_COUNT 4                     // number of possible request methods for a single path definition
#define OPERATIONS_PATH_COUNT 17                      // number of path definitions


/**
 * @brief method function lookup table
 */
typedef struct operation_method_function {
  int method;
  void(*func)(http_request*, http_response*);
} operation_method_function;

/**
 * @brief path lookup table
 */
typedef struct operation_path {
  const char *path;
  const struct operation_method_function method_functions[OPERATIONS_METHOD_COUNT];
} operation_path;

/**
 * @brief structure for storing an audio extraction progress update
 */
typedef struct progress_state {
  int rpt_type;
  char rpt_type_str[10];
  int function;
  char function_str[16];
  int track;
  long sector;
  float percentage;
  clock_t last_update;
  clock_t last_request;
} progress_state;

/**
 * @brief structure for storing the response data context used by callback functions
 */
typedef struct response_data_context {
  FILE *fp;                // file pointer to the data file
  size_t total_size;       // total size of the data to be read
  size_t total_read;       // total size of the data read so far
  size_t skip_bytes;       // number of bytes to skip before reading data
  void *codec_context;     // the encoder/decoder to transform the data
} response_data_context;


/**
 * @brief initialize the api including the cdextract library  
 *        context and the database connection
 */
extern void api_init(char *device_name, char *audio_folder, char *cddb_folder, char *web_folder, char *db_filename, int db_backup);

/**
 * @brief clean the api context including the cdextract   
 *        library and the database connection
 */
extern void api_cleanup();

/***
 * @brief set the given option
 */
extern void api_set_option(int option, int varg);

/**
 * @brief get information of the connected cdrom drive
 */
extern void api_get_drive_info(http_request *request, http_response *response);

/**
 * @brief open the connection with cdrom drive
 */
extern void api_open_drive(http_request *request, http_response *response);

/**
 * @brief close the connection with cdrom drive
 */
extern void api_close_drive(http_request *request, http_response *response);

/**
 * @brief get information of the disc in the cdrom drive
 */
extern void api_get_disc_info(http_request *request, http_response *response);

/**
 * @brief edit the information of the disc in the cdrom drive
 */
extern void api_update_disc_info(http_request *request, http_response *response);

/**
 * @brief insert a disc into the cdrom drive
 */
extern void api_insert_disc(http_request *request, http_response *response);

/**
 * @brief eject the disc from the cdrom drive
 */
extern void api_eject_disc(http_request *request, http_response *response);

/**
 * @brief get the audio extraction progress status
 */
extern void api_get_extract_disc_progress(http_request *request, http_response *response);

/**
 * @brief callback to get the audio extraction progress status
 * @return length of the returned data
 */
extern ssize_t api_callback_extract_disc_progress(void *cls, uint64_t pos, char *buf, size_t max);

/**
 * @brief helper function to set the audio extraction progress status
 */
extern void api_set_extract_disc_progress(int rpt_type, int function, int track, long sector, float percentage);

/**
 * @brief extract the audio from the disc in the cdrom drive
 */
extern void api_extract_disc(http_request *request, http_response *response);

/**
 * @brief cancel the audio extraction from disc
 */
extern void api_cancel_disc(http_request *request, http_response *response);

/**
 * @brief get the front cover of the disc in the cdrom drive
 */
extern void api_get_disc_front_cover(http_request *request, http_response *response);

/**
 * @brief update the front cover of the disc in the cdrom drive
 */
extern void api_update_disc_front_cover(http_request *request, http_response *response);

/**
 * @brief get the back cover of the disc in the cdrom drive
 */
extern void api_get_disc_back_cover(http_request *request, http_response *response);

/**
 * @brief update the back cover of the disc in the cdrom drive
 */
extern void api_update_disc_back_cover(http_request *request, http_response *response);

/**
 * @brief get the audio data of the disc (and track) in the cdrom drive
 */
extern void api_get_audio(http_request *request, http_response *response);

/**
 * @brief get the discs database rescan status
 */
extern void api_rescan_status(http_request *request, http_response *response);
 
/**
 * @brief rescan the stored discs on the filesystem to update the database
 */
extern void api_rescan_discs(http_request *request, http_response *response);

/**
 * @brief get the discs database rebuild status
 */
extern void api_rebuild_status(http_request *request, http_response *response);

/**
 * @brief rebuild the database with stored discs
 */
extern void api_rebuild_discs(http_request *request, http_response *response);

/**
 * @brief get the discs database backup status
 */
extern void api_backup_status(http_request *request, http_response *response);

/**
 * @brief backup the database with stored discs
 */
extern void api_backup_discs(http_request *request, http_response *response);

/**
 * @brief get the discs database metadata including the 
 *        total number of discs, tracks and artists
 */
extern void api_get_discs_metadata(http_request *request, http_response *response);

/**
 * @brief list all stored discs
 */
extern void api_list_discs(http_request *request, http_response *response);

/**
 * @brief get information of the specified stored disc
 */
extern void api_get_disc_by_id(http_request *request, http_response *response);

/**
 * @brief edit the information of the specified disc
 */
extern void api_update_disc_info_by_id(http_request *request, http_response *response);

/**
 * @brief get the front cover of the specified disc
 */
extern void api_get_disc_front_cover_by_id(http_request *request, http_response *response);

/**
 * @brief update the front cover of the specified disc
 */
extern void api_update_disc_front_cover_by_id(http_request *request, http_response *response);

/**
 * @brief get the back cover of the specified disc
 */
extern void api_get_disc_back_cover_by_id(http_request *request, http_response *response);

/**
 * @brief update the back cover of the specified disc
 */
extern void api_update_disc_back_cover_by_id(http_request *request, http_response *response);

/**
 * @brief get the audio data of the specified disc (and track)
 */
extern void api_get_audio_by_id(http_request *request, http_response *response);

/**
 * @brief callback to get chunks of flac encoded audio data
 * @param cls the context of the callback
 * @param pos the position in the data stream to access (MHD_Response is not re-used, MHD guarantees that pos will be the sum of all non-negative return values obtained from the content reader so far)
 * @param buf the buffer to write the data to
 * @param max the maximum number of bytes to read
 * @return length of the returned flac encoded audio data, -1 at end of stream and -2 if an error occured
 */
extern ssize_t api_callback_flac_encoder(void *cls, uint64_t pos, char *buf, size_t max);

/**
 * @brief cleanup the resources used by the flac encoder callback
 * @param cls the context of the callback
 */
extern void api_callback_flac_encoder_cleanup(void *cls);

/**
 * @brief callback to get chunks of flac decoded audio data
 * @param cls the context of the callback
 * @param pos the position in the file to start reading from
 * @param buf the buffer to write the data to
 * @param max the maximum number of bytes to read
 * @return length of the returned data
 */
extern ssize_t api_callback_flac_decoder(void *cls, uint64_t pos, char *buf, size_t max);

/**
 * @brief cleanup the resources used by the flac decoder callback
 * @param cls the context of the callback
 */
extern void api_callback_flac_decoder_cleanup(void *cls);

/**
 * @brief callback to get chunks of (audio) data from a file
 * @param cls the context of the callback
 * @param pos the position in the file to start reading from
 * @param buf the buffer to write the data to
 * @param max the maximum number of bytes to read
 * @return length of the returned data
 */
extern ssize_t api_callback_file_reader(void *cls, uint64_t pos, char *buf, size_t max);

/**
 * @brief cleanup the resources used by the file data callback
 * @param cls the context of the callback
 */
extern void api_callback_file_reader_cleanup(void *cls);

/**
 * @brief get a response message by returning the requested audio data
 */
extern void api_audio_response(http_request *request, http_response *response, track* track_info);

/**
 * @brief get a response message by returning a file
 */
extern void api_file_response(http_request *request, http_response *response);

/**
 * @brief get an API default response message (json)
 */
extern void api_default_response(http_request *request, http_response *response);

/**
 * @brief get the API response message based on the path and request method
 */
extern void api_response(http_request *request, http_response *response);

#endif