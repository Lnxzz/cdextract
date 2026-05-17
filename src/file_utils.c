/**************************************************************************

  libcdextract - file utilities

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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <sys/stat.h>

#include "string_utils.h"
#include "file_utils.h"


/**
 * @brief construct the full path using the given root folder and relative path
 * @param root_folder the root folder to use
 * @param relative_path the relative path to append to the root folder
 * @return a newly allocated string containing the full path, or NULL on error
 */
char *get_full_path(const char *root_folder, const char *path) {
  // replace characters not allowed in filename (linux and windows)
  char *relative_path = replace_chars(path, PATH_CHAR_FILTER, '-');
  if (relative_path == NULL) {
    // unable to allocate memory for relative path
    return NULL;
  }

  // get start of relative path without leading '/''s
  int pos = 0;
  while (pos < strlen(relative_path) && relative_path[pos] == '/') { pos++; }

  // determine absolute path
  int absolute_path_len = strlen(root_folder) + strlen(&relative_path[pos]) + 2;
  char *absolute_path = calloc(absolute_path_len, sizeof(char));
  if (absolute_path == NULL) {
    // unable to allocate memory for absolute path
    free(relative_path);
    return NULL;
  }
  // create absolute path by concatenating the root folder and the relative path
  snprintf(absolute_path, absolute_path_len, "%s/%s", root_folder, &relative_path[pos]);

  free(relative_path);
  return absolute_path;
}

/**
 * @brief create path with given mode
 * @param path the path to create
 * @param mode the mode to use for the path creation (e.g. 0755)
 * @return 0 on success, -1 on error
 */
int create_path(const char *path, const mode_t mode) {
  const size_t len = strlen(path);
  char _path[PATH_MAX];
  char *p; 

  errno = 0;

  // copy string so its mutable
  if (len > sizeof(_path)-1) {
    errno = ENAMETOOLONG;
    return -1; 
  }
  strcpy(_path, path);

  // iterate through the path elements
  for (p = _path + 1; *p; p++) {
    if (*p == '/') {
      // temporarily truncate
      *p = '\0';
      if (mkdir(_path, mode) != 0) {
        if (errno != EEXIST)
          return -1; 
      }
      *p = '/';
    }
  }   

  // last path element
  if (mkdir(_path, mode) != 0) {
    if (errno != EEXIST)
      return -1; 
  }   

  return 0;
}

/**
 * @brief checks if a file exists
 * @param filename the name and location of the file
 * @return 1 if file exists, 0 if not, -1 on error
 */
int file_exists(const char *filename) {
  // check input parameters
  if (filename == NULL || strlen(filename) == 0) {
    return -1;
  }

  struct stat buffer;
  int exists = stat(filename, &buffer);
  if (exists == 0) {
    return 1; // file exists
  } else if (errno == ENOENT) {
    return 0; // file does not exist
  } else {
    return -1; // error occurred
  }
}


/**
 * @brief gets the size of a file
 * @param filename the name and location of the file
 * @return the size of the file or -1 on error
 */
long file_size(const char *filename) {
  // check input parameters
  if (filename == NULL || strlen(filename) == 0) {
    return -1;
  }

  struct stat buffer;
  if (stat(filename, &buffer) == 0) {
    return buffer.st_size; // return file size
  } else {
    return -1; // error occurred
  }
}

/**
 * @brief reads a file into memory returning the size of the data
 * @param data pointer to a char pointer that will be allocated and filled with the file data
 * @param filename the name and location of the file to read
 * @return -1 if file could not be read into memory successfully, otherwise the size of the data read
 */
long read_file(char **data, const char *filename) {
  // check input parameters
  if (filename == NULL || strlen(filename) == 0) {
    return -1;
  }

  long bufsize = -1;
  char *buffer = NULL;
  FILE *fp = fopen(filename, "r");
  
  if (fp != NULL) {
    // go to the end of the file
    if (fseek(fp, 0L, SEEK_END) == 0) {
      // get the size of the file
      bufsize = ftell(fp);
      if (bufsize >= 0) {

        // allocate our buffer to that size
        buffer = malloc(sizeof(char) * (bufsize + 1));

        // go back to the start of the file
        if (fseek(fp, 0L, SEEK_SET) == 0) {

          // read the entire file into memory
          size_t size = fread(buffer, sizeof(char), bufsize, fp);
          if (ferror(fp) != 0) {
            //fputs("Error reading file", stderr);
            bufsize = -1;
            free(buffer);
          } else {
            // terminate 'string' data
            buffer[size++] = '\0';
            // update data pointer
            *data = buffer;
          }

        } else {
          bufsize = -1;
          free(buffer);
        }
      }
    }
    fclose(fp);
  }

  return bufsize;
}

/**
 * @brief writes a file from memory to the specified filename
 * @param data the data to write to the file
 * @param size the size of the data to write
 * @param filename the name and location of the file to write
 * @return -1 if file could not be written successfully
 */
int write_file(char *data, int size, const char *filename) {
  FILE *fptr;
  fptr = fopen(filename, "wb");

  if (fptr != NULL) {
    if (data != NULL && size > 0) {
      fwrite(data, size, 1, fptr);
    }
    fclose(fptr);
  } else {
    return -1;
  }
  
  return 0;
}