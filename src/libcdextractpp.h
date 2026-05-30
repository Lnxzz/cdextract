/********************************************************************

  libcdextractpp - cd audio data extraction library C++ wrapper

  Copyright (C) 2021-2026 E. Heerschop (github@heerschop.frl)

  This library uses:
  * the cdda-interface and cdda-paranoia libraries for the audio
    extraction which are part of the cdparanoia III software distribution.
    both libraries are licensed under GNU LGPL v2.1 (or any later version).
    Copyright (C) 2008 Monty monty@xiph.org
  * the CURL library for downloading disc information
    libCURL - Copyright (c) 1996 - 2024, Daniel Stenberg, <daniel@haxx.se>,
    and many contributors
  * libFLAC for encoding the audio data in the lossless FLAC format
    licensed under Xiph.Org's BSD-like license
    Copyright (C) 2000-2009  Josh Coalson
    Copyright (C) 2011-2016  Xiph.Org Foundation
  * libcoverart for downloading cover images from the musicbrainz service
    This library is licensed under GNU LGPL v2.1 (or any later version).
    Copyright (C) 2012 Andrew Hawkins


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

#ifndef LIBCDEXTRACTPP_H
#define LIBCDEXTRACTPP_H

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <iostream>
#include "libcdextract_types.h"
#include "libcdextract.h"

namespace cdextract {

/** cdextract result containing error code and error message */
typedef std::pair<int, std::string> cde_result; 

/** cdextract progress result containing function, track, sector and percentage */
typedef std::tuple<int, int, long, float> cde_progress; 


/*
 * callback handling
 */

static int verbose_;
static int rpt_type_;
static std::string rpt_msg_;

static int function_;
static int track_; 
static long sector_; 
static float percentage_;

/**
 * @brief report callback
 * 
 * @param rpt_type 
 * @param rpt_msg 
 */
static inline void cde_report_callback(int rpt_type, char *rpt_msg) {
  rpt_type_ = rpt_type;
  rpt_msg_ = rpt_msg;
  if (verbose_ == CDE_VERBOSE_ON) {
    std::cout << rpt_type_ << ": " << rpt_msg_ << std::endl;
  }
}

/**
 * @brief progress callback
 * 
 * @param rpt_type 
 * @param function 
 * @param track 
 * @param sector 
 * @param percentage 
 */
static inline void cde_progress_callback(int rpt_type, int function, int track, long sector, float percentage) {
  function_ = function;
  if (function == EXTRACT_CB_READ) {
    track_ = track;
    sector_ = sector;
    percentage_ = percentage;
  } else if (function == EXTRACT_CB_END_OF_FILE) {
    percentage_ = 100;
  }
  if (verbose_ == CDE_VERBOSE_ON) {
    std::cout << "\r" << rpt_type << " " << function_ << ": " << track_ << " " << sector_ << " " << percentage_ << "%" << std::endl;
  }
}


/**
 * @brief cdextract client class declaration
 * 
 */
class client {
 public:
  /**
   * @brief constructor 
   * 
   */
  client();

  /**
   * @brief initialize the client with custom callbacks
   *        note: must be set before connecting
   * 
   * @param report_callback 
   * @param progress_callback 
   */
  void init_callbacks(void(*report_callback)(int, char*), void(*progress_callback)(int, int, int, long, float));

  /**
   * @brief try to determine and connect with the cdrom drive
   * 
   * @return cde_result
   */
  cde_result connect(std::string device_name = "", std::string audio_folder = "/tmp/cdextract", std::string cddb_folder = "/tmp/cddb", std::string web_folder = "/tmp/cdextract", int virtual_drive = CDE_VIRTUAL_DRIVE_OFF, int verbose = CDE_VERBOSE_OFF, int output_type = CDE_OUTPUT_TYPE_FLAC, int coverart = CDE_COVERART_COVER_ONLY, int eject_when_done = CDE_EJECT_WHEN_DONE_OFF, int write_json = CDE_WRITE_JSON_OFF, int write_cue_sheet = CDE_WRITE_CUE_SHEET_OFF, int show_disc_info = CDE_SHOW_DISC_INFO_OFF);

  /**
   * @brief closes the connection with the cdrom drive
   * 
   */
  void disconnect();

  /** 
   * returns true if connected with the cdrom drive 
   */
  bool is_connected();

  /**
   * @brief  extract audio cd
   * 
   * @return cde_result 
   */
  cde_result extract();

  /**
   * @brief get cd audio extraction status
   * 
   * @return cde_result 
   */
  cde_progress extract_status();

  /**
   * @brief cancel cd audio extraction
   * 
   * @return cde_result 
   */
  cde_result cancel_extract();

  /**
   * @brief open drive tray / eject disc
   * 
   * @return cde_result 
   */
  cde_result eject();

  /**
   * @brief close drive tray
   * 
   * @return cde_result 
   */
  cde_result close_tray();

  /**
   * @brief prepare disc_info structure by reading the toc and 
   * calculating the cddb and musicbrainz disc id hashes
   * 
   * @return disc - pointer to the prepared disc information
   */
  disc *prepare_disc_info();

  /**
   * @brief download cddb/musicbrainz disc information and covers
   * 
   * @return disc - pointer to downloaded cddb/musicbrainz disc information
   */
  disc *download_disc_info(bool cleanup);

  /**
   * @brief get pointer to cddb/musicbrainz disc information and covers
   * 
   * @return disc - pointer to cddb/musicbrainz disc information
   */
  disc *get_disc_info();

  /**
   * @brief writes the gathered disc information to a file
   */
  int write_disc_info();

  /**
   * @brief parse the supplied cue sheet to a disc information structure
   *        pre: must be connected to a virtual drive
   * 
   * @return disc - pointer to the parsed cddb/musicbrainz disc information
   */
  int parse_cue_sheet(const char *cue_sheet, bool download_disc_info);

  /**
   * @brief writes a cue sheet from the gathered disc information to a file
   * PRE: prepared disc information structure
   */
   int write_cue_sheet(int overwrite);

  /**
   * @brief writes a cddb entry from the gathered disc information to a file in xmcd format
   * PRE: prepared disc information structure
   */
  int write_cddb_entry(int overwrite);

  /**
   * @brief get the cdextract audio base folder
   * 
   * @return int the client status
   */
  std::string get_audio_folder();

  /**
   * @brief get the cdextract client status
   * 
   * @return int the client status
   */
  int get_status();

 private:
  std::shared_ptr<cde_state *> cde_ = nullptr;
  char *device_name_ = NULL;
  char *audio_folder_ = NULL;
  char *cddb_folder_ = NULL;
  char *web_folder_ = NULL;
  void(*report_callback_)(int, char*) = nullptr;
  void(*progress_callback_)(int, int, int, long, float) = nullptr;
};

} // namespace cdextract

#endif