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
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <sysrepo.h>
#include <sysrepo/values.h>
#include <common.h>
#include <transform-sdbus.h>

#define YANG_MODEL "generic-sdbus"

#define RPC_SD_BUS "sd-bus"
#define RPC_SD_BUS_SERVICE "sd-bus-service"
#define RPC_SD_BUS_OBJPATH "sd-bus-object-path"
#define RPC_SD_BUS_INTERFACE "sd-bus-interface"
#define RPC_SD_BUS_METHOD "sd-bus-method"
#define RPC_SD_BUS_SIGNATURE "sd-bus-method-signature"
#define RPC_SD_BUS_ARGUMENTS "sd-bus-method-arguments"

#define RPC_SD_BUS_METHOD_XPATH "/generic-sdbus:sd-bus-call/" "sd-bus-result[sd-bus-method='%s']/sd-bus-method"
#define RPC_SD_BUS_RESPONSE_XPATH "/generic-sdbus:sd-bus-call/" "sd-bus-result[sd-bus-method='%s']/sd-bus-response"
#define RPC_SD_BUS_SIGNATURE_XPATH "/generic-sdbus:sd-bus-call/" "sd-bus-result[sd-bus-method='%s']/sd-bus-signature"

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
    char *sd_bus_reply_signature = NULL;
    sd_bus *bus = NULL;
    sd_bus_message *sd_message = NULL;
    sd_bus_message *sd_message_reply = NULL;
    sd_bus_error *error = NULL;
    struct lyd_node *node = NULL;
    struct lyd_node *child = NULL;
    struct lyd_node *next = NULL;
    struct lyd_node *ret = NULL;

	CHECK_NULL_MSG(input, rc, cleanup, "input is invalid");

	LY_TREE_FOR(input->child, child)
	{
		LY_TREE_DFS_BEGIN(child, next, node)
		{
			if (node->schema) {
				if (node->schema->nodetype == LYS_LEAF) {
					if (strcmp(RPC_SD_BUS, node->schema->name) == 0)
						sd_bus_bus = ((struct lyd_node_leaf_list *) node)->value.enm->name;
					else if (strcmp(RPC_SD_BUS_SERVICE, node->schema->name) == 0)
						sd_bus_service = ((struct lyd_node_leaf_list *) node)->value.string;
					else if (strcmp(RPC_SD_BUS_OBJPATH, node->schema->name) == 0)
						sd_bus_object_path = ((struct lyd_node_leaf_list *) node)->value.string;
					else if (strcmp(RPC_SD_BUS_INTERFACE, node->schema->name) == 0)
						sd_bus_interface = ((struct lyd_node_leaf_list *) node)->value.string;
					else if (strcmp(RPC_SD_BUS_METHOD, node->schema->name) == 0)
						sd_bus_method = ((struct lyd_node_leaf_list *) node)->value.string;
					else if (strcmp(RPC_SD_BUS_SIGNATURE, node->schema->name) == 0)
						sd_bus_method_signature = ((struct lyd_node_leaf_list *) node)->value.string;
					else if (strcmp(RPC_SD_BUS_ARGUMENTS, node->schema->name) == 0)
						sd_bus_method_arguments = ((struct lyd_node_leaf_list *) node)->value.string;
				}
				if ((node->schema->nodetype == LYS_LIST || !child->next) &&
					sd_bus_bus != NULL && sd_bus_service != NULL &&
					sd_bus_object_path != NULL && sd_bus_interface != NULL &&
					sd_bus_method != NULL && sd_bus_method_signature != NULL && sd_bus_method_arguments != NULL) {

                    if (strcmp(sd_bus_bus, "SYSTEM") == 0)
                        rc = sd_bus_open_system(&bus);
                	else
                        rc = sd_bus_open_user(&bus);
					SD_CHECK_RET(rc, cleanup, "failed to connect to system bus: %s", strerror(-rc));

					rc = sd_bus_message_new_method_call(
						bus, &sd_message, sd_bus_service, sd_bus_object_path,
						sd_bus_interface, sd_bus_method);
					SD_CHECK_RET(rc, cleanup, "failed to create a new message: %s", strerror(-rc));

                    rc = bus_message_encode(sd_bus_method_signature, sd_bus_method_arguments, sd_message);
					SD_CHECK_RET(rc, cleanup, "failed to append sd-bus method arguments: %s", strerror(-rc));

					rc = sd_bus_call(bus, sd_message, 0, error, &sd_message_reply);
					SD_CHECK_RET(rc, cleanup, "failed to call sd-bus method: %s", strerror(-rc));

					sd_bus_reply_signature = sd_bus_message_get_signature(sd_message, 1);
					CHECK_NULL_MSG(sd_bus_reply_signature, rc, cleanup, "failed get reply message signature");

                    rc = bus_message_decode(sd_message_reply, &sd_bus_reply_string);
					SD_CHECK_RET(rc, cleanup, "failed to parse reply: %s", strerror(-rc));

					xpath = realloc(xpath, strlen(RPC_SD_BUS_METHOD_XPATH) + strlen(sd_bus_method) + 1);
					sprintf(xpath, RPC_SD_BUS_METHOD_XPATH, sd_bus_method);
					ret = lyd_new_path(output, NULL, xpath, (void *) sd_bus_method, LYD_ANYDATA_STRING, LYD_PATH_OPT_OUTPUT);
					CHECK_NULL_MSG(ret, rc, cleanup, "failed to set output");

					xpath = realloc(xpath, strlen(RPC_SD_BUS_RESPONSE_XPATH) + strlen(sd_bus_method) + 1);
					sprintf(xpath, RPC_SD_BUS_RESPONSE_XPATH, sd_bus_method);
                    ret = lyd_new_path(output, NULL, xpath, (void *) sd_message_reply, LYD_ANYDATA_STRING, LYD_PATH_OPT_OUTPUT);
					CHECK_NULL_MSG(ret, rc, cleanup, "failed to set output");

					xpath = realloc(xpath, strlen(RPC_SD_BUS_SIGNATURE_XPATH) + strlen(sd_bus_method) + 1);
					sprintf(xpath, RPC_SD_BUS_SIGNATURE_XPATH, sd_bus_method);
					ret = lyd_new_path(output, NULL, xpath, (void *) sd_bus_reply_signature, LYD_ANYDATA_STRING, LYD_PATH_OPT_OUTPUT);
					CHECK_NULL_MSG(ret, rc, cleanup, "failed to set output");

					sd_bus_bus = NULL;
					sd_bus_service = NULL;
					sd_bus_object_path = NULL;
					sd_bus_interface = NULL;
					sd_bus_method = NULL;
					sd_bus_method_signature = NULL;
					sd_bus_method_arguments = NULL;
					free(sd_bus_reply_string);
					sd_bus_reply_string = NULL;
					free(sd_bus_reply_signature);
					sd_bus_reply_signature = NULL;
                }
            }

			LY_TREE_DFS_END(child, next, node)
		};
	};
cleanup:
    free(sd_bus_reply_string);
    free(xpath);
    sd_bus_message_unref(sd_message);
    sd_bus_message_unref(sd_message_reply);
    sd_bus_close(bus);
    sd_bus_unref(bus);
	free(sd_bus_reply_string);
	free(sd_bus_reply_signature);
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
	INF("%s", __func__);

	int error = 0;

	INF_MSG("Subscribing to sd-bus call rpc");
	error = sr_rpc_subscribe_tree(session, "/" YANG_MODEL ":sd-bus-call", generic_sdbus_call_rpc_tree_cb, NULL, 0, SR_SUBSCR_CTX_REUSE, subscription);
	SR_CHECK_RET(error, cleanup, "rpc subscription error: %s", sr_strerror(error));

	INF_MSG("Succesfull init");
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
	INF("%s", __func__);
	if (subscription != NULL) {
		sr_unsubscribe(subscription);
	}
	if (session != NULL) {
		sr_session_stop(session);
	}
	if (connection != NULL) {
		sr_disconnect(connection);
	}
	INF_MSG("Plugin cleaned-up successfully");
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
	INF_MSG("Sigint called, exiting...");
	exit_application = 1;
}

#endif