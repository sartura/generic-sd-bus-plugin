/*
 * @file generic_ubus.c
 * @authors Borna Blazevic <borna.blazevic@sartura.hr> Luka Paulic <luka.paulic@sartura.hr>
 *
 * @brief Implements tha main logic of the generic ubus plugin.
 *        Main functionalities include:
 *          + loading and syncing the startup data store withe the
 *            running data store
 *          + handeling creating, modifying, deleting the ubus object and
 *            ubus method structures according to the configurational data
 *            changes
 *          + retreiving the YANG module state data for a ubus object
 *            that is being monitored
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
#include <transform-sdbus.h>
#include <errno.h>
#include <stdlib.h>

#include <systemd/sd-bus.h>
#include <systemd/sd-bus-protocol.h>
#include <sysrepo.h>
#include <sysrepo/values.h>
#include <common.h>

static int find_next_argument(char **arguments, char *arg);
static int find_next_argument_string(char **arguments, char *arg);
static const char *find_matching_bracket(const char *str);
int append_complete_types_to_message(sd_bus_message *m, const char *signature, char **arguments);
int parse_message_to_string(sd_bus_message *m, char **ret, bool called_from_container);
static int append_value(char **ret, const char *value);
static int append_string(char **ret, const char *value);
static int append_boolean(char **ret, int value);
static int append_sint(char **ret, int64_t value);
static int append_uint(char **ret, uint64_t value);
static int append_double(char **ret, double value);

int parse_message_to_string(sd_bus_message *m, char **ret, bool called_from_container) {
    const char *contents;
    char type;
    int error = 0;
    for (;;) {
        error = sd_bus_message_peek_type(m, &type, &contents);
        if (error < 0) {
            printf("error reading peek type from sd_bus_message");
            return error;
        } else if (error == 0) {
            break;
        }

        switch (type) {
            case SD_BUS_TYPE_BYTE: {
                uint8_t value;
                error = sd_bus_message_read_basic(m, type, &value);
                if (error < 0) {
                    printf("error reading sd_bus_message basic type");
                    return error;
                }

                error = append_uint(ret, value);
                if (error < 0) {
                    printf("error appending value");
                    return error;
                }

                break;
            }

            case SD_BUS_TYPE_BOOLEAN: {
                int value;
                error = sd_bus_message_read_basic(m, type, &value);
                if (error < 0) {
                    printf("error reading sd_bus_message basic type");
                    return error;
                }

                error = append_boolean(ret, value);
                if (error < 0) {
                    printf("error appending value");
                    return error;
                }

                break;
            }

            case SD_BUS_TYPE_INT16: {
                int16_t value;
                error = sd_bus_message_read_basic(m, type, &value);
                if (error < 0) {
                    printf("error reading sd_bus_message basic type");
                    return error;
                }

                error = append_sint(ret, value);
                if (error < 0) {
                    printf("error appending value");
                    return error;
                }

                break;
            }

            case SD_BUS_TYPE_UINT16: {
                uint16_t value;
                error = sd_bus_message_read_basic(m, type, &value);
                if (error < 0) {
                    printf("error reading sd_bus_message basic type");
                    return error;
                }

                error = append_uint(ret, value);
                if (error < 0) {
                    printf("error appending value");
                    return error;
                }

                break;
            }

            case SD_BUS_TYPE_INT32: {
                int32_t value;
                error = sd_bus_message_read_basic(m, type, &value);
                if (error < 0) {
                    printf("error reading sd_bus_message basic type");
                    return error;
                }

                error = append_sint(ret, value);
                if (error < 0) {
                    printf("error appending value");
                    return error;
                }

                break;
            }

            case SD_BUS_TYPE_UINT32: {
                uint32_t value;
                error = sd_bus_message_read_basic(m, type, &value);
                if (error < 0) {
                    printf("error reading sd_bus_message basic type");
                    return error;
                }

                error = append_uint(ret, value);
                if (error < 0) {
                    printf("error appending value");
                    return error;
                }

                break;
            }

            case SD_BUS_TYPE_INT64: {
                int64_t value;
                error = sd_bus_message_read_basic(m, type, &value);
                if (error < 0) {
                    printf("error reading sd_bus_message basic type");
                    return error;
                }

                error = append_sint(ret, value);
                if (error < 0) {
                    printf("error appending value");
                    return error;
                }

                break;
            }

            case SD_BUS_TYPE_UINT64: {
                uint64_t value;
                error = sd_bus_message_read_basic(m, type, &value);
                if (error < 0) {
                    printf("error reading sd_bus_message basic type");
                    return error;
                }

                error = append_uint(ret, value);
                if (error < 0) {
                    printf("error appending value");
                    return error;
                }

                break;
            }

            case SD_BUS_TYPE_DOUBLE: {
                double value;
                error = sd_bus_message_read_basic(m, type, &value);
                if (error < 0) {
                    printf("error reading sd_bus_message basic type");
                    return error;
                }

                error = append_double(ret, value);
                if (error < 0) {
                    printf("error appending value");
                    return error;
                }

                break;
            }

            case SD_BUS_TYPE_SIGNATURE:
            case SD_BUS_TYPE_OBJECT_PATH: {
                const char *value;
                error = sd_bus_message_read_basic(m, type, &value);
                if (error < 0) {
                    printf("error reading sd_bus_message basic type");
                    return error;
                }

                error = append_value(ret, value);
                if (error < 0) {
                    printf("error appending value");
                    return error;
                }

                break;
            }
			
            case SD_BUS_TYPE_STRING: {
                const char *value;
                error = sd_bus_message_read_basic(m, type, &value);
                if (error < 0) {
                    printf("error reading sd_bus_message basic type");
                    return error;
                }

                error = append_string(ret, value);
                if (error < 0) {
                    printf("error appending value");
                    return error;
                }

                break;
            }

            case SD_BUS_TYPE_UNIX_FD: {
                int value;
                error = sd_bus_message_read_basic(m, type, &value);
                if (error < 0) {
                    printf("error reading sd_bus_message basic type");
                    return error;
                }

                error = append_uint(ret, value);
                if (error < 0) {
                    printf("error appending value");
                    return error;
                }

                break;
            }

            case SD_BUS_TYPE_VARIANT: {
                char *temp = NULL;

                error = sd_bus_message_enter_container(m, type, contents);
                if (error < 0) {
                    printf("error entering sd_bus_message container");
                    return error;
                }

                error = parse_message_to_string(m, &temp, false);
                if (error < 0) {
                    printf("error creating response message from sd_bus_message response");
                    free(temp);
                    return error;
                }

                error = append_value(ret, contents);
                if (error < 0) {
                    printf("error appending value");
					free(temp);
                    return error;
                }
                error = append_value(ret, temp);
                free(temp);
                if (error < 0) {
                    printf("error appending value");
                    return error;
                }

                error = sd_bus_message_exit_container(m);
                if (error < 0) {
                    printf("error exiting sd_bus_message container");
                    return error;
                }

                break;
            }

            case SD_BUS_TYPE_DICT_ENTRY:
            case SD_BUS_TYPE_STRUCT: {
                char *temp = NULL;

                error = sd_bus_message_enter_container(m, type, contents);
                if (error < 0) {
                    printf("error entering sd_bus_message container");
                    return error;
                }

                while (true) {
                    error = sd_bus_message_at_end(m, false);
                    if (error < 0) {
                        printf("error checking sd_bus_message end");
                        return error;
                    }

                    if (error > 0) {
                        break;  // end of message
                    }

                    error = parse_message_to_string(m, &temp, true);
                    if (error < 0) {
                        printf("error creating response message from sd_bus_message response");
                        return error;
                    }
                }

                error = append_value(ret, temp);
                free(temp);
                if (error < 0) {
                    printf("error appending value");
                    return error;
                }

                error = sd_bus_message_exit_container(m);
                if (error < 0) {
                    printf("error exiting sd_bus_message container");
                    return error;
                }

                break;
            }

            case SD_BUS_TYPE_ARRAY: {
                char *temp = NULL;
                int count = 0;

                error = sd_bus_message_enter_container(m, type, contents);
                if (error < 0) {
                    printf("error entering sd_bus_message container");
                    return error;
                }
                while (true) {
                    error = sd_bus_message_at_end(m, false);
                    if (error < 0) {
                        printf("error checking sd_bus_message end");
                        return error;
                    }

                    if (error > 0) {
                        break;  // end of message
                    }

                    error = parse_message_to_string(m, &temp, true);
                    if (error < 0) {
                        printf("error creating response message from sd_bus_message response");
                        return error;
                    }

                    count++;
                }

                error = append_uint(ret, count);
                if (error < 0) {
                    printf("error appending value");
                	free(temp);
                    return error;
                }

                if (count != 0) {
                    error = append_value(ret, temp);
                	if (error < 0) {
                    	printf("error appending value");
                		free(temp);
                    	return error;
                	}
                }
                free(temp);

                error = sd_bus_message_exit_container(m);
                if (error < 0) {
                    printf("error exiting sd_bus_message container");
                    return error;
                }

                break;
            }

            default:
                printf("unexpected element type");
                error = -EINVAL;
                return error;
        }

        if (called_from_container)
            break;
    }

    return 0;
}

static int append_value(char **ret, const char *value) {
    if (*ret) {
        *ret = realloc(*ret, strlen(*ret) + strlen(value) + 2);
        if (!*ret)
            return -ENOMEM;

        sprintf(*ret, "%s %s", *ret, value);
    } else {
        *ret = realloc(*ret, strlen(value) + 1);
        if (!*ret) {
            return -ENOMEM;
        }
        sprintf(*ret, "%s", value);
    }
    return 0;
}

static int append_string(char **ret, const char *value) {
    if (*ret) {
        *ret = realloc(*ret, strlen(*ret) + strlen(value) + 4);
        if (!*ret)
            return -ENOMEM;

        sprintf(*ret, "%s \"%s\"", *ret, value);
    } else {
        *ret = realloc(*ret, strlen(value) + 3);
        if (!*ret) {
            return -ENOMEM;
        }
        sprintf(*ret, "\"%s\"", value);
    }
    return 0;
}

static int append_boolean(char **ret, int value) {
    char temp[6];
    value ? sprintf(temp, "%s", "true") : sprintf(temp, "%s", "false");

    return append_value(ret, temp);
}

static int append_uint(char **ret, uint64_t value) {
    char temp[21];
    sprintf(temp, "%lu", value);

    return append_value(ret, temp);
}
static int append_sint(char **ret, int64_t value) {
    char temp[21];
    sprintf(temp, "%ld", value);

    return append_value(ret, temp);
}

static int append_double(char **ret, double value) {
    char temp[128];
    sprintf(temp, "%g", value);

    return append_value(ret, temp);
}

static int find_next_argument(char **arguments, char *arg){

	char *beggining, *end;
	
	beggining = NULL;
	end = NULL;
	
	
	if(**arguments != DELIMITER)
	    beggining = *arguments;
	
	while (**arguments != '\0')
	{
		if (**arguments == DELIMITER)
		{
			if (beggining == NULL)
				beggining = *arguments + 1;
			else{
				end = *arguments;
				strncpy(arg, beggining, end-beggining);
				arg[end-beggining] = '\0';
				return end-beggining;
			}			
		}
		*arguments = *arguments + 1;
	}

	if (beggining != NULL) {
		end = *arguments;
		strncpy(arg, beggining, end-beggining);
		arg[end-beggining] = '\0';
		return end-beggining;
	}
	
	return 0;

}

static int find_next_argument_string(char **arguments, char *arg){

	char *beggining, *end;
	char last = 0; 
	
	beggining = NULL;
	end = NULL;

	while (**arguments != '\0')
	{
		if (**arguments == STR_DELIMITER)
		{
			if(last == '\\'){
				strcpy((*arguments-1), *arguments);
				continue;
			}

			if (beggining == NULL)
				beggining = *arguments + 1;
			else{
				end = *arguments;
				strncpy(arg, beggining, end-beggining);
				arg[end-beggining] = '\0';
		        *arguments = *arguments + 1;
				return end-beggining;
			}			
		}
		last = **arguments;
		*arguments = *arguments + 1;
	}
	
	return 0;

}
static const char *find_matching_bracket(const char *str)
{
	int bracket_counter = 0;
	for (char c = *str; c; c = *++str) {
		if (c == SD_BUS_TYPE_STRUCT_BEGIN || c == SD_BUS_TYPE_DICT_ENTRY_BEGIN) {
			bracket_counter++;
			continue;
		}

		if (c == SD_BUS_TYPE_STRUCT_END || c == SD_BUS_TYPE_DICT_ENTRY_END) {
			if (bracket_counter > 0) {
				bracket_counter--;
			} else {
				return str;
			}
		}
	}

	return NULL;
}

int append_complete_types_to_message(sd_bus_message *m, const char *signature, char **arguments) {
	char type;
	int error;

	char *p = malloc((strlen(*arguments) + 1) * sizeof(char));

	while ((type = *signature) != '\0') {
		signature++;

		switch (type)
		{
			case SD_BUS_TYPE_BYTE: {
				error = find_next_argument(arguments, p);
				error = sd_bus_message_append_basic(m, type, &(uint8_t){(uint8_t) atoi(p)});
				if (error < 0) {
					free(p);
        			fprintf(stderr, "Failed to append SD_BUS_TYPE_BYTE: %s\n", strerror(-error));
					return error;
				}
						
				break;
			}                
			case SD_BUS_TYPE_BOOLEAN: {
				error = find_next_argument(arguments, p);
				error = sd_bus_message_append_basic(m, type, &(uint32_t){(uint32_t) (strcmp(p, "true")==0)});
				if (error < 0) {
					free(p);
        			fprintf(stderr, "Failed to append SD_BUS_TYPE_BOOLEAN: %s\n", strerror(-error));
					return error;
				}

				break;
			}
			case SD_BUS_TYPE_INT32:
			case SD_BUS_TYPE_UINT32:
			case SD_BUS_TYPE_UNIX_FD: {
				error = find_next_argument(arguments, p);
				error = sd_bus_message_append_basic(m, type, &(uint32_t){(uint32_t) atoi(p)});
				if (error < 0) {
					free(p);
        			fprintf(stderr, "Failed to append SD_BUS_TYPE_INT32: %s\n", strerror(-error));
					return error;
				}

				break;
			}
			case SD_BUS_TYPE_INT16:
			case SD_BUS_TYPE_UINT16: {
				error = find_next_argument(arguments, p);
				error = sd_bus_message_append_basic(m, type, &(uint16_t){(uint16_t) atoi(p)});
				if (error < 0) {
        			fprintf(stderr, "Failed to append SD_BUS_TYPE_UINT16: %s\n", strerror(-error));
					return error;
				}

				break;
			}
			case SD_BUS_TYPE_INT64:
			case SD_BUS_TYPE_UINT64: {
				error = find_next_argument(arguments, p);
				error = sd_bus_message_append_basic(m, type, &(uint64_t){(uint64_t) atoi(p)});
				if (error < 0) {
					free(p);
        			fprintf(stderr, "Failed to append SD_BUS_TYPE_UINT64: %s\n", strerror(-error));
					return error;
				}
  
				break;
			}
			case SD_BUS_TYPE_DOUBLE: {
				double value;
				char *eptr;
				error = find_next_argument(arguments, p);

				value = strtod(p, &eptr);
				if (value == 0)
				{
					if (errno == ERANGE){
						printf("The value provided was out of range\n");
						return -EINVAL;
					}
				}

				error = sd_bus_message_append_basic(m, type, &value);
				if (error < 0) {
					free(p);
        			fprintf(stderr, "Failed to append SD_BUS_TYPE_UINT64: %s\n", strerror(-error));
					return error;
				}
  
				break;
			}
			case SD_BUS_TYPE_SIGNATURE:
			case SD_BUS_TYPE_OBJECT_PATH: {
				error = find_next_argument(arguments, p);
				error = sd_bus_message_append_basic(m, type, p);
				if (error < 0) {
					free(p);
        			fprintf(stderr, "Failed to append SD_BUS_TYPE_STRING: %s\n", strerror(-error));
					return error;
				}
				
				break;
			}
			case SD_BUS_TYPE_STRING: {
				error = find_next_argument_string(arguments, p);
				error = sd_bus_message_append_basic(m, type, p);
				if (error < 0) {
					free(p);
        			fprintf(stderr, "Failed to append SD_BUS_TYPE_STRING: %s\n", strerror(-error));
					return error;
				}
				
				break;
			}
			case SD_BUS_TYPE_ARRAY: {
				char *contents_type = NULL;

				if (*signature == '(' || *signature == '{'){
					char container_type = (*signature == SD_BUS_TYPE_STRUCT_BEGIN) ? SD_BUS_TYPE_STRUCT : SD_BUS_TYPE_DICT_ENTRY;
					size_t contents_type_length = 0;

					// get inner content type
					contents_type_length = (size_t) find_matching_bracket(signature + 1) - (size_t) signature + 1;
					contents_type = malloc((contents_type_length + 1) * sizeof(char));
					memcpy(contents_type, signature, contents_type_length);
					contents_type[contents_type_length] = 0; // NULL terminate string
				}else{
					contents_type = malloc(2 * sizeof(char));
					contents_type[0] = *signature; // NULL terminate string
					contents_type[1] = 0; // NULL terminate string
				}
				
				error = sd_bus_message_open_container(m, type, contents_type);				
				if (error < 0) {
					free(p);
					free(contents_type);
        			fprintf(stderr, "Failed to open container SD_BUS_TYPE_ARRAY: %s %d\n", strerror(-error), -error);
					return error;
				}
				error = find_next_argument(arguments, p);
				int array_size = atoi(p);
				// append value(s)
				for (size_t i = 0; i < array_size; i++) {

					error = append_complete_types_to_message(m, contents_type, arguments);
					if (error < 0) {
						free(p);
						free(contents_type);
        				fprintf(stderr, "Failed to append SD_BUS_TYPE_ARRAY: %s\n", strerror(-error));
						return error;
					}
				}
				error = sd_bus_message_close_container(m);
				if (error < 0) {
					free(p);
					free(contents_type);
        			fprintf(stderr, "Failed to close container SD_BUS_TYPE_ARRAY: %s\n", strerror(-error));
					return error;
				}
				signature += strlen(contents_type); // advance to the end
				FREE_SAFE(contents_type);
				break;
			}
			case SD_BUS_TYPE_VARIANT: {
				error = find_next_argument(arguments, p);
				// open container
				error = sd_bus_message_open_container(m, type, p);
				if (error < 0) {
					free(p);
        			fprintf(stderr, "Failed to open container SD_BUS_TYPE_VARIANT: %s\n", strerror(-error));
					return error;
				}
				// append value(s)
				error = append_complete_types_to_message(m, p, arguments);
				if (error < 0) {
					free(p);
        			fprintf(stderr, "Failed to append SD_BUS_TYPE_VARIANT: %s\n", strerror(-error));
					return error;
				}
				// close container
				error = sd_bus_message_close_container(m);
				if (error < 0) {
					free(p);
        			fprintf(stderr, "Failed to close container SD_BUS_TYPE_VARIANT: %s\n", strerror(-error));
					return error;
				}
				break;
				break;
			}
			case SD_BUS_TYPE_STRUCT_BEGIN:
			case SD_BUS_TYPE_DICT_ENTRY_BEGIN: {
				char container_type = (type == SD_BUS_TYPE_STRUCT_BEGIN) ? SD_BUS_TYPE_STRUCT : SD_BUS_TYPE_DICT_ENTRY;
				char *contents_type = NULL;
				size_t contents_type_length = 0;

				// get inner content type
				contents_type_length = (size_t) find_matching_bracket(signature) - (size_t) signature;
				contents_type = malloc((contents_type_length + 1) * sizeof(char));
				memcpy(contents_type, signature, contents_type_length);
				contents_type[contents_type_length] = 0; // NULL terminate string

				// open container
				error = sd_bus_message_open_container(m, container_type, contents_type);
				if (error < 0) {
					free(contents_type);
					free(p);
        			fprintf(stderr, "Failed to open container SD_BUS_TYPE_STRUCT_BEGIN: %s\n", strerror(-error));
					return error;
				}
				
				// append value(s)
				error = append_complete_types_to_message(m, contents_type, arguments);
				if (error < 0) {
					free(contents_type);
					free(p);
        			fprintf(stderr, "Failed to append SD_BUS_TYPE_STRUCT_BEGIN: %s\n", strerror(-error));
					return error;
				}
				// close container
				error = sd_bus_message_close_container(m);
				if (error < 0) {
					free(contents_type);
					free(p);
        			fprintf(stderr, "Failed to close container SD_BUS_TYPE_STRUCT_BEGIN: %s\n", strerror(-error));
					return error;
				}

				FREE_SAFE(contents_type);
				signature += contents_type_length + 1; // advance to the end of structure
				break;
			}
			case SD_BUS_TYPE_STRUCT_END:
			case SD_BUS_TYPE_DICT_ENTRY_END: {
				return 0;
				break;
			}
			default:
				break;
			}
	}

	free(p);

    return 1;
}