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
#include <generic-sdbus.h>


#define YANG_MODEL "generic-sdbus"
#define SYSREPOCFG_EMPTY_CHECK_COMMAND "sysrepocfg -X -d startup -m " YANG_MODEL

typedef struct sr_ctx_s {
  const char *yang_model;
  sr_session_ctx_t *sess;
  sr_subscription_ctx_t *sub;
  sr_conn_ctx_t *startup_conn;
  sr_session_ctx_t *startup_sess;
} sr_ctx_t;

// static bool sdbus_running_datastore_is_empty_check(void)
// {
// 	FILE *sysrepocfg_DS_empty_check = NULL;
// 	bool is_empty = false;

// 	sysrepocfg_DS_empty_check = popen(SYSREPOCFG_EMPTY_CHECK_COMMAND, "r");
// 	if (sysrepocfg_DS_empty_check == NULL) {
// 		SRP_LOG_WRN("could not execute %s", SYSREPOCFG_EMPTY_CHECK_COMMAND);
// 		is_empty = true;
// 		goto out;
// 	}

// 	if (fgetc(sysrepocfg_DS_empty_check) == EOF) {
// 		is_empty = true;
// 	}

// out:
// 	if (sysrepocfg_DS_empty_check) {
// 		pclose(sysrepocfg_DS_empty_check);
// 	}

// 	return is_empty;
// }

int sr_plugin_init_cb(sr_session_ctx_t *session, void **private_ctx)
{
	INF("%s", __func__);

	int error = 0;

  	sr_ctx_t *ctx = calloc(1, sizeof(*ctx));
	ctx->sess = session;
	ctx->startup_conn = NULL;
  	ctx->startup_sess = NULL;
  	ctx->yang_model = YANG_MODEL;

	*private_ctx = ctx;

	// SRP_LOG_INFMSG("start session to startup datastore");

  	// error = sr_connect(SR_CONN_DEFAULT, &ctx->startup_conn);
  	// CHECK_RET(error, cleanup, "Error by sr_connect: %s", sr_strerror(error));

  	// error = sr_session_start(ctx->startup_conn, SR_DS_STARTUP, &ctx->startup_sess);
  	// CHECK_RET(error, cleanup, "Error by sr_session_start: %s", sr_strerror(error));

	// if (sdbus_running_datastore_is_empty_check() == false) {
	// 	SRP_LOG_INFMSG("startup DS is not empty");
	// }

	INF_MSG("Subscribing to sd-bus call rpc");
	error = sr_rpc_subscribe(session, "/" YANG_MODEL ":sd-bus-call", generic_sdbus_call_rpc_cb, *private_ctx, 0, SR_SUBSCR_CTX_REUSE, &ctx->sub);
	SR_CHECK_RET(error, cleanup_sub, "rpc subscription error: %s", sr_strerror(error));

	INF_MSG("Succesfull init");
	return SR_ERR_OK;

cleanup:
	if (ctx->sub != NULL) {
			sr_unsubscribe(ctx->sub);
	}
cleanup_sub:
    if (ctx->startup_sess != NULL) {
			sr_session_stop(ctx->startup_sess);
		}
	if (ctx->startup_conn != NULL) {
			sr_disconnect(ctx->startup_conn);
	}
	return error;
}

/*
 * @brief Cleans the private context passed to the callbacks and unsubscribes
 * 		  from all subscriptions.
 *
 * @param[in] session session context for unsubscribing.
 * @param[in] private_ctx context to be released fro memory.
 *
 */
void sr_plugin_cleanup_cb(sr_session_ctx_t *session, void *private_ctx)
{
	INF("%s", __func__);
	INF("Plugin cleanup called, private_ctx is %s available.", private_ctx ? "" : "not");

	if (NULL != private_ctx) {
		sr_ctx_t *ctx = private_ctx;
		if (ctx->sub != NULL) {
			sr_unsubscribe(ctx->sub);
		}
		if (ctx->startup_sess != NULL) {
			sr_session_stop(ctx->startup_sess);
		}
		if (ctx->startup_conn != NULL) {
			sr_disconnect(ctx->startup_conn);
		}

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
	void *private_data = NULL;

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

	error = sr_plugin_init_cb(session, &private_data);
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

	sr_plugin_cleanup_cb(session, private_data);

out:
  	if (NULL != session) {
    	error = sr_session_stop(session);
  	}
  	if (NULL != connection) {
    	sr_disconnect(connection);
  	}
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