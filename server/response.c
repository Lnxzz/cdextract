

/**************************************************************************

  cdextract - http response functions

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>

#include "string_utils.h"
#include "file_utils.h"
#include "response.h"


/* default html error response messages */
const char *html_response_400 = "<html><body>400 Bad Request</body></html>";
const char *html_response_403 = "<html><body>403 Forbidden</body></html>";
const char *html_response_404 = "<html><body>404 Not Found</body></html>";
const char *html_response_405 = "<html><body>405 Method Not Allowed</body></html>";
const char *html_response_409 = "<html><body>409 Conflict</body></html>";
const char *html_response_423 = "<html><body>423 Locked</body></html>";
const char *html_response_500 = "<html><body>500 Internal Server Error</body></html>";
const char *html_response_501 = "<html><body>501 Not Implemented</body></html>";
const char *html_response_503 = "<html><body>503 Service Unavailable</body></html>";


/**
 * @brief lookup table containing all supported mime types
 */
const struct mime_type_lookup mime_types[] = {
  {MIME_TYPE_JSON, "application/json"},
  {MIME_TYPE_JPEG, "image/jpeg"},
  {MIME_TYPE_PNG, "image/png"},
  {MIME_TYPE_HTML, "text/html"},
  {MIME_TYPE_JAVASCRIPT, "text/javascript"},
  {MIME_TYPE_MARKDOWN, "text/markdown"},
  {MIME_TYPE_WAV, "audio/wav"},
  {MIME_TYPE_FLAC, "audio/flac"},
  {MIME_TYPE_PCM, "audio/pcm"},
  {MIME_TYPE_MULTIPART, "multipart/x-mixed-replace"},
  {MIME_TYPE_BINARY, "application/octet-stream"}
};


/**
 * @brief get the http content type from the given mime type
 * @param mime_type 
 * @return content type
 */
const char *get_content_type(int mime_type) {
  if (mime_type < MIME_TYPE_JSON && mime_type > MIME_TYPE_BINARY) {
    mime_type = MIME_TYPE_BINARY;
  }
  /*
  char *content_type = calloc(strlen(mime_types[mime_type].mime_type_str)+1, sizeof(char));
  strncpy(content_type, mime_types[mime_type].mime_type_str, strlen(mime_types[mime_type].mime_type_str));
  return content_type;
  */
  return mime_types[mime_type].mime_type_str;
}

/**
 * @brief returns an indicator if the specified mime type results in binary content
 * @param mime_type 
 * @return 1 = binary content; 0 = human readable content
 */
int is_binary_content_type(int mime_type) {
  if (mime_type == MIME_TYPE_JSON || mime_type == MIME_TYPE_HTML || mime_type == MIME_TYPE_JAVASCRIPT || mime_type == MIME_TYPE_MARKDOWN ) {
    return 0;
  }
  return 1;
}

/**
 * @brief get the mime type based on the give path
 * @param path 
 * @returns http_mime_type
 */
int get_mime_type_from_path(const char *path) {
  if (ends_with(".json", path)) {
    return MIME_TYPE_JSON;
  }
  if (ends_with(".jpg", path) || ends_with(".jpeg", path)) {
    return MIME_TYPE_JPEG;
  }
  if (ends_with(".png", path)) {
    return MIME_TYPE_PNG;
  }
  if (ends_with(".html", path) || ends_with(".htm", path)) {
    return MIME_TYPE_HTML;
  }
  if (ends_with(".js", path)) {
    return MIME_TYPE_JAVASCRIPT;
  }
  if (ends_with(".md", path)) {
    return MIME_TYPE_MARKDOWN;
  }
  if (ends_with(".wav", path)) {
    return MIME_TYPE_WAV;
  }
  if (ends_with(".flac", path)) {
    return MIME_TYPE_FLAC;
  }
  if (ends_with(".pcm", path)) {
    return MIME_TYPE_PCM;
  }
  return MIME_TYPE_BINARY;
}

/**
 * @brief get a response containing the contents of the given data
 */
void data_response(const char *data, long size, int mime_type, http_response *response) {
  response->code = OK;
  response->mime_type = mime_type;
  response->content_type = get_content_type(response->mime_type);
  response->size = size < 0 ? 0 : size;
  response->body = calloc(response->size+1, sizeof(char));
  memcpy(response->body, data, response->size);
  response->body[response->size] = '\0';
}

/**
 * @brief get a response containing the contents of a file
 */
void file_response(const char *path, char *root_folder, http_response *response) {
  
  // replace characters not allowed in filename (linux and windows)
  char *relative_path = replace_chars(path, PATH_CHAR_FILTER, '-');
  printf("[%s]\n", relative_path);

  // add 'index.html' to the path if no filename is specified
  if (strlen(path) == 0) {
    relative_path = realloc(relative_path, 12 * sizeof(char));
    strcpy(relative_path, "/index.html");
  } else if (ends_with("/", relative_path)) {
    relative_path = realloc(relative_path, (strlen(relative_path) + 11) * sizeof(char));
    strcat(relative_path, "index.html");
  }
 
  // get the mime type based on the path and set the associated content-type
  response->mime_type = get_mime_type_from_path(relative_path);
  response->content_type = get_content_type(response->mime_type);

  // get start of relative path without leading '/''s
  int pos = 0;
  while (pos < strlen(relative_path) && relative_path[pos] == '/') { pos++; }

  // determine absolute path
  int absolute_path_len = strlen(root_folder) + strlen(&relative_path[pos]) + 2;
  char *absolute_path = calloc(absolute_path_len, sizeof(char));
  snprintf(absolute_path, absolute_path_len, "%s/%s", root_folder, &relative_path[pos]);

  // read file
  long size = read_file(&(response->body), absolute_path);
  if (size >= 0) {
    response->size = size;
    response->code = OK;
  } else {
    // unable to read file
    response->size = 0;
    response->code = NOT_FOUND;
  }

  // cleanup
  free(absolute_path);
  free(relative_path);
}

/**
 * @brief get a response containing a default html message
 */
void default_response(int code, const char *message, http_response *response) {
  response->code = code;
  response->mime_type = MIME_TYPE_HTML;
  response->content_type = get_content_type(response->mime_type);
  if (code > 0 && message != NULL) {
    response->size = strlen(message);
    response->body = calloc(strlen(message)+1, sizeof(char));
    strcpy(response->body, message);
    return;
  }
  if (code == NO_CONTENT) {
    response->size = 0;
    response->body = calloc(1, sizeof(char));
    return;
  }
  if (code == BAD_REQUEST) {
    response->size = strlen(html_response_400);
    response->body = calloc(response->size+1, sizeof(char));
    strcpy(response->body, html_response_400);
    return;
  }
  if (code == FORBIDDEN) {
    response->size = strlen(html_response_403);
    response->body = calloc(response->size+1, sizeof(char));
    strcpy(response->body, html_response_403);
    return;
  }
  if (code == NOT_FOUND) {
    response->size = strlen(html_response_404);
    response->body = calloc(response->size+1, sizeof(char));
    strcpy(response->body, html_response_404);
    return;
  }
  if (code == METHOD_NOT_ALLOWED) {
    response->size = strlen(html_response_405);
    response->body = calloc(response->size+1, sizeof(char));
    strcpy(response->body, html_response_405);
    return;
  }
  if (code == CONFLICT) {
    response->size = strlen(html_response_409);
    response->body = calloc(response->size+1, sizeof(char));
    strcpy(response->body, html_response_409);
    return;
  }
  if (code == RESOURCE_LOCKED) {
    response->size = strlen(html_response_423);
    response->body = calloc(response->size+1, sizeof(char));
    strcpy(response->body, html_response_423);
    return;
  }
  if (code == NOT_IMPLEMENTED) {
    response->size = strlen(html_response_501);
    response->body = calloc(response->size+1, sizeof(char));
    strcpy(response->body, html_response_501);
    return;
  }
  if (code == SERVICE_UNAVAILABLE) {
    response->size = strlen(html_response_503);
    response->body = calloc(response->size+1, sizeof(char));
    strcpy(response->body, html_response_503);
    return;
  }
  // unspecified behaviour: respond with Internal Server Error
  response->code = INTERNAL_SERVER_ERROR;
  response->size = strlen(html_response_500);
  response->body = calloc(response->size+1, sizeof(char));
  strcpy(response->body, html_response_500);
}

/**
 * @brief initialize the http response structure
 * @param response 
 */
void init_response(http_response *response) {
  response->content_type = NULL;
  response->size = 0;
  response->body = NULL;
  response->callback = NULL;
  response->callback_context = NULL;
  response->callback_cleanup = NULL;
  response->callback_block_size = 0;
}

/**
 * @brief free the resources allocated for the http response structure
 * @param response 
 */
void free_response(http_response *response) {
  if (response != NULL && response->body != NULL) {
    free(response->body);
    response->body = NULL;
  }
}