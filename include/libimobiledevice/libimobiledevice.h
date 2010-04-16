/**
 * @file libimobiledevice/libimobiledevice.h
 * @brief Device/Connection handling and communication
 * \internal
 *
 * Copyright (c) 2008 Jonathan Beck All Rights Reserved.
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

#ifndef LIBIMOBILEDEVICE_H
#define LIBIMOBILEDEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <plist/plist.h>
#include <glib.h>

typedef enum {
	IDEVICE_E_SUCCESS                =  0,
	IDEVICE_E_INVALID_ARG            = -1,
	IDEVICE_E_UNKNOWN_ERROR          = -2,
	IDEVICE_E_NO_DEVICE              = -3,
	IDEVICE_E_NOT_ENOUGH_DATA        = -4,
	IDEVICE_E_BAD_HEADER             = -5,
	IDEVICE_E_SSL_ERROR              = -6
} iDeviceErrorEnum;

#define IDEVICE_ERROR idevice_error_quark()
GQuark       idevice_error_quark      (void);

/** Represents an error code. */
typedef int16_t idevice_error_t;

typedef struct idevice_private idevice_private;
typedef idevice_private *idevice_t; /**< The device handle. */

typedef struct idevice_connection_private idevice_connection_private;
typedef idevice_connection_private *idevice_connection_t; /**< The connection handle. */

/* generic */
void idevice_set_debug_level(int level);

/* discovery (events/asynchronous) */
/** The event type for device add or removal */
enum idevice_event_type {
	IDEVICE_DEVICE_ADD = 1,
	IDEVICE_DEVICE_REMOVE
};

/* event data structure */
/** Provides information about the occured event. */
typedef struct {
	enum idevice_event_type event; /**< The event type. */
	const char *uuid; /**< The device unique id. */
	int conn_type; /**< The connection type. Currently only 1 for usbmuxd. */
} idevice_event_t;

/* event callback function prototype */
/** Callback to notifiy if a device was added or removed. */
typedef void (*idevice_event_cb_t) (const idevice_event_t *event, void *user_data);

/* functions */
void idevice_event_subscribe(idevice_event_cb_t callback, void *user_data, GError **error);
void idevice_event_unsubscribe(GError **error);

/* discovery (synchronous) */
void idevice_get_device_list(char ***devices, int *count, GError **error);
void idevice_device_list_free(char **devices);

/* device structure creation and destruction */
idevice_t idevice_new(const char *uuid, GError **error);
void idevice_free(idevice_t device);

/* connection/disconnection */
idevice_connection_t idevice_connect(idevice_t device, uint16_t port, GError **error);
void idevice_disconnect(idevice_connection_t connection, GError **error);

/* communication */
uint32_t idevice_connection_send(idevice_connection_t connection, const char *data, uint32_t len, GError **error);
void idevice_connection_receive_timeout(idevice_connection_t connection, char *data, uint32_t len, uint32_t *recv_bytes, unsigned int timeout, GError **error);
void idevice_connection_receive(idevice_connection_t connection, char *data, uint32_t len, uint32_t *recv_bytes, GError **error);

/* misc */
uint32_t idevice_get_handle(idevice_t device, GError **error);
char* idevice_get_uuid(idevice_t device, GError **error);

#ifdef __cplusplus
}
#endif

#endif

