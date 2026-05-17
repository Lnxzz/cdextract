/**************************************************************************

  libcdextract - musicbrainz api / coverart archive client

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

#ifndef MB_API_H
#define MB_API_H

#include "libcdextract_types.h"


#define MB_COVERART_OFF 0          // do not download coverart
#define MB_COVERART_MEM_ONLY 1     // only get front and back cover and keep the result in memory
#define MB_COVERART_COVER_ONLY 2   // only download the front and back cover
#define MB_COVERART_FULL 3         // full download of covers and metadata

#define MB_QUERY_DISCID 0          // query by disc ID / TOC
#define MB_QUERY_FUZZY 1           // query by fuzzy lookup of disc information
#define MB_QUERY_RELEASE_FULL 2    // query by artist, release title, track count, release year and CD format
#define MB_QUERY_RELEASE_PARTIAL 3 // query by artist, release title, track count and CD format
#define MB_QUERY_RELEASE_LIMITED 4 // query by artist, release title and CD format


/**
 * @brief download the json formatted release information from the coverartarchive (CAA) 
 *        using the specified musicbrainz release id
 *        PRE: output folder must exist when writing to file
 */
extern int mb_caa_get_release_info(disc *disc_info, int download_coverart, const char *folder, int verbose);

/**
 * @brief download the front cover from the coverartarchive (CAA) 
 *        using the specified musicbrainz release id
 *        PRE: output folder must exist when writing to file
 */
extern int mb_caa_get_front_cover(disc *disc_info, int download_coverart, const char *folder, int verbose);

/**
 * @brief download the back cover from the coverartarchive (CAA) 
 *        using the specified musicbrainz release id
 *        PRE: output folder must exist when writing to file
 */
extern int mb_caa_get_back_cover(disc *disc_info, int download_coverart, const char *folder, int verbose);
 
/**
 * @brief query and read from the online musicbrainz api
 *        returned information will be parsed and stored in disc_info
 */
extern int mb_get_disc_info(disc *disc_info, int query_method, int verbose);

#endif