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

#include <memory>
#include <optional>
#include <string>
#include <sstream>
#include <iostream>
#include <tuple>
#include <string.h>
#include <unistd.h>
#include "libcdextract_types.h"
#include "libcdextract.h"
#include "libcdextractpp.h"


namespace cdextract {

  /**
   * @brief constructor 
   * 
   */
  client::client() {
    report_callback_ = cde_report_callback;
    progress_callback_ = cde_progress_callback;
  }

  /**
   * @brief initialize the client with custom callbacks
   *        note: must be set before connecting
   * 
   * @param report_callback 
   * @param progress_callback 
   */
  void client::init_callbacks(void(*report_callback)(int, char*), void(*progress_callback)(int, int, int, long, float)) {
    report_callback_ = report_callback;
    progress_callback_ = progress_callback;
  }

  /**
   * @brief try to determine and connect with the cdrom drive
   * 
   * @return cde_result
   */
  cde_result client::connect(std::string device_name, std::string root_folder, std::string cddb_folder, int virtual_drive, int verbose, int output_type, int coverart, int eject_when_done, int write_json, int write_cue_sheet, int show_disc_info) {

    if (cde_ && *cde_.get() && (*cde_.get())->status > CDE_STATUS_UNINITIALIZED) {
      // already connected: return connection status
      return std::make_pair((*cde_.get())->status, "cdextract: already connected");
    }

    // set effective user id/user group to the real user id/group
    uid_t t;
    t = seteuid(getuid());
    t = setegid(getgid());

    // initialize the cdextract library
    if (device_name.length() > 0) {
      device_name_ = (char *)calloc(device_name.length()+1, sizeof(char));
      strcpy(device_name_, device_name.c_str());
    } else {
      device_name_ = (char *)calloc(1, sizeof(char));
    }
    if (root_folder.length() > 0) {
      root_folder_ = (char *)calloc(root_folder.length()+1, sizeof(char));
      strcpy(root_folder_, root_folder.c_str());
    } else {
      root_folder_ = (char *)calloc(1, sizeof(char));
    }
    if (cddb_folder.length() > 0) {
      cddb_folder_ = (char *)calloc(cddb_folder.length()+1, sizeof(char));
      strcpy(cddb_folder_, cddb_folder.c_str());
    } else {
      cddb_folder_ = (char *)calloc(1, sizeof(char));
    }
    verbose_ = verbose;

    cde_state *cde_state_p = (cde_state *)calloc(1, sizeof(cde_state));
    cde_initialize(cde_state_p, device_name_, root_folder_, cddb_folder_, report_callback_, progress_callback_);
    cde_set_option(cde_state_p, CDE_OPTION_VERBOSE, verbose);
    cde_set_option(cde_state_p, CDE_OPTION_VIRTUAL_DRIVE, virtual_drive);
    cde_set_option(cde_state_p, CDE_OPTION_OUTPUT_TYPE, output_type);
    cde_set_option(cde_state_p, CDE_OPTION_COVERART, coverart);
    cde_set_option(cde_state_p, CDE_OPTION_EJECT_WHEN_DONE, eject_when_done);
    cde_set_option(cde_state_p, CDE_OPTION_WRITE_JSON, write_json);
    cde_set_option(cde_state_p, CDE_OPTION_WRITE_CUE_SHEET, write_cue_sheet);
    cde_set_option(cde_state_p, CDE_OPTION_SHOW_DISC_INFO, show_disc_info);
    cde_ = std::move(std::make_shared<cde_state *>(cde_state_p));

    int res = cde_open_drive(*cde_.get());
    if (res == CDE_OK) {
      return std::make_pair(CDE_STATUS_IDLE, "cdextract: connected with drive");
    } else if (res < -1 && res > -97) {
      return std::make_pair(res, "cdextract: no disc available");
    } 
    
    return std::make_pair(res, "cdextract: unable to open drive");
  }

  /**
   * @brief closes the connection with the cdrom drive
   * 
   */
  void client::disconnect() {
    // cleanup
    if (cde_ && *cde_.get()) {
      cde_cleanup(*cde_.get());
      free(*cde_.get());
      cde_ = nullptr;
    }

    if (device_name_) {
        free(device_name_);
        device_name_ = nullptr;
    }

  }

  bool client::is_connected() {
    if (cde_ && *cde_.get()) {
      return (*cde_.get())->status > CDE_STATUS_UNINITIALIZED;
    }
    return false;
  }

  /**
   * @brief  extract audio cd
   * 
   * @return cde_result 
   */
  cde_result client::extract() {
    int res = -1;
    if (cde_ && *cde_.get()) {
      res = cde_extract_audio(*cde_.get());
      if (res == 0) {
        return std::make_pair(CDE_OK, "cdextract: start cd audio extraction");
      }
    }
    return std::make_pair(res, "cdextract: unable to start cd audio extraction");
  }

  /**
   * @brief get cd audio extraction status
   * 
   * @return cde_result 
   */
  cde_progress client::extract_status() {
    return std::make_tuple(function_, track_, sector_, percentage_);
  }

  /**
   * @brief cancel cd audio extraction
   * 
   * @return cde_result 
   */
  cde_result client::cancel_extract() {
    int res = -1;
    if (cde_ && *cde_.get()) {
      res = cde_cancel_extract(*cde_.get(), 1);
      if (res == CDE_OK) {
          return std::make_pair(res, "cdextract: cancelling cd extraction");
      }
    }
    return std::make_pair(res, "cdextract: unable to cancel cd extraction");
  }

  /**
   * @brief open drive tray / eject disc
   * 
   * @return cde_result 
   */
  cde_result client::eject() {
    if (cde_ && *cde_.get()) {
      if (cde_eject(*cde_.get()) == CDE_ERROR_NO_DRIVE) { 
          return std::make_pair(CDE_ERROR_NO_DRIVE, "cdextract: no drive found"); 
      }
      return std::make_pair(CDE_OK, "cdextract: eject");
    }
    return std::make_pair(CDE_ERROR_NO_DRIVE, "cdextract: no drive found");
  }

  /**
   * @brief close drive tray
   * 
   * @return cde_result 
   */
  cde_result client::close_tray() {
    int res = -1;
    if (cde_ && *cde_.get()) {
      res = cde_close_tray(*cde_.get());
      if (res == CDE_ERROR_NO_DRIVE) { 
        return std::make_pair(CDE_ERROR_NO_DRIVE, "cdextract: no drive found"); 
      } else if (res == CDE_ERROR_NO_DISC) { 
        return std::make_pair(CDE_ERROR_NO_DISC, "cdextract: no disc found"); 
      }
      return std::make_pair(CDE_OK, "cdextract: tray closed");
    }
    return std::make_pair(CDE_ERROR_NO_DRIVE, "cdextract: no drive found"); 
  }

  /**
   * @brief prepare disc_info structure by reading the toc and 
   * calculating the cddb and musicbrainz disc id hashes
   * 
   * @return disc - pointer to the prepared disc information
   */
  disc *client::prepare_disc_info() {
    if (cde_ && *cde_.get()) {
      if (cde_prepare_disc_info(*cde_.get()) == 0) {
        return (*cde_.get())->disc_info;
      }
      // fallback return last available info
      if (*cde_.get() && (*cde_.get())->disc_info) {
        return (*cde_.get())->disc_info;
      }
    }
    return nullptr;
  }

  /**
   * @brief download cddb/musicbrainz disc information and covers
   * 
   * @return disc - pointer to downloaded cddb/musicbrainz disc information
   */
  disc *client::download_disc_info(bool cleanup) {
    if (cde_ && *cde_.get()) {
      if (cde_download_disc_info(*cde_.get(), false, false, false) == 0) {
        return (*cde_.get())->disc_info;
      }
      // fallback return last available info
      if (*cde_.get() && (*cde_.get())->disc_info) {
        return (*cde_.get())->disc_info;
      }
    }
    return nullptr;
  }

  /**
   * @brief get pointer to cddb/musicbrainz disc information and covers
   * 
   * @return disc - pointer to cddb/musicbrainz disc information
   */
  disc *client::get_disc_info() {
    if (*cde_.get() && (*cde_.get())->disc_info) {
      return (*cde_.get())->disc_info;
    }
    return nullptr;
  }

  /**
   * @brief writes the gathered disc information to a file
   */
  int client::write_disc_info() {
    if (*cde_.get() && (*cde_.get())->disc_info) {
      return cde_write_disc_info(*cde_.get(), 1);
    }
    return CDE_STATUS_UNINITIALIZED;
  }

  /**
   * @brief parse the supplied cue sheet to a disc information structure
   *        pre: must be using a virtual drive
   * 
   * @return disc - pointer to the parsed cddb/musicbrainz disc information
   */
   int client::parse_cue_sheet(const char *cue_sheet, bool download_disc_info) {
    int res = CDE_ERROR_NO_DRIVE;
    res = cde_parse_cue_sheet(*cde_.get(), cue_sheet, download_disc_info);
    if ((*cde_.get())->verbose == CDE_VERBOSE_ON) {
      cde_display_disc_info((*cde_.get())->disc_info);
    }
    return res;
  }

  /**
   * @brief writes a cue sheet from the gathered disc information to a file
   * PRE: prepared disc information structure
   */
  int client::write_cue_sheet(int overwrite) {
    if (*cde_.get() && (*cde_.get())->disc_info) {
      return cde_write_cue_sheet(*cde_.get(), overwrite);
    }
    return CDE_STATUS_UNINITIALIZED;
  }

    /**
   * @brief writes a cddb entry from the gathered disc information to a file in xmcd format
   * PRE: prepared disc information structure
   */
  int client::write_cddb_entry(int overwrite) {
    if (*cde_.get() && (*cde_.get())->disc_info) {
      return cde_cddb_write_entry(*cde_.get(), overwrite);
    }
    return CDE_STATUS_UNINITIALIZED;
  }

  /**
   * @brief get the cdextract root folder
   * 
   * @return int the client status
   */
  std::string client::get_root_folder() {
    if (root_folder_) {
      return root_folder_;
    }
    return "";
  }

  /**
   * @brief get the cdextract client status
   * 
   * @return int the client status
   */
  int client::get_status() {
    if (cde_ && *cde_.get()) {
      return (*cde_.get())->status;
    }
    return CDE_STATUS_UNINITIALIZED;
  }

} // namespace cdextract