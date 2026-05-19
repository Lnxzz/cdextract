/**************************************************************************

  libcdextract - json file reader / writer

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

#ifndef JSON_FILE_H
#define JSON_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "libcdextract_types.h"


/**
 * @brief writes the gathered disc information to a json file
 * @param disc_info the disc information structure
 * @param folder folder to store the file
 * @param overwrite overwrite the file if it exists
 * @param verbose print detailed output
 */
int json_write_disc_info(disc *disc_info, const char *folder, int overwrite, int verbose);

/**
 * @brief reads the disc information from the specified json file
 * @param disc_info the disc information structure
 * @param file_path path to the json file
 * @param verbose print detailed output
 * @return 0 on success, negative value on error
 */
int json_read_disc_info(disc *disc_info, const char *file_path, int verbose);


#ifdef __cplusplus
}
#endif

#endif /* JSON_FILE_H */