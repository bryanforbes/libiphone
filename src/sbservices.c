/*
 * sbservices.c
 * com.apple.springboardservices service implementation.
 *
 * Copyright (c) 2009 Nikias Bassen, All Rights Reserved.
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
#include <unistd.h>
#include <plist/plist.h>

#include "sbservices.h"
#include "property_list_service.h"
#include "debug.h"

GQuark
sbservices_client_error_quark (void)
{
  return g_quark_from_static_string ("sbservices-client-error-quark");
}
/**
 * Locks an sbservices client, used for thread safety.
 *
 * @param client sbservices client to lock.
 */
static void sbs_lock(sbservices_client_t client)
{
	debug_info("SBServices: Locked");
	g_mutex_lock(client->mutex);
}

/**
 * Unlocks an sbservices client, used for thread safety.
 * 
 * @param client sbservices client to unlock
 */
static void sbs_unlock(sbservices_client_t client)
{
	debug_info("SBServices: Unlocked");
	g_mutex_unlock(client->mutex);
}

/**
 * Connects to the springboardservices service on the specified device.
 *
 * @param device The device to connect to.
 * @param port Destination port (usually given by lockdownd_start_service).
 * @param client Pointer that will point to a newly allocated
 *     sbservices_client_t upon successful return.
 *
 * @return SBSERVICES_E_SUCCESS on success, SBSERVICES_E_INVALID_ARG when
 *     client is NULL, or an SBSERVICES_E_* error code otherwise.
 */
sbservices_client_t sbservices_client_new(idevice_t device, uint16_t port, GError **error)
{
	/* makes sure thread environment is available */
	if (!g_thread_supported())
		g_thread_init(NULL);
	
	g_assert(device != NULL);

	GError *tmp_error = NULL;
	property_list_service_client_t plistclient = property_list_service_client_new(device, port, &tmp_error);

	if (tmp_error != NULL) {
		g_propagate_error(error, tmp_error);
		return NULL;
	}

	sbservices_client_t client_loc = (sbservices_client_t) malloc(sizeof(struct sbservices_client_private));
	client_loc->parent = plistclient;
	client_loc->mutex = g_mutex_new();

	return client_loc;
}

/**
 * Disconnects an sbservices client from the device and frees up the
 * sbservices client data.
 *
 * @param client The sbservices client to disconnect and free.
 *
 * @return SBSERVICES_E_SUCCESS on success, SBSERVICES_E_INVALID_ARG when
 *     client is NULL, or an SBSERVICES_E_* error code otherwise.
 */
void sbservices_client_free(sbservices_client_t client, GError **error)
{
	g_assert(client != NULL);

	property_list_service_client_free(client->parent, error);
	client->parent = NULL;
	if (client->mutex) {
		g_mutex_free(client->mutex);
	}
	free(client);
}

/**
 * Gets the icon state of the connected device.
 *
 * @param client The connected sbservices client to use.
 * @param state Pointer that will point to a newly allocated plist containing
 *     the current icon state. It is up to the caller to free the memory.
 *
 * @return SBSERVICES_E_SUCCESS on success, SBSERVICES_E_INVALID_ARG when
 *     client or state is invalid, or an SBSERVICES_E_* error code otherwise.
 */
plist_t sbservices_get_icon_state(sbservices_client_t client, GError **error)
{
	g_assert(client != NULL && client->parent != NULL);

	GError *tmp_error = NULL;
	plist_t state = NULL;

	plist_t dict = plist_new_dict();
	plist_dict_insert_item(dict, "command", plist_new_string("getIconState"));

	sbs_lock(client);

	property_list_service_send_binary_plist(client->parent, dict, &tmp_error);
	if (tmp_error != NULL) {
		debug_info("could not send plist, error %d, reason %s", tmp_error->code, tmp_error->message);
		g_propagate_error(error, tmp_error);
		goto leave_unlock;
	}
	plist_free(dict);
	dict = NULL;

	state = property_list_service_receive_plist(client->parent, &tmp_error);
	if (tmp_error != NULL) {
		debug_info("could not get icon state, error %d, reason %s", tmp_error->code, tmp_error->message);
		if (state != NULL) {
			plist_free(state);
			state = NULL;
		}
		g_propagate_error(error, tmp_error);
	}

leave_unlock:
	if (dict) {
		plist_free(dict);
	}
	sbs_unlock(client);
	return state;
}

/**
 * Sets the icon state of the connected device.
 *
 * @param client The connected sbservices client to use.
 * @param newstate A plist containing the new iconstate.
 *
 * @return SBSERVICES_E_SUCCESS on success, SBSERVICES_E_INVALID_ARG when
 *     client or newstate is NULL, or an SBSERVICES_E_* error code otherwise.
 */
void sbservices_set_icon_state(sbservices_client_t client, plist_t newstate, GError **error)
{
	g_assert(client != NULL && client->parent != NULL && newstate != NULL);

	plist_t dict = plist_new_dict();
	plist_dict_insert_item(dict, "command", plist_new_string("setIconState"));
	plist_dict_insert_item(dict, "iconState", plist_copy(newstate));

	sbs_lock(client);
	
	GError *tmp_error = NULL;
	property_list_service_send_binary_plist(client->parent, dict, &tmp_error);
	if (tmp_error != NULL) {
		debug_info("could not send plist, error %d, reason %s", tmp_error->code, tmp_error->message);
		g_propagate_error(error, tmp_error);
	}
	/* NO RESPONSE */

	if (dict) {
		plist_free(dict);
	}
	sbs_unlock(client);
}

/**
 * Get the icon of the specified app as PNG data.
 *
 * @param client The connected sbservices client to use.
 * @param bundleId The bundle identifier of the app to retrieve the icon for.
 * @param pngdata Pointer that will point to a newly allocated buffer
 *     containing the PNG data upon successful return. It is up to the caller
 *     to free the memory.
 * @param pngsize Pointer to a uint64_t that will be set to the size of the
 *     buffer pngdata points to upon successful return.
 *
 * @return SBSERVICES_E_SUCCESS on success, SBSERVICES_E_INVALID_ARG when
 *     client, bundleId, or pngdata are invalid, or an SBSERVICES_E_* error
 *     code otherwise.
 */
void sbservices_get_icon_pngdata(sbservices_client_t client, const char *bundleId, char **pngdata, uint64_t *pngsize, GError **error)
{
	g_assert(client != NULL && client->parent != NULL && bundleId != NULL && pngdata != NULL);

	plist_t dict = plist_new_dict();
	plist_dict_insert_item(dict, "command", plist_new_string("getIconPNGData"));
	plist_dict_insert_item(dict, "bundleId", plist_new_string(bundleId));

	sbs_lock(client);

	GError *tmp_error = NULL;
	property_list_service_send_binary_plist(client->parent, dict, &tmp_error);
	if (tmp_error != NULL) {
		debug_info("could not send plist, error %d, reason %s", tmp_error->code, tmp_error->message);
		g_propagate_error(error, tmp_error);
		goto leave_unlock;
	}
	plist_free(dict);
	dict = NULL;

	dict = property_list_service_receive_plist(client->parent, &tmp_error);
	if (tmp_error == NULL) {
		plist_t node = plist_dict_get_item(dict, "pngData");
		if (node) {
			plist_get_data_val(node, pngdata, pngsize);
		}
	} else {
		g_propagate_error(error, tmp_error);
	}

leave_unlock:
	if (dict) {
		plist_free(dict);
	}
	sbs_unlock(client);
}

