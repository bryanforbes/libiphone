/*
 * mobilesync.c 
 * Contains functions for the built-in MobileSync client.
 * 
 * Copyright (c) 2009 Jonathan Beck All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA 
 */

#include <plist/plist.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

#include "mobilesync.h"
#include "device_link_service.h"
#include "debug.h"

#define MSYNC_VERSION_INT1 100
#define MSYNC_VERSION_INT2 100

#define EMPTY_PARAMETER_STRING "___EmptyParameterString___"

/**
 * Convert an device_link_service_error_t value to an mobilesync_error_t value.
 * Used internally to get correct error codes when using device_link_service stuff.
 *
 * @param err An device_link_service_error_t error code
 *
 * @return A matching mobilesync_error_t error code,
 *     MOBILESYNC_E_UNKNOWN_ERROR otherwise.
 */
static mobilesync_error_t mobilesync_error(device_link_service_error_t err)
{
	switch (err) {
		case DEVICE_LINK_SERVICE_E_SUCCESS:
			return MOBILESYNC_E_SUCCESS;
		case DEVICE_LINK_SERVICE_E_INVALID_ARG:
			return MOBILESYNC_E_INVALID_ARG;
		case DEVICE_LINK_SERVICE_E_PLIST_ERROR:
			return MOBILESYNC_E_PLIST_ERROR;
		case DEVICE_LINK_SERVICE_E_MUX_ERROR:
			return MOBILESYNC_E_MUX_ERROR;
		case DEVICE_LINK_SERVICE_E_BAD_VERSION:
			return MOBILESYNC_E_BAD_VERSION;
		default:
			break;
	}
	return MOBILESYNC_E_UNKNOWN_ERROR;
}

/**
 * Connects to the mobilesync service on the specified device.
 *
 * @param device The device to connect to.
 * @param port Destination port (usually given by lockdownd_start_service).
 * @param client Pointer that will be set to a newly allocated
 *     mobilesync_client_t upon successful return.
 *
 * @return MOBILESYNC_E_SUCCESS on success, MOBILESYNC_E_INVALID ARG if one
 *     or more parameters are invalid, or DEVICE_LINK_SERVICE_E_BAD_VERSION if
 *     the mobilesync version on the device is newer.
 */
mobilesync_error_t mobilesync_client_new(idevice_t device, uint16_t port,
						   mobilesync_client_t * client)
{
	if (!device || port == 0 || !client || *client)
		return MOBILESYNC_E_INVALID_ARG;

	device_link_service_client_t dlclient = NULL;
	mobilesync_error_t ret = mobilesync_error(device_link_service_client_new(device, port, &dlclient));
	if (ret != MOBILESYNC_E_SUCCESS) {
		return ret;
	}

	mobilesync_client_t client_loc = (mobilesync_client_t) malloc(sizeof(struct mobilesync_client_private));
	client_loc->parent = dlclient;

	/* perform handshake */
	ret = mobilesync_error(device_link_service_version_exchange(dlclient, MSYNC_VERSION_INT1, MSYNC_VERSION_INT2));
	if (ret != MOBILESYNC_E_SUCCESS) {
		debug_info("version exchange failed, error %d", ret);
		mobilesync_client_free(client_loc);
		return ret;
	}

	*client = client_loc;

	return ret;
}

/**
 * Disconnects a mobilesync client from the device and frees up the
 * mobilesync client data.
 *
 * @param client The mobilesync client to disconnect and free.
 *
 * @return MOBILESYNC_E_SUCCESS on success, or MOBILESYNC_E_INVALID_ARG
 *     if client is NULL.
 */
mobilesync_error_t mobilesync_client_free(mobilesync_client_t client)
{
	if (!client)
		return MOBILESYNC_E_INVALID_ARG;
	device_link_service_disconnect(client->parent);
	mobilesync_error_t err = mobilesync_error(device_link_service_client_free(client->parent));
	free(client);
	return err;
}

/**
 * Polls the device for mobilesync data.
 *
 * @param client The mobilesync client
 * @param plist A pointer to the location where the plist should be stored
 *
 * @return an error code
 */
mobilesync_error_t mobilesync_receive(mobilesync_client_t client, plist_t * plist)
{
	if (!client)
		return MOBILESYNC_E_INVALID_ARG;
	mobilesync_error_t ret = mobilesync_error(device_link_service_receive(client->parent, plist));
	return ret;
}

/**
 * Sends mobilesync data to the device
 * 
 * @note This function is low-level and should only be used if you need to send
 *        a new type of message.
 *
 * @param client The mobilesync client
 * @param plist The location of the plist to send
 *
 * @return an error code
 */
mobilesync_error_t mobilesync_send(mobilesync_client_t client, plist_t plist)
{
	if (!client || !plist)
		return MOBILESYNC_E_INVALID_ARG;
	return mobilesync_error(device_link_service_send(client->parent, plist));
}

mobilesync_error_t mobilesync_start_session(mobilesync_client_t client, const char* data_class, mobilesync_anchor_exchange_t anchor_exchange, mobilesync_sync_type_t *sync_type)
{
	if (!client || !data_class || !anchor_exchange || !anchor_exchange->device_anchor ||
		!anchor_exchange->host_anchor) {
		return MOBILESYNC_E_INVALID_ARG;
	}

	mobilesync_error_t err = MOBILESYNC_E_UNKNOWN_ERROR;

	plist_t msg = plist_new_array();

	plist_array_append_item(msg, plist_new_string("SDMessageSyncDataClassWithDevice"));
	plist_array_append_item(msg, plist_new_string(data_class));
	plist_array_append_item(msg, plist_new_string(anchor_exchange->device_anchor));
	plist_array_append_item(msg, plist_new_string(anchor_exchange->host_anchor));
	plist_array_append_item(msg, plist_new_uint(anchor_exchange->version));
	plist_array_append_item(msg, plist_new_string(EMPTY_PARAMETER_STRING));

	err = mobilesync_send(client, msg);

	if (err != MOBILESYNC_E_SUCCESS) {
		goto out;
	}

	plist_free(msg);
	msg = NULL;

	err = mobilesync_receive(client, &msg);

	if (err != MOBILESYNC_E_SUCCESS) {
		goto out;
	}

	plist_t response_type_node = plist_array_get_item(msg, 0);
	if (!response_type_node) {
		err = MOBILESYNC_E_PLIST_ERROR;
		goto out;
	}

	char *response_type = NULL;
	plist_get_string_val(response_type_node, &response_type);
	if (!response_type) {
		err = MOBILESYNC_E_PLIST_ERROR;
		goto out;
	}

	if (strcmp(response_type, "SDMessageRefuseToSyncDataClassWithComputer") == 0) {
		err = MOBILESYNC_E_SYNC_REFUSED;
		goto out;
	}

	if (strcmp(response_type, "SDMessageCancelSession") == 0) {
		err = MOBILESYNC_E_CANCELLED;
		goto out;
	}

	plist_t sync_type_node = plist_array_get_item(msg, 4);
	if (!sync_type_node) {
		err = MOBILESYNC_E_PLIST_ERROR;
		goto out;
	}

	char *sync_type_str = NULL;
	plist_get_string_val(sync_type_node, &sync_type_str);
	if (!sync_type_str) {
		err = MOBILESYNC_E_PLIST_ERROR;
		goto out;
	}

	if (strcmp(sync_type_str, "SDSyncTypeFast") == 0) {
		*sync_type = MOBILESYNC_SYNC_TYPE_FAST;
	} else if (strcmp(sync_type_str, "SDSyncTypeSlow") == 0) {
		*sync_type = MOBILESYNC_SYNC_TYPE_SLOW;
	} else if (strcmp(sync_type_str, "SDSyncTypeReset") == 0) {
		*sync_type = MOBILESYNC_SYNC_TYPE_RESET;
	} else {
		err = MOBILESYNC_E_PLIST_ERROR;
		goto out;
	}

	err = MOBILESYNC_E_SUCCESS;

	out:
	if (sync_type_str) {
		free(sync_type_str);
	}
	if (response_type) {
		free(response_type);
	}
	if (msg) {
		plist_free(msg);
	}
	return err;
}

mobilesync_error_t mobilesync_finish_session(mobilesync_client_t client, const char* data_class)
{
	if (!client || !data_class) {
		return MOBILESYNC_E_INVALID_ARG;
	}

	mobilesync_error_t err = MOBILESYNC_E_UNKNOWN_ERROR;

	plist_t msg = plist_new_array();
	plist_array_append_item(msg, plist_new_string("SDMessageFinishSessionOnDevice"));
	plist_array_append_item(msg, plist_new_string(data_class));

	err = mobilesync_send(client, msg);

	if (err != MOBILESYNC_E_SUCCESS) {
		goto out;
	}

	plist_free(msg);
	msg = NULL;

	err = mobilesync_receive(client, &msg);

	if (err != MOBILESYNC_E_SUCCESS) {
		goto out;
	}

	plist_t response_type_node = plist_array_get_item(msg, 0);
	if (!response_type_node) {
		err = MOBILESYNC_E_PLIST_ERROR;
		goto out;
	}

	char *response_type = NULL;
	plist_get_string_val(response_type_node, &response_type);
	if (!response_type) {
		err = MOBILESYNC_E_PLIST_ERROR;
		goto out;
	}

	if (strcmp(response_type, "SDMessageDeviceFinishedSession") == 0) {
		err = MOBILESYNC_E_SUCCESS;
	}

	out:
	if (response_type) {
		free(response_type);
	}
	if (msg) {
		plist_free(msg);
	}
	return err;
}

static mobilesync_error_t mobilesync_loop_records(mobilesync_client_t client, const char* operation, const char* data_class, mobilesync_process_device_changes_cb_t process_changes_cb, void *user_data)
{
	if (!client || !operation || !data_class) {
		return MOBILESYNC_E_INVALID_ARG;
	}

	mobilesync_error_t err = MOBILESYNC_E_UNKNOWN_ERROR;

	plist_t msg = plist_new_array();
	plist_array_append_item(msg, plist_new_string(operation));
	plist_array_append_item(msg, plist_new_string(data_class));
	
	err = mobilesync_send(client, msg);

	if (err != MOBILESYNC_E_SUCCESS) {
		goto out;
	}

	plist_free(msg);
	msg = NULL;
	char *response_type = NULL;
	uint8_t has_more_changes = 0;
	char *cancel_reason = NULL;

	do {
		err = mobilesync_receive(client, &msg);

		plist_t response_type_node = plist_array_get_item(msg, 0);
		if (!response_type_node) {
			err = MOBILESYNC_E_PLIST_ERROR;
			break;
		}

		plist_get_string_val(response_type_node, &response_type);
		if (!response_type) {
			err = MOBILESYNC_E_PLIST_ERROR;
			break;
		}

		if (strcmp(response_type, "SDMessageCancelSession") == 0) {
			err = MOBILESYNC_E_CANCELLED;
			break;
		}

		plist_get_bool_val(plist_array_get_item(msg, 3), &has_more_changes);

		if (process_changes_cb) {
			cancel_reason = process_changes_cb(data_class, plist_array_get_item(msg, 2), has_more_changes, user_data);
		}

		plist_free(msg);
		msg = NULL;

		msg = plist_new_array();

		if (cancel_reason == NULL) {
			plist_array_append_item(msg, plist_new_string("SDMessageAcknowledgeChangesFromDevice"));
			plist_array_append_item(msg, plist_new_string(data_class));
		} else {
			mobilesync_cancel(client, data_class, cancel_reason);
			free(cancel_reason);
			goto out;
		}

		err = mobilesync_send(client, msg);
		if (err != MOBILESYNC_E_SUCCESS) {
			goto out;
		}

		plist_free(msg);
		msg = NULL;
		free(response_type);

		if (!has_more_changes) {
			break;
		}

		has_more_changes = 0;
	} while (err == MOBILESYNC_E_SUCCESS);

	err = mobilesync_receive(client, &msg);

	out:
	if (response_type) {
		free(response_type);
	}
	if (msg) {
		plist_free(msg);
	}
	return err;
}

mobilesync_error_t mobilesync_get_all_records(mobilesync_client_t client, const char* data_class, mobilesync_process_device_changes_cb_t changes_cb, void *user_data)
{
	return mobilesync_loop_records(client, "SDMessageGetAllRecordsFromDevice", data_class, changes_cb, user_data);
}

mobilesync_error_t mobilesync_get_changed_records(mobilesync_client_t client, const char* data_class, mobilesync_process_device_changes_cb_t changes_cb, void *user_data)
{
	return mobilesync_loop_records(client, "SDMessageGetChangesFromDevice", data_class, changes_cb, user_data);
}

static plist_t create_process_changes_message(const char *data_class, plist_t entity_mapping, uint8_t more_changes, const char *entity_name)
{
	plist_t msg = plist_new_array();

	plist_array_append_item(msg, plist_new_string("SDMessageProcessChanges"));
	plist_array_append_item(msg, plist_copy(entity_mapping));
	plist_array_append_item(msg, plist_new_bool(more_changes));

	plist_t entity_array = plist_new_array();
	plist_array_append_item(entity_array, plist_new_string(entity_name));

	plist_t sync_actions = plist_new_dict();
	plist_dict_insert_item(sync_actions, "SyncDeviceLinkEntityNamesKey", entity_array);
	plist_dict_insert_item(sync_actions, "SyncDeviceLinkAllRecordsOfPulledEntityTypeSentKey", plist_new_bool(1));

	plist_array_append_item(msg, sync_actions);

	return msg;
}

mobilesync_error_t mobilesync_send_changes(mobilesync_client_t client, const char* data_class, plist_t *changes, mobilesync_process_device_remapping_cb_t process_remapping_cb, void *user_data)
{
	if (!client || !data_class || !changes) {
		return MOBILESYNC_E_INVALID_ARG;
	}

	if (plist_get_node_type(changes) != PLIST_DICT) {
		return MOBILESYNC_E_INVALID_ARG;
	}

	plist_dict_iter iter = NULL;
	plist_dict_new_iter(changes, &iter);

	mobilesync_error_t err = MOBILESYNC_E_UNKNOWN_ERROR;
	char *key = NULL;
	char *next_key = NULL;
	plist_t value_node = NULL;
	plist_t next_value_node = NULL;

	plist_t msg = NULL;
	char *response_type = NULL;
	char *cancel_reason = NULL;

	plist_dict_next_item(changes, iter, &key, &value_node);

	if (!value_node) {
		err = MOBILESYNC_E_INVALID_ARG;
		goto out;
	}

	if (mobilesync_error(device_link_service_send_ping(client->parent, "Preparing to get changes for device")) !=
		MOBILESYNC_E_SUCCESS) {
		goto out;
	}

	while (value_node) {
		plist_dict_next_item(changes, iter, &next_key, &next_value_node);

		msg = create_process_changes_message(data_class, value_node, (next_value_node != NULL ? 1 : 0), key);

		err = mobilesync_send(client, msg);
		if (err != MOBILESYNC_E_SUCCESS) {
			goto out;
		}
		plist_free(msg);
		msg = NULL;

		err = mobilesync_receive(client, &msg);
		if (err != MOBILESYNC_E_SUCCESS) {
			goto out;
		}

		plist_t response_type_node = plist_array_get_item(msg, 0);
		if (!response_type_node) {
			err = MOBILESYNC_E_PLIST_ERROR;
			break;
		}

		plist_get_string_val(response_type_node, &response_type);
		if (!response_type) {
			err = MOBILESYNC_E_PLIST_ERROR;
			break;
		}

		if (strcmp(response_type, "SDMessageCancelSession") == 0) {
			err = MOBILESYNC_E_CANCELLED;
			break;
		}

		if (strcmp(response_type, "SDMessageRemapRecordIdentifiers") != 0) {
			err = MOBILESYNC_E_PLIST_ERROR;
			break;
		}

		if (process_remapping_cb) {
			plist_t mapping = plist_array_get_item(msg, 2);
			if (plist_get_node_type(mapping) == PLIST_DICT) {
				cancel_reason = process_remapping_cb(data_class, plist_array_get_item(msg, 2), user_data);
			} else {
				cancel_reason = process_remapping_cb(data_class, NULL, user_data);
			}
		}

		if (cancel_reason != NULL) {
			mobilesync_cancel(client, data_class, cancel_reason);
			free(cancel_reason);
			goto out;
		}

		plist_free(msg);
		msg = NULL;

		free(key);
		key = next_key;
		value_node = next_value_node;
	}

	err = MOBILESYNC_E_SUCCESS;

	out:
	if (response_type) {
		free(response_type);
	}
	if (msg) {
		plist_free(msg);
	}
	if (next_key) {
		free(next_key);
	}
	if (key) {
		free(key);
	}
	if (iter) {
		free(iter);
	}

	return err;
}

mobilesync_error_t mobilesync_cancel(mobilesync_client_t client, const char* data_class, const char* reason)
{
	if (!client || !data_class || !reason) {
		return MOBILESYNC_E_INVALID_ARG;
	}

	plist_t msg = plist_new_array();
	plist_array_append_item(msg, plist_new_string("SDMessageCancelSession"));
	plist_array_append_item(msg, plist_new_string(data_class));
	plist_array_append_item(msg, plist_new_string(reason));

	mobilesync_error_t err = mobilesync_send(client, msg);

	plist_free(msg);

	return err;
}
