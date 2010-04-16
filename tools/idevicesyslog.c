/*
 * idevicesyslog.c
 * Relay the syslog of a device to stdout
 *
 * Copyright (c) 2009 Martin Szulecki All Rights Reserved.
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
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <glib.h>

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

static int quit_flag = 0;

void print_usage(int argc, char **argv);

/**
 * signal handler function for cleaning up properly
 */
static void clean_exit(int sig)
{
	fprintf(stderr, "Exiting...\n");
	quit_flag++;
}

int main(int argc, char *argv[])
{
	lockdownd_client_t client = NULL;
	idevice_t phone = NULL;
	int i;
	char uuid[41];
	uint16_t port = 0;
	uuid[0] = 0;

	signal(SIGINT, clean_exit);
	signal(SIGQUIT, clean_exit);
	signal(SIGTERM, clean_exit);
	signal(SIGPIPE, SIG_IGN);

	/* parse cmdline args */
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
			idevice_set_debug_level(1);
			continue;
		}
		else if (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--uuid")) {
			i++;
			if (!argv[i] || (strlen(argv[i]) != 40)) {
				print_usage(argc, argv);
				return 0;
			}
			strcpy(uuid, argv[i]);
			continue;
		}
		else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			print_usage(argc, argv);
			return 0;
		}
		else {
			print_usage(argc, argv);
			return 0;
		}
	}

	GError *error = NULL;
	if (uuid[0] != 0) {
		phone = idevice_new(uuid, &error);
		if (error != NULL) {
			printf("No device found with uuid %s, is it plugged in?\n", uuid);
			g_error_free(error);
			return -1;
		}
	}
	else
	{
		phone = idevice_new(NULL, &error);
		if (error != NULL) {
			printf("No device found, is it plugged in?\n");
			g_error_free(error);
			return -1;
		}
	}

	client = lockdownd_client_new_with_handshake(phone, "idevicesyslog", &error);
	if (error != NULL) {
		idevice_free(phone);
		g_error_free(error);
		return -1;
	}

	/* start syslog_relay service and retrieve port */
	port = lockdownd_start_service(client, "com.apple.syslog_relay", &error);
	if ((error == NULL) && port) {
		lockdownd_client_free(client, NULL);
		
		/* connect to socket relay messages */
		idevice_connection_t conn = idevice_connect(phone, port, &error);
		if ((error != NULL) || !conn) {
			printf("ERROR: Could not open usbmux connection.\n");
			g_error_free(error);
		} else {
			while (!quit_flag) {
				char *receive = NULL;
				uint32_t datalen = 0, bytes = 0, recv_bytes = 0;

				idevice_connection_receive(conn, (char *) &datalen, sizeof(datalen), &bytes, NULL);
				datalen = GUINT32_FROM_BE(datalen);

				if (datalen == 0)
					continue;

				recv_bytes += bytes;
				receive = (char *) malloc(sizeof(char) * datalen);

				while (!quit_flag && (recv_bytes <= datalen)) {
					idevice_connection_receive(conn, receive, datalen, &bytes, NULL);

					if (bytes == 0)
						break;

					recv_bytes += bytes;

					fwrite(receive, sizeof(char), bytes, stdout);
				}

				free(receive);
			}
		}
		idevice_disconnect(conn, NULL);
	} else {
		printf("ERROR: Could not start service com.apple.syslog_relay.\n");
	}

	idevice_free(phone);

	return 0;
}

void print_usage(int argc, char **argv)
{
	char *name = NULL;
	
	name = strrchr(argv[0], '/');
	printf("Usage: %s [OPTIONS]\n", (name ? name + 1: argv[0]));
	printf("Relay syslog of a connected iPhone/iPod Touch.\n\n");
	printf("  -d, --debug\t\tenable communication debugging\n");
	printf("  -u, --uuid UUID\ttarget specific device by its 40-digit device UUID\n");
	printf("  -h, --help\t\tprints usage information\n");
	printf("\n");
}

