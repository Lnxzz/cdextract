/**************************************************************************

  libcdextract - reporting/logging functions

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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "report.h"


/**
 * @brief Report callback pointer
 */
void(*external_rpt_callback_ptr)(int, char*);

/**
 * @brief Progress callback pointer
 */
void(*external_progress_callback_ptr)(int, int, int, long, float);

/**
 * @brief Logs the reported message
 * 
 *        Calls the configured callback (if available)
 *        to report the given info, debug, warning or error message
 */
void cde_report(int rpt_type, const char *rpt_fmt, ...) {

  // process format string and passed arguments
  char buffer[CDE_MSG_MAX_MESSAGE];
  va_list args;
  va_start(args, rpt_fmt);
  vsnprintf(buffer, sizeof(buffer), rpt_fmt, args);
  va_end(args);

  // call external report callback if available
  if (external_rpt_callback_ptr != NULL) {
    (*external_rpt_callback_ptr)(rpt_type, buffer);
  } else {
    // no callback configured, print to default output
    char msg_type[10];
    switch (rpt_type) {
    case CDE_MSG_TYPE_ERROR:
      strcpy(msg_type, "ERROR");
      break;
    case CDE_MSG_TYPE_WARNING:
      strcpy(msg_type, "WARNING");
      break;
    case CDE_MSG_TYPE_INFO:
      strcpy(msg_type, "INFO");
      break;
    case CDE_MSG_TYPE_DEBUG:
      strcpy(msg_type, "DEBUG");
      break;
    case CDE_MSG_TYPE_PROGRESS:
      strcpy(msg_type, "PROGRESS");
      break; 
    default:
      strcpy(msg_type, "");
      break;
    }
    if (rpt_type == CDE_MSG_TYPE_PROGRESS) {
      fprintf(stdout, "\r%s: %s\n", msg_type, buffer);
    } else {
      fprintf(stdout, "%s: %s\n", msg_type, buffer);
    }
  }
}
