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
	client_loc->started_send_changes = 0;
	client_loc->data_class = NULL;

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

mobilesync_error_t mobilesync_session_start(mobilesync_client_t client, const char *data_class, mobilesync_anchors_t anchors, mobilesync_sync_type_t *sync_type, uint64_t *data_class_version)
{
	if (!client || client->data_class || !data_class ||
		!anchors || !anchors->host_anchor) {
		return MOBILESYNC_E_INVALID_ARG;
	}

	mobilesync_error_t err = MOBILESYNC_E_UNKNOWN_ERROR;

	plist_t msg = plist_new_array();

	plist_array_append_item(msg, plist_new_string("SDMessageSyncDataClassWithDevice"));
	plist_array_append_item(msg, plist_new_string(data_class));
	if (anchors->device_anchor) {
		plist_array_append_item(msg, plist_new_string(anchors->device_anchor));
	} else {
		plist_array_append_item(msg, plist_new_string("---"));
	}
	plist_array_append_item(msg, plist_new_string(anchors->host_anchor));
	plist_array_append_item(msg, plist_new_uint(106));
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

	char *sync_type_str = NULL;
	if (sync_type != NULL) {
		plist_t sync_type_node = plist_array_get_item(msg, 4);
		if (!sync_type_node) {
			err = MOBILESYNC_E_PLIST_ERROR;
			goto out;
		}

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
	}

	if (data_class_version != NULL) {
		plist_t data_class_version_node = plist_array_get_item(msg, 5);
		if (!data_class_version_node) {
			err = MOBILESYNC_E_PLIST_ERROR;
			goto out;
		}

		plist_get_uint_val(data_class_version_node, data_class_version);
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

	client->data_class = strdup(data_class);
	client->started_send_changes = 0;
	return err;
}

mobilesync_error_t mobilesync_session_finish(mobilesync_client_t client)
{
	if (!client || !client->data_class) {
		return MOBILESYNC_E_INVALID_ARG;
	}

	mobilesync_error_t err = MOBILESYNC_E_UNKNOWN_ERROR;

	plist_t msg = plist_new_array();
	plist_array_append_item(msg, plist_new_string("SDMessageFinishSessionOnDevice"));
	plist_array_append_item(msg, plist_new_string(client->data_class));

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

	free(client->data_class);
	client->data_class = NULL;
	client->started_send_changes = 0;
	return err;
}

static mobilesync_error_t mobilesync_get_records(mobilesync_client_t client, const char *operation)
{
	if (!client || !client->data_class || !operation) {
		return MOBILESYNC_E_INVALID_ARG;
	}

	mobilesync_error_t err = MOBILESYNC_E_UNKNOWN_ERROR;

	plist_t msg = plist_new_array();
	plist_array_append_item(msg, plist_new_string(operation));
	plist_array_append_item(msg, plist_new_string(client->data_class));
	
	err = mobilesync_send(client, msg);

	if (msg) {
		plist_free(msg);
	}
	return err;
}

mobilesync_error_t mobilesync_get_all_records_from_device(mobilesync_client_t client)
{
	return mobilesync_get_records(client, "SDMessageGetAllRecordsFromDevice");
}

mobilesync_error_t mobilesync_get_changes_from_device(mobilesync_client_t client)
{
	return mobilesync_get_records(client, "SDMessageGetChangesFromDevice");
}

mobilesync_error_t mobilesync_receive_changes(mobilesync_client_t client, plist_t *entities, uint8_t *is_last_record)
{
	if (!client || !client->data_class) {
		return MOBILESYNC_E_INVALID_ARG;
	}

	plist_t msg = NULL;

	mobilesync_error_t err = mobilesync_receive(client, &msg);

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

	if (strcmp(response_type, "SDMessageCancelSession") == 0) {
		err = MOBILESYNC_E_CANCELLED;
		goto out;
	}

	*entities = plist_copy(plist_array_get_item(msg, 2));

	uint8_t has_more_changes = 0;
	plist_get_bool_val(plist_array_get_item(msg, 3), &has_more_changes);
	*is_last_record = (has_more_changes > 0 ? 0 : 1);

	out:
	if (response_type) {
		free(response_type);
	}
	if (msg) {
		plist_free(msg);
	}
	return err;
}

mobilesync_error_t mobilesync_acknowledge_changes_from_device(mobilesync_client_t client)
{
	if (!client || !client->data_class) {
		return MOBILESYNC_E_INVALID_ARG;
	}

	plist_t msg = plist_new_array();

	plist_array_append_item(msg, plist_new_string("SDMessageAcknowledgeChangesFromDevice"));
	plist_array_append_item(msg, plist_new_string(client->data_class));

	mobilesync_error_t err = mobilesync_send(client, msg);
	plist_free(msg);
	return err;
}

static plist_t create_process_changes_message(const char *data_class, plist_t entity_mapping, uint8_t more_changes, const char **entity_names, uint32_t entity_names_length, uint8_t report_and_remap)
{
	plist_t msg = plist_new_array();

	plist_array_append_item(msg, plist_new_string("SDMessageProcessChanges"));
	plist_array_append_item(msg, plist_copy(entity_mapping));
	plist_array_append_item(msg, plist_new_bool(more_changes));

	plist_t entity_array = plist_new_array();

	uint32_t i = 0;
	for (i = 0; i < entity_names_length; i++) {
		plist_array_append_item(entity_array, plist_new_string(entity_names[i]));
	}

	plist_t sync_actions = plist_new_dict();
	plist_dict_insert_item(sync_actions, "SyncDeviceLinkEntityNamesKey", entity_array);
	plist_dict_insert_item(sync_actions, "SyncDeviceLinkAllRecordsOfPulledEntityTypeSentKey", plist_new_bool(report_and_remap));

	plist_array_append_item(msg, sync_actions);

	return msg;
}

mobilesync_error_t mobilesync_send_changes(mobilesync_client_t client, plist_t changes, uint8_t is_last_record, const char **entity_names, uint32_t entity_names_length, uint8_t report_and_remap)
{
	if (!client || !client->data_class || !changes) {
		return MOBILESYNC_E_INVALID_ARG;
	}

	if (plist_get_node_type(changes) != PLIST_DICT) {
		return MOBILESYNC_E_INVALID_ARG;
	}

	mobilesync_error_t err = MOBILESYNC_E_UNKNOWN_ERROR;
	plist_t msg = NULL;

	if (!client->started_send_changes) {
		err = mobilesync_error(device_link_service_send_ping(client->parent, "Preparing to get changes for device"));
		if (err != MOBILESYNC_E_SUCCESS) {
			goto out;
		}
		client->started_send_changes = 1;
		err = MOBILESYNC_E_UNKNOWN_ERROR;
	}

	create_process_changes_message(client->data_class, changes, (is_last_record > 0 ? 0 : 1), entity_names, entity_names_length, report_and_remap);

	err = mobilesync_send(client, msg);

	out:
	if (msg) {
		plist_free(msg);
	}

	return err;
}

mobilesync_error_t mobilesync_receive_remapping(mobilesync_client_t client, plist_t *remapping)
{
	if (!client || !client->data_class || !client->started_send_changes) {
		return MOBILESYNC_E_INVALID_ARG;
	}

	plist_t msg = NULL;

	mobilesync_error_t err = mobilesync_receive(client, &msg);
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

	if (strcmp(response_type, "SDMessageCancelSession") == 0) {
		err = MOBILESYNC_E_CANCELLED;
		goto out;
	}

	if (strcmp(response_type, "SDMessageRemapRecordIdentifiers") != 0) {
		err = MOBILESYNC_E_PLIST_ERROR;
		goto out;
	}

	if (remapping != NULL) {
		plist_t mapping = plist_array_get_item(msg, 2);
		if (plist_get_node_type(mapping) == PLIST_DICT) {
			*remapping = plist_copy(mapping);
		} else {
			*remapping = NULL;
		}
	}

	err = MOBILESYNC_E_SUCCESS;

	out:
	if (response_type) {
		free(response_type);
	}
	if (msg) {
		plist_free(msg);
	}

	return err;
}

mobilesync_error_t mobilesync_cancel(mobilesync_client_t client, const char* reason)
{
	if (!client || !client->data_class || !reason) {
		return MOBILESYNC_E_INVALID_ARG;
	}

	plist_t msg = plist_new_array();
	plist_array_append_item(msg, plist_new_string("SDMessageCancelSession"));
	plist_array_append_item(msg, plist_new_string(client->data_class));
	plist_array_append_item(msg, plist_new_string(reason));

	mobilesync_error_t err = mobilesync_send(client, msg);

	plist_free(msg);
	msg = NULL;

	free(client->data_class);
	client->data_class = NULL;
	client->started_send_changes = 0;

	return err;
}
