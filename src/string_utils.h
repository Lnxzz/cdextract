/**************************************************************************

  libcdextract - string utilities

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

#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <string.h>


#define ENCODING_UNDEFINED 0
#define ENCODING_ASCII 1
#define ENCODING_ISO_8859_1 2
#define ENCODING_CP1252 3
#define ENCODING_UTF8 4
#define ENCODING_UTF16_BE 5
#define ENCODING_UTF16_LE 6
#define ENCODING_UTF32_BE 7
#define ENCODING_UTF32_LE 8


/**
 * @brief get (next) word from data and update pos
 */
extern int get_word(char **word, char *data, int *start, int length);

/**
 * @brief get line from data and update position in data
 */
extern int get_line(char **line, char *data, int *start, int length);

/**
 * @brief get signed int from data and update position in data (position)
 */
extern int get_signed_int(char *data, int *position, int length);

/**
 * @brief get unsigned int from data and update position in data (position)
 */
extern unsigned int get_unsigned_int(char *data, int *position, int length);

/**
 * @brief get unsigned long from data and update position in data (position)
 */
extern long get_signed_long(char *data, int *position, int length);

/**
 * @brief get unsigned long from data and update position in data (position)
 */
extern unsigned long get_unsigned_long(char *data, int *position, int length);

/**
 * @brief split string in two parts using the first occurance of the given seperator
 */
extern int split(char **left, char **right, const char *source, char seperator);

/**
 * @brief check if given substring is part of the string
 */
extern int contains(const char *str, const char *substring);

/**
 * @brief check if str starts with prefix
 *        returns 1 if string str starts with string prefix, 0 otherwise
 */
extern int starts_with(const char *prefix, const char *str);

/**
 * @brief check if str ends with postfix
 *        returns 1 if string str ends with string postfix, 0 otherwise
 */
extern int ends_with(const char *postfix, const char *str);

/**
 * @brief finds string item in string str and updates position in string str
 *        returns -1 if string item has not been found, returns relative position if found
 */
extern int find(char *item, int *position, const char *str);

/**
 * @brief returns the specified substring of the given source string
 */
extern int substring(char **dest, const char *source, int *position, int count);

/**
 * @brief returns the concatened string of the given strings
 */
extern char *cat_string(char *dest, const char *str);

/**
 * @brief returns a copy of the given string
 * @param src source string with the text to copy
 * @return the copy of the string
 */
extern char *copy_string(const char *str);

/**
 * @brief store the given text in the given destination string
 *        create an empty string if no input present
 * @param dest destination string
 * @param src source string with the text to set
 * @return 0 if successful
 */
extern int set_string(char **dest, const char *src);

/**
 * @brief store the given binary data in the given byte array
 * @param dest destination array
 * @param src source data
 * @param size size of the source data
 * @return 0 if successful
 */
extern int set_binary(char **dest, const char* src, int size);

/**
 * @brief trim leading and trailing whitespace
 * @param dest destination string
 * @param str the string from which to remove leading and trailing whitespace
 */
extern int trim_whitespace(char **dest, const char *str);

/**
 * @brief removes all occurrences of a substring from the given string
 * @param str the string from which to remove the substring
 * @param substring the substring to be removed
 * @return the modified string
 */
extern char *remove_substring(char *str, const char *substring);

/**
 * @brief removes the first `count` characters from the string
 * @param str the string from which the characters will be removed
 * @param count the number of characters to remove
 * @return the number of characters left after removal
 */
extern int remove_first_chars(char *str, int count);

/**
 * @brief returns the string with the given character replaced
 */
extern char *replace_char(const char *str, const char from, const char to);

/**
 * @brief returns the string with the given characters replaced
 */
extern char *replace_chars(const char *str, const char *from, const char to);

/**
 * @brief convert the given string to the corresponding string in lower case
 *        returns -1 if conversion is not possible
 */
extern int to_lower(char **dest, const char *source);

/**
 * @brief convert the given data to a hexadecimal string
 *        returns -1 if conversion is not possible
 */
extern int to_hex(char **dest, const char *data, int length);

/**
 * @brief convert the given hexadecimal string to a binary array
 *        returns -1 if conversion is not possible
 */
extern int from_hex(char **dest, const char *data, int length);

/**
 * @brief convert the given hexadecimal string to an unsigned integer
 *        returns -1 if conversion is not possible
 */
extern int uint_from_hex(unsigned int *dest, const char *data);

/**
 * @brief convert the given hexadecimal string to a 64-bit unsigned integer
 *        returns -1 if conversion is not possible
 */
extern int uint64_from_hex(u_int64_t *dest, const char *data);

/**
 * @brief returns an url encoded string
 * @param str the string to be encoded
 * @return a newly allocated string containing the URL-encoded version of the input string
 */
extern char *url_encode(const char *str);

/**
 * @brief check if the given string is a valid utf-8 string
 *       returns 1 if the string is valid utf-8, 0 otherwise
 */
extern int is_utf8(const char *data);

/**
 * @brief convert the given string to a utf-8 string
 *        returns NULL if conversion is not possible
 */
extern char *to_utf8(const char *data, int check_bom, int *encoding);

/**
 * @brief get the encoding string for the given encoding
 *        returns 0 if encoding is undefined
 */
extern const char *get_encoding(int encoding);


#endif