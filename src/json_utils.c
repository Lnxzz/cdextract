/**************************************************************************

  libcdextract - json utilities 

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
#include <stdbool.h>
#include <string.h>
#include "json_utils.h"


/**
 * @brief initialize the json_token state for the given json_data
 * @param token - json token to initialize
 * @return status of the json token
 */
int json_init(json_token *token, char *json_data, int size) {
  if (json_data == NULL) {
    token->type = json_unknown;
    token->status = json_invalid_data;
    token->size = -1;
  } else {
    token->type = json_unknown;
    token->status = json_ok;
    token->size = size;
    if (size < 0) {
      // determine size if size not specified
      token->size = strlen(json_data);
    }
  }
  token->position = 0;
  token->level = 0;
  token->str[0] = '\0';
  return (int)token->status;
}

/**
 * @brief reset the json_token state
 * @param token - json token to reset
 * @return status of the json token
 */
int json_reset(json_token *token) {
  token->type = json_unknown;
  token->status = json_ok;
  token->position = 0;
  token->level = 0;
  token->str[0] = '\0';
  return (int)token->status;
}

/**
 * @brief set the json_token state from the given token
 * @param token - destination json token
 * @param source_token - source token
 * @return status of the destination json token
 */
int json_set(json_token *token, json_token *source_token) {
  if (source_token == NULL) {
    token->type = json_unknown;
    token->status = json_invalid_data;
    token->position = 0;
    token->size = -1;
    token->level = 0;
    token->str[0] = '\0';
  } else {
    token->type = source_token->type;
    token->status = source_token->status;
    token->position = source_token->position;
    token->size = source_token->size;
    token->level = source_token->level;
    memcpy(&(token->str[0]), &(source_token->str[0]), JSON_UTILS_MAX_TOKEN_STR_SIZE);
  }
  return (int)token->status;
}

/**
 * @brief gets an json escaped string
 *        PRE: dest has been allocated with max_size
 * @param dest - escaped output string
 * @param source - unescaped input string
 * @param max_size - maximum size of the escaped output string
 * @return -1 when escaping is not possible, otherwise the length of the destination string
 */
int json_escape(char *dest, char *source, size_t max_size) {
  int pos = 0;
  int len = 0; 
  int d_pos = 0;

  if (source != NULL) {
    len = strlen(source);
  }

  while (pos < len && d_pos < max_size - 1) {
    if (source[pos] == '\"' || source[pos] == '\\' || source[pos] == '/' || 
        source[pos] == '\b' || source[pos] == '\f' || source[pos] == '\n' || 
        source[pos] == '\r' || source[pos] == '\t') {
      dest[d_pos++] = '\\';
    }
    dest[d_pos++] = source[pos++];
  }

  dest[d_pos] = '\0';

  if (pos < len && d_pos == max_size - 1) {
    return -1;
  }
  return d_pos;
}

/**
 * @brief gets an json unescaped string
 *        PRE: dest has been allocated with max_size
 * @param dest - unescaped output string
 * @param source - escaped input string
 * @param max_size - maximum size of the unescaped output string
 * @return -1 when escaping is not possible, otherwise the length of the destination string
 */
int json_unescape(char *dest, char *source, size_t max_size) {
  int pos = 0;
  int len = 0; 
  int d_pos = 0;

  if (source != NULL) {
    len = strlen(source);
  }

  while (pos < len && d_pos < max_size - 1) {
    if (source[pos] == '\\' && pos + 1 < len) {
      ++pos;
      if (source[pos] == '\"') {
        dest[d_pos] = '\"';
      } else if (source[pos] == '\\') {
        dest[d_pos] = '\\';
      } else if (source[pos] == 'n') {
        dest[d_pos] = '\n';
      } else if (source[pos] == 'r') {
        dest[d_pos] = '\r';
      } else if (source[pos] == 't') {
        dest[d_pos] = '\t';
      } else if (source[pos] == 'b') {
        dest[d_pos] = '\b';
      } else if (source[pos] == 'f') {
        dest[d_pos] = '\f';
      } else {
        dest[d_pos] = '\\';
        ++d_pos;
        dest[d_pos] = source[pos];
      }
    } else {
      dest[d_pos] = source[pos];
    }

    ++pos;
    ++d_pos;
  }

  dest[d_pos] = '\0';
  
  if (pos < len && d_pos == max_size - 1) {
    return -1;
  }
  return strlen(dest);
}

/**
 * @brief get next token from data and update pos
 * @param token - the json token structure to track the parsing status
 * @param data - the json input data
 * @param token_str - pointer to a pointer for the output token string use NULL to store the output token within the json token structure
 * @return -1 if an error occurs, otherwise the number of characters processed
 */
int json_next_token(json_token *token, char *data, char **token_str)
{
  int proceed = 1;
  int cnt = 0;
  int pos = token->position;
  int quoted = 0;
  int escaped = 0;
  float number = 0;

  // check if data is available
  if (data == NULL) {
    return 0;
  }

  // iterate while we have whitespace
  do {

    // reset type to unknown
    token->type = json_unknown;

    // iterate through data till we either reach the end or found a token
    while (pos < token->size && proceed) {

        if (quoted == 0 || quoted == -1) {

            if (data[pos] == ' ' || data[pos] == '\t' || data[pos] == '\r' || 
                data[pos] == '\n' || data[pos] == '\0') {
                // whitespace character
                if (cnt==0) {
                    // discard leading whitespace
                    token->position++;
                    token->type = json_ws;
                } else {
                    // stop when we find trailing whitespace
                    proceed = 0;
                }

            } else if (data[pos] == '{' || data[pos] == '}' || data[pos] == '[' || 
                data[pos] == ']' || data[pos] == ',' || data[pos] == ':') {
                // 'control' character
                if (cnt == 0) {
                    if (quoted == -1) {    
                        // if quoted is -1 and cnt equals 0 then we are should be
                        // (beyond) the end of a quoted empty string
                        token->type = json_string;
                        if (data[pos] == '{' || data[pos] == '}' || data[pos] == '[' || 
                            data[pos] == ']' || data[pos] == ',') {
                            // we will handle the {} []] and , in the next round
                            pos--;
                        }
                    } else if (data[pos] == '{') {
                        token->type = json_object;
                        token->level++;
                    } else if (data[pos] == '}') {
                        token->type = json_object_end;
                        token->level--;
                    } else if (data[pos] == '[') {
                        token->type = json_array;
                        token->level++;
                    } else if (data[pos] == ']') {
                        token->type = json_array_end;
                        token->level--;
                    } else if (data[pos] == ',') {
                        // do nothing: move to next token
                        token->type = json_separator;
                    } else if (data[pos] == ':') {
                        token->type = json_name;
                        //printf("error: empty name should not occur\n");
                    } 
                    
                } else {
                    if (data[pos] == ':') {
                        token->type = json_name;
                    } else if (data[pos] == '}' || data[pos] == ']' || data[pos] == ',') {
                        // we will handle the } ] and , in the next round
                        pos--;
                    }
                }
                proceed = 0;

            } else {
                // 'normal' character
                if (data[pos] == '"') {
                    // start of quoted string
                    token->type = json_string;
                    quoted = 1;
                    // discard start " from token string
                    token->position++;
                } else {
                    // no white space or special character: count on
                    cnt++;
                }
            }
        } else {
            // in quoted string
            if (escaped == 0) {
                // not escaped
                if (data[pos] == '\\') {
                    escaped = 1;
                }
                if (data[pos] == '"') {
                    // end of quoted string
                    quoted = -1;
                    // discard end " from token string
                    cnt--;
                }
            } else {
                // escaped
                // normal case: we are done after first character
                if (data[pos] == 'u') {
                    // special case: hex value uXXXX
                    cnt = cnt + 4;
                }
                // done with escaped data
                escaped = 0;
            }
            // no white space or special character: count on
            cnt++;
        }
        pos++;
    }

    if (token_str == NULL) {
      // no pointer to output token string, we store the token string in the token structure
      if (cnt >= JSON_UTILS_MAX_TOKEN_STR_SIZE - 1) {
        // insufficient space in destination string
        token->status = json_error;
        token->position = pos;
        return -1;
      }

      // copy data to the token string
      memcpy(&(token->str[0]), &(data[token->position]), cnt);
      token->str[cnt] = '\0';
    } else {
      // pointer to output token string available: allocate new string and copy data
      char *tmp = realloc(*token_str, (cnt + 1) * sizeof(char));
      if (tmp == NULL) {
          // handle memory allocation failure
          free(*token_str);
          token->status = json_error;
          token->position = pos;
          return cnt;
      }
      *token_str = tmp;
      memcpy(*token_str, &(data[token->position]), cnt);
      (*token_str)[cnt] = '\0';
    }


    // assign type to token
    if (token->type==json_string || token->type==json_name ||
        token->type==json_object || token->type==json_object_end ||
        token->type==json_array || token->type==json_array_end || 
        token->type==json_separator || token->type==json_ws) {
      // do nothing; already have the correct type
    } else if (strcmp(token->str, "true")==0) {
      token->type = json_true;
    } else if (strcmp(token->str, "false")==0) {
      token->type = json_false;
    } else if (strcmp(token->str, "null")==0) {
      token->type = json_null;
    } else if (pos==token->size) {
      token->status = json_done;
    } else if(sscanf(token->str, "%e", &number) == 1) {
      // at this point, we expected a number and the conversion to number succeeded
      token->type = json_number;
    } else {
      // error: unknown type
      token->status = json_error;
    }

  } while (token->type == json_ws);

  // set new start position in data
  token->position = pos;

  return cnt;
}

/**
 * @brief finds (next) member with given name and level in the json_data
 *        use level=-1 to discard the level check
 *        use level=-2 to ensure token level is at the same or lower (child) level
 * @param token - the json token structure to track the parsing status
 * @param name - the name of the member to find
 * @param level - the token level
 * @param json_data - the json input data
 * @return -1 if an error occurs, 1 if member has been found, 0 if member has not been found
 */
int json_find_member(json_token *token, const char *name, int level, char *json_data) {
  int start_level = token->level;

  while (token->status == json_ok) {
    if (json_next_token(token, json_data, NULL) < 0) {
      return -1;
    }
    if (token->type == json_name && strcmp(token->str, name)==0 && 
        (level == token->level || level == -1 || (start_level <= token->level && level == -2))) {
      return 1;
    }
  }
  return 0;
}

/**
 * @brief finds (next) member with given name, level and value in the json_data
 *        use level=-1 to discard the level check
 *        use level=-2 to ensure token level is at the same or lower (child) level
 * @param token - the json token structure to track the parsing status
 * @param name - the name of the member to find
 * @param level - the token level
 * @param str_value - the string value to find
 * @param json_data - the json input data
 * @return -1 if an error occurs, 1 if selection has been found, 0 otherwise
 */
int json_find_member_with_value(json_token *token, const char *name, int level, const char *str_value, char *json_data) {

  while (token->status == json_ok && json_find_member(token, name, level, json_data) == 1) {
    int name_level = token->level;
    while (token->status == json_ok) {
      if (json_next_token(token, json_data, NULL) < 0) {
        return -1;
      }    
      if ((token->type == json_string || token->type == json_number || token->type == json_null ||
          token->type == json_true || token->type == json_false) 
        && strcmp(token->str, str_value)==0  
        && name_level <= token->level) {
        return 1;
      }
    }
  }
  return 0;
}

/**
 * @brief finds (next) member with given name and level and gets the associated value (limited to str_max_size) from the json_data
 *        use level=-1 to discard the level check
 *        use level=-2 to ensure token level is at the same or lower (child) level
 *        PRE: str_value has been allocated with size str_max_size
 * @param token - the json token structure to track the parsing status
 * @param name - the name of the member to find
 * @param level - the token level
 * @param str_value - pointer to the output string value
 * @param str_max_size - the maximum size of the output string
 * @param json_data - the json input data
 * @return -1 if an error occurs, 1 if member has been found, 0 if member has not been found
 */
int json_find_member_get_value_restricted(json_token *token, const char *name, int level, char *str_value, size_t str_max_size, char *json_data) {
  int name_level, len = 0;
  if (json_find_member(token, name, level, json_data) == 1) {
    name_level = token->level;
    while (token->status == json_ok) {
      len = json_next_token(token, json_data, NULL);
      if (len < 0) {
        return -1;
      }
      if ((token->type == json_string || token->type == json_number || token->type == json_null ||
          token->type == json_true || token->type == json_false)
          && name_level <= token->level) {
        if (len < str_max_size) {
            // found, copy value from token string to the destination string
          memcpy(str_value, &(token->str[0]), len);
          str_value[len] = '\0';
          return 1;
        } else {
          // found, but value does not fit in the destination string
          return -1;
        }
      }
    }
  }
  return 0;    
}

/**
 * @brief finds (next) member with given name, level and value in the json_data
 *        use level=-1 to discard the level check
 *        use level=-2 to ensure token level is at the same or lower (child) level
 * @param token - the json token structure to track the parsing status
 * @param name - the name of the member to find
 * @param level - the token level
 * @param str_value - pointer to pointer for the output string value
 * @param json_data - the json input data
 * @return -1 if an error occurs, 1 if member has been found, 0 otherwise
 */
int json_find_member_get_value(json_token *token, char *name, int level, char **str_value, char *json_data) {
  int name_level, len = 0;
  if (json_find_member(token, name, level, json_data) == 1) {
    name_level = token->level;
    while (token->status == json_ok) {
      len = json_next_token(token, json_data, str_value);
      if (len < 0) {
        return -1;
      }
      if ((token->type == json_string || token->type == json_number || token->type == json_null ||
          token->type == json_true || token->type == json_false)
          && name_level <= token->level) {
        return 1;
      }
    }
  }
  return 0;   
}

/**
 * @brief gets the string value of the next member (restricted to str_max_size) with the given name in the json_data
 *        PRE: str_value has been allocated with size str_max_size
 * @param token - the json token structure to track the parsing status
 * @param name - the name of the member to find
 * @param str_value - pointer to the output string value
 * @param str_max_size - the maximum size of the output string
 * @param json_data - the json input data
 * @return -1 if an error occurs, 1 if member has been found, 0 if member has not been found
 */
int json_get_string_restricted(json_token *token, char *name, char *str_value, size_t str_max_size, char *json_data) {
  return json_find_member_get_value_restricted(token, name, -2, str_value, str_max_size, json_data);
}

/**
 * @brief gets the string value of the next member with the given name in the json_data
 *        note: this version uses dynamic memory allocation
 * @param token - the json token structure to track the parsing status
 * @param name - the name of the member to find
 * @param str_value - pointer to pointer for the output string value
 * @param json_data - the json input data
 * @return -1 if an error occurs, 1 if member has been found, 0 if member has not been found
 */
int json_get_string(json_token *token, char *name, char **str_value, char *json_data) {
  return json_find_member_get_value(token, name, -2, str_value, json_data);
}

/**
 * @brief gets the boolean value of the next member with the given name in the json_data
 * @param token - the json token structure to track the parsing status
 * @param name - the name of the member to find
 * @param boolean_value - pointer the output boolean value
 * @param json_data - the json input data
 * @return -1 if an error occurs, 1 if boolean has been found, 0 if member has not been found
 */
int json_get_boolean(json_token *token, char *name, bool *boolean_value, char *json_data) {

  int found = json_find_member_get_value(token, name, -2, NULL, json_data);

  if (found==1) {
    if (strcmp(token->str, "true") == 0) {
      *boolean_value = 1;
    } else if (strcmp(token->str, "false") == 0) {
      *boolean_value = 0;
    }
  }
  return found;
}

/**
 * @brief gets the integer value of the next member with the given name in the json_data
 * @param token - the json token structure to track the parsing status
 * @param name - the name of the member to find
 * @param int_value - pointer the output integer value
 * @param json_data - the json input data
 * @return -1 if an error occurs, 1 if integer has been found, 0 if member has not been found
 */
int json_get_integer(json_token *token, char *name, int *int_value, char *json_data) {
  int val;
  int found = json_find_member_get_value(token, name, -2, NULL, json_data);

  if (found==1 && sscanf(token->str, "%d", &val) == 1) {
    *int_value = val;
  }
  return found;
}

/**
 * @brief gets the float value of the next member with the given name in the json_data
 * @param token - the json token structure to track the parsing status
 * @param name - the name of the member to find
 * @param float_value - pointer the output float value
 * @param json_data - the json input data
 * @return -1 if an error occurs, 1 if float has been found, 0 if member has not been found
 */
int json_get_float(json_token *token, char *name, float *float_value, char *json_data) {
  float val;
  int found = json_find_member_get_value(token, name, -2, NULL, json_data);

  if (found==1 && sscanf(token->str, "%e", &val) == 1) {
    *float_value = val;
  }
  return found;
}

/**
 * @brief gets the unsigned integer value of the next member stored in hexadecimal with the given name in the json_data
 * @param token the json_token structure used to keep track of the parsing state.
 * @param name the name of the member to find.
 * @param uint_value pointer to the unsigned integer value to be filled.
 * @param json_data the json data to parse
 * @return -1 if an error occurs, 1 if integer has been found, 0 if member has not been found
 */
int json_get_hex32(json_token *token, char *name, unsigned int *uint_value, char *json_data) {
  unsigned int val;
  int found = json_find_member_get_value(token, name, -2, NULL, json_data);

  if (found==1 && sscanf(token->str, "%8x", &val) == 1) {
    *uint_value = val;
  }
  return found;
}

/**
 * @brief gets the 64-bit unsigned integer value of the next member stored in hexadecimal with the given name in the json_data
 * @param token the json_token structure used to keep track of the parsing state.
 * @param name the name of the member to find.
 * @param uint64_value pointer to the unsigned 64-bit integer value to be filled.
 * @param json_data the json data to parse
 * @return -1 if an error occurs, 1 if integer has been found, 0 if member has not been found
 */
int json_get_hex64(json_token *token, char *name, uint64_t *uint64_value, char *json_data) {
  uint64_t val;
  int found = json_find_member_get_value(token, name, -2, NULL, json_data);

  if (found==1 && sscanf(token->str, "%16lx", &val) == 1) {
    *uint64_value = val;
  }
  return found;
}

/**
 * @brief gets the binary array (stored as hex) of the next member with the given name in the json_data
 *        note: this function uses dynamic memory allocation
 * @param token the json_token structure used to keep track of the parsing state.
 * @param name the name of the member to find.
 * @param bin_value pointer to the pointer of the binary value to be filled.
 * @param bin_size pointer to the size of the binary value.
 * @param json_data the json data to parse
 * @return 1 if member has been found, 0 otherwise
 */
int json_get_hex(json_token *token, char *name, char **bin_value, int *bin_size, char *json_data) {
  char *hex_value = malloc((((*bin_size) * 2) + 1) * sizeof(char));
  int found = json_find_member_get_value(token, name, -2, &hex_value, json_data);
  if (found==1) {
    *bin_size = strlen(hex_value) / 2;
    if (*bin_value != NULL) {
      free(*bin_value);
    }
    *bin_value = calloc(*bin_size, sizeof(char));
    for (int i=0; i<*bin_size; i++) {
      sscanf(&hex_value[i*2], "%2hhx", &(*bin_value)[i]);
    }
  }
  free(hex_value);
  return found;
}

/**
 * @brief get next path element from the json path
 *        PRE: path_element has been allocated with size max_path_size
 * @param path_token - the token structure to track the parsing of the path
 * @param json_path - the json path to select the member
 * @param path_element - pointer to the output path element
 * @param max_path_size - the maximum size of the output path element
 * @return -1 if an error occurs, 1 if next element has been found, 0 if member has not been found
 */
int json_next_path_element(json_token *path_token, char *json_path, char *path_element, size_t max_path_size) {
  int pos = path_token->position;
  int found = 0;
  
  // get next path element
  while (found == 0 && pos < path_token->size) {
    if (json_path[pos] == '$' || json_path[pos] == '@' || json_path[pos] == '.' || json_path[pos] == '?' ||
        json_path[pos] == '*' || json_path[pos] == '[' || json_path[pos] == ']') {
      found = 1;
    } else {
      // normal character
    }
    if (found==0 || pos==path_token->position) {
      pos++;
    }
    // '..' indicates recursive decent
    if (pos < path_token->size && json_path[pos-1] == '.' && json_path[pos] == '.') {
      pos++;
    }
  }

  if (pos - path_token->position >= max_path_size - 1) {
    // insufficient space to copy the path element
    return -1;
  }
  // copy data
  memcpy(path_element, &(json_path[path_token->position]), pos - path_token->position);
  path_element[pos - path_token->position] = '\0';
  path_token->position = pos;

  return found;
}

/**
 * @brief selects the member using the given path and gets the associated value from the json_data
 *        PRE: str_value has been allocated with size str_max_size
 * @param token - the json token structure to track the parsing status
 * @param json_path - the json path to select the member
 * @param str_value - pointer to the output string value
 * @param str_max_size - the maximum size of the output string
 * @param json_data - the json input data
 * @return -1 if an error occurs, 1 if member has been found, 0 otherwise
 */
int json_select_member(json_token *token, char *json_path, char *str_value, size_t str_max_size, char *json_data) {
    
  int result = 0;
  int found = 0;
  int level = 0;
  int recursive_decent = 0;

  json_token path_token;
  json_init(&path_token, json_path, -1);

  // empty string
  str_value[0] = '\0';
  
  while (path_token.position < path_token.size) {

    found = json_next_path_element(&path_token, json_path, path_token.str, JSON_UTILS_MAX_TOKEN_STR_SIZE);

    if (strcmp(path_token.str, "$") == 0) {
      // select the root object/element
      json_reset(token);
    } else if (strcmp(path_token.str, "@") == 0) {
      // select the current object/element
    } else if (strcmp(path_token.str, "..") == 0) {
      // recursive decent
      recursive_decent = 1;
    } else if (strcmp(path_token.str, ".") == 0) {
      // select the child element
      level = token->level;
      found = 0;
      while (found == 0 && token->status == json_ok) {
        json_next_token(token, json_data, NULL);
        if (level + 1 == token->level) {
          // child element found
          found = 1;
        }
      }
    } else if (strcmp(path_token.str, "[") == 0) {
      // start of array
      while (token->status == json_ok && token->type != json_array && token->type != json_object) {
        json_next_token(token, json_data, NULL);
      }
      int level = token->level;

      // get array selector
      found = json_next_path_element(&path_token, json_path, path_token.str, JSON_UTILS_MAX_TOKEN_STR_SIZE);

      int selected_item_nr = 0;
      int current_item_nr = 0; //should be part of token aka state
      
      if (strcmp(path_token.str, "*") == 0) {
        // select all elements: we just do the first for now
      } else if(sscanf(path_token.str, "%d", &selected_item_nr) == 1) {
        // select specified item nr
        while (token->status == json_ok && current_item_nr < selected_item_nr) {
          json_next_token(token, json_data, NULL);
          // separator at same level or end array
          if (token->type == json_separator && token->level == level) {
            current_item_nr++;
          } else if (token->level<level) {
            // level to low: item not found
            current_item_nr = selected_item_nr;
          }
        }
      }

      // next path element item must be an end array element
      json_next_path_element(&path_token, json_path, path_token.str, JSON_UTILS_MAX_TOKEN_STR_SIZE);

      // get value if we found the item and it is the last path element (plain value)
      if (path_token.position==path_token.size && current_item_nr == selected_item_nr) {
        result = json_next_token(token, json_data, NULL);
      }

    } else if (strcmp(path_token.str, "[") == 0) {
        // end of array selector
    } else if (strcmp(path_token.str, "?") == 0) {
        // filter (script) expression    } else if (strcmp(path_element, "(") == 0) {
        // start of (script) expression
    } else if (strcmp(path_token.str, ")") == 0) {
        // end of (script) expression
    } else {
      // must be element name
      if (strlen(path_token.str) > 0) {
        int lvl = token->level;
        if (recursive_decent == 1) {
          lvl = -2;
          recursive_decent = 0;
        }
        // get element and value if it is the last element
        if (found==0) {
          result = json_find_member_get_value_restricted(token, path_token.str, lvl, str_value, str_max_size, json_data);
        } else {
          json_find_member(token, path_token.str, lvl, json_data);
        }      
      }
    }
  }

  return result;
}