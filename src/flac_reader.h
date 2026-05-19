/**************************************************************************

  libcdextract - flac reader/decoder using libFLAC

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

#ifndef FLAC_READER_H
#define FLAC_READER_H

#include <stdio.h>
#include "FLAC/metadata.h"
#include "FLAC/stream_decoder.h"
#include "libcdextract_types.h"


#define DEFAULT_BLOCK_SIZE 32768


typedef struct flac_decoder_context {
  unsigned sample_rate;         // sample rate in Hz
  unsigned channels;            // number of channels
  unsigned bits_per_sample;     // bits per sample
  unsigned total_samples;       // total number of audio samples
  int length_in_frames;         // total audio length in frames
  long length_in_bytes;         // total audio length in bytes
  int format;                   // format of the requested audio data (wav=0, flac=1, pcm=2)
  FLAC__StreamDecoder *decoder; // pointer to the flac decoder
  char *block;                  // decoded block of audio data
  long pos;                     // position with the last decoded block
  size_t bytes_decoded;         // number of bytes decoded from the last audio block
  size_t total_decoded;         // total number of bytes decoded
  disc *disc_information;       // pointer to the disc information (for the cover image)
  track *track_information;     // pointer to the track information
} flac_decoder_context;


/**
 * @brief callback function for reporting decoded data
 */
extern int flac_decode_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data);

/**
 * @brief callback function for reporting metadata
 */
extern void flac_metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);

/**
 * @brief callback function for reporting errors
 */
extern void flac_error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);

/**
 * @brief allocate a flac decoder, set the given decoder context and process the flac file's metadata
 * @param fp file pointer to the flac file
 * @param disc_info pointer to the disc information (optional to set disc cover image)
 * @param track_info pointer to the track information (optional to set track information)
 * @param decoder_context the flac decoder context
 * @return 0 on success, non-zero on failure
 */
extern int flac_open(FILE *fp, disc *disc_info, track *track_info, flac_decoder_context *decoder_context);

/**
 * @brief read and decode audio data by feeding the flac decoder
 * @param decoder_context the flac decoder context
 * @param buffer buffer to write decoded audio data
 * @param buffer_size size of the buffer
 * @return number of bytes read, or -1 on error
 */
extern long flac_read(flac_decoder_context *decoder_context, char *buffer, size_t buffer_size);

/**
 * @brief close the flac file
 * @param decoder_context the flac decoder context
 * @return 0 on success, non-zero on failure
 */
extern int flac_close(flac_decoder_context *decoder_context);

#endif
