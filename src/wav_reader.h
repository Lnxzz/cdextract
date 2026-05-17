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

#ifndef WAV_READER_H
#define WAV_READER_H

#include <stdio.h>
#include "libcdextract_types.h"


#define WAV_HEADER_SIZE 44      // size of the wav header in bytes


typedef struct wav_decoding_client {
  unsigned sample_rate;         // sample rate in Hz
  unsigned channels;            // number of channels
  unsigned bits_per_sample;     // bits per sample
  unsigned total_samples;       // total number of samples
  int length_in_frames;         // the length of the audio in frames (75 frames per second)
  long length_in_bytes;         // the length of the pcm audio data in bytes
  long bytes_decoded;           // total number of bytes decoded from the wav file
  int format;                   // format of the requested audio data (wav=0, flac=1, pcm=2)
  FILE *fp_int;                 // file pointer to the input wav file stream
  FILE *fp_out;                 // file pointer to the output wav or pcm file stream
} wav_decoding_client;


/**
 * @brief open a wav file to be read
 * @param fp file pointer to the wav file
 * @param length_in_frames pointer to store the length of the audio in frames (75 frames per second)
 * @param length_in_bytes pointer to store the length of the pcm audio data in bytes
 * @param disc_info pointer to update the disc information
 * @param track_info pointer to update the track information
 * @return 0 on success, non-zero on failure
 */
int wav_open(FILE *fp, int *length_in_frames, long *length_in_bytes, disc *disc_info, track *track_info);

/**
 * @brief read wav audio data
 * @param fp file pointer to the wav file
 * @param buffer pointer to the data buffer
 * @param num_bytes number of bytes to read into the buffer
 * @return number of bytes read, or -1 on error
 */
long wav_read(FILE *fp, char *buffer, long num_bytes);

/**
 * @brief close the wav file
 * @param fp file pointer to the wav file
 * @return 0 on success, non-zero on failure
 */
int wav_close(FILE *fp);

#endif
