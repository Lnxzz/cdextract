/**************************************************************************

  cdextract - server log redirectionfunctions

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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "report.h"
#include "log.h"


/**
 * @brief redirects the standard output to the given file
 *        the original descriptor is stored in org_out
 */
void redirect_stdout(const char *filename, int *file_desc, int *org_out) {
  *file_desc = open(filename, O_RDWR|O_CREAT|O_TRUNC, 0666);
  *org_out = dup(fileno(stdout));
  dup2(*file_desc, fileno(stdout));
}

/**
 * @brief restores the standard output
 */
void restore_stdout(int file_desc, int *org_out) {
  fflush(stdout); close(file_desc);
  dup2(*org_out, fileno(stdout));
  close(*org_out);
}
