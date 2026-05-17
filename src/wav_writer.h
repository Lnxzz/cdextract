/**************************************************************************

  libcdextract - wav file writer

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

#ifndef WAV_WRITER_H
#define WAV_WRITER_H

#include <stdio.h>
#include <unistd.h>


/**
 * @brief write wav file header
 * @param fp file pointer to the wav file
 * @param bytes total number of bytes in the wav file
 */
void start_wav(FILE *fp, long bytes);

/**
 * @brief write wav file using a buffer
 * @param f file pointer to the wav file
 * @param buffer pointer to the data buffer
 * @param num_bytes number of bytes to write from the buffer
 * @return 0 on success, non-zero on failure
 */
long write_wav(FILE *fp, char *buffer, long num_bytes);

/**
 * @brief end wav file - writes out remaining buffered data and closes the file
 * @param f file pointer to the wav file
 * @return 0 on success, non-zero on failure
 */
int end_wav(FILE *fp);

#endif