/**************************************************************************

  cdextract - folder scanning functions

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

 #ifndef CDE_SCAN_H
 #define CDE_SCAN_H
 
 #include "db.h"


/**
 * @brief traverse the audio folder and scan for json disc info, audio files and cue sheets
 * @param audio_folder the audio folder to scan
 * @param db the database to store the found disc info
 * @param download_coverart download disc cover art
 * @param write_json write disc information to a json file
 * @param verbose print detailed output
 */
void scan_audio_folder(char *audio_folder, sql_db *db, int download_coverart, int write_json, int verbose);

/**
 * @brief traverse the cddb folder and scan for disc info
 * @param cddb_folder the cddb folder to scan
 * @param verbose print detailed output
 */
void scan_cddb_folder(char *cddb_folder, sql_db *db);

 #endif