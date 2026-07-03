
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <microhttpd.h>

#include "libcdextract.h"
#include "libcdextract_types.h"
#include "report.h"
#include "timer.h"
#include "db.h"
#include "request.h"
#include "response.h"
#include "api.h"
#include "handler.h"


#define REQUEST_TERMINATED_UNKNOWN 6 // add definition for unknown termination code


/**
 * @brief lookup table containing all termination codes
 */
const struct termination_code_lookup termination_codes[] = {
  {MHD_REQUEST_TERMINATED_COMPLETED_OK, "completed ok"},
  {MHD_REQUEST_TERMINATED_WITH_ERROR, "terminated with error"},
  {MHD_REQUEST_TERMINATED_TIMEOUT_REACHED, "timeout reached"},
  {MHD_REQUEST_TERMINATED_DAEMON_SHUTDOWN, "daemon shutdown"},
  {MHD_REQUEST_TERMINATED_READ_ERROR, "read error"},
  {MHD_REQUEST_TERMINATED_CLIENT_ABORT, "aborted by client"},
  {REQUEST_TERMINATED_UNKNOWN, "unknown"}
};


/**
 * @brief get the http content type from the given mime type
 * @param mime_type 
 * @return content type
 */
const char *get_termination_reason(int termination_code) {
  if (termination_code < MHD_REQUEST_TERMINATED_COMPLETED_OK && termination_code > MHD_REQUEST_TERMINATED_CLIENT_ABORT) {
    termination_code = REQUEST_TERMINATED_UNKNOWN;
  }
  return termination_codes[termination_code].termination_code_str;
}

/**
 * @brief initialize the handler and the cdextract library context
 */
void init_handler(char *device_name, char *audio_folder, char *cddb_folder, char *web_folder, char *db_filename, int db_backup) {
  api_init(device_name, audio_folder, cddb_folder, web_folder, db_filename, db_backup);
}

/**
 * @brief clean the handler and the cdextract library context
 */
void cleanup_handler() {
  api_cleanup();
}

/***
 * @brief intialize the handler with the given option
 */
void handler_set_option(int option, int varg) {
  api_set_option(option, varg);
}

/**
 * @brief function that should be called whenever the processing of an incoming request has been completed
 * @param cls 
 * @param connection 
 * @param con_cls 
 */
void request_completed_handler(void *cls, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode toe) {
  http_request *con_request = *con_cls;

  if (con_request == NULL) {
    return;
  }

  free_request(con_request);
  free(con_request);
  *con_cls = NULL;

  cde_report(CDE_MSG_TYPE_DEBUG, "request handler: request completed with status: %s", get_termination_reason(toe));
}

/**
 * @brief HTTP request handler
 */
enum MHD_Result request_handler(void *cls, struct MHD_Connection *conn, const char *url, const char *method, const char *version, const char *upload_data, size_t *upload_data_size, void **con_cls) {
  
  // first iteration: initialize the http request structure ncluding method and url
  if (*con_cls == NULL) {
    http_request *con_request;

    con_request = calloc(1, sizeof(http_request));
    if (con_request == NULL) {
      return MHD_NO;
    }

    init_request(method, url, con_request);

    // get optional query and header parameters
    const char *limit = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "limit");
    if (limit != NULL) {
      con_request->limit = atoi(limit);
      if (con_request->limit < 1) {
        con_request->limit = 1;
      } else if (con_request->limit > MAX_REQUEST_LIMIT) {
        con_request->limit = MAX_REQUEST_LIMIT;
      }
    }
    const char *offset = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "offset");
    if (offset != NULL) {
      con_request->offset = atoi(offset);
      if (con_request->limit < 0) {
        con_request->limit = 0;
      }
    }
    const char *track = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "track");
    if (track != NULL) {
      con_request->track = atoi(track);
      if (con_request->track < 0) {
        con_request->track = 0;
      } else if (con_request->track > CDE_MAX_TRACKS) {
        con_request->track = CDE_MAX_TRACKS;
      }
    }
    const char *search = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "search");
    if (search != NULL && strlen(search) > 0) {
      if (con_request->search != NULL) {
        free(con_request->search);
      }
      con_request->search = calloc(strlen(search) + 1, sizeof(char));
      if (con_request->search == NULL) {
        return MHD_NO; // allocation failed
      }
      strncpy(con_request->search, search, strlen(search));
    }
    const char *tag = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "tag");
    if (tag != NULL) {
      // check if tag is one of the valid values (disc, track, artist, genre, year)
      if (strcmp(tag, "disc") == 0) {
        con_request->tag = 0;
      } else if (strcmp(tag, "track") == 0) {
        con_request->tag = 1;
      } else if (strcmp(tag, "artist") == 0) {
        con_request->tag = 2;
      } else if (strcmp(tag, "genre") == 0) {
        con_request->tag = 3;
      } else if (strcmp(tag, "year") == 0) {
        con_request->tag = 4;
      }
      // alternatively, check if tag is specified as a number (0..4)
      else {
        con_request->tag = atoi(tag);
        if (con_request->tag < 0) {
          con_request->tag = -1;
        } else if (con_request->tag > 4) {
          con_request->tag = 4;
        }
      }
    }
    const char *format = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "format");
    if (format != NULL) {
      // check if format is flac, wav or pcm
      if (strcmp(format, "wav") == 0) {
        con_request->format = 0; // wav
      } else if (strcmp(format, "flac") == 0) {
        con_request->format = 1; // flac
      } else if (strcmp(format, "pcm") == 0) {
        con_request->format = 2; // pcm
      }
      // check if format is disc information only or include track information as well
      else if (strcmp(format, "disc") == 0) {
        con_request->format = 0; // disc information only
      } else if (strcmp(format, "track") == 0) {
        con_request->format = 1; // disc and track information
      }
      // alternatively, check if format is specified as a number (0, 1 or 2)
      else {
        con_request->format = atoi(format);
        if (con_request->format < 0) {
          con_request->format = 0;
        } else if (con_request->format > 2) {
          con_request->format = 2;
        }
      }
    }

    const char *return_default = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "default");
    if (return_default != NULL) {
      // check if default is set to 'true'
      if (strcmp(return_default, "true") == 0) {
        con_request->return_default = 1;
      } else if (strcmp(return_default, "1") == 0) {
        con_request->return_default = 1;
      }
    }
    const char *stream = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "stream");
    if (stream != NULL) {
      con_request->stream = atoi(stream);
    }
    const char *overwrite = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "overwrite");
    if (overwrite != NULL) {
      con_request->overwrite = atoi(overwrite);
    }
    const char *purge = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "purge");
    if (purge != NULL) {
      con_request->purge = atoi(purge);
    }
    const char *fuzzy_lookup = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "fuzzy");
    if (fuzzy_lookup != NULL) {
      con_request->fuzzy_lookup = atoi(fuzzy_lookup);
    }
    const char *content_length = MHD_lookup_connection_value(conn, MHD_HEADER_KIND, "Content-Length");
    if (content_length != NULL) {  
      char *endptr;
      int base = 10;
      con_request->content_length = strtoul(content_length, &endptr, base);
      if (con_request->content_length >= MAX_POST_SIZE) {
        free_request(con_request);
        free(con_request);
        return MHD_NO;
      }
    }

    *con_cls = (void *)con_request;
    return MHD_YES;
  }

  // second iteration: store the post body
  http_request *con_request = *con_cls;
  if (con_request->method_id == POST && *upload_data_size != 0) {

    if (store_request_data(upload_data, *upload_data_size, con_request) != 0) {
      return MHD_NO;
    }

    *upload_data_size = 0;
    return MHD_YES;
  }

  // third iteration: process API call and build response
  http_response api_http_response;
  init_response(&api_http_response);
  api_response(con_request, &api_http_response);
  struct MHD_Response *response = NULL;

  // normal case: no callback function set
  if (api_http_response.callback == NULL) {

    // create the response from the http response body
    response = MHD_create_response_from_buffer(api_http_response.size, (void *)api_http_response.body, MHD_RESPMEM_MUST_COPY);
    MHD_set_response_options(response, MHD_RF_SEND_KEEP_ALIVE_HEADER);
    MHD_add_response_header(response, MHD_HTTP_HEADER_CACHE_CONTROL, "no-cache, no-store");
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, api_http_response.content_type);

    // queue the response
    enum MHD_Result ret = MHD_queue_response(conn, api_http_response.code, response);
    MHD_destroy_response(response);

    // get end timestamp
    con_request->last = clock();
  
    // log request/response
    char *unit;
    double period = elapsed_format(con_request->start, con_request->last, &unit);
    cde_report(CDE_MSG_TYPE_DEBUG, "http request: method:%s; path:%s; period:%.2f%s", method, url, period, unit);
    if (is_binary_content_type(api_http_response.mime_type)) {
      cde_report(CDE_MSG_TYPE_DEBUG, "http response: code:%d; type:%s; binary: %d bytes", api_http_response.code, api_http_response.content_type, api_http_response.size);
    } else {
      cde_report(CDE_MSG_TYPE_DEBUG, "http response: code:%d; type:%s; %s", api_http_response.code, api_http_response.content_type, api_http_response.body);
    }

    // free allocated resources in http response structure
    free_response(&api_http_response);
    
    return ret;
  }

  // set callback function for continuous (status) updates or large file or audio data transfers
  response = MHD_create_response_from_callback(api_http_response.size, api_http_response.callback_block_size, api_http_response.callback, api_http_response.callback_context, api_http_response.callback_cleanup);
  MHD_set_response_options(response, MHD_RF_SEND_KEEP_ALIVE_HEADER);
  MHD_add_response_header(response, MHD_HTTP_HEADER_CACHE_CONTROL, "no-cache, no-store");
  MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, api_http_response.content_type);

  // queue the response
  enum MHD_Result ret = MHD_queue_response(conn, api_http_response.code, response);
  MHD_destroy_response(response);

  // free allocated resources in http response structur
  free_response(&api_http_response);

  return ret;
}
