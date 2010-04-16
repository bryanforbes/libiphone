/**
 * @file libimobiledevice/mobile_image_mounter.h
 * @brief Implementation of the mobile image mounter service.
 * \internal
 *
 * Copyright (c) 2010 Nikias Bassen All Rights Reserved.
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

#ifndef MOBILE_IMAGE_MOUNTER_H
#define MOBILE_IMAGE_MOUNTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libimobiledevice/libimobiledevice.h>

/** @name Error Codes */
typedef enum {
	MOBILE_IMAGE_MOUNTER_E_SUCCESS           =    0,
	MOBILE_IMAGE_MOUNTER_E_INVALID_ARG       =   -1,
	MOBILE_IMAGE_MOUNTER_E_PLIST_ERROR       =   -2,
	MOBILE_IMAGE_MOUNTER_E_CONN_FAILED       =   -3,
	MOBILE_IMAGE_MOUNTER_E_UNKNOWN_ERROR     = -256
} MobileImageMounterErrorEnum;

#define MOBILE_IMAGE_MOUNTER_CLIENT_ERROR mobile_image_mounter_client_error_quark()
GQuark       mobile_image_mounter_client_error_quark      (void);

/** Represents an error code. */
typedef int16_t mobile_image_mounter_error_t;

typedef struct mobile_image_mounter_client_private mobile_image_mounter_client_private;
typedef mobile_image_mounter_client_private *mobile_image_mounter_client_t; /**< The client handle. */

/* Interface */
mobile_image_mounter_client_t mobile_image_mounter_new(idevice_t device, uint16_t port, GError **error);
void mobile_image_mounter_free(mobile_image_mounter_client_t client, GError **error);
plist_t mobile_image_mounter_lookup_image(mobile_image_mounter_client_t client, const char *image_type, GError **error);
plist_t mobile_image_mounter_mount_image(mobile_image_mounter_client_t client, const char *image_path, const char *image_signature, uint16_t signature_length, const char *image_type, GError **error);
void mobile_image_mounter_hangup(mobile_image_mounter_client_t client, GError **error);

#ifdef __cplusplus
}
#endif

#endif
