/**************************************************************************

  libcdextract - cddb client

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

#ifndef CDDB_H
#define CDDB_H

#include <stddef.h>

#include "libcdextract_types.h"


#define CDDB_LOCAL_ENDPOINT "http://localhost:8002/cgi-bin/cddb.cgi"
#define CDDB_REMOTE_ENDPOINT "http://gnudb.gnudb.org/~cddb/cddb.cgi"
#define CDDB_HELLO "pi+cdextract+cddb-tool+0.4.7"


typedef enum cddb_category {
  data = 0,       // (ISO9660 and other data CDs)
  rock = 1,       // (incl. funk, soul, rap, pop, industrial, metal, etc.)
  folk = 2,       // (self explanatory)
  jazz = 3,       // (self explanatory)
  blues = 4,      // (self explanatory)
  classical = 5,  // (self explanatory)
  country = 6,    // (self explanatory)
  newage = 7,     // (self explanatory)
  reggae = 8,     // (self explanatory)
  soundtrack = 9, // (movies, shows)
  misc = 10       // (others that do not fit the above categories)
} cddb_category;


#define CDBB_GENRE_MAP_TYPE_END 0
#define CDBB_GENRE_MAP_TYPE_EQUAL_OR 1
#define CDBB_GENRE_MAP_TYPE_EQUAL_AND 2
#define CDBB_GENRE_MAP_TYPE_BEGINS_OR 3
#define CDBB_GENRE_MAP_TYPE_CONTAINS 4

typedef struct cdddb_genre_map {
  int map_type;
  const char **from_genre_str;
  const char *to_genre_str;
} cdddb_genre_map;

static const cdddb_genre_map genre_mapping[] = {
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Top", "Hits", NULL}, .to_genre_str = "Pop" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Sound", "Fx", NULL}, .to_genre_str = "Sound FX" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"New", "Wave", NULL}, .to_genre_str = "New Wave" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Praise", "Worship", NULL}, .to_genre_str = "Praise and Worship" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Punk", "Rock", NULL}, .to_genre_str = "Punk Rock" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Pop", "Rock", NULL}, .to_genre_str = "Pop Rock" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Acoust", "Rock", NULL}, .to_genre_str = "Acoustic Rock" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Accoust", "Rock", NULL}, .to_genre_str = "Acoustic Rock" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Adult", "Rock", NULL}, .to_genre_str = "Adult Rock" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Classical", "Oper", NULL}, .to_genre_str = "Classical Opera" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Disco", "International", NULL}, .to_genre_str = "Disco" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Rock", "Roll", NULL}, .to_genre_str = "Rock and Roll" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Rock", "roll", NULL}, .to_genre_str = "Rock and Roll" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Drum", "Bass", NULL}, .to_genre_str = "Drum and Bass" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Drum", "bass", NULL}, .to_genre_str = "Drum and Bass" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Rhythm", "Blues", NULL}, .to_genre_str = "Rhythm and Blues" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Rhythm", "blues", NULL}, .to_genre_str = "Rhythm and Blues" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Ska", "Reggae", NULL}, .to_genre_str = "Ska Reggae" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Soul", "Rock", NULL}, .to_genre_str = "Soul Rock" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Soul", "Funk", NULL}, .to_genre_str = "Soul Funk" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Soul", "Jazz", NULL}, .to_genre_str = "Soul Jazz" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"roadway", "Musical", NULL}, .to_genre_str = "Broadway Musical" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Rhythmic", "Soul", NULL}, .to_genre_str = "Rhythmic Soul" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Pop", "Soul", NULL}, .to_genre_str = "Pop Soul" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Blues", "Soul", NULL}, .to_genre_str = "Blues Soul" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Country", "Soul", NULL}, .to_genre_str = "Country Soul" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Southern", "Soul", NULL}, .to_genre_str = "Southern Soul" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Opera", "Rock", NULL}, .to_genre_str = "Rock Opera" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Blue", "Eyed", "Soul", NULL}, .to_genre_str = "Blue Eyed Soul" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Progressive", "Power", "Metal", NULL}, .to_genre_str = "Progressive Power Metal" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_AND, .from_genre_str = (const char *[]){"Power", "Metal", NULL}, .to_genre_str = "Power Metal" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_OR,  .from_genre_str = (const char *[]){"Newwave", "77 Punk New Wave", NULL}, .to_genre_str = "New Wave" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_OR,  .from_genre_str = (const char *[]){"Newage", NULL}, .to_genre_str = "New Age" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_OR,  .from_genre_str = (const char *[]){"Countryrock", NULL}, .to_genre_str = "Country Rock" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_OR,  .from_genre_str = (const char *[]){"Folklore", "Filk", NULL}, .to_genre_str = "Folk" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_OR,  .from_genre_str = (const char *[]){"Pop General", NULL}, .to_genre_str = "Pop" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_OR,  .from_genre_str = (const char *[]){"East Coast Hiphop", NULL}, .to_genre_str = "East Coast Hip Hop" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_OR,  .from_genre_str = (const char *[]){"Rock Alternative", "Alt.rock", NULL}, .to_genre_str = "Alternative Rock" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_OR,  .from_genre_str = (const char *[]){"Ebm", NULL}, .to_genre_str = "Electronic" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_OR,  .from_genre_str = (const char *[]){"Mpb", NULL}, .to_genre_str = "Brazilian Pop" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_OR,  .from_genre_str = (const char *[]){"Choral", NULL}, .to_genre_str = "Chorus" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_OR,  .from_genre_str = (const char *[]){"Ddr", "Ost", NULL}, .to_genre_str = "Deutsch" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_OR,  .from_genre_str = (const char *[]){"Christian P W", NULL}, .to_genre_str = "Praise and Worship" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_OR,  .from_genre_str = (const char *[]){"0 Reggae", "Regg", NULL}, .to_genre_str = "Reggae" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_OR,  .from_genre_str = (const char *[]){"Brock", NULL}, .to_genre_str = "Rock" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_OR,  .from_genre_str = (const char *[]){"Nu Metal", NULL}, .to_genre_str = "Metal" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_OR,  .from_genre_str = (const char *[]){"R.a.c.", "Rac", NULL}, .to_genre_str = "Indie Rock" },
  { .map_type = CDBB_GENRE_MAP_TYPE_EQUAL_OR,  .from_genre_str = (const char *[]){"Unknown", "Undefined", "Other", "Miscellaneous", "Unclassifiable", "Inconnu", "Cd Bok", "Description Of The Extended Genre", "Dittycore", "Div.", "(none)", "(null)", "(pendente)", "(pending)", "Wtf", "±¹¾Ç", "ÆÇ¼Ò¸®", NULL}, .to_genre_str = "Misc" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Art Rock", NULL}, .to_genre_str = "Art Rock" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Alternative Rock", "Alt. Rock", "Alt Rock", NULL}, .to_genre_str = "Alternative Rock" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Baroque", "Barok", "Barock",  NULL}, .to_genre_str = "Baroque" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Britpop", "Brit Pop", "British Pop", "Brit Indie Pop", NULL}, .to_genre_str = "Britpop" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"British", NULL}, .to_genre_str = "British" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Club Dance", NULL}, .to_genre_str = "Club Dance" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Club House", NULL}, .to_genre_str = "Club House" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Folk Brit", NULL}, .to_genre_str = "Folk Brit" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Folk Irish", "Folk Ire", NULL}, .to_genre_str = "Folk Irish" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Flute", "Fluet", "Fluite", NULL}, .to_genre_str = "Flute" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Bossa", "Bosanova ", NULL}, .to_genre_str = "Bossa Nova" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Bluse", "Blus", "Blues", NULL}, .to_genre_str = "Blues" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Country Rock", NULL}, .to_genre_str = "Country Rock" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Country Western", NULL}, .to_genre_str = "Country Western" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Country Blues", NULL}, .to_genre_str = "Country Blues" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Country Folk", NULL}, .to_genre_str = "Country Folk" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Country", NULL}, .to_genre_str = "Country" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Diver", "Niet In", "Nicht ", NULL}, .to_genre_str = "Misc" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Dixi", "Dixueland", NULL}, .to_genre_str = "Dixieland" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Domestic", NULL}, .to_genre_str = "Domestic" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Doo Wop", "Doo Woop", "Doo Wap", NULL}, .to_genre_str = "Doo Wop" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Doom", NULL}, .to_genre_str = "Doom Metal" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Downtemp", "Downempo", "Downtwmpo", NULL}, .to_genre_str = "Downtempo" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Drun'n'bass", NULL}, .to_genre_str = "Drum and Bass" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Drone", NULL}, .to_genre_str = "Drone" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Drum", NULL}, .to_genre_str = "Drum" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Electro", "Dsp", NULL}, .to_genre_str = "Electronic" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Rock", NULL}, .to_genre_str = "Rock" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Gothic", NULL}, .to_genre_str = "Gothic" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Discofox", "Diskofox", NULL}, .to_genre_str = "Discofox" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Discopolo", "Disco Polo", NULL}, .to_genre_str = "Disco Polo" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Disco", "Disko", NULL}, .to_genre_str = "Disco" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Dubstep", "Dub Step", NULL}, .to_genre_str = "Dubstep" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Dub", "Dup", NULL}, .to_genre_str = "Dub" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Duet", NULL}, .to_genre_str = "Duet" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Duits", NULL}, .to_genre_str = "Deutsch" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Dutch", "Niederländisch", "Voetbal", NULL}, .to_genre_str = "Nederlands" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Musicals", "Musicale", "Musical's", "Musicalv", NULL}, .to_genre_str = "Musical" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"New Age", NULL}, .to_genre_str = "New Age" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Operatta", NULL}, .to_genre_str = "Operatta" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Operett", NULL}, .to_genre_str = "Operette" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Opera ", NULL}, .to_genre_str = "Opera" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Oper ", NULL}, .to_genre_str = "Oper" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Worship", "Worshiip", "Worsip", NULL}, .to_genre_str = "Worship" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Worldmusic", "Worldmucic", "World.music", NULL}, .to_genre_str = "Worldmusic" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"World Beat", "Worldbeat", NULL}, .to_genre_str = "Worldbeat" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"World Fusion", "World Fusuion", "Worldfusion", NULL}, .to_genre_str = "World Fusion" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Yodel", NULL}, .to_genre_str = "Yodel" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Yoga", NULL}, .to_genre_str = "Yoga" },
  { .map_type = CDBB_GENRE_MAP_TYPE_BEGINS_OR, .from_genre_str = (const char *[]){"Zigeuner", NULL}, .to_genre_str = "Zigeuner" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Book", "book", "boek", "Livre Audio", "Hörbuch", "Hoerbuch", "Lydbok", "Lyd Bog", NULL}, .to_genre_str = "Audiobook" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Lo Fi", "Lofi", NULL}, .to_genre_str = "LoFi" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"J Pop", "Jpop", "Japanese Pops", "Japan Pops", "j Pop", "Jp Pop", NULL}, .to_genre_str = "JPop" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"K Pop", "Kpop", NULL}, .to_genre_str = "KPop" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"C Pop", "Kpop", NULL}, .to_genre_str = "CPop" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Pops", NULL}, .to_genre_str = "Pop" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Power Pop", NULL}, .to_genre_str = "Power Pop" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Hip Hop", "Hiphop", NULL}, .to_genre_str = "Hip Hop" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Gangster Rap", "Gangsta Rap", NULL}, .to_genre_str = "Gangsta Rap" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Progressive Rock", "Prog Rock", "progressive Rock", NULL}, .to_genre_str = "Progressive Rock" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Blues Rock", NULL}, .to_genre_str = "Blues Rock" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Folk Rock", NULL}, .to_genre_str = "Folk Rock" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Death Metal", NULL}, .to_genre_str = "Death Metal" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Hard Metal", NULL}, .to_genre_str = "Hard Metal" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Black Metal", NULL}, .to_genre_str = "Black Metal" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Euro House", NULL}, .to_genre_str = "Euro House" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Techno", NULL}, .to_genre_str = "Techno" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Punk", NULL}, .to_genre_str = "Punk" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"R B", "Rn'b", "R'nb", "Rnb", "R'n'b", NULL}, .to_genre_str = "Rhythm and Blues" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Top 40", "S_modern_american", NULL}, .to_genre_str = "Pop" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Ambient", NULL}, .to_genre_str = "Ambient" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Dynamic Sounds", NULL}, .to_genre_str = "Dynamic Sounds" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Entspannungstechniken", NULL}, .to_genre_str = "Entspannungstechniken" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Abstract Drone", NULL}, .to_genre_str = "Abstract" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Tech House", NULL}, .to_genre_str = "Tech House" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Karuwacho", NULL}, .to_genre_str = "Karuwacho" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Ethnic", NULL}, .to_genre_str = "Ethnic" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Meditative", "Meditation", NULL}, .to_genre_str = "Meditation" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Contemporary Christian", NULL}, .to_genre_str = "Contemporary Christian" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Early Baroque", NULL}, .to_genre_str = "Early Baroque" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Anime", NULL}, .to_genre_str = "Anime" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Nintendo", "Sega", "Amiga", "Game", NULL}, .to_genre_str = "Game" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Soundtrack", "Sound Track", "Film Songs", "Warner Brothers", "Disney", "Disny", NULL}, .to_genre_str = "Soundtrack" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Schlager", "Sclager", "schlager", NULL}, .to_genre_str = "Schlager" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Bluegrass", NULL}, .to_genre_str = "Bluegrass" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Ska ", " Ska", "Skajazz", NULL}, .to_genre_str = "Ska" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Soul", NULL}, .to_genre_str = "Soul" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Freestyle", NULL}, .to_genre_str = "Freestyle" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Klassik", NULL}, .to_genre_str = "Klassik" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Polka", NULL}, .to_genre_str = "Polka" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"blues", NULL}, .to_genre_str = "Blues" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"rock", NULL}, .to_genre_str = "Rock" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"punk", NULL}, .to_genre_str = "Punk" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"pop", NULL}, .to_genre_str = "Pop" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Clubhouse", NULL}, .to_genre_str = "Club House" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"Flowerpower", NULL}, .to_genre_str = "Flower Power" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"X Mas", "X'mas", "Xmas", NULL}, .to_genre_str = "Christmas" },
  { .map_type = CDBB_GENRE_MAP_TYPE_CONTAINS,  .from_genre_str = (const char *[]){"No Thanks", NULL}, .to_genre_str = "" },
  { CDBB_GENRE_MAP_TYPE_END, NULL, NULL }
}; 


typedef enum cddb_keyword {
  UNKNOWN = 0,
  DISCID = 1,    // The data following this keyword should be a comma-separated list
                 // of 8-byte disc IDs. The disc ID indicated by the track offsets in
                 // the comment section must appear somewhere in the list.
  DTITLE = 2,    // By convention contains the artist and disc title separated by a "/"
                 // with a single space on either side to separate it from the text.
                 // If the disc is a sampler containing titles of various artists, the disc
                 // artist should be set to "Various" (without the quotes).
  DYEAR = 3,     // This field contains the (4-digit) year, in which the CD was released.
  DGENRE = 4,    // This field contains the exact genre of the disc in a textual form.
  TTITLEN = 5,   // There must be one of these for each track in the CD. The track
                 // number should be substituted for the "N", starting with 0. This field
                 // should contain the title of the Nth track on the CD. If the disc is a
                 // sampler and there are different artists for the track titles, the
                 // track artist and the track title (in that order) should be separated
                 // by a "/" with a single space on either side to separate it from the text.
  EXTD = 6,      // This field contains the "extended data" for the CD. This is intended
                 // to be used as a place for interesting information related to the CD,
                 // such as credits, et cetera. If there is more than one of these lines
                 // in the file, the data is concatenated. This allows for extended data
                 // of arbitrary length.
  EXTTN = 7,     // This field contains the "extended track data" for track "N". There
                 // must be one of these for each track in the CD. The track number
                 // should be substituted for the "N", starting with 0. This field is
                 // intended to be used as a place for interesting information related to
                 // the Nth track, such as the author and other credits, or lyrics. If
                 // there is more than one of these lines in the file, the data is
                 // concatenated. This allows for extended data of arbitrary length.
  PLAYORDER = 8, // This field contains a comma-separated list of track numbers which
                 // represent a programmed track play order. This field is generally
                 // stripped of data in non-local database entries. Applications that
                 // submit entries for addition to the main database should strip any data
                 // from this keyword (i.e. add an empty "PLAYORDER=" line).
  DOT = 9,       // Indicator for the end of the data object
  COMMENT = 10   // Indicator for a comment line (i.e. a line starting with "#")
} cddb_keyword;


/**
 * @brief get the category enum value from the given string
 */
int cddb_get_enum_category(char *category_str);

/**
 * @brief get the category string value from the given enum value
 */
const char *cddb_get_string_category(cddb_category category_val);

/**
 * @brief get the sanitized genre from the given string
 * @return 0 if successful
 */
int cddb_get_string_genre(char **genre, char *genre_str);

/**
 * @brief calculate cddb checksum
 *        a number like 2344 becomes 2+3+4+4 (13)
 */
int cddb_sum(int n);

/**
 * @brief try to reconstruct cddb/musicbrainz query strings from the disc_info
 *        if they are not yet present
 * @param disc_info the disc information structure
 * @param track_frame_offsets the offsets for each track (use NULL to reconstruct)
 * @param verbose print detailed output
 * @return 0 if successful
 */
int cddb_reconstruct_query_strings(disc *disc_info, char *track_frame_offsets, int verbose);

/**
 * @brief get cddb token from data and update position in data to
 *        the contents of this token.
 */
int cddb_get_token(char *data, int *position, int length, int *title_nr);

/**
 * @brief parse query response data and store the results in disc_info
 */
int cddb_parse_query_response(disc *disc_info, char* cddb_data, int verbose);

/**
 * @brief parse read response data and store the results in disc_info
 *        PRE: disc_info must be initialized
 */
int cddb_parse_data(disc *disc_info, cdrom_drive *drive, char* cddb_data, int pos, int reconstruct_queries, int verbose);

/**
 * @brief query the online cddb service and parse the response
 *        Example url:
 *        https://gnudb.gnudb.org/~cddb/cddb.cgi?cmd=cddb+query+92093e0a+10+175+18469+34444+51154+70524+88841+104824+124686+140966+159454+2368&hello=pi+cdplayer+cddb-tool+0.4.7&proto=6
 */
int cddb_query(disc *disc_info, const char *end_point, int verbose);

/**
 * @brief read from the online cddb service and parse the response
 *        Example url:
 *        https://gnudb.gnudb.org/~cddb/cddb.cgi?cmd=cddb+read+data+92093e0a&hello=pi+cdplayer+cddb-tool+0.4.7&proto=6
 */
int cddb_read(disc *disc_info, cdrom_drive *drive, const char *end_point, int verbose);

/**
 * @brief query and read from the online cddb service and parse the response
 *        returned cddb information will be parsed and stored in disc_info
 */
int cddb_get_disc_info(disc *disc_info, cdrom_drive *drive, int verbose);

/**
 * @brief writes a cddb entry from the gathered disc information to a file in xmcd format
 *        PRE: prepared disc information structure
 * @param disc_info the disc information structure
 * @param folder folder to store the file
 * @param overwrite overwrite the file if it exists.
 * @param verbose print detailed output
 * @return 0 if successful
 */
int cddb_write_entry(disc *disc_info, const char *folder, int overwrite, int verbose);

#endif