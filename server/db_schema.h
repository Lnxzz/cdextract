/**************************************************************************

  cdextract - sqlite database schema and prepared statements

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

#ifndef CDE_DB_SCHEMA_H
#define CDE_DB_SCHEMA_H


//
// database schema and reference data
//

#define DB_SCHEMA_VERSION "1.0"
#define DB_BEGIN_TRANSACTION "BEGIN TRANSACTION;"
#define DB_END_TRANSACTION "END TRANSACTION;"


// extracted cd's including track information and covers

#define db_table_genre \
  "CREATE TABLE IF NOT EXISTS genre(" \
  "[genre_id] INTEGER PRIMARY KEY NOT NULL," \
  "[genre_name] TEXT NOT NULL," \
  "UNIQUE (genre_name)) STRICT;"

const char *db_insert_genres[] = {
  "INSERT into genre VALUES (0, '') ON CONFLICT DO NOTHING;",
  "INSERT into genre VALUES (1, 'Data') ON CONFLICT DO NOTHING;",
  "INSERT into genre VALUES (2, 'Rock') ON CONFLICT DO NOTHING;",
  "INSERT into genre VALUES (3, 'Folk') ON CONFLICT DO NOTHING;",
  "INSERT into genre VALUES (4, 'Jazz') ON CONFLICT DO NOTHING;",
  "INSERT into genre VALUES (5, 'Blues') ON CONFLICT DO NOTHING;",
  "INSERT into genre VALUES (6, 'Classical') ON CONFLICT DO NOTHING;",
  "INSERT into genre VALUES (7, 'Country') ON CONFLICT DO NOTHING;",
  "INSERT into genre VALUES (8, 'New Age') ON CONFLICT DO NOTHING;",
  "INSERT into genre VALUES (9, 'Reggae') ON CONFLICT DO NOTHING;",
  "INSERT into genre VALUES (10, 'Soundtrack') ON CONFLICT DO NOTHING;",
  "INSERT into genre VALUES (11, 'Misc') ON CONFLICT DO NOTHING;",
  NULL
};

#define db_table_artist \
  "CREATE TABLE IF NOT EXISTS artist(" \
  "[artist_id] INTEGER PRIMARY KEY NOT NULL," \
  "[artist_name] TEXT NOT NULL," \
  "UNIQUE (artist_name)) STRICT;"

#define db_table_album \
  "CREATE TABLE IF NOT EXISTS album(" \
  "[album_id] INTEGER PRIMARY KEY NOT NULL," \
  "[album_name] TEXT NOT NULL," \
  "UNIQUE (album_name)) STRICT;"

#define db_table_category \
  "CREATE TABLE IF NOT EXISTS category(" \
  "[category_id] INTEGER PRIMARY KEY NOT NULL," \
  "[category_name] TEXT NOT NULL," \
  "UNIQUE (category_name)) STRICT;"

const char *db_insert_categories[] = {
  "INSERT into category VALUES (0, '') ON CONFLICT DO NOTHING;",
  "INSERT into category VALUES (1, 'data') ON CONFLICT DO NOTHING;",
  "INSERT into category VALUES (2, 'rock') ON CONFLICT DO NOTHING;",
  "INSERT into category VALUES (3, 'folk') ON CONFLICT DO NOTHING;",
  "INSERT into category VALUES (4, 'jazz') ON CONFLICT DO NOTHING;",
  "INSERT into category VALUES (5, 'blues') ON CONFLICT DO NOTHING;",
  "INSERT into category VALUES (6, 'classical') ON CONFLICT DO NOTHING;",
  "INSERT into category VALUES (7, 'country') ON CONFLICT DO NOTHING;",
  "INSERT into category VALUES (8, 'newage') ON CONFLICT DO NOTHING;",
  "INSERT into category VALUES (9, 'reggae') ON CONFLICT DO NOTHING;",
  "INSERT into category VALUES (10, 'soundtrack') ON CONFLICT DO NOTHING;",
  "INSERT into category VALUES (11, 'misc') ON CONFLICT DO NOTHING;",
  NULL
};

#define db_table_cover \
  "CREATE TABLE IF NOT EXISTS cover(" \
  "[cover_id] INTEGER PRIMARY KEY NOT NULL," \
  "[cover_type] INTEGER DEFAULT 0 NOT NULL," \
  "[cover_size] INTEGER DEFAULT 0 NOT NULL," \
  "[cover_hash] TEXT NOT NULL," \
  "[cover_data] BLOB NULL," \
  "UNIQUE (cover_type, cover_size, cover_hash)) STRICT;" 

#define db_table_disc \
  "CREATE TABLE IF NOT EXISTS disc(" \
  "[disc_id] INTEGER PRIMARY KEY NOT NULL," \
  "[d_id] INTEGER NOT NULL," \
  "[d_length] INTEGER NOT NULL," \
  "[d_lookup] INTEGER NOT NULL," \
  "[d_artist] INTEGER NOT NULL," \
  "[d_title] INTEGER NOT NULL," \
  "[d_genre] INTEGER NOT NULL," \
  "[d_year] INTEGER NOT NULL," \
  "[d_extended] TEXT NOT NULL," \
  "[cddb_query] TEXT NOT NULL," \
  "[cddb_category] INTEGER NOT NULL," \
  "[cddb_entry_id] INTEGER NOT NULL," \
  "[cddb_disc_id] INTEGER NOT NULL," \
  "[cddb_revision] INTEGER NOT NULL," \
  "[cddb_complete] INTEGER NOT NULL," \
  "[mb_query] TEXT NOT NULL," \
  "[mb_fuzzy_lookup] TEXT NOT NULL," \
  "[mb_disc_id] TEXT NOT NULL," \
  "[mb_release_id] TEXT NOT NULL," \
  "[mb_front_cover_id] INTEGER NULL," \
  "[mb_back_cover_id] INTEGER NULL," \
  "[mb_complete] INTEGER NOT NULL," \
  "[d_extracted] INTEGER NULL," \
  "[d_tracks] INTEGER NULL," \
  "UNIQUE ([d_lookup], [d_id], [d_length])," \
  "FOREIGN KEY ([d_artist]) REFERENCES artist([artist_id])" \
  "  ON DELETE RESTRICT ON UPDATE NO ACTION," \
  "FOREIGN KEY ([d_title]) REFERENCES album([album_id])" \
  "  ON DELETE RESTRICT ON UPDATE NO ACTION," \
  "FOREIGN KEY ([d_genre]) REFERENCES genre([genre_id])" \
  "  ON DELETE RESTRICT ON UPDATE NO ACTION," \
  "FOREIGN KEY ([cddb_category]) REFERENCES category([category_id])" \
  "  ON DELETE RESTRICT ON UPDATE NO ACTION," \
  "FOREIGN KEY ([mb_front_cover_id]) REFERENCES cover([cover_id])" \
  "  ON DELETE SET NULL ON UPDATE NO ACTION," \
  "FOREIGN KEY ([mb_back_cover_id]) REFERENCES cover([cover_id])" \
  "  ON DELETE SET NULL ON UPDATE NO ACTION) STRICT;"

#define db_table_track \
  "CREATE TABLE IF NOT EXISTS track(" \
  "[t_disc_id] INTEGER NOT NULL," \
  "[t_num] INTEGER NOT NULL," \
  "[t_length] INTEGER NOT NULL," \
  "[t_title] TEXT NOT NULL," \
  "[t_artist] INTEGER NOT NULL," \
  "[t_album] INTEGER NOT NULL," \
  "[t_genre] INTEGER NOT NULL," \
  "[t_year] INTEGER NOT NULL," \
  "[t_extended] TEXT NOT NULL," \
  "[t_filename] TEXT NOT NULL," \
  "[t_skipped] INTEGER NOT NULL," \
  "PRIMARY KEY ([t_disc_id],[t_num])," \
  "FOREIGN KEY ([t_disc_id]) REFERENCES disc([disc_id])" \
  "  ON DELETE CASCADE ON UPDATE NO ACTION," \
  "FOREIGN KEY ([t_artist]) REFERENCES artist([artist_id])" \
  "  ON DELETE RESTRICT ON UPDATE RESTRICT," \
  "FOREIGN KEY ([t_album]) REFERENCES album([album_id])" \
  "  ON DELETE RESTRICT ON UPDATE RESTRICT," \
  "FOREIGN KEY ([t_genre]) REFERENCES genre([genre_id])" \
  "  ON DELETE RESTRICT ON UPDATE NO ACTION) STRICT;"

#define db_create_idx_disc_lookup \
  "CREATE INDEX IF NOT EXISTS idx_disc_lookup ON disc([d_lookup]);"


// purge tables for full rescan of all discs

#define db_table_artist_purge \
  "DELETE FROM artist;"

#define db_table_album_purge \
  "DELETE FROM album;"

#define db_table_cover_purge \
  "DELETE FROM cover;"

#define db_table_disc_purge \
  "DELETE FROM disc;"

#define db_table_track_purge \
  "DELETE FROM track;"

const char *db_purge_tables[6] = {
  db_table_track_purge,  db_table_disc_purge,
  db_table_cover_purge, db_table_album_purge,
  db_table_artist_purge, NULL};


// local cddb 'cache'

#define db_table_cddb_genre \
  "CREATE TABLE IF NOT EXISTS cddb_genre(" \
  "[c_genre_id] INTEGER PRIMARY KEY NOT NULL," \
  "[c_genre_name] TEXT NOT NULL," \
  "UNIQUE (c_genre_name)) STRICT;"

const char *db_insert_cddb_genres[] = {
  "INSERT into cddb_genre VALUES (0, '') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (1, 'Data') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (2, 'Rock') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (3, 'Folk') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (4, 'Jazz') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (5, 'Blues') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (6, 'Classical') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (7, 'Country') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (8, 'New Age') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (9, 'Reggae') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (10, 'Soundtrack') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (11, 'Misc') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (12, 'Pop') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (13, 'Disco') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (14, 'House') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (15, 'Techno') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (16, 'Punk') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (17, 'Funk') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (18, 'Baroque') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (19, 'Schlager') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (20, 'Abstract') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (21, 'Alternative') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (22, 'Anime') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (23, 'Karuwacho') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (24, 'Audiobook') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (25, 'Bluegrass') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (26, 'LoFi') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (27, 'JPop') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (28, 'KPop') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (29, 'Ska') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (30, 'Rap') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (31, 'Indie') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (32, 'Dance') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (33, 'Ethnic') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (34, 'Metal') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (35, 'Jungle') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (36, 'Soul') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (37, 'Zouk') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (38, 'Beat') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (39, 'Oldies') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (40, 'Opera') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (41, 'Latin') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (42, 'Vocal') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (43, 'Polka') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (44, 'Retro') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (45, 'Salsa') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (46, 'Swing') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (47, 'Celtic') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (48, 'Gospel') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (49, 'Chorus') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (50, 'Symphony') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (51, 'Acoustic') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (52, 'Britpop') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (53, 'Klassik') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (54, 'Gothic') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (55, 'Instrumental') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (56, 'Industrial') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (57, 'Avantgarde') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (58, 'Meditation') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (59, 'Electronic') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (60, 'Hardcore') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (61, 'Fusion') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (62, 'Comedy') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (63, 'Orgel') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (64, 'Concerto') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (65, 'Crossover') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (66, 'Americana') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (67, 'Hip Hop') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (68, 'New Wave') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (69, 'Progressive Rock') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (70, 'Blues Rock') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (71, 'Acoustic Rock') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (72, 'Symphonic Rock') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (73, 'Slow Rock') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (74, 'Alternative Rock') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (75, 'Rock Pop') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (76, 'Blues Rock') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (77, 'Country Rock') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (78, 'Folk Rock') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (79, 'Indie Rock') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (80, 'Christian Rock') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (81, 'Native American') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (82, 'Heavy Metal') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (83, 'Death Metal') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (84, 'Hard Metal') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (85, 'Black Metal') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (86, 'Power Metal') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (87, 'Punk Rock') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (88, 'Classic Rock') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (89, 'Hard Rock') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (90, 'Chamber Music') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (91, 'National Folk') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (92, 'Rock and Roll') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (93, 'Drum and Bass') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (94, 'Rhythm and Blues') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (95, 'Big Band') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (96, 'Bossa Nova') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (97, 'Early Baroque') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (98, 'Easy Listening') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (99, 'Contemporary Christian') ON CONFLICT DO NOTHING;",
  "INSERT into cddb_genre VALUES (100, 'Praise and Worship') ON CONFLICT DO NOTHING;",
  NULL
};
  
#define db_table_cddb \
  "CREATE TABLE IF NOT EXISTS cddb(" \
  "[c_id] INTEGER PRIMARY KEY," \
  "[c_lookup] INTEGER NOT NULL," \
  "[c_category] INTEGER NOT NULL," \
  "[c_entryid] INTEGER NOT NULL," \
  "[c_discid] INTEGER NOT NULL," \
  "[c_artist] TEXT NOT NULL," \
  "[c_title] TEXT NOT NULL," \
  "[c_genre] INTEGER NOT NULL," \
  "[c_year] INTEGER," \
  "[c_seconds] INTEGER NOT NULL," \
  "[c_revision] INTEGER," \
  "[c_tracks] INTEGER NOT NULL," \
  "FOREIGN KEY ([c_category]) REFERENCES category([category_id])" \
  "  ON DELETE RESTRICT ON UPDATE NO ACTION," \
  "FOREIGN KEY ([c_genre]) REFERENCES cddb_genre([c_genre_id])" \
  "  ON DELETE RESTRICT ON UPDATE NO ACTION) STRICT;"

#define db_table_cddb_track \
  "CREATE TABLE IF NOT EXISTS cddb_track(" \
  "[t_c_id] INTEGER NOT NULL," \
  "[t_num] INTEGER NOT NULL," \
  "[t_title] TEXT NOT NULL," \
  "[t_frames] INTEGER NOT NULL," \
  "PRIMARY KEY ([t_c_id],[t_num])," \
  "FOREIGN KEY ([t_c_id]) REFERENCES cddb([c_id])" \
  "  ON DELETE CASCADE ON UPDATE NO ACTION) STRICT;"

const char *db_create_schema[13] = {
    db_table_album,         db_table_genre,
    db_table_artist,        db_table_album,
    db_table_category,      db_table_cover,
    db_table_disc,          db_table_track,
    db_table_cddb_genre,    db_table_cddb,
    db_table_cddb_track,    NULL};


//
// pragma's for database connection
//

#define db_pragma_synchronous_off \
  "PRAGMA synchronous=OFF"

#define db_pragma_count_changes_off \
  "PRAGMA count_changes=OFF"

#define db_pragma_journal_mode_mem \
  "PRAGMA journal_mode=MEMORY"

#define db_pragma_journal_mode_wal \
  "PRAGMA journal_mode=WAL"

#define db_pragma_temp_store_mem \
  "PRAGMA temp_store=MEMORY"

#define db_pragma_cache_size_128m \
  "PRAGMA cache_size=-128000"

const char *db_pragmas_open_db[6] = {
    db_pragma_synchronous_off,  db_pragma_count_changes_off,
    db_pragma_journal_mode_mem, db_pragma_temp_store_mem,
    db_pragma_cache_size_128m, NULL};


//
// pragma's used during database rebuild
//

#define db_pragma_foreign_keys_on \
  "PRAGMA foreign_keys=ON"

#define db_pragma_foreign_keys_off \
  "PRAGMA foreign_keys=OFF"


//
// prepared statements
//

// database backup

const char *db_backup_vacuum = \
  "VACUUM main INTO ?";


// local cddb 'cache' rebuild

#define db_table_cddb_lookup_temp \
  "CREATE TEMP TABLE IF NOT EXISTS cddb_lookup(" \
  "[l_c_lookup] INTEGER PRIMARY KEY," \
  "[l_c_id] INTEGER NOT NULL) WITHOUT ROWID;"

#define db_drop_cddb_lookup_temp \
  "DROP TABLE IF EXISTS cddb_lookup;"

#define db_create_idx_cddb_lookup \
  "CREATE INDEX IF NOT EXISTS idx_cddb_lookup ON cddb([c_lookup]);"

#define db_drop_idx_cddb_lookup \
  "DROP INDEX IF EXISTS idx_cddb_lookup;"

#define db_create_idx_cddb_artist \
  "CREATE INDEX IF NOT EXISTS idx_cddb_artist ON cddb([c_artist]);"

#define db_drop_idx_cddb_artist \
  "DROP INDEX IF EXISTS idx_cddb_artist;"

#define db_create_idx_cddb_title \
  "CREATE INDEX IF NOT EXISTS idx_cddb_title ON cddb([c_title]);"

#define db_drop_idx_cddb_title \
  "DROP INDEX IF EXISTS idx_cddb_title;"


// extracted cd's including track information and covers

const char *db_select_category_id = \
  "SELECT category_id FROM category WHERE category_name=?";

const char *db_select_artist_id = \
  "SELECT artist_id FROM artist WHERE artist_name=?";

const char *db_insert_artist = \
  "INSERT INTO artist(artist_name) " \
  "VALUES(?) ON CONFLICT (artist_name) DO NOTHING RETURNING artist_id";

const char *db_select_album_id = \
  "SELECT album_id FROM album WHERE album_name=?";

const char *db_insert_album = \
  "INSERT INTO album(album_name) " \
  "VALUES(?) ON CONFLICT (album_name) DO NOTHING RETURNING album_id";

const char *db_select_genre_id = \
  "SELECT genre_id FROM genre WHERE genre_name=?";

const char *db_insert_genre = \
  "INSERT INTO genre(genre_name) " \
  "VALUES(?) ON CONFLICT DO NOTHING RETURNING genre_id";

const char *db_select_cover_id = \
  "SELECT cover_id FROM cover WHERE cover_type=? AND cover_size=? AND cover_hash=?";

const char *db_select_front_cover = \
  "SELECT [cover_data] FROM disc " \
  "INNER JOIN cover on disc.[mb_front_cover_id] = cover.[cover_id] " \
  "WHERE [disc_id]=?";

const char *db_select_back_cover = \
  "SELECT [cover_data] FROM disc " \
  "INNER JOIN cover on disc.[mb_back_cover_id] = cover.[cover_id] " \
  "WHERE [disc_id]=?";

const char *db_insert_cover = \
  "INSERT INTO cover(cover_type, cover_size, cover_hash, cover_data) " \
  "VALUES(?,?,?,?) ON CONFLICT DO NOTHING RETURNING cover_id";

const char *db_update_front_cover_link = \
  "UPDATE disc SET [mb_front_cover_id]=? " \
  "WHERE [disc_id]=?";

const char *db_update_back_cover_link = \
  "UPDATE disc SET [mb_back_cover_id]=? " \
  "WHERE [disc_id]=?";

const char *db_select_disc_id_by_disc_info = \
  "SELECT [disc_id] " \
  "FROM disc " \
  "WHERE [d_lookup]=? AND [d_id]=? AND [d_length]=?";

const char *db_exists_disc_id = \
  "SELECT [disc_id] FROM disc WHERE [disc_id]=?";

const char *db_select_disc_list = \
  "SELECT [disc_id],[d_id],[d_length],[d_lookup],[artist_name],[album_name],[genre_name],[d_year],[d_extended]," \
  "  [cddb_query],[category_name],[cddb_entry_id],[cddb_disc_id],[cddb_revision],[cddb_complete]," \
  "  [mb_query],[mb_fuzzy_lookup],[mb_disc_id],[mb_release_id]," \
  "  front_cover.[cover_data],back_cover.[cover_data],[mb_complete],[d_extracted],[d_tracks] " \
  "FROM disc " \
  "INNER JOIN category on disc.[cddb_category] = category.[category_id] " \
  "INNER JOIN artist on disc.[d_artist] = artist.[artist_id] " \
  "INNER JOIN album on disc.[d_title] = album.[album_id] " \
  "INNER JOIN genre on disc.[d_genre] = genre.[genre_id] " \
  "LEFT JOIN cover as front_cover on disc.[mb_front_cover_id] = front_cover.[cover_id] " \
  "LEFT JOIN cover as back_cover on disc.[mb_back_cover_id] = back_cover.[cover_id] " \
  "ORDER BY [disc].[rowid] LIMIT ? OFFSET ?;";

const char *db_select_disc_id_by_toc = \
"SELECT [disc_id],[t_num],[t_length] " \
  "FROM disc " \
  "INNER JOIN track ON [disc].[disc_id]=[track].[t_disc_id] " \
  "WHERE [d_lookup]=? AND [d_id]=? AND [d_length]=? " \
  "ORDER BY [disc_id] DESC, [t_num] ASC;";

const char *db_select_disc_details_by_disc_id = \
  "SELECT [disc_id],[d_id],[d_length],[d_lookup],[artist_name],[album_name],[genre_name],[d_year],[d_extended]," \
  "  [cddb_query],[category_name],[cddb_entry_id],[cddb_disc_id],[cddb_revision],[cddb_complete]," \
  "  [mb_query],[mb_fuzzy_lookup],[mb_disc_id],[mb_release_id]," \
  "  front_cover.[cover_data],back_cover.[cover_data],[mb_complete],[d_extracted],[d_tracks] " \
  "FROM disc " \
  "INNER JOIN category on disc.[cddb_category] = category.[category_id] " \
  "INNER JOIN artist on disc.[d_artist] = artist.[artist_id] " \
  "INNER JOIN album on disc.[d_title] = album.[album_id] " \
  "INNER JOIN genre on disc.[d_genre] = genre.[genre_id] " \
  "LEFT JOIN cover as front_cover on disc.[mb_front_cover_id] = front_cover.[cover_id] " \
  "LEFT JOIN cover as back_cover on disc.[mb_back_cover_id] = back_cover.[cover_id] " \
  "WHERE [disc_id]=?;";

const char *db_insert_disc = \
  "INSERT INTO disc([d_id],[d_length],[d_lookup],[d_artist],[d_title],[d_genre],[d_year],[d_extended]," \
  "  [cddb_query],[cddb_category],[cddb_entry_id],[cddb_disc_id],[cddb_revision],[cddb_complete]," \
  "  [mb_query],[mb_fuzzy_lookup],[mb_disc_id],[mb_release_id]," \
  "  [mb_front_cover_id],[mb_back_cover_id],[mb_complete],[d_extracted],[d_tracks]) " \
  "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) " \
  "ON CONFLICT DO NOTHING RETURNING [disc_id]";

const char *db_update_disc = \
  "UPDATE disc SET [d_artist]=?, [d_title]=?, [d_genre]=?, [d_year]=?, [d_extended]=?," \
  "  [cddb_revision]=?, [cddb_complete]=?, [mb_complete]=?, [d_extracted]=? " \
  "WHERE [disc_id]=?";

const char *db_select_track_id = \
  "SELECT [rowid] FROM track WHERE [t_disc_id]=? AND [t_num]=?";

const char *db_select_track_count = \
  "SELECT [t_disc_id], COUNT(*) from track where [t_disc_id]=?";

const char *db_select_track_details = \
  "SELECT [t_disc_id],[t_num],[t_length],[t_title],[artist_name],[album_name]," \
  "  [genre_name],[t_year],[t_extended],[t_filename],[t_skipped] " \
  "FROM track " \
  "INNER JOIN artist on track.[t_artist] = artist.[artist_id] " \
  "INNER JOIN album on track.[t_album] = album.[album_id] " \
  "INNER JOIN genre on track.[t_genre] = genre.[genre_id] " \
  "WHERE [t_disc_id]=? " \
  "ORDER BY [t_disc_id],[t_num] ASC;";

const char *db_insert_track = \
  "INSERT INTO track([t_disc_id],[t_num],[t_length],[t_title],[t_artist],[t_album],[t_genre]," \
  "  [t_year],[t_extended],[t_filename],[t_skipped]) " \
  "VALUES(?,?,?,?,?,?,?,?,?,?,?) " \
  "ON CONFLICT DO NOTHING";

const char *db_update_track = \
  "UPDATE track SET [t_title]=?, [t_artist]=?, [t_album]=?, [t_genre]=?, [t_year]=?," \
  "  [t_extended]=?, [t_filename]=?, [t_skipped]=? " \
  "WHERE [t_disc_id]=? AND [t_num]=?";


// local cddb 'cache'

const char *db_select_cddb_genre_id = \
  "SELECT [c_genre_id] FROM cddb_genre WHERE [c_genre_name]=?";

const char *db_insert_cddb_genre = \
  "INSERT INTO cddb_genre(c_genre_name) " \
  "VALUES(?) ON CONFLICT DO NOTHING RETURNING c_genre_id";

const char *db_select_cddb_id_by_lookup_cddb = \
  "SELECT [c_id] FROM cddb WHERE [c_lookup]=?";

const char *db_select_cddb_id_by_lookup_temp = \
  "SELECT [l_c_id] FROM cddb_lookup WHERE [l_c_lookup]=?";

const char *db_select_cddb_by_toc = \
  "SELECT [c_id],[c_lookup],[category_name],[c_entryid],[c_discid],[c_artist],[c_title],[c_genre_name]," \
  "  [c_year],[c_seconds],[c_revision],[c_tracks],[t_num],[t_title],[t_frames] " \
  "FROM cddb " \
  "INNER JOIN category ON [c_category]=[category_id] " \
  "INNER JOIN cddb_genre ON [c_genre]=[c_genre_id] " \
  "INNER JOIN cddb_track ON [cddb].[c_id]=[cddb_track].[t_c_id] " \
  "WHERE [cddb].[c_lookup]=? " \
  "ORDER BY [c_id] DESC, [c_revision] DESC, [t_num] ASC;";

const char *db_select_cddb_by_cddb_id = \
  "SELECT [c_id],[c_lookup],[category_name],[c_entryid],[c_discid],[c_artist],[c_title],[c_genre_name]," \
  "  [c_year],[c_seconds],[c_revision],[c_tracks],[t_num],[t_title],[t_frames] " \
  "FROM cddb " \
  "INNER JOIN category ON [c_category]=[category_id] " \
  "INNER JOIN cddb_genre ON [c_genre]=[c_genre_id] " \
  "INNER JOIN cddb_track ON [cddb].[c_id]=[cddb_track].[t_c_id] " \
  "WHERE [cddb].[c_id]=? " \
  "ORDER BY [t_num] ASC;";

// search cddb - c_lookup by either a specific value or a range based on the number of tracks and disc length:
// c_lookup: 8 bits: most significant bits for the number of tracks; 16 bits: disc length; 40 bits: hash; >= 0x TT | DDDD | 0000000000
const char *db_search_cddb = \
  "WITH [weight] AS ( " \
  "  SELECT [c_id], [c_genre_name], ( " \
  "  CASE WHEN [c_lookup]=?1 THEN 512 ELSE 0 END + " \
  "  CASE WHEN [c_tracks]=?2 THEN 256 ELSE 0 END + " \
	"  CASE WHEN [c_artist]=?3 THEN 128 ELSE 0 END + " \
  "  CASE WHEN [c_title]=?4 THEN 64 ELSE 0 END + " \
  "  CASE WHEN [c_artist] LIKE ?5 THEN 32 ELSE 0 END + " \
  "  CASE WHEN [c_title] LIKE ?6 THEN 16 ELSE 0 END + " \
  "  CASE WHEN [c_seconds]=?7 THEN 8 ELSE 0 END + " \
  "  CASE WHEN ABS([c_seconds] - ?7) < 5 THEN 4 ELSE 0 END + " \
  "  CASE WHEN [c_year] = ?8 THEN 2 ELSE 0 END + " \
  "  CASE WHEN [c_genre_name] LIKE ?9 THEN 1 ELSE 0 END " \
  "  ) as [c_weight] " \
  "  FROM cddb " \
  "  INNER JOIN cddb_genre ON [c_genre]=[c_genre_id] " \
  "  WHERE [c_lookup] %s AND c_tracks=?2 %s" \
  "  ORDER BY [c_weight] DESC, [c_id] DESC " \
  "  LIMIT 3 " \
  ") " \
  "SELECT [cddb].[c_id],[c_lookup],[category_name],[c_entryid],[c_discid],[c_artist],[c_title],[c_genre_name],[c_year],[c_seconds],[c_revision],[c_tracks],[t_num],[t_title],[t_frames],[weight].[c_weight] " \
  "FROM cddb " \
  "INNER JOIN [category] ON [c_category]=[category_id] " \
  "INNER JOIN [cddb_track] ON [cddb].[c_id]=[cddb_track].[t_c_id] " \
  "INNER JOIN [weight] ON [cddb].[c_id]=[weight].[c_id] " \
  "ORDER BY [c_weight] DESC, [cddb].[c_id] DESC, [t_num] ASC;";

// search specific cddb entry by c_lookup hash
const char *db_search_cddb_lookup_single = \
  "=?1";

// search BETWEEN c_tracks * 72057594037927936 + c_seconds(1) * 75 * 1099511627776 AND c_tracks * 72057594037927936 + ((c_seconds(5999) * 75) & 65535) * 1099511627776
const char *db_search_cddb_lookup_between = \
  "BETWEEN (?2 * 72057594037927936) + (75 * 1099511627776) AND (?2 * 72057594037927936) + (65535 * 1099511627776)";

// search cddb entry by artist and title
const char *db_search_cddb_lookup_artist_title = \
  "AND [c_artist] LIKE ?3 AND [c_title] LIKE ?4";

const char *db_update_cddb = \
  "UPDATE cddb SET [c_category]=?, [c_entryid]=?, [c_discid]=?, [c_artist]=?, [c_title]=?, " \
  "  [c_genre]=?, [c_year]=?, [c_seconds]=?, [c_revision]=?, [c_tracks]=? WHERE [c_id]=?";

const char *db_insert_cddb = \
  "INSERT INTO cddb([c_lookup],[c_category],[c_entryid],[c_discid],[c_artist],[c_title],[c_genre],[c_year],[c_seconds],[c_revision],[c_tracks]) " \
  "VALUES(?,?,?,?,?,?,?,?,?,?,?) " \
  "ON CONFLICT DO NOTHING RETURNING [c_id]";

const char *db_select_cddb_track_id = \
  "SELECT [rowdid] FROM cddb_track WHERE [t_c_id]=? AND [t_num]=?";

const char *db_select_cddb_track = \
  "SELECT [t_num],[t_title],[t_frames] FROM cddb_track WHERE [t_c_id]=? ORDER BY t_num ASC;";

const char *db_insert_cddb_track = \
  "INSERT INTO cddb_track([t_c_id],[t_num],[t_title],[t_frames]) " \
  "VALUES(?,?,?,?) " \
  "ON CONFLICT DO NOTHING";

const char *db_update_cddb_track = \
  "UPDATE cddb_track SET [t_title]=?, [t_frames]=? " \
  "WHERE [t_c_id]=? AND [t_num]=?";

const char *db_insert_cddb_temp = \
  "INSERT INTO cddb_lookup([l_c_lookup],[l_c_id]) VALUES(?,?) ON CONFLICT DO NOTHING";

#endif