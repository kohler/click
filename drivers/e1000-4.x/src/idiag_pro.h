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

#ifndef _IDIAG_PRO_H
#define _IDIAG_PRO_H

#define IDIAG_PRO_VERSION           0x200
#define IDIAG_PRO_PARAM_SIZE        0x80

#define IDIAG_PRO_DRIVER_UNKNOWN    0x0
#define IDIAG_PRO_IDENTIFY_DRIVER   0x0

#define IDIAG_PRO_BASE_SIOC    (SIOCDEVPRIVATE + 10)

enum idiag_pro_stat {
	IDIAG_PRO_STAT_OK,
	IDIAG_PRO_STAT_BAD_PARAM,
	IDIAG_PRO_STAT_TEST_FAILED,
	IDIAG_PRO_STAT_INVALID_STATE,
	IDIAG_PRO_STAT_NOT_SUPPORTED,
	IDIAG_PRO_STAT_TEST_FATAL
};

struct idiag_pro_data {
	/* in */
	uint32_t cmd;
	uint32_t interface_ver;
	uint32_t driver_id;
	uint32_t reserved_in[8];
	/* out */
	enum idiag_pro_stat status;
	uint32_t reserved_out[8];
	/* in/out */
	uint8_t diag_param[IDIAG_PRO_PARAM_SIZE];
};

#endif /* _IDIAG_PRO_H */
