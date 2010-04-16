/*
 * mobile_image_mounter.c
 * com.apple.mobile.mobile_image_mounter service implementation.
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
#include <unistd.h>
#include <plist/plist.h>

#include "mobile_image_mounter.h"
#include "property_list_service.h"
#include "debug.h"

GQuark
mobile_image_mounter_client_error_quark (void)
{
  return g_quark_from_static_string ("mobile-image-mounter-client-error-quark");
}

/**
 * Locks a mobile_image_mounter client, used for thread safety.
 *
 * @param client mobile_image_mounter client to lock
 */
static void mobile_image_mounter_lock(mobile_image_mounter_client_t client)
{
	g_mutex_lock(client->mutex);
}

/**
 * Unlocks a mobile_image_mounter client, used for thread safety.
 * 
 * @param client mobile_image_mounter client to unlock
 */
static void mobile_image_mounter_unlock(mobile_image_mounter_client_t client)
{
	g_mutex_unlock(client->mutex);
}

/**
 * Convert a property_list_service_error_t value to a
 * mobile_image_mounter_error_t value.
 * Used internally to get correct error codes.
 *
 * @param err A property_list_service_error_t error code
 *
 * @return A matching mobile_image_mounter_error_t error code,
 *     MOBILE_IMAGE_MOUNTER_E_UNKNOWN_ERROR otherwise.
 */
static mobile_image_mounter_error_t mobile_image_mounter_error(property_list_service_error_t err)
{
	switch (err) {
		case PROPERTY_LIST_SERVICE_E_SUCCESS:
			return MOBILE_IMAGE_MOUNTER_E_SUCCESS;
		case PROPERTY_LIST_SERVICE_E_INVALID_ARG:
			return MOBILE_IMAGE_MOUNTER_E_INVALID_ARG;
		case PROPERTY_LIST_SERVICE_E_PLIST_ERROR:
			return MOBILE_IMAGE_MOUNTER_E_PLIST_ERROR;
		case PROPERTY_LIST_SERVICE_E_MUX_ERROR:
			return MOBILE_IMAGE_MOUNTER_E_CONN_FAILED;
		default:
			break;
	}
	return MOBILE_IMAGE_MOUNTER_E_UNKNOWN_ERROR;
}

/**
 * Connects to the mobile_image_mounter service on the specified device.
 * 
 * @param device The device to connect to.
 * @param port Destination port (usually given by lockdownd_start_service).
 * @param client Pointer that will be set to a newly allocated
 *    mobile_image_mounter_client_t upon successful return.
 * 
 * @return MOBILE_IMAGE_MOUNTER_E_SUCCESS on success,
 *    MOBILE_IMAGE_MOUNTER_E_INVALID_ARG if device is NULL,
 *    or MOBILE_IMAGE_MOUNTER_E_CONN_FAILED if the connection to the
 *    device could not be established.
 */
mobile_image_mounter_client_t mobile_image_mounter_new(idevice_t device, uint16_t port, GError **error)
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

	mobile_image_mounter_client_t client_loc = (mobile_image_mounter_client_t) malloc(sizeof(struct mobile_image_mounter_client_private));
	client_loc->parent = plistclient;

	client_loc->mutex = g_mutex_new();

	return client_loc;
}

/**
 * Disconnects a mobile_image_mounter client from the device and frees up the
 * mobile_image_mounter client data.
 * 
 * @param client The mobile_image_mounter client to disconnect and free.
 *
 * @return MOBILE_IMAGE_MOUNTER_E_SUCCESS on success,
 *    or MOBILE_IMAGE_MOUNTER_E_INVALID_ARG if client is NULL.
 */
void mobile_image_mounter_free(mobile_image_mounter_client_t client, GError **error)
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
 * Tells if the image of ImageType is already mounted.
 *
 * @param client The client use
 * @param image_type The type of the image to look up
 * @param result Pointer to a plist that will receive the result of the
 *    operation.
 *
 * @note This function may return MOBILE_IMAGE_MOUNTER_E_SUCCESS even if the
 *    operation has failed. Check the resulting plist for further information.
 *
 * @return MOBILE_IMAGE_MOUNTER_E_SUCCESS on success, or an error code on error
 */
plist_t mobile_image_mounter_lookup_image(mobile_image_mounter_client_t client, const char *image_type, GError **error)
{
	g_assert(client != NULL && image_type != NULL);

	GError *tmp_error = NULL;
	plist_t result = NULL;

	mobile_image_mounter_lock(client);

	plist_t dict = plist_new_dict();
	plist_dict_insert_item(dict,"Command", plist_new_string("LookupImage"));
	plist_dict_insert_item(dict,"ImageType", plist_new_string(image_type));

	property_list_service_send_xml_plist(client->parent, dict, &tmp_error);
	plist_free(dict);

	if (tmp_error != NULL) {
		debug_info("%s: Error sending XML plist to device!", __func__);
		g_propagate_error(error, tmp_error);
		goto leave_unlock;
	}

	result = property_list_service_receive_plist(client->parent, &tmp_error);
	if (tmp_error != NULL) {
		debug_info("%s: Error receiving response from device!", __func__);
		g_propagate_error(error, tmp_error);
	}

leave_unlock:
	mobile_image_mounter_unlock(client);
	return result;
}

/**
 * Mounts an image on the device.
 *
 * @param client The connected mobile_image_mounter client.
 * @param image_path The absolute path of the image to mount. The image must
 *    be present before calling this function.
 * @param image_signature Pointer to a buffer holding the images' signature
 * @param signature_length Length of the signature image_signature points to
 * @param image_type Type of image to mount
 * @param result Pointer to a plist that will receive the result of the
 *    operation.
 *
 * @note This function may return MOBILE_IMAGE_MOUNTER_E_SUCCESS even if the
 *    operation has failed. Check the resulting plist for further information.
 *    Note that there is no unmounting function. The mount persists until the
 *    device is rebooted.
 *
 * @return MOBILE_IMAGE_MOUNTER_E_SUCCESS on success,
 *    MOBILE_IMAGE_MOUNTER_E_INVALID_ARG if on ore more parameters are
 *    invalid, or another error code otherwise.
 */
plist_t mobile_image_mounter_mount_image(mobile_image_mounter_client_t client, const char *image_path, const char *image_signature, uint16_t signature_length, const char *image_type, GError **error)
{
	g_assert(client != NULL && image_path != NULL && image_signature != NULL && signature_length > 0);
	g_assert(image_type != NULL);

	GError *tmp_error = NULL;
	plist_t result = NULL;

	mobile_image_mounter_lock(client);

	plist_t dict = plist_new_dict();
	plist_dict_insert_item(dict, "Command", plist_new_string("MountImage"));
	plist_dict_insert_item(dict, "ImagePath", plist_new_string(image_path));
	plist_dict_insert_item(dict, "ImageSignature", plist_new_data(image_signature, signature_length));
	plist_dict_insert_item(dict, "ImageType", plist_new_string(image_type));

	property_list_service_send_xml_plist(client->parent, dict, &tmp_error);
	plist_free(dict);

	if (tmp_error != NULL) {
		debug_info("%s: Error sending XML plist to device!", __func__);
		g_propagate_error(error, tmp_error);
		goto leave_unlock;
	}

	result = property_list_service_receive_plist(client->parent, &tmp_error);
	if (tmp_error != NULL) {
		debug_info("%s: Error receiving response from device!", __func__);
		g_propagate_error(error, tmp_error);
	}

leave_unlock:
	mobile_image_mounter_unlock(client);
	return result;
}

/**
 * Hangs up the connection to the mobile_image_mounter service.
 * This functions has to be called before freeing up a mobile_image_mounter
 * instance. If not, errors appear in the device's syslog.
 *
 * @param client The client to hang up
 *
 * @return MOBILE_IMAGE_MOUNTER_E_SUCCESS on success,
 *     MOBILE_IMAGE_MOUNTER_E_INVALID_ARG if client is invalid,
 *     or another error code otherwise.
 */
void mobile_image_mounter_hangup(mobile_image_mounter_client_t client, GError **error)
{
	g_assert(client != NULL);

	GError *tmp_error = NULL;

	mobile_image_mounter_lock(client);

	plist_t dict = plist_new_dict();
	plist_dict_insert_item(dict, "Command", plist_new_string("Hangup"));

	property_list_service_send_xml_plist(client->parent, dict, &tmp_error);
	plist_free(dict);

	if (tmp_error != NULL) {
		debug_info("%s: Error sending XML plist to device!", __func__);
		g_propagate_error(error, tmp_error);
		goto leave_unlock;
	}

	dict = NULL;
	dict = property_list_service_receive_plist(client->parent, &tmp_error);
	if (tmp_error != NULL) {
		debug_info("%s: Error receiving response from device!", __func__);
		g_propagate_error(error, tmp_error);
	}
	if (dict) {
		debug_plist(dict);
		plist_free(dict);
	}

leave_unlock:
	mobile_image_mounter_unlock(client);
}
