/*
 * afc.c 
 * Contains functions for the built-in AFC client.
 * 
 * Copyright (c) 2008 Zach C. All Rights Reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "afc.h"
#include "idevice.h"
#include "debug.h"

GQuark
afc_client_error_quark (void)
{
  return g_quark_from_static_string ("afc-client-error-quark");
}

/** The maximum size an AFC data packet can be */
static const int MAXIMUM_PACKET_SIZE = (2 << 15);

/**
 * Locks an AFC client, done for thread safety stuff
 * 
 * @param client The AFC client connection to lock
 */
static void afc_lock(afc_client_t client)
{
	debug_info("Locked");
	g_mutex_lock(client->mutex);
}

/**
 * Unlocks an AFC client, done for thread safety stuff.
 * 
 * @param client The AFC 
 */
static void afc_unlock(afc_client_t client)
{
	debug_info("Unlocked");
	g_mutex_unlock(client->mutex);
}

/**
 * Makes a connection to the AFC service on the phone.
 * 
 * @param device The device to connect to.
 * @param port The destination port.
 * @param client Pointer that will be set to a newly allocated afc_client_t
 *     upon successful return.
 * 
 * @return AFC_E_SUCCESS on success, AFC_E_INVALID_ARG when device or port is
 *  invalid, AFC_E_MUX_ERROR when the connection failed, or AFC_E_NO_MEM if
 *  there is a memory allocation problem.
 */
afc_client_t afc_client_new(idevice_t device, uint16_t port, GError **error)
{
	/* makes sure thread environment is available */
	if (!g_thread_supported())
		g_thread_init(NULL);

	g_assert(device != NULL && port > 0);

	/* attempt connection */
	idevice_connection_t connection = idevice_connect(device, port, error);
	if (connection == NULL) {
		return NULL;
	}

	afc_client_t client_loc = (afc_client_t) malloc(sizeof(struct afc_client_private));
	client_loc->connection = connection;

	/* allocate a packet */
	client_loc->afc_packet = (AFCPacket *) malloc(sizeof(AFCPacket));
	if (!client_loc->afc_packet) {
		GError *tmp_error = NULL;
		idevice_disconnect(client_loc->connection, &tmp_error);
		g_error_free(tmp_error);
		free(client_loc);
		g_set_error(error, AFC_CLIENT_ERROR,
			AFC_E_NO_MEM,
			"Out of memory");
		return NULL;
	}

	client_loc->afc_packet->packet_num = 0;
	client_loc->afc_packet->entire_length = 0;
	client_loc->afc_packet->this_length = 0;
	memcpy(client_loc->afc_packet->magic, AFC_MAGIC, AFC_MAGIC_LEN);
	client_loc->file_handle = 0;
	client_loc->lock = 0;
	client_loc->mutex = g_mutex_new();

	return client_loc;
}

/**
 * Disconnects an AFC client from the phone.
 * 
 * @param client The client to disconnect.
 */
void afc_client_free(afc_client_t client, GError **error)
{
	g_assert(client != NULL && client->connection != NULL && client->afc_packet != NULL);

	idevice_disconnect(client->connection, error);
	free(client->afc_packet);
	if (client->mutex) {
		g_mutex_free(client->mutex);
	}
	free(client);
}

/**
 * Dispatches an AFC packet over a client.
 * 
 * @param client The client to send data through.
 * @param data The data to send.
 * @param length The length to send.
 * @param bytes_sent The number of bytes actually sent.
 *
 * @return AFC_E_SUCCESS on success or an AFC_E_* error value.
 * 
 * @warning set client->afc_packet->this_length and
 *          client->afc_packet->entire_length to 0 before calling this.  The
 *          reason is that if you set them to different values, it indicates
 *          you want to send the data as two packets.
 */
static uint32_t afc_dispatch_packet(afc_client_t client, const char *data, uint32_t length, GError **error)
{
	uint32_t offset = 0;
	uint32_t sent = 0;
	uint32_t bytes_sent = 0;

	g_assert(client != NULL && client->connection != NULL && client->afc_packet != NULL);

	if (!data || !length)
		length = 0;

	client->afc_packet->packet_num++;
	if (!client->afc_packet->entire_length) {
		client->afc_packet->entire_length = (length) ? sizeof(AFCPacket) + length : sizeof(AFCPacket);
		client->afc_packet->this_length = client->afc_packet->entire_length;
	}
	if (!client->afc_packet->this_length) {
		client->afc_packet->this_length = sizeof(AFCPacket);
	}
	/* We want to send two segments; buffer+sizeof(AFCPacket) to this_length
	   is the parameters and everything beyond that is the next packet.
	   (for writing) */
	if (client->afc_packet->this_length != client->afc_packet->entire_length) {
		offset = client->afc_packet->this_length - sizeof(AFCPacket);

		debug_info("Offset: %i", offset);
		if ((length) < (client->afc_packet->entire_length - client->afc_packet->this_length)) {
			debug_info("Length did not resemble what it was supposed to based on packet");
			debug_info("length minus offset: %i", length - offset);
			debug_info("rest of packet: %i\n", client->afc_packet->entire_length - client->afc_packet->this_length);
			g_set_error(error, AFC_CLIENT_ERROR,
				AFC_E_INTERNAL_ERROR,
				"Length did not resemble what it was supposed to be");
			return 0;
		}

		/* send AFC packet header */
		AFCPacket_to_LE(client->afc_packet);
		sent = 0;
		sent = idevice_connection_send(client->connection, (void*)client->afc_packet, sizeof(AFCPacket), error);
		if (sent == 0) {
			/* FIXME: should this be handled as success?! */
			return 0;
		}
		bytes_sent += sent;

		/* send AFC packet data */
		sent = 0;
		sent = idevice_connection_send(client->connection, data, offset, error);
		if (sent == 0) {
			return 0;
		}
		bytes_sent += sent;

		debug_info("sent the first now go with the second");
		debug_info("Length: %i", length - offset);
		debug_info("Buffer: ");
		debug_buffer(data + offset, length - offset);

		sent = idevice_connection_send(client->connection, data + offset, length - offset, error);

		bytes_sent = sent;
		return bytes_sent;
	} else {
		debug_info("doin things the old way");
		debug_info("packet length = %i", client->afc_packet->this_length);

		debug_buffer((char*)client->afc_packet, sizeof(AFCPacket));

		/* send AFC packet header */
		AFCPacket_to_LE(client->afc_packet);
		sent = idevice_connection_send(client->connection, (void*)client->afc_packet, sizeof(AFCPacket), error);
		if (sent == 0) {
			return 0;
		}
		bytes_sent += sent;
		/* send AFC packet data (if there's data to send) */
		if (length > 0) {
			debug_info("packet data follows");

			debug_buffer(data, length);
			sent = idevice_connection_send(client->connection, data, length, error);
			bytes_sent += sent;
		}
		return bytes_sent;
	}
	g_set_error(error, AFC_CLIENT_ERROR,
		AFC_E_INTERNAL_ERROR,
		"Internal error");
	return 0;
}

/**
 * Receives data through an AFC client and sets a variable to the received data.
 * 
 * @param client The client to receive data on.
 * @param dump_here The char* to point to the newly-received data.
 * @param bytes_recv How much data was received.
 * 
 * @return AFC_E_SUCCESS on success or an AFC_E_* error value.
 */
static void afc_receive_data(afc_client_t client, char **dump_here, uint32_t *bytes_recv, GError **error)
{
	AFCPacket header;
	uint32_t entire_len = 0;
	uint32_t this_len = 0;
	uint32_t current_count = 0;
	uint64_t param1 = -1;

	*bytes_recv = 0;

	/* first, read the AFC header */
	idevice_connection_receive(client->connection, (char*)&header, sizeof(AFCPacket), bytes_recv, error);
	AFCPacket_from_LE(&header);
	if (*bytes_recv == 0) {
		debug_info("Just didn't get enough.");
		*dump_here = NULL;
		g_set_error(error, AFC_CLIENT_ERROR,
			AFC_E_MUX_ERROR,
			"Just didn't get enough.");
		return;
	} else if (*bytes_recv < sizeof(AFCPacket)) {
		debug_info("Did not even get the AFCPacket header");
		*dump_here = NULL;
		g_set_error(error, AFC_CLIENT_ERROR,
			AFC_E_MUX_ERROR,
			"Did not even get the AFCPacket header");
		return;
	}

	/* check if it's a valid AFC header */
	if (strncmp(header.magic, AFC_MAGIC, AFC_MAGIC_LEN)) {
		debug_info("Invalid AFC packet received (magic != " AFC_MAGIC ")!");
	}

	/* check if it has the correct packet number */
	if (header.packet_num != client->afc_packet->packet_num) {
		/* otherwise print a warning but do not abort */
		debug_info("ERROR: Unexpected packet number (%lld != %lld) aborting.", header.packet_num, client->afc_packet->packet_num);
		*dump_here = NULL;
		g_set_error(error, AFC_CLIENT_ERROR,
			AFC_E_OP_HEADER_INVALID,
			"ERROR: Unexpected packet number (%lld != %lld) aborting.",
			(long long int)header.packet_num,
			(long long int)client->afc_packet->packet_num);
		return;
	}

	/* then, read the attached packet */
	if (header.this_length < sizeof(AFCPacket)) {
		debug_info("Invalid AFCPacket header received!");
		*dump_here = NULL;
		g_set_error(error, AFC_CLIENT_ERROR,
			AFC_E_OP_HEADER_INVALID,
			"Invalid AFCPacket header received!");
		return;
	} else if ((header.this_length == header.entire_length)
			&& header.entire_length == sizeof(AFCPacket)) {
		debug_info("Empty AFCPacket received!");
		*dump_here = NULL;
		*bytes_recv = 0;
		if (header.operation == AFC_OP_DATA) {
			return;
		} else {
			g_set_error(error, AFC_CLIENT_ERROR,
				AFC_E_IO_ERROR,
				"IO Error");
			return;
		}
	}

	debug_info("received AFC packet, full len=%lld, this len=%lld, operation=0x%llx", header.entire_length, header.this_length, header.operation);

	entire_len = (uint32_t)header.entire_length - sizeof(AFCPacket);
	this_len = (uint32_t)header.this_length - sizeof(AFCPacket);

	/* this is here as a check (perhaps a different upper limit is good?) */
	if (entire_len > (uint32_t)MAXIMUM_PACKET_SIZE) {
		fprintf(stderr, "%s: entire_len is larger than MAXIMUM_PACKET_SIZE, (%d > %d)!", __func__, entire_len, MAXIMUM_PACKET_SIZE);
	}

	*dump_here = (char*)malloc(entire_len);
	if (this_len > 0) {
		idevice_connection_receive(client->connection, *dump_here, this_len, bytes_recv, error);
		if (*bytes_recv <= 0) {
			free(*dump_here);
			*dump_here = NULL;
			debug_info("Did not get packet contents!");
			g_set_error(error, AFC_CLIENT_ERROR,
				AFC_E_NOT_ENOUGH_DATA,
				"Did not get packet contents");
			return;
		} else if (*bytes_recv < this_len) {
			free(*dump_here);
			*dump_here = NULL;
			debug_info("Could not receive this_len=%d bytes", this_len);
			g_set_error(error, AFC_CLIENT_ERROR,
				AFC_E_NOT_ENOUGH_DATA,
				"Could not receive this_len=%d bytes", this_len);
			return;
		}
	}

	current_count = this_len;

	if (entire_len > this_len) {
		while (current_count < entire_len) {
			idevice_connection_receive(client->connection, (*dump_here)+current_count, entire_len - current_count, bytes_recv, error);
			if (*bytes_recv <= 0) {
				debug_info("Error receiving data (recv returned %d)", *bytes_recv);
				break;
			}
			current_count += *bytes_recv;
		}
		if (current_count < entire_len) {
			debug_info("WARNING: could not receive full packet (read %s, size %d)", current_count, entire_len);
		}
	}

	if (current_count >= sizeof(uint64_t)) {
		param1 = *(uint64_t*)(*dump_here);
	}

	debug_info("packet data size = %i", current_count);
	debug_info("packet data follows");
	debug_buffer(*dump_here, current_count);

	/* check operation types */
	if (header.operation == AFC_OP_STATUS) {
		/* status response */
		debug_info("got a status response, code=%lld", param1);

		if (param1 != AFC_E_SUCCESS) {
			/* error status */
			/* free buffer */
			free(*dump_here);
			*dump_here = NULL;
			g_set_error(error, AFC_CLIENT_ERROR,
				(afc_error_t)param1,
				"Error caught");
			return;
		}
	} else if (header.operation == AFC_OP_DATA) {
		/* data response */
		debug_info("got a data response");
	} else if (header.operation == AFC_OP_FILE_OPEN_RES) {
		/* file handle response */
		debug_info("got a file handle response, handle=%lld", param1);
	} else if (header.operation == AFC_OP_FILE_TELL_RES) {
		/* tell response */
		debug_info("got a tell response, position=%lld", param1);
	} else {
		/* unknown operation code received */
		free(*dump_here);
		*dump_here = NULL;
		*bytes_recv = 0;

		debug_info("WARNING: Unknown operation code received 0x%llx param1=%lld", header.operation, param1);
		fprintf(stderr, "%s: WARNING: Unknown operation code received 0x%llx param1=%lld", __func__, (long long)header.operation, (long long)param1);

		g_set_error(error, AFC_CLIENT_ERROR,
			AFC_E_OP_NOT_SUPPORTED,
			"Unknown operation code received 0x%llx param1=%lld",
			(long long int)header.operation,
			(long long int)param1);
		return;
	}

	*bytes_recv = current_count;
}

/**
 * Returns counts of null characters within a string.
 */
static uint32_t count_nullspaces(char *string, uint32_t number)
{
	uint32_t i = 0, nulls = 0;

	for (i = 0; i < number; i++) {
		if (string[i] == '\0')
			nulls++;
	}

	return nulls;
}

/**
 * Splits a string of tokens by null characters and returns each token in a
 * char array/list.
 *
 * @param tokens The characters to split into a list.
 * @param length The length of the tokens string.
 *
 * @return A char ** list with each token found in the string. The caller is
 *  responsible for freeing the memory.
 */
static char **make_strings_list(char *tokens, uint32_t length)
{
	uint32_t nulls = 0, i = 0, j = 0;
	char **list = NULL;

	if (!tokens || !length)
		return NULL;

	nulls = count_nullspaces(tokens, length);
	list = (char **) malloc(sizeof(char *) * (nulls + 1));
	for (i = 0; i < nulls; i++) {
		list[i] = strdup(tokens + j);
		j += strlen(list[i]) + 1;
	}
	list[i] = NULL;

	return list;
}

/**
 * Gets a directory listing of the directory requested.
 * 
 * @param client The client to get a directory listing from.
 * @param dir The directory to list. (must be a fully-qualified path)
 * @param list A char list of files in that directory, terminated by an empty
 *         string or NULL if there was an error.
 * 
 * @return AFC_E_SUCCESS on success or an AFC_E_* error value.
 */
char** afc_read_directory(afc_client_t client, const char *dir, GError **error)
{
	g_assert(client != NULL && dir != NULL);
	uint32_t bytes = 0;
	char *data = NULL, **list = NULL;
	GError *tmp_error = NULL;

	afc_lock(client);

	/* Send the command */
	client->afc_packet->operation = AFC_OP_READ_DIR;
	client->afc_packet->entire_length = 0;
	client->afc_packet->this_length = 0;
	bytes = afc_dispatch_packet(client, dir, strlen(dir)+1, &tmp_error);
	if (tmp_error != NULL) {
		afc_unlock(client);
		g_propagate_error(error, tmp_error);
		return NULL;
	}
	/* Receive the data */
	afc_receive_data(client, &data, &bytes, &tmp_error);
	if (tmp_error != NULL) {
		afc_unlock(client);
		g_propagate_error(error, tmp_error);
		return NULL;
	}
	/* Parse the data */
	list = make_strings_list(data, bytes);
	if (data)
		free(data);

	afc_unlock(client);

	return list;
}

/**
 * Get device info for a client connection to phone. The device information
 * returned is the device model as well as the free space, the total capacity
 * and blocksize on the accessed disk partition.
 * 
 * @param client The client to get device info for.
 * @param infos A char ** list of parameters as given by AFC or NULL if there
 *  was an error.
 * 
 * @return AFC_E_SUCCESS on success or an AFC_E_* error value.
 */
char** afc_get_device_info(afc_client_t client, GError **error)
{
	g_assert(client != NULL);

	uint32_t bytes = 0;
	char *data = NULL, **list = NULL;
	GError *tmp_error = NULL;

	afc_lock(client);

	/* Send the command */
	client->afc_packet->operation = AFC_OP_GET_DEVINFO;
	client->afc_packet->entire_length = client->afc_packet->this_length = 0;
	bytes = afc_dispatch_packet(client, NULL, 0, &tmp_error);
	if (tmp_error != NULL) {
		afc_unlock(client);
		g_propagate_error(error, tmp_error);
		return NULL;
	}
	/* Receive the data */
	afc_receive_data(client, &data, &bytes, &tmp_error);
	if (tmp_error != NULL) {
		afc_unlock(client);
		g_propagate_error(error, tmp_error);
		return NULL;
	}
	/* Parse the data */
	list = make_strings_list(data, bytes);
	if (data)
		free(data);

	afc_unlock(client);

	return list;
}

/**
 * Get a specific key of the device info list for a client connection.
 * Known key values are: Model, FSTotalBytes, FSFreeBytes and FSBlockSize.
 * This is a helper function for afc_get_device_info().
 *
 * @param client The client to get device info for.
 * @param key The key to get the value of.
 * @param value The value for the key if successful or NULL otherwise.
 *
 * @return AFC_E_SUCCESS on success or an AFC_E_* error value.
 */
char* afc_get_device_info_key(afc_client_t client, const char *key, GError **error)
{
	g_assert(client != NULL && key != NULL);

	char **kvps, **ptr;

	char *value = NULL;
	GError *tmp_error = NULL;

	kvps = afc_get_device_info(client, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error(error, tmp_error);
		return NULL;
	}

	for (ptr = kvps; *ptr; ptr++) {
		if (!strcmp(*ptr, key)) {
			value = strdup(*(ptr+1));
			break;
		}
	}

	g_strfreev(kvps);

	return value;
}

/**
 * Deletes a file or directory.
 * 
 * @param client The client to use.
 * @param path The path to delete. (must be a fully-qualified path)
 * 
 * @return AFC_E_SUCCESS on success or an AFC_E_* error value.
 */
void afc_remove_path(afc_client_t client, const char *path, GError **error)
{
	g_assert(client != NULL && path != NULL && client->afc_packet != NULL && client->connection != NULL);

	char *response = NULL;
	uint32_t bytes = 0;
	GError *tmp_error = NULL;

	afc_lock(client);

	/* Send command */
	client->afc_packet->this_length = client->afc_packet->entire_length = 0;
	client->afc_packet->operation = AFC_OP_REMOVE_PATH;
	bytes = afc_dispatch_packet(client, path, strlen(path)+1, &tmp_error);
	if (tmp_error != NULL) {
		afc_unlock(client);
		g_propagate_error(error, tmp_error);
		return;
	}
	/* Receive response */
	afc_receive_data(client, &response, &bytes, &tmp_error);
	if (response)
		free(response);

	/* special case; unknown error actually means directory not empty */
	if (tmp_error != NULL && tmp_error->code == AFC_E_UNKNOWN_ERROR)
		tmp_error->code = AFC_E_DIR_NOT_EMPTY;
	if (tmp_error != NULL)
		g_propagate_error(error, tmp_error);

	afc_unlock(client);
}

/**
 * Renames a file or directory on the phone.
 * 
 * @param client The client to have rename.
 * @param from The name to rename from. (must be a fully-qualified path)
 * @param to The new name. (must also be a fully-qualified path)
 * 
 * @return AFC_E_SUCCESS on success or an AFC_E_* error value.
 */
void afc_rename_path(afc_client_t client, const char *from, const char *to, GError **error)
{
	g_assert(client != NULL && from != NULL && to != NULL);
	g_assert(client->afc_packet != NULL && client->connection != NULL);

	char *response = NULL;
	char *send = (char *) malloc(sizeof(char) * (strlen(from) + strlen(to) + 1 + sizeof(uint32_t)));
	uint32_t bytes = 0;
	GError *tmp_error = NULL;

	afc_lock(client);

	/* Send command */
	memcpy(send, from, strlen(from) + 1);
	memcpy(send + strlen(from) + 1, to, strlen(to) + 1);
	client->afc_packet->entire_length = client->afc_packet->this_length = 0;
	client->afc_packet->operation = AFC_OP_RENAME_PATH;
	bytes = afc_dispatch_packet(client, send, strlen(to)+1 + strlen(from)+1, &tmp_error);
	free(send);
	if (tmp_error != NULL) {
		afc_unlock(client);
		g_propagate_error(error, tmp_error);
		return;
	}
	/* Receive response */
	afc_receive_data(client, &response, &bytes, error);
	if (response)
		free(response);

	afc_unlock(client);
}

/**
 * Creates a directory on the phone.
 * 
 * @param client The client to use to make a directory.
 * @param dir The directory's path. (must be a fully-qualified path, I assume
 *        all other mkdir restrictions apply as well)
 *
 * @return AFC_E_SUCCESS on success or an AFC_E_* error value.
 */
void afc_make_directory(afc_client_t client, const char *dir, GError **error)
{
	g_assert(client != NULL);

	uint32_t bytes = 0;
	char *response = NULL;
	GError *tmp_error = NULL;

	afc_lock(client);

	/* Send command */
	client->afc_packet->operation = AFC_OP_MAKE_DIR;
	client->afc_packet->this_length = client->afc_packet->entire_length = 0;
	bytes = afc_dispatch_packet(client, dir, strlen(dir)+1, &tmp_error);
	if (tmp_error != NULL) {
		afc_unlock(client);
		g_propagate_error(error, tmp_error);
		return;
	}
	/* Receive response */
	afc_receive_data(client, &response, &bytes, error);
	if (response)
		free(response);

	afc_unlock(client);
}

/**
 * Gets information about a specific file.
 * 
 * @param client The client to use to get the information of the file.
 * @param path The fully-qualified path to the file. 
 * @param infolist Pointer to a buffer that will be filled with a NULL-terminated
 *                 list of strings with the file information.
 *                 Set to NULL before calling this function.
 * 
 * @return AFC_E_SUCCESS on success or an AFC_E_* error value.
 */
char** afc_get_file_info(afc_client_t client, const char *path, GError **error)
{
	g_assert(client != NULL && path != NULL);

	char *received = NULL;
	char **infolist = NULL;
	uint32_t bytes = 0;
	GError *tmp_error = NULL;

	afc_lock(client);

	/* Send command */
	client->afc_packet->operation = AFC_OP_GET_FILE_INFO;
	client->afc_packet->entire_length = client->afc_packet->this_length = 0;
	bytes = afc_dispatch_packet(client, path, strlen(path)+1, &tmp_error);
	if (tmp_error != NULL) {
		afc_unlock(client);
		g_propagate_error(error, tmp_error);
		return NULL;
	}

	/* Receive data */
	afc_receive_data(client, &received, &bytes, error);
	if (received) {
		infolist = make_strings_list(received, bytes);
		free(received);
	}

	afc_unlock(client);

	return infolist;
}

/**
 * Opens a file on the phone.
 * 
 * @param client The client to use to open the file. 
 * @param filename The file to open. (must be a fully-qualified path)
 * @param file_mode The mode to use to open the file. Can be AFC_FILE_READ or
 * 		    AFC_FILE_WRITE; the former lets you read and write,
 * 		    however, and the second one will *create* the file,
 * 		    destroying anything previously there.
 * @param handle Pointer to a uint64_t that will hold the handle of the file
 * 
 * @return AFC_E_SUCCESS on success or an AFC_E_* error value.
 */
uint64_t
afc_file_open(afc_client_t client, const char *filename,
					 afc_file_mode_t file_mode, GError **error)
{
	g_assert(client != NULL && client->connection != NULL && client->afc_packet != NULL);

	uint64_t file_mode_loc = GUINT64_TO_LE(file_mode);
	uint32_t bytes = 0;
	char *data = (char *) malloc(sizeof(char) * (8 + strlen(filename) + 1));
	GError *tmp_error = NULL;

	/* set handle to 0 so in case an error occurs, the handle is invalid */
	uint64_t handle = 0;

	afc_lock(client);

	/* Send command */
	memcpy(data, &file_mode_loc, 8);
	memcpy(data + 8, filename, strlen(filename));
	data[8 + strlen(filename)] = '\0';
	client->afc_packet->operation = AFC_OP_FILE_OPEN;
	client->afc_packet->entire_length = client->afc_packet->this_length = 0;
	bytes = afc_dispatch_packet(client, data, 8 + strlen(filename) + 1, &tmp_error);
	free(data);

	if (tmp_error != NULL) {
		debug_info("Didn't receive a response to the command");
		afc_unlock(client);
		g_propagate_error(error, tmp_error);
		return 0;
	}
	/* Receive the data */
	afc_receive_data(client, &data, &bytes, &tmp_error);
	if (tmp_error == NULL && (bytes > 0) && data) {
		afc_unlock(client);

		/* Get the file handle */
		memcpy(&handle, data, sizeof(uint64_t));
		free(data);
		return handle;
	}

	debug_info("Didn't get any further data");

	afc_unlock(client);
	g_propagate_error(error, tmp_error);

	return 0;
}

/**
 * Attempts to the read the given number of bytes from the given file.
 * 
 * @param client The relevant AFC client
 * @param handle File handle of a previously opened file
 * @param data The pointer to the memory region to store the read data
 * @param length The number of bytes to read
 * @param bytes_read The number of bytes actually read.
 *
 * @return AFC_E_SUCCESS on success or an AFC_E_* error value.
 */
uint32_t
afc_file_read(afc_client_t client, uint64_t handle, char *data, uint32_t length, GError **error)
{
	g_assert(client != NULL && client->afc_packet != NULL && client->connection != NULL);
	g_assert(handle > 0);

	char *input = NULL;
	uint32_t current_count = 0, bytes_loc = 0;
	const uint32_t MAXIMUM_READ_SIZE = 1 << 16;
	GError *tmp_error = NULL;

	debug_info("called for length %i", length);

	afc_lock(client);

	/* Looping here to get around the maximum amount of data that
	   afc_receive_data can handle */
	while (current_count < length) {
		debug_info("current count is %i but length is %i", current_count, length);

		/* Send the read command */
		AFCFilePacket *packet = (AFCFilePacket *) malloc(sizeof(AFCFilePacket));
		packet->filehandle = handle;
		packet->size = GUINT64_TO_LE(((length - current_count) < MAXIMUM_READ_SIZE) ? (length - current_count) : MAXIMUM_READ_SIZE);
		client->afc_packet->operation = AFC_OP_READ;
		client->afc_packet->entire_length = client->afc_packet->this_length = 0;
		bytes_loc = afc_dispatch_packet(client, (char *) packet, sizeof(AFCFilePacket), &tmp_error);
		free(packet);

		if (tmp_error != NULL) {
			afc_unlock(client);
			g_propagate_error(error, tmp_error);
			return 0;
		}
		/* Receive the data */
		afc_receive_data(client, &input, &bytes_loc, &tmp_error);
		if (tmp_error != NULL) {
			debug_info("afc_receive_data returned error: %d", tmp_error->code);
		}
		debug_info("bytes returned: %i", bytes_loc);
		if (tmp_error != NULL) {
			afc_unlock(client);
			g_propagate_error(error, tmp_error);
			return 0;
		} else if (bytes_loc == 0) {
			if (input)
				free(input);
			afc_unlock(client);
			/* FIXME: check that's actually a success */
			return current_count;
		} else {
			if (input) {
				debug_info("%d", bytes_loc);
				memcpy(data + current_count, input, (bytes_loc > length) ? length : bytes_loc);
				free(input);
				input = NULL;
				current_count += (bytes_loc > length) ? length : bytes_loc;
			}
		}
	}
	debug_info("returning current_count as %i", current_count);

	afc_unlock(client);
	if (tmp_error != NULL)
		g_propagate_error(error, tmp_error);
	return current_count;
}

/**
 * Writes a given number of bytes to a file.
 * 
 * @param client The client to use to write to the file.
 * @param handle File handle of previously opened file. 
 * @param data The data to write to the file.
 * @param length How much data to write.
 * @param bytes_written The number of bytes actually written to the file.
 * 
 * @return AFC_E_SUCCESS on success or an AFC_E_* error value.
 */
uint32_t
afc_file_write(afc_client_t client, uint64_t handle, const char *data, uint32_t length, GError **error)
{
	g_assert(client != NULL && client->afc_packet != NULL && client->connection != NULL);
	g_assert(handle > 0);

	char *acknowledgement = NULL;
	const uint32_t MAXIMUM_WRITE_SIZE = 1 << 15;
	uint32_t current_count = 0, i = 0;
	uint32_t segments = (length / MAXIMUM_WRITE_SIZE);
	uint32_t bytes_loc = 0;
	char *out_buffer = NULL;
	GError *tmp_error = NULL;

	afc_lock(client);

	debug_info("Write length: %i", length);

	/* Divide the file into segments. */
	for (i = 0; i < segments; i++) {
		/* Send the segment */
		client->afc_packet->this_length = sizeof(AFCPacket) + 8;
		client->afc_packet->entire_length = client->afc_packet->this_length + MAXIMUM_WRITE_SIZE;
		client->afc_packet->operation = AFC_OP_WRITE;
		out_buffer = (char *) malloc(sizeof(char) * client->afc_packet->entire_length - sizeof(AFCPacket));
		memcpy(out_buffer, (char *)&handle, sizeof(uint64_t));
		memcpy(out_buffer + 8, data + current_count, MAXIMUM_WRITE_SIZE);
		bytes_loc = afc_dispatch_packet(client, out_buffer, MAXIMUM_WRITE_SIZE + 8, &tmp_error);
		if (tmp_error != NULL) {
			afc_unlock(client);
			g_propagate_error(error, tmp_error);
			return 0;
		}
		free(out_buffer);
		out_buffer = NULL;

		current_count += bytes_loc;
		afc_receive_data(client, &acknowledgement, &bytes_loc, &tmp_error);
		if (tmp_error != NULL) {
			afc_unlock(client);
			g_propagate_error(error, tmp_error);
			return 0;
		} else {
			free(acknowledgement);
		}
	}

	/* By this point, we should be at the end. i.e. the last segment that didn't
	   get sent in the for loop. This length is fine because it's always
	   sizeof(AFCPacket) + 8, but to be sure we do it again */
	if (current_count == length) {
		afc_unlock(client);
		return current_count;
	}

	client->afc_packet->this_length = sizeof(AFCPacket) + 8;
	client->afc_packet->entire_length = client->afc_packet->this_length + (length - current_count);
	client->afc_packet->operation = AFC_OP_WRITE;
	out_buffer = (char *) malloc(sizeof(char) * client->afc_packet->entire_length - sizeof(AFCPacket));
	memcpy(out_buffer, (char *) &handle, sizeof(uint64_t));
	memcpy(out_buffer + 8, data + current_count, (length - current_count));
	bytes_loc = afc_dispatch_packet(client, out_buffer, (length - current_count) + 8, &tmp_error);
	free(out_buffer);
	out_buffer = NULL;

	current_count += bytes_loc;

	if (tmp_error != NULL) {
		afc_unlock(client);
		g_clear_error(error); // TODO: really clear this?
		return current_count;
	}

	afc_receive_data(client, &acknowledgement, &bytes_loc, &tmp_error);
	afc_unlock(client);
	if (tmp_error != NULL) {
		debug_info("uh oh?");
		g_propagate_error(error, tmp_error);
	} else {
		free(acknowledgement);
	}
	return current_count;
}

/**
 * Closes a file on the phone.
 * 
 * @param client The client to close the file with.
 * @param handle File handle of a previously opened file.
 */
void afc_file_close(afc_client_t client, uint64_t handle, GError **error)
{
	g_assert(client != NULL && handle > 0);
	char *buffer = malloc(sizeof(char) * 8);
	uint32_t bytes = 0;
	GError *tmp_error = NULL;

	afc_lock(client);

	debug_info("File handle %i", handle);

	/* Send command */
	memcpy(buffer, &handle, sizeof(uint64_t));
	client->afc_packet->operation = AFC_OP_FILE_CLOSE;
	client->afc_packet->entire_length = client->afc_packet->this_length = 0;
	bytes = afc_dispatch_packet(client, buffer, 8, &tmp_error);
	free(buffer);
	buffer = NULL;

	if (tmp_error != NULL) {
		afc_unlock(client);
		g_propagate_error(error, tmp_error);
		return;
	}

	/* Receive the response */
	afc_receive_data(client, &buffer, &bytes, error);
	if (buffer)
		free(buffer);

	afc_unlock(client);
}

/**
 * Locks or unlocks a file on the phone. 
 *
 * makes use of flock on the device, see
 * http://developer.apple.com/documentation/Darwin/Reference/ManPages/man2/flock.2.html
 *
 * @param client The client to lock the file with.
 * @param handle File handle of a previously opened file.
 * @param operation the lock or unlock operation to perform, this is one of
 *        AFC_LOCK_SH (shared lock), AFC_LOCK_EX (exclusive lock),
 *        or AFC_LOCK_UN (unlock).
 */
void afc_file_lock(afc_client_t client, uint64_t handle, afc_lock_op_t operation, GError **error)
{
	g_assert(client != NULL && handle > 0);

	char *buffer = malloc(16);
	uint32_t bytes = 0;
	uint64_t op = GUINT64_TO_LE(operation);
	GError *tmp_error = NULL;

	afc_lock(client);

	debug_info("file handle %i", handle);

	/* Send command */
	memcpy(buffer, &handle, sizeof(uint64_t));
	memcpy(buffer + 8, &op, 8);

	client->afc_packet->operation = AFC_OP_FILE_LOCK;
	client->afc_packet->entire_length = client->afc_packet->this_length = 0;
	bytes = afc_dispatch_packet(client, buffer, 16, &tmp_error);
	free(buffer);
	buffer = NULL;

	if (tmp_error != NULL) {
		afc_unlock(client);
		debug_info("could not send lock command");
		g_propagate_error(error, tmp_error);
		return;
	}
	/* Receive the response */
	afc_receive_data(client, &buffer, &bytes, error);
	if (buffer) {
		debug_buffer(buffer, bytes);
		free(buffer);
	}
	afc_unlock(client);
}

/**
 * Seeks to a given position of a pre-opened file on the phone. 
 * 
 * @param client The client to use to seek to the position.
 * @param handle File handle of a previously opened.
 * @param offset Seek offset.
 * @param whence Seeking direction, one of SEEK_SET, SEEK_CUR, or SEEK_END.
 * 
 * @return AFC_E_SUCCESS on success or an AFC_E_* error value.
 */
void afc_file_seek(afc_client_t client, uint64_t handle, int64_t offset, int whence, GError **error)
{
	g_assert(client != NULL && handle > 0);

	char *buffer = (char *) malloc(sizeof(char) * 24);
	int64_t offset_loc = (int64_t)GUINT64_TO_LE(offset);
	uint64_t whence_loc = GUINT64_TO_LE(whence);
	uint32_t bytes = 0;
	GError *tmp_error = NULL;

	afc_lock(client);

	/* Send the command */
	memcpy(buffer, &handle, sizeof(uint64_t));	/* handle */
	memcpy(buffer + 8, &whence_loc, sizeof(uint64_t));	/* fromwhere */
	memcpy(buffer + 16, &offset_loc, sizeof(uint64_t));	/* offset */
	client->afc_packet->operation = AFC_OP_FILE_SEEK;
	client->afc_packet->this_length = client->afc_packet->entire_length = 0;
	bytes = afc_dispatch_packet(client, buffer, 24, &tmp_error);
	free(buffer);
	buffer = NULL;

	if (tmp_error != NULL) {
		afc_unlock(client);
		g_propagate_error(error, tmp_error);
		return;
	}
	/* Receive response */
	afc_receive_data(client, &buffer, &bytes, error);
	if (buffer)
		free(buffer);

	afc_unlock(client);
}

/**
 * Returns current position in a pre-opened file on the phone.
 * 
 * @param client The client to use.
 * @param handle File handle of a previously opened file.
 * @param position Position in bytes of indicator
 * 
 * @return AFC_E_SUCCESS on success or an AFC_E_* error value.
 */
uint64_t afc_file_tell(afc_client_t client, uint64_t handle, GError **error)
{
	g_assert(client != NULL && handle > 0);

	char *buffer = (char *) malloc(sizeof(char) * 8);
	uint32_t bytes = 0;
	uint64_t position = 0;
	GError *tmp_error = NULL;

	afc_lock(client);

	/* Send the command */
	memcpy(buffer, &handle, sizeof(uint64_t));	/* handle */
	client->afc_packet->operation = AFC_OP_FILE_TELL;
	client->afc_packet->this_length = client->afc_packet->entire_length = 0;
	bytes = afc_dispatch_packet(client, buffer, 8, &tmp_error);
	free(buffer);
	buffer = NULL;

	if (tmp_error != NULL) {
		afc_unlock(client);
		g_propagate_error(error, tmp_error);
		return 0;
	}

	/* Receive the data */
	afc_receive_data(client, &buffer, &bytes, error);
	if (bytes > 0 && buffer) {
		/* Get the position */
		memcpy(&position, buffer, sizeof(uint64_t));
		position = GUINT64_FROM_LE(position);
	}
	if (buffer)
		free(buffer);

	afc_unlock(client);

	return position;
}

/**
 * Sets the size of a file on the phone.
 * 
 * @param client The client to use to set the file size.
 * @param handle File handle of a previously opened file.
 * @param newsize The size to set the file to. 
 * 
 * @return AFC_E_SUCCESS on success or an AFC_E_* error value.
 * 
 * @note This function is more akin to ftruncate than truncate, and truncate
 *       calls would have to open the file before calling this, sadly.
 */
void afc_file_truncate(afc_client_t client, uint64_t handle, uint64_t newsize, GError **error)
{
	g_assert(client != NULL && handle > 0);

	char *buffer = (char *) malloc(sizeof(char) * 16);
	uint32_t bytes = 0;
	uint64_t newsize_loc = GUINT64_TO_LE(newsize);
	GError *tmp_error = NULL;

	afc_lock(client);

	/* Send command */
	memcpy(buffer, &handle, sizeof(uint64_t));	/* handle */
	memcpy(buffer + 8, &newsize_loc, sizeof(uint64_t));	/* newsize */
	client->afc_packet->operation = AFC_OP_FILE_SET_SIZE;
	client->afc_packet->this_length = client->afc_packet->entire_length = 0;
	bytes = afc_dispatch_packet(client, buffer, 16, &tmp_error);
	free(buffer);
	buffer = NULL;

	if (tmp_error != NULL) {
		afc_unlock(client);
		g_propagate_error(error, tmp_error);
		return;
	}
	/* Receive response */
	afc_receive_data(client, &buffer, &bytes, error);
	if (buffer)
		free(buffer);

	afc_unlock(client);
}

/**
 * Sets the size of a file on the phone without prior opening it.
 * 
 * @param client The client to use to set the file size.
 * @param path The path of the file to be truncated.
 * @param newsize The size to set the file to. 
 * 
 * @return AFC_E_SUCCESS on success or an AFC_E_* error value.
 */
void afc_truncate(afc_client_t client, const char *path, uint64_t newsize, GError **error)
{
	g_assert(client != NULL && path != NULL && client->afc_packet != NULL && client->connection != NULL);

	char *response = NULL;
	char *send = (char *) malloc(sizeof(char) * (strlen(path) + 1 + 8));
	uint32_t bytes = 0;
	uint64_t size_requested = GUINT64_TO_LE(newsize);
	GError *tmp_error = NULL;

	afc_lock(client);

	/* Send command */
	memcpy(send, &size_requested, 8);
	memcpy(send + 8, path, strlen(path) + 1);
	client->afc_packet->entire_length = client->afc_packet->this_length = 0;
	client->afc_packet->operation = AFC_OP_TRUNCATE;
	bytes = afc_dispatch_packet(client, send, 8 + strlen(path) + 1, &tmp_error);
	free(send);
	if (tmp_error != NULL) {
		afc_unlock(client);
		g_propagate_error(error, tmp_error);
		return;
	}
	/* Receive response */
	afc_receive_data(client, &response, &bytes, error);
	if (response)
		free(response);

	afc_unlock(client);
}

/**
 * Creates a hard link or symbolic link on the device. 
 * 
 * @param client The client to use for making a link
 * @param linktype 1 = hard link, 2 = symlink
 * @param target The file to be linked.
 * @param linkname The name of link.
 * 
 * @return AFC_E_SUCCESS on success or an AFC_E_* error value.
 */
void afc_make_link(afc_client_t client, afc_link_type_t linktype, const char *target, const char *linkname, GError **error)
{
	g_assert(client != NULL && target != NULL && linkname != NULL);
	g_assert(client->afc_packet != NULL && client->connection != NULL);

	char *response = NULL;
	char *send = (char *) malloc(sizeof(char) * (strlen(target)+1 + strlen(linkname)+1 + 8));
	uint32_t bytes = 0;
	uint64_t type = GUINT64_TO_LE(linktype);
	GError *tmp_error = NULL;

	afc_lock(client);

	debug_info("link type: %lld", type);
	debug_info("target: %s, length:%d", target, strlen(target));
	debug_info("linkname: %s, length:%d", linkname, strlen(linkname));

	/* Send command */
	memcpy(send, &type, 8);
	memcpy(send + 8, target, strlen(target) + 1);
	memcpy(send + 8 + strlen(target) + 1, linkname, strlen(linkname) + 1);
	client->afc_packet->entire_length = client->afc_packet->this_length = 0;
	client->afc_packet->operation = AFC_OP_MAKE_LINK;
	bytes = afc_dispatch_packet(client, send, 8 + strlen(linkname) + 1 + strlen(target) + 1, &tmp_error);
	free(send);
	if (tmp_error != NULL) {
		afc_unlock(client);
		g_propagate_error(error, tmp_error);
		return;
	}
	/* Receive response */
	afc_receive_data(client, &response, &bytes, error);
	if (response)
		free(response);

	afc_unlock(client);
}

/**
 * Sets the modification time of a file on the phone.
 * 
 * @param client The client to use to set the file size.
 * @param path Path of the file for which the modification time should be set.
 * @param mtime The modification time to set in nanoseconds since epoch.
 * 
 * @return AFC_E_SUCCESS on success or an AFC_E_* error value.
 */
void afc_set_file_time(afc_client_t client, const char *path, uint64_t mtime, GError **error)
{
	g_assert(client != NULL && path != NULL && client->afc_packet != NULL && client->connection != NULL);

	char *response = NULL;
	char *send = (char *) malloc(sizeof(char) * (strlen(path) + 1 + 8));
	uint32_t bytes = 0;
	uint64_t mtime_loc = GUINT64_TO_LE(mtime);
	GError *tmp_error = NULL;

	afc_lock(client);

	/* Send command */
	memcpy(send, &mtime_loc, 8);
	memcpy(send + 8, path, strlen(path) + 1);
	client->afc_packet->entire_length = client->afc_packet->this_length = 0;
	client->afc_packet->operation = AFC_OP_SET_FILE_TIME;
	bytes = afc_dispatch_packet(client, send, 8 + strlen(path) + 1, &tmp_error);
	free(send);
	if (tmp_error != NULL) {
		afc_unlock(client);
		g_propagate_error(error, tmp_error);
		return;
	}
	/* Receive response */
	afc_receive_data(client, &response, &bytes, error);
	if (response)
		free(response);

	afc_unlock(client);
}

