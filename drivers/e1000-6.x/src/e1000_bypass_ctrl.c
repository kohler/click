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
 * Intel PRO diagnostics
 */

#include "e1000.h"
#include <asm/uaccess.h>
#include "e1000_bypass_ctrl.h"
#define E1000_CTRL_SDP0_SHIFT 18
#define E1000_CTRL_EXT_SDP6_SHIFT 6

static enum mode_status
e1000_drive_sdp_signals(struct e1000_adapter *adapter, uint32_t bypass_param)
{
	uint32_t value;

	DPRINTK(DRV, ERR, "Rcvd Sig %s-%1x\n", adapter->netdev->name, bypass_param);

	value = (E1000_READ_REG(&adapter->hw, CTRL));
	/* Make SDP0 Pin Directonality to Output */
	value |= E1000_CTRL_SWDPIO0;
	E1000_WRITE_REG(&adapter->hw, CTRL, value);

	value &= ~E1000_CTRL_SWDPIN0;
	value |= ((bypass_param & 0x1) << E1000_CTRL_SDP0_SHIFT);
	E1000_WRITE_REG(&adapter->hw, CTRL, value);

	value = (E1000_READ_REG(&adapter->hw, CTRL_EXT));
	/* Make SDP2 Pin Directonality to Output */
	value |= E1000_CTRL_EXT_SDP6_DIR;
	E1000_WRITE_REG(&adapter->hw, CTRL_EXT, value);

	value &= ~E1000_CTRL_EXT_SDP6_DATA;
	value |= (((bypass_param & 0x2) >> 1) << E1000_CTRL_EXT_SDP6_SHIFT);
	E1000_WRITE_REG(&adapter->hw, CTRL_EXT, value);

	return BYPASS_MODE_STAT_OK;
}

static enum mode_status e1000_check_bypass_status(struct e1000_adapter *adapter)
{
	uint32_t value;

	if(E1000_READ_REG(&adapter->hw, STATUS) & E1000_STATUS_FUNC_1) {
		value = ((E1000_READ_REG(&adapter->hw, CTRL_EXT)) & E1000_CTRL_EXT_SDP7_DATA);
		if(value)
			DPRINTK(DRV, ERR, "%s is in Bypass Mode\n", adapter->netdev->name);
		else
			DPRINTK(DRV, ERR, "%s is in Non-Bypass Mode\n", adapter->netdev->name);
		return BYPASS_MODE_STAT_OK;
	}
	else
		return BYPASS_MODE_STAT_FAILED;
}

int
e1000_bypass_ctrl_ioctl(struct net_device *netdev, struct ifreq *ifr)
{
	void *useraddr = ifr->ifr_data;
	struct ibypass_ctrl_data bypass_ctrl_data;
	struct e1000_adapter *adapter = netdev->priv;

	if(copy_from_user(&bypass_ctrl_data, useraddr, sizeof(bypass_ctrl_data)))
	{
		DPRINTK(DRV, ERR, "Could not copy from user %s\n", ifr->ifr_name);
		return -EPERM;
	}

	bypass_ctrl_data.status = BYPASS_MODE_STAT_NOT_SUPPORTED;

	if(bypass_ctrl_data.interface_ver != IBYPASS_MODECTRL_VERSION && 
			bypass_ctrl_data.driver_id != IBYPASS_E1000_DRIVER)
	{
		DPRINTK(DRV, ERR, "incorrect inf ver/dirver id %s\n", ifr->ifr_name);
		return -EFAULT;
	}

	/* Drive Bypass Mode Control SDP signals */
	switch (bypass_ctrl_data.cmd) {
		case IBYPASS_MODE_IDENTIFY_DRIVER:
			bypass_ctrl_data.driver_id = IBYPASS_E1000_DRIVER;
			bypass_ctrl_data.status = BYPASS_MODE_STAT_OK;
			break;
		case IDRIVE_BYPASS_CTRL_SIG:
			bypass_ctrl_data.status = 
				e1000_drive_sdp_signals(adapter, (uint32_t)bypass_ctrl_data.bypass_param[0]);
			break;
		case ICHECK_BYPASS_CTRL_STATUS:
			bypass_ctrl_data.status = e1000_check_bypass_status(adapter);
			break;
		default:
			DPRINTK(DRV, ERR, "Unknown Bypass Ctrl Sig - %s\n", ifr->ifr_name);
			bypass_ctrl_data.status = BYPASS_MODE_STAT_NOT_SUPPORTED;
			break;
	}

	if(copy_to_user(useraddr, &bypass_ctrl_data, sizeof(bypass_ctrl_data)))
		return -EPERM;

	return 0;
}

