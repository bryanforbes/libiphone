/*
 * mobilebackup.c 
 * Contains functions for the built-in MobileBackup client.
 * 
 * Copyright (c) 2009 Martin Szulecki All Rights Reserved.
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

#include "mobilebackup.h"
#include "device_link_service.h"
#include "debug.h"

#define MBACKUP_VERSION_INT1 100
#define MBACKUP_VERSION_INT2 0

GQuark
mobilebackup_client_error_quark (void)
{
  return g_quark_from_static_string ("mobilebackup-client-error-quark");
}

/**
 * Convert an device_link_service_error_t value to an mobilebackup_error_t value.
 * Used internally to get correct error codes when using device_link_service stuff.
 *
 * @param err An device_link_service_error_t error code
 *
 * @return A matching mobilebackup_error_t error code,
 *     MOBILEBACKUP_E_UNKNOWN_ERROR otherwise.
 */
static mobilebackup_error_t mobilebackup_error(device_link_service_error_t err)
{
	switch (err) {
		case DEVICE_LINK_SERVICE_E_SUCCESS:
			return MOBILEBACKUP_E_SUCCESS;
		case DEVICE_LINK_SERVICE_E_INVALID_ARG:
			return MOBILEBACKUP_E_INVALID_ARG;
		case DEVICE_LINK_SERVICE_E_PLIST_ERROR:
			return MOBILEBACKUP_E_PLIST_ERROR;
		case DEVICE_LINK_SERVICE_E_MUX_ERROR:
			return MOBILEBACKUP_E_MUX_ERROR;
		case DEVICE_LINK_SERVICE_E_BAD_VERSION:
			return MOBILEBACKUP_E_BAD_VERSION;
		default:
			break;
	}
	return MOBILEBACKUP_E_UNKNOWN_ERROR;
}

/**
 * Connects to the mobilebackup service on the specified device.
 *
 * @param device The device to connect to.
 * @param port Destination port (usually given by lockdownd_start_service).
 * @param client Pointer that will be set to a newly allocated
 *     mobilebackup_client_t upon successful return.
 *
 * @return MOBILEBACKUP_E_SUCCESS on success, MOBILEBACKUP_E_INVALID ARG if one
 *     or more parameters are invalid, or DEVICE_LINK_SERVICE_E_BAD_VERSION if
 *     the mobilebackup version on the device is newer.
 */
mobilebackup_client_t mobilebackup_client_new(idevice_t device, uint16_t port, GError **error)
{
	g_assert(device != NULL && port > 0);

	GError *tmp_error = NULL;
	device_link_service_client_t dlclient = device_link_service_client_new(device, port, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error(error, tmp_error);
		return NULL;
	}

	mobilebackup_client_t client_loc = (mobilebackup_client_t) malloc(sizeof(struct mobilebackup_client_private));
	client_loc->parent = dlclient;

	/* perform handshake */
	device_link_service_version_exchange(dlclient, MBACKUP_VERSION_INT1, MBACKUP_VERSION_INT2, &tmp_error);
	if (tmp_error != NULL) {
		debug_info("version exchange failed, error %d, reason %s", tmp_error->code, tmp_error->message);
		mobilebackup_client_free(client_loc, NULL);
		g_propagate_error(error, tmp_error);
		return NULL;
	}

	return client_loc;
}

/**
 * Disconnects a mobilebackup client from the device and frees up the
 * mobilebackup client data.
 *
 * @param client The mobilebackup client to disconnect and free.
 *
 * @return MOBILEBACKUP_E_SUCCESS on success, or MOBILEBACKUP_E_INVALID_ARG
 *     if client is NULL.
 */
void mobilebackup_client_free(mobilebackup_client_t client, GError **error)
{
	g_assert(client != NULL);

	device_link_service_disconnect(client->parent, NULL);

	device_link_service_client_free(client->parent, error);
	free(client);
}

/**
 * Polls the device for mobilebackup data.
 *
 * @param client The mobilebackup client
 * @param plist A pointer to the location where the plist should be stored
 *
 * @return an error code
 */
plist_t mobilebackup_receive(mobilebackup_client_t client, GError **error)
{
	g_assert(client != NULL);
	return device_link_service_receive(client->parent, error);
}

/**
 * Sends mobilebackup data to the device
 * 
 * @note This function is low-level and should only be used if you need to send
 *        a new type of message.
 *
 * @param client The mobilebackup client
 * @param plist The location of the plist to send
 *
 * @return an error code
 */
void mobilebackup_send(mobilebackup_client_t client, plist_t plist, GError **error)
{
	g_assert(client != NULL && plist != NULL);
	device_link_service_send(client->parent, plist, error);
}

/**
 * Request a backup from the connected device.
 *
 * @param client The connected MobileBackup client to use.
 * @param backup_manifest The backup manifest, a plist_t of type PLIST_DICT
 *    containing the backup state of the last backup. For a first-time backup
 *    set this parameter to NULL.
 * @param base_path The base path on the device to use for the backup
 *    operation, usually "/".
 * @param proto_version A string denoting the version of the backup protocol
 *    to use. Latest known version is "1.6"
 *
 * @return MOBILEBACKUP_E_SUCCESS on success, MOBILEBACKUP_E_INVALID_ARG if
 *    one of the parameters is invalid, MOBILEBACKUP_E_PLIST_ERROR if
 *    backup_manifest is not of type PLIST_DICT, MOBILEBACKUP_E_MUX_ERROR
 *    if a communication error occurs, MOBILEBACKUP_E_REPLY_NOT_OK
 */
void mobilebackup_request_backup(mobilebackup_client_t client, plist_t backup_manifest, const char *base_path, const char *proto_version, GError **error)
{
	g_assert(client != NULL && client->parent != NULL && base_path != NULL && proto_version != NULL);

	if (backup_manifest && (plist_get_node_type(backup_manifest) != PLIST_DICT)) {
		g_set_error(error, MOBILEBACKUP_CLIENT_ERROR,
			MOBILEBACKUP_E_PLIST_ERROR,
			"Backup manifest must be a dict");
		return;
	}

	GError *tmp_error = NULL;

	/* construct request plist */
	plist_t dict = plist_new_dict();
	if (backup_manifest)
		plist_dict_insert_item(dict, "BackupManifestKey", plist_copy(backup_manifest));
	plist_dict_insert_item(dict, "BackupComputerBasePathKey", plist_new_string(base_path));
	plist_dict_insert_item(dict, "BackupMessageTypeKey", plist_new_string("BackupMessageBackupRequest"));
	plist_dict_insert_item(dict, "BackupProtocolVersion", plist_new_string(proto_version));

	/* send it as DLMessageProcessMessage */
	device_link_service_send_process_message(client->parent, dict, &tmp_error);
	plist_free(dict);
	dict = NULL;
	if (tmp_error != NULL) {
		debug_info("ERROR: Could not send backup request message (%d: %s)!", tmp_error->code, tmp_error->message);
		g_propagate_error(error, tmp_error);
		goto leave;
	}

	/* now receive and hopefully get a BackupMessageBackupReplyOK */
	dict = device_link_service_receive_process_message(client->parent, &tmp_error);
	if (tmp_error != NULL) {
		debug_info("ERROR: Could not receive BackupReplyOK message (%d: %s)!", tmp_error->code, tmp_error->message);
		g_propagate_error(error, tmp_error);
		goto leave;
	}

	plist_t node = plist_dict_get_item(dict, "BackupMessageTypeKey");
	if (!node) {
		debug_info("ERROR: BackupMessageTypeKey not found in BackupReplyOK message!");
		g_set_error(error, MOBILEBACKUP_CLIENT_ERROR,
			MOBILEBACKUP_E_PLIST_ERROR,
			"BackupMessageTypeKey not found in BackupReplyOK message");
		goto leave;
	}

	char *str = NULL;
	plist_get_string_val(node, &str);
	if (!str || (strcmp(str, "BackupMessageBackupReplyOK") != 0)) {
		debug_info("ERROR: BackupMessageTypeKey value does not match 'BackupMessageBackupReplyOK'");
		g_set_error(error, MOBILEBACKUP_CLIENT_ERROR,
			MOBILEBACKUP_E_REPLY_NOT_OK,
			"BackupMessageTypeKey value does not match 'BackupMessageBackupReplyOK'");
		if (str)
			free(str);
		goto leave;
	}
	free(str);
	str = NULL;

	int bad_version = 0;
	node = plist_dict_get_item(dict, "BackupProtocolVersion");
	if (node) {
		plist_get_string_val(node, &str);
		if (str) {
			if (strcmp(str, proto_version) != 0) {
				bad_version = 1;
			}
			free(str);
		}
	}
	if (bad_version == 1) {
		g_set_error(error, MOBILEBACKUP_CLIENT_ERROR,
			MOBILEBACKUP_E_BAD_VERSION,
			"Bad version");
		goto leave;
	}

	/* BackupMessageBackupReplyOK received, send it back */
	device_link_service_send_process_message(client->parent, dict, &tmp_error);
	if (tmp_error != NULL) {
		debug_info("ERROR: Could not send BackupReplyOK ACK (%d: %s)", tmp_error->code, tmp_error->message);
		g_propagate_error(error, tmp_error);
	}

leave:
	if (dict)
		plist_free(dict);
}

/**
 * Sends a confirmation to the device that a backup file has been received.
 *
 * @param client The connected MobileBackup client to use.
 * 
 * @return MOBILEBACKUP_E_SUCCESS on success, MOBILEBACKUP_E_INVALID_ARG if
 *    client is invalid, or MOBILEBACKUP_E_MUX_ERROR if a communication error
 *    occurs.
 */
void mobilebackup_send_backup_file_received(mobilebackup_client_t client, GError **error)
{
	g_assert(client != NULL);

	/* construct ACK plist */
	plist_t dict = plist_new_dict();
	plist_dict_insert_item(dict, "BackupMessageTypeKey", plist_new_string("kBackupMessageBackupFileReceived"));

	/* send it as DLMessageProcessMessage */
	device_link_service_send_process_message(client->parent, dict, error);
	plist_free(dict);
}

/**
 * Sends a backup error message to the device.
 *
 * @param client The connected MobileBackup client to use.
 * @param reason A string describing the reason for the error message.
 * 
 * @return MOBILEBACKUP_E_SUCCESS on success, MOBILEBACKUP_E_INVALID_ARG if
 *    one of the parameters is invalid, or MOBILEBACKUP_E_MUX_ERROR if a
 *    communication error occurs.
 */
void mobilebackup_send_error(mobilebackup_client_t client, const char *reason, GError **error)
{
	g_assert(client != NULL && client->parent != NULL && reason != NULL);

	/* construct error plist */
	plist_t dict = plist_new_dict();
	plist_dict_insert_item(dict, "BackupMessageTypeKey", plist_new_string("BackupMessageError"));
	plist_dict_insert_item(dict, "BackupErrorReasonKey", plist_new_string(reason));

	/* send it as DLMessageProcessMessage */
	device_link_service_send_process_message(client->parent, dict, error);
	plist_free(dict);
}
