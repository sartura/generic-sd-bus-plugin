/**
 * @file generic-sd-bus.c
 * @authors Borna Blazevic <borna.blazevic@sartura.hr> Luka Paulic <luka.paulic@sartura.hr>
 *
 * @brief Implements tha main logic of the generic sd-bus plugin.
 *        Main functionalities include:
 *          + enables RPC for implementig the sd-bus method call of an sd-bus service.
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
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/wait.h>

#include <sysrepo.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-bus-protocol.h>

#include "transform-sd-bus.h"

#define YANG_MODEL "generic-sd-bus"

#define RPC_SD_BUS "sd-bus"
#define RPC_SD_BUS_SERVICE "sd-bus-service"
#define RPC_SD_BUS_OBJPATH "sd-bus-object-path"
#define RPC_SD_BUS_INTERFACE "sd-bus-interface"
#define RPC_SD_BUS_METHOD "sd-bus-method"
#define RPC_SD_BUS_SIGNATURE "sd-bus-method-signature"
#define RPC_SD_BUS_ARGUMENTS "sd-bus-method-arguments"

#define RPC_SD_BUS_METHOD_XPATH "/generic-sd-bus:sd-bus-call/" \
								"sd-bus-result[sd-bus-method='%s']/sd-bus-method"
#define RPC_SD_BUS_RESPONSE_XPATH "/generic-sd-bus:sd-bus-call/" \
								  "sd-bus-result[sd-bus-method='%s']/sd-bus-response"
#define RPC_SD_BUS_SIGNATURE_XPATH "/generic-sd-bus:sd-bus-call/" \
								   "sd-bus-result[sd-bus-method='%s']/sd-bus-signature"

/*
 * @brief Callback for sd-bus call RPC method. Used to invoke an sd-bus call and
 *        retreive sd-bus call result data.
 *
 * @param[in] xpath xpath to the module RPC.
 * @param[in] input sysrepo RPC input data.
 * @param[out] output sysrepo RPC output data to be set.
 * @param[in] private_ctx context being passed to the callback function.
 *
 * @return error code.
 */
int generic_sdbus_call_rpc_tree_cb(sr_session_ctx_t *session, const char *op_path,
								   const struct lyd_node *input, sr_event_t event,
								   uint32_t request_id, struct lyd_node *output,
								   void *private_data)
{
	int rc = SR_ERR_OK;
	char *xpath = NULL;
	const char *sd_bus_bus = NULL;
	const char *sd_bus_service = NULL;
	const char *sd_bus_object_path = NULL;
	const char *sd_bus_interface = NULL;
	const char *sd_bus_method = NULL;
	const char *sd_bus_method_signature = NULL;
	const char *sd_bus_method_arguments = NULL;
	char *sd_bus_reply_string = NULL;
	const char *sd_bus_reply_signature = NULL;
	sd_bus *bus = NULL;
	sd_bus_message *sd_message = NULL;
	sd_bus_message *sd_message_reply = NULL;
	sd_bus_error *error = NULL;
	struct lyd_node *node = NULL;
	struct lyd_node *child = NULL;
	struct lyd_node *next = NULL;
	struct lyd_node *ret = NULL;

	if (NULL == input) {
		rc = SR_ERR_INTERNAL;
		SRP_LOG_ERRMSG("input is invalid");
		goto cleanup;
	}

	LY_TREE_FOR(input->child, child)
	{
		LY_TREE_DFS_BEGIN(child, next, node)
		{
			if (node->schema) {
				if (node->schema->nodetype == LYS_LEAF) {
					if (strcmp(RPC_SD_BUS, node->schema->name) == 0) {
						sd_bus_bus = ((struct lyd_node_leaf_list *) node)->value.enm->name;
					} else if (strcmp(RPC_SD_BUS_SERVICE, node->schema->name) == 0) {
						sd_bus_service = ((struct lyd_node_leaf_list *) node)->value.string;
					} else if (strcmp(RPC_SD_BUS_OBJPATH, node->schema->name) == 0) {
						sd_bus_object_path = ((struct lyd_node_leaf_list *) node)->value.string;
					} else if (strcmp(RPC_SD_BUS_INTERFACE, node->schema->name) == 0) {
						sd_bus_interface = ((struct lyd_node_leaf_list *) node)->value.string;
					} else if (strcmp(RPC_SD_BUS_METHOD, node->schema->name) == 0) {
						sd_bus_method = ((struct lyd_node_leaf_list *) node)->value.string;
					} else if (strcmp(RPC_SD_BUS_SIGNATURE, node->schema->name) == 0) {
						sd_bus_method_signature = ((struct lyd_node_leaf_list *) node)->value.string;
					} else if (strcmp(RPC_SD_BUS_ARGUMENTS, node->schema->name) == 0) {
						sd_bus_method_arguments = ((struct lyd_node_leaf_list *) node)->value.string;
					}
				}
				if ((node->schema->nodetype == LYS_LIST || !child->next) &&
					sd_bus_bus != NULL && sd_bus_service != NULL &&
					sd_bus_object_path != NULL && sd_bus_interface != NULL &&
					sd_bus_method != NULL && sd_bus_method_signature != NULL && sd_bus_method_arguments != NULL) {
					if (strcmp(sd_bus_bus, "SYSTEM") == 0)
						rc = sd_bus_open_system(&bus);
					else
						rc = sd_bus_open_user(&bus);
					if (rc < SR_ERR_OK) {
						SRP_LOG_ERR("failed to connect to system bus: %s", strerror(-rc));
						goto cleanup;
					}

					rc = sd_bus_message_new_method_call(
						bus, &sd_message, sd_bus_service, sd_bus_object_path,
						sd_bus_interface, sd_bus_method);
					if (rc < SR_ERR_OK) {
						SRP_LOG_ERR("failed to create a new message: %s", strerror(-rc));
						goto cleanup;
					}

					rc = bus_message_encode(sd_bus_method_signature, sd_bus_method_arguments, sd_message);
					if (rc < SR_ERR_OK) {
						SRP_LOG_ERR("failed to parse reply: %s", strerror(-rc));
						goto cleanup;
					}

					rc = sd_bus_call(bus, sd_message, 0, error, &sd_message_reply);
					if (rc < SR_ERR_OK) {
						SRP_LOG_ERR("failed to call sd-bus method: %s", strerror(-rc));
						goto cleanup;
					}

					sd_bus_reply_signature = sd_bus_message_get_signature(sd_message_reply, 1);
					if (NULL == sd_bus_reply_signature) {
						rc = SR_ERR_INTERNAL;
						SRP_LOG_ERRMSG("failed get reply message signature");
						goto cleanup;
					}

					rc = bus_message_decode(sd_message_reply, &sd_bus_reply_string);
					if (rc < SR_ERR_OK) {
						SRP_LOG_ERR("failed to parse reply: %s", strerror(-rc));
						goto cleanup;
					}

					xpath = realloc(xpath, strlen(RPC_SD_BUS_METHOD_XPATH) + strlen(sd_bus_method) + 1);
					sprintf(xpath, RPC_SD_BUS_METHOD_XPATH, sd_bus_method);
					ret = lyd_new_path(output, NULL, xpath, (void *) sd_bus_method, LYD_ANYDATA_STRING, LYD_PATH_OPT_OUTPUT);
					if (NULL == ret) {
						rc = SR_ERR_INTERNAL;
						SRP_LOG_ERRMSG("failed to set output");
						goto cleanup;
					}

					xpath = realloc(xpath, strlen(RPC_SD_BUS_RESPONSE_XPATH) + strlen(sd_bus_method) + 1);
					sprintf(xpath, RPC_SD_BUS_RESPONSE_XPATH, sd_bus_method);
					ret = lyd_new_path(output, NULL, xpath, (void *) sd_bus_reply_string, LYD_ANYDATA_STRING, LYD_PATH_OPT_OUTPUT);
					if (NULL == ret) {
						rc = SR_ERR_INTERNAL;
						SRP_LOG_ERRMSG("failed to set output");
						goto cleanup;
					}

					xpath = realloc(xpath, strlen(RPC_SD_BUS_SIGNATURE_XPATH) + strlen(sd_bus_method) + 1);
					sprintf(xpath, RPC_SD_BUS_SIGNATURE_XPATH, sd_bus_method);
					ret = lyd_new_path(output, NULL, xpath, (void *) sd_bus_reply_signature, LYD_ANYDATA_STRING, LYD_PATH_OPT_OUTPUT);
					if (NULL == ret) {
						rc = SR_ERR_INTERNAL;
						SRP_LOG_ERRMSG("failed to set output");
						goto cleanup;
					}

					sd_bus_bus = NULL;
					sd_bus_service = NULL;
					sd_bus_object_path = NULL;
					sd_bus_interface = NULL;
					sd_bus_method = NULL;
					sd_bus_method_signature = NULL;
					sd_bus_method_arguments = NULL;

					free(sd_bus_reply_string);
					sd_bus_reply_string = NULL;
					sd_bus_error_free(error);
					sd_message = sd_bus_message_unref(sd_message);
					sd_message_reply = sd_bus_message_unref(sd_message_reply);
					bus = sd_bus_close_unref(bus);
				}
			}

			LY_TREE_DFS_END(child, next, node)
		};
	};
cleanup:
	free(xpath);
	sd_bus_message_unref(sd_message);
	sd_message_reply = sd_bus_message_unref(sd_message_reply);
	sd_bus_error_free(error);
	sd_bus_close_unref(bus);
	free(sd_bus_reply_string);

	return rc;
}

/*
 * @brief Callback for initializing the plugin.
 * 		  Subscribes to generic sd-bus call.
 *
 * @param[in] session session context used for subscribiscions.
 * @param[out] subscription subscription to be unsubscribed on program termination.
 *
 * @return error code.
 *
 */
int sr_plugin_init_cb(sr_session_ctx_t *session, sr_subscription_ctx_t **subscription)
{
	SRP_LOG_INF("%s", __func__);

	int error = 0;

	SRP_LOG_INFMSG("Subscribing to sd-bus call rpc");
	error = sr_rpc_subscribe_tree(session, "/" YANG_MODEL ":sd-bus-call", generic_sdbus_call_rpc_tree_cb, NULL, 0, SR_SUBSCR_CTX_REUSE, subscription);
	if (SR_ERR_OK != error) {
		SRP_LOG_ERR("rpc subscription error: %s", sr_strerror(error));
		goto cleanup;
	}

	SRP_LOG_INFMSG("Succesfull init");
	return SR_ERR_OK;

cleanup:
	if (subscription != NULL) {
		sr_unsubscribe(*subscription);
		*subscription = NULL;
	}
	return error;
}

/*
 * @brief Unsubscribes from all subscriptions, stops the plugin session and connection.
 *
 * @param[in] connection connection for unsubscribing.
 * @param[in] session session for unsubscribing.
 * @param[in] subscription subscription for unsubscribing.
 *
 */
void sr_plugin_cleanup_cb(sr_conn_ctx_t *connection, sr_session_ctx_t *session, sr_subscription_ctx_t *subscription)
{
	SRP_LOG_INF("%s", __func__);
	if (subscription != NULL) {
		sr_unsubscribe(subscription);
	}
	if (session != NULL) {
		sr_session_stop(session);
	}
	if (connection != NULL) {
		sr_disconnect(connection);
	}
	SRP_LOG_INFMSG("Plugin cleaned-up successfully");
}

#ifndef PLUGIN
#include <signal.h>
#include <unistd.h>

volatile int exit_application = 0;

static void sigint_handler(__attribute__((unused)) int signum);

/*
 * @brief Initializes the connection to sysrepo and initializes the plugin.
 * 		  When the program is interupted the cleanup code is called.
 *
 * @return error code.
 *
 */
int main(void)
{
	int error = SR_ERR_OK;
	sr_conn_ctx_t *connection = NULL;
	sr_session_ctx_t *session = NULL;
	sr_subscription_ctx_t *subscription = NULL;

	sr_log_stderr(SR_LL_DBG);

	/* connect to sysrepo */
	error = sr_connect(SR_CONN_DEFAULT, &connection);
	if (error) {
		SRP_LOG_ERR("sr_connect error (%d): %s", error, sr_strerror(error));
		goto out;
	}

	error = sr_session_start(connection, SR_DS_RUNNING, &session);
	if (error) {
		SRP_LOG_ERR("sr_session_start error (%d): %s", error, sr_strerror(error));
		goto out;
	}

	error = sr_plugin_init_cb(session, &subscription);
	if (error) {
		SRP_LOG_ERRMSG("dhcp_plugin_init_cb error");
		goto out;
	}

	/* loop until ctrl-c is pressed / SIGINT is received */
	signal(SIGINT, sigint_handler);
	signal(SIGPIPE, SIG_IGN);
	while (!exit_application) {
		sleep(1); /* or do some more useful work... */
	}

out:
	sr_plugin_cleanup_cb(connection, session, subscription);
	return error;
}

/*
 * @brief Termination signal handeling
 *
 * @param[in] signum signal identifier.
 *
 * @note signum is not used.
 */
static void sigint_handler(__attribute__((unused)) int signum)
{
	SRP_LOG_INFMSG("Sigint called, exiting...");
	exit_application = 1;
}

#endif
