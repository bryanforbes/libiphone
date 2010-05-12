/*
 * msyncclient.c
 * Rudimentary interface to the MobileSync service.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <glib.h>

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/mobilesync.h>
#include "../src/mobilesync.h"

static char check_string(plist_t node, char* string)
{
	char ret = 1;
	char* msg = NULL;
	plist_type type = plist_get_node_type(node);
	if (PLIST_STRING == type) {
		plist_get_string_val(node, &msg);
	}
	if (PLIST_STRING != type || strcmp(msg, string)) {
		printf("%s: ERROR: MobileSync client did not find %s !\n", __func__, string);
		ret = 0;
	}
	free(msg);
	msg = NULL;
	return ret;
}

static mobilesync_error_t mobilesync_get_all_contacts(mobilesync_client_t client)
{
	if (!client)
		return MOBILESYNC_E_INVALID_ARG;

	mobilesync_error_t ret = MOBILESYNC_E_UNKNOWN_ERROR;
	GTimeVal current_time = { 0, 0 };
	mobilesync_sync_type_t sync_type;
	uint64_t data_class_version;

	g_get_current_time(&current_time);
	mobilesync_anchors anchors = {
		NULL,
		g_time_val_to_iso8601(&current_time)
	};

	ret = mobilesync_session_start(client, "com.apple.Calendars", &anchors, &sync_type, &data_class_version);

	if (ret != MOBILESYNC_E_SUCCESS) {
		goto out;
	}

	ret = mobilesync_get_all_records_from_device(client);
	if (ret != MOBILESYNC_E_SUCCESS) {
		goto out;
	}

	uint8_t is_last_record = 0;
	plist_t records = NULL;

	char *xml_out = NULL;
	uint32_t xml_out_len = 0;

	do {
		ret = mobilesync_receive_changes(client, &records, &is_last_record);
		if (ret != MOBILESYNC_E_SUCCESS) {
			goto out;
		}

		if (records) {
			plist_to_xml(records, &xml_out, &xml_out_len);
			if (xml_out_len > 0) {
				printf("%s\n", xml_out);
				free(xml_out);
			}
			xml_out = NULL;
			xml_out_len = 0;
		}

		plist_free(records);
		records = NULL;

		printf("%s\n", client->data_class);

		ret = mobilesync_acknowledge_changes_from_device(client);
		if (ret != MOBILESYNC_E_SUCCESS) {
			goto out;
		}
	} while(!is_last_record);

	mobilesync_session_finish(client);

	out:
	if (xml_out) {
		free(xml_out);
	}
	if (records) {
		plist_free(records);
	}

	return ret;
}

int main(int argc, char *argv[])
{
	uint16_t port = 0;
	lockdownd_client_t client = NULL;
	idevice_t phone = NULL;

	if (argc > 1 && !strcasecmp(argv[1], "--debug"))
		idevice_set_debug_level(1);

	if (IDEVICE_E_SUCCESS != idevice_new(&phone, NULL)) {
		printf("No device found, is it plugged in?\n");
		return -1;
	}

	if (LOCKDOWN_E_SUCCESS != lockdownd_client_new_with_handshake(phone, &client, "msyncclient")) {
		idevice_free(phone);
		return -1;
	}

	lockdownd_start_service(client, "com.apple.mobilesync", &port);

	if (port) {
		mobilesync_client_t msync = NULL;
		mobilesync_client_new(phone, port, &msync);
		if (msync) {
			mobilesync_get_all_contacts(msync);
			mobilesync_client_free(msync);
		}
	} else {
		printf("Start service failure.\n");
	}

	printf("All done.\n");

	lockdownd_client_free(client);
	idevice_free(phone);

	return 0;
}
