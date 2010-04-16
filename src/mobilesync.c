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

#include "mobilesync.h"
#include "device_link_service.h"
#include "debug.h"

#define MSYNC_VERSION_INT1 100
#define MSYNC_VERSION_INT2 100

GQuark
mobilesync_client_error_quark (void)
{
  return g_quark_from_static_string ("mobilesync-client-error-quark");
}

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
mobilesync_client_t mobilesync_client_new(idevice_t device, uint16_t port, GError **error)
{
	g_assert(device != NULL && port > 0);

	GError *tmp_error = NULL;
	device_link_service_client_t dlclient = device_link_service_client_new(device, port, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error(error, tmp_error);
		return NULL;
	}

	mobilesync_client_t client_loc = (mobilesync_client_t) malloc(sizeof(struct mobilesync_client_private));
	client_loc->parent = dlclient;

	/* perform handshake */
	device_link_service_version_exchange(dlclient, MSYNC_VERSION_INT1, MSYNC_VERSION_INT2, &tmp_error);
	if (tmp_error != NULL) {
		debug_info("version exchange failed, error %d, reason %s", tmp_error->code, tmp_error->message);
		mobilesync_client_free(client_loc, NULL);
		g_propagate_error(error, tmp_error);
		return NULL;
	}

	return client_loc;
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
void mobilesync_client_free(mobilesync_client_t client, GError **error)
{
	g_assert(client != NULL);

	device_link_service_disconnect(client->parent, NULL);

	device_link_service_client_free(client->parent, error);
	free(client);
}

/**
 * Polls the device for mobilesync data.
 *
 * @param client The mobilesync client
 * @param plist A pointer to the location where the plist should be stored
 *
 * @return an error code
 */
plist_t mobilesync_receive(mobilesync_client_t client, GError **error)
{
	g_assert(client != NULL);
	
	return device_link_service_receive(client->parent, error);
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
void mobilesync_send(mobilesync_client_t client, plist_t plist, GError **error)
{
	g_assert(client != NULL && plist != NULL);
	device_link_service_send(client->parent, plist, error);
}
