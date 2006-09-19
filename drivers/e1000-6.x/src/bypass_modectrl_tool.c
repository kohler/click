/*******************************************************************************

  
  Copyright(c) 1999 - 2005 Intel Corporation. All rights reserved.
  
  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
  
  Contact Information:
  Linux NICS <linux.nics@intel.com>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

/*
 * Sample test app to excersize the Intel PRO Bypass Mode Control interface
 */

#include <stdio.h>
#include <string.h>
#include <linux/unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/types.h>

typedef __uint32_t uint32_t;        /* hack, so we can include driver files */
typedef __uint16_t uint16_t;        /* ditto */
typedef __uint8_t uint8_t;          /* ditto */
#include "e1000_bypass_ctrl.h"

extern char *optarg;
extern int optopt, optind, opterr;

#define SYSCALL(X,S) { if((X) < 0) { perror(S); return -1; } }
#define FILEPART(S)  ( rindex((S), '/') == NULL ? (S) : rindex((S), '/') + 1 )

static int
e1000_bypass_mode(int sock1, int sock2, struct ifreq *interface1, 
		struct ifreq *interface2, unsigned int sdpBits)
{
	struct ibypass_ctrl_data *bypass_ctrl_data1, *bypass_ctrl_data2;

	bypass_ctrl_data1 = (struct ibypass_ctrl_data *)interface1->ifr_data;
	bypass_ctrl_data1->cmd = IDRIVE_BYPASS_CTRL_SIG;
	bypass_ctrl_data1->bypass_param[0] = sdpBits & 0x3;
	SYSCALL(ioctl(sock1, BYPASS_MODE_CTRL_SIOC, interface1), interface1->ifr_name);

	bypass_ctrl_data2 = (struct ibypass_ctrl_data *)interface2->ifr_data;
	bypass_ctrl_data2->cmd = IDRIVE_BYPASS_CTRL_SIG;
	bypass_ctrl_data2->bypass_param[0] = ((sdpBits & 0xc) >> 2);
	SYSCALL(ioctl(sock2, BYPASS_MODE_CTRL_SIOC, interface2), interface2->ifr_name);

	switch (bypass_ctrl_data1->status) {
	case BYPASS_MODE_STAT_OK:
		printf("%s %s: Bypass Mode is passed\n", 
				interface1->ifr_name, interface2->ifr_name);
		return 0;
	case BYPASS_MODE_STAT_INVALID_STATE:
		printf("%s %s: invalid Bypass Mode\n", 
				interface1->ifr_name, interface2->ifr_name);
		return -1;
	case BYPASS_MODE_STAT_FAILED:
		printf("%s %s: Bypass Mode FAILED (NO link).\n", 
				interface1->ifr_name, interface2->ifr_name);
		return -1;
	default:
		printf("%s %s: Bypass Mode FAILED.\n", 
				interface1->ifr_name, interface2->ifr_name);
		printf("%s %s:   Reason unknown.\n", 
				interface1->ifr_name, interface2->ifr_name);
		return -1;
	}
	return 0;
}

static void
usage(char *name)
{
	fprintf(stderr, "\nUsage: %s [OPTION]\n\n", name);
	fprintf(stderr, "  -1 <interface> -2 <interface>: \
			interface (eth0, eth1, etc)\n\n");
	fprintf(stderr, "  -d <sdpBits>: sdp ModeCtrl Bits: (0<0000>, 1<0001>, etc)\n");
	fprintf(stderr, "  -s : To check the status of the bypass mode\n");
	fprintf(stderr, "  -m : ???\n");
}

int
main(int argc, char **argv)
{
	struct ifreq interface1, interface2;
	struct ibypass_ctrl_data bypass_ctrl_data1, bypass_ctrl_data2;
	unsigned char *iface1 = NULL, *iface2 = NULL;
	int retval = 0, ch, sock1, sock2;
	unsigned int sdpBits;
	unsigned char command_line_parsed = 0;
	enum {
		DRIVE_SDP        = 0x1,
		BYPASS_STATUS    = 0x2,
	};
	unsigned int tests = 0;

	/* Make sure at least two interface name were given on the 
	 * command line */
	if(argc < 3) {
		usage(FILEPART(argv[0]));
		return -1;
	}

	while((ch = getopt(argc, argv, ":1:2:d:s:m")) != -1) {
		switch (ch) {
		case '1':
			iface1 = optarg;
			break;
		case '2':
			iface2 = optarg;
			break;
		case 'd':
			tests |= DRIVE_SDP;
			sdpBits = strtol(optarg, NULL, 0);
			break;
		case 's':
			tests |= BYPASS_STATUS;
			sdpBits = strtol(optarg, NULL, 0);
			break;
		case 'm':
		case ':':
		case '?':
			usage(FILEPART(argv[0]));
			return -1;

		}
		command_line_parsed = 1;
	}

	if(command_line_parsed == 0) {
		usage(FILEPART(argv[0]));
		return -1;
	}

	if((iface1 == NULL) || (iface2 == NULL)) {
		usage(FILEPART(argv[0]));
		return -1;
	}

	/* Test for existance of the network interface-1 */
	strcpy(interface1.ifr_name, iface1);
	SYSCALL((sock1 = socket(AF_INET, SOCK_DGRAM, 0)), "Failed to create socket");
	SYSCALL(ioctl(sock1, SIOCGIFINDEX, &interface1), interface1.ifr_name);

	/* Test for the Bypass interface1, and get the driver ID */
	bypass_ctrl_data1.interface_ver = IBYPASS_MODECTRL_VERSION;
	bypass_ctrl_data1.driver_id = IBYPASS_E1000_DRIVER;
	bypass_ctrl_data1.cmd = IBYPASS_MODE_IDENTIFY_DRIVER;
	interface1.ifr_data = (char *)&bypass_ctrl_data1;
	SYSCALL(ioctl(sock1, BYPASS_MODE_CTRL_SIOC, &interface1),
		interface1.ifr_name);

	if(bypass_ctrl_data1.driver_id != IBYPASS_E1000_DRIVER) {
		fprintf(stderr, "Driver ID 0x%x not supported by %s\n",
			IBYPASS_E1000_DRIVER, interface1.ifr_name);
		return -1;
	}

	if(bypass_ctrl_data1.status != BYPASS_MODE_STAT_OK) {
		fprintf(stderr, "Inf1 ver 0x%x not supported by %s\n",
			IBYPASS_MODECTRL_VERSION, interface1.ifr_name);
		return -1;
	};

	/* Test for existance of the network interface-2 */
	strcpy(interface2.ifr_name, iface2);
	SYSCALL((sock2 = socket(AF_INET, SOCK_DGRAM, 0)), "Failed to create socket");
	SYSCALL(ioctl(sock2, SIOCGIFINDEX, &interface2), interface2.ifr_name);

	/* Test for the iDIAG interface2, and get the driver ID */
	bypass_ctrl_data2.interface_ver = IBYPASS_MODECTRL_VERSION;
	bypass_ctrl_data2.driver_id = IBYPASS_E1000_DRIVER;
	bypass_ctrl_data2.cmd = IBYPASS_MODE_IDENTIFY_DRIVER;
	interface2.ifr_data = (char *)&bypass_ctrl_data2;
	SYSCALL(ioctl(sock2, BYPASS_MODE_CTRL_SIOC, &interface2),
		interface2.ifr_name);

	if(bypass_ctrl_data2.driver_id != IBYPASS_E1000_DRIVER) {
		fprintf(stderr, "Driver ID 0x%x not supported by %s\n",
			IBYPASS_E1000_DRIVER, interface2.ifr_name);
		return -1;
	}

	if(bypass_ctrl_data2.status != BYPASS_MODE_STAT_OK) {
		fprintf(stderr, "Inf-2 ver 0x%x not supported by %s\n",
			IBYPASS_MODECTRL_VERSION, interface2.ifr_name);
		return -1;
	};

	/* Run the Bypass mode Control signals on SDP pins */
	if(tests & DRIVE_SDP) 
		retval |= e1000_bypass_mode(sock1, sock2, &interface1, &interface2, sdpBits);
	else if(tests & BYPASS_STATUS) {
		/* Check the Status of the Bypass mode */
		bypass_ctrl_data2.interface_ver = IBYPASS_MODECTRL_VERSION;
		bypass_ctrl_data2.driver_id = IBYPASS_E1000_DRIVER;
		bypass_ctrl_data2.cmd = ICHECK_BYPASS_CTRL_STATUS;
		interface2.ifr_data = (char *)&bypass_ctrl_data2;
		SYSCALL(ioctl(sock2, BYPASS_MODE_CTRL_SIOC, &interface2), interface2.ifr_name);
		if ((bypass_ctrl_data2.status) == BYPASS_MODE_STAT_FAILED ){
			bypass_ctrl_data1.interface_ver = IBYPASS_MODECTRL_VERSION;
			bypass_ctrl_data1.driver_id = IBYPASS_E1000_DRIVER;
			bypass_ctrl_data1.cmd = ICHECK_BYPASS_CTRL_STATUS;
			interface1.ifr_data = (char *)&bypass_ctrl_data1;
			SYSCALL(ioctl(sock1, BYPASS_MODE_CTRL_SIOC, &interface1), interface1.ifr_name);
		}
		retval == BYPASS_MODE_STAT_OK;
	}
	return retval;
}
