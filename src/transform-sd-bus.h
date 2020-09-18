/**
 * @file transform-sd-bus.h
 * @authors Borna Blazevic <borna.blazevic@sartura.hr> Luka Kudra <luka.kudra@sartura.hr>
 *
 * @brief Lists the functions for sd-bus message parsing
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
#ifndef _TRANSFORM_SDBUS_H_
#define _TRANSFORM_SDBUS_H_

#include <systemd/sd-bus.h>
#include <systemd/sd-bus-protocol.h>

int append_complete_types_to_message(sd_bus_message *m, const char *signature, char **arguments);
int parse_message_to_string(sd_bus_message *m, char **ret, bool called_from_container);

int bus_message_encode(const char *signature, const char *arguments, sd_bus_message *m);
int bus_message_decode(sd_bus_message *m, char **arguments);

#define FREE_SAFE(x) \
	do {             \
		free(x);     \
		(x) = NULL;  \
	} while (0)

#endif //_TRANSFORM_SDBUS_H_