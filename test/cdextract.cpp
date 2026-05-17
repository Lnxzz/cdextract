/**************************************************************************

  cdextract - cpp cd audio extraction test application using libcdextract

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

#include <iostream>
#include <string>
#include <fstream>
#include <streambuf>
#include <chrono>
#include <thread>
#include <stdio.h>
#include <string.h>
#include "../src/report.h"
#include "../src/libcdextract_types.h"
#include "../src/libcdextract.h"
#include "../src/libcdextractpp.h"


/*
 * callback for handling error, warning, info and debug messages
 */
static void main_report_callback(int rpt_type, char *rpt_msg) {
  std::string msg_type;
  switch (rpt_type) {
  case CDE_MSG_TYPE_ERROR:
    msg_type = "ERROR";
    break;
  case CDE_MSG_TYPE_WARNING:
    msg_type = "WARNING";
    break;
  case CDE_MSG_TYPE_INFO:
    msg_type = "INFO";
    break;
  case CDE_MSG_TYPE_DEBUG:
    msg_type = "DEBUG";
    break;
  case CDE_MSG_TYPE_PROGRESS:
    msg_type = "PROGRESS";
    break; 
  default:
    msg_type = "";
    break;
  }
  if (rpt_type == CDE_MSG_TYPE_PROGRESS) {
    std::cout << "\r" << msg_type << ": " << rpt_msg;
  } else {
    std::cout << msg_type << ": " << rpt_msg << "\n";
  }
}

/*
 * callback for showing extraction progress
 */
static void main_progress_callback(int rpt_type, int function, int track, long sector, float percentage) {
  std::string function_str;
  std::string msg_type;
  msg_type = "PROGRESS";
  switch (function)
  {
  case EXTRACT_CB_READ:
    function_str = "read  ";
    break;
  case EXTRACT_CB_VERIFY:
    function_str = "verify";
    break;
  case EXTRACT_CB_WRITE_FILE:
    function_str = "write ";
    break;
  case EXTRACT_CB_END_OF_FILE:
    function_str = "end   ";
    break;
  case EXTRACT_CB_END_OF_DISC:
    function_str = "done  ";
    break;
  default:
    // unsupported callback message type
    function_str =  "function: " + std::to_string(function);
    msg_type = "WARNING";
    break;
  }
  std::cout << "\r[" << msg_type << "; " << function_str << "; track: " << track << "; sector: " << sector << "; percentage:" << percentage << "]";
}

int read_file(char **data, std::string filename) {
  std::ifstream t(filename);
  std::string str;

  t.seekg(0, std::ios::end);   
  str.reserve(t.tellg());
  t.seekg(0, std::ios::beg);

  str.assign((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

  *data = (char *)calloc(str.length()+1, sizeof(char));
  memcpy(*data, str.c_str(), str.length());

  return str.length();
}

inline void show(cdextract::cde_result res) {
  std::cout << "> " << res.first << ": " << res.second << "\n";
  if (res.first < 0) {
    exit(1);
  }
}

/*
 * main function
 */
int main(int argc, char *argv[]) {
  cdextract::cde_result res;
  cdextract::client cde_client;
  std::string file_name =  "../../tests/sheet.cue";;
  std::string device_name = "";
  std::string root_folder = "/tmp"; 
  std::string cddb_folder = "/tmp/cddb"; 
  int virtual_drive = CDE_VIRTUAL_DRIVE_ON;
  int verbose = CDE_VERBOSE_ON;
  int output_type = CDE_OUTPUT_TYPE_FLAC;
  int coverart = CDE_COVERART_COVER_ONLY;
  int eject_when_done = CDE_EJECT_WHEN_DONE_OFF;
  int write_json = CDE_WRITE_JSON_OFF;
  int write_cue_sheet = CDE_WRITE_CUE_SHEET_OFF;
  int write_cddb = CDE_WRITE_CDDB_OFF;
  int show_disc_info = CDE_SHOW_DISC_INFO_OFF;

  // connect
  std::cout << "connect: ";
  show(cde_client.connect(device_name, root_folder, cddb_folder, virtual_drive, verbose, output_type, coverart, eject_when_done, write_json, write_cue_sheet, show_disc_info));

  if (cde_client.is_connected()) {
    std::cout << "connected\n";
  }

  if (virtual_drive) {
    // load cuesheet and download disc info
    char *content;
    size_t csize = read_file(&content, file_name);
    if (csize >= 0) {
      auto disc_info = cde_client.parse_cue_sheet(content, true);
      std::cout << "parse_cue_sheet: done\n";
    }
  } else {
    // prepare disc information
    auto disc_info = cde_client.prepare_disc_info();
    if (disc_info) {
      std::cout << "# prepare disc info: " << disc_info->d_id << "\n";
    } else {
      std::cout << "# prepared: no disc info available\n";
    }
  }

  // extract
  show(cde_client.extract());

  // show progress while extraction is running
  std::chrono::seconds one_sec(1);
  cdextract::cde_progress progress;
  std::this_thread::sleep_for(one_sec);
  while (cde_client.get_status() > CDE_STATUS_IDLE) {
    // check if we need to cancel the extraction
    int ch = std::getchar();
    if (ch == 27) {
      cde_client.cancel_extract();
    }
    // show extract status
    auto [function, track, sector, percentage] = cde_client.extract_status();
    if (verbose == CDE_VERBOSE_ON) {
      std::cout << "\r>>" << function << ": " << track << " " << sector << " " << percentage << "%" << std::endl;
    }
    std::this_thread::sleep_for(one_sec);
  }

  // eject
  show(cde_client.eject());

  // disconnect
  if (cde_client.is_connected()) {
    cde_client.disconnect();
    std::cout << "disconnected\n";
  }

  return 0;
}
