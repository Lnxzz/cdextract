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

#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <stdbool.h>
#include <stdint.h>

/*

For more information about JSON see: https://www.json.org/json-en.html

JSON Grammar:
json        : element
value       : object | array | string | number | "true" | "false" | "null"
object      : '{' ws '}' | '{' members '}'
members     : member | member ',' members
member      : ws string ws ':' element
array       : '[' ws ']' | '[' elements ']'
elements    : element | element ',' elements
element     : ws value ws
string      : '"' characters '"'
characters  :   "" | character characters
character   : '0020'..'10FFFF' - '"' - '\' | '\' escape
escape      : '"' | '\' | '/' | 'b' | 'f' | 'n' | 'r' | 't' | 'u' hex hex hex hex
hex         : digit | 'A'..'F' | 'a'..'f'
number      : integer fraction exponent
integer     : digit | onenine digits | '-' digit | '-' onenine digits
digits      : digit | digit digits
digit       : '0' | onenine
onenine     : '1'..'9'
fraction    : "" | '.' digits
exponent    : "" | 'E' sign digits | 'e' sign digits
sign        : "" | '+' | '-'
ws          : "" | ' ' ws | '\n' ws | '\r' ws | '\t' ws


Specific members can be selected using expressions similar to json path, 
see: https://goessner.net/articles/JsonPath/ for more information

JSONPath 	Description
 	$ 	    the root object/element
 	@ 	    the current object/element
  . 	    child operator
 	.. 	    recursive descent
	* 	    wildcard. All objects/elements regardless their names.
 	[] 	    array operator.
 	?() 	applies a filter (script) expression.

 */

#define JSON_UTILS_MAX_TOKEN_STR_SIZE 128


typedef enum json_status {
    json_error = -1,            // error state; cannot continue
    json_ok = 0,                // no errors occured
    json_done = 1,              // parsing completed
    json_invalid_data = 2,      // invalid input json data
    json_member_not_found = 3   // selected member not found        
} json_status;

typedef enum json_type {
    json_unknown = -2,
    json_ws = -1,
    json_false = 0,
    json_true = 1,
    json_null = 2,
    json_number = 3,
    json_string = 4,
    json_array = 5,
    json_object = 6,
    json_array_end = 7,
    json_object_end = 8,
    json_name = 9,
    json_separator = 10
} json_type;

typedef struct json_token {
    json_type type;
    json_status status;
    int position;
    int size;
    int level;
    char str[JSON_UTILS_MAX_TOKEN_STR_SIZE];
} json_token;


// Interface functions
#ifdef __cplusplus
"C" {
#endif


/**
 * @brief initialize the json_token state for the given json_data
 * @param token - json token to initialize
 * @return status of the json token
 */
int json_init(json_token *token, char *json_data, int size);

/**
 * @brief reset the json_token state
 * @param token - json token to reset
 * @return status of the json token
 */
int json_reset(json_token *token);

/**
 * @brief set the json_token state from the given token
 * @param token - destination json token
 * @param source_token - source token
 * @return status of the destination json token
 */
int json_set(json_token *token, json_token *source_token);

/**
 * @brief gets an json escaped string
 *        PRE: dest has been allocated with max_size
 * @param dest - escaped output string
 * @param source - unescaped input string
 * @param max_size - maximum size of the escaped output string
 * @return -1 when escaping is not possible, otherwise the length of the destination string
 */
int json_escape(char *dest, char *source, size_t max_size);

/**
 * @brief gets an json unescaped string
 *        PRE: dest has been allocated with max_size
 * @param dest - unescaped output string
 * @param source - escaped input string
 * @param max_size - maximum size of the unescaped output string
 * @return -1 when escaping is not possible, otherwise the length of the destination string
 */
int json_unescape(char *dest, char *source, size_t max_size);

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
int json_find_member(json_token *token, const char *name, int level, char *json_data);

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
int json_find_member_with_value(json_token *token, const char *name, int level, const char *str_value, char *json_data);

/**
 * @brief finds (next) member with given name and level and gets the associated value (restricted to str_max_size) from the json_data
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
int json_find_member_get_value_restricted(json_token *token, const char *name, int level, char *str_value, size_t str_max_size, char *json_data);

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
int json_find_member_get_value(json_token *token, char *name, int level, char **str_value, char *json_data);

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
int json_get_string_restricted(json_token *token, char *name, char *str_value, size_t str_max_size, char *json_data);

/**
 * @brief gets the string value of the next member with the given name in the json_data
 *        note: this version uses dynamic memory allocation
 * @param token - the json token structure to track the parsing status
 * @param name - the name of the member to find
 * @param str_value - pointer to pointer for the output string value
 * @param json_data - the json input data
 * @return -1 if an error occurs, 1 if member has been found, 0 if member has not been found
 */
int json_get_string(json_token *token, char *name, char **str_value, char *json_data);

/**
 * @brief gets the boolean value of the next member with the given name in the json_data
 * @param token - the json token structure to track the parsing status
 * @param name - the name of the member to find
 * @param boolean_value - pointer the output boolean value
 * @param json_data - the json input data
 * @return -1 if an error occurs, 1 if boolean has been found, 0 if member has not been found
 */
int json_get_boolean(json_token *token, char *name, bool *boolean_value, char *json_data);

/**
 * @brief gets the integer value of the next member with the given name in the json_data
 * @param token - the json token structure to track the parsing status
 * @param name - the name of the member to find
 * @param int_value - pointer the output integer value
 * @param json_data - the json input data
 * @return -1 if an error occurs, 1 if integer has been found, 0 if member has not been found
 */
int json_get_integer(json_token *token, char *name, int *int_value, char *json_data);

/**
 * @brief gets the float value of the next member with the given name in the json_data
 * @param token - the json token structure to track the parsing status
 * @param name - the name of the member to find
 * @param float_value - pointer the output float value
 * @param json_data - the json input data
 * @return -1 if an error occurs, 1 if float has been found, 0 if member has not been found
 */
int json_get_float(json_token *token, char *name, float *float_value, char *json_data);

/**
 * @brief gets the unsigned integer value of the next member stored in hexadecimal with the given name in the json_data
 * @param token the json_token structure used to keep track of the parsing state.
 * @param name the name of the member to find.
 * @param uint_value pointer to the unsigned integer value to be filled.
 * @param json_data the json data to parse
 * @return -1 if an error occurs, 1 if integer has been found, 0 if member has not been found
 */
int json_get_hex32(json_token *token, char *name, unsigned int *uint_value, char *json_data);

/**
 * @brief gets the 64-bit unsigned integer value of the next member stored in hexadecimal with the given name in the json_data
 * @param token the json_token structure used to keep track of the parsing state.
 * @param name the name of the member to find.
 * @param uint64_value pointer to the unsigned 64-bit integer value to be filled.
 * @param json_data the json data to parse
 * @return -1 if an error occurs, 1 if integer has been found, 0 if member has not been found
 */
int json_get_hex64(json_token *token, char *name, uint64_t *uint64_value, char *json_data);

/**
 * @brief gets the binary array (stored as hex) of the next member with the given name in the json_data
 *        note: this version uses dynamic memory allocation
 * @param token the json_token structure used to keep track of the parsing state.
 * @param name the name of the member to find.
 * @param bin_value pointer to the pointer of the binary value to be filled.
 * @param bin_size pointer to the size of the binary value.
 * @param json_data the json data to parse
 * @return 1 if member has been found, 0 otherwise
 */
int json_get_hex(json_token *token, char *name, char **bin_value, int *bin_size, char *json_data);

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
int json_select_member(json_token *token, char *json_path, char *str_value, size_t str_max_size, char *json_data);


#ifdef __cplusplus
}
#endif

#endif // JSON_UTILS_H