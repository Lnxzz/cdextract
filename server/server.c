/**************************************************************************

  cdextract - cd audio extraction server using libcdextract

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

#include <fcntl.h>
#include <getopt.h>
#include <locale.h>
#include <math.h>
#include <microhttpd.h>
#include <signal.h>
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
#include "handler.h"


#define DAEMON_OFF 0                                  // run server process in the foreground
#define DAEMON_ON 1                                   // fork server to the background and run as daemon

#define DEFAULT_PORT 8001                             // default port for the HTTP server
#define DEFAULT_PID_FILE "/tmp/cdextract.pid"         // default pid file for the server process
#define DEFAULT_LOG_FILE "/tmp/cdextract.log"         // default log file for the server process
#define DEFAULT_DB_FILE "/tmp/cdextract.db"           // default database filename
#define DEFAULT_ROOT_FOLDER "/tmp/cdextract"          // default root folder for audio files (/mnt/data/music/flac)
#define DEFAULT_CDDB_FOLDER "/tmp/cdda"               // default folder to import cddb data (/mnt/temp/cdda)

struct MHD_Daemon *http_daemon = NULL;


/**
 * @brief start the HTTP server
 * @return 0=OK, server start
 */
static int start_server(uint16_t port) {
  if (http_daemon != NULL) {
    return 0;
  }
  http_daemon = MHD_start_daemon(MHD_USE_EPOLL_INTERNAL_THREAD, port, NULL, NULL, &request_handler, NULL, MHD_OPTION_NOTIFY_COMPLETED, &request_completed_handler, NULL, MHD_OPTION_END);
  if (http_daemon == NULL) {
    return 1;
  }
  cde_report(CDE_MSG_TYPE_INFO, "cdextract server started at: http://localhost:%d\n", port);
  return 0;
}

/**
 * @brief stop the HTTP server
 */
static void stop_server() {
  if (http_daemon == NULL) {
    return;
  }
  MHD_stop_daemon(http_daemon);
  http_daemon = NULL;
}

/**
 * @brief callback for handling signals
 */ 
void handle_signal(int sig) {
  if (sig == SIGINT || sig == SIGTERM) {
    // stop the server
    stop_server();
    // reset signal handling to default behavior
    signal(sig, SIG_DFL);
  }
}

/**
 * @brief detach from controlling process and run in the background
 */
void fork_to_background(const char* pidfile) {
  pid_t pid;

  // try to fork off the parent process
  pid = fork();
  if (pid < 0) {
    // fork failed
    exit(EXIT_FAILURE);
  }

  // let the parent terminate
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  // the child process becomes session leader
  if (setsid() < 0) {
    exit(EXIT_FAILURE);
  }

  // handle signals
  signal(SIGCHLD, SIG_IGN);
  signal(SIGHUP, SIG_IGN);
  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  // try to fork off for the second time
  pid = fork();
  if (pid < 0) {
    exit(EXIT_FAILURE);
  }

  // let the parent terminate
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  // set new file permissions
  umask(0);

  // change the working directory to the root directory
  int res = chdir("/");
  if (res < 0) {
    // unable to change directory
    exit(EXIT_FAILURE);
  }

  // close the standard file descriptors because we 
  // do not interact directly with the user anymore
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  
  // close all other open file descriptors
  for (long fd = sysconf((int)_SC_OPEN_MAX); fd > 0; fd--) {
    close((int)fd);
  }

  // write PID of process to pidfile
  if (pidfile != NULL) {
    char str[256];
    int pid_fd = open(pidfile, O_RDWR|O_CREAT, 0640);
    if (pid_fd < 0) {
            // unable to open open file
            exit(EXIT_FAILURE);
    }
    if (lockf(pid_fd, F_TLOCK, 0) < 0) {
            // unable to lock file
            exit(EXIT_FAILURE);
    }
    // get current PID
    sprintf(str, "%d\n", getpid());
    // write PID
    ssize_t written = write(pid_fd, str, strlen(str));
    if (written < 0) {
      // unable to write PID
      exit(EXIT_FAILURE);
    }
  }
}

/**
 * @brief main function
 */
int main(int argc, char *argv[]) {

  int daemon_mode = DAEMON_OFF;
  int output_type = CDE_OUTPUT_TYPE_FLAC;
  int db_backup = CDE_BACKUP_OFF;
  int cover_art_img = CDE_COVERART_COVER_ONLY;
  int verbose = CDE_VERBOSE_OFF;
  uint16_t port = DEFAULT_PORT;
  FILE *log_file = NULL;
  char *log_filename = NULL;

  char *pid_file = calloc(strlen(DEFAULT_PID_FILE) + 1, sizeof(char));
  strcpy(pid_file, DEFAULT_PID_FILE);

  char *db_filename = calloc(strlen(DEFAULT_DB_FILE) + 1, sizeof(char));
  strcpy(db_filename, DEFAULT_DB_FILE);

  char *audio_root_folder = calloc(strlen(DEFAULT_ROOT_FOLDER) + 1, sizeof(char));
  strcpy(audio_root_folder, DEFAULT_ROOT_FOLDER);

  char *cddb_folder = calloc(strlen(DEFAULT_CDDB_FOLDER) + 1, sizeof(char));
  strcpy(cddb_folder, DEFAULT_CDDB_FOLDER);

  char *device_name = NULL;

  // set locale to provide unicode support
  setlocale(LC_ALL, "en_US.UTF-8");

  // process the command line
  for (int i=1; i< argc; i++) {
    if (starts_with("-b", argv[i])) {
      daemon_mode = DAEMON_ON;
    } else if (starts_with("-c", argv[i])) {
      cddb_folder = realloc(cddb_folder, strlen(&argv[i][1]) * sizeof(char));
      strcpy(cddb_folder, &argv[i][2]);
    } else if (starts_with("-db", argv[i])) {
      db_filename = realloc(db_filename, strlen(&argv[i][2]) * sizeof(char));
      strcpy(db_filename, &argv[i][3]);
    } else if (starts_with("-d", argv[i])) {
      device_name = calloc(strlen(&argv[i][1]), sizeof(char));
      strcpy(device_name, &argv[i][2]);
    } else if (starts_with("-a", argv[i])) {
      audio_root_folder = realloc(audio_root_folder, strlen(&argv[i][1]) * sizeof(char));
      strcpy(audio_root_folder, &argv[i][2]);
    } else if (starts_with("-l", argv[i])) {
      log_filename = calloc(strlen(&argv[i][1]), sizeof(char));
      strcpy(log_filename, &argv[i][2]);
    } else if (starts_with("-pid", argv[i])) {
      pid_file = realloc(pid_file, strlen(&argv[i][3]) * sizeof(char));
      strcpy(pid_file, &argv[i][4]);
    } else if (starts_with("-p", argv[i])) {
      int p = atoi(&argv[i][2]);
      if (p>0 && p<=65535) {
        port = (uint16_t)p;
      }
    } else  if (starts_with("-s", argv[i])) {
      db_backup = CDE_BACKUP_ON;
    } else if (starts_with("-t", argv[i])) {
      if (strcmp(&argv[i][2], "flac") == 0) {
        output_type = CDE_OUTPUT_TYPE_FLAC;
      } else if (strcmp(&argv[i][2], "wav") == 0) {
        output_type = CDE_OUTPUT_TYPE_WAV;
      } else {
        fprintf(stderr, "Error: unknown audio output type: %s\n", &argv[i][2]);
        return 1;
      }
    } else if (starts_with("-i", argv[i])) {
      int cart_img = atoi(&argv[i][2]);
      if (cart_img>=0 && cart_img<=3) {
        cover_art_img = cart_img;
      }
    } else if (starts_with("-v", argv[i])) {
      verbose = CDE_VERBOSE_ON;
    } else if (starts_with("-h", argv[i])) {
      fprintf(stdout, "%s [option]...\n", argv[0]);
      fprintf(stdout, "options:\n");
      fprintf(stdout, " -b                 background; start server as daemon process and run in the background\n");
      fprintf(stdout, " -c<ccddb folder>   folder to import cddb data; default: '%s'\n", DEFAULT_CDDB_FOLDER);
      fprintf(stdout, " -db<database file> database file; default: '%s'\n", DEFAULT_DB_FILE);
      fprintf(stdout, " -d<drive name>     cd-rom drive name; default auto detect\n");
      fprintf(stdout, " -a<root folder>    root folder for extracted audio data; default: '%s'\n", DEFAULT_ROOT_FOLDER);
      fprintf(stdout, " -l<log file>       log file; default log to standard output\n");
      fprintf(stdout, " -pid<pid file>     pid file; default: '%s'\n", DEFAULT_PID_FILE);
      fprintf(stdout, " -p<port>           server port; default port: %d\n", DEFAULT_PORT);
      fprintf(stdout, " -s                 backup database at startup\n");
      fprintf(stdout, " -t<flac|wav>       flac or wav audio output; default: flac\n");
      fprintf(stdout, " -i<0|1|2|3>        download cover images; 0=off;1=not to file;2=front and back;3=full default: %d\n", CDE_COVERART_COVER_ONLY);
      fprintf(stdout, " -v                 verbose; use verbose messaging\n");
      fprintf(stdout, " -h                 help; show command line options\n\n");

      // show version information, cleanup and exit
      cde_version();
      goto cleanup;
    }
  }

  if (daemon_mode == DAEMON_ON) {
    // if configured as daemon: detach from controlling process and run in the background
    fork_to_background(pid_file);
  }

  // set effective user id/user group to the real user id/group
  seteuid(getuid());
  setegid(getgid());

  // redirect output to the log file when requested or when running as daemon
  if (daemon_mode == DAEMON_ON && log_filename == NULL) {
    log_filename = calloc(strlen(DEFAULT_LOG_FILE) + 1, sizeof(char));
    strcpy(log_filename, DEFAULT_LOG_FILE);
  } 
  if (log_filename != NULL) {
    log_file = fopen(log_filename, "w");
    if (log_file == NULL) {
      fprintf(stderr, "Error:unable to open log file: %s\n", log_filename);
      goto cleanup;
    }
    cde_report_set_output(log_file);
  } else {
    cde_report_set_output(stdout);
  }

  // initialize the request/response handler and the cdextract library context
  init_handler(device_name, audio_root_folder, cddb_folder, db_filename, db_backup);
  handler_set_option(CDE_OPTION_VERBOSE, verbose);
  handler_set_option(CDE_OPTION_VIRTUAL_DRIVE, CDE_VIRTUAL_DRIVE_OFF);
  handler_set_option(CDE_OPTION_OUTPUT_TYPE, output_type);
  handler_set_option(CDE_OPTION_COVERART, cover_art_img);
  handler_set_option(CDE_OPTION_EJECT_WHEN_DONE, CDE_EJECT_WHEN_DONE_OFF);
  handler_set_option(CDE_OPTION_WRITE_JSON, CDE_WRITE_JSON_ON);
  handler_set_option(CDE_OPTION_WRITE_CUE_SHEET, CDE_WRITE_CUE_SHEET_ON);
  handler_set_option(CDE_OPTION_WRITE_CDDB, CDE_WRITE_CDDB_ON);
  handler_set_option(CDE_OPTION_SHOW_DISC_INFO, CDE_SHOW_DISC_INFO_ON);

  // log cd extract, cdda and paranoia library versions
  cde_version();

  // start the HTTP server
  if (start_server(port) != 0) {
    fprintf(stderr, "Error: unable to start server at port: %d\n", port);
    goto cleanup;
  }

  // process commands
  if (daemon_mode == DAEMON_ON) {
    // wait until the signal handler receives a SIGINT signal
    // and initiates the shutdown process of the running services
    for (;;) {
      sleep(10);
    }
  } else {
    // console: wait for key press to shutdown
    fprintf(stderr, "Press Enter to stop the services and exit the application...\n");
    char ch;
    do {
      ch = (char)getchar();
    } while (ch != '\n' && ch != EOF);
    // stop the http server
    stop_server();
  }

  // cleanup of remaining resources and exit
cleanup:
  cleanup_handler();
  if (device_name) {
    free(device_name);
  }
  free(db_filename);
  free(cddb_folder);
  free(audio_root_folder);
  free(pid_file);
  if (log_filename) {
    free(log_filename);
  }
  if (log_file != NULL) {
    fclose(log_file);
  }
  return 0;
}
