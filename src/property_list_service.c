/* 
 * property_list_service.c
 * PropertyList service implementation.
 *
 * Copyright (c) 2010 Nikias Bassen. All Rights Reserved.
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
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "property_list_service.h"
#include "idevice.h"
#include "debug.h"

GQuark
property_list_service_error_quark (void)
{
  return g_quark_from_static_string ("property-list-service-error-quark");
}

/**
 * Convert an idevice_error_t value to an property_list_service_error_t value.
 * Used internally to get correct error codes.
 *
 * @param err An idevice_error_t error code
 *
 * @return A matching property_list_service_error_t error code,
 *     PROPERTY_LIST_SERVICE_E_UNKNOWN_ERROR otherwise.
 */
static property_list_service_error_t idevice_to_property_list_service_error(idevice_error_t err)
{
	switch (err) {
		case IDEVICE_E_SUCCESS:
			return PROPERTY_LIST_SERVICE_E_SUCCESS;
		case IDEVICE_E_INVALID_ARG:
			return PROPERTY_LIST_SERVICE_E_INVALID_ARG;
		case IDEVICE_E_SSL_ERROR:
			return PROPERTY_LIST_SERVICE_E_SSL_ERROR;
		default:
			break;
	}
	return PROPERTY_LIST_SERVICE_E_UNKNOWN_ERROR;
}

/**
 * Creates a new property list service for the specified port.
 * 
 * @param device The device to connect to.
 * @param port The port on the device to connect to, usually opened by a call to
 *     lockdownd_start_service.
 * @param client Pointer that will be set to a newly allocated
 *     property_list_service_client_t upon successful return.
 *
 * @return PROPERTY_LIST_SERVICE_E_SUCCESS on success,
 *     PROPERTY_LIST_SERVICE_E_INVALID_ARG when one of the arguments is invalid,
 *     or PROPERTY_LIST_SERVICE_E_MUX_ERROR when connecting to the device failed.
 */
property_list_service_client_t property_list_service_client_new(idevice_t device, uint16_t port, GError **error)
{
	GError *idevice_error = NULL;

	g_assert(device != NULL && port > 0);

	/* Attempt connection */
	idevice_connection_t connection = idevice_connect(device, port, &idevice_error);
	if (connection == NULL) {
		g_set_error_literal(error, PROPERTY_LIST_SERVICE_ERROR,
			PROPERTY_LIST_SERVICE_E_MUX_ERROR,
			idevice_error->message);
		g_error_free(idevice_error);
		return NULL;
	}

	/* create client object */
	property_list_service_client_t client_loc = (property_list_service_client_t)malloc(sizeof(struct property_list_service_client_private));
	client_loc->connection = connection;

	return client_loc;
}

/**
 * Frees a PropertyList service.
 *
 * @param client The property list service to free.
 *
 * @return PROPERTY_LIST_SERVICE_E_SUCCESS on success,
 *     PROPERTY_LIST_SERVICE_E_INVALID_ARG when client is invalid, or a
 *     PROPERTY_LIST_SERVICE_E_UNKNOWN_ERROR when another error occured.
 */
void property_list_service_client_free(property_list_service_client_t client, GError **error)
{
	g_assert(client != NULL);

	GError *idevice_error = NULL;
	idevice_disconnect(client->connection, &idevice_error);
	if (idevice_error != NULL) {
		g_propagate_error(error, idevice_error);
	}

	free(client);
}

/**
 * Sends a plist using the given property list service client.
 * Internally used generic plist send function.
 *
 * @param client The property list service client to use for sending.
 * @param plist plist to send
 * @param binary 1 = send binary plist, 0 = send xml plist
 *
 * @return PROPERTY_LIST_SERVICE_E_SUCCESS on success,
 *      PROPERTY_LIST_SERVICE_E_INVALID_ARG when one or more parameters are
 *      invalid, PROPERTY_LIST_SERVICE_E_PLIST_ERROR when dict is not a valid
 *      plist, or PROPERTY_LIST_SERVICE_E_UNKNOWN_ERROR when an unspecified
 *      error occurs.
 */
static void internal_plist_send(property_list_service_client_t client, plist_t plist, int binary, GError **error)
{
	char *content = NULL;
	uint32_t length = 0;
	uint32_t nlen = 0;
	int bytes = 0;
	GError *idevice_error = NULL;

	g_assert(client != NULL && client->connection != NULL && plist != NULL);

	if (binary) {
		plist_to_bin(plist, &content, &length);
	} else {
		plist_to_xml(plist, &content, &length);
	}

	if (!content || length == 0) {
		g_set_error(error, PROPERTY_LIST_SERVICE_ERROR,
			PROPERTY_LIST_SERVICE_E_PLIST_ERROR,
			"Property list error");
		return;
	}

	nlen = GUINT32_TO_BE(length);
	debug_info("sending %d bytes", length);
	bytes = idevice_connection_send(client->connection, (const char*)&nlen, sizeof(nlen), &idevice_error);
	if (bytes == sizeof(nlen)) {
		if (idevice_error != NULL) { // ignore error
			g_error_free(idevice_error);
			idevice_error = NULL;
		}
		bytes = idevice_connection_send(client->connection, content, length, &idevice_error);
		if (bytes > 0) {
			debug_info("sent %d bytes", bytes);
			debug_plist(plist);
			if ((uint32_t)bytes != length) {
				debug_info("ERROR: Could not send all data (%d of %d)!", bytes, length);
				g_set_error(error, PROPERTY_LIST_SERVICE_ERROR,
					PROPERTY_LIST_SERVICE_E_UNKNOWN_ERROR,
					"Could not send all data (%d of %d)",
					bytes, length);
			}
		}
	}
	if (bytes <= 0) {
		debug_info("ERROR: sending to device failed.");
		g_set_error(error, PROPERTY_LIST_SERVICE_ERROR,
			PROPERTY_LIST_SERVICE_E_UNKNOWN_ERROR,
			"Sending to device failed");
	}

	if (idevice_error != NULL) {
		g_error_free(idevice_error);
		idevice_error = NULL;
	}
	free(content);
}

/**
 * Sends an XML plist.
 *
 * @param client The property list service client to use for sending.
 * @param plist plist to send
 *
 * @return PROPERTY_LIST_SERVICE_E_SUCCESS on success,
 *      PROPERTY_LIST_SERVICE_E_INVALID_ARG when client or plist is NULL,
 *      PROPERTY_LIST_SERVICE_E_PLIST_ERROR when dict is not a valid plist,
 *      or PROPERTY_LIST_SERVICE_E_UNKNOWN_ERROR when an unspecified error occurs.
 */
void property_list_service_send_xml_plist(property_list_service_client_t client, plist_t plist, GError **error)
{
	internal_plist_send(client, plist, 0, error);
}

/**
 * Sends a binary plist.
 *
 * @param client The property list service client to use for sending.
 * @param plist plist to send
 *
 * @return PROPERTY_LIST_SERVICE_E_SUCCESS on success,
 *      PROPERTY_LIST_SERVICE_E_INVALID_ARG when client or plist is NULL,
 *      PROPERTY_LIST_SERVICE_E_PLIST_ERROR when dict is not a valid plist,
 *      or PROPERTY_LIST_SERVICE_E_UNKNOWN_ERROR when an unspecified error occurs.
 */
void property_list_service_send_binary_plist(property_list_service_client_t client, plist_t plist, GError **error)
{
	internal_plist_send(client, plist, 1, error);
}

/**
 * Receives a plist using the given property list service client.
 * Internally used generic plist receive function.
 *
 * @param client The property list service client to use for receiving
 * @param plist pointer to a plist_t that will point to the received plist
 *      upon successful return
 * @param timeout Maximum time in milliseconds to wait for data.
 *
 * @return PROPERTY_LIST_SERVICE_E_SUCCESS on success,
 *      PROPERTY_LIST_SERVICE_E_INVALID_ARG when client or *plist is NULL,
 *      PROPERTY_LIST_SERVICE_E_PLIST_ERROR when the received data cannot be
 *      converted to a plist, PROPERTY_LIST_SERVICE_E_MUX_ERROR when a
 *      communication error occurs, or PROPERTY_LIST_SERVICE_E_UNKNOWN_ERROR
 *      when an unspecified error occurs.
 */
static plist_t internal_plist_receive_timeout(property_list_service_client_t client, unsigned int timeout, GError **error)
{
	//property_list_service_error_t res = PROPERTY_LIST_SERVICE_E_UNKNOWN_ERROR;
	uint32_t pktlen = 0;
	uint32_t bytes = 0;
	plist_t plist = NULL;
	plist_t res = NULL;
	GError *idevice_error = NULL;

	if (!client || (client && !client->connection)) {
		g_set_error(error, PROPERTY_LIST_SERVICE_ERROR,
			PROPERTY_LIST_SERVICE_E_INVALID_ARG,
			"Invalid argument");
		return NULL;
	}

	idevice_connection_receive_timeout(client->connection, (char*)&pktlen, sizeof(pktlen), &bytes, timeout, &idevice_error);
	debug_info("initial read=%i", bytes);
	if (bytes < 4) {
		debug_info("initial read failed!");
		if (idevice_error != NULL)
			g_error_free(idevice_error);
		g_set_error(error, PROPERTY_LIST_SERVICE_ERROR,
			PROPERTY_LIST_SERVICE_E_MUX_ERROR,
			"Initial read failed");
		return NULL;
	} else {
		if ((char)pktlen == 0) { /* prevent huge buffers */
			uint32_t curlen = 0;
			char *content = NULL;
			pktlen = GUINT32_FROM_BE(pktlen);
			debug_info("%d bytes following", pktlen);
			content = (char*)malloc(pktlen);

			while (curlen < pktlen) {
				idevice_connection_receive(client->connection, content+curlen, pktlen-curlen, &bytes, &idevice_error);
				if (bytes <= 0) {
					res = NULL;
					break;
				}
				debug_info("received %d bytes", bytes);
				curlen += bytes;
			}
			if (!memcmp(content, "bplist00", 8)) {
				plist_from_bin(content, pktlen, &plist);
			} else {
				plist_from_xml(content, pktlen, &plist);
			}
			if (plist != NULL) {
				debug_plist(plist);
				res = plist;
			} else {
				g_set_error(error, PROPERTY_LIST_SERVICE_ERROR,
					PROPERTY_LIST_SERVICE_E_PLIST_ERROR,
					"Property list error");
				res = NULL;
			}
			free(content);
			content = NULL;
		} else {
			g_set_error(error, PROPERTY_LIST_SERVICE_ERROR,
				PROPERTY_LIST_SERVICE_E_UNKNOWN_ERROR,
				"Unknown error");
		}
	}
	return res;
}

/**
 * Receives a plist using the given property list service client with specified
 * timeout.
 * Binary or XML plists are automatically handled.
 *
 * @param client The property list service client to use for receiving
 * @param plist pointer to a plist_t that will point to the received plist
 *              upon successful return
 * @param timeout Maximum time in milliseconds to wait for data.
 *
 * @return PROPERTY_LIST_SERVICE_E_SUCCESS on success,
 *      PROPERTY_LIST_SERVICE_E_INVALID_ARG when connection or *plist is NULL,
 *      PROPERTY_LIST_SERVICE_E_PLIST_ERROR when the received data cannot be
 *      converted to a plist, PROPERTY_LIST_SERVICE_E_MUX_ERROR when a
 *      communication error occurs, or PROPERTY_LIST_SERVICE_E_UNKNOWN_ERROR when
 *      an unspecified error occurs.
 */
plist_t property_list_service_receive_plist_with_timeout(property_list_service_client_t client, unsigned int timeout, GError **error)
{
	return internal_plist_receive_timeout(client, timeout, error);
}

/**
 * Receives a plist using the given property list service client.
 * Binary or XML plists are automatically handled.
 *
 * This function is like property_list_service_receive_plist_with_timeout
 *   using a timeout of 10 seconds.
 * @see property_list_service_receive_plist_with_timeout
 *
 * @param client The property list service client to use for receiving
 * @param plist pointer to a plist_t that will point to the received plist
 *      upon successful return
 *
 * @return PROPERTY_LIST_SERVICE_E_SUCCESS on success,
 *      PROPERTY_LIST_SERVICE_E_INVALID_ARG when client or *plist is NULL,
 *      PROPERTY_LIST_SERVICE_E_PLIST_ERROR when the received data cannot be
 *      converted to a plist, PROPERTY_LIST_SERVICE_E_MUX_ERROR when a
 *      communication error occurs, or PROPERTY_LIST_SERVICE_E_UNKNOWN_ERROR when
 *      an unspecified error occurs.
 */
plist_t property_list_service_receive_plist(property_list_service_client_t client, GError **error)
{
	return internal_plist_receive_timeout(client, 10000, error);
}

/**
 * Enable SSL for the given property list service client.
 *
 * @param client The connected property list service client for which SSL
 *     should be enabled.
 *
 * @return PROPERTY_LIST_SERVICE_E_SUCCESS on success,
 *     PROPERTY_LIST_SERVICE_E_INVALID_ARG if client or client->connection is
 *     NULL, PROPERTY_LIST_SERVICE_E_SSL_ERROR when SSL could not be enabled,
 *     or PROPERTY_LIST_SERVICE_E_UNKNOWN_ERROR otherwise.
 */
void property_list_service_enable_ssl(property_list_service_client_t client, GError **error)
{
	g_assert(client != NULL && client->connection != NULL);

	idevice_connection_enable_ssl(client->connection, error);
}

/**
 * Disable SSL for the given property list service client.
 *
 * @param client The connected property list service client for which SSL
 *     should be disabled.
 *
 * @return PROPERTY_LIST_SERVICE_E_SUCCESS on success,
 *     PROPERTY_LIST_SERVICE_E_INVALID_ARG if client or client->connection is
 *     NULL, or PROPERTY_LIST_SERVICE_E_UNKNOWN_ERROR otherwise.
 */
void property_list_service_disable_ssl(property_list_service_client_t client)
{
	g_assert(client != NULL && client->connection != NULL);
	idevice_connection_disable_ssl(client->connection);
}

