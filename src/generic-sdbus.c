
#include <json-c/json.h>
#include <generic-sdbus.h>

#include <systemd/sd-bus.h>
#include <systemd/sd-bus-protocol.h>
#include <xpath.h>
#include <sysrepo.h>
#include <sysrepo/values.h>
#include <common.h>

static int create_json_response_from_message(sd_bus_message *m, json_object **result_jobj);
static int find_next_argument(char **arguments, char *arg);
static int find_next_argument_string(char **arguments, char *arg);
static const char *find_matching_bracket(const char *str);
static int append_complete_types_to_message(sd_bus_message *m, const char *signature, char **arguments);



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
	const char *sd_bus_reply_signature = NULL;
	sr_val_t *result = NULL;
	size_t count = 0;
    sd_bus *bus = NULL;
    sd_bus_message *m = NULL;
    sd_bus_message *reply = NULL;
    sd_bus_error *error = NULL;
	json_object *json_response = NULL;


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
	}
    printf("sdbus call rpc:\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n", 
		   sd_bus_bus, sd_bus_service, sd_bus_object_path, sd_bus_interface, 
		   sd_bus_method, sd_bus_method_signature, sd_bus_method_arguments);

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
		bus, &m, sd_bus_service, sd_bus_object_path,
		sd_bus_interface, sd_bus_method);

	if (rc < 0) {
        fprintf(stderr, "Failed to create a new message: %s\n", strerror(-rc));
		goto cleanup;
	}

	rc = append_complete_types_to_message(m, sd_bus_method_signature, &sd_bus_method_arguments);
	
    if (rc < 0) {
        fprintf(stderr, "Failed to append: %s\n", strerror(-rc));
        goto cleanup;
    }

	rc = sd_bus_call(bus, m,  0, error, &reply);
	if (rc < 0) {
        fprintf(stderr, "Failed to call: %s\n", strerror(-rc));
		goto cleanup;
	}

	sd_bus_reply_signature = sd_bus_message_get_signature(m, 1);
	if (!sd_bus_reply_signature) {
        fprintf(stderr, "Failed to get reply message signature");
        goto cleanup;
    }

    rc = create_json_response_from_message(reply, &json_response);
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

    rc = sr_val_set_str_data(&result[count], SR_STRING_T, json_object_to_json_string(json_response));
    SR_CHECK_RET(rc, cleanup, "sr value set str data: %s", sr_strerror(rc));

	count++;

    rc = sr_val_build_xpath(&result[count], RPC_SD_BUS_SIGNATURE_XPATH, sd_bus_method);
    SR_CHECK_RET(rc, cleanup, "sr value set xpath: %s", sr_strerror(rc));

    rc = sr_val_set_str_data(&result[count], SR_STRING_T, sd_bus_reply_signature);
    SR_CHECK_RET(rc, cleanup, "sr value set str data: %s", sr_strerror(rc));

	count++;

	FREE_SAFE(json_response);
	FREE_SAFE(tail_node);
	FREE_SAFE(m);
	FREE_SAFE(reply);
	sd_bus_close(bus);
	sd_bus_unref(bus);
	bus=NULL;

  	*output_cnt = count;
  	*output = result;

    return SR_ERR_OK;

	cleanup:
		free(json_response);
		free(tail_node);
		free(m);
		free(reply);
		sd_bus_close(bus);
		sd_bus_unref(bus);
		if (result != NULL) {
			sr_free_values(result, count);
		}

		return rc;
}
static int create_json_response_from_message(sd_bus_message *m, json_object **result_jobj)
{
	int error = 0;
	char type;
	const char *contents_type;
	json_object *value_jobj = NULL;

	error = sd_bus_message_peek_type(m, &type, &contents_type);


	if (error < 0) {
		SRP_LOG_ERR("error parsing type %c from sd_bus_message", type);
		return error;
	}

	if (!contents_type) {
		// contents_type is NULL if type is a basic type
		union {
			uint8_t u8;
			uint16_t u16;
			int16_t s16;
			uint32_t u32;
			int32_t s32;
			uint64_t u64;
			int64_t s64;
			double d64;
			const char *string;
			int i;
		} basic;

		error = sd_bus_message_read_basic(m, type, &basic);
		switch (type) {
			case SD_BUS_TYPE_BYTE: {
				value_jobj = json_object_new_int(basic.u8);
				break;
			}

			case SD_BUS_TYPE_BOOLEAN: {
				value_jobj = json_object_new_boolean(basic.u8);
				break;
			}

			case SD_BUS_TYPE_INT16: {
				value_jobj = json_object_new_int(basic.s16);
				break;
			}

			case SD_BUS_TYPE_UINT16: {
				value_jobj = json_object_new_int(basic.u16);
				break;
			}

			case SD_BUS_TYPE_INT32: {
				value_jobj = json_object_new_int(basic.s32);
				break;
			}

			case SD_BUS_TYPE_UINT32: {
				value_jobj = json_object_new_int((int32_t) basic.u32);
				break;
			}

			case SD_BUS_TYPE_INT64: {
				value_jobj = json_object_new_int64(basic.s64);
				break;
			}

			case SD_BUS_TYPE_UINT64: {
				value_jobj = json_object_new_int64((int64_t) basic.u64);
				break;
			}

			case SD_BUS_TYPE_DOUBLE: {
				value_jobj = json_object_new_double(basic.d64);
				break;
			}

			case SD_BUS_TYPE_STRING:
			case SD_BUS_TYPE_OBJECT_PATH:
			case SD_BUS_TYPE_SIGNATURE: {
				value_jobj = json_object_new_string(basic.string);
				break;
			}

			case SD_BUS_TYPE_UNIX_FD: {
				value_jobj = json_object_new_int(basic.i);
				break;
			}

			default: {
				SRP_LOG_ERR("unexpected element type", NULL);
				return SR_ERR_UNSUPPORTED;
			}
		}
	} else {
		// else it is container type
		error = sd_bus_message_enter_container(m, type, contents_type);

		switch (type) {
			case SD_BUS_TYPE_VARIANT: {
				json_object *contents_value = NULL;
				error = create_json_response_from_message(m, &contents_value);
				value_jobj = json_object_new_object();
				json_object_object_add(value_jobj, contents_type, contents_value);
				break;
			}

			case SD_BUS_TYPE_DICT_ENTRY: {
				json_object *dict_key_jobj = NULL;
				json_object *dict_value_jobj = NULL;
				error = create_json_response_from_message(m, &dict_key_jobj);
				error |= create_json_response_from_message(m, &dict_value_jobj);
				value_jobj = json_object_new_object();
				json_object_object_add(value_jobj, json_object_get_string(dict_key_jobj), dict_value_jobj);

				json_object_put(dict_key_jobj);
				break;
			}

			case SD_BUS_TYPE_ARRAY:
			case SD_BUS_TYPE_STRUCT: {
				value_jobj = json_object_new_array();
				while (!sd_bus_message_at_end(m, 0)) {
					json_object *element_value = NULL;
					error = create_json_response_from_message(m, &element_value);
					json_object_array_add(value_jobj, element_value);
				}
				break;
			}

			default: {
				SRP_LOG_ERR("unexpected element type", NULL);
				return -SR_ERR_UNSUPPORTED;
			}
		}

		error = sd_bus_message_exit_container(m);
	}

	if (error < 0) {
		SRP_LOG_ERR("error parsing type %c from sd_bus_message", type);
		return error;
	}

	*result_jobj = value_jobj;
	return 0;
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
	
	return 0;

}

static int find_next_argument_string(char **arguments, char *arg){

	char *beggining, *end;
	
	beggining = NULL;
	end = NULL;

	while (**arguments != '\0')
	{
		if (**arguments == STR_DELIMITER)
		{
			if(*(*arguments-1) == '\\'){
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
			case SD_BUS_TYPE_STRING:
			case SD_BUS_TYPE_OBJECT_PATH:
			case SD_BUS_TYPE_SIGNATURE: {
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
					contents_type = malloc((strlen(signature) + 1) * sizeof(char));
					memcpy(contents_type, signature, strlen(signature));
					contents_type[strlen(signature)] = 0; // NULL terminate string
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

    return 1;
}