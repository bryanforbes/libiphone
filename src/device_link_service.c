 /* 
 * device_link_service.c
 * DeviceLink service implementation.
 * 
 * Copyright (c) 2010 Nikias Bassen, All Rights Reserved.
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
#include <string.h>
#include <stdlib.h>
#include "device_link_service.h"
#include "property_list_service.h"
#include "debug.h"

GQuark
device_link_service_error_quark (void)
{
  return g_quark_from_static_string ("device-link-service-error-quark");
}

/**
 * Internally used function to extract the message string from a DLMessage*
 * plist.
 *
 * @param dl_msg The DeviceLink property list to parse.
 *
 * @return An allocated char* with the DLMessage from the given plist,
 *     or NULL when the plist does not contain any DLMessage. It is up to
 *     the caller to free the allocated memory.
 */
static char *device_link_service_get_message(plist_t dl_msg)
{
	uint32_t cnt = 0;
	plist_t cmd = 0;
	char *cmd_str = NULL;

	/* sanity check */
	if ((plist_get_node_type(dl_msg) != PLIST_ARRAY) || ((cnt = plist_array_get_size(dl_msg)) < 1)) {
		return NULL;
	}

	/* get dl command */
	cmd = plist_array_get_item(dl_msg, 0);
	if (!cmd || (plist_get_node_type(cmd) != PLIST_STRING)) {
		return NULL;
	}

	plist_get_string_val(cmd, &cmd_str);
	if (!cmd_str) {
		return NULL;
	}

	if ((strlen(cmd_str) < (strlen("DLMessage")+1))
	    || (strncmp(cmd_str, "DLMessage", strlen("DLMessage")))) {
		free(cmd_str);
		return NULL;
	}

	/* we got a DLMessage* command */
	return cmd_str;
}

/**
 * Creates a new device link service client.
 *
 * @param device The device to connect to.
 * @param port Port on device to connect to.
 * @param client Reference that will point to a newly allocated
 *     device_link_service_client_t upon successful return.
 *
 * @return DEVICE_LINK_SERVICE_E_SUCCESS on success,
 *     DEVICE_LINK_SERVICE_E_INVALID_ARG when one of the parameters is invalid,
 *     or DEVICE_LINK_SERVICE_E_MUX_ERROR when the connection failed.
 */
device_link_service_client_t device_link_service_client_new(idevice_t device, uint16_t port, GError **error)
{
	GError *plist_error = NULL;

	g_assert(device != NULL && port > 0);

	property_list_service_client_t plistclient = property_list_service_client_new(device, port, &plist_error);
	if (plistclient == NULL) {
		g_set_error_literal(error, DEVICE_LINK_SERVICE_ERROR,
			DEVICE_LINK_SERVICE_E_MUX_ERROR,
			plist_error->message);
		g_error_free(plist_error);
		return NULL;
	}

	/* create client object */
	device_link_service_client_t client_loc = (device_link_service_client_t) malloc(sizeof(struct device_link_service_client_private));
	client_loc->parent = plistclient;

	/* all done, return client */
	return client_loc;
}

/**
 * Frees a device link service client.
 *
 * @param client The device_link_service_client_t to free.
 *
 * @return DEVICE_LINK_SERVICE_E_SUCCESS on success,
 *     DEVICE_LINK_SERVICE_E_INVALID_ARG when one of client or client->parent
 *     is invalid, or DEVICE_LINK_SERVICE_E_UNKNOWN_ERROR when the was an error
 *     freeing the parent property_list_service client.
 */
void device_link_service_client_free(device_link_service_client_t client, GError **error)
{
	g_assert(client != NULL);

	GError *plist_error = NULL;
	property_list_service_client_free(client->parent, &plist_error);
	if (plist_error != NULL) {
		g_propagate_error(error, plist_error);
	}
}

/**
 * Performs the DLMessageVersionExchange with the connected device.
 * This should be the first operation to be executed by an implemented
 * device link service client.
 *
 * @param client The device_link_service client to use.
 * @param version_major The major version number to check.
 * @param version_minor The minor version number to check.
 *
 * @return DEVICE_LINK_SERVICE_E_SUCCESS on success,
 *     DEVICE_LINK_SERVICE_E_INVALID_ARG when client is NULL,
 *     DEVICE_LINK_SERVICE_E_MUX_ERROR when a communication error occurs,
 *     DEVICE_LINK_SERVICE_E_PLIST_ERROR when the received plist has not the
 *     expected contents, DEVICE_LINK_SERVICE_E_BAD_VERSION when the version
 *     given by the device is larger than the given version,
 *     or DEVICE_LINK_SERVICE_E_UNKNOWN_ERROR otherwise.
 */
void device_link_service_version_exchange(device_link_service_client_t client, uint64_t version_major, uint64_t version_minor, GError **error)
{
	g_assert(client != NULL);

	/* perform version exchange */
	GError *plist_error = NULL;
	plist_t array = NULL;
	char *msg = NULL;

	/* receive DLMessageVersionExchange from device */
	array = property_list_service_receive_plist(client->parent, &plist_error);
	if (array == NULL) {
		debug_info("Did not receive initial message from device!");
		g_set_error(error, DEVICE_LINK_SERVICE_ERROR,
			DEVICE_LINK_SERVICE_E_MUX_ERROR,
			"Did not receive initial message from device");
		goto leave;
	}
	msg = device_link_service_get_message(array);
	if (!msg || strcmp(msg, "DLMessageVersionExchange")) {
		debug_info("Did not receive DLMessageVersionExchange from device!");
		g_set_error(error, DEVICE_LINK_SERVICE_ERROR,
			DEVICE_LINK_SERVICE_E_PLIST_ERROR,
			"Did no receive DLMessageVersionExchange from device");
		goto leave;
	}
	free(msg);
	msg = NULL;

	/* get major and minor version number */
	if (plist_array_get_size(array) < 3) {
		debug_info("DLMessageVersionExchange has unexpected format!");
		g_set_error(error, DEVICE_LINK_SERVICE_ERROR,
			DEVICE_LINK_SERVICE_E_PLIST_ERROR,
			"DLMessageVersionExchange has unexpected format");
		goto leave;
	}
	plist_t maj = plist_array_get_item(array, 1);
	plist_t min = plist_array_get_item(array, 2);
	uint64_t vmajor = 0;
	uint64_t vminor = 0;
	if (maj) {
		plist_get_uint_val(maj, &vmajor);
	}
	if (min) {
		plist_get_uint_val(min, &vminor);
	}
	if (vmajor > version_major) {
		debug_info("Version mismatch: device=(%lld,%lld) > expected=(%lld,%lld)", vmajor, vminor, version_major, version_minor);
		g_set_error(error, DEVICE_LINK_SERVICE_ERROR,
			DEVICE_LINK_SERVICE_E_BAD_VERSION,
			"Version mismatch: device=(%lld,%lld) > expected=(%lld,%lld)",
			(long long int)vmajor, (long long int)vminor, (long long int)version_major, (long long int)version_minor);
		goto leave;
	} else if ((vmajor == version_major) && (vminor > version_minor)) {
		debug_info("WARNING: Version mismatch: device=(%lld,%lld) > expected=(%lld,%lld)", vmajor, vminor, version_major, version_minor);
		g_set_error(error, DEVICE_LINK_SERVICE_ERROR,
			DEVICE_LINK_SERVICE_E_BAD_VERSION,
			"WARNING: Version mismatch: device=(%lld,%lld) > expected=(%lld,%lld)",
			(long long int)vmajor, (long long int)vminor, (long long int)version_major, (long long int)version_minor);
		goto leave;
	}
	plist_free(array);

	/* version is ok, send reply */
	array = plist_new_array();
	plist_array_append_item(array, plist_new_string("DLMessageVersionExchange"));
	plist_array_append_item(array, plist_new_string("DLVersionsOk"));
	plist_array_append_item(array, plist_new_uint(version_major));
	property_list_service_send_binary_plist(client->parent, array, &plist_error);
	if (plist_error != NULL) {
		debug_info("Error when sending DLVersionsOk");
		g_set_error(error, DEVICE_LINK_SERVICE_ERROR,
			DEVICE_LINK_SERVICE_E_MUX_ERROR,
			"Error when sending DLVersionsOk");
		goto leave;
	}
	plist_free(array);

	/* receive DeviceReady message */
	array = NULL;
	array = property_list_service_receive_plist(client->parent, &plist_error);
	if (array == NULL) {
		debug_info("Error when receiving DLMessageDeviceReady!");
		g_set_error(error, DEVICE_LINK_SERVICE_ERROR,
			DEVICE_LINK_SERVICE_E_MUX_ERROR,
			"Error when receiving DLMessageDeviceReady!");
		goto leave;
	}
	msg = device_link_service_get_message(array);
	if (!msg || strcmp(msg, "DLMessageDeviceReady")) {
		debug_info("Did not get DLMessageDeviceReady!");
		g_set_error(error, DEVICE_LINK_SERVICE_ERROR,
			DEVICE_LINK_SERVICE_E_PLIST_ERROR,
			"Did not get DLMessageDeviceReady!");
		goto leave;
	}

leave:
	if (msg) {
		free(msg);
	}
	if (plist_error) {
		g_error_free(plist_error);
	}
	if (array) {
		plist_free(array);
	}
}

/**
 * Performs a disconnect with the connected device link service client.
 *
 * @param client The device link service client to disconnect.
 * 
 * @return DEVICE_LINK_SERVICE_E_SUCCESS on success,
 *     DEVICE_LINK_SERVICE_E_INVALID_ARG if client is NULL,
 *     or DEVICE_LINK_SERVICE_E_MUX_ERROR when there's an error when sending
 *     the the disconnect message.
 */
void device_link_service_disconnect(device_link_service_client_t client, GError **error)
{
	g_assert(client != NULL);

	plist_t array = plist_new_array();
	plist_array_append_item(array, plist_new_string("DLMessageDisconnect"));
	plist_array_append_item(array, plist_new_string("All done, thanks for the memories"));

	GError *plist_error = NULL;
	property_list_service_send_binary_plist(client->parent, array, &plist_error);
	if (plist_error != NULL) {
		g_propagate_error(error, plist_error);
	}
	plist_free(array);
}

/**
 * Sends a DLMessagePing plist.
 *
 * @param client The device link service client to use.
 * @param message String to send as ping message.
 *
 * @return DEVICE_LINK_SERVICE_E_SUCCESS on success,
 *     DEVICE_LINK_SERVICE_E_INVALID_ARG if client or message is invalid,
 *     or DEVICE_LINK_SERVICE_E_MUX_ERROR if the DLMessagePing plist could
 *     not be sent.
 */
void device_link_service_send_ping(device_link_service_client_t client, const char *message, GError **error)
{
	g_assert(client != NULL && message != NULL);

	plist_t array = plist_new_array();
	plist_array_append_item(array, plist_new_string("DLMessagePing"));
	plist_array_append_item(array, plist_new_string(message));

	GError *plist_error = NULL;
	property_list_service_send_binary_plist(client->parent, array, &plist_error);
	if (plist_error != NULL) {
		g_propagate_error(error, plist_error);
	}
	plist_free(array);
}

/**
 * Sends a DLMessageProcessMessage plist.
 *
 * @param client The device link service client to use.
 * @param message PLIST_DICT to send.
 *
 * @return DEVICE_LINK_SERVICE_E_SUCCESS on success,
 *     DEVICE_LINK_SERVICE_E_INVALID_ARG if client or message is invalid or
 *     message is not a PLIST_DICT, or DEVICE_LINK_SERVICE_E_MUX_ERROR if
 *     the DLMessageProcessMessage plist could not be sent.
 */
void device_link_service_send_process_message(device_link_service_client_t client, plist_t message, GError **error)
{
	g_assert(client != NULL && client->parent != NULL && message != NULL);
	g_assert(plist_get_node_type(message) == PLIST_DICT);

	plist_t array = plist_new_array();
	plist_array_append_item(array, plist_new_string("DLMessageProcessMessage"));
	plist_array_append_item(array, plist_copy(message));

	GError *plist_error = NULL;
	property_list_service_send_binary_plist(client->parent, array, &plist_error);
	if (plist_error != NULL) {
		g_propagate_error(error, plist_error);
	}
	plist_free(array);
}

/**
 * Receives a DLMessageProcessMessage plist.
 *
 * @param client The connected device link service client used for receiving.
 * @param message Pointer to a plist that will be set to the contents of the
 *    message contents upon successful return.
 *
 * @return DEVICE_LINK_SERVICE_E_SUCCESS when a DLMessageProcessMessage was
 *    received, DEVICE_LINK_SERVICE_E_INVALID_ARG when client or message is
 *    invalid, DEVICE_LINK_SERVICE_E_PLIST_ERROR if the received plist is
 *    invalid or is not a DLMessageProcessMessage,
 *    or DEVICE_LINK_SERVICE_E_MUX_ERROR if receiving from device fails.
 */
plist_t device_link_service_receive_process_message(device_link_service_client_t client, GError **error)
{
	g_assert(client != NULL && client->parent != NULL);

	GError *plist_error = NULL;
	plist_t pmsg = property_list_service_receive_plist(client->parent, &plist_error);
	if (pmsg == NULL) {
		g_set_error_literal(error, DEVICE_LINK_SERVICE_ERROR,
			DEVICE_LINK_SERVICE_E_MUX_ERROR,
			plist_error->message);
		g_error_free(plist_error);
		return NULL;
	}

	char *msg = device_link_service_get_message(pmsg);
	if (!msg || strcmp(msg, "DLMessageProcessMessage")) {
		debug_info("Did not receive DLMessageProcessMessage as expected!");
		g_set_error(error, DEVICE_LINK_SERVICE_ERROR,
			DEVICE_LINK_SERVICE_E_PLIST_ERROR,
			"Did not receive DLMessageProcessMessage as expected!");
		goto leave;
	}

	if (plist_array_get_size(pmsg) != 2) {
		debug_info("Malformed plist received for DLMessageProcessMessage");
		g_set_error(error, DEVICE_LINK_SERVICE_ERROR,
			DEVICE_LINK_SERVICE_E_PLIST_ERROR,
			"Malformed plist received for DLMessageProcessMessage");
		goto leave;
	}

	plist_t res = NULL;
	plist_t msg_loc = plist_array_get_item(pmsg, 1);

	if (msg_loc) {
		res = plist_copy(msg_loc);
	} else {
		g_set_error(error, DEVICE_LINK_SERVICE_ERROR,
			DEVICE_LINK_SERVICE_E_PLIST_ERROR,
			"Property list error");
	}

leave:
	if (msg)
		free(msg);
	if (pmsg)
		plist_free(pmsg);

	return res;
}

/**
 * Generic device link service send function.
 *
 * @param client The device link service client to use for sending
 * @param plist The property list to send
 *
 * @return DEVICE_LINK_SERVICE_E_SUCCESS on success,
 *     DEVICE_LINK_SERVICE_E_INVALID_ARG when client or plist is NULL,
 *     or DEVICE_LINK_SERVICE_E_MUX_ERROR when the given property list could
 *     not be sent.
 */
void device_link_service_send(device_link_service_client_t client, plist_t plist, GError **error)
{
	g_assert(client != NULL && plist != NULL);

	property_list_service_send_binary_plist(client->parent, plist, error);
}

/* Generic device link service receive function.
 *
 * @param client The device link service client to use for sending
 * @param plist Pointer that will point to the property list received upon
 *     successful return.
 *
 * @return DEVICE_LINK_SERVICE_E_SUCCESS on success,
 *     DEVICE_LINK_SERVICE_E_INVALID_ARG when client or plist is NULL,
 *     or DEVICE_LINK_SERVICE_E_MUX_ERROR when no property list could be
 *     received.
 */
plist_t device_link_service_receive(device_link_service_client_t client, GError **error)
{
	g_assert(client != NULL && client->parent != NULL);

	GError *plist_error = NULL;
	plist_t plist = property_list_service_receive_plist(client->parent, &plist_error);
	if (plist_error != NULL) {
		g_propagate_error(error, plist_error);
		if (plist != NULL) {
			plist_free(plist);
		}
		return NULL;
	}
	return plist;
}

