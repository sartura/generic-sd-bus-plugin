
#include <json-c/json.h>
#include <generic-sdbus.h>
#include <errno.h>
#include <stdlib.h>

#include <systemd/sd-bus.h>
#include <systemd/sd-bus-protocol.h>
#include <xpath.h>
#include <sysrepo.h>
#include <sysrepo/values.h>
#include <common.h>

static int find_next_argument(char **arguments, char *arg);
static int find_next_argument_string(char **arguments, char *arg);
static const char *find_matching_bracket(const char *str);
static int append_complete_types_to_message(sd_bus_message *m, const char *signature, char **arguments);
static int parse_message_to_string(sd_bus_message *m, char **ret, bool called_from_container);
static int append_value(char **ret, const char *value);
static int append_string(char **ret, const char *value);
static int append_boolean(char **ret, int value);
static int append_sint(char **ret, int64_t value);
static int append_uint(char **ret, uint64_t value);
static int append_double(char **ret, double value);

int generic_sdbus_call_rpc_cb(sr_session_ctx_t *session, const char *op_path,
				       const sr_val_t *input, const size_t input_cnt,
				       sr_event_t event, uint32_t request_id,
				       sr_val_t **output, size_t *output_cnt, void *private_data)
{
	int rc = SR_ERR_OK;
	char *tail_node = NULL;
	char *sd_bus_bus = NULL;
	char *sd_bus_service = NULL;
	char *sd_bus_object_path = NULL;
	char *sd_bus_interface = NULL;
	char *sd_bus_method = NULL;
	char *sd_bus_method_signature = NULL;
	char *sd_bus_method_arguments = NULL;
	char *string_reply = NULL;
	const char *sd_bus_reply_signature = NULL;
	sr_val_t *result = NULL;
	size_t count = 0;
    sd_bus *bus = NULL;
    sd_bus_message *sd_message = NULL;
    sd_bus_message *sd_message_reply = NULL;
    sd_bus_error *error = NULL;


	for (int i = 0; i < input_cnt; i++) {
		rc = xpath_get_tail_node(input[i].xpath, &tail_node);
		CHECK_RET_MSG(rc, cleanup, "get tail node error");

		if (strcmp(RPC_SD_BUS, tail_node) == 0) {
		sd_bus_bus = input[i].data.string_val;
		} else if (strcmp(RPC_SD_BUS_SERVICE, tail_node) == 0) {
		sd_bus_service = input[i].data.string_val;
		} else if (strcmp(RPC_SD_BUS_OBJPATH, tail_node) == 0) {
		sd_bus_object_path = input[i].data.string_val;
		} else if (strcmp(RPC_SD_BUS_INTERFACE, tail_node) == 0) {
		sd_bus_interface = input[i].data.string_val;
		} else if (strcmp(RPC_SD_BUS_METHOD, tail_node) == 0) {
		sd_bus_method = input[i].data.string_val;
		} else if (strcmp(RPC_SD_BUS_SIGNATURE, tail_node) == 0) {
		sd_bus_method_signature = input[i].data.string_val;
		} else if (strcmp(RPC_SD_BUS_ARGUMENTS, tail_node) == 0) {
		sd_bus_method_arguments = input[i].data.string_val;
		}

		uint8_t last = (i + 1) >= input_cnt;

		if ((strstr(tail_node, RPC_SD_BUS_ARGUMENTS) != NULL &&
			 sd_bus_bus != NULL && sd_bus_service != NULL && 
			 sd_bus_object_path != NULL && sd_bus_interface != NULL && 
			 sd_bus_method != NULL && sd_bus_method_signature != NULL 
			 && sd_bus_method_arguments != NULL) || last == 1) {
		
			if (strcmp(sd_bus_bus, "SYSTEM") == 0) {
				rc = sd_bus_open_system(&bus);
			}else
			{
				rc = sd_bus_open_user(&bus);
			}

			if (rc < 0) {
				fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-rc));
				goto cleanup;
			}

			rc = sd_bus_message_new_method_call(
				bus, &sd_message, sd_bus_service, sd_bus_object_path,
				sd_bus_interface, sd_bus_method);

			if (rc < 0) {
				fprintf(stderr, "Failed to create a new message: %s\n", strerror(-rc));
				goto cleanup;
			}

			rc = append_complete_types_to_message(sd_message, sd_bus_method_signature, &sd_bus_method_arguments);
			
			if (rc < 0) {
				fprintf(stderr, "Failed to append: %s\n", strerror(-rc));
				goto cleanup;
			}

			rc = sd_bus_call(bus, sd_message,  0, error, &sd_message_reply);
			if (rc < 0) {
				fprintf(stderr, "Failed to call: %s\n", strerror(-rc));
				goto cleanup;
			}

			sd_bus_reply_signature = sd_bus_message_get_signature(sd_message, 1);
			if (!sd_bus_reply_signature) {
				fprintf(stderr, "Failed to get reply message signature");
				goto cleanup;
			}

			rc = parse_message_to_string(sd_message_reply, &string_reply, false);
			if (rc < 0) {
			fprintf(stderr, "Failed to json: %s\n", strerror(-rc));
			goto cleanup;
			}
			rc = sr_realloc_values(count, count + 3, &result);
			SR_CHECK_RET(rc, cleanup, "sr realloc values error: %s", sr_strerror(rc));

			rc = sr_val_build_xpath(&result[count], RPC_SD_BUS_METHOD_XPATH, sd_bus_method);
			SR_CHECK_RET(rc, cleanup, "sr value set xpath: %s", sr_strerror(rc));

			rc = sr_val_set_str_data(&result[count], SR_STRING_T, sd_bus_method);
			SR_CHECK_RET(rc, cleanup, "sr value set str data: %s", sr_strerror(rc));

			count++;

			rc = sr_val_build_xpath(&result[count], RPC_SD_BUS_RESPONSE_XPATH, sd_bus_method);
			SR_CHECK_RET(rc, cleanup, "sr value set xpath: %s", sr_strerror(rc));

			rc = sr_val_set_str_data(&result[count], SR_STRING_T, string_reply);
			SR_CHECK_RET(rc, cleanup, "sr value set str data: %s", sr_strerror(rc));

			count++;

			rc = sr_val_build_xpath(&result[count], RPC_SD_BUS_SIGNATURE_XPATH, sd_bus_method);
			SR_CHECK_RET(rc, cleanup, "sr value set xpath: %s", sr_strerror(rc));

			rc = sr_val_set_str_data(&result[count], SR_STRING_T, sd_bus_reply_signature);
			SR_CHECK_RET(rc, cleanup, "sr value set str data: %s", sr_strerror(rc));

			count++;

			FREE_SAFE(string_reply);
			sd_bus_message_unref(sd_message);
			sd_message=NULL;
			sd_bus_message_unref(sd_message_reply);
			sd_message_reply=NULL;
			sd_bus_close(bus);
			sd_bus_unref(bus);
			bus=NULL;

		}
		FREE_SAFE(tail_node);
		
	}



  	*output_cnt = count;
  	*output = result;

    return SR_ERR_OK;

	cleanup:
		free(string_reply);
		free(tail_node);
		sd_bus_message_unref(sd_message);
		sd_bus_message_unref(sd_message_reply);
		sd_bus_close(bus);
		sd_bus_unref(bus);
		if (result != NULL) {
			sr_free_values(result, count);
		}

		return rc;
}

static int parse_message_to_string(sd_bus_message *m, char **ret, bool called_from_container) {
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
                    printf("error creating json response message from sd_bus_message response");
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
                        printf("error creating json response message from sd_bus_message response");
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
                        printf("error creating json response message from sd_bus_message response");
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

static int append_complete_types_to_message(sd_bus_message *m, const char *signature, char **arguments) {
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