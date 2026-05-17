/**************************************************************************

  libcdextract - flac writer/encoder using libFLAC

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

#ifndef FLAC_WRITER_H
#define FLAC_WRITER_H

#include <stdio.h>
#include "FLAC/metadata.h"
#include "FLAC/stream_encoder.h"
#include "libcdextract_types.h"


#define SAMPLES_PER_BLOCK 1024
#define MAX_CHANNELS 2
#define MAX_BITS_PER_SAMPLE 32
#define DEFAULT_BITS_PER_SAMPLE 16
#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_BLOCK_SIZE 32768


/**
 * @brief context available to the encoder
 */
typedef struct flac_encoder_context {
  unsigned sample_rate;         // sample rate in Hz
  unsigned channels;            // number of channels
  unsigned bits_per_sample;     // bits per sample
  unsigned total_samples;       // total number of audio samples
  unsigned length_in_bytes;     // total audio length in bytes
  unsigned bytes_left;          // bytes left to encode
  int format;                   // format of the requested audio data (wav=0, flac=1, pcm=2)
  FILE  *fp;                    // file pointer to output file
  unsigned buf_pos;             // current position within buf
  FLAC__byte buf[SAMPLES_PER_BLOCK * MAX_CHANNELS * (MAX_BITS_PER_SAMPLE / 8)]; // raw pcm audio input buffer
  FLAC__int32 pcm[SAMPLES_PER_BLOCK * MAX_CHANNELS];  // flac encoder compatible pcm audio input buffer
  FLAC__StreamMetadata *metadata[2]; // flac metadata structures
  FLAC__StreamEncoder *encoder; // pointer to the flac encoder
  char *block;                  // output buffer containing encoded audio data and/or metadata
  size_t block_size;            // number of bytes encoded from the last block of audio or metadata
  size_t block_pos;             // position within the output buffer
  size_t block_allocated;       // allocated output buffer size
  size_t total_size;            // total size of encodded metadata and audio in bytes
  track *track_information;     // pointer to the track information
} flac_encoder_context;


/**
 * @brief callback function for reporting encoding progress
 * @param encoder the encoder instance calling the callback
 * @param bytes_written bytes written so far
 * @param samples_written samples written so far
 * @param frames_written frames written so far
 * @param total_frames_estimate the estimate of the total number of frames to be written
 * @param client_data the callee's client data set through FLAC__stream_encoder_init_*()
 */
extern void flac_encode_progress_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written, unsigned total_frames_estimate, void *client_data);
 
/**
 * @brief callback function for handling encoded flac data
 * @param encoder the encoder instance calling the callback
 * @param buffer the buffer containing the block of encoded data
 * @param bytes number of bytes contained in the buffer
 * @param samples number of samples
 * @param current_frame current audio frame
 * @param client_data the callee's client data set through FLAC__stream_encoder_init_*()
 */
extern FLAC__StreamEncoderWriteStatus flac_encode_write_callback(const FLAC__StreamEncoder *encoder, const FLAC__byte *buffer, size_t bytes, uint32_t samples, uint32_t current_frame, void *client_data);

/**
 * @brief start a new flac file or stream to be encoded
 */
int start_flac(FILE *fp, unsigned int total_bytes, track *track_info, flac_encoder_context *encoder_context);

/**
 * @brief write data to file by feeding the flac encoder
 *        note: the encoded data is written to the file identified by fp at the start or the flac encoder (start_flac)
 * @param encoder_context the flac encoder context
 * @param buffer buffer from which to read the audio data
 * @param num_bytes bytes to read from the buffer (use 0 to flush remaining bytes in buffer)
 * @return 0 on success, non-zero on failure
 */
int write_flac(flac_encoder_context *encoder_context, char *buffer, long num_bytes);

/**
 * @brief write data to output buffer by feeding the flac encoder
 *        note: when starting the flac encoder (start_flac) the output file pointer should be set at NULL
 * @param encoder_context the flac encoder context
 * @param fp file pointer to the open input file to read pcm data from
 * @param buffer_out output buffer to write the flac encoded audio data
 * @param max_bytes_out maximum number of bytes to write to the output buffer (use -1 to flush remaining bytes in buffer)
 * @return length of the data written to the buffer, -1 on failure
 */
ssize_t write_flac_to_buffer(flac_encoder_context *encoder_context, FILE *fp, char *buffer_out, size_t max_bytes_out);

/**
 * @brief end flac file
 * @param encoder_context the flac encoder context
 * @return 0 on success, non-zero on failure
 */
int end_flac(flac_encoder_context *encoder_context);

#endif
