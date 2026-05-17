/**
 * @brief extract audio cd - actual cd audio extraction
 *        pre: cde_initialize has been called to set the right settings
 */
void *cde_extract_audio(void *state) {
  int res = 1;
  char *file_suffix = "flac";                        // filename suffix
  FILE *out;                                         // audio file to write
  cdrom_paranoia *p = NULL;                          // cdda paranoia

  // get cde_state pointer
  cde_state *cde = (cde_state*)state;

  if (cde == NULL) {
    // no cde_state available
    goto cleanup;
  }

  // get disc information
  if (cde->drv == NULL || cde->disc_info == NULL) {
    if (cde_download_disc_info(cde, 0, 0) != CDE_OK) {
      // no drive and or disc information available
      goto cleanup;
    }
  }

  // change the filename suffix if needed (default set to flac)
  if (cde->output_type==CDE_OUTPUT_TYPE_WAV) { 
    strcpy(file_suffix, "wav");
  }

  // check if cancellation requested before starting the actual data extraction
  if (cde_get_status(cde) == CDE_STATUS_CANCEL) {
    cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: aborting data extraction: extraction cancelled");
    // return to 'idle' state and cleanup
    res = CDE_STATUS_CANCEL;
    goto cleanup;
  }

  // preparation done, update status to extracting
  cde_set_status(cde, CDE_STATUS_EXTRACTING);

  if (cde->drv->nsectors == 1) {
    cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: aborting data extraction: the autosensed/selected sectors per read value is one sector");
    res = 1;
    goto cleanup;
  }

  // set cdrom speed
  if (cdda_speed_set(cde->drv, cde->cd_speed)) {
    if (cde->verbose || cde->cd_speed != -1) {
      cde_report(CDE_MSG_TYPE_WARNING, "cde_extract_audio: attempt to set cdrom speed failed");
    }
  } else {
    if (cde->verbose) {
      cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: set cdrom speed: drive returned ok");
    }
  }

  // set up begin and end sectors of data to extract
  long first_sector = cdda_track_firstsector(cde->drv, 1);
  long last_sector = cdda_disc_lastsector(cde->drv);
  int first_track = cdda_sector_gettrack(cde->drv, first_sector);
  int last_track = cdda_sector_gettrack(cde->drv, last_sector);
  
  long cursor;
  int16_t offset_buffer[1176];
  int offset_buffer_used = 0;
  
  // unable to handle non-audio tracks
  for (int i = first_track; i <= last_track; i++) {
    if (!cdda_track_audiop(cde->drv, i)) {
      cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: disc contains non audio tracks. Aborting data extraction.");
      res = 1;
      goto cleanup;
    }
  }

  cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: extracting data from sector %7ld (track %2d) to sector %7ld (track %2d)", first_sector, first_track, last_sector, last_track);

  // set full paranoia mode, but allow skipping
  int paranoia_mode = PARANOIA_MODE_FULL ^ PARANOIA_MODE_NEVERSKIP;

  // initialize libparanoia
  p = paranoia_init(cde->drv);
  paranoia_modeset(p, paranoia_mode);

  if (cde->verbose) {
    cdda_verbose_set(cde->drv, CDDA_MESSAGE_LOGIT, CDDA_MESSAGE_LOGIT);
  } else {
    cdda_verbose_set(cde->drv, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);
  }
  paranoia_seek(p, cursor = first_sector, SEEK_SET);

  current_track = 0;
  // keep extracting till we reach the end (and don't reach the track extraction limit)
  while (cursor <= last_sector && current_track < CDE_MAX_TRACKS) {
   
    current_track = cdda_sector_gettrack(cde->drv, cursor);
    if (current_track < 1) {
      cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: invalid track (%d) for sector: %ld", current_track, cursor);
      // return to 'idle' state and cleanup
      res = 1;
      goto cleanup;
    }
    track_firstsector = cursor;
    track_lastsector = cdda_track_lastsector(cde->drv, current_track);
    if (track_lastsector > last_sector) {
      track_lastsector = last_sector;
    }

    // set output folder and filename
    char *output_filename = calloc(sizeof(char), PATH_MAX+1);
    set_full_path(&output_filename, cde, current_track-1, file_suffix);
    set_relative_path(&(cde->disc_info->tracks[current_track-1].t_filename), output_filename, cde->root_folder);
    cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: output file %s", cde->disc_info->tracks[current_track-1].t_filename);

    // open file as binary write
    out = fopen(output_filename, "wb");
    if (out == NULL) {
      cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: cannot open output file %s: %s", output_filename, strerror(errno));
      // return to 'idle' state and cleanup
      res = 1;
      goto cleanup;
    }
    cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: outputting to %s", output_filename);
    cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: track: %d; first sector: %ld; last sector: %ld", current_track, track_firstsector, track_lastsector);

    // start output by writing header (and metadata)
    switch (cde->output_type) {
    case CDE_OUTPUT_TYPE_WAV:
      start_wav(out, (track_lastsector - track_firstsector + 1) * CD_FRAMESIZE_RAW);
      break;
    case CDE_OUTPUT_TYPE_FLAC:
      if (start_flac(out, (track_lastsector - track_firstsector + 1) * CD_FRAMESIZE_RAW, &cde->disc_info->tracks[current_track-1])) {
        cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: error starting flac");
        // return to 'idle' state and cleanup
        res = 1;
        goto cleanup;
      }
      break;
    }

    // write buffer to file
    if (offset_buffer_used) {
      cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: offset_buffer_used=true");
      // partial sector from previous batch read
      cursor++;
      if (cde->output_type == CDE_OUTPUT_TYPE_FLAC) {
        // write flac
        if (write_flac(out, ((char *)offset_buffer) + offset_buffer_used, CD_FRAMESIZE_RAW - offset_buffer_used)) {
          cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: error writing output: %s", strerror(errno));
          // return to 'idle' state and cleanup
          res = 1;
          goto cleanup;
        }
      } else {
        // write raw or wav
        if (write_wav(out, ((char *)offset_buffer) + offset_buffer_used, CD_FRAMESIZE_RAW - offset_buffer_used)) {
          cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: error writing output: %s", strerror(errno));
          // return to 'idle' state and cleanup
          res = 1;
          goto cleanup;
        } else {
          cde_report(CDE_MSG_TYPE_DEBUG, "cde_extract_audio: noffset_buffer_used=true; write_wav=ok;");
        }
      }
    }

    int skipped_flag = 0;
    while (cursor <= track_lastsector) {
      // use paranoia to read data
      int16_t *readbuf = paranoia_read_limited(p, cde_progress_callback, cde->max_retries);
      char *err = cdda_errors(cde->drv);
      char *mes = cdda_messages(cde->drv);

      if (mes || err) {
        cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: paranoia_read: %s%s", mes ? mes : "", err ? err : "");
      }
      if (err) {
        free(err);
      }
      if (mes) {
        free(mes);
      }
      if (readbuf == NULL) {
        if (errno == EBADF || errno == ENOMEDIUM) {
          cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: paranoia_read: drive unavailable");
          res = 1;
          goto cleanup;
        }
        skipped_flag = 1;
        cde_report(CDE_MSG_TYPE_ERROR, "cde_extract_audio: paranoia_read: unrecoverable error");
        break;
      }
      if (skipped_flag && cde->abort_on_skip) {
        cursor = track_lastsector + 1;
        break;
      }

      skipped_flag = 0;
      cursor++;

      cde_progress_callback(cursor * (CD_FRAMEWORDS)-1, EXTRACT_CB_WRITE_FILE);

      if (cde->output_type == CDE_OUTPUT_TYPE_FLAC) {
        // write flac
        res = write_flac(out, ((char *)readbuf), CD_FRAMESIZE_RAW);
        if (res) {
          cde_report(CDE_MSG_TYPE_ERROR, "Error writing output: %s", strerror(errno));
          goto cleanup;
        }
      } else {
        // write wav
        res = write_wav(out, ((char *)readbuf), CD_FRAMESIZE_RAW);
        if (res) {
          cde_report(CDE_MSG_TYPE_ERROR, "Error writing output: %s", strerror(errno));
          goto cleanup;
        }
      }

      // check if extraction has been cancelled
      if (cde_get_status(cde) == CDE_STATUS_CANCEL) {
        cde_report(CDE_MSG_TYPE_INFO, "aborting data extraction: extraction cancelled");
        skipped_flag = 1;
        break;
      }

    } // while read sector

    // end of file
    cde_progress_callback(cursor * (CD_FRAMESIZE_RAW / 2) - 1, EXTRACT_CB_END_OF_FILE);
    if (cde->output_type == CDE_OUTPUT_TYPE_FLAC) {
      cde_report(CDE_MSG_TYPE_DEBUG, "end_flac");
      res = end_flac(out);
      if (res) {
        goto cleanup;
      }
    } else {
      cde_report(CDE_MSG_TYPE_DEBUG, "end_wav");
      res = end_wav(out);
      if (res) {
        goto cleanup;
      }
    }

    if (skipped_flag) {
      // remove the file
      cde_report(CDE_MSG_TYPE_INFO, "removing aborted file: %s", output_filename);
      unlink(output_filename);
      // check for cancellation
      if (cde_get_status(cde) == CDE_STATUS_CANCEL) {
        free(output_filename);
        goto cleanup;
      }
      // set the skipped indicator for this track
      cde->disc_info->tracks[current_track-1].t_skipped = 1;
      // make the cursor correct if we have another track
      if (current_track != -1) {
        current_track++;
        cursor = cdda_track_firstsector(cde->drv, current_track);
        paranoia_seek(p, cursor, SEEK_SET);
        offset_buffer_used = 0;
      }
    }
    free(output_filename);
    cde_report(CDE_MSG_TYPE_INFO, "end of track");
  } // while extracting

  // we are done: set the extraction completed indicator 
  cde->disc_info->d_extracted = 1;

  // extraction done progress callback and status report
  cde_progress_callback(cursor * (CD_FRAMEWORDS)-1, EXTRACT_CB_END_OF_DISC);
  cde_report(CDE_MSG_TYPE_INFO, "cde_extract_audio: done");

  // cleanup
cleanup:
  if (p) {
    paranoia_free(p);
    p = NULL;
  }

  // return to idle state
  cde_set_status(cde, CDE_STATUS_IDLE);

  // eject disc if audio extraction completed and requested to do so
  if (cde->eject_when_done == CDE_EJECT_WHEN_DONE_ON && cde->disc_info->d_extracted == 1) {
    cde_eject(cde);
  }

  // terminate thread
  pthread_exit(NULL);

  return (void*) (size_t)res;
}