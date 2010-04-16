 /* 
 * device_link_service.h
 * Definitions for the DeviceLink service
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
#ifndef DEVICE_LINK_SERVICE_H
#define DEVICE_LINK_SERVICE_H

#include "property_list_service.h"

/* Error Codes */
typedef enum {
	DEVICE_LINK_SERVICE_E_SUCCESS               =  0,
	DEVICE_LINK_SERVICE_E_INVALID_ARG           = -1,
	DEVICE_LINK_SERVICE_E_PLIST_ERROR           = -2,
	DEVICE_LINK_SERVICE_E_MUX_ERROR             = -3,
	DEVICE_LINK_SERVICE_E_BAD_VERSION           = -4,
	DEVICE_LINK_SERVICE_E_UNKNOWN_ERROR       = -256
} DeviceLinkServiceErrorEnum;

#define DEVICE_LINK_SERVICE_ERROR device_link_service_error_quark()
GQuark       device_link_service_error_quark      (void);

/** Represents an error code. */
typedef int16_t device_link_service_error_t;

struct device_link_service_client_private {
	property_list_service_client_t parent;
};

typedef struct device_link_service_client_private *device_link_service_client_t;

device_link_service_client_t device_link_service_client_new(idevice_t device, uint16_t port, GError **error);
void device_link_service_client_free(device_link_service_client_t client, GError **error);

void device_link_service_version_exchange(device_link_service_client_t client, uint64_t version_major, uint64_t version_minor, GError **error);
void device_link_service_send_ping(device_link_service_client_t client, const char *message, GError **error);
void device_link_service_send_process_message(device_link_service_client_t client, plist_t message, GError **error);
plist_t device_link_service_receive_process_message(device_link_service_client_t client, GError **error);
void device_link_service_disconnect(device_link_service_client_t client, GError **error);
void device_link_service_send(device_link_service_client_t client, plist_t plist, GError **error);
plist_t device_link_service_receive(device_link_service_client_t client, GError **error);

#endif
