/**
 * @file libimobiledevice/lockdown.h
 * @brief Implementation to communicate with the lockdown device daemon
 * \internal
 *
 * Copyright (c) 2008 Zach C. All Rights Reserved.
 * Copyright (c) 2009 Martin S. All Rights Reserved.
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

#ifndef LOCKDOWN_H
#define LOCKDOWN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libimobiledevice/libimobiledevice.h>

/** @name Error Codes */
typedef enum {
	LOCKDOWN_E_SUCCESS                   =    0,
	LOCKDOWN_E_INVALID_ARG               =   -1,
	LOCKDOWN_E_INVALID_CONF              =   -2,
	LOCKDOWN_E_PLIST_ERROR               =   -3,
	LOCKDOWN_E_PAIRING_FAILED            =   -4,
	LOCKDOWN_E_SSL_ERROR                 =   -5,
	LOCKDOWN_E_DICT_ERROR                =   -6,
	LOCKDOWN_E_START_SERVICE_FAILED      =   -7,
	LOCKDOWN_E_NOT_ENOUGH_DATA           =   -8,
	LOCKDOWN_E_SET_VALUE_PROHIBITED      =   -9,
	LOCKDOWN_E_GET_VALUE_PROHIBITED      =  -10,
	LOCKDOWN_E_REMOVE_VALUE_PROHIBITED   =  -11,
	LOCKDOWN_E_MUX_ERROR                 =  -12,
	LOCKDOWN_E_ACTIVATION_FAILED         =  -13,
	LOCKDOWN_E_PASSWORD_PROTECTED        =  -14,
	LOCKDOWN_E_NO_RUNNING_SESSION        =  -15,
	LOCKDOWN_E_INVALID_HOST_ID           =  -16,
	LOCKDOWN_E_INVALID_SERVICE           =  -17,
	LOCKDOWN_E_INVALID_ACTIVATION_RECORD =  -18,
	LOCKDOWN_E_UNKNOWN_ERROR             = -256
} LockdowndClientErrorEnum;

#define LOCKDOWND_CLIENT_ERROR lockdownd_client_error_quark()
GQuark       lockdownd_client_error_quark      (void);

/** Represents an error code. */
typedef int16_t lockdownd_error_t;

typedef struct lockdownd_client_private lockdownd_client_private;
typedef lockdownd_client_private *lockdownd_client_t; /**< The client handle. */

struct lockdownd_pair_record {
	char *device_certificate; /**< The device certificate */
	char *host_certificate;   /**< The host certificate */
	char *host_id;            /**< A unique HostID for the host computer */
	char *root_certificate;   /**< The root certificate */
};
/** A pair record holding device, host and root certificates along the host_id */
typedef struct lockdownd_pair_record *lockdownd_pair_record_t;

/* Interface */
lockdownd_client_t lockdownd_client_new(idevice_t device, const char *label, GError **error);
lockdownd_client_t lockdownd_client_new_with_handshake(idevice_t device, const char *label, GError **error);
void lockdownd_client_free(lockdownd_client_t client, GError **error);

char* lockdownd_query_type(lockdownd_client_t client, GError **error);
plist_t lockdownd_get_value(lockdownd_client_t client, const char *domain, const char *key, GError **error);
void lockdownd_set_value(lockdownd_client_t client, const char *domain, const char *key, plist_t value, GError **error);
void lockdownd_remove_value(lockdownd_client_t client, const char *domain, const char *key, GError **error);
uint16_t lockdownd_start_service(lockdownd_client_t client, const char *service, GError **error);
void lockdownd_start_session(lockdownd_client_t client, const char *host_id, char **session_id, int *ssl_enabled, GError **error);
void lockdownd_stop_session(lockdownd_client_t client, const char *session_id, GError **error);
void lockdownd_send(lockdownd_client_t client, plist_t plist, GError **error);
plist_t lockdownd_receive(lockdownd_client_t client, GError **error);
void lockdownd_pair(lockdownd_client_t client, lockdownd_pair_record_t pair_record, GError **error);
void lockdownd_validate_pair(lockdownd_client_t client, lockdownd_pair_record_t pair_record, GError **error);
void lockdownd_unpair(lockdownd_client_t client, lockdownd_pair_record_t pair_record, GError **error);
void lockdownd_activate(lockdownd_client_t client, plist_t activation_record, GError **error);
void lockdownd_deactivate(lockdownd_client_t client, GError **error);
void lockdownd_enter_recovery(lockdownd_client_t client, GError **error);
void lockdownd_goodbye(lockdownd_client_t client, GError **error);

/* Helper */
void lockdownd_client_set_label(lockdownd_client_t client, const char *label);
char* lockdownd_get_device_uuid(lockdownd_client_t control, GError **error);
char* lockdownd_get_device_name(lockdownd_client_t client, GError **error);

#ifdef __cplusplus
}
#endif

#endif
