/**
 * idevicescreenshot -- Gets a screenshot from a connected iPhone/iPod Touch
 *
 * Copyright (C) 2010 Nikias Bassen <nikias@gmx.li>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more profile.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 
 * USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/screenshotr.h>

void print_usage(int argc, char **argv);

int main(int argc, char **argv)
{
	idevice_t device = NULL;
	lockdownd_client_t lckd = NULL;
	screenshotr_client_t shotr = NULL;
	uint16_t port = 0;
	int result = -1;
	int i;
	char *uuid = NULL;

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
			uuid = strdup(argv[i]);
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

	device = idevice_new(uuid, &error);
	if (error != NULL) {
		printf("No device found, is it plugged in?\n");
		if (uuid) {
			free(uuid);
		}
		g_error_free(error);
		return -1;
	}
	if (uuid) {
		free(uuid);
	}

	lckd = lockdownd_client_new_with_handshake(device, NULL, &error);
	if (error != NULL) {
		idevice_free(device);
		printf("Exiting.\n");
		g_error_free(error);
		return -1;
	}

	port = lockdownd_start_service(lckd, "com.apple.mobile.screenshotr", NULL);
	lockdownd_client_free(lckd, NULL);
	if (port > 0) {
		shotr = screenshotr_client_new(device, port, &error);
		if (error != NULL) {
			printf("Could not connect to screenshotr!\n");
			g_error_free(error);
		} else {
			char *imgdata = NULL;
			char filename[36];
			uint64_t imgsize = 0;
			time_t now = time(NULL);
			strftime(filename, 36, "screenshot-%Y-%m-%d-%H-%M-%S.tiff", gmtime(&now));
			screenshotr_take_screenshot(shotr, &imgdata, &imgsize, &error);
			if (error == NULL) {
				FILE *f = fopen(filename, "w");
				if (f) {
					if (fwrite(imgdata, 1, (size_t)imgsize, f) == (size_t)imgsize) {
						printf("Screenshot saved to %s\n", filename);
						result = 0;
					} else {
						printf("Could not save screenshot to file %s!\n", filename);
					}
					fclose(f);
				} else {
					printf("Could not open %s for writing: %s\n", filename, strerror(errno));
				}
			} else {
				printf("Could not get screenshot!\n");
			}
			screenshotr_client_free(shotr, NULL);
		}
	} else {
		printf("Could not start screenshotr service! Remember that you have to mount the Developer disk image on your device if you want to use the screenshotr service.\n");
	}
	idevice_free(device);
	
	return result;
}

void print_usage(int argc, char **argv)
{
        char *name = NULL;

        name = strrchr(argv[0], '/');
        printf("Usage: %s [OPTIONS]\n", (name ? name + 1: argv[0]));
        printf("Gets a screenshot from the connected iPhone/iPod Touch.\n");
        printf("The screenshot is saved as a TIFF image in the current directory.\n");
        printf("NOTE: A mounted developer disk image is required on the device, otherwise\n");
        printf("the screenshotr service is not available.\n\n");
        printf("  -d, --debug\t\tenable communication debugging\n");
        printf("  -u, --uuid UUID\ttarget specific device by its 40-digit device UUID\n");
        printf("  -h, --help\t\tprints usage information\n");
        printf("\n");
}
