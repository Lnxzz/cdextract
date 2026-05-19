/**************************************************************************

  libcdextract - string utilities

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "string_utils.h"


/**
 * @brief get (next) word from data and update pos
 */
int get_word(char **word, char *data, int *start, int length) {
  int proceed = 1;
  int cnt = 0;
  int pos = *start;

  // check if data is available
  if (data == NULL) {
    return 0;
  }

  // iterate through data
  while (pos < length && proceed) {
    if (data[pos] == ' ' || data[pos] == '\t' || data[pos] == '\r' || data[pos] == '\n' || data[pos] == '\0') {
      // found whitespace
      if (cnt==0) {
        // discard leading whitespace
        (*start)++;
      } else {
        // stop when we find trailing whitespace
        proceed = 0;
      }
    } else {
      // normal character: count on
      cnt++;
    }
    pos++;
  }

  // allocate new string and copy data
  char *tmp = realloc(*word, (cnt + 1) * sizeof(char));
  if (tmp == NULL) {
    // handle memory allocation failure
    free(*word);
    return 0;
  }
  *word = tmp;
  memcpy(*word, &(data[*start]), cnt);
  (*word)[cnt] = '\0';

  // set new start position in data
  *start = pos;
  return cnt;
}

/**
 * @brief get line from data and update position in data
 */
int get_line(char **line, char *data, int *start, int length) {
  int proceed = 1;
  int cnt = 0;
  int pos = *start;

  // check if data is available
  if (data == NULL) {
    return 0;
  }

  // iterate through data
  while (pos < length && proceed) {
    if (data[pos] == '\r' || data[pos] == '\n' || data[pos] == '\0') {
      // stop when we find a carriage return, line feed or string termination character
      proceed = 0;
    } else {
      // normal character: count on
      cnt++;
      pos++;
    }
  }

  // allocate new string and copy data
  char *tmp = realloc(*line, (cnt + 1) * sizeof(char));
  if (tmp == NULL) {
    // handle memory allocation failure
    free(*line);
    return 0;
  } 
  *line = tmp;
  memcpy(*line, &(data[*start]), cnt);
  (*line)[cnt] = '\0';
  
  // move position till start of next line (to take care of \r\n terminated lines)
  while (pos < length && (data[pos] == '\r' || data[pos] == '\n')) {
    pos++;
  }

  // set new start position in data
  *start = pos;
  return cnt;
}

/**
 * @brief get signed int from data and update position in data (position)
 */
int get_signed_int(char *data, int *position, int length) {
  int result = 0;
  char *word = calloc(1, sizeof(char));
  // get first word from data 
  if (get_word(&word, data, position, length) > 0) {
    // convert word to number
    sscanf(word, "%d", &result);
  }
  free(word);
  return result;
}

/**
 * @brief get unsigned int from data and update position in data (position)
 */
unsigned int get_unsigned_int(char *data, int *position, int length) {
  unsigned int result = 0;
  char *word = calloc(1, sizeof(char));
  // get first word from data 
  if (get_word(&word, data, position, length) > 0) {
    // convert word to number
    sscanf(word, "%u", &result);
  }
  free(word);
  return result;
}

/**
 * @brief get signed long from data and update position in data (position)
 */
long get_signed_long(char *data, int *position, int length) {
  long result = 0;
  char *word = calloc(1, sizeof(char));
  // get first word from data 
  if (get_word(&word, data, position, length) > 0) {
    // convert word to number
    sscanf(word, "%ld", &result);
  }
  free(word);
  return result;
}

/**
 * @brief get unsigned long from data and update position in data (position)
 */
unsigned long get_unsigned_long(char *data, int *position, int length) {
  unsigned long result = 0;
  char *word = calloc(1, sizeof(char));
  // get first word from data 
  if (get_word(&word, data, position, length) > 0) {
    // convert word to number
    sscanf(word, "%lu", &result);
  }
  free(word);
  return result;
}

/**
 * @brief split the source string in two parts using the first occurance of the given seperator
 *        returns 1 if the string can be split
 */
int split(char **left, char **right, const char *source, char seperator) {
  if (source == NULL) {
    return 0;
  }
  char *pos = strchr(source, seperator);
  if (pos == NULL) {
    return 0;
  }
  int size_left = (int)(pos - source);
  int size_right = (strlen(source) - size_left) - 1;
  int delta = 1;
  // remove trailing whitespace of the left string
  while (size_left > 0 && source[size_left-1] == ' ') {
    size_left--;
    delta++;
  }
  // remove leading whitespace of the right string
  while (size_right > 0 && source[size_left+delta] == ' ') {
    size_right--;
    delta++;
  }
  // allocate strings and copy data
  if (size_left >= 0 && size_right >= 0) {
    if (*left == NULL) {
      *left = malloc((size_left + 1) * sizeof(char));
    } else {
      *left = realloc(*left, (size_left + 1) * sizeof(char));
    }
    memcpy(*left, source, size_left);
    (*left)[size_left] = '\0';
    if (*right == NULL) {
      *right = malloc((size_right + 1) * sizeof(char));
    } else {
      *right = realloc(*right, (size_right + 1) * sizeof(char));
    }
    memcpy(*right, &(source[size_left + delta]), size_right);
    (*right)[size_right] = '\0';
    return 1;
  }
  return 0;
}

/**
 * @brief check if given substring is part of the string
 *        returns 1 if the given substring is part of the string
 */
int contains(const char *str, const char *substring) {
  return (strstr(str, substring) != NULL) ? 1 : 0;
}

/**
 * @brief check if str starts with prefix
 *        returns 1 if string str starts with string prefix, 0 otherwise
 */
int starts_with(const char *prefix, const char *str) {
  if (str==NULL || prefix==NULL) {
    return 0;
  }
  return strncmp(prefix, str, strlen(prefix)) == 0 ? 1 : 0;
}

/**
 * @brief check if str ends with postfix
 *        returns 1 if string str ends with string postfix, 0 otherwise
 */
int ends_with(const char *postfix, const char *str) {
  if (str==NULL || postfix==NULL || strlen(postfix) > strlen(str)) {
    return 0;
  }
  return strncmp(postfix, &str[strlen(str) - strlen(postfix)], strlen(postfix)) == 0 ? 1 : 0;
}

/**
 * @brief finds string item in string str and updates position in string str
 *        returns -1 if string item has not been found, returns relative position if found
 */
int find(char *item, int *position, const char *str) {
  int pos_from_start = 0;

  if (item==NULL || str==NULL) {
    return -1;
  }

  // search item in str
  char *p = strstr(str, item);
  
  if (p != NULL) {
    pos_from_start = (int)(p - str);
    *position = pos_from_start;
  }
  
  
  char *part = calloc(64, sizeof(char));
	memcpy(part, &(str[*position]), 63);

  return pos_from_start;
}

/**
 * @brief returns the specified substring of the given source string
 */
int substring(char **dest, const char *source, int *position, int count)
{
  int size = count;
  if (*position + count > strlen(source)) {
    size = (*position + count) -  strlen(source);
  }
  *dest = realloc(*dest, (size + 1) * sizeof(char));
  strncpy(*dest, (source + *position), size);
  *position = *position + count;
  return (int)0;
}

/**
 * @brief returns the concatened string of the given strings
 */
char *cat_string(char *dest, const char *str)  {
  if (str) {
    if (dest)
      dest = realloc(dest, (strlen(dest) + strlen(str) + 1) * sizeof(char));
    else
      dest = calloc(strlen(str) + 1, sizeof(char));
    strcat(dest, str);
  }
  return (dest);
}

/**
 * @brief returns a copy of the given string
 * @param src source string with the text to copy
 * @return the copy of the string
 */
char *copy_string(const char *str) {
  if (str) {
    char *ret = malloc((strlen(str) + 1) * sizeof(char));
    strcpy(ret, str);
    return ret;
  }
  return NULL;
}

/**
 * @brief store the given text in the given destination string
 *        create an empty string if no input present
 * @param dest destination string
 * @param src source string with the text to set
 * @return 0 if successful
 */
int set_string(char **dest, const char *src) {
  if (src != NULL) {
    if (*dest != NULL) {
      char *tmp = realloc(*dest, (strlen(src) + 1) * sizeof(char));
      if (tmp == NULL) {
        free(*dest);
        return -1;
      }
      *dest = tmp;
    } else {
      char *tmp = calloc(strlen(src) + 1, sizeof(char));
      if (tmp == NULL) {
        return -1;
      }
      *dest = tmp;
    }
    strcpy(*dest, src);
    return 0;
  }
  // src is NULL
  if (*dest == NULL) {
    char *tmp = calloc(1, sizeof(char));
    if (tmp == NULL) {
      return -1;
    }
    *dest = tmp;
  }
  (*dest)[0] = '\0';
  return 0;
}

/**
 * @brief store the given binary data in the given byte array
 * @param dest destination array
 * @param src source data
 * @param size size of the source data
 * @return 0 if successful
 */
int set_binary(char **dest, const char* src, int size) {
  if (src != NULL) {
    if (*dest != NULL) {
      char *tmp = realloc(*dest, size * sizeof(char));
      if (tmp == NULL) {
        free(*dest);
        return -1;
      }
      *dest = tmp;
    } else {
      char *tmp = calloc(size, sizeof(char));
      if (tmp == NULL) {
        return -1;
      }
      *dest = tmp;
    }
    memcpy(*dest, src, size);
  }
  return 0;
}

/**
 * @brief trim leading and trailing whitespace
 * @param dest destination string
 * @param str the string from which to remove leading and trailing whitespace
 */
int trim_whitespace(char **dest, const char *str) {
  if(str == NULL) {
    char *tmp;
    if (*dest == NULL) {
      tmp = calloc(1, sizeof(char));
    } else {
      tmp = realloc(*dest, sizeof(char));
    }
    *dest = tmp;
    (*dest)[0] = '\0';
    return 0;
  }

  // trim leading spaces
  int start = 0;
  while((str[start] == ' ' || str[start] == '\t' || str[start] == '\r' || str[start] == '\n') && str[start] != '\0') {
    ++start;
  }
 
  // check if result is empty string
  if (start == strlen(str)) {
    char *tmp;
    if (*dest == NULL) {
      tmp = calloc(1, sizeof(char));
    } else {
      tmp = realloc(*dest, sizeof(char));
    }
    *dest = tmp;
    (*dest)[0] = '\0';
    return 0;
  }

  // trim trailing spaces
  int end = strlen(str)-1;
  while((str[end] == ' ' || str[end] == '\t' || str[end] == '\r' || str[end] == '\n') && str[end] != '\0') {
     --end;
  }
  ++end;

  // set output size to minimum of trimmed string length and buffer size minus 1
  int dest_size = end - start;

  // copy trimmed string and add null terminator
  char *tmp = calloc(dest_size + 1, sizeof(char));
  memcpy(tmp, &str[start], dest_size);
  tmp[dest_size] = '\0';
  if (*dest != NULL) {
    free(*dest);
  }
  *dest = tmp;
  return dest_size;
}

/**
 * @brief removes all occurrences of a substring from the given string
 * @param str the string from which to remove the substring
 * @param substring the substring to be removed
 * @return the modified string
 */
char *remove_substring(char *str, const char *substring) {
  char *p, *q, *r;
  if (str == NULL || substring == NULL) {
      return str;
  }  
  if (*substring && (q = r = strstr(str, substring)) != NULL) {
      size_t len = strlen(substring);
      while ((r = strstr(p = r + len, substring)) != NULL) {
          memmove(q, p, r - p);
          q += r - p;
      }
      memmove(q, p, strlen(p) + 1);
  }
  return str;
}

/**
 * @brief removes the first `count` characters from the string
 * @param str the string from which the characters will be removed
 * @param count the number of characters to remove
 * @return the number of characters left after removal
 */
int remove_first_chars(char *str, int count) {
  if (str == NULL || count <= 0) {
    return -1;
  }
  int len = strlen(str);
  if (count > len) {
    count = len;
  }
  memmove(str, str + count, len - count + 1);
  return len - count;
}

/**
 * @brief returns the string with the given character replaced
 */
char *replace_char(const char *str, const char from, const char to) {
  if (str != NULL) {
    ssize_t len = strlen(str);
    char *ret = calloc((len + 1), sizeof(char));
    if (ret == NULL) {
      return NULL; // memory allocation failed
    }
    for (size_t i = 0; i < len; i++) {
      if (str[i] == from) {
        ret[i] = to;
      } else {
        ret[i] = str[i];
      }
    }
    return ret;
  }
  return NULL;
}

/**
 * @brief returns the string with the given characters replaced
 */
char *replace_chars(const char *str, const char *from, const char to) {
  if (str != NULL && from != NULL) {
    ssize_t len = strlen(str);
    size_t from_length =  strlen(from);
    char *ret = calloc((len + 1), sizeof(char));
    if (ret == NULL) {
      return NULL; // memory allocation failed
    }
    for (size_t i = 0; i < len; i++) {
      ret[i] = str[i];
      for (size_t j = 0; j < from_length; j++) {
        if (str[i] == from[j]) {
          ret[i] = to;
        }
      }
    }
    return ret;
  }
  return NULL;
}

/**
 * @brief convert the given string to the corresponding string in lower case
 *        returns -1 if conversion is not possible
 */
int to_lower(char **dest, const char *source) {
  size_t length = 0;
  if (source == NULL) {
    return -1;
  }
  length = strlen(source);
  *dest = realloc(*dest, (length + 1) * sizeof(char));
  for (size_t i = 0; i < length; ++i) { 
    (*dest)[i] = tolower((unsigned char)source[i]);
  }
  (*dest)[length] = '\0';
  return 0;
}

/**
 * @brief convert the given data to a hexadecimal string
 *        returns -1 if conversion is not possible
 */
int to_hex(char **dest, const char *data, int length) {
  if (data == NULL) {
    return -1;
  }
  if (length > 0) {
    char *tmp = realloc(*dest, (length * 2 + 1) * sizeof(char));
    if (tmp == NULL) {
      return -1;
    }
    *dest = tmp;
    char b;
    for (int y = 0, x = 0; y < length; ++y, ++x) {
      b = ((char)((data[y] >> 4)) & 0xF);
      (*dest)[x] = (char)(b > 9 ? b + 0x37 : b + 0x30);
      b = ((char)(data[y] & 0xF));
      (*dest)[++x] = (char)(b > 9 ? b + 0x37 : b + 0x30);
    } 
    (*dest)[length] = '\0';
    return 0;
  }
  return -1;
}

/**
 * @brief convert the given hexadecimal string to a binary array
 *        returns -1 if conversion is not possible
 */
extern int from_hex(char **dest, const char *data, int length) {
  if (data == NULL) {
    return -1;
  }
  if (length % 2 != 0) {
    return -1;
  }
  if (length > 0) {
    char *tmp = realloc(*dest, (length / 2) * sizeof(char));
    if (tmp == NULL) {
      return -1;
    }
    *dest = tmp;
    for (int i = 0; i < length; i += 2) {
      sscanf(&data[i], "%2hhx", &(*dest)[i / 2]);
    }
    return 0;
  }
  return -1;
}

/**
 * @brief convert the given hexadecimal string to an unsigned integer
 *        returns -1 if conversion is not possible
 */
int uint_from_hex(unsigned int *dest, const char *data) {
  if (data == NULL) {
    return -1;
  }
  size_t length = strlen(data);
  if (length==0 || length > 8) {
    return -1;
  }
  if (sscanf(data, "%x", dest) == 1) {
    return 0;
  }
  return -1;
}

/**
 * @brief convert the given hexadecimal string to a 64-bit unsigned integer
 *        returns -1 if conversion is not possible
 */
int uint64_from_hex(u_int64_t *dest, const char *data) {
  if (data == NULL) {
    return -1;
  }
  size_t length = strlen(data);
  if (length==0 || length > 16) {
    return -1;
  }
  if (sscanf(data, "%lx", dest) == 1) {
    return 0;
  }
  return -1;
}

/**
 * @brief returns an url encoded string
 * @param str the string to be encoded
 * @return a newly allocated string containing the URL-encoded version of the input string
 */
char *url_encode(const char *str) {
  if (str == NULL) {
    return calloc(1, sizeof(char));
  }
  size_t len = strlen(str);
  char *encoded = calloc(len * 3 + 1, sizeof(char));
  if (encoded == NULL) {
    return NULL;
  }
  char *p = encoded;
  for (size_t i = 0; i < len; ++i) {
    unsigned char c = (unsigned char)str[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      // keep alphanumeric and other accepted characters intact
      *p++ = c;
    } else {
      // any other characters are percent-encoded
      sprintf(p, "%%%02X", c);
      p += 3;
    }
  }
  return encoded;
}

/**
 * @brief check if the given string is a valid utf-8 string
 *       returns 1 if the string is valid utf-8, 0 otherwise
 */
int is_utf8(const char *data) {
  if (data == NULL) {
    return 0;
  }
  unsigned char c;
  int i, j, len;
  for (i = 0; data[i] != '\0'; i++) {
    c = (unsigned char)data[i];
    if (c < 0x80) {
      continue;
    }
    if ((c & 0xe0) == 0xc0) {
      len = 1;
    } else if ((c & 0xf0) == 0xe0) {
      len = 2;
    } else if ((c & 0xf8) == 0xf0) {
      len = 3;
    } else {
      return 0;
    }
    for (j = 0; j < len; j++) {
      if ((data[++i] & 0xc0) != 0x80) {
        return 0;
      }
    }
  }
  return 1;
}

/**
 * @brief convert the given string to a utf-8 string
 *        returns NULL if conversion is not possible
 */
char *to_utf8(const char *data, int check_bom, int *encoding) {
  *encoding = ENCODING_UNDEFINED;
  if (data == NULL) {
    return NULL;
  }
  size_t len = strlen(data);
  // check for byte order marks
  if (check_bom == 1) {
    if (len >= 2 && (const unsigned char)data[0] == 0xfe && (const unsigned char)data[1] == 0xff) {
      *encoding = ENCODING_UTF16_BE;
      return NULL;
    }
    if (len >=2 && (const unsigned char)data[0] == 0xff && (const unsigned char)data[1] == 0xfe) {
      *encoding = ENCODING_UTF16_LE;
      return NULL;
    }
    if (len >= 4 && (const unsigned char)data[0] == 0x00 && (const unsigned char)data[1] == 0x00 && (const unsigned char)data[2] == 0xfe && (const unsigned char)data[3] == 0xff) {
      *encoding = ENCODING_UTF32_BE;
      return NULL;
    }
    if (len >= 4 && (const unsigned char)data[0] == 0xff && (const unsigned char)data[1] == 0xfe && (const unsigned char)data[2] == 0x00 && (const unsigned char)data[3] == 0x00) {
      *encoding = ENCODING_UTF32_LE;
      return NULL;
    }
    if (len >= 3 && (const unsigned char)data[0] == 0xef && (const unsigned char)data[1] == 0xbb && (const unsigned char)data[2] == 0xbf) {
      *encoding = ENCODING_UTF8;
      // data in utf-8 format: no conversion needed
      return copy_string(data + 3);
    }
  }
  // check if data is already utf-8
  if (is_utf8(data)) {
    *encoding = ENCODING_UTF8;
    // data in utf-8 format: no conversion needed
    return copy_string(data);
  }
  // convert ascii/cp-1252/iso-8859-1 to utf-8
  char *utf8 = calloc((len * 3 + 1), sizeof(char));
  if (utf8 == NULL) {
    return NULL;
  }
  size_t in_pos = 0;
  size_t out_pos = 0;
  size_t ascii_cnt = 0;
  size_t cp1252_cnt = 0;
  size_t iso_8859_1_cnt = 0;
  size_t other_cnt = 0;
  while (in_pos < len) {
    unsigned char c = (unsigned char)data[in_pos];
    if (c >= 0x00 && c < 0x20) {
      // non-printable 7-bit ASCII character
      if (c == 0x09 || c == 0x0a || c == 0x0d) {
        // tab, line feed or carriage return
        utf8[out_pos] = c;
        ++out_pos;
      } else {
        // no conversion available
        ++other_cnt;
      }
      ++ascii_cnt;
    } else if (c >= 0x20 && c < 0x80) {
      // printable 7-bit ASCII character
      utf8[out_pos] = c;
      ++out_pos;
      ++ascii_cnt;
    } else if (c >= 0x80 && c < 0xa0) {
      // cp-1252 character
      if (c == 0x80) {
        // euro sign (€)
        utf8[out_pos] = 0xe2;
        utf8[out_pos + 1] = 0x82;
        utf8[out_pos + 2] = 0xac;
        out_pos += 3;
        ++cp1252_cnt;
      } else if (c == 0x81) {
        // no conversion available
        ++other_cnt;
      } else if (c == 0x82) {
        // single low-9 quotation mark (‚)
        utf8[out_pos] = 0xe2;
        utf8[out_pos + 1] = 0x80;
        utf8[out_pos + 2] = 0x9a;
        out_pos += 3;
        ++cp1252_cnt;
      } else if (c == 0x83) {
        // latin small letter f with hook (ƒ)
        utf8[out_pos] = 0xc6;
        utf8[out_pos + 1] = 0x92;
        out_pos += 2;
        ++cp1252_cnt;
      } else if (c == 0x84) {
        // double low-9 quotation mark („)
        utf8[out_pos] = 0xe2;
        utf8[out_pos + 1] = 0x80;
        utf8[out_pos + 2] = 0x9e;
        out_pos += 3;
        ++cp1252_cnt;
      } else if (c == 0x85) {
        // horizontal ellipsis (…)
        utf8[out_pos] = 0xe2;
        utf8[out_pos + 1] = 0x80;
        utf8[out_pos + 2] = 0xa6;
        out_pos += 3;
        ++cp1252_cnt;
      } else if (c == 0x86) {
        // dagger (†)
        utf8[out_pos] = 0xe2;
        utf8[out_pos + 1] = 0x80;
        utf8[out_pos + 2] = 0xa0;
        out_pos += 3;
        ++cp1252_cnt;
      } else if (c == 0x87) {
        // double dagger (‡)
        utf8[out_pos] = 0xe2;
        utf8[out_pos + 1] = 0x80;
        utf8[out_pos + 2] = 0xa1;
        out_pos += 3;
        ++cp1252_cnt;
      } else if (c == 0x88) {
        // modifier letter circumflex accent (ˆ)
        utf8[out_pos] = 0xcb;
        utf8[out_pos + 1] = 0x86;
        out_pos += 2;
        ++cp1252_cnt;
      } else if (c == 0x89) {
        // per mille sign (‰)
        utf8[out_pos] = 0xe2;
        utf8[out_pos + 1] = 0x80;
        utf8[out_pos + 2] = 0xb0;
        out_pos += 3;
        ++cp1252_cnt;
      } else if (c == 0x8a) {
        // latin capital letter S with caron (Š)
        utf8[out_pos] = 0xc5;
        utf8[out_pos + 1] = 0xa0;
        out_pos += 2;
        ++cp1252_cnt;
      } else if (c == 0x8b) {
        // single left-pointing angle quotation mark (‹)
        utf8[out_pos] = 0xe2;
        utf8[out_pos + 1] = 0x80;
        utf8[out_pos + 2] = 0xb9;
        out_pos += 3;
        ++cp1252_cnt;
      } else if (c == 0x8c) {
        // latin capital letter OE (Œ)
        utf8[out_pos] = 0xc5;
        utf8[out_pos + 1] = 0x92;
        out_pos += 2;
        ++cp1252_cnt;
      } else if (c == 0x8d) {
        // no conversion available
        ++other_cnt;
      } else if (c == 0x8e) {
        // latin capital letter Ž
        utf8[out_pos] = 0xc5;
        utf8[out_pos + 1] = 0xbd;
        out_pos += 2;
        ++cp1252_cnt;
      } else if (c == 0x8f) {
        // no conversion available
        ++other_cnt;
      } else if (c == 0x90) {
        // no conversion available
        ++other_cnt;
      } else if (c == 0x91) {
        // left single quotation mark (‘)
        utf8[out_pos] = 0xe2;
        utf8[out_pos + 1] = 0x80;
        utf8[out_pos + 2] = 0x98;
        out_pos += 3;
        ++cp1252_cnt;
      } else if (c == 0x92) {
        // right single quotation mark (’)
        utf8[out_pos] = 0xe2;
        utf8[out_pos + 1] = 0x80;
        utf8[out_pos + 2] = 0x99;
        out_pos += 3;
        ++cp1252_cnt;
      } else if (c == 0x93) {
        // left double quotation mark (“)
        utf8[out_pos] = 0xe2;
        utf8[out_pos + 1] = 0x80;
        utf8[out_pos + 2] = 0x9c;
        out_pos += 3;
        ++cp1252_cnt;
      } else if (c == 0x94) {
        // right double quotation mark (”)
        utf8[out_pos] = 0xe2;
        utf8[out_pos + 1] = 0x80;
        utf8[out_pos + 2] = 0x9d;
        out_pos += 3;
        ++cp1252_cnt;
      } else if (c == 0x95) {
        // bullet (•)
        utf8[out_pos] = 0xe2;
        utf8[out_pos + 1] = 0x80;
        utf8[out_pos + 2] = 0xa2;
        out_pos += 3;
        ++cp1252_cnt;
      } else if (c == 0x96) {
        // en dash (–)
        utf8[out_pos] = 0xe2;
        utf8[out_pos + 1] = 0x80;
        utf8[out_pos + 2] = 0x93;
        out_pos += 3;
        ++cp1252_cnt;
      } else if (c == 0x97) {
        // em dash (—)
        utf8[out_pos] = 0xe2;
        utf8[out_pos + 1] = 0x80;
        utf8[out_pos + 2] = 0x94;
        out_pos += 3;
        ++cp1252_cnt;
      } else if (c == 0x98) {
        // small tilde (˜)
        utf8[out_pos] = 0xcb;
        utf8[out_pos + 1] = 0x9c;
        out_pos += 2;
        ++cp1252_cnt;
      } else if (c == 0x99) {
        // trade mark sign (™)
        utf8[out_pos] = 0xe2;
        utf8[out_pos + 1] = 0x84;
        utf8[out_pos + 2] = 0xa2;
        out_pos += 3;
        ++cp1252_cnt;
      } else if (c == 0x9a) {
        // latin small letter s with caron (š)
        utf8[out_pos] = 0xc5;
        utf8[out_pos + 1] = 0xa1;
        out_pos += 2;
        ++cp1252_cnt;
      } else if (c == 0x9b) {
        // single right-pointing angle quotation mark (›)
        utf8[out_pos] = 0xe2;
        utf8[out_pos + 1] = 0x80;
        utf8[out_pos + 2] = 0xba;
        out_pos += 3;
        ++cp1252_cnt;
      } else if (c == 0x9c) {
        // latin small ligature oe (œ)
        utf8[out_pos] = 0xc5;
        utf8[out_pos + 1] = 0x93;
        out_pos += 2;
        ++cp1252_cnt;
      } else if (c == 0x9d) {
        // no conversion available
        ++other_cnt;
      } else if (c == 0x9e) {
        // latin small letter z with caron (ž)
        utf8[out_pos] = 0xc5;
        utf8[out_pos + 1] = 0xbe;
        out_pos += 2;
        ++cp1252_cnt;
      } else if (c == 0x9f) {
        // latin capital letter Y with diaeresis (Ÿ)
        utf8[out_pos] = 0xc5;
        utf8[out_pos + 1] = 0xb8;
        out_pos += 2;
        ++cp1252_cnt;
      }
      // end cp-1252 character
    } else if (c >= 0xa0 && c < 0xc0) {
      // 8-bit character to be converted to two-byte utf-8
      utf8[out_pos] = 0xc2;
      utf8[out_pos + 1] = c;
      out_pos += 2;
      ++iso_8859_1_cnt;
    } else if (c >= 0xc0 && c <= 0xff) {
      // 8-bit character to be converted to two-byte utf-8
      utf8[out_pos] = 0xc3;
      utf8[out_pos + 1] = c - 0x40;
      out_pos += 2;
      ++iso_8859_1_cnt;
    }
    ++in_pos;
  }
  // set encoding
  *encoding = ENCODING_ASCII;
  if (other_cnt > 0) {
    *encoding = ENCODING_UNDEFINED;
  } else if ((cp1252_cnt + iso_8859_1_cnt) * 4 > ascii_cnt * 3) {
    // too much special characters to be seen as a valid string
    *encoding = ENCODING_UNDEFINED;
  } else if (cp1252_cnt > 0) {
    *encoding = ENCODING_CP1252;
  } else if (iso_8859_1_cnt > 0) {
    *encoding = ENCODING_ISO_8859_1;
  }
  utf8[out_pos] = '\0';
  return utf8;
}

/**
 * @brief get the encoding string for the given encoding
 *        returns 0 if encoding is undefined
 */
const char *get_encoding(int encoding) {
  switch (encoding) {
    case ENCODING_ASCII:
      return "ASCII";
    case ENCODING_ISO_8859_1:
      return "ISO-8859-1";
    case ENCODING_CP1252:
      return "CP1252";
    case ENCODING_UTF8:
      return "UTF-8";
    case ENCODING_UTF16_BE:
      return "UTF-16BE";
    case ENCODING_UTF16_LE:
      return "UTF-16LE";
    case ENCODING_UTF32_BE:
      return "UTF-32BE";
    case ENCODING_UTF32_LE:
      return "UTF-32LE";
    default:
      return "UNDEFINED";
  }
}
