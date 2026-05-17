/**************************************************************************

  libcdextract - flac reader/decoder using libFLAC

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
#include "libcdextract_types.h"
#include "string_utils.h"
#include "flac_reader.h"


#define WAV_HEADER_SIZE 44      // size of the wav header in bytes


/**
 * @brief callback function for reporting decoded data
 */
int flac_decode_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data) {
  
  flac_decoder_context *decoder_context = (flac_decoder_context *)client_data;
  
  (void)decoder;

  if (decoder_context->total_samples == 0) {
    fprintf(stderr, "ERROR (flac_decode_callback): total_samples count not in STREAMINFO\n");
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }
  if (decoder_context->channels != 2) {
    fprintf(stderr, "ERROR (flac_decode_callback): only stereo streams are supported\n");
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }
  if (decoder_context->bits_per_sample != 16 && decoder_context->bits_per_sample != 24) {
    fprintf(stderr, "ERROR (flac_decode_callback): only 16 and 24 bit streams are supported\n");
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }
  if (frame->header.channels != decoder_context->channels) {
    fprintf(stderr, "ERROR (flac_decode_callback): this frame contains %u channels (expected %u channels)\n", frame->header.channels, decoder_context->channels);
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }
  if (buffer[0] == NULL) {
    fprintf(stderr, "ERROR (flac_decode_callback): left channel data is NULL\n");
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }
  if (buffer[1] == NULL) {
    fprintf(stderr, "ERROR (flac_decode_callback): right channel is NULL\n");
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }

  // process the first frame
  size_t header = 0;
  if (frame->header.number.sample_number == 0 && decoder_context->format == 0) {
    // add wav header to first frame
    header = WAV_HEADER_SIZE;
  }
  
  // (re)allocate memory for the output block of data
  uint32_t blocksize = frame->header.blocksize * 2 * sizeof(FLAC__int16) + header;
  char *tmp = realloc(decoder_context->block, blocksize * sizeof(char));
  if (tmp == NULL) {
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }
  decoder_context->block = tmp;

  // set wav header
  size_t pos = 0;
  if (header > 0) {
    decoder_context->block[pos++] = 'R';
    decoder_context->block[pos++] = 'I';
    decoder_context->block[pos++] = 'F';
    decoder_context->block[pos++] = 'F';
    uint32_t val_uint32 = decoder_context->length_in_bytes + 36;
    decoder_context->block[pos++] = val_uint32 & 0xFF;
    decoder_context->block[pos++] = (val_uint32 >> 8) & 0xFF;
    decoder_context->block[pos++] = (val_uint32 >> 16) & 0xFF;
    decoder_context->block[pos++] = (val_uint32 >> 24) & 0xFF;
    decoder_context->block[pos++] = 'W';
    decoder_context->block[pos++] = 'A';
    decoder_context->block[pos++] = 'V';
    decoder_context->block[pos++] = 'E';
    decoder_context->block[pos++] = 'f';
    decoder_context->block[pos++] = 'm';
    decoder_context->block[pos++] = 't';
    decoder_context->block[pos++] = ' ';
    val_uint32 = 16;
    decoder_context->block[pos++] = val_uint32 & 0xFF;
    decoder_context->block[pos++] = (val_uint32 >> 8) & 0xFF;
    decoder_context->block[pos++] = (val_uint32 >> 16) & 0xFF;
    decoder_context->block[pos++] = (val_uint32 >> 24) & 0xFF;
    uint16_t val_uint16 = 1;
    decoder_context->block[pos++] = val_uint16 & 0xFF;
    decoder_context->block[pos++] = (val_uint16 >> 8) & 0xFF;
    val_uint16 = (uint16_t)decoder_context->channels;
    decoder_context->block[pos++] = val_uint16 & 0xFF;
    decoder_context->block[pos++] = (val_uint16 >> 8) & 0xFF;
    val_uint32 = (uint32_t)decoder_context->sample_rate;
    decoder_context->block[pos++] = val_uint32 & 0xFF;
    decoder_context->block[pos++] = (val_uint32 >> 8) & 0xFF;
    decoder_context->block[pos++] = (val_uint32 >> 16) & 0xFF;
    decoder_context->block[pos++] = (val_uint32 >> 24) & 0xFF;
    val_uint32 = (uint32_t)(decoder_context->sample_rate * decoder_context->channels * (decoder_context->bits_per_sample / 8));
    decoder_context->block[pos++] = val_uint32 & 0xFF;
    decoder_context->block[pos++] = (val_uint32 >> 8) & 0xFF;
    decoder_context->block[pos++] = (val_uint32 >> 16) & 0xFF;
    decoder_context->block[pos++] = (val_uint32 >> 24) & 0xFF;
    val_uint16 = (uint16_t)(decoder_context->channels * (decoder_context->bits_per_sample / 8));
    decoder_context->block[pos++] = val_uint16 & 0xFF;
    decoder_context->block[pos++] = (val_uint16 >> 8) & 0xFF;
    val_uint16 = (uint16_t)decoder_context->bits_per_sample;
    decoder_context->block[pos++] = val_uint16 & 0xFF;
    decoder_context->block[pos++] = (val_uint16 >> 8) & 0xFF;
    decoder_context->block[pos++] = 'd';
    decoder_context->block[pos++] = 'a';
    decoder_context->block[pos++] = 't';
    decoder_context->block[pos++] = 'a';
    val_uint32 = decoder_context->length_in_bytes + 36;
    decoder_context->block[pos++] = val_uint32 & 0xFF;
    decoder_context->block[pos++] = (val_uint32 >> 8) & 0xFF;
    decoder_context->block[pos++] = (val_uint32 >> 16) & 0xFF;
    decoder_context->block[pos++] = (val_uint32 >> 24) & 0xFF;
  }

  // copy decoded PCM samples from FLAC write buffer to the data block with interleaving
  pos = header;
  for(int i = 0; i < frame->header.blocksize; i++) {
    // copy left channel
    FLAC__int16 l = (FLAC__int16)buffer[0][i];
    decoder_context->block[pos++] = (l & 0xFF); // low byte
    decoder_context->block[pos++] = (l >> 8) & 0xFF; // high byte
    // copy right channel
    FLAC__int16 r = (FLAC__int16)buffer[1][i];
    decoder_context->block[pos++] = (r & 0xFF); // low byte
    decoder_context->block[pos++] = (r >> 8) & 0xFF; // high byte
  }

  // set the number of bytes decoded from this last block of data
  decoder_context->bytes_decoded = (size_t)blocksize;

  // reset the position with the block of data
  decoder_context->pos = 0;

  // update the total number of bytes decoded
  decoder_context->total_decoded += decoder_context->bytes_decoded;

  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

/**
 * @brief callback function for reporting metadata
 */
void flac_metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data) {
  
  flac_decoder_context *decoder_context = (flac_decoder_context *)client_data;

  (void)decoder;

  switch (metadata->type) {
    case FLAC__METADATA_TYPE_STREAMINFO:
      // set stream information
      decoder_context->total_samples = metadata->data.stream_info.total_samples;
      decoder_context->sample_rate = metadata->data.stream_info.sample_rate;
      decoder_context->channels = metadata->data.stream_info.channels;
      decoder_context->bits_per_sample = metadata->data.stream_info.bits_per_sample;
      decoder_context->length_in_frames = (int)(decoder_context->total_samples * 75 / decoder_context->sample_rate);
      decoder_context->length_in_bytes = decoder_context->total_samples * decoder_context->channels * (decoder_context->bits_per_sample / 8);
      break;
    case FLAC__METADATA_TYPE_VORBIS_COMMENT:
      // set track information from vorbis comment metadata
      if (decoder_context->track_information != NULL) {
        int tag_num = -1;
        const char *value, *cptr;

        if ((tag_num = FLAC__metadata_object_vorbiscomment_find_entry_from(metadata, 0, "TRACKNUMBER")) >= 0) {
          value = (const char*) metadata->data.vorbis_comment.comments [tag_num].entry;
          if ((cptr = strchr(value, '=')) != NULL) {
            value = cptr + 1;
          }
          int track_number = atoi(value);
          if (track_number > 0 && track_number <= CDE_MAX_TRACKS && decoder_context->track_information->t_num == 0) {
            // set track number only if not already set
            decoder_context->track_information->t_num = track_number;
            //fprintf(stderr, "DEBUG (flac_metadata_callback): track number: %u; from metadata: %u\n", decoder_context->track_information->t_num, track_number);
          }
        }

        if ((tag_num = FLAC__metadata_object_vorbiscomment_find_entry_from(metadata, 0, "TITLE")) >= 0) {
          value = (const char*) metadata->data.vorbis_comment.comments [tag_num].entry;
          if ((cptr = strchr(value, '=')) != NULL) {
            value = cptr + 1;
          }
          if (strlen(value) > 0) {
            set_string(&(decoder_context->track_information->t_title), value);
            //fprintf(stderr, "DEBUG (flac_metadata_callback): TITLE: %s\n", decoder_context->track_information->t_title);
          }
        }

        if ((tag_num = FLAC__metadata_object_vorbiscomment_find_entry_from(metadata, 0, "ARTIST")) >= 0) {
          value = (const char*) metadata->data.vorbis_comment.comments [tag_num].entry;
          if ((cptr = strchr(value, '=')) != NULL) {
            value = cptr + 1;
          }
          if (strlen(value) > 0) {
            set_string(&(decoder_context->track_information->t_artist), value);
            //fprintf(stderr, "DEBUG (flac_metadata_callback): ARTIST: %s\n", decoder_context->track_information->t_artist);
          }
        }
        
        if ((tag_num = FLAC__metadata_object_vorbiscomment_find_entry_from(metadata, 0, "ALBUM")) >= 0) {
          value = (const char*) metadata->data.vorbis_comment.comments [tag_num].entry;
          if ((cptr = strchr(value, '=')) != NULL) {
            value = cptr + 1;
          }
          if (strlen(value) > 0) {
            set_string(&(decoder_context->track_information->t_album), value);
            //fprintf(stderr, "DEBUG (flac_metadata_callback): ALBUM: %s\n", decoder_context->track_information->t_album);
          }
        }

        if ((tag_num = FLAC__metadata_object_vorbiscomment_find_entry_from(metadata, 0, "GENRE")) >= 0) {
          value = (const char*) metadata->data.vorbis_comment.comments [tag_num].entry;
          if ((cptr = strchr(value, '=')) != NULL) {
            value = cptr + 1;
          }
          if (strlen(value) > 0) {
            set_string(&(decoder_context->track_information->t_genre), value);
            //fprintf(stderr, "DEBUG (flac_metadata_callback): GENRE: %s\n", decoder_context->track_information->t_genre);
          }
        }

        if ((tag_num = FLAC__metadata_object_vorbiscomment_find_entry_from(metadata, 0, "YEAR")) >= 0) {
          value = (const char*) metadata->data.vorbis_comment.comments [tag_num].entry;
          if ((cptr = strchr(value, '=')) != NULL) {
            value = cptr + 1;
          }
          int track_year = atoi(value);
          if (track_year > CDE_MIN_YEAR && track_year < CDE_MAX_YEAR) {
            decoder_context->track_information->t_year = track_year;
            //fprintf(stderr, "DEBUG (flac_metadata_callback): YEAR: %u\n", decoder_context->track_information->t_year);
          }
        }   
      }
      break;
    case FLAC__METADATA_TYPE_PICTURE:
      if (decoder_context->track_information != NULL) {
        // process picture metadata
        const char *value;
        value = (const char*) metadata->data.picture.data;
        FLAC__uint32 data_length = metadata->data.picture.data_length; 
        if (data_length > 0 && value != NULL && decoder_context->disc_information != NULL && decoder_context->disc_information->mb_front_cover_size == 0) {
          // set cover art data
          decoder_context->disc_information->mb_front_cover = malloc(data_length);
          if (decoder_context->disc_information->mb_front_cover != NULL) {
            memcpy(decoder_context->disc_information->mb_front_cover, value, data_length);
            decoder_context->disc_information->mb_front_cover_size = data_length;
          }
        }
        //fprintf(stderr, "DEBUG (flac_metadata_callback): PICTURE data processed (%u bytes)\n", data_length);
      }
      break;
    default:
      break;
  }
}

/**
 * @brief callback function for reporting errors
 */
void flac_error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data) {
  (void)decoder, (void)client_data;

  fprintf(stderr, "ERROR: (flac_error_callback): %s\n", FLAC__StreamDecoderErrorStatusString[status]);
}


/**
 * @brief allocate a flac decoder, set the given decoder context and process the flac file's metadata
 * @param fp file pointer to the flac file
 * @param disc_info pointer to the disc information (optional to set disc cover image)
 * @param track_info pointer to the track information (optional to set track information)
 * @param decoder_context the flac_decoder_context
 * @return 0 on success, non-zero on failure
 */
int flac_open(FILE *fp, disc *disc_info, track *track_info, flac_decoder_context *decoder_context) {

  if (decoder_context == NULL) {
    return -1; // error no flac_decoder_context
  }
  // initialize the flac decoder context
  decoder_context->sample_rate = 0;
  decoder_context->channels = 0;
  decoder_context->bits_per_sample = 0;
  decoder_context->total_samples = 0;
  decoder_context->length_in_frames = 0;
  decoder_context->length_in_bytes = 0;
  decoder_context->bytes_decoded = 0;
  decoder_context->total_decoded = 0;
  // allocate the decoder
  if ((decoder_context->decoder = FLAC__stream_decoder_new()) == NULL) {
    return -1; // error allocating flac decoder
  }
  decoder_context->block = malloc(DEFAULT_BLOCK_SIZE * sizeof(char));
  if (decoder_context->block == NULL) {
    return -1; // error allocating memory for the decoded audio data
  }
  decoder_context->pos = 0;
  // set the pointers to the disc and track information
  decoder_context->disc_information = disc_info;
  decoder_context->track_information = track_info;
  
  FLAC__bool ok = FLAC__stream_decoder_set_md5_checking(decoder_context->decoder, 1);
  if (!ok) {
    return -2; // error setting md5 checking
  }

  ok = FLAC__stream_decoder_set_metadata_ignore_all(decoder_context->decoder);
  if (!ok) {
    return -3; // error setting metadata ignore
  }

  ok = FLAC__stream_decoder_set_metadata_respond(decoder_context->decoder, FLAC__METADATA_TYPE_STREAMINFO);
  if (!ok) {
    return -4; // error setting metadata respond for STREAMINFO
  }

  ok = FLAC__stream_decoder_set_metadata_respond(decoder_context->decoder, FLAC__METADATA_TYPE_PICTURE);
  if (!ok) {
    return -5; // error setting metadata respond for PICTURE
  }

  ok = FLAC__stream_decoder_set_metadata_respond(decoder_context->decoder, FLAC__METADATA_TYPE_VORBIS_COMMENT);
  if (!ok) {
    return -6; // error setting metadata respond for VORBIS_COMMENT
  }

  // initialize the flac decoder with the file pointer
  FLAC__StreamDecoderInitStatus init_status = FLAC__stream_decoder_init_FILE(decoder_context->decoder, fp, (FLAC__StreamDecoderWriteCallback)flac_decode_callback, flac_metadata_callback, flac_error_callback, (void *)decoder_context);
  if(init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
    return -7; // error initializing flac decoder; get status string using: FLAC__StreamDecoderInitStatusString[init_status]
  }
  
  ok = FLAC__stream_decoder_process_until_end_of_metadata(decoder_context->decoder);
  if (!ok) {
    return -8; // error processing metadata
  }
    
  return 0; // success
}

/**
 * @brief read and decode audio data by feeding the flac decoder
 * @param decoder_context the flac decoder context
 * @param buffer buffer to write decoded audio data
 * @param buffer_size size of the buffer
 * @return number of bytes read, or -1 on error
 */
extern long flac_read(flac_decoder_context *decoder_context, char *buffer, size_t buffer_size) {
  if (decoder_context == NULL || buffer == NULL || buffer_size <= 0) {
    return -1; // invalid parameters
  }

  if (decoder_context->pos == decoder_context->bytes_decoded) {
    // decode one audio frame
    //fprintf(stderr, "DEBUG: (flac_read): decode one audio frame\n");
    FLAC__bool ok = FLAC__stream_decoder_process_single(decoder_context->decoder);
    if (!ok) {
      fprintf(stderr, "ERROR: (flac_read): state: %s\n", FLAC__StreamDecoderStateString[FLAC__stream_decoder_get_state(decoder_context->decoder)]);
    }
  } 

  // copy data to buffer
  size_t num_bytes = decoder_context->bytes_decoded - decoder_context->pos;
  if (buffer_size < num_bytes) {
    num_bytes = buffer_size;
  }
  memcpy(buffer, &(decoder_context->block[decoder_context->pos]), num_bytes);
  decoder_context->pos += num_bytes;
  
  if (decoder_context->pos != num_bytes) {
    fprintf(stderr, "DEBUG: (flac_read): pos: %ld; size: %ld; max: %ld\n", decoder_context->pos, num_bytes, buffer_size);
  }

  // return number of decoded audio bytes
  return num_bytes;
}

/**
 * @brief close the flac file
 * @param decoder_context the flac decoder context
 * @return 0 on success, non-zero on failure
 */
extern int flac_close(flac_decoder_context *decoder_context) {
  if (decoder_context != NULL) {
    if (decoder_context->decoder != NULL) {
      if (decoder_context->bytes_decoded > 0) {
        FLAC__stream_decoder_finish(decoder_context->decoder);
      }
      FLAC__stream_decoder_delete(decoder_context->decoder);
      if (decoder_context->block != NULL) {
        free(decoder_context->block);
        decoder_context->decoder = NULL;
      }
    }
    decoder_context->disc_information = NULL;
    decoder_context->track_information = NULL;
  }
  return 0; 
}