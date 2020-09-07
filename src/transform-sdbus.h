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
#include <sysrepo.h>
#ifndef _GENERIC_UBUS_H_
#define _GENERIC_UBUS_H_

#define RPC_SD_BUS "sd-bus"
#define RPC_SD_BUS_SERVICE "sd-bus-service"
#define RPC_SD_BUS_OBJPATH "sd-bus-object-path"
#define RPC_SD_BUS_INTERFACE "sd-bus-interface"
#define RPC_SD_BUS_METHOD "sd-bus-method"
#define RPC_SD_BUS_SIGNATURE "sd-bus-method-signature"
#define RPC_SD_BUS_ARGUMENTS "sd-bus-method-arguments"

#define STR_DELIMITER   '\"'
#define DELIMITER       ' '

#define RPC_SD_BUS_METHOD_XPATH "/generic-sdbus:sd-bus-call/" "sd-bus-result[sd-bus-method='%s']/sd-bus-method"
#define RPC_SD_BUS_RESPONSE_XPATH "/generic-sdbus:sd-bus-call/" "sd-bus-result[sd-bus-method='%s']/sd-bus-response"
#define RPC_SD_BUS_SIGNATURE_XPATH "/generic-sdbus:sd-bus-call/" "sd-bus-result[sd-bus-method='%s']/sd-bus-signature"

#define FREE_SAFE(x) \
	do {             \
		free(x);     \
		(x) = NULL;  \
	} while (0)

int generic_sdbus_call_rpc_cb(sr_session_ctx_t *session, const char *op_path,
				       const sr_val_t *input, const size_t input_cnt,
				       sr_event_t event, uint32_t request_id,
				       sr_val_t **output, size_t *output_cnt, void *private_data);

#endif //_GENERIC_UBUS_H_