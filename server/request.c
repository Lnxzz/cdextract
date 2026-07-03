
/**************************************************************************

  cdextract - http request functions

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

#include <stdlib.h>
#include <string.h>

#include "request.h"


/**
 * @brief HTTP request method strings to be used within API calls
 */
const char *http_request_methods[] = {
  "GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS", "TRACE", "PATCH", "CONNECT", "INVALID"
};

/**
 * @brief get the HTTP request method
 */
int get_request_method(const char *method) {
  for (int i=GET; i<INVALID; i++) {
    if (strcmp(http_request_methods[i], method)==0) {
      return i;
    }
  }
  return INVALID;
}

/**
 * @brief parses the path from a http request and places the individual path, identifier and 
 *        query parameters in the given http request structure
 * @param method - request method
 * @param path - request path
 * @param request - http request structure to fil
 * @return 0 = OK; -1 = allocation failed
 */
int init_request(const char *method, const char *path, http_request *request) {

  request->start = clock();
  request->last = 0;
  request->method = method;
  request->method_id = get_request_method(method);
  request->path = path;
  request->resource_id = NULL;
  request->data = NULL;
  request->size = 0;
  request->content_length = 0;
  request->limit = MAX_REQUEST_LIMIT; // default limit for the number of items to return
  request->offset = 0;                // default offset for the number of items to skip
  request->track = 1;                 // default track number for the audio data request
  request->search = NULL;             // default search string for the request
  request->tag = -1;                  // no default tag to filter the discs by (0=disc, 1=track, 2=artist, 3=genre, 4=year)
  request->format = -1;               // no default format for audio data (wav=0, flac=1, pcm=2) or disc/track list information (disc=0, include tracks=1)
  request->return_default = 0;        // default to not return default response if requested resource is not found
  request->stream = 0;                // default to not stream the response
  request->overwrite = 0;
  request->purge = 0;
  request->fuzzy_lookup = 0;
  request->terminate_thread = 0;

  // check if path contains a resource identifier (format: /<version>/<resource>[/<resource_id>][/<command>][?<params...>])
  int path_len = strlen(path);
  int resource_begin = 0;
  int slash_cnt = 0;
  for (int i=0; i<path_len - 1; i++) {
    if (path[i] == '/') {
      slash_cnt++;
      if (slash_cnt == 3) {
        resource_begin = i + 1;
        break;
      }
    }
  }
  if (slash_cnt < 3) {
    return 0; // no resource identifier
  }
  int resource_end = resource_begin;
  while (resource_end < path_len && path[resource_end] != '?' && path[resource_end] != '/') {
    resource_end++;
  }
  if (resource_end - resource_begin == 0) {
    return 0; // no resource identifier
  }
  request->resource_id = calloc(resource_end - resource_begin + 1, sizeof(char));
  if (request->resource_id == NULL) {
    return -1; // allocation failed
  }
  strncpy(request->resource_id, &(path[resource_begin]), resource_end - resource_begin);
  return 0;
}

/**
 * @brief stores the post data from a http request in the given http request structure
 * @param data - uploaded data
 * @param size - uploaded data size
 * @param request - http request structure to fil
 * @return 0 = OK; -1 = allocation failed; -2 = invalid size
 */
int store_request_data(const char *data, size_t size, http_request *request) {
  if (size > 0) {
    if (request->data == NULL) {
      request->data = malloc((request->content_length + 1) * sizeof(char));
      if (request->data == NULL) {
        return -1; // allocation failed
      }
      request->size = 0;
    } else if (request->size + size > request->content_length) {
      char *tmp = realloc(request->data, (request->size +size + 1) * sizeof(char));
      if (tmp == NULL) {
        return -1; // allocation failed
      }
      request->data = tmp;
    }
    if (request->size + size < MAX_POST_SIZE) {
      memcpy(&(request->data[request->size]), data, size);
      request->data[request->size + size] = '\0';
      request->size = request->size + size;
    } else {
      return -2; // invalid size
    }
  }
  return 0;
}

/**
 * @brief free the resources allocated for the http request structure
 * @param request 
 */
void free_request(http_request *request) {
  if (request != NULL) {
    if (request->search != NULL) {
      free(request->search);
      request->search = NULL;
    }
    if (request->resource_id != NULL) {
      free(request->resource_id);
      request->resource_id = NULL;
    }
    if (request->data != NULL) {
      free(request->data);
      request->data = NULL;
    }
  }
}
