
/**************************************************************************

  cdextract - http response functions

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

#ifndef CDE_RESPONSE_H
#define CDE_RESPONSE_H

#include <stdint.h>


/**
 * @brief HTTP response codes reported by the API
 */
typedef enum {
  OK = 200, 
  NO_CONTENT = 204,
  BAD_REQUEST = 400,
  FORBIDDEN = 403,
  NOT_FOUND = 404,
  METHOD_NOT_ALLOWED = 405,
  CONFLICT = 409,
  RESOURCE_LOCKED = 423,
  INTERNAL_SERVER_ERROR = 500,
  NOT_IMPLEMENTED = 501,
  SERVICE_UNAVAILABLE = 503
} http_response_code;

/**
 * @brief HTTP mime types reported by the API
 */
typedef enum {
  MIME_TYPE_JSON,
  MIME_TYPE_JPEG,
  MIME_TYPE_PNG,
  MIME_TYPE_HTML,
  MIME_TYPE_JAVASCRIPT,
  MIME_TYPE_MARKDOWN,
  MIME_TYPE_WAV,
  MIME_TYPE_FLAC,
  MIME_TYPE_PCM,
  MIME_TYPE_MULTIPART,
  MIME_TYPE_BINARY,
} http_mime_type;

/**
 * @brief mime type lookup table
 */
typedef struct mime_type_lookup {
  http_mime_type mime_type;
  const char *mime_type_str;
} mime_type_lookup;

/**
 * @brief HTTP response structure
 */
typedef struct {
  http_response_code code;                                              // HTTP response code
  http_mime_type mime_type;                                             // mime type of the response
  const char *content_type;                                             // content type of the response
  unsigned long long size;                                              // size of the response body, use SIZE_UNKNOWN_RESPONSE / MHD_SIZE_UNKNOWN for unknown response size
  char *body;                                                           // body of the response
  ssize_t (*callback)(void *cls, uint64_t pos, char *buf, size_t max);  // callback function to read data from the response
  void *callback_context;                                               // context for the callback function
  void (*callback_cleanup)(void *cls);                                  // cleanup function for the callback context
  size_t callback_block_size;                                           // preferred size of each block provided by the callback
} http_response;


/**
 * @brief get the http content type from the given mime type
 * @param mime_type 
 * @return content type
 */
extern const char *get_content_type(int mime_type);

/**
 * @brief returns an indicator if the specified mime type results in binary content
 * @param mime_type 
 * @return 1 = binary content; 0 = human readable content
 */
extern int is_binary_content_type(int mime_type);

/**
 * @brief get a response containing the contents of the given data
 */
extern void data_response(const char *data, long size, int mime_type, http_response *response);

/**
 * @brief get a response containing the contents of a file
 */
extern void file_response(const char *path, char *root_folder, http_response *response);

/**
 * @brief get a response containing a default html message
 */
extern void default_response(int code, const char *message, http_response *response);

/**
 * @brief initialize the http response structure
 * @param response 
 */
extern void init_response(http_response *response);

/**
 * @brief free the resources allocated for the http response structure
 * @param response 
 */
extern void free_response(http_response *response);

#endif