/**
 * @file libimobiledevice/sbservices.h
 * @brief Implementation to talk to the SpringBoard services on a device
 * \internal
 *
 * Copyright (c) 2009 Nikias Bassen All Rights Reserved.
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

#ifndef SB_SERVICES_H
#define SB_SERVICES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libimobiledevice/libimobiledevice.h>

/** @name Error Codes */
typedef enum {
	SBSERVICES_E_SUCCESS            =    0,
	SBSERVICES_E_INVALID_ARG        =   -1,
	SBSERVICES_E_PLIST_ERROR        =   -2,
	SBSERVICES_E_CONN_FAILED        =   -3,
	SBSERVICES_E_UNKNOWN_ERROR      = -256
} SBServicesClientErrorEnum;

#define SBSERVICES_CLIENT_ERROR sbservices_client_error_quark()
GQuark       sbservices_client_error_quark      (void);

/** Represents an error code. */
typedef int16_t sbservices_error_t;

typedef struct sbservices_client_private sbservices_client_private;
typedef sbservices_client_private *sbservices_client_t; /**< The client handle. */

/* Interface */
sbservices_client_t sbservices_client_new(idevice_t device, uint16_t port, GError **error);
void sbservices_client_free(sbservices_client_t client, GError **error);
plist_t sbservices_get_icon_state(sbservices_client_t client, GError **error);
void sbservices_set_icon_state(sbservices_client_t client, plist_t newstate, GError **error);
void sbservices_get_icon_pngdata(sbservices_client_t client, const char *bundleId, char **pngdata, uint64_t *pngsize, GError **error);

#ifdef __cplusplus
}
#endif

#endif
