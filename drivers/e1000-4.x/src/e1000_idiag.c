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
 * Intel PRO diagnostics
 */

#include "e1000.h"
#include "idiag_pro.h"
#include "idiag_e1000.h"

extern int e1000_up(struct e1000_adapter *adapter);
extern void e1000_down(struct e1000_adapter *adapter);
extern void e1000_reset(struct e1000_adapter *adapter);

#define REG_PATTERN_TEST(R, M, W)                                          \
{                                                                          \
	uint32_t pat, value;                                               \
	uint32_t test[] =                                                  \
		{0x5A5A5A5A, 0xA5A5A5A5, 0x00000000, 0xFFFFFFFF};          \
	for(pat = 0; pat < sizeof(test)/sizeof(test[0]); pat++) {          \
		E1000_WRITE_REG(&adapter->hw, R, (test[pat] & W));     \
		value = E1000_READ_REG(&adapter->hw, R);               \
		if(value != (test[pat] & W & M)) {                         \
			param->reg =                                       \
				(adapter->hw.mac_type < e1000_82543) ? \
				E1000_82542_##R : E1000_##R;               \
			param->write_value = test[pat] & W;                \
			param->read_value = value;                         \
			return IDIAG_PRO_STAT_TEST_FAILED;                 \
		}                                                          \
	}                                                                  \
}

#define REG_SET_AND_CHECK(R, M, W)                                         \
{                                                                          \
	uint32_t value;                                                    \
	E1000_WRITE_REG(&adapter->hw, R, W & M);                       \
	value = E1000_READ_REG(&adapter->hw, R);                       \
	if ((W & M) != (value & M)) {                                      \
		param->reg = (adapter->hw.mac_type < e1000_82543) ?    \
			E1000_82542_##R : E1000_##R;                       \
		param->write_value = W & M;                                \
		param->read_value = value & M;                             \
		return IDIAG_PRO_STAT_TEST_FAILED;                         \
	}                                                                  \
}

static enum idiag_pro_stat
e1000_diag_reg_test(struct e1000_adapter *adapter,
		    uint8_t *diag_param)
{
	struct idiag_e1000_diag_reg_test_param *param =
		(struct idiag_e1000_diag_reg_test_param *) diag_param;
	uint32_t value;
	uint32_t i;

	/* The status register is Read Only, so a write should fail.
	 * Some bits that get toggled are ignored.
	 */
	value = (E1000_READ_REG(&adapter->hw, STATUS) & (0xFFFFF833));
	E1000_WRITE_REG(&adapter->hw, STATUS, (0xFFFFFFFF));
	if(value != (E1000_READ_REG(&adapter->hw, STATUS) & (0xFFFFF833))) {
		param->reg = E1000_STATUS;
		param->write_value = 0xFFFFFFFF;
		param->read_value = value;
		return IDIAG_PRO_STAT_TEST_FAILED;
	}

	REG_PATTERN_TEST(FCAL, 0xFFFFFFFF, 0xFFFFFFFF);
	REG_PATTERN_TEST(FCAH, 0x0000FFFF, 0xFFFFFFFF);
	REG_PATTERN_TEST(FCT, 0x0000FFFF, 0xFFFFFFFF);
	REG_PATTERN_TEST(VET, 0x0000FFFF, 0xFFFFFFFF);
	REG_PATTERN_TEST(RDTR, 0x0000FFFF, 0xFFFFFFFF);
	REG_PATTERN_TEST(RDBAH, 0xFFFFFFFF, 0xFFFFFFFF);
	REG_PATTERN_TEST(RDLEN, 0x000FFF80, 0x000FFFFF);
	REG_PATTERN_TEST(RDH, 0x0000FFFF, 0x0000FFFF);
	REG_PATTERN_TEST(RDT, 0x0000FFFF, 0x0000FFFF);
	REG_PATTERN_TEST(FCRTH, 0x0000FFF8, 0x0000FFF8);
	REG_PATTERN_TEST(FCTTV, 0x0000FFFF, 0x0000FFFF);
	REG_PATTERN_TEST(TIPG, 0x3FFFFFFF, 0x3FFFFFFF);
	REG_PATTERN_TEST(TDBAH, 0xFFFFFFFF, 0xFFFFFFFF);
	REG_PATTERN_TEST(TDLEN, 0x000FFF80, 0x000FFFFF);

	REG_SET_AND_CHECK(RCTL, 0xFFFFFFFF, 0x00000000);
	REG_SET_AND_CHECK(RCTL, 0x06DFB3FE, 0x003FFFFB);
	REG_SET_AND_CHECK(TCTL, 0xFFFFFFFF, 0x00000000);

	if(adapter->hw.mac_type >= e1000_82543) {

		REG_SET_AND_CHECK(RCTL, 0x06DFB3FE, 0xFFFFFFFF);
		REG_PATTERN_TEST(RDBAL, 0xFFFFFFF0, 0xFFFFFFFF);
		REG_PATTERN_TEST(TXCW, 0xC000FFFF, 0x0000FFFF);
		REG_PATTERN_TEST(TDBAL, 0xFFFFFFF0, 0xFFFFFFFF);
		REG_PATTERN_TEST(TIDV, 0x0000FFFF, 0x0000FFFF);

		for(i = 0; i < E1000_RAR_ENTRIES; i++) {
			REG_PATTERN_TEST(RA + ((i << 1) << 2), 0xFFFFFFFF,
					 0xFFFFFFFF);
			REG_PATTERN_TEST(RA + (((i << 1) + 1) << 2), 0x8003FFFF,
					 0xFFFFFFFF);
		}

	} else {

		REG_SET_AND_CHECK(RCTL, 0xFFFFFFFF, 0x01FFFFFF);
		REG_PATTERN_TEST(RDBAL, 0xFFFFF000, 0xFFFFFFFF);
		REG_PATTERN_TEST(TXCW, 0x0000FFFF, 0x0000FFFF);
		REG_PATTERN_TEST(TDBAL, 0xFFFFF000, 0xFFFFFFFF);

	}

	for(i = 0; i < E1000_MC_TBL_SIZE; i++)
		REG_PATTERN_TEST(MTA + (i << 2), 0xFFFFFFFF, 0xFFFFFFFF);

	return IDIAG_PRO_STAT_OK;
}


static enum idiag_pro_stat
e1000_diag_eeprom_test(struct e1000_adapter *adapter,
		       uint8_t *diag_param)
{
	struct idiag_e1000_diag_eeprom_test_param *param =
		(struct idiag_e1000_diag_eeprom_test_param *) diag_param;
	uint16_t temp;
	uint16_t checksum = 0;
	uint16_t i;

	/* Read and add up the contents of the EEPROM */
	for(i = 0; i < (EEPROM_CHECKSUM_REG + 1); i++) {
		if((e1000_read_eeprom(&adapter->hw, i, &temp)) < 0) {
			param->actual_checksum = checksum;
			param->expected_checksum = EEPROM_SUM;
			return IDIAG_PRO_STAT_TEST_FATAL;
		}
		checksum += temp;
	}

	/* If Checksum is not Correct return error else test passed */
	if(checksum != (uint16_t) EEPROM_SUM) {
		param->actual_checksum = checksum;
		param->expected_checksum = EEPROM_SUM;
		return IDIAG_PRO_STAT_TEST_FAILED;
	}

	return IDIAG_PRO_STAT_OK;
}

static void
e1000_diag_intr(int irq,
		void *data,
		struct pt_regs *regs)
{
	struct net_device *netdev = (struct net_device *) data;
	struct e1000_adapter *adapter = netdev->priv;

	adapter->diag_icr |= E1000_READ_REG(&adapter->hw, ICR);

	return;
}

static enum idiag_pro_stat
e1000_diag_intr_test(struct e1000_adapter *adapter,
		     uint8_t *diag_param)
{
	struct net_device *netdev = adapter->netdev;
	enum idiag_e1000_diag_intr_test_param *param =
		(enum idiag_e1000_diag_intr_test_param *) diag_param;
	uint32_t icr, i, mask;

	*param = IDIAG_E1000_INTR_TEST_OK;

	/* Hook up diag interrupt handler just for this test */
	if(request_irq
	   (netdev->irq, &e1000_diag_intr, SA_SHIRQ, netdev->name, netdev))
		return IDIAG_PRO_STAT_TEST_FATAL;

	/* Disable all the interrupts */
	E1000_WRITE_REG(&adapter->hw, IMC, 0xFFFFFFFF);
	msec_delay(10);

	/* Interrupts are disabled, so read interrupt cause
	 * register (icr) twice to verify that there are no interrupts
	 * pending.  icr is clear on read.
	 */
	icr = E1000_READ_REG(&adapter->hw, ICR);
	icr = E1000_READ_REG(&adapter->hw, ICR);

	if(icr != 0) {
		/* if icr is non-zero, there is no point
		 * running other interrupt tests.
		 */
		*param = IDIAG_E1000_INTR_TEST_NOT_EXEC;
		return IDIAG_PRO_STAT_TEST_FAILED;
	}

	/* Test each interrupt */
	for(i = 0; i < 10; i++) {

		/* Interrupt to test */
		mask = 1 << i;

		/* Disable the interrupt to be reported in
		 * the cause register and then force the same
		 * interrupt and see if one gets posted.  If
		 * an interrupt was posted to the bus, the
		 * test failed.
		 */
		adapter->diag_icr = 0;
		E1000_WRITE_REG(&adapter->hw, IMC, mask);
		E1000_WRITE_REG(&adapter->hw, ICS, mask);
		msec_delay(10);

		if(adapter->diag_icr & mask) {
			*param = IDIAG_E1000_INTR_TEST_FAILED_WHILE_DISABLED;
			break;
		}

		/* Enable the interrupt to be reported in
		 * the cause register and then force the same
		 * interrupt and see if one gets posted.  If
		 * an interrupt was not posted to the bus, the
		 * test failed.
		 */
		adapter->diag_icr = 0;
		E1000_WRITE_REG(&adapter->hw, IMS, mask);
		E1000_WRITE_REG(&adapter->hw, ICS, mask);
		msec_delay(10);

		if(!(adapter->diag_icr & mask)) {
			*param = IDIAG_E1000_INTR_TEST_FAILED_WHILE_ENABLED;
			break;
		}

		/* Disable the other interrupts to be reported in
		 * the cause register and then force the other
		 * interrupts and see if any get posted.  If
		 * an interrupt was posted to the bus, the
		 * test failed.
		 */
		adapter->diag_icr = 0;
		E1000_WRITE_REG(&adapter->hw, IMC, ~mask);
		E1000_WRITE_REG(&adapter->hw, ICS, ~mask);
		msec_delay(10);

		if(adapter->diag_icr) {
			*param = IDIAG_E1000_INTR_TEST_FAILED_MASKED_ENABLED;
			break;
		}
	}

	/* Disable all the interrupts */
	E1000_WRITE_REG(&adapter->hw, IMC, 0xFFFFFFFF);
	msec_delay(10);

	/* Unhook diag interrupt handler */
	free_irq(netdev->irq, netdev);

	return (*param ==
		IDIAG_E1000_INTR_TEST_OK) ? IDIAG_PRO_STAT_OK :
		IDIAG_PRO_STAT_TEST_FAILED;
}

static void
e1000_free_desc_rings(struct e1000_adapter *adapter)
{
	struct e1000_desc_ring *txdr = &adapter->diag_tx_ring;
	struct e1000_desc_ring *rxdr = &adapter->diag_rx_ring;
	struct pci_dev *pdev = adapter->pdev;
	int i;

	if(txdr->desc && txdr->buffer_info) {
		for(i = 0; i < txdr->count; i++) {
			if(txdr->buffer_info[i].dma)
				pci_unmap_single(pdev, txdr->buffer_info[i].dma,
						 txdr->buffer_info[i].length,
						 PCI_DMA_TODEVICE);
			if(txdr->buffer_info[i].skb)
				dev_kfree_skb(txdr->buffer_info[i].skb);
		}
	}

	if(rxdr->desc && rxdr->buffer_info) {
		for(i = 0; i < rxdr->count; i++) {
			if(rxdr->buffer_info[i].dma)
				pci_unmap_single(pdev, rxdr->buffer_info[i].dma,
						 rxdr->buffer_info[i].length,
						 PCI_DMA_FROMDEVICE);
			if(rxdr->buffer_info[i].skb)
				dev_kfree_skb(rxdr->buffer_info[i].skb);
		}
	}

	if(txdr->desc)
		pci_free_consistent(pdev, txdr->size, txdr->desc, txdr->dma);
	if(rxdr->desc)
		pci_free_consistent(pdev, rxdr->size, rxdr->desc, rxdr->dma);

#if 0
	if(txdr->buffer_info)
		kfree(txdr->buffer_info);
	if(rxdr->buffer_info)
		kfree(rxdr->buffer_info);
#endif

	return;
}

static int
e1000_setup_desc_rings(struct e1000_adapter *adapter)
{
	struct e1000_desc_ring *txdr = &adapter->diag_tx_ring;
	struct e1000_desc_ring *rxdr = &adapter->diag_rx_ring;
	struct pci_dev *pdev = adapter->pdev;
	uint32_t rctl;
	int size, i;

	/* Setup Tx descriptor ring and Tx buffers */

	txdr->count = 80;

	size = txdr->count * sizeof(struct e1000_buffer);
#if 0
	if(!(txdr->buffer_info = kmalloc(size, GFP_KERNEL)))
		goto err_nomem;
#endif
	memset(txdr->buffer_info, 0, size);

	txdr->size = txdr->count * sizeof(struct e1000_tx_desc);
	E1000_ROUNDUP(txdr->size, 4096);
	if(!(txdr->desc = pci_alloc_consistent(pdev, txdr->size, &txdr->dma)))
		goto err_nomem;
	memset(txdr->desc, 0, txdr->size);
	txdr->next_to_use = txdr->next_to_clean = 0;

	E1000_WRITE_REG(&adapter->hw, TDBAL,
			((uint64_t) txdr->dma & 0x00000000FFFFFFFF));
	E1000_WRITE_REG(&adapter->hw, TDBAH, ((uint64_t) txdr->dma >> 32));
	E1000_WRITE_REG(&adapter->hw, TDLEN,
			txdr->count * sizeof(struct e1000_tx_desc));
	E1000_WRITE_REG(&adapter->hw, TDH, 0);
	E1000_WRITE_REG(&adapter->hw, TDT, 0);
	E1000_WRITE_REG(&adapter->hw, TCTL,
			E1000_TCTL_PSP | E1000_TCTL_EN |
			E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT |
			E1000_FDX_COLLISION_DISTANCE << E1000_COLD_SHIFT);

	for(i = 0; i < txdr->count; i++) {
		struct e1000_tx_desc *tx_desc = E1000_TX_DESC(*txdr, i);
		struct sk_buff *skb;
		unsigned int size = 1024;

		if(!(skb = alloc_skb(size, GFP_KERNEL)))
			goto err_nomem;
		skb_put(skb, size);
		txdr->buffer_info[i].skb = skb;
		txdr->buffer_info[i].length = skb->len;
		txdr->buffer_info[i].dma =
			pci_map_single(pdev, skb->data, skb->len,
				       PCI_DMA_TODEVICE);
		tx_desc->buffer_addr = cpu_to_le64(txdr->buffer_info[i].dma);
		tx_desc->lower.data = cpu_to_le32(skb->len);
		tx_desc->lower.data |= E1000_TXD_CMD_EOP;
		tx_desc->lower.data |= E1000_TXD_CMD_IFCS;
		tx_desc->lower.data |= E1000_TXD_CMD_RPS;
		tx_desc->upper.data = 0;
	}

	/* Setup Rx descriptor ring and Rx buffers */

	rxdr->count = 80;

	size = rxdr->count * sizeof(struct e1000_buffer);
#if 0
	if(!(rxdr->buffer_info = kmalloc(size, GFP_KERNEL)))
		goto err_nomem;
#endif
	memset(rxdr->buffer_info, 0, size);

	rxdr->size = rxdr->count * sizeof(struct e1000_rx_desc);
	if(!(rxdr->desc = pci_alloc_consistent(pdev, rxdr->size, &rxdr->dma)))
		goto err_nomem;
	memset(rxdr->desc, 0, rxdr->size);
	rxdr->next_to_use = rxdr->next_to_clean = 0;

	rctl = E1000_READ_REG(&adapter->hw, RCTL);
	E1000_WRITE_REG(&adapter->hw, RCTL, rctl & ~E1000_RCTL_EN);
	E1000_WRITE_REG(&adapter->hw, RDBAL,
			((uint64_t) rxdr->dma & 0xFFFFFFFF));
	E1000_WRITE_REG(&adapter->hw, RDBAH, ((uint64_t) rxdr->dma >> 32));
	E1000_WRITE_REG(&adapter->hw, RDLEN, rxdr->size);
	E1000_WRITE_REG(&adapter->hw, RDH, 0);
	E1000_WRITE_REG(&adapter->hw, RDT, 0);
	rctl = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SZ_2048 |
		E1000_RCTL_LBM_NO | E1000_RCTL_RDMTS_HALF | 
		(adapter->hw.mc_filter_type << E1000_RCTL_MO_SHIFT);
	E1000_WRITE_REG(&adapter->hw, RCTL, rctl);

	for(i = 0; i < rxdr->count; i++) {
		struct e1000_rx_desc *rx_desc = E1000_RX_DESC(*rxdr, i);
		struct sk_buff *skb;

		if(!(skb = alloc_skb(E1000_RXBUFFER_2048 + 2, GFP_KERNEL)))
			goto err_nomem;
		skb_reserve(skb, 2);
		rxdr->buffer_info[i].skb = skb;
		rxdr->buffer_info[i].length = E1000_RXBUFFER_2048;
		rxdr->buffer_info[i].dma =
			pci_map_single(pdev, skb->data, E1000_RXBUFFER_2048,
				       PCI_DMA_FROMDEVICE);
		rx_desc->buffer_addr = cpu_to_le64(rxdr->buffer_info[i].dma);
		memset(skb->data, 0x00, skb->len);
	}

	return 0;

      err_nomem:
	e1000_free_desc_rings(adapter);
	return -ENOMEM;
}

/**
 * e1000_phy_disable_receiver - This routine disables the receiver
 * during loopback testing to insure that if, in the middle of a
 * loopback test, a link partner is connected, it won't change the
 * speed or link status and thus cause a failure.
 *
 * @adapter: board private structure
 **/

static void
e1000_phy_disable_receiver(struct e1000_adapter *adapter)
{
	/* Write out to PHY registers 29 and 30 to disable the Receiver. */
	e1000_write_phy_reg(&adapter->hw, 29, 0x001F);
	e1000_write_phy_reg(&adapter->hw, 30, 0x8FFC);
	e1000_write_phy_reg(&adapter->hw, 29, 0x001A);
	e1000_write_phy_reg(&adapter->hw, 30, 0x8FF0);

	return;
}

/**
 * e1000_phy_reset_clk_and_crs - This routine resets the TX_CLK and
 * TX_CRS registers on the non-integrated PHY.
 *
 * @adapter: board private structure
 **/

static void
e1000_phy_reset_clk_and_crs(struct e1000_adapter *adapter)
{
	uint16_t phy_reg;

	/* Because we reset the PHY above, we need to re-force TX_CLK in the
	 * Extended PHY Specific Control Register to 25MHz clock.  This
	 * value defaults back to a 2.5MHz clock when the PHY is reset.
	 */
	e1000_read_phy_reg(&adapter->hw, M88E1000_EXT_PHY_SPEC_CTRL, &phy_reg);
	phy_reg |= M88E1000_EPSCR_TX_CLK_25;
	e1000_write_phy_reg(&adapter->hw, 
		M88E1000_EXT_PHY_SPEC_CTRL, phy_reg);

	/* In addition, because of the s/w reset above, we need to enable
	 * CRS on TX.  This must be set for both full and half duplex
	 * operation.
	 */
	e1000_read_phy_reg(&adapter->hw, M88E1000_PHY_SPEC_CTRL, &phy_reg);
	phy_reg |= M88E1000_PSCR_ASSERT_CRS_ON_TX;
	e1000_write_phy_reg(&adapter->hw, 
		M88E1000_PHY_SPEC_CTRL, phy_reg);
}

/**
 * e1000_nonintegrated_phy_loopback - This routine enables the PHY 
 * loopback circuit to work on the non-integrated PHY, under *any* link
 * condition.
 *
 * @adapter: board private structure
 *
 * Returns 0 on success, 1 on failure
 **/

static int
e1000_nonintegrated_phy_loopback(struct e1000_adapter *adapter)
{
	uint32_t ctrl_reg;
	uint16_t phy_reg;
	int status = 1;

	/* Setup the Device Control Register for PHY loopback test. */

	ctrl_reg = E1000_READ_REG(&adapter->hw, CTRL);
	ctrl_reg |= (E1000_CTRL_ILOS |		/* Invert Loss-Of-Signal */
		     E1000_CTRL_FRCSPD |	/* Set the Force Speed Bit */
		     E1000_CTRL_FRCDPX |	/* Set the Force Duplex Bit */
		     E1000_CTRL_SPD_1000 |	/* Force Speed to 1000 */
		     E1000_CTRL_FD);		/* Force Duplex to FULL */

	E1000_WRITE_REG(&adapter->hw, CTRL, ctrl_reg);

	/* Read the PHY Specific Control Register (0x10) */
	e1000_read_phy_reg(&adapter->hw, M88E1000_PHY_SPEC_CTRL, &phy_reg);

	/* Clear Auto-Crossover bits in PHY Specific Control Register
	 * (bits 6:5).
	 */
	phy_reg &= ~M88E1000_PSCR_AUTO_X_MODE;
	e1000_write_phy_reg(&adapter->hw, M88E1000_PHY_SPEC_CTRL, phy_reg);

	/* Perform software reset on the PHY */
	e1000_phy_reset(&adapter->hw);

	/* Have to setup TX_CLK and TX_CRS after software reset */
	e1000_phy_reset_clk_and_crs(adapter);

	e1000_write_phy_reg(&adapter->hw, PHY_CTRL, 0x8100);

	/* Wait for reset to complete. */
	usec_delay(500);

	/* Have to setup TX_CLK and TX_CRS after software reset */
	e1000_phy_reset_clk_and_crs(adapter);

	/* Write out to PHY registers 29 and 30 to disable the Receiver. */
	e1000_phy_disable_receiver(adapter);

	/* Set the loopback bit in the PHY control register. */
	e1000_read_phy_reg(&adapter->hw, PHY_CTRL, &phy_reg);
	phy_reg |= MII_CR_LOOPBACK;
	e1000_write_phy_reg(&adapter->hw, PHY_CTRL, phy_reg);

	/* Setup TX_CLK and TX_CRS one more time. */
	e1000_phy_reset_clk_and_crs(adapter);

	status = 0;

	/* Check Phy Configuration */
	e1000_read_phy_reg(&adapter->hw, PHY_CTRL, &phy_reg);
	if(phy_reg != 0x4100) {
		status = 1;
	}

	e1000_read_phy_reg(&adapter->hw, M88E1000_EXT_PHY_SPEC_CTRL, &phy_reg);
	if(phy_reg != 0x0070) {
		status = 1;
	}

	e1000_read_phy_reg(&adapter->hw, 29, &phy_reg);
	if(phy_reg != 0x001A) {
		status = 1;
	}

	return status;
}

/**
 * e1000_integrated_phy_loopback - This routine is used by diagnostic
 * software to put the 82544, 82540, 82545, and 82546 MAC based network
 * cards into loopback mode.
 *
 * @adapter: board private structure
 * @speed: speed
 *               
 *  Current procedure is to:
 *    1) Disable auto-MDI/MDIX
 *    2) Perform SW phy reset (bit 15 of PHY_CTRL)
 *    3) Disable autoneg and reset
 *    4) For the specified speed, set the loopback
 *       mode for that speed.  Also force the MAC
 *       to the correct speed and duplex for the
 *       specified operation.
 *    5) If this is an 82543, setup the TX_CLK and
 *       TX_CRS again.
 *    6) Disable the receiver so a cable disconnect
 *       and reconnect will not cause autoneg to
 *       begin.
 *
 * Returns 0 on success, 1 on failure
 **/

static int
e1000_integrated_phy_loopback(struct e1000_adapter *adapter,
			      uint16_t speed)
{
	uint32_t ctrl_reg = 0;
	uint32_t stat_reg = 0;
	boolean_t loopback_mode_set = FALSE;

	adapter->hw.autoneg = FALSE;

	/* Set up desired loopback speed and duplex depending on input
	 * into this function.
	 */
	switch (speed) {
	case SPEED_1000:
		/* Set up the MII control reg to the desired loopback speed. */

		/* Auto-MDI/MDIX Off */
		e1000_write_phy_reg(&adapter->hw, M88E1000_PHY_SPEC_CTRL,
				    0x0808);
		/* reset to update Auto-MDI/MDIX */
		e1000_write_phy_reg(&adapter->hw, PHY_CTRL, 0x9140);
		/* autoneg off */
		e1000_write_phy_reg(&adapter->hw, PHY_CTRL, 0x8140);
		/* force 1000, set loopback */
		e1000_write_phy_reg(&adapter->hw, PHY_CTRL, 0x4140);

		/* Now set up the MAC to the same speed/duplex as the PHY. */
		ctrl_reg = E1000_READ_REG(&adapter->hw, CTRL);
		ctrl_reg &= ~E1000_CTRL_SPD_SEL; /* Clear the speed sel bits */
		ctrl_reg |= (E1000_CTRL_FRCSPD | /* Set the Force Speed Bit */
			     E1000_CTRL_FRCDPX | /* Set the Force Duplex Bit */
			     E1000_CTRL_SPD_1000 |/* Force Speed to 1000 */
			     E1000_CTRL_FD);	 /* Force Duplex to FULL */

		if(adapter->hw.media_type == e1000_media_type_copper) {
			ctrl_reg |= E1000_CTRL_ILOS; /* Invert Loss of Signal */
		} else {
			/* Set the ILOS bit on the fiber Nic is half
			 * duplex link is detected. */
			stat_reg = E1000_READ_REG(&adapter->hw, STATUS);
			if((stat_reg & E1000_STATUS_FD) == 0)
				ctrl_reg |= (E1000_CTRL_ILOS | E1000_CTRL_SLU);
		}

		E1000_WRITE_REG(&adapter->hw, CTRL, ctrl_reg);
		loopback_mode_set = TRUE;
		break;

	case SPEED_100:
		/* Set up the MII control reg to the desired loopback speed. */

		/* Auto-MDI/MDIX Off */
		e1000_write_phy_reg(&adapter->hw, M88E1000_PHY_SPEC_CTRL,
				    0x0808);
		/* reset to update Auto-MDI/MDIX */
		e1000_write_phy_reg(&adapter->hw, PHY_CTRL, 0x9140);
		/* autoneg off */
		e1000_write_phy_reg(&adapter->hw, PHY_CTRL, 0x8140);
		/* reset to update autoneg */
		e1000_write_phy_reg(&adapter->hw, PHY_CTRL, 0x8100);
		/* MAC interface speed to 100Mbps */
		e1000_write_phy_reg(&adapter->hw,
				    M88E1000_EXT_PHY_SPEC_CTRL, 0x0c14);
		/* reset to update MAC interface speed */
		e1000_write_phy_reg(&adapter->hw, PHY_CTRL, 0xe100);
		/* force 100, set loopback */
		e1000_write_phy_reg(&adapter->hw, PHY_CTRL, 0x6100);

		/* Now set up the MAC to the same speed/duplex as the PHY. */
		ctrl_reg = E1000_READ_REG(&adapter->hw, CTRL);
		ctrl_reg &= ~E1000_CTRL_SPD_SEL; /* Clear the speed sel bits */
		ctrl_reg |= (E1000_CTRL_ILOS |	 /* Invert Loss-Of-Signal */
			     E1000_CTRL_SLU |    /* Set the Force Link Bit */
			     E1000_CTRL_FRCSPD | /* Set the Force Speed Bit */
			     E1000_CTRL_FRCDPX | /* Set the Force Duplex Bit */
			     E1000_CTRL_SPD_100 |/* Force Speed to 100 */
			     E1000_CTRL_FD);	 /* Force Duplex to FULL */

		E1000_WRITE_REG(&adapter->hw, CTRL, ctrl_reg);
		loopback_mode_set = TRUE;
		break;

	case SPEED_10:
		/* Set up the MII control reg to the desired loopback speed. */

		/* Auto-MDI/MDIX Off */
		e1000_write_phy_reg(&adapter->hw, M88E1000_PHY_SPEC_CTRL,
				    0x0808);
		/* reset to update Auto-MDI/MDIX */
		e1000_write_phy_reg(&adapter->hw, PHY_CTRL, 0x9140);
		/* autoneg off */
		e1000_write_phy_reg(&adapter->hw, PHY_CTRL, 0x8140);
		/* reset to update autoneg */
		e1000_write_phy_reg(&adapter->hw, PHY_CTRL, 0x8100);
		/* MAC interface speed to 10Mbps */
		e1000_write_phy_reg(&adapter->hw,
				    M88E1000_EXT_PHY_SPEC_CTRL, 0x0c04);
		/* reset to update MAC interface speed */
		e1000_write_phy_reg(&adapter->hw, PHY_CTRL, 0x8100);
		/* force 10, set loopback */
		e1000_write_phy_reg(&adapter->hw, PHY_CTRL, 0x4100);

		/* Now set up the MAC to the same speed/duplex as the PHY. */
		ctrl_reg = E1000_READ_REG(&adapter->hw, CTRL);
		ctrl_reg &= ~E1000_CTRL_SPD_SEL; /* Clear the speed sel bits */
		ctrl_reg |= (E1000_CTRL_SLU |	/* Set the Force Link Bit */
			     E1000_CTRL_FRCSPD |/* Set the Force Speed Bit */
			     E1000_CTRL_FRCDPX |/* Set the Force Duplex Bit */
			     E1000_CTRL_SPD_10 |/* Force Speed to 10 */
			     E1000_CTRL_FD);	/* Force Duplex to FULL */

		E1000_WRITE_REG(&adapter->hw, CTRL, ctrl_reg);
		loopback_mode_set = TRUE;
		break;

	default:
		loopback_mode_set = FALSE;
		break;
	}

	/* Disable the receiver on the PHY so when a cable is plugged
	 * in, the PHY does not begin to autoneg when a cable is 
	 * reconnected to the NIC.
	 */
	e1000_phy_disable_receiver(adapter);

	usec_delay(500);

	return loopback_mode_set ? 0 : 1;
}

/**
 * e1000_set_phy_loopback - Set the PHY into loopback mode.
 * @adapter: board private structure
 *
 * Returns 0 on success, 1 on failure
 **/

static int
e1000_set_phy_loopback(struct e1000_adapter *adapter)
{
	uint16_t phy_reg = 0;
	uint16_t speed = 0;
	uint16_t duplex = 0;
	int status = 1;
	int count;

	switch (adapter->hw.mac_type) {
	case e1000_82543:
		if(adapter->hw.media_type == e1000_media_type_copper) {
			/* Attempt to setup Loopback mode on Non-
			 * integrated PHY.  Some PHY registers get
			 * corrupted at random, so attempt this
			 * 10 times.
			 */
			for(count = 0; count < 10; count++)
				if(!e1000_nonintegrated_phy_loopback(adapter))
					break;
			status = 0;
		}
		break;

	case e1000_82544:
		if(adapter->hw.media_type == e1000_media_type_copper)
			e1000_get_speed_and_duplex(&adapter->hw, &speed,
			                           &duplex);
			status = e1000_integrated_phy_loopback(adapter, speed);
		break;

	case e1000_82540:
	case e1000_82545:
	case e1000_82546:
		e1000_get_speed_and_duplex(&adapter->hw, &speed, &duplex);
		status = e1000_integrated_phy_loopback(adapter, speed);
		break;

	default:
		/* Default PHY loopback work is to read the MII 
		 * control register and assert bit 14 (loopback mode).
		 */
		e1000_read_phy_reg(&adapter->hw, PHY_CTRL, &phy_reg);
		phy_reg |= MII_CR_LOOPBACK;
		e1000_write_phy_reg(&adapter->hw, PHY_CTRL,
				    phy_reg);
		status = 0;
		break;
	}

	return status;
}

static int
e1000_set_loopback_mode(struct e1000_adapter *adapter,
			enum idiag_e1000_diag_loopback_mode mode)
{
	uint32_t rctl;
	uint16_t phy_reg;
	int status = 1;

	switch (mode) {

	case IDIAG_E1000_DIAG_NONE_LB:
		/* Clear bits 7:6 to turn off loopback mode */
		rctl = E1000_READ_REG(&adapter->hw, RCTL);
		rctl &= ~(E1000_RCTL_LBM_TCVR | E1000_RCTL_LBM_MAC);
		E1000_WRITE_REG(&adapter->hw, RCTL, rctl);
		/* Only modify the GMII/MII PHY device if the
		 * media type is copper.
		 */
		if(adapter->hw.media_type == e1000_media_type_copper ||
		  (adapter->hw.media_type == e1000_media_type_fiber &&
		  (adapter->hw.mac_type == e1000_82545 ||
		   adapter->hw.mac_type == e1000_82546))) {
			adapter->hw.autoneg = TRUE;
			/* De-assert bit 14 (loopback mode) in PHY */
			e1000_read_phy_reg(&adapter->hw, PHY_CTRL, &phy_reg);
			/* Only turn off PHY loopback if enabled */
			if(phy_reg & MII_CR_LOOPBACK) {
				phy_reg &= ~MII_CR_LOOPBACK;
				e1000_write_phy_reg(&adapter->hw, PHY_CTRL,
						    phy_reg);
				/* Reset the PHY to make sure we
				 * regain link */
				e1000_phy_reset(&adapter->hw);
			}
		}
		status = 0;
		break;


	case IDIAG_E1000_DIAG_MAC_LB:
		/* Not supported */
		break;

	case IDIAG_E1000_DIAG_PHY_TCVR_LB:
		if(adapter->hw.media_type == e1000_media_type_fiber) {
			if(adapter->hw.mac_type == e1000_82545 ||
			   adapter->hw.mac_type == e1000_82546) {
				status = e1000_set_phy_loopback(adapter);
			} else {
				rctl = E1000_READ_REG(&adapter->hw, RCTL);
				rctl |= E1000_RCTL_LBM_TCVR;
				E1000_WRITE_REG(&adapter->hw, RCTL, rctl);
				status = 0;
			}
		}
		if(adapter->hw.media_type == e1000_media_type_copper) {
			status = e1000_set_phy_loopback(adapter);
		}
		break;
	}

	return status;
}

static void
e1000_create_lbtest_frame(struct sk_buff *skb,
			  unsigned int frame_size)
{
	memset(skb->data, 0xFF, frame_size);
	frame_size = (frame_size % 2) ? (frame_size - 1) : frame_size;
	memset(&skb->data[frame_size / 2], 0xAA, frame_size / 2 - 1);
	memset(&skb->data[frame_size / 2 + 10], 0xBE, 1);
	memset(&skb->data[frame_size / 2 + 12], 0xAF, 1);
}

static int
e1000_check_lbtest_frame(struct sk_buff *skb,
			 unsigned int frame_size)
{
	frame_size = (frame_size % 2) ? (frame_size - 1) : frame_size;
	if(*(skb->data + 3) == 0xFF) {
		if((*(skb->data + frame_size / 2 + 10) == 0xBE) &&
		   (*(skb->data + frame_size / 2 + 12) == 0xAF)) {
			return 1;
		}
	}
	return 0;
}

static enum idiag_e1000_diag_loopback_result
e1000_loopback_test(struct e1000_adapter *adapter)
{
	struct e1000_desc_ring *txdr = &adapter->diag_tx_ring;
	struct e1000_desc_ring *rxdr = &adapter->diag_rx_ring;
	struct pci_dev *pdev = adapter->pdev;
	int i;

	E1000_WRITE_REG(&adapter->hw, RDT, rxdr->count - 1);

	for(i = 0; i < 64; i++) {
		e1000_create_lbtest_frame(txdr->buffer_info[i].skb, 1024);
		pci_dma_sync_single(pdev, txdr->buffer_info[i].dma,
				    txdr->buffer_info[i].length,
				    PCI_DMA_TODEVICE);
	}
	E1000_WRITE_REG(&adapter->hw, TDT, i);

	msec_delay(200);

	pci_dma_sync_single(pdev, rxdr->buffer_info[0].dma,
			    rxdr->buffer_info[0].length, PCI_DMA_FROMDEVICE);
	if(e1000_check_lbtest_frame(rxdr->buffer_info[0].skb, 1024))
		return IDIAG_E1000_LOOPBACK_TEST_OK;
	else
		return IDIAG_E1000_LOOPBACK_TEST_FAILED;
}

static enum idiag_pro_stat
e1000_diag_loopback_test(struct e1000_adapter *adapter,
			 uint8_t *diag_param)
{
	struct idiag_e1000_diag_loopback_test_param *param =
		(struct idiag_e1000_diag_loopback_test_param *) diag_param;

	if(param->mode == IDIAG_E1000_DIAG_MAC_LB) {
		/* Loopback test not support */
		param->result = IDIAG_E1000_LOOPBACK_TEST_NOT_EXEC;
		return IDIAG_PRO_STAT_NOT_SUPPORTED;
	}

	if(e1000_setup_desc_rings(adapter)) {
		param->result = IDIAG_E1000_LOOPBACK_TEST_NOT_EXEC;
		return IDIAG_PRO_STAT_TEST_FAILED;
	}

	if(e1000_set_loopback_mode(adapter, param->mode)) {
		param->result = IDIAG_E1000_LOOPBACK_TEST_NOT_EXEC;
		e1000_free_desc_rings(adapter);
		return IDIAG_PRO_STAT_TEST_FAILED;
	}
	param->result = e1000_loopback_test(adapter);
	e1000_set_loopback_mode(adapter, IDIAG_E1000_DIAG_NONE_LB);

	e1000_free_desc_rings(adapter);

	return (param->result ==
		IDIAG_E1000_LOOPBACK_TEST_OK) ? IDIAG_PRO_STAT_OK :
		IDIAG_PRO_STAT_TEST_FAILED;
}

static enum idiag_pro_stat
e1000_diag_link_test(struct e1000_adapter *adapter,
		     uint8_t *diag_param)
{
	e1000_check_for_link(&adapter->hw);

	if(E1000_READ_REG(&adapter->hw, STATUS) & E1000_STATUS_LU)
		return IDIAG_PRO_STAT_OK;
	else
		return IDIAG_PRO_STAT_TEST_FAILED;
}

int
e1000_diag_ioctl(struct net_device *netdev,
		 struct ifreq *ifr)
{
	struct e1000_adapter *adapter = netdev->priv;
	struct idiag_pro_data *diag_data =
		(struct idiag_pro_data *) ifr->ifr_data;
	boolean_t run_offline;
	boolean_t interface_up = netif_running(netdev);

	diag_data->status = IDIAG_PRO_STAT_NOT_SUPPORTED;

	if(!capable(CAP_NET_ADMIN))
		/* must have admin capabilities */
		return -EPERM;

	if(diag_data->interface_ver != IDIAG_PRO_VERSION)
		/* incorrect diagnostics interface version */
		return -EFAULT;

	if(diag_data->cmd != IDIAG_PRO_IDENTIFY_DRIVER &&
	   diag_data->driver_id != IDIAG_E1000_DRIVER)
		/* incorrect driver identifier */
		return -EFAULT;

	/* Some test requring exclusive access to hardware, so
	 * we need to teardown the hardware setup, run the test,
	 * and restore the hardware to resume the network 
	 * connection. 
	 */
	run_offline =  (diag_data->cmd == IDIAG_E1000_DIAG_REG_TEST ||
			diag_data->cmd == IDIAG_E1000_DIAG_INTR_TEST ||
			diag_data->cmd == IDIAG_E1000_DIAG_LOOPBACK_TEST);

	if(run_offline) {
		if(interface_up)
			e1000_down(adapter);
		else
			e1000_reset(adapter);
	}

	/* Run the diagnotic test */
	switch (diag_data->cmd) {

	case IDIAG_PRO_IDENTIFY_DRIVER:
		diag_data->driver_id = IDIAG_E1000_DRIVER;
		diag_data->status = IDIAG_PRO_STAT_OK;
		break;

	case IDIAG_E1000_DIAG_REG_TEST:
		diag_data->status =
			e1000_diag_reg_test(adapter, diag_data->diag_param);
		break;

	case IDIAG_E1000_DIAG_XSUM_TEST:
		diag_data->status =
			e1000_diag_eeprom_test(adapter, diag_data->diag_param);
		break;

	case IDIAG_E1000_DIAG_INTR_TEST:
		diag_data->status =
			e1000_diag_intr_test(adapter, diag_data->diag_param);
		break;

	case IDIAG_E1000_DIAG_LOOPBACK_TEST:
		diag_data->status =
			e1000_diag_loopback_test(adapter,
						 diag_data->diag_param);
		break;

	case IDIAG_E1000_DIAG_LINK_TEST:
		diag_data->status =
			e1000_diag_link_test(adapter, diag_data->diag_param);
		break;

	default:
		diag_data->status = IDIAG_PRO_STAT_NOT_SUPPORTED;
		break;
	}

	if(run_offline) {
		e1000_reset(adapter);
		if(interface_up) {
			if(e1000_up(adapter)) {
				diag_data->status = IDIAG_PRO_STAT_TEST_FATAL;
				return -EFAULT;
			}
		}
	}

	return 0;
}

