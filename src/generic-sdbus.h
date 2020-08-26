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