/**************************************************************************

  libcdextract - wav reader

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wav_reader.h"


/**
 * @brief open a wav file to be read
 * @param fp file pointer to the wav file
 * @param length_in_frames pointer to store the length of the audio in frames (75 frames per second)
 * @param length_in_bytes pointer to store the length of the pcm audio data in bytes
 * @param disc_info pointer to update the disc information
 * @param track_info pointer to update the track information
 * @return 0 on success, non-zero on failure
 */
int wav_open(FILE *fp, int *length_in_frames, long *length_in_bytes, disc *disc_info, track *track_info) {
  long size = -1;
  if (fp != NULL) {
    // go to the end of the file
    if (fseek(fp, 0L, SEEK_END) == 0) {
      // get the size of the file
      size = ftell(fp);
      // go back to the start of the file
      if (size > WAV_HEADER_SIZE && fseek(fp, 0L, SEEK_SET) == 0) {
        char header[WAV_HEADER_SIZE];
        fread(header, sizeof(char), WAV_HEADER_SIZE, fp);
        if (ferror(fp) == 0) {
          // header read from file: check for "RIFF" and "WAVE" identifiers
          if (strncmp(header, "RIFF", 4) == 0 && strncmp(header + 8, "WAVEfmt ", 8) == 0) {
            // determine the total number of audio bytes
            unsigned long total_bytes = (unsigned long)size - WAV_HEADER_SIZE;

            // extract sample rate, channels and bits per sample from the header
            unsigned int sample_rate = *(unsigned int *)(header + 24);
            unsigned short channels = *(unsigned short *)(header + 22);
            unsigned short bits_per_sample = *(unsigned short *)(header + 34);
            if (sample_rate == 0 || channels == 0 || bits_per_sample == 0) {
              return -3; // invalid wav file header
            }

            // set length in frames, for cd audio we expect 2 channel, 16-bit audio: 4 bytes per sample; 44100 samples per second; 75 frames per second
            if (length_in_frames != NULL) {
              *length_in_frames = (int)(total_bytes * 75 / (channels * (bits_per_sample / 8) * sample_rate));
            }

            // set length of the pcm audio data in bytes
            if (length_in_bytes != NULL) {
              *length_in_bytes = total_bytes;
            }

            // header correct: success
            return 0;
          } else {
            return -2; // not a valid wav file
          }
        }
      }
    }
  }
  return -1; // read, data error
}

/**
 * @brief read wav audio data
 *        PRE: file is opened and header is read with wav_open()
 * @param f file pointer to the wav file
 * @param buffer pointer to the data buffer
 * @param num_bytes number of bytes to read into the buffer
 * @return number of bytes read, or -1 on error
 */
long wav_read(FILE *fp, char *buffer, long num_bytes) {
  if (fp == NULL || buffer == NULL || num_bytes <= 0) {
    return -1;
  }
  return (long)fread(buffer, sizeof(char), num_bytes, fp);
}

/**
 * @brief close the wav file
 * @param f file pointer to the wav file
 * @return 0 on success, non-zero on failure
 */
int wav_close(FILE *fp) {
  if (fp != NULL) {
      fclose(fp);
  }
  return 0; 
}
