/**************************************************************************

  cdextract - server timing functions

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

#ifndef CDE_TIMER_H
#define CDE_TIMER_H

#include <time.h>


/**
 * @brief provide time period between start and end
 */
double elapsed_format(clock_t start, clock_t end, char **o_unit);

/**
 * @brief sleep for the given number of milliseconds.
 */ 
int msleep(long msec);

#endif