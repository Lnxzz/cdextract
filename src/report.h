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

#ifndef REPORT_H
#define REPORT_H

#ifdef __cplusplus
extern "C" {
#endif


#define CDE_MSG_TYPE_ERROR 1            // error message
#define CDE_MSG_TYPE_WARNING 2          // warning message
#define CDE_MSG_TYPE_INFO 3             // information message
#define CDE_MSG_TYPE_DEBUG 4            // debug message
#define CDE_MSG_TYPE_PROGRESS 5         // progress message

#define CDE_MSG_MAX_MESSAGE 32768       // maximum size of a message


/**
 * @brief Report callback pointer
 */
extern void(*external_rpt_callback_ptr)(int, char*);

/**
 * @brief Progress callback pointer
 */
extern void(*external_progress_callback_ptr)(int, int, int, long, float);

/**
 * @brief Logs the reported message
 * 
 *        Calls the configured callback (if available)
 *        to report the given info, debug, warning or error message
 */
extern void cde_report(int rpt_type, const char *rpt_fmt, ...);


#ifdef __cplusplus
}
#endif

#endif