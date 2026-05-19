/**************************************************************************

  libcdextract - wav file writer

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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>


#define OUTBUFSZ 32*1024


// globals for buffering calls
static FILE  *b_f  = NULL;
static long b_pos = 0;
static char b_outbuf[OUTBUFSZ];


/**
 * @brief write number 'num' to the file
 * @param fp file pointer to the output file
 * @param num number to write
 * @param endianness 0 for little-endian, 1 for big-endian
 * @param bytes number of bytes to write
 */
void write_number(FILE *fp, long num, int endianness, int bytes) {
  int i;
  unsigned char c;

  if(!endianness) {
    i=0;
  } else {
    i=bytes-1;
  }
  while (bytes--) {
    c = (num >> (i<<3)) & 0xff;
    if(fputc(c, fp) == -1){
      perror("Could not write to output.");
      exit(1);
    }
    if(endianness) {
      i--;
	} else {
      i++;
	}
  }
}

/**
 * @brief write data in buffer to file
 * @param fp file pointer to the output file
 * @param buffer pointer to the data buffer
 * @param num_bytes number of bytes to write from the buffer
 * @return 0 on success, -1 on failure (errno is set)
 */
long write_data(FILE *fp, char *buffer, long num_bytes) {
  size_t r = 0;
  r = fwrite(buffer, num_bytes, 1, fp);
  if (r < 0) {
    if (errno != EINTR && errno != EAGAIN) {
      return(-1);
    }
  }
  return(0);
}

/**
 * @brief write wav file header
 * @param fp file pointer to the wav file
 * @param bytes total number of bytes in the wav file
 */
void start_wav(FILE *fp, long bytes) {
  fprintf(fp, "RIFF");                /* 00-03 */
  write_number(fp, bytes+44-8, 0, 4); /* 04-07 */
  fprintf(fp, "WAVEfmt ");            /* 08-15 */
  write_number(fp, 16, 0, 4);         /* 16-19 */
  write_number(fp, 1, 0, 2);          /* 20-21 */
  write_number(fp, 2, 0, 2);          /* 22-23 */
  write_number(fp, 44100, 0, 4);      /* 24-27 */
  write_number(fp, 44100*2*2, 0, 4);  /* 28-31 */
  write_number(fp, 4, 0, 2);          /* 32-33 */
  write_number(fp, 16, 0, 2);         /* 34-35 */
  fprintf(fp, "data");                /* 36-39 */
  write_number(fp, bytes, 0, 4);      /* 40-43 */
}

/**
 * @brief write wav file using a buffer
 * @param fp file pointer to the wav file
 * @param buffer pointer to the data buffer
 * @param num_bytes number of bytes to write from the buffer
 * @return 0 on success, non-zero on failure
 */
long write_wav(FILE *fp, char *buffer, long num_bytes) {
	if (fp != b_f) {
		// flush buffer after buffering for some other file
		if (b_f >= 0 && b_pos > 0) {
			if (write_data(b_f, b_outbuf, b_pos)) {
				perror("write (in buffering_write, flushing)");
			}
		}
		b_f  = fp;
		b_pos = 0;
	}

	if (b_pos + num_bytes > OUTBUFSZ) {
		// fill our buffer first, then write, then modify buffer and num_bytes
		memcpy(&b_outbuf[b_pos], buffer, OUTBUFSZ - b_pos);
		if (write_data(fp, b_outbuf, OUTBUFSZ)) {
			perror("write (in buffering_write, full buffer)");
			return(-1);
		}
		num_bytes -= (OUTBUFSZ - b_pos);
		buffer += (OUTBUFSZ - b_pos);
		b_pos = 0;
	}
	// save data
	if(buffer && num_bytes) {
	  memcpy(&b_outbuf[b_pos], buffer, num_bytes);
	}
	b_pos += num_bytes;

	return(0);
}

/**
 * @brief end wav file - writes out remaining buffered data and closes the file
 * @param f file pointer to the wav file
 * @return 0 on success, non-zero on failure
 */
int end_wav(FILE *fp) {
	if (fp == b_f && b_pos > 0) {
		// write out remaining data in buffer
		if (write_data(fp, b_outbuf, b_pos)) {
			perror("write (in buffering_close)");
		}
		b_f  = NULL;
		b_pos = 0;
	}
	return(fclose(fp));
}
