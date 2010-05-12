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
#include <glib.h>

/** @name Error Codes */
/*@{*/
#define MOBILESYNC_E_SUCCESS                0
#define MOBILESYNC_E_INVALID_ARG           -1
#define MOBILESYNC_E_PLIST_ERROR           -2
#define MOBILESYNC_E_MUX_ERROR             -3
#define MOBILESYNC_E_BAD_VERSION           -4
#define MOBILESYNC_E_SYNC_REFUSED          -5
#define MOBILESYNC_E_CANCELLED             -6
#define MOBILESYNC_E_WRONG_DIRECTION       -7
#define MOBILESYNC_E_NOT_READY             -8

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
	char *computer_anchor;
} mobilesync_anchors;
typedef mobilesync_anchors *mobilesync_anchors_t;

mobilesync_error_t mobilesync_client_new(idevice_t device, uint16_t port, mobilesync_client_t * client);
mobilesync_error_t mobilesync_client_free(mobilesync_client_t client);
mobilesync_error_t mobilesync_receive(mobilesync_client_t client, plist_t *plist);
mobilesync_error_t mobilesync_send(mobilesync_client_t client, plist_t plist);

mobilesync_error_t mobilesync_session_start(mobilesync_client_t client, const char *data_class, mobilesync_anchors_t anchors, mobilesync_sync_type_t *sync_type, uint64_t *data_class_version);
mobilesync_error_t mobilesync_session_finish(mobilesync_client_t client);

/* receive */
mobilesync_error_t mobilesync_get_all_records_from_device(mobilesync_client_t client);
mobilesync_error_t mobilesync_get_changes_from_device(mobilesync_client_t client);
mobilesync_error_t mobilesync_receive_changes(mobilesync_client_t client, plist_t *entities, uint8_t *more_changes);
mobilesync_error_t mobilesync_acknowledge_changes_from_device(mobilesync_client_t client);

/* send */
mobilesync_error_t mobilesync_ready_to_send_changes_from_computer(mobilesync_client_t client);
mobilesync_error_t mobilesync_send_changes(mobilesync_client_t client, plist_t changes, uint8_t is_last_record, plist_t client_options);
mobilesync_error_t mobilesync_receive_remapping(mobilesync_client_t client, plist_t *remapping);

/* cancel */
mobilesync_error_t mobilesync_cancel(mobilesync_client_t client, const char* reason);

/* helpers */
mobilesync_anchors_t mobilesync_anchors_new(const char *device_anchor, const char *computer_anchor);
void mobilesync_anchors_free(mobilesync_anchors_t anchors);

plist_t mobilesync_client_options_new();
void mobilesync_client_options_add(plist_t client_options, ...) G_GNUC_NULL_TERMINATED;
void mobilesync_client_options_free(plist_t client_options);

#ifdef __cplusplus
}
#endif

#endif
