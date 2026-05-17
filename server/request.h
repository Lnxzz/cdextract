
/**************************************************************************

  cdextract - http request functions

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

#ifndef CDE_REQUEST_H
#define CDE_REQUEST_H

#include <time.h>


#define MAX_POST_SIZE (unsigned long)8192*1024*1024   // 8MB
#define MAX_REQUEST_LIMIT 100                         // maximum number of items to request


/**
 * @brief HTTP request methods to be used within API calls
 */
enum http_request_method {
  GET = 0, 
  POST,
  PUT, 
  DELETE, 
  HEAD,
  OPTIONS,
  TRACE,
  PATCH,
  CONNECT,
  INVALID
};

/**
 * @brief HTTP request structure
 */
typedef struct http_request {
  const char *method;     // method from the request
  int method_id;          // method id from the request
  const char *path;       // path from the request
  char *resource_id;      // identifier of the resource from the path
  char *data;             // data from the request
  size_t size;            // size of the data
  size_t content_length;  // content length reported in the header of the request
  int limit;              // limit for the number of items to return
  int offset;             // offset for the number of items to skip
  int track;              // track number for the audio data request
  int format;             // format of the requested audio data (wav=0, flac=1, pcm=2)
  int return_default;     // indicator to return default response if requested resource is not found
  int stream;             // indicator to stream the response
  int overwrite;          // indicator to enable/disable overwrite of existing data
  int purge;              // indicator to enable/disable purge of existing data
  int fuzzy_lookup;       // indicator for fuzzy lookup (get_disc_info)
  int terminate_thread;   // indicator to terminate the thread processing the request
  clock_t start;          // start time of the request
  clock_t last;           // end time of processing the request
} http_request;


/**
 * @brief get the HTTP request method
 */
int get_request_method(const char *method);

/**
 * @brief parses the path from a http request and places the individual path, identifier and 
 *        query parameters in the given http request structure
 * @param method - request method
 * @param path - request path
 * @param request - http request structure to fil
 * @return 0 = OK; -1 = allocation failed
 */
int init_request(const char *method, const char *path, http_request *request);

/**
 * @brief stores the post data from a http request in the given http request structure
 * @param data - uploaded data
 * @param size - uploaded data size
 * @param request - http request structure to fil
 * @return 0 = OK; -1 = allocation failed; -2 = invalid size
 */
int store_request_data(const char *data, size_t size, http_request *request);

/**
 * @brief free the resources allocated for the http request structure
 * @param request 
 */
void free_request(http_request *request);

#endif