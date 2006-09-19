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

#ifndef _IDEWEY_BEACH_E1000_H
#define _IDEWEY_BEACH_E1000_H

/* Unique base driver identifier */

#define IBYPASS_E1000_DRIVER                      0x02

#define IBYPASS_MODECTRL_VERSION 0x200
#define IBYPASS_PARAM_SIZE        0x80

#define IBYPASS_MODE_DRIVER_UNKNOWN    0x0
#define IBYPASS_MODE_IDENTIFY_DRIVER   0x0

#define BYPASS_MODE_CTRL_SIOC    (SIOCDEVPRIVATE + 10)

/* e1000 diagnostic commands */

#define IDRIVE_BYPASS_CTRL_SIG						0x01
#define ICHECK_BYPASS_CTRL_STATUS					0x02

enum mode_status {
	BYPASS_MODE_STAT_OK,
	BYPASS_MODE_STAT_FAILED,
	BYPASS_MODE_STAT_BAD_PARAM,
	BYPASS_MODE_STAT_INVALID_STATE,
	BYPASS_MODE_STAT_NOT_SUPPORTED,
};

struct ibypass_ctrl_data {
	/* in */
	uint32_t cmd;
	uint32_t interface_ver;
	uint32_t driver_id;
	uint32_t reserved_in[8];
	/* out */
	enum mode_status status;
	uint32_t reserved_out[8];
	/* in/out */
	uint8_t bypass_param[IBYPASS_PARAM_SIZE];
};

#endif /* _IDEWEY_BEACH_E1000_H */
