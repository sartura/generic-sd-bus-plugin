/*
 * @file xpath.c
 * @author Luka Paulic <luka.paulic@sartura.hr>
 *
 * @brief Implements necessary xpath manipulation functions required in the
 *        plugin.
 *
 * @copyright
 * Copyright (C) 2019 Deutsche Telekom AG.
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
#include <string.h>

#include "sysrepo/xpath.h"

#include "xpath.h"
#include "common.h"

/*=========================Function definitions===============================*/

/*
 * @brief Get the name of the last list node.
 *
 * @param[in] xpath xpath to be examined.
 * @param[out] node name of the last list node.
 *
 * @note The name of the node needs to be fried by the caller.
 *
 * @return error code.
 *
*/
int xpath_get_tail_list_node(const char *xpath, char **node)
{
    int rc = SR_ERR_OK;
    *node = NULL;
    CHECK_NULL_MSG(xpath, &rc, cleanup, "input argument xpath is null");
    char* nth_ptr;

    char partial_xpath[256+1] = {0};

    int start_index = 0, end_index = 0;

    nth_ptr=strrchr(xpath, '[');
    if (nth_ptr == NULL)
    {
        rc = -2;
        goto cleanup;
    }
    end_index = (int)(nth_ptr - xpath + 1);

    snprintf(partial_xpath, end_index, "%s", xpath);

    nth_ptr=strrchr(partial_xpath, '/');
    if (nth_ptr == NULL)
    {
        rc = -2;
        goto cleanup;
    }
    start_index = (int)(nth_ptr - partial_xpath + 1);

    int len = end_index - start_index;

    char *name = calloc(1, sizeof(char)*len + 1);
    CHECK_NULL_MSG(name, &rc, cleanup, "allocation for name error");
    strncpy(name, nth_ptr+1, len);
    name[len] = '\0';

    *node = name;

cleanup:
    return rc;
}

/*
 * @brief Get the name of the last node, container of leaf
 *
 * @param[in] xpath xpath to be examined.
 * @param[out] node name of the last node.
 *
 * @note The name of the node needs to be fried by the caller.
 *
 * @return error code.
 *
*/
int xpath_get_tail_node(const char *xpath, char **node)
{
    int rc = SR_ERR_OK;
    *node = NULL;
    CHECK_NULL_MSG(xpath, &rc, cleanup, "input argument xpath is null");
    char* nth_ptr;

    nth_ptr=strrchr(xpath, '/');
    if (nth_ptr == NULL)
    {
        INF_MSG("'/' is not found");
        return -2;
    }
    //CHECK_NULL_MSG(nth_ptr, &rc, cleanup, "'/' is not found in string");

    int len = strlen(nth_ptr);

    char *name = calloc(1, sizeof(char)*len + 1);
    CHECK_NULL_MSG(name, &rc, cleanup, "allocation for name error");
    strncpy(name, nth_ptr+1, len);
    name[len] = '\0';

    *node = name;
cleanup:
    return rc;
}

/*
 * @brief Get the value of an node attribute.
 *
 * @param[in] xpath xpath to be examined.
 * @param[in] node_name name of the node which attribute is needed.
 * @param[in] key_name name of the attribute for which the value is needed.
 * @param[out] key_value value of the attribute to be returned
 *
 * @note The value of the attribute needs to be fried by the caller.
 *
 * @return error code.
 *
*/
int xpath_get_node_key_value(char *xpath, const char *node_name, const char *key_name, char **key_value)
{
    int rc = SR_ERR_OK;
    *key_value = NULL;
    CHECK_NULL_MSG(xpath, &rc, cleanup, "input argument xpath is null");
    CHECK_NULL_MSG(node_name, &rc, cleanup, "input argument node_name is null");
    CHECK_NULL_MSG(key_name, &rc, cleanup, "input argument key_name is null");

    sr_xpath_ctx_t ctx;
    char *key_val = sr_xpath_key_value(xpath, node_name, key_name, &ctx);
    CHECK_NULL_MSG(key_val, &rc, cleanup, "key value not found");
    int len = strlen(key_val);
    char *value = calloc(1, sizeof(char)*len + 1);
    CHECK_NULL_MSG(value, &rc, cleanup, "allocation for name error");
    strncpy(value, key_val, len);
    value[len] = '\0';

    *key_value = value;
cleanup:
    sr_xpath_recover(&ctx);
    return rc;
}

/*
 * @brief Get the name of the YANG module
 *
 * @param[in] xpath xpath to be examined.
 * @param[out] module_name module name from xpath to be returned.
 *
 * @note The name of the YANG module needs to be fried by the caller.
 *
 * @return error code.
 *
*/
int xpath_get_module_name(const char *xpath, char **module_name)
{
    int rc = SR_ERR_OK;
    *module_name = NULL;
    CHECK_NULL_MSG(xpath, &rc, cleanup, "input argument xpath is null");

    char *nth_ptr = strchr(xpath, ':');
    CHECK_NULL_MSG(nth_ptr, &rc, cleanup, "':' not found");

    int len = (int)(nth_ptr - xpath - 1);

    char *module = calloc(1, sizeof(char)*len + 1);
    CHECK_NULL_MSG(module, &rc, cleanup, "allocation for module error");
    strncpy(module, xpath+1, len);
    module[len] = '\0';

    *module_name = module;

    INF("%s", *module_name);

cleanup:
    return rc;
}
