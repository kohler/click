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

#ifndef _IDIAG_E1000_H
#define _IDIAG_E1000_H

/* Unique base driver identifier */

#define IDIAG_E1000_DRIVER                      0x02

/* e1000 diagnostic commands */

#define IDIAG_E1000_DIAG_REG_TEST               0x01
#define IDIAG_E1000_DIAG_XSUM_TEST              0x03
#define IDIAG_E1000_DIAG_INTR_TEST              0x04
#define IDIAG_E1000_DIAG_LOOPBACK_TEST          0x05
#define IDIAG_E1000_DIAG_LINK_TEST              0x06

struct idiag_e1000_diag_reg_test_param {
	uint16_t reg;
	uint16_t pad;
	uint32_t write_value;
	uint32_t read_value;
};


struct idiag_e1000_diag_eeprom_test_param {
	uint32_t expected_checksum;
	uint32_t actual_checksum;
};

enum idiag_e1000_diag_intr_test_param {
	IDIAG_E1000_INTR_TEST_OK,
	IDIAG_E1000_INTR_TEST_NOT_EXEC,
	IDIAG_E1000_INTR_TEST_FAILED_WHILE_DISABLED,
	IDIAG_E1000_INTR_TEST_FAILED_WHILE_ENABLED,
	IDIAG_E1000_INTR_TEST_FAILED_MASKED_ENABLED
};

enum idiag_e1000_diag_loopback_mode {
	IDIAG_E1000_DIAG_NONE_LB = 0,
	IDIAG_E1000_DIAG_MAC_LB,
	IDIAG_E1000_DIAG_PHY_TCVR_LB,
};

enum idiag_e1000_diag_loopback_result {
	IDIAG_E1000_LOOPBACK_TEST_OK,
	IDIAG_E1000_LOOPBACK_TEST_NOT_EXEC,
	IDIAG_E1000_LOOPBACK_TEST_FAILED
};

struct idiag_e1000_diag_loopback_test_param {
	enum idiag_e1000_diag_loopback_mode mode;
	enum idiag_e1000_diag_loopback_result result;
};

#endif /* _IDIAG_E1000_H */
