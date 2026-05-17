/**************************************************************************

  libcdextract - cue sheet writer / parser

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

#ifndef CUE_SHEET_H
#define CUE_SHEET_H

#ifdef __cplusplus
extern "C" {
#endif

#include "libcdextract_types.h"


typedef enum cue_keyword {
    CUE_CATALOG = 0,                      // A 13-digit UPC/EAN code, also referred to as the Media Catolog Number (MCN). 12-digit UPC codes should be prefixed with a "0"
    CUE_CDTEXTFILE = 1,                   // A path to a file containing CD-Text info
    CUE_FILE = 2,                         // A path to a file containing audio data, and to which subsequent commands apply
    CUE_FLAGS = 3,                        // Per-track subcode flag(s):
    CUE_DCP = 4,                          //      Digital copy permitted
    CUE_4CH = 5,                          //      Four channel audio
    CUE_PRE = 6,                          //      Pre-emphasis enabled (audio tracks only)
    CUE_SCMS = 7,                         //      Serial Copy Management System
    CUE_INDEX = 8,                        // Per-track index(es)
    CUE_ISRC = 9,                         // Per-track ISRC(s)
    CUE_PERFORMER = 10,                   // Per-disc or per-track performer name for CD-Text data
    CUE_POSTGAP = 11,                     // Amount of post-track silence to add
    CUE_PREGAP = 12,                      // Amount of pre-track silence to add
    CUE_SONGWRITER = 13,                  // Per-disc or per-track songwriter name for CD-Text data
    CUE_TITLE = 14,                       // Per-disc or per-track title for CD-Text data
    CUE_TRACK = 15,                       // Type of track to create, and to which subsequent commands apply
    CUE_REM = 16,                         // A remark/comment
    CUE_REM_GENRE = 17,                   // Genre
    CUE_REM_CATEGORY = 18,                // Category
    CUE_REM_DATE = 19,                    // Release date
    CUE_REM_DISCID = 20,                  // Read disc id
    CUE_REM_LOOKUP = 21,                  // 64-bit internal hash value of the disc information to enable fast lookups
    CUE_REM_COMMENT = 22,                 // Comment
    CUE_REM_LENGTH = 23,                  // Length of disc in frames
    CUE_REM_CDDBQUERY = 24,               // CDDB query
    CUE_REM_CDDBCATEGORY = 25,            // CDDB category
    CUE_REM_CDDBENTRYID = 26,             // CDDB entry id
    CUE_REM_CDDBDISCID = 27,              // CDDB disc id
    CUE_REM_CDDBREVISION = 28,            // CDDB revision
    CUE_REM_MBQUERY = 29,                 // Musicbrainz query
    CUE_REM_MBFUZZYLOOKUP = 30,           // Musicbrainz fuzzy TOC search
    CUE_REM_MBDISCID = 31,                // Musicbrainz disc id
    CUE_REM_MBRELEASEID = 32,             // Musicbrainz release id
    CUE_REM_EXTENDED = 33,                // Extended disc information
    CUE_REM_REPLAYGAIN_ALBUM_GAIN = 34, 	// Album gain
    CUE_REM_REPLAYGAIN_ALBUM_PEAK = 35,   // Album peak
    CUE_REM_REPLAYGAIN_TRACK_GAIN = 36,   // Track gain
    CUE_REM_REPLAYGAIN_TRACK_PEAK = 37,   // Track peak
    CUE_REM_FILE = 38,                    // Path to a file containing track data
    CUE_UNKNOWN = 39,                     // Unknown keyword
} cue_keyword;


/**
 * @brief parses a cue sheet and stores the results in the disc information structure
 *        PRE: allocated disc information structure for all possible tracks
 * @param disc_info the disc information structure
 * @param cue_data the cue sheet data to parse
 * @param verbose print detailed output
 * @return 0 on success, non-zero on failure
 */
int parse_cue_sheet(disc *disc_info, const char *cue_data, int verbose);

/**
 * @brief writes a cue sheet from the gathered disc information to a file
 *        PRE: prepared disc information structure
 * @param disc_info the disc information structure
 * @param folder folder to store the file
 * @param overwrite overwrite the file if it exists.
 * @param verbose print detailed output
 */
int write_cue_sheet(disc *disc_info, const char *folder, int overwrite, int verbose);

#ifdef __cplusplus
}
#endif

#endif /* CUE_SHEET_H */