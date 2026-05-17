/**************************************************************************

  cdextract - http client using libcurl

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
#include <curl/curl.h>

#include "report.h"
#include "http_client.h"


#define SKIP_PEER_VERIFICATION 1
#define SKIP_HOSTNAME_VERIFICATION 1


/*
 * block of memory for storing return data
 */
typedef struct http_memory_block {
  char *memory;
  size_t size;
} http_memory_block;

/**
 * @brief write callback for storing returned data in memory
 */
static size_t http_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  http_memory_block *mem = (http_memory_block *)userp;
 
  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if(!ptr) {
    // out of memory!
    cde_report(CDE_MSG_TYPE_ERROR, "not enough memory (realloc returned NULL)");
    return 0;
  }
 
  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}

/**
 * @brief custom utf-8 to host conversion function which keeps the data in utf-8 format
 */
[[maybe_unused]] static CURLcode no_utf8_to_host_conversion(char *buffer, size_t length) {
  return CURLE_OK;
}

/**
 * @brief get data from the endpoint specified by url
 * @return size of the response on success, -http code if not 200 OK or -1 on another error
 */
ssize_t http_get_data(char *url, char **response, int verbose) {
  CURL *curl;
  CURLcode res;
  long http_code = 0;
  ssize_t size = -1;

  http_memory_block chunk;
  chunk.memory = malloc(sizeof(char)); // will be grown as needed by the realloc above
  chunk.size = 0; // no data at this point
 
  curl_global_init(CURL_GLOBAL_DEFAULT);
 
  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // send all data to this function
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
  
    // we pass our 'chunk' struct to the callback function
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
  
    // set user agent
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);

    // set follow any "Location: " header that the server sends as part of the HTTP header
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
 
#ifdef SKIP_PEER_VERIFICATION
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif
 
#ifdef SKIP_HOSTNAME_VERIFICATION
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
#endif
 
    // perform the request, res will get the return code
    if (verbose) {
      cde_report(CDE_MSG_TYPE_INFO, "calling: %s", url);
    }
    res = curl_easy_perform(curl);

    // check for errors
    if (res != CURLE_OK) {
      cde_report(CDE_MSG_TYPE_ERROR, "http request failed: %s", curl_easy_strerror(res));
    } else {
      // the memory block now contains the response from the webserver
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
      if (verbose) {
        cde_report(CDE_MSG_TYPE_INFO, "http response %d: %lu bytes retrieved", http_code, (unsigned long)chunk.size);
        if (http_code != 200) {
          cde_report(CDE_MSG_TYPE_DEBUG, "received data:\n%s", chunk.memory);
        }
      }
      
      if (http_code == 200) {
        // if OK, copy the data to the response
        char *tmp = realloc(*response, (chunk.size + 1) * sizeof(char));
        if (tmp != NULL) {
          *response = tmp;
          memcpy(*response, chunk.memory, chunk.size);
          (*response)[chunk.size] = '\0';
          size = chunk.size;
        }
      } else {
        // otherwise we response a negative value with the http code
        size = -1 * http_code;
      }
    }

    // always cleanup curl
    curl_easy_cleanup(curl);

    // free memory block
    free(chunk.memory);
  }
 
  // cleanup libcurl
  curl_global_cleanup();

  return size;
}