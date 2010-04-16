/*
 * installation_proxy.c
 * com.apple.mobile.installation_proxy service implementation.
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

#include "installation_proxy.h"
#include "property_list_service.h"
#include "debug.h"

GQuark
instproxy_client_error_quark (void)
{
  return g_quark_from_static_string ("instproxy-client-error-quark");
}

struct instproxy_status_data {
	instproxy_client_t client;
	instproxy_status_cb_t cbfunc;
	char *operation;
	void *user_data;
	GError *error;
};

/**
 * Locks an installation_proxy client, used for thread safety.
 *
 * @param client The installation_proxy client to lock
 */
static void instproxy_lock(instproxy_client_t client)
{
	debug_info("InstallationProxy: Locked");
	g_mutex_lock(client->mutex);
}

/**
 * Unlocks an installation_proxy client, used for thread safety.
 * 
 * @param client The installation_proxy client to lock
 */
static void instproxy_unlock(instproxy_client_t client)
{
	debug_info("InstallationProxy: Unlocked");
	g_mutex_unlock(client->mutex);
}

/**
 * Connects to the installation_proxy service on the specified device.
 *
 * @param device The device to connect to
 * @param port Destination port (usually given by lockdownd_start_service).
 * @param client Pointer that will be set to a newly allocated
 *     instproxy_client_t upon successful return.
 *
 * @return INSTPROXY_E_SUCCESS on success, or an INSTPROXY_E_* error value
 *     when an error occured.
 */
instproxy_client_t instproxy_client_new(idevice_t device, uint16_t port, GError **error)
{
	/* makes sure thread environment is available */
	if (!g_thread_supported())
		g_thread_init(NULL);

	g_assert(device != NULL && port > 0);

	GError *tmp_error = NULL;
	property_list_service_client_t plistclient = property_list_service_client_new(device, port, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error(error, tmp_error);
		return NULL;
	}

	instproxy_client_t client_loc = (instproxy_client_t) malloc(sizeof(struct instproxy_client_private));
	client_loc->parent = plistclient;
	client_loc->mutex = g_mutex_new();
	client_loc->status_updater = NULL;

	return client_loc;
}

/**
 * Disconnects an installation_proxy client from the device and frees up the
 * installation_proxy client data.
 *
 * @param client The installation_proxy client to disconnect and free.
 *
 * @return INSTPROXY_E_SUCCESS on success
 *      or INSTPROXY_E_INVALID_ARG if client is NULL.
 */
void instproxy_client_free(instproxy_client_t client, GError **error)
{
	g_assert(client != NULL);

	property_list_service_client_free(client->parent, error);
	client->parent = NULL;
	if (client->status_updater) {
		debug_info("joining status_updater");
		g_thread_join(client->status_updater);
	}
	if (client->mutex) {
		g_mutex_free(client->mutex);
	}
	free(client);
}

/**
 * Send a command with specified options to the device.
 * Only used internally.
 *
 * @param client The connected installation_proxy client.
 * @param command The command to execute. Required.
 * @param client_options The client options to use, as PLIST_DICT, or NULL.
 * @param appid The ApplicationIdentifier to add or NULL if not required.
 * @param package_path The installation package path or NULL if not required.
 *
 * @return INSTPROXY_E_SUCCESS on success or an INSTPROXY_E_* error value if
 *     an error occured.
 */
static void instproxy_send_command(instproxy_client_t client, const char *command, plist_t client_options, const char *appid, const char *package_path, GError **error)
{
	g_assert(client != NULL && command != NULL && client_options != NULL);
	g_assert(plist_get_node_type(client_options) == PLIST_DICT);

	plist_t dict = plist_new_dict();
	if (appid) {
		plist_dict_insert_item(dict, "ApplicationIdentifier", plist_new_string(appid));
	}
	if (client_options && (plist_dict_get_size(client_options) > 0)) {
		plist_dict_insert_item(dict, "ClientOptions", plist_copy(client_options));
	}
	plist_dict_insert_item(dict, "Command", plist_new_string(command));
	if (package_path) {
		plist_dict_insert_item(dict, "PackagePath", plist_new_string(package_path));
	}

	property_list_service_send_xml_plist(client->parent, dict, error);
	plist_free(dict);
}

/**
 * List installed applications. This function runs synchronously.
 *
 * @param client The connected installation_proxy client
 * @param client_options The client options to use, as PLIST_DICT, or NULL.
 *        Valid client options include:
 *          "ApplicationType" -> "User"
 *          "ApplicationType" -> "System"
 * @param result Pointer that will be set to a plist that will hold an array
 *        of PLIST_DICT holding information about the applications found.
 *
 * @return INSTPROXY_E_SUCCESS on success or an INSTPROXY_E_* error value if
 *     an error occured.
 */
plist_t instproxy_browse(instproxy_client_t client, plist_t client_options, GError **error)
{
	g_assert(client != NULL && client->parent != NULL);

	plist_t result = NULL;
	GError *tmp_error = NULL;

	instproxy_lock(client);
	instproxy_send_command(client, "Browse", client_options, NULL, NULL, &tmp_error);
	if (tmp_error != NULL) {
		debug_info("could not send plist");
		g_propagate_error(error, tmp_error);
		goto leave_unlock;
	}

	int browsing = 0;
	plist_t apps_array = plist_new_array();
	plist_t dict = NULL;

	do {
		browsing = 0;
		dict = NULL;
		dict = property_list_service_receive_plist(client->parent, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error(error, tmp_error);
			break;
		}
		if (dict) {
			uint64_t i;
			uint64_t current_amount = 0;
			char *status = NULL;
			plist_t camount = plist_dict_get_item(dict, "CurrentAmount");
			plist_t pstatus = plist_dict_get_item(dict, "Status");
			if (camount) {
				plist_get_uint_val(camount, &current_amount);
			}
			if (current_amount > 0) {
				plist_t current_list = plist_dict_get_item(dict, "CurrentList");
				for (i = 0; current_list && (i < current_amount); i++) {
					plist_t item = plist_array_get_item(current_list, i);
					plist_array_append_item(apps_array, plist_copy(item));
				}
			}
			if (pstatus) {
				plist_get_string_val(pstatus, &status);
			}
			if (status) {
				if (!strcmp(status, "BrowsingApplications")) {
					browsing = 1;
				} else if (!strcmp(status, "Complete")) {
					debug_info("Browsing applications completed");
					g_clear_error(error);
				}
				free(status);
			}
			plist_free(dict);
		}
	} while (browsing);

	if (tmp_error == NULL) {
		result = apps_array;
	}

leave_unlock:
	instproxy_unlock(client);
	return result;
}

/**
 * Internally used function that will synchronously receive messages from
 * the specified installation_proxy until it completes or an error occurs.
 *
 * If status_cb is not NULL, the callback function will be called each time
 * a status update or error message is received.
 *
 * @param client The connected installation proxy client
 * @param status_cb Pointer to a callback function or NULL
 * @param operation Operation name. Will be passed to the callback function
 *        in async mode or shown in debug messages in sync mode. 
 */
static void instproxy_perform_operation(instproxy_client_t client, instproxy_status_cb_t status_cb, const char *operation, void *user_data, GError **error)
{
	int ok = 1;
	plist_t dict = NULL;

	do {
		instproxy_lock(client);
		GError *tmp_error = NULL;
		dict = property_list_service_receive_plist_with_timeout(client->parent, 30000, &tmp_error);
		instproxy_unlock(client);
		if (tmp_error != NULL) {
			debug_info("could not receive plist, error %d, reason %s", tmp_error->code, tmp_error->message);
			g_propagate_error(error, tmp_error);
			break;
		}
		if (dict) {
			/* invoke callback function */
			if (status_cb) {
				status_cb(operation, dict, user_data);
			}
			/* check for 'Error', so we can abort cleanly */
			plist_t err = plist_dict_get_item(dict, "Error");
			if (err) {
#ifndef STRIP_DEBUG_CODE
				char *err_msg = NULL;
				plist_get_string_val(err, &err_msg);
				if (err_msg) {
					debug_info("(%s): ERROR: %s", operation, err_msg);
					free(err_msg);
				}
#endif
				ok = 0;
				g_set_error(error, INSTPROXY_CLIENT_ERROR,
					INSTPROXY_E_OP_FAILED,
					"Operation failed: %s", operation);
			}
			/* get 'Status' */
			plist_t status = plist_dict_get_item(dict, "Status");
			if (status) {
				char *status_msg = NULL;
				plist_get_string_val(status, &status_msg);
				if (status_msg) {
					if (!strcmp(status_msg, "Complete")) {
						ok = 0;
						g_clear_error(error);
					}
#ifndef STRIP_DEBUG_CODE
					plist_t npercent = plist_dict_get_item(dict, "PercentComplete");
					if (npercent) {
						uint64_t val = 0;
						int percent;
						plist_get_uint_val(npercent, &val);
						percent = val;
						debug_info("(%s): %s (%d%%)", operation, status_msg, percent);
					} else {
						debug_info("(%s): %s", operation, status_msg);
					}
#endif
					free(status_msg);
				}
			}
			plist_free(dict);
			dict = NULL;
		}
	} while (ok && client->parent);
}

/**
 * Internally used status updater thread function that will call the specified
 * callback function when status update messages (or error messages) are
 * received.
 *
 * @param arg Pointer to an allocated struct instproxy_status_data that holds
 *     the required data about the connected client and the callback function.
 *
 * @return Always NULL.
 */
static gpointer instproxy_status_updater(gpointer arg)
{	
	struct instproxy_status_data *data = (struct instproxy_status_data*)arg;

	/* run until the operation is complete or an error occurs */
	instproxy_perform_operation(data->client, data->cbfunc, data->operation, data->user_data, NULL);

	/* cleanup */
	instproxy_lock(data->client);
	debug_info("done, cleaning up.");
	if (data->operation) {
	    free(data->operation);
	}
	data->client->status_updater = NULL;
	instproxy_unlock(data->client);
	free(data);

	return NULL;
}

/**
 * Internally used helper function that creates a status updater thread which
 * will call the passed callback function when status updates occur.
 * If status_cb is NULL no thread will be created, but the operation will
 * run synchronously until it completes or an error occurs.
 *
 * @param client The connected installation proxy client
 * @param status_cb Pointer to a callback function or NULL
 * @param operation Operation name. Will be passed to the callback function
 *        in async mode or shown in debug messages in sync mode.
 *
 * @return INSTPROXY_E_SUCCESS when the thread was created (async mode), or
 *         when the operation completed successfully (sync).
 *         An INSTPROXY_E_* error value is returned if an error occured.
 */
static void instproxy_create_status_updater(instproxy_client_t client, instproxy_status_cb_t status_cb, const char *operation, void *user_data, GError **error)
{
	if (status_cb) {
		g_set_error(error, INSTPROXY_CLIENT_ERROR,
			INSTPROXY_E_UNKNOWN_ERROR,
			"Uknown error");

		/* async mode */
		struct instproxy_status_data *data = (struct instproxy_status_data*)malloc(sizeof(struct instproxy_status_data));
		if (data) {
			data->client = client;
			data->cbfunc = status_cb;
			data->operation = strdup(operation);
			data->user_data = user_data;

			client->status_updater = g_thread_create(instproxy_status_updater, data, TRUE, NULL);
			if (client->status_updater) {
				g_clear_error(error);
			}
		}
	} else {
		/* sync mode */
		instproxy_perform_operation(client, NULL, operation, NULL, error);
	}
}


/**
 * Internal function used by instproxy_install and instproxy_upgrade.
 *
 * @param client The connected installation_proxy client
 * @param pkg_path Path of the installation package (inside the AFC jail)
 * @param client_options The client options to use, as PLIST_DICT, or NULL.
 * @param status_cb Callback function for progress and status information. If
 *        NULL is passed, this function will run synchronously.
 * @param command The command to execute.
 *
 * @return INSTPROXY_E_SUCCESS on success or an INSTPROXY_E_* error value if
 *     an error occured. 
 */
static void instproxy_install_or_upgrade(instproxy_client_t client, const char *pkg_path, plist_t client_options, instproxy_status_cb_t status_cb, const char *command, void *user_data, GError **error)
{
	g_assert(client != NULL && client->parent != NULL && pkg_path != NULL);

	if (client->status_updater) {
		g_set_error(error, INSTPROXY_CLIENT_ERROR,
			INSTPROXY_E_OP_IN_PROGRESS,
			"Operation already in progress");
		return;
	}

	instproxy_lock(client);
	GError *tmp_error = NULL;
	instproxy_send_command(client, command, client_options, NULL, pkg_path, &tmp_error);
	instproxy_unlock(client);

	if (tmp_error != NULL) {
		debug_info("could not send plist, error %d, reason %s",
			tmp_error->code, tmp_error->message);
		g_propagate_error(error, tmp_error);
		return;
	}

	instproxy_create_status_updater(client, status_cb, command, user_data, error);
}

/**
 * Install an application on the device.
 *
 * @param client The connected installation_proxy client
 * @param pkg_path Path of the installation package (inside the AFC jail)
 * @param client_options The client options to use, as PLIST_DICT, or NULL.
 *        Valid options include:
 *          "iTunesMetadata" -> PLIST_DATA
 *          "ApplicationSINF" -> PLIST_DATA
 *          "PackageType" -> "Developer"
 *        If PackageType -> Developer is specified, then pkg_path points to
 *        an .app directory instead of an install package.
 * @param status_cb Callback function for progress and status information. If
 *        NULL is passed, this function will run synchronously.
 *
 * @return INSTPROXY_E_SUCCESS on success or an INSTPROXY_E_* error value if
 *     an error occured.
 *
 * @note If a callback function is given (async mode), this function returns
 *     INSTPROXY_E_SUCCESS immediately if the status updater thread has been
 *     created successfully; any error occuring during the operation has to be
 *     handled inside the specified callback function.
 */
void instproxy_install(instproxy_client_t client, const char *pkg_path, plist_t client_options, instproxy_status_cb_t status_cb, void *user_data, GError **error)
{
	instproxy_install_or_upgrade(client, pkg_path, client_options, status_cb, "Install", user_data, error);
}

/**
 * Upgrade an application on the device. This function is nearly the same as
 * instproxy_install; the difference is that the installation progress on the
 * device is faster if the application is already installed.
 *
 * @param client The connected installation_proxy client
 * @param pkg_path Path of the installation package (inside the AFC jail)
 * @param client_options The client options to use, as PLIST_DICT, or NULL.
 *        Valid options include:
 *          "iTunesMetadata" -> PLIST_DATA
 *          "ApplicationSINF" -> PLIST_DATA
 *          "PackageType" -> "Developer"
 *        If PackageType -> Developer is specified, then pkg_path points to
 *        an .app directory instead of an install package.
 * @param status_cb Callback function for progress and status information. If
 *        NULL is passed, this function will run synchronously.
 *
 * @return INSTPROXY_E_SUCCESS on success or an INSTPROXY_E_* error value if
 *     an error occured.
 *
 * @note If a callback function is given (async mode), this function returns
 *     INSTPROXY_E_SUCCESS immediately if the status updater thread has been
 *     created successfully; any error occuring during the operation has to be
 *     handled inside the specified callback function.
 */
void instproxy_upgrade(instproxy_client_t client, const char *pkg_path, plist_t client_options, instproxy_status_cb_t status_cb, void *user_data, GError **error)
{
	return instproxy_install_or_upgrade(client, pkg_path, client_options, status_cb, "Upgrade", user_data, error);
}

/**
 * Uninstall an application from the device.
 *
 * @param client The connected installation proxy client
 * @param appid ApplicationIdentifier of the app to uninstall
 * @param client_options The client options to use, as PLIST_DICT, or NULL.
 *        Currently there are no known client options, so pass NULL here.
 * @param status_cb Callback function for progress and status information. If
 *        NULL is passed, this function will run synchronously.
 *
 * @return INSTPROXY_E_SUCCESS on success or an INSTPROXY_E_* error value if
 *     an error occured.
 *
 * @note If a callback function is given (async mode), this function returns
 *     INSTPROXY_E_SUCCESS immediately if the status updater thread has been
 *     created successfully; any error occuring during the operation has to be
 *     handled inside the specified callback function.
 */
void instproxy_uninstall(instproxy_client_t client, const char *appid, plist_t client_options, instproxy_status_cb_t status_cb, void *user_data, GError **error)
{
	g_assert(client != NULL && client->parent != NULL && appid != NULL);

	if (client->status_updater) {
		g_set_error(error, INSTPROXY_CLIENT_ERROR,
			INSTPROXY_E_OP_IN_PROGRESS,
			"Operation already in progress");
		return;
	}

	plist_t dict = plist_new_dict();
	plist_dict_insert_item(dict, "ApplicationIdentifier", plist_new_string(appid));
	plist_dict_insert_item(dict, "Command", plist_new_string("Uninstall"));

	instproxy_lock(client);
	GError *tmp_error = NULL;
	instproxy_send_command(client, "Uninstall", client_options, appid, NULL, &tmp_error);
	instproxy_unlock(client);

	plist_free(dict);

	if (tmp_error != NULL) {
		debug_info("could not send plist, error %d, reason %s", tmp_error->code, tmp_error->message);
		g_propagate_error(error, tmp_error);
		return;
	}

	instproxy_create_status_updater(client, status_cb, "Uninstall", user_data, error);
}

/**
 * List archived applications. This function runs synchronously.
 *
 * @see instproxy_archive
 *
 * @param client The connected installation_proxy client
 * @param client_options The client options to use, as PLIST_DICT, or NULL.
 *        Currently there are no known client options, so pass NULL here.
 * @param result Pointer that will be set to a plist containing a PLIST_DICT
 *        holding information about the archived applications found.
 *
 * @return INSTPROXY_E_SUCCESS on success or an INSTPROXY_E_* error value if
 *     an error occured.
 */
plist_t instproxy_lookup_archives(instproxy_client_t client, plist_t client_options, GError **error)
{
	g_assert(client != NULL && client->parent != NULL);

	plist_t result = NULL;
	GError *tmp_error = NULL;

	instproxy_lock(client);
	instproxy_send_command(client, "LookupArchives", client_options, NULL, NULL, &tmp_error);

	if (tmp_error != NULL) {
		debug_info("could not send plist, error %d, reason: %s", tmp_error->code, tmp_error->message);
		g_propagate_error(error, tmp_error);
		goto leave_unlock;
	}

	result = property_list_service_receive_plist(client->parent, &tmp_error);
	if (tmp_error != NULL) {
		debug_info("could not receive plist, error %d, reason: %s", tmp_error->code, tmp_error->message);
		g_propagate_error(error, tmp_error);
		goto leave_unlock;
	}

leave_unlock:
	instproxy_unlock(client);
	return result;
}

/**
 * Archive an application on the device.
 * This function tells the device to make an archive of the specified
 * application. This results in the device creating a ZIP archive in the
 * 'ApplicationArchives' directory and uninstalling the application.
 *
 * @param client The connected installation proxy client
 * @param appid ApplicationIdentifier of the app to archive.
 * @param client_options The client options to use, as PLIST_DICT, or NULL.
 *        Valid options include:
 *          "SkipUninstall" -> Boolean
 *          "ArchiveType" -> "ApplicationOnly" 
 * @param status_cb Callback function for progress and status information. If
 *        NULL is passed, this function will run synchronously.
 *
 * @return INSTPROXY_E_SUCCESS on success or an INSTPROXY_E_* error value if
 *     an error occured.
 *
 * @note If a callback function is given (async mode), this function returns
 *     INSTPROXY_E_SUCCESS immediately if the status updater thread has been
 *     created successfully; any error occuring during the operation has to be
 *     handled inside the specified callback function.
 */
void instproxy_archive(instproxy_client_t client, const char *appid, plist_t client_options, instproxy_status_cb_t status_cb, void *user_data, GError **error)
{
	g_assert(client != NULL && client->parent != NULL && appid != NULL);

	if (client->status_updater) {
		g_set_error(error, INSTPROXY_CLIENT_ERROR,
			INSTPROXY_E_OP_IN_PROGRESS,
			"Operation already in progress");
		return;
	}

	GError *tmp_error = NULL;

	instproxy_lock(client);
	instproxy_send_command(client, "Archive", client_options, appid, NULL, &tmp_error);
	instproxy_unlock(client);

	if (tmp_error != NULL) {
		debug_info("could not send plist, error %d, reason %s",
			tmp_error->code, tmp_error->message);
		g_propagate_error(error, tmp_error);
		return;
	}
	instproxy_create_status_updater(client, status_cb, "Archive", user_data, error);
}

/**
 * Restore a previously archived application on the device.
 * This function is the counterpart to instproxy_archive.
 * @see instproxy_archive
 *
 * @param client The connected installation proxy client
 * @param appid ApplicationIdentifier of the app to restore.
 * @param client_options The client options to use, as PLIST_DICT, or NULL.
 *        Currently there are no known client options, so pass NULL here.
 * @param status_cb Callback function for progress and status information. If
 *        NULL is passed, this function will run synchronously.
 *
 * @return INSTPROXY_E_SUCCESS on success or an INSTPROXY_E_* error value if
 *     an error occured.
 *
 * @note If a callback function is given (async mode), this function returns
 *     INSTPROXY_E_SUCCESS immediately if the status updater thread has been
 *     created successfully; any error occuring during the operation has to be
 *     handled inside the specified callback function.
 */
void instproxy_restore(instproxy_client_t client, const char *appid, plist_t client_options, instproxy_status_cb_t status_cb, void *user_data, GError **error)
{
	g_assert(client != NULL && client->parent != NULL && appid != NULL);

	if (client->status_updater) {
		g_set_error(error, INSTPROXY_CLIENT_ERROR,
			INSTPROXY_E_OP_IN_PROGRESS,
			"Operation already in progress");
		return;
	}

	GError *tmp_error = NULL;

	instproxy_lock(client);
	instproxy_send_command(client, "Restore", client_options, appid, NULL, &tmp_error);
	instproxy_unlock(client);

	if (tmp_error != NULL) {
		debug_info("could not send plist, error %d, reason %s",
			tmp_error->code, tmp_error->message);
		g_propagate_error(error, tmp_error);
		return;
	}
	instproxy_create_status_updater(client, status_cb, "Restore", user_data, error);
}

/**
 * Removes a previously archived application from the device.
 * This function removes the ZIP archive from the 'ApplicationArchives'
 * directory.
 *
 * @param client The connected installation proxy client
 * @param appid ApplicationIdentifier of the archived app to remove.
 * @param client_options The client options to use, as PLIST_DICT, or NULL.
 *        Currently there are no known client options, so passing NULL is fine.
 * @param status_cb Callback function for progress and status information. If
 *        NULL is passed, this function will run synchronously.
 *
 * @return INSTPROXY_E_SUCCESS on success or an INSTPROXY_E_* error value if
 *     an error occured.
 *
 * @note If a callback function is given (async mode), this function returns
 *     INSTPROXY_E_SUCCESS immediately if the status updater thread has been
 *     created successfully; any error occuring during the operation has to be
 *     handled inside the specified callback function.
 */
void instproxy_remove_archive(instproxy_client_t client, const char *appid, plist_t client_options, instproxy_status_cb_t status_cb, void *user_data, GError **error)
{
	g_assert(client != NULL && client->parent != NULL && appid != NULL);

	if (client->status_updater) {
		g_set_error(error, INSTPROXY_CLIENT_ERROR,
			INSTPROXY_E_OP_IN_PROGRESS,
			"Operation already in progress");
		return;
	}

	GError *tmp_error = NULL;

	instproxy_lock(client);
	instproxy_send_command(client, "RemoveArchive", client_options, appid, NULL, &tmp_error);
	instproxy_unlock(client);

	if (tmp_error != NULL) {
		debug_info("could not send plist, error %d, reason %s",
			tmp_error->code, tmp_error->message);
		g_propagate_error(error, tmp_error);
		return;
	}
	instproxy_create_status_updater(client, status_cb, "RemoveArchive", user_data, error);
}

/**
 * Create a new client_options plist.
 *
 * @return A new plist_t of type PLIST_DICT. 
 */
plist_t instproxy_client_options_new()
{
	return plist_new_dict();
}

/**
 * Add one or more new key:value pairs to the given client_options.
 *
 * @param client_options The client options to modify.
 * @param ... KEY, VALUE, [KEY, VALUE], NULL
 *
 * @note The keys and values passed are expected to be strings, except for
 *       "ApplicationSINF" and "iTunesMetadata" expecting a plist node of type
 *       PLIST_DATA as value, or "SkipUninstall" needing int as value.
 */
void instproxy_client_options_add(plist_t client_options, ...)
{
	if (!client_options)
		return;
	va_list args;
	va_start(args, client_options);
	char *arg = va_arg(args, char*);
	while (arg) {
		char *key = strdup(arg);
		if (!strcmp(key, "SkipUninstall")) {
			int intval = va_arg(args, int);
			plist_dict_insert_item(client_options, key, plist_new_bool(intval));
		} else if (!strcmp(key, "ApplicationSINF") || !strcmp(key, "iTunesMetadata")) {
			plist_t plistval = va_arg(args, plist_t);
			if (!plistval) {
				free(key);
				break;
			}
			plist_dict_insert_item(client_options, key, plist_copy(plistval));
		} else {
			char *strval = va_arg(args, char*);
			if (!strval) {
				free(key);
				break;
			}
			plist_dict_insert_item(client_options, key, plist_new_string(strval));
		}
		free(key);
		arg = va_arg(args, char*);
	}
	va_end(args);
}

/**
 * Free client_options plist.
 *
 * @param client_options The client options plist to free. Does nothing if NULL
 *        is passed.
 */
void instproxy_client_options_free(plist_t client_options)
{
	if (client_options) {
		plist_free(client_options);
	}
}
