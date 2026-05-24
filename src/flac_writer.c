/**************************************************************************

  libcdextract - flac writer/encoder using libFLAC

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "flac_writer.h"


/**
 * @brief callback function for reporting encoding progress
 * @param encoder the encoder instance calling the callback
 * @param bytes_written bytes written so far
 * @param samples_written samples written so far
 * @param frames_written frames written so far
 * @param total_frames_estimate the estimate of the total number of frames to be written
 * @param client_data the callee's client data set through FLAC__stream_encoder_init_*()
 */
void flac_encode_progress_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written, unsigned total_frames_estimate, void *client_data) {
  (void)encoder;
}

/**
 * @brief callback function for handling encoded flac data
 * @param encoder the encoder instance calling the callback
 * @param buffer the buffer containing the block of encoded data
 * @param bytes number of bytes contained in the buffer
 * @param samples number of samples
 * @param current_frame current audio frame
 * @param client_data the callee's client data set through FLAC__stream_encoder_init_*()
 */
FLAC__StreamEncoderWriteStatus flac_encode_write_callback(const FLAC__StreamEncoder *encoder, const FLAC__byte *buffer, size_t bytes, uint32_t samples, uint32_t current_frame, void *client_data) {
  
  flac_encoder_context *encoder_context = (flac_encoder_context *)client_data;
  
  (void)encoder;

  if (encoder_context->total_samples == 0) {
    fprintf(stderr, "ERROR (flac_encode_write_callback): total_samples count not in STREAMINFO\n");
    return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
  }
  if (encoder_context->channels != 2) {
    fprintf(stderr, "ERROR (flac_encode_write_callback): only stereo streams are supported\n");
    return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
  }
  if (encoder_context->bits_per_sample != 16 && encoder_context->bits_per_sample != 24) {
    fprintf(stderr, "ERROR (flac_encode_write_callback): only 16 and 24 bit streams are supported\n");
    return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
  }
  if (buffer == NULL) {
    fprintf(stderr, "ERROR (flac_encode_write_callback): data is NULL\n");
    return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
  }

  // allocate memory for the output block of data and copy any remaining data if present
  size_t remaining = encoder_context->block_size - encoder_context->block_pos;
  size_t block_size = remaining + bytes;

  if (block_size > encoder_context->block_allocated || remaining > 0) {
    if (block_size > encoder_context->block_allocated) {
      encoder_context->block_allocated = block_size;
    }
    char *tmp = malloc(encoder_context->block_allocated * sizeof(char));
    if (tmp == NULL) {
      return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
    }
    if (remaining > 0) {
      memcpy(tmp, &(encoder_context->block[encoder_context->block_pos]), remaining);
      encoder_context->block_pos = 0;
    }
    free(encoder_context->block);
    encoder_context->block = tmp;    
  }

  // copy decoded PCM samples from FLAC write buffer to the data block
  memcpy(&(encoder_context->block[remaining]), buffer, bytes);

  // set the number of bytes encoded from this last block of data
  encoder_context->block_size = block_size;

  // update the total number of bytes encoded
  encoder_context->total_size += bytes;

  return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

/**
 * @brief feed the flac encoder with data from the buffer
 * @param samples number of samples to process
 * @return 0 on success, non-zero on failure
 */
int flac_feed_encoder(flac_encoder_context *encoder_context, size_t samples) {

  // convert the packed little-endian 16-bit PCM samples from WAVE into an interleaved FLAC__int32 buf for libFLAC
  size_t i;
  for(i = 0; i < samples * encoder_context->channels; i++) {
    // inefficient but simple and works on big- or little-endian machines
    encoder_context->pcm[i] = (FLAC__int32)(((FLAC__int16)(FLAC__int8)encoder_context->buf[2*i+1] << 8) | (FLAC__int16)encoder_context->buf[2*i]);
  }
  // feed samples to encoder
  FLAC__bool ok = FLAC__stream_encoder_process_interleaved(encoder_context->encoder, encoder_context->pcm, samples);

  return ok ? 0 : 1;
}

/**
 * @brief start a new flac file to be encoded
 */
int start_flac(FILE *fp, unsigned int total_bytes, track *track_info, flac_encoder_context *encoder_context) {
  FLAC__StreamEncoderInitStatus init_status;
  FLAC__StreamMetadata_VorbisComment_Entry entry;

  // initialize the flac decoder context
  if (encoder_context->sample_rate == 0) {
    encoder_context->sample_rate = DEFAULT_SAMPLE_RATE;
  }
  if (encoder_context->channels == 0) {
    encoder_context->channels = MAX_CHANNELS;
  }
  if (encoder_context->bits_per_sample == 0) {
    encoder_context->bits_per_sample = DEFAULT_BITS_PER_SAMPLE;
  }
  encoder_context->total_samples = total_bytes / (encoder_context->channels * (encoder_context->bits_per_sample / 8));
  encoder_context->length_in_bytes = total_bytes;
  encoder_context->bytes_left = total_bytes;
  encoder_context->buf_pos = 0;
  encoder_context->block = calloc(DEFAULT_BLOCK_SIZE, sizeof(char));
  encoder_context->block_size = 0;
  encoder_context->block_pos = 0;
  encoder_context->block_allocated = DEFAULT_BLOCK_SIZE;
  encoder_context->total_size = 0;
  // allocate the encoder
  if((encoder_context->encoder = FLAC__stream_encoder_new()) == NULL) {
    fprintf(stderr, "ERROR: allocating encoder\n");
    return 1;
  }

  FLAC__bool ok = FLAC__stream_encoder_set_verify(encoder_context->encoder, 1);
  ok &= FLAC__stream_encoder_set_compression_level(encoder_context->encoder, 5);
  ok &= FLAC__stream_encoder_set_channels(encoder_context->encoder, encoder_context->channels);
  ok &= FLAC__stream_encoder_set_bits_per_sample(encoder_context->encoder, encoder_context->bits_per_sample);
  ok &= FLAC__stream_encoder_set_sample_rate(encoder_context->encoder, encoder_context->sample_rate);
  ok &= FLAC__stream_encoder_set_total_samples_estimate(encoder_context->encoder, encoder_context->total_samples);

  // now add some metadata; we'll add some tags and a padding block
  if(ok) {
    if(ok && (encoder_context->metadata[0] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT)) != NULL &&
             (encoder_context->metadata[1] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING)) != NULL) {
      // add vorbiscomment tags - copy=false: let metadata object take control of entry's allocated string
      if (track_info->t_num > 0 && track_info->t_num <= CDE_MAX_TRACKS) {
        char *t_num_tmp=calloc(4, sizeof(char));
        if (t_num_tmp != NULL) {
          snprintf(t_num_tmp, 4, "%d", track_info->t_num);
          ok &= FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "TRACKNUMBER", t_num_tmp);
          ok &= FLAC__metadata_object_vorbiscomment_append_comment(encoder_context->metadata[0], entry, /**copy=*/1);
          free(entry.entry);
          free(t_num_tmp);
        }
      }
      ok &= FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "TITLE", track_info->t_title);
      ok &= FLAC__metadata_object_vorbiscomment_append_comment(encoder_context->metadata[0], entry, /**copy=*/1);
      free(entry.entry);
      ok &= FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "ARTIST", track_info->t_artist);
      ok &= FLAC__metadata_object_vorbiscomment_append_comment(encoder_context->metadata[0], entry, /**copy=*/1);
      free(entry.entry);
      ok &= FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "ALBUM", track_info->t_album);
      ok &= FLAC__metadata_object_vorbiscomment_append_comment(encoder_context->metadata[0], entry, /**copy=*/1);
      free(entry.entry);
      ok &= FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "GENRE", track_info->t_genre);
      ok &= FLAC__metadata_object_vorbiscomment_append_comment(encoder_context->metadata[0], entry, /**copy=*/1);
      free(entry.entry);
      if (track_info->t_year > CDE_MIN_YEAR && track_info->t_year < CDE_MAX_YEAR) {
        char *t_year_tmp=calloc(5, sizeof(char));
        if (t_year_tmp != NULL) {
          snprintf(t_year_tmp, 5, "%d", track_info->t_year);
          ok &= FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "YEAR", t_year_tmp);
          ok &= FLAC__metadata_object_vorbiscomment_append_comment(encoder_context->metadata[0], entry, /**copy=*/1);
          free(entry.entry);
          free(t_year_tmp);
        }
      }
    }
    if (ok) {
      encoder_context->metadata[1]->length = 1234; // set the padding length
      ok = FLAC__stream_encoder_set_metadata(encoder_context->encoder, encoder_context->metadata, 2);
    } else {
      fprintf(stderr, "ERROR: out of memory or tag error\n");
    }
  }
  // initialize encoder
  if(ok) {
    if (fp != NULL) {
      // encode to file
      init_status = FLAC__stream_encoder_init_FILE(encoder_context->encoder, fp, flac_encode_progress_callback, (void *)encoder_context);
    } else {
      // encode to stream
      init_status = FLAC__stream_encoder_init_stream(encoder_context->encoder, flac_encode_write_callback, NULL, NULL, NULL, (void *)encoder_context);
    }
    if(init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
      fprintf(stderr, "ERROR: initializing encoder: %s\n", FLAC__StreamEncoderInitStatusString[init_status]);
      ok = 0;
    }
  }
  return ok ? 0 : 1;
}

/**
 * @brief write data to file by feeding the flac encoder
 *        note: the encoded data is written to the file identified by fp at the start or the flac encoder (start_flac)
 * @param encoder_context the flac encoder context
 * @param buffer buffer from which to read the audio data
 * @param num_bytes bytes to read from the buffer (use 0 to flush remaining bytes in buffer)
 * @return 0 on success, non-zero on failure
 */
int write_flac(flac_encoder_context *encoder_context, char *buffer, long num_bytes) {
  if (encoder_context == NULL || (buffer == NULL && num_bytes > 0) || num_bytes < 0) {
    return -1; // invalid parameters
  }
  
  // set the number of bytes needed for a single sample containing all channels
  size_t ch_bytes_per_sample = encoder_context->channels * (encoder_context->bits_per_sample / 8);
  
  // check if we need to flush the remaining buffer
  if (num_bytes == 0 && encoder_context->buf_pos > 0) {
    size_t remaining_samples = encoder_context->buf_pos / ch_bytes_per_sample;
    if (flac_feed_encoder(encoder_context, remaining_samples)) {
      fprintf(stderr, "ERROR: unable to feed the encoder to flush the remaining buffer\n");
    }
    encoder_context->buf_pos = 0;
    return 0;
  }
	
  int res = 0;
  // fill the internal buffer and feed the encoder with it while we have enough input data
  while (encoder_context->buf_pos + num_bytes > SAMPLES_PER_BLOCK * ch_bytes_per_sample) {
    // fill the internal buffer first
    memcpy(&(encoder_context->buf[encoder_context->buf_pos]), buffer, (SAMPLES_PER_BLOCK * ch_bytes_per_sample) - encoder_context->buf_pos);	

    // feed samples to the flac encoder
    res &= flac_feed_encoder(encoder_context, SAMPLES_PER_BLOCK);

    // set internal buffer position
    unsigned bytes_processed = ((SAMPLES_PER_BLOCK * ch_bytes_per_sample) - encoder_context->buf_pos);
    num_bytes -= bytes_processed;	
    encoder_context->bytes_left -= bytes_processed;
    buffer += bytes_processed;
    encoder_context->buf_pos = 0;
  }

  // more left? or not enough data at start?: place received remaining data in internal buffer
  if(buffer && num_bytes) {
    memcpy(&(encoder_context->buf[encoder_context->buf_pos]), buffer, num_bytes);
    encoder_context->bytes_left -= num_bytes;
    buffer += num_bytes;
    encoder_context->buf_pos += num_bytes;
  }
  
  return res;
}

/**
 * @brief write data to output buffer by feeding the flac encoder
 *        note: when starting the flac encoder (start_flac) the output file pointer should be set at NULL
 * @param encoder_context the flac encoder context
 * @param fp file pointer to the open input file to read pcm data from
 * @param buffer_out output buffer to write the flac encoded audio data
 * @param max_bytes_out maximum number of bytes to write to the output buffer (use -1 to flush remaining bytes in buffer)
 * @return length of the data written to the buffer, -1 at end of encoding the stream and -2 on failure
 */
ssize_t write_flac_to_buffer(flac_encoder_context *encoder_context, FILE *fp, char *buffer_out, size_t max_bytes_out) {
  if (encoder_context == NULL || fp == NULL || buffer_out == NULL || max_bytes_out <= 0) {
    return -2; // invalid parameters
  }

  if (encoder_context->bytes_left <= 0 && encoder_context->block_size == 0) {
    return -1; // end of encoding the stream and no data left to copy
  }

  int res = 0;
  size_t bytes_out = 0;

  // set the number of bytes to read a block of pcm data
  size_t read_size = SAMPLES_PER_BLOCK * encoder_context->channels * (encoder_context->bits_per_sample / 8);

  // continue writing metadata and encoded audio data to the output buffer while there is room left
  do {

    // if available, copy block of data with encoded remaining bytes to the output buffer
    if (encoder_context->block_pos < encoder_context->block_size) {
      size_t size_out = encoder_context->block_size - encoder_context->block_pos;
      if (size_out + bytes_out > max_bytes_out) {
        size_out = max_bytes_out - bytes_out;
      }
      memcpy(&(buffer_out[bytes_out]), &(encoder_context->block[encoder_context->block_pos]), size_out);
      bytes_out += size_out;
      encoder_context->block_pos += size_out;
      if (encoder_context->block_pos >= encoder_context->block_size) {
        encoder_context->block_pos = 0;
        encoder_context->block_size = 0;
      } else {
        // block not completely copied
        break;
      }
    }

    // check if there are any bytes left to read
    if (encoder_context->bytes_left <= 0) {
      break;
    }

    // read from the file to fill the internal buffer
    size_t bytes_read = fread(encoder_context->buf, sizeof(char), read_size, fp);

    // update remaining bytes to read
    encoder_context->bytes_left -= bytes_read;

    // feed the samples to the flac encoder
    if (bytes_read == read_size) {
      res &= flac_feed_encoder(encoder_context, SAMPLES_PER_BLOCK);
    } else {
      // end of input stream reached and not enough data for a full block: feed encoder with the remaining samples
      size_t samples_remaining = bytes_read / (encoder_context->channels * (encoder_context->bits_per_sample / 8));
      res &= flac_feed_encoder(encoder_context, samples_remaining);
    }

    // check if there are any bytes left to read
    if (encoder_context->bytes_left <= 0) {

      // finish the encoding process
      FLAC__stream_encoder_finish(encoder_context->encoder);
    }

  } while (bytes_out < max_bytes_out && res == 0);

  if (res != 0) {
    return -2;
  }

  return bytes_out;
}

/**
 * @brief end flac file
 * @param encoder_context the flac encoder context
 * @return 0 on success, non-zero on failure
 */
int end_flac(flac_encoder_context *encoder_context) {
  FLAC__bool ok = 1;
  if (encoder_context != NULL) {
    if (encoder_context->encoder != NULL) {
      // flush remaining data in buffer
      write_flac(encoder_context, NULL, 0);

      // finish the encoding process
      ok = FLAC__stream_encoder_finish(encoder_context->encoder);

      // now that encoding is finished, the metadata can be freed
      FLAC__metadata_object_delete(encoder_context->metadata[0]);
      FLAC__metadata_object_delete(encoder_context->metadata[1]);

      // free the encoder instance
      FLAC__stream_encoder_delete(encoder_context->encoder);
      encoder_context->encoder = NULL;

      // free audio data block
      if (encoder_context->block != NULL) {
        free(encoder_context->block);
        encoder_context->block = NULL;
      }
    }
  }
  return ok ? 0 : 1;
}
