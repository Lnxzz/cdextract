/**************************************************************************

  cdextract - sqlite database functions

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

#ifndef CDE_DB_H
#define CDE_DB_H

#include <sqlite3.h>

#include "libcdextract_types.h"


// result codes to indicate success or failure of the database operation
// these codes are compatible with the sqlite3 result codes
#define DB_NO_UPDATE   -4   // database not updated
#define DB_DUPLICATE   -3   // duplicate record found
#define DB_NO_RESULT   -2   // no result available
#define DB_ERROR       -1   // generic error
#define DB_OK           0   // successful result


#define DB_DUPLICATE_CHECK_NONE   0 // no duplicate check, just insert
#define DB_DUPLICATE_CHECK_LOOKUP 1 // duplicate check by lookup in cddb table (default)
#define DB_DUPLICATE_CHECK_TEMP   2 // duplicate check by lookup in temp table


/**
 * @brief database operation mode
 */
enum db_operation_mode {
  DB_CLOSED = 0, 
  DB_NORMAL,
  DB_RESCAN,
  DB_REBUILD,
  DB_BACKUP
};

/**
 * @brief sqlite3 database structure
 */
typedef struct sql_db {
  sqlite3 *database;  // sqlite database connection handle
  char *filename;     // database filename
  char *msg;          // last error message
  int status;         // last command execution status
  int mode;           // database operation mode (0=closed; 1=normal; 2=rescan; 3=backup;)
  pthread_t thread;   // thread used by the backup and rescan process
} sql_db;

/**
 * @brief linked list of disc information structures
 */
typedef struct disc_list {
  disc *disc_info;                // disc information structures
  struct disc_list *disc_next;    // pointer to the next disc information structure
} disc_list;


/**
 * @brief allocate a new disc information structure 
 *        and add it to the end of the disc list
 * @param disc_info_list
 * @param track_count nr of tracks on the disc to add 
 * @return disc_list pointer to disc_list item containing the new disc information structure 
 */
disc_list *alloc_disc_list(disc_list **disc_info_list, int track_count);

/**
 * @brief free a dynamically allocated disc list and containing 
 *        disc information structures
 * @param disc_info_list 
 */
void free_disc_list(disc_list **disc_info_list);

/**
 * @brief push a disc information structure to the disc list
 * @param disc_info_list the list of disc
 * @param disc_info disc information structure to add
 */
void push_disc_list(disc_list **disc_info_list, disc *disc_info);

/**
 * @brief pop a disc information structure from the disc list
 *        note: ensure the disc info is freed with cde_free_disc
 * @param disc_info_list the list of disc
 * @param disc_info disc information structure to add
 * @return 0 if successful
 */
disc *pop_disc_list(disc_list **disc_info_list);

/**
 * @brief execute the given sql statement
 * @param db database structure
 * @param sql_statement the sql statement to execute
 * @return 0 if successful; another value indicates an error
 */
int execute_sql(sql_db *db, const char *sql_statement);

/**
 * @brief open the connection with the specified sqlite3 database
 * @param db database structure
 * @param filename the filename of the database to open
 * @return 0 if successful; another value indicates an error
 */
int open_database(sql_db *db, const char *filename);

/**
 * @brief close the connection with the sqlite3 database
 * @param db database structure
 * @return 0 if successful; another value indicates an error
 */
int close_database(sql_db *db);

/**
 * @brief backup the sqlite3 database
 * @param handle pointer to database structure
 * @return 0 if successful; another value indicates an error
 */
void *backup_database_t(void *handle);

/**
 * @brief backup the sqlite3 database
 *        uses a separate thread to perform the backup proces
 * @param db database structure
 * @return 0 if successful; another value indicates an error
 */
int backup_database(sql_db *db);

/**
 * @brief prepare the rebuild of the local cddb 'cache'
 *        existing indexes will be dropped and a temp table will be created
 * @param db database structure
 * @return 0 if successful; another value indicates an error
 */
int rebuild_cddb_pre(sql_db *db);

/**
 * @brief finish the rebuild of the local cddb 'cache'
 *        indexes for lookup, artist and title will be created 
 *        and the temp table will be dropped
 * @param db database structure
 * @return 0 if successful; another value indicates an error
 */
int rebuild_cddb_post(sql_db *db);

/**
 * @brief get the total number of stored discs
 * @return the number of stored discs - a value <0 indicates an error
 */
long get_total_disc_count(sql_db *db);

/**
 * @brief get the total number of stored tracks
 * @return the number of stored tracks - a value <0 indicates an error
 */
long get_total_track_count(sql_db *db);

/**
 * @brief get the total number of stored artists
 * @return the number of stored artists - a value <0 indicates an error
 */
long get_total_artist_count(sql_db *db);

/**
 * @brief get the disc category id
 * @param db database structure
 * @param category_name the disc category name
 * @return category_id - a value <0 indicates an error
 */
long get_category_id(sql_db *db, const char *category_name);

/**
 * @brief get the artist id, optionally inserts the artist when not yet stored
 * @param db database structure
 * @param artist_name the artist name
 * @param insert indicator to insert record if artist name is not available
 * @return artist_id - a value <0 indicates an error
 */
long get_artist_id(sql_db *db, const char *artist_name, int insert);

/**
 * @brief get the album id, optionally inserts the album when not yet stored
 * @param db database structure
 * @param album_name the album name
 * @param insert indicator to insert record if album name is not available
 * @return album_id - a value <0 indicates an error
 */
long get_album_id(sql_db *db, const char *album_name, int insert);

/**
 * @brief get the genre id, optionally inserts the genre when not yet stored
 * @param db database structure
 * @param genre_name the genre name
 * @param insert indicator to insert record if the genre is not available
 * @return album_id - a value <0 indicates an error
 */
long get_genre_id(sql_db *db, const char *genre_name, int insert);

/**
 * @brief get the id for the specified cover, optionally inserts the cover when not yet stored
 * @param db database structure
 * @param disc_id the disc id
 * @param cover_type the type of cover; 0=front; 1=back
 * @param cover_data the cover data
 * @param cover_size the size of the cover data
 * @param insert indicator to insert the record if it is not available
 * @return disc_id - a value <0 indicates an error
 */
long get_cover_id(sql_db *db, int cover_type, char *cover_data, int cover_size, int insert);

/**
 * @brief get the front or back cover linked to given resource
 * @param db database structure
 * @param resource_id the resource identifier (disc_id) for the disc information structure
 * @param cover_type the type of cover; 0=front; 1=back
 * @param cover_data the cover data
 * @param cover_size the size of the cover data
 * @return 0 if successful; another value indicates an error
 */
int get_cover_from_database(sql_db *db, const char *resource_id, int cover_type, char **cover_data, int *cover_size);

/**
 * @brief update the front or back cover linked to given disc identifier
 * @param db database structure
 * @param disc_id the disc id
 * @param cover_type the type of cover; 0=front; 1=back
 * @param cover_data the cover data
 * @param cover_size the size of the cover data
 * @return 0 if successful; another value indicates an error
 */
int update_cover_in_database(sql_db *db, long disc_id, int cover_type, char *cover_data, int cover_size);

/**
 * @brief get the disc id using the 64-bit lookup hash, the calculated cddb disc id and the length of the disc
 * @param db database structure
 * @param disc_info disc information structure
 * @return disc_id - a value <0 indicates an error
 */
long get_disc_id(sql_db *db, disc *disc_info);

/**
 * @brief check if the given disc id exists
 * @param db database structure
 * @param resource_id the resource identifier (disc_id as string)
 * @return disc_id - a value <0 indicates an error
 */
long exists_disc_id(sql_db *db, const char *resource_id);

/**
 * @brief get the id of the track (rowid) 
 *        identified by disc_id and track_num
 * @param db database structure
 * @param disc_id disc id
 * @param track_num track number
 * @return rowid - the id of the track
 */
long get_track_id(sql_db *db, long disc_id, int track_num);

/**
 * @brief get the number of tracks on the given disc id
 * @param db database structure
 * @param resource_id the resource identifier (disc_id as string)
 * @param track_count number of tracks
 * @return disc_id - the id of the track
 */
long get_track_count(sql_db *db, const char *resource_id, int *track_count);

/**
 * @brief get a list of disc and optional track information filtered by the given criteria from the database
 * @param db database structure
 * @param limit the limit of the list
 * @param offset the offset to start the list
 * @param search the search string to filter the list
 * @param tag the tag to filter the list (0=disc, 1=track, 2=artist, 3=genre, 4=year)
 * @param include_tracks indicator to include the track data
 * @param disc_info_list output disc information list structure
 * @return 0 if successful; another value indicates an error
 */
int get_disc_list_from_database(sql_db *db, int limit, int offset, const char *search, int tag, int include_tracks, disc_list **disc_info_list);

/**
 * @brief get the disc information from the database using the toc information from the disc
 * @param db database structure
 * @param include_cover indicator to include the cover data
 * @param disc_info disc information structure
 * @return 0 if successful; another value indicates an error
 */
int get_disc_from_database_by_toc(sql_db *db, int include_cover, disc *disc_info);

/**
 * @brief get the disc information linked to the given resource id from the database
 * @param db database structure
 * @param resource_id the resource identifier (disc_id) for the disc information structure
 * @param include_cover indicator to include the cover data
 * @param disc_info output disc information structure
 * @return 0 if successful; another value indicates an error
 */
int get_disc_from_database(sql_db *db, const char *resource_id, int include_cover, disc **disc_info);

/**
 * @brief store the given disc information in the database
 * @param db database structure
 * @param disc_info disc information structure
 * @param update indicator to update the record if it is already stored
 * @return 0 if successful; another value indicates an error
 */
int store_disc_in_database(sql_db *db, disc *disc_info, int update);

/**
 * @brief purge all disc entries from the database
 * @param db database structure
 * @return 0 if successful; another value indicates an error
 */
int purge_discs_from_database(sql_db *db);

/**
 * @brief get the cddb entry from the database using the toc information from the disc
 * @param db database structure
 * @param disc_info input disc information structure
 * @param cddb_id the internal cddb identifier if a matching cddb entry is found
 * @param cddb_info output disc information structure
 * @return 0 if successful; another value indicates an error
 */
int get_cddb_entry_from_database_by_toc(sql_db *db, disc *disc_info, long *cddb_id, disc **cddb_info);

/**
 * @brief get the cddb entry from the database using the toc information from the disc
 * @param db database structure
 * @param cddb_id the internal cddb identifier
 * @param cddb_info output disc information structure
 * @return 0 if successful; another value indicates an error
 */
int get_cddb_entry_from_database(sql_db *db, long cddb_id, disc **cddb_info);

/**
 * @brief search for a cddb entry in the database using the provided disc information
 * @param db database structure
 * @param disc_info input disc information structure
 * @param cddb_id the internal cddb identifier if a matching cddb entry is found
 * @param cddb_info output disc information structure
 * @return 0 if successful; another value indicates an error
 */
int search_cddb_entry_in_database(sql_db *db, disc *disc_info, long *cddb_id, disc **cddb_info);

/**
 * @brief update the cddb information identified by cddb_id in the database
 * @param db database structure
 * @param disc_info disc information structure containing the cddb information
 * @param cddb_id the internal cddb identifier
 * @param category_id the cddb file category id
 * @return 0 if successful; another value indicates an error
 */
int update_cddb_entry_in_database(sql_db *db, disc *disc_info, long cddb_id, long category_id);

/**
 * @brief insert the given cddb information in the database
 * @param db database structure
 * @param disc_info disc information structure containing the cddb information
 * @param category_id the cddb file category id
 * @param lookup_method method to determine how to lookup the cddb id (0=by category and file, 1=by lookup in cddb, 2=by lookup in cddb_lookup temp table)
 * @return 0 if successful; another value indicates an error
 */
int insert_cddb_entry_in_database(sql_db *db, disc *disc_info, long category_id, int lookup_method);

/**
 * @brief store the given cddb information in the database by updating the existing information or inserting a new entry
 * @param db database structure
 * @param disc_info disc information structure containing the cddb information
 * @param category_id the cddb file category id
 * @param lookup_method method to determine how to lookup the cddb id (0=by category and file, 1=by lookup in cddb, 2=by lookup in cddb_lookup temp table)
 * @return 0 if successful; another value indicates an error
 */
int store_cddb_entry_in_database(sql_db *db, disc *disc_info, long category_id, int lookup_method);

#endif