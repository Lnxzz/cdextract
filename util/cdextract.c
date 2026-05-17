/**************************************************************************

  cdextract - cd audio extraction utility using libcdextract

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

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "libcdextract.h"
#include "report.h"
#include "string_utils.h"
#include "file_utils.h"
#include "json_utils.h"


/**
 * @brief sleep for the given number of milliseconds.
 */ 
int msleep(long msec) {
  struct timespec ts;
  int res;

  if (msec < 0)
  {
      errno = EINVAL;
      return -1;
  }

  ts.tv_sec = msec / 1000;
  ts.tv_nsec = (msec % 1000) * 1000000;

  do {
      res = nanosleep(&ts, &ts);
  } while (res && errno == EINTR);

  return res;
}

/**
 * @brief report the given info, debug, warning or error message
 */
static void main_report(int rpt_type, const char *rpt_fmt, ...) {

  // process format string and passed arguments
  char buffer[1024];
  va_list args;
  va_start(args, rpt_fmt);
  int rc = vsnprintf(buffer, sizeof(buffer), rpt_fmt, args);
  va_end(args);

  // print to default output
  char msg_type[10];
  switch (rpt_type) {
  case CDE_MSG_TYPE_ERROR:
    strcpy(msg_type, "ERROR:  ");
    break;
  case CDE_MSG_TYPE_WARNING:
    strcpy(msg_type, "WARNING:");
    break;
  case CDE_MSG_TYPE_INFO:
    strcpy(msg_type, "INFO:   ");
    break;
  case CDE_MSG_TYPE_DEBUG:
    strcpy(msg_type, "DEBUG:  ");
    break;
  case CDE_MSG_TYPE_PROGRESS:
    strcpy(msg_type, "PROGRESS");
    break; 
  default:
    strcpy(msg_type, ":       ");
    break;
  }
  if (rpt_type == CDE_MSG_TYPE_PROGRESS) {
    fprintf(stdout, "\rcd_extract: %s: %s\n", msg_type, buffer);
  } else {
    fprintf(stdout, "cd_extract: %s %s\n", msg_type, buffer);
  }
}

/**
 * @brief callback for handling error, warning, info and debug messages
 */
static void main_report_callback(int rpt_type, char *rpt_msg) {
  main_report(rpt_type, rpt_msg);
}

/**
 * @brief callback for showing extraction progress
 */
static void main_progress_callback(int rpt_type, int function, int track, long sector, float percentage) {
  char function_str[16];
  char msg_type[10];
  strcpy(msg_type, "PROGRESS");
  switch (function) {
  case EXTRACT_CB_READ:
    strcpy(function_str, "read  ");
    break;
  case EXTRACT_CB_VERIFY:
    strcpy(function_str, "verify");
    break;
  case EXTRACT_CB_WRITE_FILE:
    strcpy(function_str, "write ");
    break;
  case EXTRACT_CB_END_OF_FILE:
    strcpy(function_str, "end   ");
    break;
  case EXTRACT_CB_END_OF_DISC:
    strcpy(function_str, "done  ");
    break;
  default:
    // unsupported callback message type
    sprintf(function_str, "function: %d", function);
    strcpy(msg_type, "WARNING");
    break;
  }
  fprintf(stdout, "\rcd_extract: [%s; %s; track: %d; sector:%ld; percentage:%.1f%%]    ", msg_type, function_str, track, sector, percentage);
}

/**
 * @brief main function
 */
int main(int argc, char *argv[]) {

  int output_type = CDE_OUTPUT_TYPE_FLAC;
  int verbose = CDE_VERBOSE_OFF;

  char cmd = ' ';
  char *str = calloc(256, sizeof(char));

  char *root_folder = calloc(15, sizeof(char));
  strcpy(root_folder, "/tmp/cdextract");
  
  char *cddb_folder = calloc(10, sizeof(char));
  strcpy(cddb_folder, "/tmp/cddb");

  char *device_name = NULL;

  char *cue_sheet = calloc(15, sizeof(char));
  strcpy(cue_sheet, "/tmp/sheet.cue");

  // process the command line
  int user_input = 1; // default: interactive mode
  for (int i=1; i< argc; i++) {
    if (starts_with("-e", argv[i])) {
      cmd = 'e';
      user_input = 0;
    } else if (starts_with("-g", argv[i])) {
      cmd = 'g';
      user_input = 0;
    } else if (starts_with("-o", argv[i])) {
      cmd = 'o';
      user_input = 0;
    } else if (starts_with("-c", argv[i])) {
      cmd = 'c';
      user_input = 0;
    } else if (starts_with("-t", argv[i])) {
      cmd = 't';
      user_input = 0;
    } else if (starts_with("-twav", argv[i])) {
      output_type = CDE_OUTPUT_TYPE_WAV;
    } else if (starts_with("-d", argv[i])) {
      device_name = calloc(strlen(&argv[i][1]), sizeof(char));
      strcpy(device_name, &argv[i][2]);
    } else if (starts_with("-f", argv[i])) {
      root_folder = realloc(root_folder, strlen(&argv[i][1]) * sizeof(char));
      strcpy(root_folder, &argv[i][2]);
    } else if (starts_with("-l", argv[i])) {
      cmd = 'l';
      cue_sheet = realloc(cue_sheet, strlen(&argv[i][1]) * sizeof(char));
      strcpy(cue_sheet, &argv[i][2]);
      user_input = 0;
    } else if (starts_with("-v", argv[i])) {
      verbose = CDE_VERBOSE_ON;
    } else if (starts_with("-h", argv[i])) {
      printf("cd_extract: [options] <command>\n");
      printf("options:\n");
      printf(" -t<flac|wav>\n");
      printf(" -d<drive name>\n");
      printf(" -f<output folder>\n");
      printf(" -v verbose\n");
      printf("commands:\n");
      printf(" -e extract audio data\n");
      printf(" -g get disc information\n");
      printf(" -l<load cue sheet>\n");
      printf(" -o open tray\n");
      printf(" -c close tray\n");
      printf(" -h help\n");
    } 
  }

  // set effective user id/user group to the real user id/group
  uid_t t;
  t = seteuid(getuid());
  t = setegid(getgid());

  // initialize the cdextract library
  cde_state *cde = calloc(1, sizeof(cde_state));
  cde_initialize(cde, device_name, root_folder, cddb_folder, main_report_callback, main_progress_callback);
  cde_set_option(cde, CDE_OPTION_VERBOSE, verbose);
  cde_set_option(cde, CDE_OPTION_VIRTUAL_DRIVE, CDE_VIRTUAL_DRIVE_OFF);
  cde_set_option(cde, CDE_OPTION_OUTPUT_TYPE, output_type);
  cde_set_option(cde, CDE_OPTION_COVERART, CDE_COVERART_COVER_ONLY);
  cde_set_option(cde, CDE_OPTION_EJECT_WHEN_DONE, CDE_EJECT_WHEN_DONE_OFF);
  cde_set_option(cde, CDE_OPTION_WRITE_JSON, CDE_WRITE_JSON_ON);
  cde_set_option(cde, CDE_OPTION_WRITE_CUE_SHEET, CDE_WRITE_CUE_SHEET_ON);
  cde_set_option(cde, CDE_OPTION_WRITE_CDDB, CDE_WRITE_CDDB_ON);
  cde_set_option(cde, CDE_OPTION_SHOW_DISC_INFO, CDE_SHOW_DISC_INFO_ON);

  // show cd extract, cdda and paranoia library versions
  cde_version();

  // try to determine and open the cdrom drive
  int res;
  if (res = cde_open_drive(cde)) {
    main_report(CDE_MSG_TYPE_ERROR, "unable to open drive"); 
  } else {
    // wait until the drive becomes available
    msleep(500);
  }

  // process commands
  do {

    if (user_input) {
      // get command from command line
      printf("cd_extract: please specify command: (e)xtract, (g)et disc information, (o)pen, (c)lose, (a)bort, (l)oad cue sheet, (q)uit\n");
      int n = scanf("%s", str);
      cmd = str[0];
      printf("cd_extract: command '%c' specified\n", cmd);
      if (cmd == 'l') {
        printf("cd_extract: cue sheet file:");
        int n = scanf("%s", str);
        if (n>0) {
          int str_len = strlen(str) + 1;
          cue_sheet = realloc(cue_sheet, str_len * sizeof(char));
          strcpy(cue_sheet, str);
        } else {
          cmd = ' ';
        }
      }
    }
    
    if (cmd == 'e') {

      // extract audio cd
      cde_extract_audio(cde);

    } else if (cmd == 'g') {

      // download cddb/musicbrainz disc information and covers
      cde_download_disc_info(cde, 0, 0, 1);

    } else if (cmd == 'o') {

      // open/eject drive tray
      if (cde_eject(cde) == CDE_ERROR_NO_DRIVE) { 
        main_report(CDE_MSG_TYPE_ERROR, "no drive found"); 
      }

    } else if (cmd == 'c') {

      // close drive tray
      int res = cde_close_tray(cde);
      if (res == CDE_ERROR_NO_DRIVE) { 
        main_report(CDE_MSG_TYPE_ERROR, "no drive found"); 
      } else if (res == CDE_ERROR_NO_DISC) { 
        main_report(CDE_MSG_TYPE_ERROR, "no disc found"); 
      }

    } else if (cmd == 'a') {

      // cancel cd audio extraction
      cde_cancel_extract(cde, 1);

    } else if (cmd == 'q') {

      // quit
      if (cde->status == CDE_STATUS_IDLE || cde->status == CDE_STATUS_INITIALIZED) {
        main_report(CDE_MSG_TYPE_INFO, "bye.");
        user_input = 0;
      } else {
        main_report(CDE_MSG_TYPE_INFO, "finish or cancel current operation first");
      }

    } else if (cmd == 'l') {

      // load cue sheet
      cde_state *cde_cue = calloc(1, sizeof(cde_state));
      cde_initialize(cde_cue, device_name, root_folder, cddb_folder, main_report_callback, main_progress_callback);
      cde_set_option(cde_cue, CDE_OPTION_VERBOSE, verbose);
      cde_set_option(cde_cue, CDE_OPTION_VIRTUAL_DRIVE, CDE_VIRTUAL_DRIVE_ON);

      // open 'virtual' drive
      if (cde_open_drive(cde_cue) != 0) {
        main_report(CDE_MSG_TYPE_ERROR, "unable to open drive"); 
      } else {
        main_report(CDE_MSG_TYPE_INFO, "using virtual drive"); 
      }
      char *cue_data;
      long cue_size = read_file(&cue_data, cue_sheet);
      if (cue_size <= 0) {
        main_report(CDE_MSG_TYPE_ERROR, "unable to read cue sheet file: %s", cue_sheet);
      } else {

        // parse cue sheet
        main_report(CDE_MSG_TYPE_INFO, "parsing cue sheet..");
        res = cde_parse_cue_sheet(cde_cue, cue_data, 1);
        
        //displays the gathered disc information
        main_report(CDE_MSG_TYPE_INFO, "parse cue sheet result: %d", res);
        cde_display_disc_info(cde_cue->disc_info);

        if (res != CDE_OK) {
          main_report(CDE_MSG_TYPE_ERROR, "unable to parse and process cue sheet file: %d", res);
        }

        // cleanup of cue sheet data
        free(cue_data);
      }

      // cleanup
      cde_cleanup(cde_cue);
      free(cde_cue);     
    }
  } while (user_input);

  // cleanup
  cde_cleanup(cde);
  free(cde);

  if (device_name) {
    free(device_name);
  }

  free(cue_sheet);
  free(cddb_folder);
  free(root_folder);
  free(str);
  return 0;
}
