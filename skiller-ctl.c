/*
 * skiller-ctl
 * -----------
 * 
 *   Control the additional features (e.g., LEDs) of Sharkoon Skiller keyboards
 *
 *   Copyright (C) 2016 Mario Kicherer (dev@kicherer.org)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libusb.h>
#include <errno.h>

#ifdef DEBUG
#define DBG(x, ...) printf(x, __VA_ARGS__)
#else
#define DBG(x, ...)
#endif

struct kbd_type {
	char *name; // description
	uint16_t vendor_id; // USB vendor ID
	uint16_t product_id; // USB product ID
	
	unsigned int features; // (unused)
	unsigned int *polling_rates; // supported polling rates
	char **colors; // supported colors
	
	// codes for the different commands
	unsigned char prefix;
	unsigned char change_profile;
	unsigned char change_settings;
	unsigned char change_led;
	unsigned char change_polling_rate;
	unsigned char windows_key_off;
};

struct kbd_type kbd_types[] = {
	{
		.name = "Skiller Pro Plus",
		.vendor_id = 0x04d9,
		.product_id = 0xa096,
		
		.features = 0,
		.polling_rates = (unsigned int []){ 125, 250, 500, 1000 },
		.colors = (char*[]){"red", "green", "darkblue", "purple", "turquois", "yellow", "lightblue", 0},
		
		.prefix = 0x07,
		.change_profile = 0x02,
		.change_settings = 0x0b,
		.change_led = 0x0a,
		.change_polling_rate = 0x01,
		
		.windows_key_off = 0x01,
	},
	{0},
};

void show_help(char **argv) {
	fprintf(stderr, "Usage: %s [options]\n", argv[0]);
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "--------\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  -a                always power on the device (root privileges required)\n");
	fprintf(stderr, "  -b <number>       brightness between 0 and 10 (default: 10)\n");
	fprintf(stderr, "  -B                pulsing brightness\n");
	fprintf(stderr, "  -c <string>       set color (query possible values with -C)\n");
	fprintf(stderr, "  -C                list supported colors\n");
	fprintf(stderr, "  -d <number>       select device (default: 0)\n");
	fprintf(stderr, "  -i                disco mode - pulsing brightness with changing color\n");
	fprintf(stderr, "  -l                list available devices\n");
	fprintf(stderr, "  -p <number>       change profile (1-3)\n");
	fprintf(stderr, "  -r <number>       change polling rate to: 125, 250, 500 or 1000 Hz\n");
	fprintf(stderr, "  -w <number>       windows key on (1) or off (0)\n");
	fprintf(stderr, "\n");
}

// look for and print supported devices and return the libusb handle and the kbd_type of the chosen device
static void browse_devices(libusb_device **devices, uint8_t device, libusb_device_handle **dev_handle, 
						struct kbd_type **kbd_type, uint8_t list_devices, uint8_t always_power_on) {
	struct libusb_device_descriptor desc;
	struct kbd_type *p;
	int r, i, c;
	uint8_t path[8];
	char power_file[128];
	
	char supported;
	
	*dev_handle = 0;
	
	c = 0;
	i = 0;
	while (devices[c]) {
		r = libusb_get_device_descriptor(devices[c], &desc);
		if (r < 0) {
			fprintf(stderr, "failed to get device descriptor: %s", libusb_strerror(r));
			break;
		}
		
		supported = 0;
		for (p=kbd_types;p->name != 0; p++) {
			if (p->vendor_id == desc.idVendor &&
				p->product_id == desc.idProduct)
			{
				supported = 1;
				break;
			}
		}
		
		if (supported) {
			if (list_devices) {
				printf("%04x:%04x Bus %03d Device %03d \"%s\"", desc.idVendor, desc.idProduct, 
					libusb_get_bus_number(devices[c]), libusb_get_device_address(devices[c]),
					p->name);
			}
			
			r = libusb_get_port_numbers(devices[c], path, sizeof(path));
			if (r > 0) {
				int j, count;
				
				count = sprintf(power_file, "/sys/bus/usb/devices/%d-%d", libusb_get_bus_number(devices[c]), path[0]);
				
				for (j = 1; j < r; j++)
					count += sprintf(&power_file[count], ".%d", path[j]);
				
				if (list_devices)
					printf("\tpath: %s/", power_file);
				
				sprintf(&power_file[count], "/power/level");
			}
			
			if (list_devices)
				printf("\n");
		}
		
		if (supported) {
			if (i == device) {
				FILE *f;
				
				*kbd_type = p;
				
				if (always_power_on) {
					f = fopen(power_file, "w");
					if (f) {
						DBG("set device #%u to always on\n", device);
						fwrite("on\n", 3, 1, f);
						fclose(f);
					} else {
						fprintf(stderr, "opening %s failed: %s\n", power_file, strerror(errno));
					}
				}
				
				r = libusb_open(devices[c], dev_handle);
				if (r < 0) {
					printf("error opening device: %s\n", libusb_strerror(r));
				}
			}
			i++;
		}
		
		c++;
	}
}

int main(int argc, char **argv) {
	libusb_device_handle *dev_handle;
	libusb_context *ctx = NULL;
	libusb_device **devices;
	int bytes_written, opt, r;
	uint8_t cmd[8];
	uint8_t device;
	struct kbd_type *kbd_type;
	
	char *color;
	unsigned char brightness;
	unsigned char profile_idx;
	unsigned char windows_key_state;
	unsigned int polling_rate;
	char change_led;
	char change_profile;
	char change_setting;
	char change_polling;
	char list_devices;
	char list_colors;
	char always_power_on;
	
	always_power_on = 0;
	device = 0;
	list_devices = 0;
	list_colors = 0;
	profile_idx = 1;
	brightness = 10;
	change_led = 0;
	change_profile = 0;
	change_setting = 0;
	change_polling = 0;
	
	while ((opt = getopt(argc, argv, "ab:Bc:Cd:hilp:r:w:")) != -1) {
		switch (opt) {
		case 'a':
			always_power_on = 1;
			break;
		case 'b':
			if (sscanf(optarg, "%hhu", &brightness) != 1) {
				fprintf(stderr, "error, cannot parse \"%s\" for brightness\n", optarg);
				show_help(argv);
				exit(1);
			}
			if (brightness < 0 || brightness > 10) {
				fprintf(stderr, "error, choose a brightness between 0 and 10\n");
				show_help(argv);
				exit(1);
			}
			change_led = 1;
			break;
		case 'B':
			brightness = 0x0b;
			change_led = 1;
			break;
		case 'c':
			color = strdup(optarg);
			change_led = 1;
			break;
		case 'C':
			list_colors = 1;
			break;
		case 'd':
			if (sscanf(optarg, "%hhu", &device) != 1) {
				fprintf(stderr, "error, cannot parse \"%s\" as device number\n", optarg);
				show_help(argv);
				exit(1);
			}
			if (device < 0) {
				fprintf(stderr, "error, device number must be equal or greater than 0\n");
				show_help(argv);
				exit(1);
			}
			break;
		case 'h':
			show_help(argv);
			exit(0);
		case 'i':
			brightness = 0x0c;
			change_led = 1;
			break;
		case 'l':
			list_devices = 1;
			break;
		case 'p':
			if (sscanf(optarg, "%hhu", &profile_idx) != 1) {
				fprintf(stderr, "error, cannot parse \"%s\" for profile selection\n", optarg);
				show_help(argv);
				exit(1);
			}
			if (profile_idx < 1 || profile_idx > 3) {
				fprintf(stderr, "error, choose a profile between 1 and 3\n");
				show_help(argv);
				exit(1);
			}
			change_profile = 1;
			break;
		case 'r':
			{
			unsigned int *uint_it;
			
			if (sscanf(optarg, "%u", &polling_rate) != 1) {
				fprintf(stderr, "error, cannot parse \"%s\" for the polling rate\n", optarg);
				show_help(argv);
				exit(1);
			}
			uint_it = kbd_type->polling_rates;
			while (*uint_it) {
				if (*uint_it == polling_rate)
					break;
				uint_it++;
			}
			if (!uint_it || *uint_it != polling_rate) {
				fprintf(stderr, "error, unsupported value for the polling rate\n");
				show_help(argv);
				exit(1);
			}
			change_polling = 1;
			break;
			}
		case 'w':
			if (sscanf(optarg, "%hhu", &windows_key_state) != 1) {
				fprintf(stderr, "error, cannot parse \"%s\" for windows key state\n", optarg);
				show_help(argv);
				exit(1);
			}
			if (windows_key_state != 0 && windows_key_state != 1) {
				fprintf(stderr, "error, value for -w must bei either 0 or 1\n");
				show_help(argv);
				exit(1);
			}
			change_setting = 1;
			break;
		default:
			show_help(argv);
			exit(1);
		}
	}
	
	if (!change_led && !change_profile && !change_setting && !change_polling &&
		!list_devices && !list_colors && !always_power_on)
	{
		show_help(argv);
		exit(0);
	}
	
	r = libusb_init(&ctx);
	if(r < 0) {
		fprintf(stderr, "error initializing libusb: %s\n", libusb_strerror(r));
		return 1;
	}
	
	#ifdef DEBUG
	libusb_set_debug(ctx, 3);
	#endif
	
	r = libusb_get_device_list(NULL, &devices);
	if (r < 0) {
		fprintf(stderr, "error retrieving list of devices: %s\n", libusb_strerror(r));
		return 1;
	}
	
	/* look for and print supported devices and return the libusb handle and
	 * the kbd_type of the chosen device
	 */
	browse_devices(devices, device, &dev_handle, &kbd_type, list_devices, always_power_on);
	
	if (!dev_handle) {
		fprintf(stderr, "error, requested device not found\n");
		return 1;
	}
	
	if (list_colors) {
		char **c;
		
		c = kbd_type->colors;
		while (*c) {
			printf("%s ", *c);
			c++;
		}
		printf("\n");
	}
	
	if (change_profile || change_setting || change_led || change_polling) {
		libusb_set_auto_detach_kernel_driver(dev_handle, 1);
		
		libusb_claim_interface(dev_handle, 1);
		
		if (change_profile) {
			memset(cmd, 0, sizeof(cmd));
			cmd[0] = kbd_type->prefix;
			cmd[1] = kbd_type->change_profile;
			cmd[2] = profile_idx;
			
			DBG("[p%u] change profile\n", profile_idx);
			bytes_written = libusb_control_transfer(dev_handle, 0x21, 0x9, 0x0307, 0x1, cmd, 8, 0);
			if (bytes_written != 8) {
				fprintf(stderr, "error sending control URB (%d): %s\n", bytes_written, libusb_strerror(r));
			}
			usleep(100000);
		}
		
		if (change_setting) {
			memset(cmd, 0, sizeof(cmd));
			cmd[0] = kbd_type->prefix;
			cmd[1] = kbd_type->change_settings;
			cmd[2] = profile_idx;
			if (windows_key_state == 0)
				cmd[3] = kbd_type->windows_key_off;
			
			DBG("[p%u] change windows key to %u\n", profile_idx, windows_key_state);
			bytes_written = libusb_control_transfer(dev_handle, 0x21, 0x9, 0x0307, 0x1, cmd, 8, 0);
			if (bytes_written != 8) {
				fprintf(stderr, "error sending control URB (%d): %s\n", bytes_written, libusb_strerror(r));
			}
			usleep(100000);
		}
		
		if (change_led) {
			char *c;
			unsigned char color_idx;
			
			color_idx = 0;
			c = kbd_type->colors[color_idx];
			while (c) {
				if (!strcmp(c, color)) {
					break;
				}
				color_idx++;
				c = kbd_type->colors[color_idx];
			}
			if (!c) {
				fprintf(stderr, "error, color \"%s\" not available\n", color);
				exit(1);
			}
			
			memset(cmd, 0, sizeof(cmd));
			cmd[0] = kbd_type->prefix;
			cmd[1] = kbd_type->change_led;
			cmd[2] = profile_idx;
			cmd[3] = brightness;
			cmd[4] = 0x04;
			cmd[6] = color_idx;
			
			DBG("[p%u] brightness %u, color %u\n", profile_idx, brightness, color_idx);
			bytes_written = libusb_control_transfer(dev_handle, 0x21, 0x9, 0x0307, 0x1, cmd, 8, 0);
			if (bytes_written != 8) {
				fprintf(stderr, "error sending control URB (%d): %s\n", bytes_written, libusb_strerror(r));
			}
			usleep(100000);
		}
		
		if (change_polling) {
			memset(cmd, 0, sizeof(cmd));
			cmd[0] = kbd_type->prefix;
			cmd[1] = kbd_type->change_polling_rate;
			cmd[2] = 1000/polling_rate;
			
			DBG("[p%u] change polling rate to %u\n", profile_idx, 1000/polling_rate);
			bytes_written = libusb_control_transfer(dev_handle, 0x21, 0x9, 0x0307, 0x1, cmd, 8, 0);
			if (bytes_written != 8) {
				fprintf(stderr, "error sending control URB (%d): %s\n", bytes_written, libusb_strerror(r));
			}
			usleep(100000);
		}
		
		r = libusb_release_interface(dev_handle, 1);
		if (r < 0) {
			printf("error while releasing interface: %s\n", libusb_strerror(r));
		}
	}
	
	libusb_close(dev_handle);
	
	libusb_free_device_list(devices, 1);
	
	libusb_exit(ctx);
	
	return 0;
}
