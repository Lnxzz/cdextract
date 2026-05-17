/**************************************************************************

  libcdextract - http client using libcurl

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

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H


#define USER_AGENT      "Mozilla/5.0 (X11; Linux x86_64; rv:150.0) Gecko/20100101 Firefox/150.0"
#define USER_AGENT_CURL "libcurl-agent/1.0"


/**
 * @brief get data from the endpoint specified by url
 * @return size of the response on success, -http code if not 200 OK or -1 on another error
 */
ssize_t http_get_data(char *url, char **response, int verbose);

#endif