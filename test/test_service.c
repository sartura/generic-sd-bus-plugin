/**
 * @file test_service.c
 * @authors Borna Blazevic <borna.blazevic@sartura.hr>
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
#include <errno.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>

#include <transform-sd-bus.h>

#define TEST_1_SIGNATURE "s"
//For the input '"str_arg"'
#define TEST_1_EXPECTED_RESULT "\"str_arg\""

#define TEST_2_SIGNATURE "x"
//For the input '15'
#define TEST_2_EXPECTED_RESULT "15"

#define TEST_3_SIGNATURE "d"
//For the input '1.1532'
#define TEST_3_EXPECTED_RESULT "1.1532"

#define TEST_4_SIGNATURE "v"
//For the input 'au 1 14460'
#define TEST_4_EXPECTED_RESULT "au 1 14460"

#define TEST_5_SIGNATURE "a{ss}"
//For the input '2 "str_arg" "str_arg" "str_arg" "str_arg"'
#define TEST_5_EXPECTED_RESULT "2 \"str_arg\" \"str_arg\" \"str_arg\" \"str_arg\""

#define TEST_6_SIGNATURE "a(ssso)"
//For the input '2 "str_arg" "str_arg" "str_arg" /test/test "str_arg" "str_arg" "str_arg" /test/test'
#define TEST_6_EXPECTED_RESULT "2 \"str_arg\" \"str_arg\" \"str_arg\" /test/test \"str_arg\" \"str_arg\" \"str_arg\" /test/test"

#define TEST_7_SIGNATURE "asssbb"
//For the input '4 "str_arg" "str_arg" "str_arg" "str_arg" "str_arg" "str_arg" true false'
#define TEST_7_EXPECTED_RESULT "4 \"str_arg\" \"str_arg\" \"str_arg\" \"str_arg\" \"str_arg\" \"str_arg\" true false"

#define TEST_8_SIGNATURE "sayssusaia(sv)"
//For the input '"str_arg" 2 10 20 "str_arg" "str_arg" 1000 "str_arg" 3 1 2 3 3 "str_arg" s "str_arg" "str_arg" u 1000 "str_arg" b true'
#define TEST_8_EXPECTED_RESULT "\"str_arg\" 2 10 20 \"str_arg\" \"str_arg\" 1000 \"str_arg\" 3 1 2 3 3 \"str_arg\" s \"str_arg\" \"str_arg\" u 1000 \"str_arg\" b true"

#define TEST_9_SIGNATURE "ssa(sv)a(sa(sv))"
//For the input '"str_arg" "str_arg" 2 "str_arg" au 1 14460 "str_arg" s "str_arg" 2 "str_arg" 3 "str_arg" y 1 "str_arg" u 2 "str_arg" x 3 "str_arg" 0'
#define TEST_9_EXPECTED_RESULT "\"str_arg\" \"str_arg\" 2 \"str_arg\" au 1 14460 \"str_arg\" s \"str_arg\" 2 \"str_arg\" 3 \"str_arg\" y 1 \"str_arg\" u 2 \"str_arg\" x 3 \"str_arg\" 0"

//Function declarations
static int method_test1(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_test2(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_test3(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_test4(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_test5(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_test6(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_test7(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_test8(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_test9(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int execute_test(const char *test_print_format, sd_bus_message *m, const char *result);

/* The vtable of our little object, implements the net.poettering.Calculator interface */
static const sd_bus_vtable test_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Test1", TEST_1_SIGNATURE, "x", method_test1, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Test2", TEST_2_SIGNATURE, "x", method_test2, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Test3", TEST_3_SIGNATURE, "x", method_test3, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Test4", TEST_4_SIGNATURE, "x", method_test4, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Test5", TEST_5_SIGNATURE, "x", method_test5, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Test6", TEST_6_SIGNATURE, "x", method_test6, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Test7", TEST_7_SIGNATURE, "x", method_test7, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Test8", TEST_8_SIGNATURE, "x", method_test8, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Test9", TEST_9_SIGNATURE, "x", method_test9, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END};

int main(int argc, char *argv[]) {
    sd_bus_slot *slot = NULL;
    sd_bus *bus = NULL;
    int r;

    /* Connect to the user bus this time */
    r = sd_bus_open_user(&bus);
    if (r < 0) {
        printf("Failed to connect to system bus: %s\n", strerror(-r));
        goto finish;
    }

    /* Install the object */
    r = sd_bus_add_object_vtable(bus,
                                 &slot,
                                 "/net/sysrepo/SDBUSTest", /* object path */
                                 "net.sysrepo.SDBUSTest",  /* interface name */
                                 test_vtable,
                                 NULL);
    if (r < 0) {
        printf("Failed to issue method call: %s\n", strerror(-r));
        goto finish;
    }

    /* Take a well-known service name so that clients can find us */
    r = sd_bus_request_name(bus, "net.sysrepo.SDBUSTest", 0);
    if (r < 0) {
        printf("Failed to acquire service name: %s\n", strerror(-r));
        goto finish;
    }

    for (;;) {
        /* Process requests */
        r = sd_bus_process(bus, NULL);
        if (r < 0) {
            printf("Failed to process bus: %s\n", strerror(-r));
            goto finish;
        }
        if (r > 0) /* we processed a request, try to process another one, right-away */
            continue;

        /* Wait for the next request to process */
        r = sd_bus_wait(bus, (uint64_t)-1);
        if (r < 0) {
            printf("Failed to wait on bus: %s\n", strerror(-r));
            goto finish;
        }
    }

finish:
    sd_bus_slot_unref(slot);
    sd_bus_unref(bus);

    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

static int method_test1(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    return execute_test(TEST_1_SIGNATURE, m, TEST_1_EXPECTED_RESULT);
}

static int method_test2(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    return execute_test(TEST_2_SIGNATURE, m, TEST_2_EXPECTED_RESULT);
}

static int method_test3(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    return execute_test(TEST_3_SIGNATURE, m, TEST_3_EXPECTED_RESULT);
}

static int method_test4(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    return execute_test(TEST_4_SIGNATURE, m, TEST_4_EXPECTED_RESULT);
}

static int method_test5(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    return execute_test(TEST_5_SIGNATURE, m, TEST_5_EXPECTED_RESULT);
}

static int method_test6(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    return execute_test(TEST_6_SIGNATURE, m, TEST_6_EXPECTED_RESULT);
}

static int method_test7(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    return execute_test(TEST_7_SIGNATURE, m, TEST_7_EXPECTED_RESULT);
}

static int method_test8(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    return execute_test(TEST_8_SIGNATURE, m, TEST_8_EXPECTED_RESULT);
}

static int method_test9(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    return execute_test(TEST_9_SIGNATURE, m, TEST_9_EXPECTED_RESULT);
}

static int execute_test(const char *test_signature, sd_bus_message *m, const char *result) {
    int r;
	char *message = NULL;

    /* Read the parameters */
    r = bus_message_decode(m, &message);
    if (r < 0) {
        printf("Failed to parse message: %s\n", strerror(-r));
        return sd_bus_reply_method_return(m, "x", 0);
    }

    printf("TEST SIGNATURE %s\n", test_signature);

	if (strcmp(message, result) == 0)
	{
    	printf("TEST SUCCESSFULL\n");
		free(message);
    	return sd_bus_reply_method_return(m, "x", 0);
	} else
	{
    	printf("TEST FAILED\n");
    	printf("got:\n\t%s\nexpected:\n\t%s\n", message, result);
		free(message);
    	return sd_bus_reply_method_return(m, "x", 1);
	}
}
