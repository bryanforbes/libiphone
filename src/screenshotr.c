/*
 * screenshotr.c 
 * com.apple.mobile.screenshotr service implementation.
 * 
 * Copyright (c) 2010 Nikias Bassen All Rights Reserved.
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

#include "screenshotr.h"
#include "device_link_service.h"
#include "debug.h"

#define SCREENSHOTR_VERSION_INT1 100
#define SCREENSHOTR_VERSION_INT2 0

GQuark
screenshotr_client_error_quark (void)
{
  return g_quark_from_static_string ("screenshotr-client-error-quark");
}

/**
 * Convert a device_link_service_error_t value to a screenshotr_error_t value.
 * Used internally to get correct error codes.
 *
 * @param err An device_link_service_error_t error code
 *
 * @return A matching screenshotr_error_t error code,
 *     SCREENSHOTR_E_UNKNOWN_ERROR otherwise.
 */
static screenshotr_error_t screenshotr_error(device_link_service_error_t err)
{
	switch (err) {
		case DEVICE_LINK_SERVICE_E_SUCCESS:
			return SCREENSHOTR_E_SUCCESS;
		case DEVICE_LINK_SERVICE_E_INVALID_ARG:
			return SCREENSHOTR_E_INVALID_ARG;
		case DEVICE_LINK_SERVICE_E_PLIST_ERROR:
			return SCREENSHOTR_E_PLIST_ERROR;
		case DEVICE_LINK_SERVICE_E_MUX_ERROR:
			return SCREENSHOTR_E_MUX_ERROR;
		case DEVICE_LINK_SERVICE_E_BAD_VERSION:
			return SCREENSHOTR_E_BAD_VERSION;
		default:
			break;
	}
	return SCREENSHOTR_E_UNKNOWN_ERROR;
}

/**
 * Connects to the screenshotr service on the specified device.
 *
 * @param device The device to connect to.
 * @param port Destination port (usually given by lockdownd_start_service).
 * @param client Pointer that will be set to a newly allocated
 *     screenshotr_client_t upon successful return.
 *
 * @note This service is only available if a developer disk image has been
 *     mounted.
 *
 * @return SCREENSHOTR_E_SUCCESS on success, SCREENSHOTR_E_INVALID ARG if one
 *     or more parameters are invalid, or SCREENSHOTR_E_CONN_FAILED if the
 *     connection to the device could not be established.
 */
screenshotr_client_t screenshotr_client_new(idevice_t device, uint16_t port, GError **error)
{
	g_assert(device != NULL && port > 0);

	GError *tmp_error = NULL;
	device_link_service_client_t dlclient = device_link_service_client_new(device, port, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error(error, tmp_error);
		return NULL;
	}

	screenshotr_client_t client_loc = (screenshotr_client_t) malloc(sizeof(struct screenshotr_client_private));
	client_loc->parent = dlclient;

	/* perform handshake */
	device_link_service_version_exchange(dlclient, SCREENSHOTR_VERSION_INT1, SCREENSHOTR_VERSION_INT2, &tmp_error);
	if (tmp_error != NULL) {
		debug_info("version exchange failed, error %d, reason %s", tmp_error->code, tmp_error->message);
		screenshotr_client_free(client_loc, NULL);
		g_propagate_error(error, tmp_error);
		return NULL;
	}

	return client_loc;
}

/**
 * Disconnects a screenshotr client from the device and frees up the
 * screenshotr client data.
 *
 * @param client The screenshotr client to disconnect and free.
 *
 * @return SCREENSHOTR_E_SUCCESS on success, or SCREENSHOTR_E_INVALID_ARG
 *     if client is NULL.
 */
void screenshotr_client_free(screenshotr_client_t client, GError **error)
{
	g_assert(client != NULL);

	device_link_service_disconnect(client->parent, NULL);

	GError *tmp_error = NULL;
	device_link_service_client_free(client->parent, &tmp_error);
	free(client);
	if (tmp_error != NULL)
		g_propagate_error(error, tmp_error);
}

/**
 * Get a screen shot from the connected device.
 *
 * @param client The connection screenshotr service client.
 * @param imgdata Pointer that will point to a newly allocated buffer
 *     containing TIFF image data upon successful return. It is up to the
 *     caller to free the memory.
 * @param imgsize Pointer to a uint64_t that will be set to the size of the
 *     buffer imgdata points to upon successful return.
 *
 * @return SCREENSHOTR_E_SUCCESS on success, SCREENSHOTR_E_INVALID_ARG if
 *     one or more parameters are invalid, or another error code if an
 *     error occured.
 */
void screenshotr_take_screenshot(screenshotr_client_t client, char **imgdata, uint64_t *imgsize, GError **error)
{
	g_assert(client != NULL && client->parent != NULL && imgdata != NULL);

	plist_t dict = plist_new_dict();
	plist_dict_insert_item(dict, "MessageType", plist_new_string("ScreenShotRequest"));

	GError *tmp_error = NULL;
	device_link_service_send_process_message(client->parent, dict, &tmp_error);
	plist_free(dict);
	if (tmp_error != NULL) {
		debug_info("could not send plist, error %d, reason %s", tmp_error->code, tmp_error->message);
		g_propagate_error(error, tmp_error);
		return;
	}

	dict = NULL;
	dict = device_link_service_receive_process_message(client->parent, &tmp_error);
	if (tmp_error != NULL) {
		debug_info("could not get screenshot data, error %d, reason %s", tmp_error->code, tmp_error->message);
		g_propagate_error(error, tmp_error);
		goto leave;
	}
	if (!dict) {
		debug_info("did not receive screenshot data!");
		g_set_error(error, SCREENSHOTR_CLIENT_ERROR,
			SCREENSHOTR_E_PLIST_ERROR,
			"Did not receive screenshot data");
		goto leave;
	}

	plist_t node = plist_dict_get_item(dict, "MessageType");
	char *strval = NULL;
	plist_get_string_val(node, &strval);
	if (!strval || strcmp(strval, "ScreenShotReply")) {
		debug_info("invalid screenshot data received!");
		g_set_error(error, SCREENSHOTR_CLIENT_ERROR,
			SCREENSHOTR_E_PLIST_ERROR,
			"Invalid screenshot data received");
		goto leave;
	}
	node = plist_dict_get_item(dict, "ScreenShotData");
	if (!node || plist_get_node_type(node) != PLIST_DATA) {
		debug_info("no PNG data received!");
		g_set_error(error, SCREENSHOTR_CLIENT_ERROR,
			SCREENSHOTR_E_PLIST_ERROR,
			"No PNG data received");
		goto leave;
	}

	plist_get_data_val(node, imgdata, imgsize);
leave:
	if (dict)
		plist_free(dict);
}
