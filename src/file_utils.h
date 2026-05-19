/**************************************************************************

  libcdextract - file utilities

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

#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <sys/stat.h>


/* array of characters not allowed in filename (combined windows and linux) */
#define FILENAME_CHAR_FILTER "/\\|\"<>:?*#%@{}=!$"

/* array of characters not allowed in path (combined windows and linux) */
#define PATH_CHAR_FILTER "|\"<>:?*#%@{}=!$"

/**
 * @brief construct the full path using the given root folder and path
 * @param root_folder the root folder to use
 * @param relative_path the relative path to append to the root folder
 * @return a newly allocated string containing the full path, or NULL on error
 */
extern char *get_full_path(const char *root_folder, const char *path);

/**
 * @brief create path with given mode
 * @param path the path to create
 * @param mode the mode to use for the path creation (e.g. 0755)
 * @return 0 on success, -1 on error
 */
extern int create_path(const char *path, const mode_t mode);

/**
 * @brief checks if a file exists
 * @param filename the name and location of the file
 * @return 1 if file exists, 0 if not, -1 on error
 */
extern int file_exists(const char *filename);

/**
 * @brief gets the size of a file
 * @param filename the name and location of the file
 * @return the size of the file or -1 on error
 */
extern long file_size(const char *filename);

/**
 * @brief reads a file into memory returning the size of the data
 * @param data pointer to a char pointer that will be allocated and filled with the file data
 * @param filename the name and location of the file to read
 * @return -1 if file could not be read into memory successfully, otherwise the size of the data read
 */
extern long read_file(char **data, const char *filename);

/**
 * @brief writes a file from memory to the specified filename
 * @param data the data to write to the file
 * @param size the size of the data to write
 * @param filename the name and location of the file to write
 * @return -1 if file could not be written successfully
 */
extern int write_file(char *data, int size, const char *filename);

#endif