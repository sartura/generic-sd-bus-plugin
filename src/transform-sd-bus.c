/*
 * @file generic_sd-bus.c
 * @authors Borna Blazevic <borna.blazevic@sartura.hr> Luka Paulic <luka.paulic@sartura.hr>
 *
 * @brief Implements sd-bus message parsing
 *
 * @copyright
 * Copyright (C) 2020 Deutsche Telekom AG.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/*=========================Includes===========================================*/
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include <sysrepo.h>
#include <sysrepo/values.h>

#include <systemd/sd-bus-protocol.h>
#include <systemd/sd-bus.h>

#include "transform-sd-bus.h"

// bus argument iterator structure
typedef struct bus_argument_iterator_s {
	const char *arguments;
	size_t arguments_offset;

	char **argument_array;
	size_t argument_array_size;

	size_t argument_array_element_size;
} bus_argument_iterator_t;

int bus_message_encode(const char *signature, const char *arguments, sd_bus_message *m);
int bus_message_decode(sd_bus_message *m, char **arguments);
static int bus_message_encode_recursive(const char *signature, bus_argument_iterator_t *iterator, sd_bus_message *m);
static int boolean_parse(const char *string_value, int *boolean_value);
static int bracket_close_find(const char *bracket_open, size_t *bracket_close_offset);
static int append_argument(bool is_argument_a_string, const char *argument_to_append, char **arguments_string);

static int bus_argument_iterator_create(bus_argument_iterator_t **iterator, const char *argument);
static int bus_argument_iterator_next(bus_argument_iterator_t *iterator, const char **argument);
static void bus_argument_iterator_destroy(bus_argument_iterator_t *iterator);

int bus_message_encode(const char *signature, const char *arguments, sd_bus_message *m)
{
	// TYPES STRING GRAMMAR
	//            types ::= complete_type*
	//            complete_type ::= basic_type | variant | structure | array | dictionary
	//            basic_type ::= "y" | "n" | "q" | "u" | "i" | "x" | "t" | "d" | "b" | "h" |"s" | "o" | "g"
	//            variant ::= "v"
	//            structure ::= "(" complete_type+ ")"
	//            array ::= "a" complete_type
	//            dictionary ::= "a" dict_entry
	//            dict_entry ::= "{" basic_type complete_type "}"

	int error = 0;
	bus_argument_iterator_t *argument_iterator = NULL;

	error = bus_argument_iterator_create(&argument_iterator, arguments);
	if (error < 0) {
		goto out;
	}

	error = bus_message_encode_recursive(signature, argument_iterator, m);
	if (error < 0) {
		goto out;
	}

out:
	bus_argument_iterator_destroy(argument_iterator);
	return (error < 0) ? error : 0;
}

static int bus_message_encode_recursive(const char *signature, bus_argument_iterator_t *iterator, sd_bus_message *m)
{
	int error = 0;
	char type = 0;
	const char *argument_next = NULL;
	int boolean_value = 0;
	size_t bracket_close_offset = 0;
	char contents_type[SD_BUS_MAXIMUM_SIGNATURE_LENGTH + 1] = {0};
	size_t array_size = 0;

	while ((type = *signature)) {
		switch (type) {
			case SD_BUS_TYPE_BOOLEAN:
				error = bus_argument_iterator_next(iterator, &argument_next);
				if (error < 0) {
					goto out;
				}

				error = boolean_parse(argument_next, &boolean_value);
				if (error < 0) {
					goto out;
				}

				error = sd_bus_message_append_basic(m, type, &boolean_value);
				if (error < 0) {
					goto out;
				}

				signature++;
				break;

			case SD_BUS_TYPE_BYTE:
				error = bus_argument_iterator_next(iterator, &argument_next);
				if (error < 0) {
					goto out;
				}

				error = sd_bus_message_append_basic(m, type, &(uint8_t){(uint8_t) strtoul(argument_next, (char **) NULL, 10)}); // TODO: add an error or range check here?
				if (error < 0) {
					goto out;
				}

				signature++;
				break;

			case SD_BUS_TYPE_INT16:
				error = bus_argument_iterator_next(iterator, &argument_next);
				if (error < 0) {
					goto out;
				}

				error = sd_bus_message_append_basic(m, type, &(int16_t){(int16_t) strtol(argument_next, (char **) NULL, 10)}); // TODO: add an error or range check here?
				if (error < 0) {
					goto out;
				}

				signature++;
				break;

			case SD_BUS_TYPE_UINT16:
				error = bus_argument_iterator_next(iterator, &argument_next);
				if (error < 0) {
					goto out;
				}

				error = sd_bus_message_append_basic(m, type, &(uint16_t){(uint16_t) strtoul(argument_next, (char **) NULL, 10)}); // TODO: add an error or range check here?
				if (error < 0) {
					goto out;
				}

				signature++;
				break;

			case SD_BUS_TYPE_INT32:
			case SD_BUS_TYPE_UNIX_FD:
				error = bus_argument_iterator_next(iterator, &argument_next);
				if (error < 0) {
					goto out;
				}

				error = sd_bus_message_append_basic(m, type, &(int32_t){(int32_t) strtol(argument_next, (char **) NULL, 10)}); // TODO: add an error or range check here?
				if (error < 0) {
					goto out;
				}

				signature++;
				break;

			case SD_BUS_TYPE_UINT32:
				error = bus_argument_iterator_next(iterator, &argument_next);
				if (error < 0) {
					goto out;
				}

				error = sd_bus_message_append_basic(m, type, &(uint32_t){(uint32_t) strtoul(argument_next, (char **) NULL, 10)}); // TODO: add an error or range check here?
				if (error < 0) {
					goto out;
				}

				signature++;
				break;

			case SD_BUS_TYPE_INT64:
				error = bus_argument_iterator_next(iterator, &argument_next);
				if (error < 0) {
					goto out;
				}

				error = sd_bus_message_append_basic(m, type, &(int64_t){(int64_t) strtol(argument_next, (char **) NULL, 10)}); // TODO: add an error or range check here?
				if (error < 0) {
					goto out;
				}

				signature++;
				break;

			case SD_BUS_TYPE_UINT64:
				error = bus_argument_iterator_next(iterator, &argument_next);
				if (error < 0) {
					goto out;
				}

				error = sd_bus_message_append_basic(m, type, &(uint64_t){(uint64_t) strtoul(argument_next, (char **) NULL, 10)}); // TODO: add an error or range check here?
				if (error < 0) {
					goto out;
				}

				signature++;
				break;

			case SD_BUS_TYPE_DOUBLE:
				error = bus_argument_iterator_next(iterator, &argument_next);
				if (error < 0) {
					goto out;
				}

				error = sd_bus_message_append_basic(m, type, &(double){strtod(argument_next, (char **) NULL)}); // TODO: add an error or range check here?
				if (error < 0) {
					goto out;
				}

				signature++;
				break;

			case SD_BUS_TYPE_STRING:
			case SD_BUS_TYPE_OBJECT_PATH:
			case SD_BUS_TYPE_SIGNATURE:
				error = bus_argument_iterator_next(iterator, &argument_next);
				if (error < 0) {
					goto out;
				}

				error = sd_bus_message_append_basic(m, type, argument_next);
				if (error < 0) {
					goto out;
				}

				signature++;
				break;

			case SD_BUS_TYPE_VARIANT:
				error = bus_argument_iterator_next(iterator, &argument_next);
				if (error < 0) {
					goto out;
				}

				error = sd_bus_message_open_container(m, type, argument_next);
				if (error < 0) {
					goto out;
				}

				error = bus_message_encode_recursive(argument_next, iterator, m);
				if (error < 0) {
					goto out;
				}

				error = sd_bus_message_close_container(m);
				if (error < 0) {
					goto out;
				}

				signature++;
				break;

			case SD_BUS_TYPE_STRUCT_BEGIN:
			case SD_BUS_TYPE_DICT_ENTRY_BEGIN:
				error = bracket_close_find(signature, &bracket_close_offset);
				if (error < 0) {
					goto out;
				}
				memcpy(contents_type, signature + 1, bracket_close_offset - 1);

				error = sd_bus_message_open_container(m, (type == SD_BUS_TYPE_STRUCT_BEGIN) ? SD_BUS_TYPE_STRUCT : SD_BUS_TYPE_DICT_ENTRY, contents_type);
				if (error < 0) {
					goto out;
				}

				error = bus_message_encode_recursive(contents_type, iterator, m);
				if (error < 0) {
					goto out;
				}

				error = sd_bus_message_close_container(m);
				if (error < 0) {
					goto out;
				}

				signature += bracket_close_offset + 1;

				break;

			case SD_BUS_TYPE_ARRAY:
				error = bus_argument_iterator_next(iterator, &argument_next);
				if (error < 0) {
					goto out;
				}

				if (*(signature + 1) == SD_BUS_TYPE_STRUCT_BEGIN || *(signature + 1) == SD_BUS_TYPE_DICT_ENTRY_BEGIN) {
					error = bracket_close_find(signature + 1, &bracket_close_offset);
					if (error < 0) {
						goto out;
					}
					memcpy(contents_type, signature + 1, bracket_close_offset + 1);
				} else {
					memcpy(contents_type, signature + 1, 1);
				}

				error = sd_bus_message_open_container(m, type, contents_type);
				if (error < 0) {
					goto out;
				}

				array_size = strtoul(argument_next, (char **) NULL, 10); // TODO: check return value of strtol

				for (size_t i = 0; i < array_size; i++) {
					error = bus_message_encode_recursive(contents_type, iterator, m);
					if (error < 0) {
						goto out;
					}
				}

				error = sd_bus_message_close_container(m);
				if (error < 0) {
					goto out;
				}

				if (*(signature + 1) == SD_BUS_TYPE_STRUCT_BEGIN || *(signature + 1) == SD_BUS_TYPE_DICT_ENTRY_BEGIN) {
					signature += bracket_close_offset + 2;
				} else {
					signature += 2;
				}

				break;

			default:
				error = -EINVAL;
				goto out;
		}
	}

out:

	return (error < 0) ? error : 0;
}

static int bus_argument_iterator_create(bus_argument_iterator_t **iterator, const char *arguments)
{
	if (iterator == NULL) {
		return -1;
	}

	if (arguments == NULL) {
		return -1;
	}

	*iterator = calloc(1, sizeof(bus_argument_iterator_t));
	(*iterator)->arguments = arguments;
	(*iterator)->arguments_offset = 0;
	(*iterator)->argument_array = NULL;
	(*iterator)->argument_array_size = 0;
	(*iterator)->argument_array_element_size = 0;

	return 0;
}

static int bus_argument_iterator_next(bus_argument_iterator_t *iterator, const char **argument)
{
	bool argument_is_quoted = false;

	if (iterator == NULL) {
		return -1;
	}

	if (argument == NULL) {
		return -1;
	}

	if (*iterator->arguments == '\0') {
		return -1;
	}

	iterator->argument_array = realloc(iterator->argument_array, sizeof(char *) * (iterator->argument_array_size + 1));
	iterator->argument_array[iterator->argument_array_size] = NULL;
	iterator->argument_array_element_size = 0;
	while (iterator->arguments_offset < strlen(iterator->arguments)) {
		if (iterator->arguments[iterator->arguments_offset] == '\\') {
			iterator->argument_array[iterator->argument_array_size] = realloc(iterator->argument_array[iterator->argument_array_size], sizeof(char) * (iterator->argument_array_element_size + 1));
			iterator->argument_array[iterator->argument_array_size][iterator->argument_array_element_size++] = iterator->arguments[iterator->arguments_offset + 1];
			iterator->arguments_offset += 2;
		} else if (argument_is_quoted == false && iterator->arguments[iterator->arguments_offset] == '"') {
			argument_is_quoted = true;
			iterator->arguments_offset += 1;
		} else if (argument_is_quoted == true && iterator->arguments[iterator->arguments_offset] == '"') {
			iterator->arguments_offset += 2;
			break;
		} else if (argument_is_quoted == false && iterator->arguments[iterator->arguments_offset] == ' ') {
			iterator->arguments_offset += 1;
			break;
		} else {
			iterator->argument_array[iterator->argument_array_size] = realloc(iterator->argument_array[iterator->argument_array_size], sizeof(char) * (iterator->argument_array_element_size + 1));
			iterator->argument_array[iterator->argument_array_size][iterator->argument_array_element_size++] = iterator->arguments[iterator->arguments_offset];
			iterator->arguments_offset += 1;
		}
	}
	iterator->argument_array[iterator->argument_array_size] = realloc(iterator->argument_array[iterator->argument_array_size], sizeof(char) * (iterator->argument_array_element_size + 1));
	iterator->argument_array[iterator->argument_array_size][iterator->argument_array_element_size++] = '\0';
	*argument = iterator->argument_array[iterator->argument_array_size++];

	return 0;
}

static void bus_argument_iterator_destroy(bus_argument_iterator_t *iterator)
{
	if (iterator == NULL) {
		return;
	}

	for (size_t i = 0; i < iterator->argument_array_size; i++) {
		free(iterator->argument_array[i]);
	}
	free(iterator->argument_array);

	free(iterator);
}

static int boolean_parse(const char *string_value, int *boolean_value)
{
	if (string_value == NULL) {
		return -1;
	}

	if (boolean_value == NULL) {
		return -1;
	}

	if (strcmp(string_value, "1") == 0 ||
		strcmp(string_value, "yes") == 0 ||
		strcmp(string_value, "y") == 0 ||
		strcmp(string_value, "true") == 0 ||
		strcmp(string_value, "t") == 0 ||
		strcmp(string_value, "on") == 0) {
		*boolean_value = 1;

		return 0;
	}

	if (strcmp(string_value, "0") == 0 ||
		strcmp(string_value, "no") == 0 ||
		strcmp(string_value, "n") == 0 ||
		strcmp(string_value, "false") == 0 ||
		strcmp(string_value, "f") == 0 ||
		strcmp(string_value, "off") == 0) {
		*boolean_value = 0;

		return 0;
	}

	return -1;
}

static int bracket_close_find(const char *bracket_open, size_t *bracket_close_offset)
{
	const char *bracket_close = NULL;
	size_t bracket_counter = 0;

	if (bracket_open && bracket_open[0] == '\0' && bracket_open[0] != SD_BUS_TYPE_STRUCT_BEGIN && bracket_open[0] != SD_BUS_TYPE_DICT_ENTRY_BEGIN) {
		return -1;
	}

	bracket_close = bracket_open + 1;
	bracket_counter = 1;

	for (; bracket_close; bracket_close++) {
		if (*bracket_close == SD_BUS_TYPE_STRUCT_BEGIN || *bracket_close == SD_BUS_TYPE_DICT_ENTRY_BEGIN) {
			++bracket_counter;
			continue;
		}

		if (*bracket_close == SD_BUS_TYPE_STRUCT_END || *bracket_close == SD_BUS_TYPE_DICT_ENTRY_END) {
			if (--bracket_counter) {
				continue;
			} else {
				break;
			}
		}
	}

	if (bracket_counter) {
		return -1;
	}

	*bracket_close_offset = (size_t)(bracket_close - bracket_open);
	if (*bracket_close_offset > SD_BUS_MAXIMUM_SIGNATURE_LENGTH) {
		return -1;
	}

	return 0;
}

int bus_message_decode(sd_bus_message *m, char **arguments)
{
	int error = 0;
	char type = 0;
	const char *contents = NULL;
	int buffer_size = 0;
	char *string_representation_of_argument = NULL;
	uint8_t argument_byte = 0;
	int argument_boolean = 0;
	int16_t argument_int16 = 0;
	uint16_t argument_uint16 = 0;
	int32_t argument_int32 = 0;
	uint32_t argument_uint32 = 0;
	int64_t argument_int64 = 0;
	uint64_t argument_uint64 = 0;
	double argument_double = 0;
	const char *argument_string = NULL;
	int argument_fd = 0;
	char *arguments_tmp = NULL;
	int count = 0;
	static bool is_array_loop = false;
	bool temp;

	while (sd_bus_message_peek_type(m, &type, &contents) > 0) {
		switch (type) {
			case SD_BUS_TYPE_BYTE:
				error = sd_bus_message_read_basic(m, type, &argument_byte);
				if (error < 0)
					goto error_out;

				buffer_size = snprintf(NULL, 0, "%u", argument_byte);
				string_representation_of_argument = calloc(1, (size_t) buffer_size + 1);
				error = snprintf(string_representation_of_argument, (size_t) buffer_size + 1, "%u", argument_byte);
				if (error < 0)
					goto error_out;

				error = append_argument(false, string_representation_of_argument, arguments);
				if (error < 0)
					goto error_out;

				free(string_representation_of_argument);
				string_representation_of_argument = NULL;

				break;

			case SD_BUS_TYPE_BOOLEAN:
				error = sd_bus_message_read_basic(m, type, &argument_boolean);
				if (error < 0)
					goto error_out;

				buffer_size = snprintf(NULL, 0, "%d", argument_boolean);
				string_representation_of_argument = calloc(1, (size_t) buffer_size + 1);
				error = snprintf(string_representation_of_argument, (size_t) buffer_size + 1, "%d", argument_boolean);
				if (error < 0)
					goto error_out;

				error = append_argument(false, string_representation_of_argument, arguments);
				if (error < 0)
					goto error_out;

				free(string_representation_of_argument);
				string_representation_of_argument = NULL;

				break;

			case SD_BUS_TYPE_INT16:
				error = sd_bus_message_read_basic(m, type, &argument_int16);
				if (error < 0)
					goto error_out;

				buffer_size = snprintf(NULL, 0, "%d", argument_int16);
				string_representation_of_argument = calloc(1, (size_t) buffer_size + 1);
				error = snprintf(string_representation_of_argument, (size_t) buffer_size + 1, "%d", argument_int16);
				if (error < 0)
					goto error_out;

				error = append_argument(false, string_representation_of_argument, arguments);
				if (error < 0)
					goto error_out;

				free(string_representation_of_argument);
				string_representation_of_argument = NULL;

				break;

			case SD_BUS_TYPE_UINT16:
				error = sd_bus_message_read_basic(m, type, &argument_uint16);
				if (error < 0)
					goto error_out;

				buffer_size = snprintf(NULL, 0, "%u", argument_uint16);
				string_representation_of_argument = calloc(1, (size_t) buffer_size + 1);
				error = snprintf(string_representation_of_argument, (size_t) buffer_size + 1, "%u", argument_uint16);
				if (error < 0)
					goto error_out;

				error = append_argument(false, string_representation_of_argument, arguments);
				if (error < 0)
					goto error_out;

				free(string_representation_of_argument);
				string_representation_of_argument = NULL;

				break;

			case SD_BUS_TYPE_INT32:
				error = sd_bus_message_read_basic(m, type, &argument_int32);
				if (error < 0)
					goto error_out;

				buffer_size = snprintf(NULL, 0, "%d", argument_int32);
				string_representation_of_argument = calloc(1, (size_t) buffer_size + 1);
				error = snprintf(string_representation_of_argument, (size_t) buffer_size + 1, "%d", argument_int32);
				if (error < 0)
					goto error_out;

				error = append_argument(false, string_representation_of_argument, arguments);
				if (error < 0)
					goto error_out;

				free(string_representation_of_argument);
				string_representation_of_argument = NULL;

				break;

			case SD_BUS_TYPE_UINT32:
				error = sd_bus_message_read_basic(m, type, &argument_uint32);
				if (error < 0)
					goto error_out;

				buffer_size = snprintf(NULL, 0, "%u", argument_uint32);
				string_representation_of_argument = calloc(1, (size_t) buffer_size + 1);
				error = snprintf(string_representation_of_argument, (size_t) buffer_size + 1, "%u", argument_uint32);
				if (error < 0)
					goto error_out;

				error = append_argument(false, string_representation_of_argument, arguments);
				if (error < 0)
					goto error_out;

				free(string_representation_of_argument);
				string_representation_of_argument = NULL;

				break;

			case SD_BUS_TYPE_INT64:
				error = sd_bus_message_read_basic(m, type, &argument_int64);
				if (error < 0)
					goto error_out;

				buffer_size = snprintf(NULL, 0, "%ld", argument_int64);
				string_representation_of_argument = calloc(1, (size_t) buffer_size + 1);
				error = snprintf(string_representation_of_argument, (size_t) buffer_size + 1, "%ld", argument_int64);
				if (error < 0)
					goto error_out;

				error = append_argument(false, string_representation_of_argument, arguments);
				if (error < 0)
					goto error_out;

				free(string_representation_of_argument);
				string_representation_of_argument = NULL;

				break;

			case SD_BUS_TYPE_UINT64:
				error = sd_bus_message_read_basic(m, type, &argument_uint64);
				if (error < 0)
					goto error_out;

				buffer_size = snprintf(NULL, 0, "%lu", argument_uint64);
				string_representation_of_argument = calloc(1, (size_t) buffer_size + 1);
				error = snprintf(string_representation_of_argument, (size_t) buffer_size + 1, "%lu", argument_uint64);
				if (error < 0)
					goto error_out;

				error = append_argument(false, string_representation_of_argument, arguments);
				if (error < 0)
					goto error_out;

				free(string_representation_of_argument);
				string_representation_of_argument = NULL;

				break;

			case SD_BUS_TYPE_DOUBLE:
				error = sd_bus_message_read_basic(m, type, &argument_double);
				if (error < 0)
					goto error_out;

				buffer_size = snprintf(NULL, 0, "%g", argument_double);
				string_representation_of_argument = calloc(1, (size_t) buffer_size + 1);
				error = snprintf(string_representation_of_argument, (size_t) buffer_size + 1, "%g", argument_double);
				if (error < 0)
					goto error_out;

				error = append_argument(false, string_representation_of_argument, arguments);
				if (error < 0)
					goto error_out;

				free(string_representation_of_argument);
				string_representation_of_argument = NULL;

				break;

			case SD_BUS_TYPE_STRING:
			case SD_BUS_TYPE_OBJECT_PATH:
			case SD_BUS_TYPE_SIGNATURE:
				error = sd_bus_message_read_basic(m, type, &argument_string);
				if (error < 0)
					goto error_out;

				error = append_argument(true, argument_string, arguments);
				if (error < 0)
					goto error_out;

				break;

			case SD_BUS_TYPE_UNIX_FD:
				error = sd_bus_message_read_basic(m, type, &argument_fd);
				if (error < 0)
					goto error_out;

				buffer_size = snprintf(NULL, 0, "%d", argument_fd);
				string_representation_of_argument = calloc(1, (size_t) buffer_size + 1);
				error = snprintf(string_representation_of_argument, (size_t) buffer_size + 1, "%d", argument_fd);
				if (error < 0)
					goto error_out;

				error = append_argument(false, string_representation_of_argument, arguments);
				if (error < 0)
					goto error_out;

				free(string_representation_of_argument);
				string_representation_of_argument = NULL;

				break;

			case SD_BUS_TYPE_VARIANT:
				error = sd_bus_message_enter_container(m, type, contents);
				if (error < 0)
					goto error_out;

				error = append_argument(false, contents, arguments);
				if (error < 0)
					goto error_out;

				free(arguments_tmp);
				arguments_tmp = NULL;

				temp = is_array_loop;
				is_array_loop = false;
				error = bus_message_decode(m, &arguments_tmp);
				if (error < 0)
					goto error_out;
				is_array_loop = temp;

				error = append_argument(false, arguments_tmp, arguments);
				if (error < 0)
					goto error_out;

				error = sd_bus_message_exit_container(m);
				if (error < 0)
					goto error_out;

				break;

			case SD_BUS_TYPE_ARRAY:
				count = 0;

				error = sd_bus_message_enter_container(m, type, contents);
				if (error < 0)
					goto error_out;

				free(arguments_tmp);
				arguments_tmp = NULL;
				while (sd_bus_message_at_end(m, false) == 0) {
					is_array_loop = true;
					error = bus_message_decode(m, &arguments_tmp);
					if (error < 0)
						goto error_out;

					count++;
				}

				is_array_loop = false;

				buffer_size = snprintf(NULL, 0, "%d", count);
				string_representation_of_argument = calloc(1, (size_t) buffer_size + 1);
				error = snprintf(string_representation_of_argument, (size_t) buffer_size + 1, "%d", count);
				if (error < 0)
					goto error_out;

				error = append_argument(false, string_representation_of_argument, arguments);
				if (error < 0)
					goto error_out;

				free(string_representation_of_argument);
				string_representation_of_argument = NULL;

				if (count > 0) {
					error = append_argument(false, arguments_tmp, arguments);
					if (error < 0)
						goto error_out;
				}

				error = sd_bus_message_exit_container(m);
				if (error < 0)
					goto error_out;

				break;

			case SD_BUS_TYPE_DICT_ENTRY:
			case SD_BUS_TYPE_STRUCT:
				error = sd_bus_message_enter_container(m, type, contents);
				if (error < 0)
					goto error_out;

				free(arguments_tmp);
				arguments_tmp = NULL;

				while (sd_bus_message_at_end(m, false) == 0) {
					temp = is_array_loop;
					is_array_loop = false;
					error = bus_message_decode(m, &arguments_tmp);
					if (error < 0)
						goto error_out;
					is_array_loop = temp;
				}

				error = append_argument(false, arguments_tmp, arguments);
				if (error < 0)
					goto error_out;

				error = sd_bus_message_exit_container(m);
				if (error < 0)
					goto error_out;

				break;

			default:
				error = -EINVAL;
				goto error_out;
		}

		if (is_array_loop) {
			break;
		}
	}

	goto out;

error_out:
	free(*arguments);

out:
	free(string_representation_of_argument);
	string_representation_of_argument = NULL;
	free(arguments_tmp);
	arguments_tmp = NULL;

	return (error < 0) ? error : 0;
}

static int append_argument(bool is_argument_a_string, const char *argument_to_append, char **arguments_string)
{
	if (argument_to_append == NULL) {
		return -1;
	}

	if (arguments_string == NULL) {
		return -1;
	}

	if (*arguments_string == NULL) {
		if (is_argument_a_string) {
			*arguments_string = calloc(1, strlen("\"") + strlen(argument_to_append) + strlen("\"") + 1);
			memcpy(*arguments_string + strlen(*arguments_string), "\"", strlen("\"") + 1);
			memcpy(*arguments_string + strlen(*arguments_string), argument_to_append, strlen(argument_to_append) + 1);
			memcpy(*arguments_string + strlen(*arguments_string), "\"", strlen("\"") + 1);
		} else {
			*arguments_string = calloc(1, strlen(argument_to_append) + 1);
			memcpy(*arguments_string, argument_to_append, strlen(argument_to_append) + 1);
		}
	} else {
		if (is_argument_a_string) {
			*arguments_string = realloc(*arguments_string, strlen(*arguments_string) + strlen(" ") + strlen("\"") + strlen(argument_to_append) + strlen("\"") + 1);
			memcpy(*arguments_string + strlen(*arguments_string), " ", strlen(" ") + 1);
			memcpy(*arguments_string + strlen(*arguments_string), "\"", strlen("\"") + 1);
			memcpy(*arguments_string + strlen(*arguments_string), argument_to_append, strlen(argument_to_append) + 1);
			memcpy(*arguments_string + strlen(*arguments_string), "\"", strlen("\"") + 1);
		} else {
			*arguments_string = realloc(*arguments_string, strlen(*arguments_string) + strlen(" ") + strlen(argument_to_append) + 1);
			memcpy(*arguments_string + strlen(*arguments_string), " ", strlen(" ") + 1);
			memcpy(*arguments_string + strlen(*arguments_string), argument_to_append, strlen(argument_to_append) + 1);
		}
	}

	return 0;
}
