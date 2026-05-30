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

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <unistd.h>

#include "base64.h"
#include "sha1.h"
#include "file_utils.h"
#include "string_utils.h"
#include "libcdextract.h"
#include "db.h"
#include "db_schema.h"


#define MAX_LENGTH_DEVIATION 4                   // maximum length deviation in seconds for track matching
#define LIKE_REPLACE_CHARS " [](){}-_&;:'`\\/"   // characters to replace in SQL LIKE queries


/**
 * @brief store the text that describes the error or an
 *        empty string if no error message is available.
 */
void set_error_message(sql_db *db) {
  const char* errmsg = sqlite3_errmsg(db->database);
  if (errmsg) {
    char *tmp = realloc(db->msg, (strlen(errmsg) + 1) * sizeof(char));
    if (tmp == NULL) {
      return;
    }
    db->msg = tmp; 
    strncpy(db->msg, errmsg, strlen(errmsg));
    db->msg[strlen(errmsg)] = '\0';
  } else {
    char *tmp = realloc(db->msg, sizeof(char));
    if (tmp == NULL) {
      return;
    }
    db->msg = tmp;
    db->msg[0] = '\0';
  }
}

/**
 * @brief allocate a new disc information structure 
 *        and add it to the end of the disc list
 * @param disc_info_list
 * @param track_count nr of tracks on the disc to add 
 * @return disc_list pointer to disc_list item containing the new disc information structure 
 */
disc_list *alloc_disc_list(disc_list **disc_info_list, int track_count) {
  disc_list *tmp = *disc_info_list;
  while (tmp != NULL) {
    tmp = (*disc_info_list)->disc_next;
  }
  tmp = (disc_list *)malloc(sizeof(disc_list));
  tmp->disc_info = (disc *)calloc(1, sizeof(disc));
  if (track_count > 0) {
    tmp->disc_info->d_tracks = track_count;
    tmp->disc_info->tracks = (track *)calloc(track_count, sizeof(track));
  }
  tmp->disc_next = NULL;
  return tmp;
}

/**
 * @brief free a dynamically allocated disc list and containing 
 *        disc information structures
 * @param disc_info_list 
 */
void free_disc_list(disc_list **disc_info_list) {
  disc_list *tmp;
  while (*disc_info_list != NULL) {
    tmp = *disc_info_list;
    cde_free_disc(&(tmp->disc_info), -1);
    *disc_info_list = tmp->disc_next;
    free(tmp);
  }
}

/**
 * @brief push a disc information structure to the disc list
 * @param disc_info_list the list of disc
 * @param disc_info disc information structure to add
 */
void push_disc_list(disc_list **disc_info_list, disc *disc_info) {
  disc_list *tmp = (disc_list *)malloc(sizeof(disc_list));
  tmp->disc_info = disc_info;
  tmp->disc_next = *disc_info_list;
  *disc_info_list = tmp;
}

/**
 * @brief pop a disc information structure from the disc list
 *        note: ensure the disc info is freed with cde_free_disc
 * @param disc_info_list the list of disc
 * @param disc_info disc information structure to add
 * @return 0 if successful
 */
disc *pop_disc_list(disc_list **disc_info_list) {
  if (*disc_info_list == NULL) {
    return NULL;
  }
  disc_list *tmp = (*disc_info_list)->disc_next;
  disc *disc_info = (*disc_info_list)->disc_info;
  free(*disc_info_list);
  *disc_info_list = tmp;
  return disc_info;
}

/**
 * @brief execute the given sql statement
 * @param db database structure
 * @param sql_statement the sql statement to execute
 * @return 0 if successful; another value indicates an error
 */
int execute_sql(sql_db *db, const char *sql_statement) {
  char* errmsg = NULL;
  db->status = sqlite3_exec(db->database, sql_statement, NULL, NULL, &errmsg);
  if (errmsg != NULL) {
    set_error_message(db);
    sqlite3_free(errmsg);
  }
  return db->status;
}

/**
 * @brief create the database schema
 * @param sql_statement the sql statement to execute
 * @return 0 if successful; another value indicates an error
 */
int create_schema(sql_db *db) {
  int res = DB_OK;

  const char **create_schema_ptr = db_create_schema;
  while (*create_schema_ptr != NULL && res == DB_OK) {
    res = execute_sql(db, *create_schema_ptr);
    create_schema_ptr++;
  }

  const char **insert_categories_ptr = db_insert_categories;
  while (*insert_categories_ptr != NULL && res == DB_OK) {
    res = execute_sql(db, *insert_categories_ptr);
    insert_categories_ptr++;
  }

  const char **insert_genres_ptr = db_insert_genres;
  while (*insert_genres_ptr != NULL && res == DB_OK) {
    res = execute_sql(db, *insert_genres_ptr);
    insert_genres_ptr++;
  }

  const char **insert_cddb_genres_ptr = db_insert_cddb_genres;
  while (*insert_cddb_genres_ptr != NULL && res == DB_OK) {
    res = execute_sql(db, *insert_cddb_genres_ptr);
    insert_cddb_genres_ptr++;
  }

  res = execute_sql(db, db_create_idx_disc_lookup);
  res = execute_sql(db, db_create_idx_cddb_lookup);

  // try to insert an empty front and back cover
  get_cover_id(db, 0, NULL, 0, 1);
  get_cover_id(db, 1, NULL, 0, 1);

  return res;
}

/**
 * @brief open the connection with the specified sqlite3 database
 * @param db database structure
 * @param filename the filename of the database to open
 * @return 0 if successful; another value indicates an error
 */
int open_database(sql_db *db, const char *filename) {
  if (db==NULL || filename==NULL) {
    return DB_ERROR;
  }
  // (re)set filename
  if (db->filename) {
    char *tmp = realloc(db->filename, (strlen(filename) + 1) * sizeof(char));
    if (tmp == NULL) {
      return DB_ERROR;
    }
    db->filename = tmp;
  } else {
    db->filename = calloc(strlen(filename) + 1, sizeof(char));
  }
  strncpy(db->filename, filename, strlen(filename));
  // (re)set error message
  if (db->msg) {
    char *tmp = realloc(db->msg, sizeof(char));
    if (tmp == NULL) {
      return DB_ERROR;
    }
    db->msg[0] = '\0';
  } else {
    db->msg = calloc(1, sizeof(char));
  }
  // try to open or create the database
  db->status = sqlite3_open(db->filename, &(db->database));
  if(db->status != DB_OK) {
    // cannot open database: log reason
    set_error_message(db);
  } else {
    // execute pragma's
    int res = DB_OK;
    const char **db_pragmas_ptr = db_pragmas_open_db;
    while (*db_pragmas_ptr != NULL && res == DB_OK) {
      res = execute_sql(db, *db_pragmas_ptr);
      db_pragmas_ptr++;
    }
    // database opened, create schema (if not already done)
    db->status = create_schema(db);
  }
  if (db->status == DB_OK) {
    db->mode = DB_NORMAL;
  } else {
    db->mode = DB_CLOSED;
  }
  return db->status;
}

/**
 * @brief close the connection with the sqlite3 database
 * @param db database structure
 * @return 0 if successful; another value indicates an error
 */
int close_database(sql_db *db) {
  if (db==NULL || db->database==NULL) {
    return DB_OK;
  }
  if (db->msg != NULL) {
    free(db->msg);
    db->msg = NULL;
  }
  if (db->filename != NULL) {
    free(db->filename);
    db->filename = NULL;
  }
  db->status = sqlite3_close(db->database);
  db->mode = DB_CLOSED;
  return db->status;
}

/**
 * @brief backup the sqlite3 database to the given file
 * @param handle pointer to database structure
 * @return 0 if successful; another value indicates an error
 */
void *backup_database_t(void *handle) {
  // get pointer to sql_db structure
  sql_db *db = (sql_db*)handle;
  if (db==NULL || db->mode != DB_NORMAL) {
    return (void*) (size_t)DB_ERROR;
  }
  // set database operation mode back to backup
  db->mode = DB_BACKUP;
  // prepare VACUUM statement
  sqlite3_stmt *statement = NULL;
  db->status = sqlite3_prepare_v2(db->database, db_backup_vacuum, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return (void*) (size_t)DB_ERROR;
  }
  // prepare backup filename and remove existing backup if present
  char *backup_filename = calloc(strlen(db->filename)+8 , sizeof(char));
  snprintf(backup_filename, strlen(db->filename)+8, "%s.backup", db->filename);
  unlink(backup_filename);
  // bind filename
  sqlite3_bind_text(statement, 1, backup_filename, -1, SQLITE_STATIC);
  // execute statement
  db->status = sqlite3_step(statement);
  if (db->status != SQLITE_DONE) {
    set_error_message(db);
    sqlite3_finalize(statement);
    free(backup_filename);
    return (void*) (size_t)DB_ERROR;
  }
  // delete statement
  db->status = sqlite3_finalize(statement);
  free(backup_filename);
  // set database operation mode back to normal
  db->mode = DB_NORMAL;
  // terminate thread
  pthread_exit(NULL);
  return (void*) (size_t)db->status;
}

/**
 * @brief backup the sqlite3 database to the given file
 *        uses a separate thread to perform the backup proces
 * @param db database structure
 * @return 0 if successful; another value indicates an error
 */
int backup_database(sql_db *db) {
  if (db==NULL || db->mode != DB_NORMAL) {
    return DB_ERROR;
  }
  pthread_create(&db->thread, NULL, backup_database_t, (void*)db);
  return DB_OK;
}

/**
 * @brief prepare the rebuild of the local cddb 'cache'
 *        existing indexes will be dropped and a temp table will be created
 * @param db database structure
 * @return 0 if successful; another value indicates an error
 */
int rebuild_cddb_pre(sql_db *db) {

  // set foreign key constraint checking off
  int res = execute_sql(db, db_pragma_foreign_keys_off);
  if (res != DB_OK) {
    return res;
  }

  // drop index on cddb discid
  res = execute_sql(db, db_drop_idx_cddb_lookup);
  if (res != DB_OK) {
    return res;
  }

  // drop index on cddb artist
  res = execute_sql(db, db_drop_idx_cddb_artist);
  if (res != DB_OK) {
    return res;
  }

  // drop index on cddb title
  res = execute_sql(db, db_drop_idx_cddb_title);
  if (res != DB_OK) {
    return res;
  }

  // create cddb_lookup temp table for duplicate checking
  res = execute_sql(db, db_table_cddb_lookup_temp);

  return res;
}

/**
 * @brief finish the rebuild of the local cddb 'cache'
 *        indexes for lookup, artist and title will be created 
 *        and the temp table will be dropped
 * @param db database structure
 * @return 0 if successful; another value indicates an error
 */
int rebuild_cddb_post(sql_db *db) {

  // remove cddb_lookup temp table
  int res = execute_sql(db, db_drop_cddb_lookup_temp);
  if (res != DB_OK) {
    return res;
  }

  // create index on the lookup hash
  res = execute_sql(db, db_create_idx_cddb_lookup);
  if (res != DB_OK) {
    return res;
  }

  // create index on cddb artist
  res = execute_sql(db, db_create_idx_cddb_artist);
  if (res != DB_OK) {
    return res;
  }

  // create index on cddb title
  res = execute_sql(db, db_create_idx_cddb_title);
  if (res != DB_OK) {
    return res;
  }

  // foreign key constraint checking on
  res = execute_sql(db, db_pragma_foreign_keys_on);

  return res;
}

/**
 * @brief get the disc category id
 * @param db database structure
 * @param category_name the disc category name
 * @return category_id - a value <0 indicates an error
 */
long get_category_id(sql_db *db, const char *category_name) {
  if (category_name == NULL || strlen(category_name) == 0) {
    return 0; // return 0 (unspecified)
  }
  sqlite3_stmt *statement = NULL;
  long category_id = -1;
  db->status = sqlite3_prepare_v2(db->database, db_select_category_id, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return category_id;
  }
  // bind category name
  char *category_name_lower = calloc(strlen(category_name)+1, sizeof(char));
  to_lower(&category_name_lower, category_name);
  sqlite3_bind_text(statement, 1, category_name_lower, -1, SQLITE_STATIC);
  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      category_id = sqlite3_column_int64(statement, 0);
    } else if (db->status == SQLITE_DONE) {
      break;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      free(category_name_lower);
      return category_id;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);
  free(category_name_lower);
  return category_id;
}

/**
 * @brief get the artist id
 * @param db database structure
 * @param artist_name the artist name
 * @param insert indicator to insert record if artist name is not available
 * @return artist_id - a value <0 indicates an error
 */
long get_artist_id(sql_db *db, const char *artist_name, int insert) {
  if (artist_name == NULL || strlen(artist_name) == 0) {
    return -1;
  }
  sqlite3_stmt *statement = NULL;
  long artist_id = -1;
  if (insert == 1) {
    // try to insert artist returning the artist_id
    db->status = sqlite3_prepare_v2(db->database, db_insert_artist, -1, &statement, NULL);
    if (db->status != DB_OK) {
      set_error_message(db);
      sqlite3_finalize(statement);
      return artist_id;
    }
    // bind artist name
    sqlite3_bind_text(statement, 1, artist_name, -1, SQLITE_STATIC);
    // execute statement and get result
    while (1) {
      db->status = sqlite3_step(statement);
      if (db->status == SQLITE_ROW) {
        artist_id = sqlite3_column_int64(statement, 0);
      } else if (db->status == SQLITE_DONE) {
        break;
      } else {
        set_error_message(db);
        sqlite3_finalize(statement);
        return artist_id;
      }
    }
    // delete statement
    db->status = sqlite3_finalize(statement);
    if (artist_id >= 0) {
      return artist_id;
    } 
  }
  // try to get the artist
  db->status = sqlite3_prepare_v2(db->database, db_select_artist_id, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return artist_id;
  }
  // bind artist name
  sqlite3_bind_text(statement, 1, artist_name, -1, SQLITE_STATIC);
  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      artist_id = sqlite3_column_int64(statement, 0);
    } else if (db->status == SQLITE_DONE) {
      break;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      return artist_id;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);
  return artist_id;
}

/**
 * @brief get the album id
 * @param db database structure
 * @param album_name the album name
 * @param insert indicator to insert record if album name is not available
 * @return album_id - a value <0 indicates an error
 */
long get_album_id(sql_db *db, const char *album_name, int insert) {
  if (album_name == NULL || strlen(album_name) == 0) {
    return -1;
  }
  sqlite3_stmt *statement = NULL;
  long album_id = -1;
  if (insert == 1) {
    // try to insert the album returning the album_id
    db->status = sqlite3_prepare_v2(db->database, db_insert_album, -1, &statement, NULL);
    if (db->status != DB_OK) {
      set_error_message(db);
      sqlite3_finalize(statement);
      return album_id;
    }
    // bind album name
    sqlite3_bind_text(statement, 1, album_name, -1, SQLITE_STATIC);
    // execute statement and get result
    while (1) {
      db->status = sqlite3_step(statement);
      if (db->status == SQLITE_ROW) {
        album_id = sqlite3_column_int64(statement, 0);
      } else if (db->status == SQLITE_DONE) {
        break;
      } else {
        set_error_message(db);
        sqlite3_finalize(statement);
        return album_id;
      }
    }
    // delete statement
    db->status = sqlite3_finalize(statement);
    if (album_id >= 0) {
      return album_id;
    } 
  }
  // try to get the album
  db->status = sqlite3_prepare_v2(db->database, db_select_album_id, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return DB_ERROR;
  }
  // bind album name
  sqlite3_bind_text(statement, 1, album_name, -1, SQLITE_STATIC);
  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      album_id = sqlite3_column_int64(statement, 0);
    } else if (db->status == SQLITE_DONE) {
      break;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      return DB_ERROR;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);
  return album_id;
}

/**
 * @brief get the genre id, optionally inserts the genre when not yet stored
 * @param db database structure
 * @param genre_name the genre name
 * @param insert indicator to insert record if the genre is not available
 * @return album_id - a value <0 indicates an error
 */
long get_genre_id(sql_db *db, const char *genre_name, int insert) {
  if (genre_name == NULL || strlen(genre_name) == 0) {
    return 0; // return 0 (unspecified)
  }
  sqlite3_stmt *statement = NULL;
  long genre_id = -1;
  char *genre_name_lower = calloc(strlen(genre_name)+1, sizeof(char));
  to_lower(&genre_name_lower, genre_name);
  if (insert == 1) {
    // try to insert the genre returning the genre_id
    db->status = sqlite3_prepare_v2(db->database, db_insert_genre, -1, &statement, NULL);
    if (db->status != DB_OK) {
      set_error_message(db);
      sqlite3_finalize(statement);
      free(genre_name_lower);
      return genre_id;
    }
    // bind genre name
    sqlite3_bind_text(statement, 1, genre_name_lower, -1, SQLITE_STATIC);
    // execute statement and get result
    while (1) {
      db->status = sqlite3_step(statement);
      if (db->status == SQLITE_ROW) {
        genre_id = sqlite3_column_int64(statement, 0);
      } else if (db->status == SQLITE_DONE) {
        break;
      } else {
        set_error_message(db);
        sqlite3_finalize(statement);
        free(genre_name_lower);
        return genre_id;
      }
    }
    // delete statement
    db->status = sqlite3_finalize(statement);
    if (genre_id >= 0) {
      free(genre_name_lower);
      return genre_id;
    } 
  }
  // try to get the genre
  db->status = sqlite3_prepare_v2(db->database, db_select_genre_id, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    free(genre_name_lower);
    return genre_id;
  }
  // bind genre name
  sqlite3_bind_text(statement, 1, genre_name_lower, -1, SQLITE_STATIC);
  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      genre_id = sqlite3_column_int64(statement, 0);
    } else if (db->status == SQLITE_DONE) {
      break;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      free(genre_name_lower);
      return genre_id;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);
  free(genre_name_lower);
  return genre_id;
}

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
long get_cover_id(sql_db *db, int cover_type, char *cover_data, int cover_size, int insert) {
  if (db==NULL || cover_type < 0 || cover_type > 1 || cover_data==NULL || cover_size < 0) {
    return DB_ERROR;
  }

  // get the cover id for the updated cover (inserts the cover when not yet stored)
  sqlite3_stmt *statement = NULL;
  long cover_id = -1;

  // create hash of the cover data
  SHA_INFO cover_sha;
  unsigned char cover_digest[20];
  unsigned long	digest_size;
  sha_init(&cover_sha);
  sha_update(&cover_sha, (SHA_BYTE *)cover_data, (size_t)cover_size);
  sha_final(&cover_digest[0], &cover_sha);
  char *cover_hash = (char *)rfc822_binary(&cover_digest[0], sizeof(cover_digest), &digest_size);
  
  // try to insert the cover returning the cover id
  if (insert == 1) {
    db->status = sqlite3_prepare_v2(db->database, db_insert_cover, -1, &statement, NULL);
    if (db->status != DB_OK) {
      set_error_message(db);
      sqlite3_finalize(statement);
      free(cover_hash);
      return cover_id;
    }
    // bind cover type, size, hash and data
    sqlite3_bind_int(statement, 1, cover_type);
    sqlite3_bind_int(statement, 2, cover_size);
    sqlite3_bind_text(statement, 3, cover_hash, -1, SQLITE_STATIC);
    sqlite3_bind_blob(statement, 4, cover_data, cover_size, SQLITE_STATIC);
    // execute statement and get result
    while (1) {
      db->status = sqlite3_step(statement);
      if (db->status == SQLITE_ROW) {
        cover_id = sqlite3_column_int64(statement, 0);
      } else if (db->status == SQLITE_DONE) {
        break;
      } else {
        set_error_message(db);
        sqlite3_finalize(statement);
        free(cover_hash);
        return cover_id;
      }
    }
    // delete statement
        db->status = sqlite3_finalize(statement);
    if (cover_id >= 0) {
      free(cover_hash);
      return cover_id;
    } 
  }
  // try to get cover id if not inserted
  db->status = sqlite3_prepare_v2(db->database, db_select_cover_id, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    free(cover_hash);
    return cover_id;
  }
  // bind cover parameters
  sqlite3_bind_int(statement, 1, cover_type);
  sqlite3_bind_int(statement, 2, cover_size);
  sqlite3_bind_text(statement, 3, cover_hash, -1, SQLITE_STATIC);
  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      cover_id = sqlite3_column_int64(statement, 0);
    } else if (db->status == SQLITE_DONE) {
      break;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      free(cover_hash);
      return cover_id;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);
  free(cover_hash);
  return cover_id;
}

/**
 * @brief get the front or back cover linked to given resource
 * @param db database structure
 * @param resource_id the resource identifier (disc_id) for the disc information structure
 * @param cover_type the type of cover; 0=front; 1=back
 * @param cover_data the cover data
 * @param cover_size the size of the cover data
 * @return 0 if successful; another value indicates an error
 */
int get_cover_from_database(sql_db *db, const char *resource_id, int cover_type, char **cover_data, int *cover_size) {
  if (db==NULL || resource_id==NULL || cover_type < 0 || cover_type > 1) {
    return DB_ERROR;
  }
  sqlite3_stmt *statement = NULL;
  if (cover_type == 1) {
    db->status = sqlite3_prepare_v2(db->database, db_select_back_cover, -1, &statement, NULL);
  } else {
    db->status = sqlite3_prepare_v2(db->database, db_select_front_cover, -1, &statement, NULL);
  }
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return DB_ERROR;
  }
  // convert resource_id to disc_id
  long disc_id = 0;
  if (sscanf(resource_id, "%ld", &disc_id) != 1) {
    return DB_ERROR;
  }
  // bind disc id parameters
  sqlite3_bind_int64(statement, 1, disc_id);
  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      *cover_size = sqlite3_column_bytes(statement, 0);
      set_binary(cover_data, sqlite3_column_blob(statement, 0), *cover_size);
    } else if (db->status == SQLITE_DONE) {
      break;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      return DB_ERROR;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);

  // done. cover data is retrieved
  return DB_OK;
}

/**
 * @brief update the front or back cover linked to given disc identifier
 * @param db database structure
 * @param disc_id the disc id
 * @param cover_type the type of cover; 0=front; 1=back
 * @param cover_data the cover data
 * @param cover_size the size of the cover data
 * @return 0 if successful; another value indicates an error
 */
int update_cover_in_database(sql_db *db, long disc_id, int cover_type, char *cover_data, int cover_size) {
  if (db==NULL || cover_type < 0 || cover_type > 1 || cover_data==NULL || cover_size < 0) {
    return DB_ERROR;
  }
  
  // return if the disc is not already stored
  if (disc_id < 0) {
    return DB_NO_UPDATE;
  }

  long cover_id = get_cover_id(db, cover_type, cover_data, cover_size, 1);

  // unable to get cover id (and insert if needed)
  if (cover_id < 0) {
    return DB_ERROR;
  }

  // update the disc information record
  sqlite3_stmt *statement = NULL;
  if (cover_type == 1) {
    db->status = sqlite3_prepare_v2(db->database, db_update_back_cover_link, -1, &statement, NULL);
  } else {
    db->status = sqlite3_prepare_v2(db->database, db_update_front_cover_link, -1, &statement, NULL);
  }
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return DB_ERROR;
  }
  // bind parameters
  sqlite3_bind_int64(statement, 1, cover_id);
  sqlite3_bind_int64(statement, 2, disc_id);
  // execute statement and get result
  db->status = sqlite3_step(statement);
  if (db->status != SQLITE_DONE) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return DB_ERROR;
  }
  // delete statement
  db->status = sqlite3_finalize(statement);
  // done. cover data is stored
  return DB_OK;
}

/**
 * @brief update the front or back cover linked to given resource
 * @param db database structure
 * @param resource_id the resource identifier (disc_id) for the disc information structure
 * @param cover_type the type of cover; 0=front; 1=back
 * @param cover_data the cover data
 * @param cover_size the size of the cover data
 * @return 0 if successful; another value indicates an error
 */
int update_cover_in_database_by_resource_id_(sql_db *db, const char *resource_id, int cover_type, char *cover_data, int cover_size) {
  if (db==NULL || resource_id==NULL || cover_type < 0 || cover_type > 1 || cover_data==NULL || cover_size < 0) {
    return DB_ERROR;
  }

  // check if the disc identified by the resource id is already stored
  long disc_id = exists_disc_id(db, resource_id);
  
  // return if the disc is not already stored
  if (disc_id < 0) {
    return DB_NO_UPDATE;
  }

  // try update the front or back cover linked to given disc identifier
  return update_cover_in_database(db, disc_id, cover_type, cover_data, cover_size);
}

/**
 * @brief get the disc id using the 64-bit lookup hash, the calculated cddb disc id and the length of the disc
 * @param db database structure
 * @param disc_info disc information structure
 * @return disc_id - a value <0 indicates an error
 */
long get_disc_id(sql_db *db, disc *disc_info) {
  if (db==NULL || disc_info==NULL) {
    return -1;
  }
  sqlite3_stmt *statement = NULL;
  long disc_id = -1;
  db->status = sqlite3_prepare_v2(db->database, db_select_disc_id_by_disc_info, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return disc_id;
  }
  // bind search parameters
  sqlite3_bind_int64(statement, 1, disc_info->d_lookup);
  sqlite3_bind_int64(statement, 2, disc_info->d_id);
  sqlite3_bind_int(statement, 3, disc_info->d_length);
  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      disc_id = sqlite3_column_int64(statement, 0);
    } else if (db->status == SQLITE_DONE) {
      break;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      return disc_id;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);
  return disc_id;
}

/**
 * @brief check if the given disc id exists
 * @param db database structure
 * @param resource_id the resource identifier (disc_id as string)
 * @return disc_id - a value <0 indicates an error
 */
long exists_disc_id(sql_db *db, const char *resource_id) {
  if (db==NULL || resource_id==NULL || strlen(resource_id) == 0) {
    return -1;
  }
  sqlite3_stmt *statement = NULL;
  long disc_id = -1;
  db->status = sqlite3_prepare_v2(db->database, db_exists_disc_id, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return disc_id;
  }
  // convert resource_id to disc_id
  long disc_id_in = 0;
  if (sscanf(resource_id, "%ld", &disc_id_in) != 1) {
    return disc_id;
  }
  // bind the disc id parameter
  sqlite3_bind_int64(statement, 1, disc_id_in);
  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      disc_id = sqlite3_column_int64(statement, 0);
    } else if (db->status == SQLITE_DONE) {
      break;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      return disc_id;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);
  return disc_id;
}

/**
 * @brief get the id of the track (rowid) 
 *        identified by disc_id and track_num
 * @param db database structure
 * @param disc_id disc id
 * @param track_num track number
 * @return rowid - the id of the track
 */
long get_track_id(sql_db *db, long disc_id, int track_num) {
  if (db==NULL || disc_id < 0 || track_num < 0) {
    return -1;
  }
  sqlite3_stmt *statement = NULL;
  long rowid = -1;
  db->status = sqlite3_prepare_v2(db->database, db_select_track_id, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return rowid;
  }
  // bind the search parameters
  sqlite3_bind_int64(statement, 1, disc_id);
  sqlite3_bind_int(statement, 2, track_num);
  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      rowid = sqlite3_column_int64(statement, 0);
    } else if (db->status == SQLITE_DONE) {
      break;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      return rowid;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);
  return rowid;
}

/**
 * @brief get the number of tracks on the given disc id
 * @param db database structure
 * @param resource_id the resource identifier (disc_id as string)
 * @param track_count number of tracks
 * @return disc_id - the id of the track
 */
long get_track_count(sql_db *db, const char *resource_id, int *track_count) {
  if (db==NULL || resource_id==NULL || strlen(resource_id) == 0) {
    return -1;
  }
  sqlite3_stmt *statement = NULL;
  long disc_id = -1;
  long disc_id_in = 0;
  // convert resource_id to disc_id
  if (sscanf(resource_id, "%ld", &disc_id_in) != 1) {
    return disc_id;
  }
  db->status = sqlite3_prepare_v2(db->database, db_select_track_count, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return disc_id;
  }
  // bind the disc id parameters
  sqlite3_bind_int64(statement, 1, disc_id_in);
  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      disc_id = sqlite3_column_int64(statement, 0);
      *track_count = sqlite3_column_int(statement, 1);
    } else if (db->status == SQLITE_DONE) {
      break;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      return disc_id;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);
  return disc_id;
}

/**
 * @brief get the disc information linked to the given resource id from the database
 * @param db database structure
 * @param limit the limit of the list
 * @param offset the offset to start the list
 * @param disc_info_list output disc information list structure
 * @return 0 if successful; another value indicates an error
 */
int get_disc_list_from_database(sql_db *db, int limit, int offset, disc_list **disc_info_list) {
  if (db==NULL || offset<0 || limit<0) {
    return DB_ERROR;
  }
  // get disc information
  sqlite3_stmt *statement = NULL;
  db->status = sqlite3_prepare_v2(db->database, db_select_disc_list, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return DB_ERROR;
  }
  // bind offset and limit parameters
  sqlite3_bind_int(statement, 1, limit);
  sqlite3_bind_int(statement, 2, offset);
  // execute statement and get results
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      disc *disc_info = (disc *)calloc(1, sizeof(disc));
      disc_info->db_id = (uint64_t)sqlite3_column_int64(statement, 0);
      disc_info->d_id = (unsigned int)sqlite3_column_int64(statement, 1);
      disc_info->d_length = sqlite3_column_int(statement, 2);
      disc_info->d_lookup = (uint64_t)sqlite3_column_int64(statement, 3);
      set_string(&(disc_info->d_artist), (const char*)sqlite3_column_text(statement, 4));
      set_string(&(disc_info->d_title), (const char*)sqlite3_column_text(statement, 5));
      set_string(&(disc_info->d_genre), (const char*)sqlite3_column_text(statement, 6));
      disc_info->d_year = sqlite3_column_int(statement, 7);
      set_string(&(disc_info->d_extended), (const char*)sqlite3_column_text(statement, 8));
      set_string(&(disc_info->cddb_query), (const char*)sqlite3_column_text(statement, 9));
      set_string(&(disc_info->cddb_category), (const char*)sqlite3_column_text(statement, 10));
      disc_info->cddb_e_id = (unsigned int)sqlite3_column_int64(statement, 11);
      disc_info->cddb_d_id = (unsigned int)sqlite3_column_int64(statement, 12);
      disc_info->cddb_revision = sqlite3_column_int(statement, 13);
      disc_info->cddb_complete = sqlite3_column_int(statement, 14);
      set_string(&(disc_info->mb_query), (const char*)sqlite3_column_text(statement, 15));
      set_string(&(disc_info->mb_fuzzy_lookup), (const char*)sqlite3_column_text(statement, 16));
      set_string(&(disc_info->mb_disc_id), (const char*)sqlite3_column_text(statement, 17));
      set_string(&(disc_info->mb_release_id), (const char*)sqlite3_column_text(statement, 18));
      disc_info->mb_front_cover_size = sqlite3_column_bytes(statement, 19);
      disc_info->mb_back_cover_size = sqlite3_column_bytes(statement, 20);
      disc_info->mb_complete = sqlite3_column_int(statement, 21);
      disc_info->d_extracted = sqlite3_column_int(statement, 22);
      disc_info->d_tracks = sqlite3_column_int(statement, 23);
      disc_info->tracks = NULL;
      push_disc_list(disc_info_list, disc_info);
    } else if (db->status == SQLITE_DONE) {
      break;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      return DB_ERROR;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);

  // done. disc and track information is retrieved
  return DB_OK;
}

/**
 * @brief get the disc information linked to the given resource id from the database
 * @param db database structure
 * @param disc_id the disc id in the database for the disc information structure
 * @param include_cover indicator to include the cover data
 * @param disc_info disc information structure
 * @return 0 if successful; another value indicates an error
 */
int get_disc_from_database_by_id(sql_db *db, long disc_id, int include_cover, disc **disc_info) {
  if (db==NULL || disc_id < 0) {
    return DB_ERROR;
  }

  // ensure memory is allocated for the disc information
  if (*disc_info == NULL) {
    *disc_info = (disc *)calloc(1, sizeof(disc));
    (*disc_info)->d_tracks = 0;
    (*disc_info)->tracks = NULL;
  }
  int track_count = 0;

  // get disc information
  sqlite3_stmt *statement = NULL;
  db->status = sqlite3_prepare_v2(db->database, db_select_disc_details_by_disc_id, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return DB_ERROR;
  }
  // bind disc_id parameter
  sqlite3_bind_int64(statement, 1, disc_id);
  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      (*disc_info)->db_id = (uint64_t)sqlite3_column_int64(statement, 0);
      (*disc_info)->d_id = (unsigned int)sqlite3_column_int64(statement, 1);
      (*disc_info)->d_length = sqlite3_column_int(statement, 2);
      (*disc_info)->d_lookup = (uint64_t)sqlite3_column_int64(statement, 3);
      set_string(&((*disc_info)->d_artist), (const char*)sqlite3_column_text(statement, 4));
      set_string(&((*disc_info)->d_title), (const char*)sqlite3_column_text(statement, 5));
      set_string(&((*disc_info)->d_genre), (const char*)sqlite3_column_text(statement, 6));
      (*disc_info)->d_year = sqlite3_column_int(statement, 7);
      set_string(&((*disc_info)->d_extended), (const char*)sqlite3_column_text(statement, 8));
      set_string(&((*disc_info)->cddb_query), (const char*)sqlite3_column_text(statement, 9));
      set_string(&((*disc_info)->cddb_category), (const char*)sqlite3_column_text(statement, 10));
      (*disc_info)->cddb_e_id = (unsigned int)sqlite3_column_int64(statement, 11);
      (*disc_info)->cddb_d_id = (unsigned int)sqlite3_column_int64(statement, 12);
      (*disc_info)->cddb_revision = sqlite3_column_int(statement, 13);
      (*disc_info)->cddb_complete = sqlite3_column_int(statement, 14);
      set_string(&((*disc_info)->mb_query), (const char*)sqlite3_column_text(statement, 15));
      set_string(&((*disc_info)->mb_fuzzy_lookup), (const char*)sqlite3_column_text(statement, 16));
      set_string(&((*disc_info)->mb_disc_id), (const char*)sqlite3_column_text(statement, 17));
      set_string(&((*disc_info)->mb_release_id), (const char*)sqlite3_column_text(statement, 18));
      (*disc_info)->mb_front_cover_size = sqlite3_column_bytes(statement, 19);
      if (include_cover == 1) {
        set_binary(&((*disc_info)->mb_front_cover), sqlite3_column_blob(statement, 19), (*disc_info)->mb_front_cover_size);
      }
      (*disc_info)->mb_back_cover_size = sqlite3_column_bytes(statement, 20);
      if (include_cover == 1) {
        set_binary(&((*disc_info)->mb_back_cover), sqlite3_column_blob(statement, 20), (*disc_info)->mb_back_cover_size);
      }
      (*disc_info)->mb_complete = sqlite3_column_int(statement, 21);
      (*disc_info)->d_extracted = sqlite3_column_int(statement, 22);
      track_count = sqlite3_column_int(statement, 23);
    } else if (db->status == SQLITE_DONE) {
      break;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      return DB_ERROR;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);

  // ensure memory is allocated for the track information
  if ((*disc_info)->tracks == NULL) {
    (*disc_info)->d_tracks = track_count;
    (*disc_info)->tracks = (track *)calloc(track_count, sizeof(track));
  } else if ((*disc_info)->d_tracks != track_count) {
    return DB_ERROR;
  }

  // get track information
  db->status = sqlite3_prepare_v2(db->database, db_select_track_details, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return DB_ERROR;
  }
  // bind disc_id parameter
  sqlite3_bind_int64(statement, 1, disc_id);
  // execute statement and get result
  int t_idx = 0;
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      // disc_id (col:0)
      (*disc_info)->tracks[t_idx].t_num = sqlite3_column_int(statement, 1);
      (*disc_info)->tracks[t_idx].t_length = sqlite3_column_int(statement, 2);
      set_string(&((*disc_info)->tracks[t_idx].t_title), (const char*)sqlite3_column_text(statement, 3));
      set_string(&((*disc_info)->tracks[t_idx].t_artist), (const char*)sqlite3_column_text(statement, 4));
      set_string(&((*disc_info)->tracks[t_idx].t_album), (const char*)sqlite3_column_text(statement, 5));
      set_string(&((*disc_info)->tracks[t_idx].t_genre), (const char*)sqlite3_column_text(statement, 6));
      (*disc_info)->tracks[t_idx].t_year = sqlite3_column_int(statement, 7);
      set_string(&((*disc_info)->tracks[t_idx].t_extended), (const char*)sqlite3_column_text(statement, 8));
      set_string(&((*disc_info)->tracks[t_idx].t_filename), (const char*)sqlite3_column_text(statement, 9));
      (*disc_info)->tracks[t_idx].t_skipped = sqlite3_column_int(statement, 10);
      t_idx++;
    } else if (db->status == SQLITE_DONE) {
      break;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      return DB_ERROR;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);

  // done. disc and track information is retrieved
  return DB_OK;
}

/**
 * @brief get the disc information from the database using the toc information from the disc
 * @param db database structure
 * @param include_cover indicator to include the cover data
 * @param disc_info disc information structure
 * @return 0 if successful; another value indicates an error
 */
int get_disc_from_database_by_toc(sql_db *db, int include_cover, disc *disc_info) {
  if (db==NULL || disc_info==NULL) {
    return DB_ERROR;
  }

  // prepare statement to get cddb information using the toc information from the disc
  sqlite3_stmt *statement = NULL;
  db->status = sqlite3_prepare_v2(db->database, db_select_disc_id_by_toc, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return DB_ERROR;
  }
  
  // bind disc information values
  sqlite3_bind_int64(statement, 1, disc_info->d_lookup);
  sqlite3_bind_int64(statement, 2, disc_info->d_id);
  sqlite3_bind_int(statement, 3, disc_info->d_length);

  long disc_id, prev_disc_id = -1;
  int track_num, track_length, total_length, valid = 1;

  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      disc_id = sqlite3_column_int64(statement, 0);
      if (disc_id != prev_disc_id) {
        valid = 1;
        total_length = CDE_CD_MSF_OFFSET;
        prev_disc_id = disc_id;
      }
      track_num = sqlite3_column_int(statement, 1);
      track_length = sqlite3_column_int(statement, 2);
      // set track information
      if (valid == 1 && track_num > 0 && track_num <= disc_info->d_tracks
        && disc_info->tracks[track_num-1].t_num == track_num
        && disc_info->tracks[track_num-1].t_length == track_length) {
          // set track information
          total_length += track_length;
      } else {
        // error: invalid track number or incorrect track length
        valid = 0;
      }
      if (track_num == disc_info->d_tracks && valid == 1
        && total_length == disc_info->d_length) {
        // disc information completely retrieved and valid
        break;
      }
    } else if (db->status == SQLITE_DONE) {
      // no disc information found or not complete
      sqlite3_finalize(statement);
      return DB_NO_RESULT;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      return DB_ERROR;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);

  // full match with disc found, return the disc information stored in the database
  return get_disc_from_database_by_id(db, disc_id, include_cover, &disc_info);
}

/**
 * @brief get the disc information linked to the given resource id from the database
 * @param db database structure
 * @param resource_id the resource identifier (disc_id) for the disc information structure
 * @param include_cover indicator to include the cover data
 * @param disc_info output disc information structure
 * @return 0 if successful; another value indicates an error
 */
int get_disc_from_database(sql_db *db, const char *resource_id, int include_cover, disc **disc_info) {
  if (db==NULL || resource_id==NULL) {
    return DB_ERROR;
  }

  // check if the disc is already stored and get the nr of tracks
  int track_count = 0;
  long disc_id = get_track_count(db, resource_id, &track_count);
  
  // disc_id must be available to return the disc and track information
  if (disc_id < 0 || track_count <= 0) {
    return DB_ERROR;
  }

  // return the disc information stored in the database
  return get_disc_from_database_by_id(db, disc_id, include_cover, disc_info);
}

/**
 * @brief store the given disc information in the database
 * @param db database structure
 * @param disc_info disc information structure
 * @param update indicator to update the record if it is already stored
 * @return 0 if successful; another value indicates an error
 */
int store_disc_in_database(sql_db *db, disc *disc_info, int update) {
  if (db==NULL || disc_info==NULL) {
    return DB_ERROR;
  }

  // try to get the category
  long category_id = get_category_id(db, disc_info->cddb_category);
  
  // try to get the disc_id to check if the disc is already stored
  long disc_id = get_disc_id(db, disc_info);
  
  // return if the disc is already stored and updating is not requested
  if (disc_id >= 0 && update == 0) {
    disc_info->db_id = disc_id;
    return DB_NO_UPDATE;
  }

  // try to get the artist_id and insert if not available
  long artist_id = get_artist_id(db, disc_info->d_artist, 1);
  
  // try to get the album_id and insert if not available
  long album_id = get_album_id(db, disc_info->d_title, 1);

  // try to insert the genre_id and insert if not available
  long genre_id = get_genre_id(db, disc_info->d_genre, 1);

  // check inserted/retrieved identifiers
  if (category_id < 0 || artist_id < 0 || album_id < 0 || genre_id < 0) {
    return DB_ERROR;
  }

  // insert or update disc information returning the disc_id
  db->status = sqlite3_exec(db->database, DB_BEGIN_TRANSACTION, NULL, NULL, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    return DB_ERROR;
  }
  sqlite3_stmt *statement = NULL;
  if (disc_id < 0) {
    // disc_id not known: insert the covers and disc information

    // try to insert the front cover returning the cover id
    long front_cover_id = get_cover_id(db, 0, disc_info->mb_front_cover, disc_info->mb_front_cover_size, 1);

    // try to insert the back cover returning the cover id
    long back_cover_id = get_cover_id(db, 1, disc_info->mb_back_cover, disc_info->mb_back_cover_size, 1);

    // check inserted/retrieved cover identifiers
    if (front_cover_id < 0 || back_cover_id < 0) {
      return DB_ERROR;
    }

    // insert the disc information
    db->status = sqlite3_prepare_v2(db->database, db_insert_disc, -1, &statement, NULL);
    if (db->status != DB_OK) {
      set_error_message(db);
      sqlite3_finalize(statement);
      return DB_ERROR;
    }
    // bind disc information values
    sqlite3_bind_int64(statement, 1, disc_info->d_id);
    sqlite3_bind_int(statement, 2, disc_info->d_length);
    sqlite3_bind_int64(statement, 3, disc_info->d_lookup);
    sqlite3_bind_int64(statement, 4, artist_id);
    sqlite3_bind_int64(statement, 5, album_id);
    sqlite3_bind_int64(statement, 6, genre_id);
    sqlite3_bind_int(statement, 7, disc_info->d_year);
    sqlite3_bind_text(statement, 8, disc_info->d_extended, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 9, disc_info->cddb_query, -1, SQLITE_STATIC);
    sqlite3_bind_int64(statement, 10, category_id);
    sqlite3_bind_int64(statement, 11, disc_info->cddb_e_id);
    sqlite3_bind_int64(statement, 12, disc_info->cddb_d_id);
    sqlite3_bind_int(statement, 13, disc_info->cddb_revision);
    sqlite3_bind_int(statement, 14, disc_info->cddb_complete);
    sqlite3_bind_text(statement, 15, disc_info->mb_query, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 16, disc_info->mb_fuzzy_lookup, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 17, disc_info->mb_disc_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 18, disc_info->mb_release_id, -1, SQLITE_STATIC);
    sqlite3_bind_int64(statement, 19, front_cover_id);
    sqlite3_bind_int64(statement, 20, back_cover_id);
    sqlite3_bind_int(statement, 21, disc_info->mb_complete);
    sqlite3_bind_int(statement, 22, disc_info->d_extracted);
    sqlite3_bind_int(statement, 23, disc_info->d_tracks);
    // execute statement and get result
    while (1) {
      db->status = sqlite3_step(statement);
      if (db->status == SQLITE_ROW) {
        disc_id = sqlite3_column_int64(statement, 0);
      } else if (db->status == SQLITE_DONE) {
        break;
      } else {
        set_error_message(db);
        sqlite3_finalize(statement);
        return DB_ERROR;
      }
    }
    // delete statement
    db->status = sqlite3_finalize(statement);
  } else if (update == 1) {
    // disc_id known and update requested: update the disc information allowed to update
    db->status = sqlite3_prepare_v2(db->database, db_update_disc, -1, &statement, NULL);
    if (db->status != DB_OK) {
      set_error_message(db);
      sqlite3_finalize(statement);
      return DB_ERROR;
    }
    // bind disc information values
    sqlite3_bind_int64(statement, 1, artist_id);
    sqlite3_bind_int64(statement, 2, album_id);
    sqlite3_bind_int64(statement, 3, genre_id);
    sqlite3_bind_int(statement, 4, disc_info->d_year);
    sqlite3_bind_text(statement, 5, disc_info->d_extended, -1, SQLITE_STATIC);
    sqlite3_bind_int(statement, 6, disc_info->cddb_revision);
    sqlite3_bind_int(statement, 7, disc_info->cddb_complete);
    sqlite3_bind_int(statement, 8, disc_info->mb_complete);
    sqlite3_bind_int(statement, 9, disc_info->d_extracted);
    sqlite3_bind_int64(statement, 10, disc_id);
    // execute statement
    db->status = sqlite3_step(statement);
    if (db->status != SQLITE_DONE) {
      set_error_message(db);
      sqlite3_finalize(statement);
      return DB_ERROR;
    }
    // delete statement
    db->status = sqlite3_finalize(statement);
  }

  // disc_id must be available to insert or update track information
  if (disc_id < 0) {
    sqlite3_exec(db->database, DB_END_TRANSACTION, NULL, NULL, NULL);
    return DB_ERROR;
  }

  // insert or update track information
  db->status = sqlite3_prepare_v2(db->database, db_insert_track, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return DB_ERROR;
  }
  sqlite3_stmt *statement2 = NULL;
  db->status = sqlite3_prepare_v2(db->database, db_update_track, -1, &statement2, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    sqlite3_finalize(statement2);
    return DB_ERROR;
  }
  long track_id = -1;
  for (int i=0; i<disc_info->d_tracks; i++) {
    track_id = get_track_id(db, disc_id, disc_info->tracks[i].t_num);
    if (track_id < 0) {
      // insert track information
      artist_id = get_artist_id(db, disc_info->tracks[i].t_artist, 1);
      album_id = get_album_id(db, disc_info->tracks[i].t_album, 1);
      genre_id = get_genre_id(db, disc_info->tracks[i].t_genre, 1);
      // bind track information values
      sqlite3_bind_int64(statement, 1, disc_id);
      sqlite3_bind_int(statement, 2, disc_info->tracks[i].t_num);
      sqlite3_bind_int(statement, 3, disc_info->tracks[i].t_length);
      sqlite3_bind_text(statement, 4, disc_info->tracks[i].t_title, -1, SQLITE_STATIC);
      sqlite3_bind_int64(statement, 5, artist_id);
      sqlite3_bind_int64(statement, 6, album_id);
      sqlite3_bind_int64(statement, 7, genre_id);
      sqlite3_bind_int(statement, 8, disc_info->tracks[i].t_year);
      sqlite3_bind_text(statement, 9, disc_info->tracks[i].t_extended, -1, SQLITE_STATIC);
      sqlite3_bind_text(statement, 10, disc_info->tracks[i].t_filename, -1, SQLITE_STATIC);
      sqlite3_bind_int(statement, 11, disc_info->tracks[i].t_skipped);
      // execute statement
      db->status = sqlite3_step(statement);
      if (db->status != SQLITE_DONE) {
        set_error_message(db);
        sqlite3_finalize(statement);
        sqlite3_finalize(statement2);
        return DB_ERROR;
      }
      // reset statement so we can bind the values for the next track
      db->status = sqlite3_reset(statement);
    } else if (update == 1) {
      // update track information
      artist_id = get_artist_id(db, disc_info->tracks[i].t_artist, 1);
      album_id = get_album_id(db, disc_info->tracks[i].t_album, 1);
      genre_id = get_genre_id(db, disc_info->tracks[i].t_genre, 1);
      // bind track information values
      sqlite3_bind_text(statement2, 1, disc_info->tracks[i].t_title, -1, SQLITE_STATIC);
      sqlite3_bind_int64(statement2, 2, artist_id);
      sqlite3_bind_int64(statement2, 3, album_id);
      sqlite3_bind_int64(statement2, 4, genre_id);
      sqlite3_bind_int(statement2, 5, disc_info->tracks[i].t_year);
      sqlite3_bind_text(statement2, 6, disc_info->tracks[i].t_extended, -1, SQLITE_STATIC);
      sqlite3_bind_text(statement2, 7, disc_info->tracks[i].t_filename, -1, SQLITE_STATIC);
      sqlite3_bind_int(statement2, 8, disc_info->tracks[i].t_skipped);
      sqlite3_bind_int64(statement2, 9, disc_id);
      sqlite3_bind_int(statement2, 10, disc_info->tracks[i].t_num);
      // execute statement
      db->status = sqlite3_step(statement2);
      if (db->status != SQLITE_DONE) {
        set_error_message(db);
        sqlite3_finalize(statement);
        sqlite3_finalize(statement2);
        return DB_ERROR;
      }
      // reset statement so we can bind the values for the next track
      db->status = sqlite3_reset(statement2);
    }
  }
  // delete statements
  db->status = sqlite3_finalize(statement);
  db->status = sqlite3_finalize(statement2);
  db->status = sqlite3_exec(db->database, DB_END_TRANSACTION, NULL, NULL, NULL);

  // done. disc information is inserted or disc was already present and updated
  disc_info->db_id = disc_id;
  return DB_OK;
}

/**
 * @brief purge all disc entries from the database
 * @param db database structure
 * @return 0 if successful; another value indicates an error
 */
int purge_discs_from_database(sql_db *db) {
  if (db==NULL) {
    return DB_ERROR;
  }
  // delete all disc entries and associated track, artist, album and cover entries
  int res = DB_OK;
  const char **db_purge_tables_ptr = db_purge_tables;
  while (*db_purge_tables_ptr != NULL && res == DB_OK) {
    res = execute_sql(db, *db_purge_tables_ptr);
    db_purge_tables_ptr++;
  }
  return res;
}

/**
 * @brief get the cddb database id using the lookup id in the cddb table
 * @param db database structure
 * @param lookup_id
 * @return cddb_id - a value <0 indicates an error
 */
long get_cddb_id_by_lookup_cddb(sql_db *db, uint64_t lookup_id) {
  if (db == NULL || lookup_id < 0) {
    return -1;
  }
  sqlite3_stmt *statement = NULL;
  long cddb_id = -1;
  db->status = sqlite3_prepare_v2(db->database, db_select_cddb_id_by_lookup_cddb, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return cddb_id;
  }
  // bind the lookup id parameter
  sqlite3_bind_int64(statement, 1, lookup_id);
  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      cddb_id = sqlite3_column_int64(statement, 0);
    } else if (db->status == SQLITE_DONE) {
      break;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      return cddb_id;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);
  return cddb_id;
}


/**
 * @brief get the cddb database id using the lookup id in the temporary cddb_temp table
 *        note: only used as part of rebuilding the database
 * @param db database structure
 * @param lookup_id
 * @return cddb_id - a value <0 indicates an error
 */
long get_cddb_id_by_lookup_temp(sql_db *db, uint64_t lookup_id) {
  if (db == NULL || lookup_id < 0) {
    return -1;
  }
  sqlite3_stmt *statement = NULL;
  long cddb_id = -1;
  db->status = sqlite3_prepare_v2(db->database, db_select_cddb_id_by_lookup_temp, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return cddb_id;
  }
  // bind the lookup id parameter
  sqlite3_bind_int64(statement, 1, lookup_id);
  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      cddb_id = sqlite3_column_int64(statement, 0);
    } else if (db->status == SQLITE_DONE) {
      break;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      return cddb_id;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);
  return cddb_id;
}

/**
 * @brief get the id of the track (rowid) 
 *        identified by the cddb disc id and the track_num
 * @param db database structure
 * @param cddb_id disc id
 * @param track_num track number
 * @return rowid - the id of the track
 */
long get_cddb_track_id(sql_db *db, long cddb_id, int track_num) {
  if (db==NULL || cddb_id < 0 || track_num < 0) {
    return -1;
  }
  sqlite3_stmt *statement = NULL;
  long rowid = -1;
  db->status = sqlite3_prepare_v2(db->database, db_select_cddb_track_id, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return rowid;
  }
  // bind the search parameters
  sqlite3_bind_int64(statement, 1, cddb_id);
  sqlite3_bind_int(statement, 2, track_num);
  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      rowid = sqlite3_column_int64(statement, 0);
    } else if (db->status == SQLITE_DONE) {
      break;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      return rowid;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);
  return rowid;
}

/**
 * @brief get the cddb genre id
 * @param db database structure
 * @param genre_name the genre name
 * @param insert indicator to insert record if the genre is not available
 * @return album_id - a value <0 indicates an error
 */
long get_cddb_genre_id(sql_db *db, const char *genre_name, int insert) {
  if (genre_name == NULL || strlen(genre_name) == 0) {
    return 0;
  }
  sqlite3_stmt *statement = NULL;
  long genre_id = -1;

  // try to get the genre
  db->status = sqlite3_prepare_v2(db->database, db_select_cddb_genre_id, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return genre_id;
  }
  // bind genre name
  sqlite3_bind_text(statement, 1, genre_name, -1, SQLITE_STATIC);
  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      genre_id = sqlite3_column_int64(statement, 0);
      db->status = sqlite3_finalize(statement);
      return genre_id;
    } else if (db->status == SQLITE_DONE) {
      break;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      return genre_id;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);

  // try to insert the genre
  if (genre_id < 0 && insert == 1) {
    db->status = sqlite3_prepare_v2(db->database, db_insert_cddb_genre, -1, &statement, NULL);
    if (db->status != DB_OK) {
      set_error_message(db);
      sqlite3_finalize(statement);
      return genre_id;
    }
    // bind genre name
    sqlite3_bind_text(statement, 1, genre_name, -1, SQLITE_STATIC);
    // execute statement and get result
    while (1) {
      db->status = sqlite3_step(statement);
      if (db->status == SQLITE_ROW) {
        genre_id = sqlite3_column_int64(statement, 0);
      } else if (db->status == SQLITE_DONE) {
        break;
      } else {
        set_error_message(db);
        sqlite3_finalize(statement);
        return genre_id;
      }
    }
    // delete statement
    db->status = sqlite3_finalize(statement);
  }

  return genre_id;
}

/**
 * @brief get the cddb entry from the database using the toc information from the disc
 * @param db database structure
 * @param disc_info input disc information structure
 * @param cddb_id the internal cddb identifier if a matching cddb entry is found
 * @param cddb_info output disc information structure
 * @return 0 if successful; another value indicates an error
 */
int get_cddb_entry_from_database_by_toc(sql_db *db, disc *disc_info, long *cddb_id, disc **cddb_info) {
  if (db==NULL || disc_info==NULL || *cddb_info!=NULL) {
    return DB_ERROR;
  }

  // prepare statement to get cddb information using the toc information from the disc
  sqlite3_stmt *statement = NULL;
  db->status = sqlite3_prepare_v2(db->database, db_select_cddb_by_toc, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return DB_ERROR;
  }

  // bind disc lookup hash
  sqlite3_bind_int64(statement, 1, disc_info->d_lookup);

  long int_cddb_id, prev_cddb_id = -1;
  int track_cnt, track_num, track_length, total_length, valid = 1;

  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      int_cddb_id = sqlite3_column_int64(statement, 0);
      if (int_cddb_id != prev_cddb_id) {
        valid = 1;
        total_length = CDE_CD_MSF_OFFSET;
        track_cnt = sqlite3_column_int(statement, 11);
        // allocate memory for cddb_info to return
        if (*cddb_info == NULL) {
          *cddb_info = cde_alloc_disc(track_cnt);
          if (*cddb_info == NULL) {
            sqlite3_finalize(statement);
            return DB_ERROR;
          }
        }
        // set disc information
        (*cddb_info)->d_lookup = sqlite3_column_int64(statement, 1);
        set_string(&((*cddb_info)->cddb_category), (const char*)sqlite3_column_text(statement, 2));
        (*cddb_info)->cddb_e_id = sqlite3_column_int64(statement, 3);
        (*cddb_info)->cddb_d_id = sqlite3_column_int64(statement, 4);
        set_string(&((*cddb_info)->d_artist), (const char*)sqlite3_column_text(statement, 5));
        set_string(&((*cddb_info)->d_title), (const char*)sqlite3_column_text(statement, 6));
        set_string(&((*cddb_info)->d_genre), (const char*)sqlite3_column_text(statement, 7));
        (*cddb_info)->d_year = sqlite3_column_int(statement, 8);
        (*cddb_info)->d_length = sqlite3_column_int(statement, 9) * CDE_CD_FRAMES;
        (*cddb_info)->cddb_revision = sqlite3_column_int(statement, 10);
        (*cddb_info)->d_tracks = track_cnt;
        prev_cddb_id = int_cddb_id;
      }
      track_num = sqlite3_column_int(statement, 12);
      track_length = sqlite3_column_int(statement, 14);
      // check if track information is valid
      if (valid == 1 && track_num > 0 && track_num <= disc_info->d_tracks &&
        disc_info->tracks[track_num-1].t_num == track_num && (disc_info->tracks[track_num-1].t_length == 0 || 
          abs(disc_info->tracks[track_num-1].t_length - track_length) <= CDE_CD_MSF_OFFSET)) {
          // set track information
          (*cddb_info)->tracks[track_num-1].t_num = track_num;
          (*cddb_info)->tracks[track_num-1].t_length = track_length;
          set_string(&((*cddb_info)->tracks[track_num-1].t_title), (const char*)sqlite3_column_text(statement, 13));
          if (strlen((*cddb_info)->tracks[track_num-1].t_artist) == 0) {
            set_string(&((*cddb_info)->tracks[track_num-1].t_artist), (*cddb_info)->d_artist);
          }
          if (strlen((*cddb_info)->tracks[track_num-1].t_album) == 0) {
            set_string(&((*cddb_info)->tracks[track_num-1].t_album), (*cddb_info)->d_title);
          }          
          if (strlen((*cddb_info)->tracks[track_num-1].t_genre) == 0) {
            set_string(&((*cddb_info)->tracks[track_num-1].t_genre), (*cddb_info)->d_genre);
          }          
          total_length += track_length;
      } else {
        // error: invalid track number or track length
        valid = 0;
      }
      if (track_num == disc_info->d_tracks && valid == 1) {
        // cddb information completely retrieved
        (*cddb_info)->d_length = total_length;
        (*cddb_info)->d_id = (*cddb_info)->cddb_d_id;
        (*cddb_info)->cddb_complete = 1;
        break;
      }
    } else if (db->status == SQLITE_DONE) {
      // no cddb information found or not complete
      sqlite3_finalize(statement);
      cde_free_disc(cddb_info, -1);
      return DB_NO_RESULT;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      cde_free_disc(cddb_info, -1);
      return DB_ERROR;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);

  // set the internal cddb identifier
  *cddb_id = int_cddb_id;

  // done. cddb disc and track information retrieved
  return DB_OK;
}

/**
 * @brief get the cddb entry from the database using the toc information from the disc
 * @param db database structure
 * @param cddb_id the internal cddb identifier
 * @param cddb_info output disc information structure
 * @return 0 if successful; another value indicates an error
 */
int get_cddb_entry_from_database(sql_db *db, long cddb_id, disc **cddb_info) {
  if (db==NULL || cddb_id < 0 || *cddb_info!=NULL) {
    return DB_ERROR;
  }

  // prepare statement to get cddb information using the toc information from the disc
  sqlite3_stmt *statement = NULL;
  db->status = sqlite3_prepare_v2(db->database, db_select_cddb_by_cddb_id, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return DB_ERROR;
  }

  // bind disc information values
  sqlite3_bind_int64(statement, 1, cddb_id);

  int track_cnt, track_num, track_length, total_length;
  long db_cddb_id = 0;

  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      if (db_cddb_id == 0) {
        db_cddb_id = sqlite3_column_int64(statement, 0);
        total_length = CDE_CD_MSF_OFFSET;
        track_cnt = sqlite3_column_int(statement, 11);
        // allocate memory for cddb_info to return
        *cddb_info = cde_alloc_disc(track_cnt);
        if (*cddb_info == NULL) {
          sqlite3_finalize(statement);
          return DB_ERROR;
        }
        // set disc information
        (*cddb_info)->d_lookup = sqlite3_column_int64(statement, 1);
        set_string(&((*cddb_info)->cddb_category), (const char*)sqlite3_column_text(statement, 2));
        (*cddb_info)->cddb_e_id = sqlite3_column_int64(statement, 3);
        (*cddb_info)->cddb_d_id = sqlite3_column_int64(statement, 4);
        set_string(&((*cddb_info)->d_artist), (const char*)sqlite3_column_text(statement, 5));
        set_string(&((*cddb_info)->d_title), (const char*)sqlite3_column_text(statement, 6));
        set_string(&((*cddb_info)->d_genre), (const char*)sqlite3_column_text(statement, 7));
        (*cddb_info)->d_year = sqlite3_column_int(statement, 8);
        (*cddb_info)->d_length = sqlite3_column_int(statement, 9) * CDE_CD_FRAMES;
        (*cddb_info)->cddb_revision = sqlite3_column_int(statement, 10);
        (*cddb_info)->d_tracks = track_cnt;
      }
      track_num = sqlite3_column_int(statement, 12);
      track_length = sqlite3_column_int(statement, 14);
      // set track information
      if (track_num > 0 && track_num <= track_cnt) {
          // set track information
          (*cddb_info)->tracks[track_num-1].t_num = track_num;
          set_string(&((*cddb_info)->tracks[track_num-1].t_title), (const char*)sqlite3_column_text(statement, 13));
          (*cddb_info)->tracks[track_num-1].t_length = track_length;
          total_length += track_length;
      }
      if (track_num == track_cnt) {
        // cddb information completely retrieved
        (*cddb_info)->d_length = total_length;
        (*cddb_info)->cddb_complete = 1;
        break;
      }
    } else if (db->status == SQLITE_DONE) {
      // no cddb information found or not complete
      sqlite3_finalize(statement);
      cde_free_disc(cddb_info, -1);
      return DB_NO_RESULT;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      cde_free_disc(cddb_info, -1);
      return DB_ERROR;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);

  // done. cddb disc and track information is retrieved
  return DB_OK;
}

/**
 * @brief search for a cddb entry in the database using the provided disc information
 * @param db database structure
 * @param disc_info input disc information structure
 * @param cddb_id the internal cddb identifier if a matching cddb entry is found
 * @param cddb_info output disc information structure
 * @return 0 if successful; another value indicates an error
 */
int search_cddb_entry_in_database(sql_db *db, disc *disc_info, long *cddb_id, disc **cddb_info) {
  if (db==NULL || disc_info==NULL || *cddb_info!=NULL) {
    return DB_ERROR;
  }

  // prepare the search parameters depending on the available disc information
  char *where_lookup;
  char *where_specifier;
  if (disc_info->d_lookup > 0) {
    where_lookup = calloc(4, sizeof(char));
    if (where_lookup == NULL) {
      return DB_ERROR;
    }
    sprintf(where_lookup, "%s", db_search_cddb_lookup_single);

    // use the single lookup specifier only
    where_specifier = calloc(1, sizeof(char));
    if (where_specifier == NULL) {
      free(where_lookup);
      return DB_ERROR;
    }
  } else {
    where_lookup = calloc(111, sizeof(char));
    if (where_lookup == NULL) {
      return DB_ERROR;
    }
    sprintf(where_lookup, "%s", db_search_cddb_lookup_between);

    // extend the where clause with the artist and title
    where_specifier = calloc(45, sizeof(char));
    if (where_specifier == NULL) {
      free(where_lookup);
      return DB_ERROR;
    }
    sprintf(where_specifier, "%s", db_search_cddb_lookup_artist_title);
  }

  // prepare the complete search query string
  char *search_query = calloc(strlen(db_search_cddb) + strlen(where_lookup) + strlen(where_specifier) + 1, sizeof(char));
  if (search_query == NULL) {
    free(where_lookup);
    free(where_specifier);
    return DB_ERROR;
  }
  sprintf(search_query, db_search_cddb, where_lookup, where_specifier);
  free(where_lookup);
  free(where_specifier);

  // prepare statement to search for cddb information using the provided disc information
  sqlite3_stmt *statement = NULL;
  db->status = sqlite3_prepare_v2(db->database, search_query, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    free(search_query);
    return DB_ERROR;
  }
  // free the search query string as it is no longer needed
  free(search_query);

  // prepare like disc artist lookup using SQL LIKE
  char *like_d_artist_ext = calloc(strlen(disc_info->d_artist) + 3, sizeof(char));
  if (like_d_artist_ext == NULL) {
    sqlite3_finalize(statement);
    return DB_ERROR;
  }
  sprintf(like_d_artist_ext, "%%%s%%", disc_info->d_artist);

  // replace 'special characters' for SQL LIKE
  char *like_d_artist = replace_chars(like_d_artist_ext, LIKE_REPLACE_CHARS, '%');
  free(like_d_artist_ext);
  if (like_d_artist == NULL) {
    sqlite3_finalize(statement);
    return DB_ERROR;
  }

  // prepare disc title lookup using SQL LIKE
  char *like_d_title_ext = calloc(strlen(disc_info->d_title) + 3, sizeof(char));
  if (like_d_title_ext == NULL) {
    free(like_d_artist);
    sqlite3_finalize(statement);
    return DB_ERROR;
  }
  sprintf(like_d_title_ext, "%%%s%%", disc_info->d_title);

  // replace 'special characters' for SQL LIKE
  char *like_d_title = replace_chars(like_d_title_ext, LIKE_REPLACE_CHARS, '%');
  free(like_d_title_ext);
  if (like_d_title == NULL) {
    free(like_d_artist);
    sqlite3_finalize(statement);
    return DB_ERROR;
  }

  // bind search parameters
  sqlite3_bind_int64(statement, 1, disc_info->d_lookup);                    // bind the disc lookup hash
  sqlite3_bind_int(statement, 2, disc_info->d_tracks);                      // bind number of tracks
  sqlite3_bind_text(statement, 3, disc_info->d_artist, -1, SQLITE_STATIC);  // bind artist name
  sqlite3_bind_text(statement, 4, disc_info->d_title, -1, SQLITE_STATIC);   // bind disc title
  sqlite3_bind_text(statement, 5, like_d_artist, -1, SQLITE_STATIC);        // bind like artist title
  sqlite3_bind_text(statement, 6, like_d_title, -1, SQLITE_STATIC);         // bind like disc title
  sqlite3_bind_int(statement, 7, disc_info->d_length / 75);                 // bind disc length in seconds
  sqlite3_bind_int(statement, 8, disc_info->d_year);                        // bind disc release year
  sqlite3_bind_text(statement, 9, disc_info->d_genre, -1, SQLITE_STATIC);   // bind disc genre

  long int_cddb_id, prev_cddb_id = -1;
  long weight, prev_weight = -1;
  int track_cnt, track_num, track_length, total_length, valid = 1;

  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      int_cddb_id = sqlite3_column_int64(statement, 0);
      weight = sqlite3_column_int64(statement, 15);
      if (int_cddb_id != prev_cddb_id || weight != prev_weight) {
        valid = 1;
        total_length = CDE_CD_MSF_OFFSET;
        track_cnt = sqlite3_column_int(statement, 11);
        // allocate memory for cddb_info to return
        if (*cddb_info == NULL) {
          *cddb_info = cde_alloc_disc(track_cnt);
          if (*cddb_info == NULL) {
            sqlite3_finalize(statement);
            free(like_d_artist);
            free(like_d_title);
            return DB_ERROR;
          }
        }
        // set disc information
        (*cddb_info)->d_lookup = sqlite3_column_int64(statement, 1);
        set_string(&((*cddb_info)->cddb_category), (const char*)sqlite3_column_text(statement, 2));
        (*cddb_info)->cddb_e_id = sqlite3_column_int64(statement, 3);
        (*cddb_info)->cddb_d_id = sqlite3_column_int64(statement, 4);
        set_string(&((*cddb_info)->d_artist), (const char*)sqlite3_column_text(statement, 5));
        set_string(&((*cddb_info)->d_title), (const char*)sqlite3_column_text(statement, 6));
        set_string(&((*cddb_info)->d_genre), (const char*)sqlite3_column_text(statement, 7));
        (*cddb_info)->d_year = sqlite3_column_int(statement, 8);
        (*cddb_info)->d_length = sqlite3_column_int(statement, 9) * CDE_CD_FRAMES;
        (*cddb_info)->cddb_revision = sqlite3_column_int(statement, 10);
        (*cddb_info)->d_tracks = track_cnt;
        prev_cddb_id = int_cddb_id;
        prev_weight = weight;
      }
      track_num = sqlite3_column_int(statement, 12);
      track_length = sqlite3_column_int(statement, 14);

      // check if track information is valid
      if (valid == 1) { 
        if (track_num > 0 && track_num <= disc_info->d_tracks &&
            disc_info->tracks[track_num-1].t_num == track_num && (disc_info->tracks[track_num-1].t_length == 0 || 
            abs(disc_info->tracks[track_num-1].t_length - track_length) <= MAX_LENGTH_DEVIATION * CDE_CD_FRAMES)) {
          // set track information
          (*cddb_info)->tracks[track_num-1].t_num = track_num;
          (*cddb_info)->tracks[track_num-1].t_length = track_length;
          set_string(&((*cddb_info)->tracks[track_num-1].t_title), (const char*)sqlite3_column_text(statement, 13));
          if (strlen((*cddb_info)->tracks[track_num-1].t_artist) == 0) {
            set_string(&((*cddb_info)->tracks[track_num-1].t_artist), (*cddb_info)->d_artist);
          }
          if (strlen((*cddb_info)->tracks[track_num-1].t_album) == 0) {
            set_string(&((*cddb_info)->tracks[track_num-1].t_album), (*cddb_info)->d_title);
          }          
          if (strlen((*cddb_info)->tracks[track_num-1].t_genre) == 0) {
            set_string(&((*cddb_info)->tracks[track_num-1].t_genre), (*cddb_info)->d_genre);
          }          
          total_length += track_length;
        } else if (valid == 1) {
          // error: invalid track number or track length
          valid = 0;
        }
      }
      if (track_num == disc_info->d_tracks && valid == 1) {
        // cddb information completely retrieved
        (*cddb_info)->d_length = total_length;
        (*cddb_info)->d_id = (*cddb_info)->cddb_d_id;
        (*cddb_info)->cddb_complete = 1;
        break;
      }
    } else if (db->status == SQLITE_DONE) {
      // no cddb information found or not complete
      sqlite3_finalize(statement);
      free(like_d_artist);
      free(like_d_title);
      cde_free_disc(cddb_info, -1);
      return DB_NO_RESULT;
    } else {
      // error occurred (no SQLITE_ROW or SQLITE_DONE)
      set_error_message(db);
      sqlite3_finalize(statement);
      free(like_d_artist);
      free(like_d_title);
      cde_free_disc(cddb_info, -1);
      return DB_ERROR;
    }
  }

  // delete statement
  db->status = sqlite3_finalize(statement);

  // free the like parameters
  free(like_d_artist);
  free(like_d_title);

  // set the internal cddb identifier
  *cddb_id = int_cddb_id;

  // done. cddb disc and track information retrieved
  return DB_OK;
}

/**
 * @brief update the cddb information identified by cddb_id in the database
 * @param db database structure
 * @param disc_info disc information structure containing the cddb information
 * @param cddb_id the internal cddb identifier
 * @param category_id the cddb file category id
 * @return 0 if successful; another value indicates an error
 */
int update_cddb_entry_in_database(sql_db *db, disc *disc_info, long cddb_id, long category_id) {
  if (db==NULL || disc_info==NULL || cddb_id < 0 || category_id < 0) {
    return DB_ERROR;
  }

  // try to insert the genre_id and insert if not available
  long c_genre_id = get_cddb_genre_id(db, disc_info->d_genre, 1);
  if (c_genre_id < 0) {
    // failed to get or insert genre_id
    return DB_ERROR;
  }

  // cddb_id known and update requested: update the cddb information allowed to update
  sqlite3_stmt *statement = NULL;
  db->status = sqlite3_exec(db->database, DB_BEGIN_TRANSACTION, NULL, NULL, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    return DB_ERROR;
  }
  db->status = sqlite3_prepare_v2(db->database, db_update_cddb, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return DB_ERROR;
  }
  // bind disc information values
  sqlite3_bind_int64(statement, 1, category_id);                            // file category
  sqlite3_bind_int64(statement, 2, disc_info->cddb_e_id);                   // cddb entry id
  sqlite3_bind_int64(statement, 3, disc_info->cddb_d_id);                   // cddb disc id
  sqlite3_bind_text(statement, 4, disc_info->d_artist, -1, SQLITE_STATIC);  // bind artist name
  sqlite3_bind_text(statement, 5, disc_info->d_title, -1, SQLITE_STATIC);   // bind disc title
  sqlite3_bind_int64(statement, 6, c_genre_id);                             // bind cddb genre id
  sqlite3_bind_int(statement, 7, disc_info->d_year);                        // bind disc year
  sqlite3_bind_int(statement, 8, disc_info->d_length / CDE_CD_FRAMES);      // bind disc length in seconds
  sqlite3_bind_int(statement, 9, disc_info->cddb_revision);                 // bind cddb revision
  sqlite3_bind_int(statement, 10, disc_info->d_tracks);                     // bind number of tracks
  sqlite3_bind_int64(statement, 11, cddb_id);                               // bind the cddb_id for the update
  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      cddb_id = sqlite3_column_int64(statement, 0);
    } else if (db->status == SQLITE_DONE) {
      break;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      return DB_ERROR;
    }
  }
  // delete statement
  db->status = sqlite3_finalize(statement);
  // update track information
  db->status = sqlite3_prepare_v2(db->database, db_update_cddb_track, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return DB_ERROR;
  }
  long track_id = -1;
  for (int i=0; i<disc_info->d_tracks; i++) {
    track_id = get_cddb_track_id(db, cddb_id, disc_info->tracks[i].t_num);
    if (track_id >= 0) {
      // bind track information values
      sqlite3_bind_text(statement, 1, disc_info->tracks[i].t_title, -1, SQLITE_STATIC);
      sqlite3_bind_int(statement, 2, disc_info->tracks[i].t_length);
      sqlite3_bind_int64(statement, 3, cddb_id);
      sqlite3_bind_int(statement, 4, disc_info->tracks[i].t_num);
      // execute statement
      db->status = sqlite3_step(statement);
      if (db->status != SQLITE_DONE) {
        set_error_message(db);
        sqlite3_finalize(statement);
        return DB_ERROR;
      }
      // reset statement so we can bind the values for the next track
      db->status = sqlite3_reset(statement);
    }
  }
  // delete statements
  db->status = sqlite3_finalize(statement);
  db->status = sqlite3_exec(db->database, DB_END_TRANSACTION, NULL, NULL, NULL);

  // done. disc information is updated
  return DB_OK;
}

/**
 * @brief insert the given cddb information in the database
 * @param db database structure
 * @param disc_info disc information structure containing the cddb information
 * @param category_id the cddb file category id
 * @param lookup_method method to determine how to lookup the cddb id (0=by category and file, 1=by lookup in cddb, 2=by lookup in cddb_lookup temp table)
 * @return 0 if successful; another value indicates an error
 */
int insert_cddb_entry_in_database(sql_db *db, disc *disc_info, long category_id, int lookup_method) {

  // try to insert the genre_id and insert if not available
  long c_genre_id = get_cddb_genre_id(db, disc_info->d_genre, 1);
  if (c_genre_id < 0) {
    // failed to get or insert genre_id
    return DB_ERROR;
  }

  // insert disc information returning the disc_id
  long cddb_id = -1;
  sqlite3_stmt *statement = NULL;
  db->status = sqlite3_exec(db->database, DB_BEGIN_TRANSACTION, NULL, NULL, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    return DB_ERROR;
  }
  db->status = sqlite3_prepare_v2(db->database, db_insert_cddb, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return DB_ERROR;
  }
  // bind disc information values
  sqlite3_bind_int64(statement, 1, disc_info->d_lookup);                    // internal hash value to enable fast lookups
  sqlite3_bind_int64(statement, 2, category_id);                            // bind file category
  sqlite3_bind_int64(statement, 3, disc_info->cddb_e_id);                   // bind cddb entry id
  sqlite3_bind_int64(statement, 4, disc_info->cddb_d_id);                   // bind cddb disc id
  sqlite3_bind_text(statement, 5, disc_info->d_artist, -1, SQLITE_STATIC);  // bind artist name
  sqlite3_bind_text(statement, 6, disc_info->d_title, -1, SQLITE_STATIC);   // bind disc title
  sqlite3_bind_int64(statement, 7, c_genre_id);                             // bind cddb genre id
  sqlite3_bind_int(statement, 8, disc_info->d_year);                        // bind disc year
  sqlite3_bind_int(statement, 9, disc_info->d_length / CDE_CD_FRAMES);      // bind disc length in seconds
  sqlite3_bind_int(statement, 10, disc_info->cddb_revision);                // bind cddb revision
  sqlite3_bind_int(statement, 11, disc_info->d_tracks);                     // bind number of tracks
  // execute statement and get result
  while (1) {
    db->status = sqlite3_step(statement);
    if (db->status == SQLITE_ROW) {
      cddb_id = sqlite3_column_int64(statement, 0);
    } else if (db->status == SQLITE_DONE) {
      break;
    } else {
      set_error_message(db);
      sqlite3_finalize(statement);
      return DB_ERROR;
    }
  }
  // delete cddb insert statement
  db->status = sqlite3_finalize(statement);
  // cddb_id must be available to insert the track information
  if (cddb_id < 0) {
    return DB_ERROR;
  }
  // insert the track information
  db->status = sqlite3_prepare_v2(db->database, db_insert_cddb_track, -1, &statement, NULL);
  if (db->status != DB_OK) {
    set_error_message(db);
    sqlite3_finalize(statement);
    return DB_ERROR;
  }
  for (int i=0; i<disc_info->d_tracks; i++) {
    // bind track information values
    sqlite3_bind_int64(statement, 1, cddb_id);
    sqlite3_bind_int(statement, 2, disc_info->tracks[i].t_num);
    sqlite3_bind_text(statement, 3, disc_info->tracks[i].t_title, -1, SQLITE_STATIC);
    sqlite3_bind_int(statement, 4, disc_info->tracks[i].t_length);
    // execute statement
    db->status = sqlite3_step(statement);
    if (db->status != SQLITE_DONE) {
      set_error_message(db);
      sqlite3_finalize(statement);
      return DB_ERROR;
    }
    // reset statement so we can bind the values for the next track
    db->status = sqlite3_reset(statement);
  }
  // delete track insert statement
  db->status = sqlite3_finalize(statement);

  // insert internal hash value and cddb id to enable fast lookups for duplicate checking if we are using a temp lookup table
  if (lookup_method == DB_DUPLICATE_CHECK_TEMP && cddb_id >= 0) {
    db->status = sqlite3_prepare_v2(db->database, db_insert_cddb_temp, -1, &statement, NULL);
    if (db->status != DB_OK) {
      set_error_message(db);
      sqlite3_finalize(statement);
      return DB_ERROR; // added return statement to handle error case
    }
    // bind lookup and cddb id's
    sqlite3_bind_int64(statement, 1, disc_info->d_lookup);                    
    sqlite3_bind_int64(statement, 2, cddb_id);
    // execute statement
    db->status = sqlite3_step(statement);
    if (db->status != SQLITE_DONE) {
      set_error_message(db);
      sqlite3_finalize(statement);
      return DB_ERROR;
    }
    // delete temp table insert statement
    db->status = sqlite3_finalize(statement);
  }
  sqlite3_exec(db->database, DB_END_TRANSACTION, NULL, NULL, NULL);

  // done. disc information is inserted
  return DB_OK;
}

/**
 * @brief store the given cddb information in the database by updating the existing information or inserting a new entry
 * @param db database structure
 * @param disc_info disc information structure containing the cddb information
 * @param category_id the cddb file category id
 * @param lookup_method method to determine how to lookup the cddb id (0=by category and file, 1=by lookup in cddb, 2=by lookup in cddb_lookup temp table)
 * @return 0 if successful; another value indicates an error
 */
int store_cddb_entry_in_database(sql_db *db, disc *disc_info, long category_id, int lookup_method) {
  if (db==NULL || disc_info==NULL || category_id < 0) {
    return DB_ERROR;
  }
  
  // check if the disc information is already stored
  long cddb_id = -1;
  if (lookup_method == DB_DUPLICATE_CHECK_TEMP) {
    // try to get the cddb database id using the lookup id in the temporary table
    cddb_id = get_cddb_id_by_lookup_temp(db, disc_info->d_lookup);
  } else if (lookup_method == DB_DUPLICATE_CHECK_LOOKUP) {
    // try to get the cddb database id using the lookup id in the cddb table
    cddb_id = get_cddb_id_by_lookup_cddb(db, disc_info->d_lookup);
  }

  if (cddb_id >= 0) {
    // entry found: determine to 1) update; 2) discard update (duplicate) or 3) insert anyway
    disc* cddb_info = NULL;
    db->status = get_cddb_entry_from_database(db, cddb_id, &cddb_info);
    if (db->status != DB_OK || cddb_info == NULL) {
      return DB_ERROR;
    }
    // entry has the same lookup id and therefore also the same number of tracks and disc length
    if (disc_info->d_tracks == cddb_info->d_tracks && disc_info->d_length / CDE_CD_FRAMES == cddb_info->d_length / CDE_CD_FRAMES) {
      // check artist, title ignoring case
      if (strcasecmp(disc_info->d_artist, cddb_info->d_artist) == 0 && strcasecmp(disc_info->d_title, cddb_info->d_title) == 0 ) {

        // check year and revision
        if (disc_info->d_year == cddb_info->d_year && disc_info->cddb_revision <= cddb_info->cddb_revision) {
          // year and revision are the same: no update needed
          return DB_DUPLICATE;
        }

        if (disc_info->d_year == 0) {
          disc_info->d_year = cddb_info->d_year;
        }

        // update cddb entry
        return update_cddb_entry_in_database(db, disc_info, cddb_id, category_id);

      } // else: artist or title is different, insert new entry
    } // else: inconsistent entry, insert new entry
  } // else cddb_id not available, insert new entry

  // entry not found (or no look up perfomed), insert a new cddb entry
  return insert_cddb_entry_in_database(db, disc_info, category_id, lookup_method);
}
