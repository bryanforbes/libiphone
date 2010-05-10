/**
 * @file libimobiledevice/mobilesync.h
 * @brief MobileSync Implementation
 * \internal
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

#ifndef IMOBILESYNC_H
#define IMOBILESYNC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libimobiledevice/libimobiledevice.h>
#include <time.h>

/** @name Error Codes */
/*@{*/
#define MOBILESYNC_E_SUCCESS                0
#define MOBILESYNC_E_INVALID_ARG           -1
#define MOBILESYNC_E_PLIST_ERROR           -2
#define MOBILESYNC_E_MUX_ERROR             -3
#define MOBILESYNC_E_BAD_VERSION           -4
#define MOBILESYNC_E_SYNC_REFUSED          -5
#define MOBILESYNC_E_CANCELLED             -6

#define MOBILESYNC_E_UNKNOWN_ERROR       -256
/*@}*/

typedef enum {
	MOBILESYNC_SYNC_TYPE_FAST,
	MOBILESYNC_SYNC_TYPE_SLOW,
	MOBILESYNC_SYNC_TYPE_RESET
} mobilesync_sync_type_t;

/** Represents an error code. */
typedef int16_t mobilesync_error_t;

typedef struct mobilesync_client_private mobilesync_client_private;
typedef mobilesync_client_private *mobilesync_client_t; /**< The client handle */

typedef struct {
	char *device_anchor;
	char *host_anchor;
	int version;
} mobilesync_anchor_exchange;
typedef mobilesync_anchor_exchange *mobilesync_anchor_exchange_t;

typedef char* (*mobilesync_process_device_changes_cb_t) (const char* data_class, plist_t entity_mapping, uint8_t more_changes, void *user_data);
typedef char* (*mobilesync_process_device_remapping_cb_t) (const char* data_class, plist_t entity_remapping, void *user_data);

mobilesync_error_t mobilesync_client_new(idevice_t device, uint16_t port, mobilesync_client_t * client);
mobilesync_error_t mobilesync_client_free(mobilesync_client_t client);
mobilesync_error_t mobilesync_receive(mobilesync_client_t client, plist_t *plist);
mobilesync_error_t mobilesync_send(mobilesync_client_t client, plist_t plist);

mobilesync_error_t mobilesync_start_session(mobilesync_client_t client, const char* data_class, mobilesync_anchor_exchange_t anchor_exchange, mobilesync_sync_type_t* sync_type);
mobilesync_error_t mobilesync_finish_session(mobilesync_client_t client, const char* data_class);

mobilesync_error_t mobilesync_get_all_records(mobilesync_client_t client, const char* data_class, mobilesync_process_device_changes_cb_t process_changes_cb, void *user_data);
mobilesync_error_t mobilesync_get_changed_records(mobilesync_client_t client, const char* data_class, mobilesync_process_device_changes_cb_t process_changes_cb, void *user_data);
mobilesync_error_t mobilesync_send_changes(mobilesync_client_t client, const char* data_class, plist_t *changes, mobilesync_process_device_remapping_cb_t process_remapping_cb, void *user_data);

mobilesync_error_t mobilesync_cancel(mobilesync_client_t client, const char* data_class, const char* reason);

#ifdef __cplusplus
}
#endif

#endif
