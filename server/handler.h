/**************************************************************************

  cdextract - http request/response handler functions

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

#ifndef CDE_HANDLER_H
#define CDE_HANDLER_H

#include <microhttpd.h>


/**
 * @brief termination code lookup table
 */
typedef struct termination_code_lookup {
  int termination_code;
  const char *termination_code_str;
} termination_code_lookup;


/**
 * @brief initialize the handler and the cdextract library context
 */
void init_handler(char *device_name, char *audio_folder, char *cddb_folder, char *web_folder, char *db_filename, int db_backup);

/**
 * @brief clean the handler and the cdextract library context
 */
void cleanup_handler();

/***
 * @brief intialize the handler with the given option
 */
void handler_set_option(int option, int varg);

/**
 * @brief function that should be called whenever the processing of an incoming request has been completed
 * @param cls 
 * @param connection 
 * @param con_cls 
 */
void request_completed_handler(void *cls, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode toe);

/**
 * @brief HTTP request handler
 */
enum MHD_Result request_handler(void *cls, struct MHD_Connection *conn, const char *url, const char *method, const char *version, const char *upload_data, size_t *upload_data_size, void **con_cls);

#endif