
/**************************************************************************

  cdextract - server timing functions

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

#include <errno.h>
#include <time.h>

#include "timer.h"


char *second_s = "s";
char *millisecond_s = "ms";
char *microsecond_s = "μs";

/**
 * @brief provide time period between start and end
 */
double elapsed_format(clock_t start, clock_t end, char **o_unit) {
  double s = ((double)(end - start)) / CLOCKS_PER_SEC;

  if (s >= 1)
    *o_unit = second_s;
  else if ((s *= 1000) >= 1)
    *o_unit = millisecond_s;
  else {
    s *= 1000;
    *o_unit = microsecond_s;
  }

  return s;
}

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