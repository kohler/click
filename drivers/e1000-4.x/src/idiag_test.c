/*******************************************************************************

  
  Copyright(c) 1999 - 2002 Intel Corporation. All rights reserved.
  
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
 * Sample test app to excersize the Intel PRO Diagnostics interface
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
#include "idiag_pro.h"
#include "idiag_e1000.h"

extern char *optarg;
extern int optopt, optind, opterr;

#define SYSCALL(X,S) { if((X) < 0) { perror(S); return -1; } }
#define FILEPART(S)  ( rindex((S), '/') == NULL ? (S) : rindex((S), '/') + 1 )

static int
e1000_test_register(int sock, struct ifreq *interface)
{
	struct idiag_pro_data *diag_data;
	struct idiag_e1000_diag_reg_test_param *param;

	diag_data = (struct idiag_pro_data *)interface->ifr_data;
	param = (struct idiag_e1000_diag_reg_test_param *)diag_data->diag_param;

	diag_data->cmd = IDIAG_E1000_DIAG_REG_TEST;
	SYSCALL(ioctl(sock, IDIAG_PRO_BASE_SIOC, interface),
		interface->ifr_name);

	switch (diag_data->status) {
	case IDIAG_PRO_STAT_OK:
		printf("%s: Register test passed.\n", interface->ifr_name);
		return 0;
	case IDIAG_PRO_STAT_INVALID_STATE:
		printf("%s: Register test invalid state.\n",
		       interface->ifr_name);
		return -1;
	case IDIAG_PRO_STAT_TEST_FAILED:
		printf("%s: Register test FAILED.\n", interface->ifr_name);
		printf("%s:   Wrote %lu, read %lu at register %u\n",
		       interface->ifr_name,
		       param->write_value, param->read_value, param->reg);
		return -1;
	default:
		printf("%s: Register test FAILED.\n", interface->ifr_name);
		printf("%s:   Reason unknown.\n", interface->ifr_name);
		return -1;
	}
	return 0;
}


static int
e1000_test_xsum(int sock, struct ifreq *interface)
{
	struct idiag_pro_data *diag_data;
	struct idiag_e1000_diag_eeprom_test_param *param;

	diag_data = (struct idiag_pro_data *)interface->ifr_data;
	param = (struct idiag_e1000_diag_eeprom_test_param *)
		diag_data->diag_param;

	diag_data->cmd = IDIAG_E1000_DIAG_XSUM_TEST;
	SYSCALL(ioctl(sock, IDIAG_PRO_BASE_SIOC, interface),
		interface->ifr_name);

	switch (diag_data->status) {
	case IDIAG_PRO_STAT_OK:
		printf("%s: EEPROM checksum test passed.\n",
		       interface->ifr_name);
		return 0;
	case IDIAG_PRO_STAT_INVALID_STATE:
		printf("%s: EEPROM checksum test invalid state.\n",
		       interface->ifr_name);
		return -1;
	case IDIAG_PRO_STAT_TEST_FAILED:
		printf("%s: EEPROM checksum test FAILED.\n",
		       interface->ifr_name);
		printf("%s:   Expected checksum %lu, actual checksum %lu\n",
		       interface->ifr_name,
		       param->expected_checksum, param->actual_checksum);
		return -1;
	default:
		printf("%s: EEPROM checksum test FAILED.\n", 
			interface->ifr_name);
		printf("%s:   Reason unknown.\n", interface->ifr_name);
		return -1;
	}
	return 0;
}

static int
e1000_test_intr(int sock, struct ifreq *interface)
{
	struct idiag_pro_data *diag_data;
	enum idiag_e1000_diag_intr_test_param *param;

	diag_data = (struct idiag_pro_data *)interface->ifr_data;
	param = (enum idiag_e1000_diag_intr_test_param *)diag_data->diag_param;

	diag_data->cmd = IDIAG_E1000_DIAG_INTR_TEST;
	SYSCALL(ioctl(sock, IDIAG_PRO_BASE_SIOC, interface),
		interface->ifr_name);

	switch (diag_data->status) {
	case IDIAG_PRO_STAT_OK:
		printf("%s: Interrupt test passed.\n", interface->ifr_name);
		return 0;
	case IDIAG_PRO_STAT_INVALID_STATE:
		printf("%s: Interrupt test invalid state.\n",
		       interface->ifr_name);
		return -1;
	case IDIAG_PRO_STAT_TEST_FAILED:
		printf("%s: Interrupt test FAILED.\n", interface->ifr_name);
		switch (*param) {
		case IDIAG_E1000_INTR_TEST_FAILED_WHILE_DISABLED:
			printf("%s:   Failed while interrupts disabled.\n",
				interface->ifr_name);
			break;
		case IDIAG_E1000_INTR_TEST_FAILED_WHILE_ENABLED:
			printf("%s:   Failed while interrupts enabled.\n",
				interface->ifr_name);
			break;
		case IDIAG_E1000_INTR_TEST_FAILED_MASKED_ENABLED:
			printf("%s:   Failed while interrupts masked.\n",
				interface->ifr_name);
		}
		return -1;
	default:
		printf("%s: Interrupt test FAILED.\n", 
			interface->ifr_name);
		printf("%s:   Reason unknown.\n", interface->ifr_name);
		return -1;
	}
	return 0;
}

static int
e1000_test_loopback(int sock, struct ifreq *interface)
{
	struct idiag_pro_data *diag_data;
	struct idiag_e1000_diag_loopback_test_param *param;
	int result = 0;

	diag_data = (struct idiag_pro_data *)interface->ifr_data;
	param = (struct idiag_e1000_diag_loopback_test_param *)
		diag_data->diag_param;

	diag_data->cmd = IDIAG_E1000_DIAG_LOOPBACK_TEST;
	param->mode = IDIAG_E1000_DIAG_PHY_TCVR_LB;
	SYSCALL(ioctl(sock, IDIAG_PRO_BASE_SIOC, interface),
		interface->ifr_name);

	switch (diag_data->status) {
	case IDIAG_PRO_STAT_OK:
		printf("%s: %s passed.\n", interface->ifr_name,
			"PHY/TCVR loopback test");
		break;
	case IDIAG_PRO_STAT_INVALID_STATE:
		printf("%s: %s invalid state\n",
		       interface->ifr_name, "PHY/TCVR loopback test");
		result = -1;
		break;
	case IDIAG_PRO_STAT_TEST_FAILED:
		printf("%s: %s FAILED.\n", interface->ifr_name,
			"PHY/TCVR loopback test");
		result = -1;
		break;
	default:
		printf("%s: %s FAILED.\n", interface->ifr_name,
			"PHY/TCVR loopback test");
		printf("%s:   Reason unknown.\n", interface->ifr_name);
		result = -1;
		break;
	}

	return result;
}

static int
e1000_test_link(int sock, struct ifreq *interface)
{
	struct idiag_pro_data *diag_data;

	diag_data = (struct idiag_pro_data *)interface->ifr_data;

	diag_data->cmd = IDIAG_E1000_DIAG_LINK_TEST;
	SYSCALL(ioctl(sock, IDIAG_PRO_BASE_SIOC, interface),
		interface->ifr_name);

	switch (diag_data->status) {
	case IDIAG_PRO_STAT_OK:
		printf("%s: Link test passed (has link).\n", 
				interface->ifr_name);
		return 0;
	case IDIAG_PRO_STAT_INVALID_STATE:
		printf("%s: Link test invalid state.\n",
		       interface->ifr_name);
		return -1;
	case IDIAG_PRO_STAT_TEST_FAILED:
		printf("%s: Link test FAILED (NO link).\n", 
				interface->ifr_name);
		return -1;
	default:
		printf("%s: Link test FAILED.\n", interface->ifr_name);
		printf("%s:   Reason unknown.\n", interface->ifr_name);
		return -1;
	}
	return 0;
}

static void
usage(char *name)
{
	fprintf(stderr, "\nUsage: %s [OPTION]\n\n", name);
	fprintf(stderr, "  -i <interface> : interface (eth0, etc)\n\n");
	fprintf(stderr, "  -l : run link test\n");
	fprintf(stderr, "  -t : run interrupt test\n");
	fprintf(stderr, "  -e : run EEPROM test\n");
	fprintf(stderr, "  -r : run register test\n");
	fprintf(stderr, "  -k : run loopback test\n");
}

int
main(int argc, char **argv)
{
	struct ifreq interface;
	struct idiag_pro_data diag_data;
	unsigned char *iface = NULL;
	int retval = 0, ch, sock;
	unsigned char command_line_parsed = 0;
	enum {
		TEST_LINK        = 0x1,
		TEST_INTERRUPT   = 0x2,
		TEST_EEPROM_XSUM = 0x8,
		TEST_REGISTER    = 0x10,
		TEST_LOOPBACK    = 0x20
	};
	unsigned int tests = 0;

	/* Make sure at least the interface name was given on the 
	 * command line */
	if(argc < 2) {
		usage(FILEPART(argv[0]));
		return -1;
	}

	while((ch = getopt(argc, argv, ":i:ltferk")) != -1) {
		switch (ch) {
		case 'i':
			iface = optarg;
			break;
		case 'l':
			tests |= TEST_LINK;
			break;
		case 't':
			tests |= TEST_INTERRUPT;
			break;
		case 'e':
			tests |= TEST_EEPROM_XSUM;
			break;
		case 'r':
			tests |= TEST_REGISTER;
			break;
		case 'k':
			tests |= TEST_LOOPBACK;
			break;
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

	if(iface == NULL) {
		usage(FILEPART(argv[0]));
		return -1;
	}

	/* Test for existance of the network interface */
	strcpy(interface.ifr_name, iface);
	SYSCALL((sock = socket(AF_INET, SOCK_DGRAM, 0)), 
		"Failed to create socket.");
	SYSCALL(ioctl(sock, SIOCGIFINDEX, &interface), interface.ifr_name);

	/* Test for the iDIAG interface, and get the driver ID */
	diag_data.interface_ver = IDIAG_PRO_VERSION;
	diag_data.driver_id = IDIAG_PRO_DRIVER_UNKNOWN;
	diag_data.cmd = IDIAG_PRO_IDENTIFY_DRIVER;
	interface.ifr_data = (char *)&diag_data;
	SYSCALL(ioctl(sock, IDIAG_PRO_BASE_SIOC, &interface),
		interface.ifr_name);

	if(diag_data.driver_id != IDIAG_E1000_DRIVER) {
		fprintf(stderr,
			"iDIAG driver ID 0x%x not supported by %s\n",
			IDIAG_E1000_DRIVER, interface.ifr_name);
		return -1;
	}

	if(diag_data.status != IDIAG_PRO_STAT_OK) {
		fprintf(stderr,
			"iDIAG interface version 0x%x not supported by %s\n",
			IDIAG_PRO_VERSION, interface.ifr_name);
		return -1;
	};

	/* Run the test(s) */

	if(tests & TEST_LINK)
		retval |= e1000_test_link(sock, &interface);

	if(tests & TEST_REGISTER)
		retval |= e1000_test_register(sock, &interface);


	if(tests & TEST_EEPROM_XSUM)
		retval |= e1000_test_xsum(sock, &interface);

	if(tests & TEST_INTERRUPT)
		retval |= e1000_test_intr(sock, &interface);

	if(tests & TEST_LOOPBACK)
		retval |= e1000_test_loopback(sock, &interface);

	return retval;
}
