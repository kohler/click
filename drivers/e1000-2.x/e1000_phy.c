/*****************************************************************************
 *****************************************************************************
 Copyright (c) 1999-2000, Intel Corporation 

 All rights reserved.

 Redistribution and use in source and binary forms, with or without 
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, 
 this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation 
 and/or other materials provided with the distribution.

 3. Neither the name of Intel Corporation nor the names of its contributors 
 may be used to endorse or promote products derived from this software 
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

 *****************************************************************************
****************************************************************************/

#include <linux/types.h>
#include "e1000.h"
#include "e1000_fxhw.h"
#include "e1000_phy.h"

#define GOOD_MII_IF 0

UINT16
ReadPhyRegister(IN PADAPTER_STRUCT Adapter,
                IN UINT32 RegAddress, IN UINT32 PhyAddress);

VOID
WritePhyRegister(IN PADAPTER_STRUCT Adapter,
                 IN UINT32 RegAddress,
                 IN UINT32 PhyAddress, IN UINT16 Data);

STATIC
    VOID
MIIShiftOutPhyData(IN PADAPTER_STRUCT Adapter,
                   IN UINT32 Data, IN UINT16 Count);

STATIC
    VOID
RaiseMdcClock(IN PADAPTER_STRUCT Adapter, IN OUT UINT32 * CtrlRegValue);

STATIC
    VOID
LowerMdcClock(IN PADAPTER_STRUCT Adapter, IN OUT UINT32 * CtrlRegValue);

STATIC UINT16 MIIShiftInPhyData(IN PADAPTER_STRUCT Adapter);

VOID PhyHardwareReset(IN PADAPTER_STRUCT Adapter);

BOOLEAN PhyReset(IN PADAPTER_STRUCT Adapter);

BOOLEAN PhySetup(IN PADAPTER_STRUCT Adapter, IN UINT32 DeviceControlReg);

STATIC BOOLEAN PhySetupAutoNegAdvertisement(IN PADAPTER_STRUCT Adapter);

STATIC VOID PhyForceSpeedAndDuplex(IN PADAPTER_STRUCT Adapter);

VOID
ConfigureMacToPhySettings(IN PADAPTER_STRUCT Adapter,
                          IN UINT16 MiiRegisterData);

VOID DisplayMiiContents(IN PADAPTER_STRUCT Adapter, IN UINT8 PhyAddress);

UINT32 AutoDetectGigabitPhy(IN PADAPTER_STRUCT Adapter);
VOID PxnPhyResetDsp(IN PADAPTER_STRUCT Adapter);

UINT16
ReadPhyRegister(IN PADAPTER_STRUCT Adapter,
                IN UINT32 RegAddress, IN UINT32 PhyAddress)
 {
    UINT32 Data;
    UINT32 Command;

    ASSERT(RegAddress <= MAX_PHY_REG_ADDRESS);

    if (0) {

        Command = ((RegAddress << MDI_REGADD_SHIFT) |
                   (PhyAddress << MDI_PHYADD_SHIFT) | (E1000_MDI_READ));

        E1000_WRITE_REG(Mdic, Command);

        while (TRUE) {
            DelayInMicroseconds(10);

            Data = E1000_READ_REG(Mdic);

            if (Data & E1000_MDI_READY)
                break;
        }

        ASSERT(!(Data & E1000_MDI_ERR));
    } else {

        MIIShiftOutPhyData(Adapter, PHY_PREAMBLE, PHY_PREAMBLE_SIZE);

        Command = ((RegAddress) |
                   (PhyAddress << 5) |
                   (PHY_OP_READ << 10) | (PHY_SOF << 12));

        MIIShiftOutPhyData(Adapter, Command, 14);

        Data = (UINT32) MIIShiftInPhyData(Adapter);

        ASSERT(!(Data & E1000_MDI_ERR));
    }

    return ((UINT16) Data);
}

VOID
WritePhyRegister(IN PADAPTER_STRUCT Adapter,
                 IN UINT32 RegAddress,
                 IN UINT32 PhyAddress, IN UINT16 Data)
 {

    UINT32 Command;

    ASSERT(RegAddress <= MAX_PHY_REG_ADDRESS);

    if (0) {

        Command = (((ULONG) Data) |
                   (RegAddress << MDI_REGADD_SHIFT) |
                   (PhyAddress << MDI_PHYADD_SHIFT) | (E1000_MDI_WRITE));

        E1000_WRITE_REG(Mdic, Command);

        DelayInMicroseconds(10);

        while (!(E1000_READ_REG(Mdic) & E1000_MDI_READY))
            DelayInMicroseconds(10);

        ASSERT(!(E1000_READ_REG(Mdic) & E1000_MDI_ERR));
    } else {

        MIIShiftOutPhyData(Adapter, PHY_PREAMBLE, PHY_PREAMBLE_SIZE);

        Command = (Data |
                   (PHY_TURNAROUND << 16) |
                   (RegAddress << 18) |
                   (PhyAddress << 23) |
                   (PHY_OP_WRITE << 28) | (PHY_SOF << 30));

        MIIShiftOutPhyData(Adapter, Command, 32);
    }

    return;
}

STATIC UINT16 MIIShiftInPhyData(IN PADAPTER_STRUCT Adapter)
 {
    UINT32 CtrlRegValue;
    UINT16 Data = 0;
    UINT8 i;

    CtrlRegValue = E1000_READ_REG(Ctrl);

    CtrlRegValue &= ~E1000_CTRL_MDIO_DIR;
    CtrlRegValue &= ~E1000_CTRL_MDIO;

    E1000_WRITE_REG(Ctrl, CtrlRegValue);

    RaiseMdcClock(Adapter, &CtrlRegValue);
    LowerMdcClock(Adapter, &CtrlRegValue);

    for (Data = 0, i = 0; i < 16; i++) {
        Data = Data << 1;
        RaiseMdcClock(Adapter, &CtrlRegValue);

        CtrlRegValue = E1000_READ_REG(Ctrl);

        if (CtrlRegValue & E1000_CTRL_MDIO)
            Data |= 1;

        LowerMdcClock(Adapter, &CtrlRegValue);
    }

    RaiseMdcClock(Adapter, &CtrlRegValue);
    LowerMdcClock(Adapter, &CtrlRegValue);

    return (Data);
}

STATIC
    VOID
MIIShiftOutPhyData(IN PADAPTER_STRUCT Adapter,
                   IN UINT32 Data, IN UINT16 Count)
 {
    UINT32 CtrlRegValue;
    UINT32 Mask;

    if (Count > 32)
        ASSERT(0);

    Mask = 0x01 << (Count - 1);

    CtrlRegValue = E1000_READ_REG(Ctrl);

    CtrlRegValue |= (E1000_CTRL_MDIO_DIR | E1000_CTRL_MDC_DIR);

    while (Mask) {

        if (Data & Mask)
            CtrlRegValue |= E1000_CTRL_MDIO;
        else
            CtrlRegValue &= ~E1000_CTRL_MDIO;

        E1000_WRITE_REG(Ctrl, CtrlRegValue);

        DelayInMicroseconds(2);

        RaiseMdcClock(Adapter, &CtrlRegValue);
        LowerMdcClock(Adapter, &CtrlRegValue);

        Mask = Mask >> 1;
    }
}

STATIC
    VOID
RaiseMdcClock(IN PADAPTER_STRUCT Adapter, IN OUT UINT32 * CtrlRegValue)
 {

    E1000_WRITE_REG(Ctrl, (*CtrlRegValue | E1000_CTRL_MDC));

    DelayInMicroseconds(2);
}

STATIC
    VOID
LowerMdcClock(IN PADAPTER_STRUCT Adapter, IN OUT UINT32 * CtrlRegValue)
 {

    E1000_WRITE_REG(Ctrl, (*CtrlRegValue & ~E1000_CTRL_MDC));

    DelayInMicroseconds(2);
}

VOID PhyHardwareReset(IN PADAPTER_STRUCT Adapter)
 {
    UINT32 ExtCtrlRegValue, CtrlRegValue;
    UINT16 PhyCtrlRegValue;

    DEBUGFUNC("PhyHardwareReset")

        DEBUGOUT("Resetting Phy (GoodHope hardware)..\n");

    ExtCtrlRegValue = E1000_READ_REG(Exct);

    ExtCtrlRegValue |= E1000_CTRL_PHY_RESET_DIR4;

    E1000_WRITE_REG(Exct, ExtCtrlRegValue);

    DelayInMilliseconds(20);

    ExtCtrlRegValue = E1000_READ_REG(Exct);

    ExtCtrlRegValue &= ~E1000_CTRL_PHY_RESET4;

    E1000_WRITE_REG(Exct, ExtCtrlRegValue);

    DelayInMilliseconds(20);

    ExtCtrlRegValue = E1000_READ_REG(Exct);

    ExtCtrlRegValue |= E1000_CTRL_PHY_RESET4;

    E1000_WRITE_REG(Exct, ExtCtrlRegValue);

    DelayInMilliseconds(20);

    return;
}

BOOLEAN PhyReset(IN PADAPTER_STRUCT Adapter)
 {
    UINT16 RegData;
    UINT16 i;
    BOOLEAN Status;

    DEBUGFUNC("PhyReset")

        RegData = ReadPhyRegister(Adapter,
                                  PHY_MII_CTRL_REG, Adapter->PhyAddress);

    RegData |= MII_CR_RESET;

    WritePhyRegister(Adapter,
                     PHY_MII_CTRL_REG, Adapter->PhyAddress, RegData);

    i = 0;
    while ((RegData & MII_CR_RESET) && i++ < 500) {
        RegData = ReadPhyRegister(Adapter,
                                  PHY_MII_CTRL_REG, Adapter->PhyAddress);
        DelayInMicroseconds(1);
    }

    if (i >= 500) {
        DEBUGOUT("Timeout waiting for PHY to reset.\n");
        return FALSE;
    }

    return TRUE;
}

BOOLEAN PhySetup(IN PADAPTER_STRUCT Adapter, UINT32 DeviceControlReg)
 {
    UINT32 ExtDevControlReg;
    UINT32 DeviceCtrlReg;
    UINT16 MiiCtrlReg, MiiStatusReg;
    UINT16 MiiAutoNegAdvertiseReg, Mii1000TCtrlReg;
    UINT16 i, AutoNegAdDefault, FlowControlAdDefault, Data;
    UINT16 AutoNegHwSetting;
    BOOLEAN Status = FALSE;
    BOOLEAN RestartAutoNeg = FALSE;

    DEBUGFUNC("PhySetup")

        ASSERT(Adapter->MacType >= MAC_LIVENGOOD);

    DeviceControlReg |=
        (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX | E1000_CTRL_SLU);

    E1000_WRITE_REG(Ctrl, DeviceControlReg);

    PhyHardwareReset(Adapter);

    Adapter->PhyAddress = AutoDetectGigabitPhy(Adapter);

    if (Adapter->PhyAddress > MAX_PHY_REG_ADDRESS) {

        Status = FALSE;
    }

    DEBUGOUT1("Phy ID = %x \n", Adapter->PhyId);

    MiiCtrlReg = ReadPhyRegister(Adapter,
                                 PHY_MII_CTRL_REG, Adapter->PhyAddress);

    DEBUGOUT1("MII Ctrl Reg contents = %x\n", MiiCtrlReg);

    MiiCtrlReg &= ~(MII_CR_ISOLATE);

    WritePhyRegister(Adapter,
                     PHY_MII_CTRL_REG, Adapter->PhyAddress, MiiCtrlReg);

    Data = ReadPhyRegister(Adapter,
                           PXN_PHY_SPEC_CTRL_REG, Adapter->PhyAddress);

    Data |= PXN_PSCR_ASSERT_CRS_ON_TX;

    DEBUGOUT1("Paxson PSCR: %x \n", Data);

    WritePhyRegister(Adapter,
                     PXN_PHY_SPEC_CTRL_REG, Adapter->PhyAddress, Data);

    Data = ReadPhyRegister(Adapter,
                           PXN_EXT_PHY_SPEC_CTRL_REG, Adapter->PhyAddress);

    Data |= PXN_EPSCR_TX_CLK_25;

    WritePhyRegister(Adapter,
                     PXN_EXT_PHY_SPEC_CTRL_REG, Adapter->PhyAddress, Data);

    MiiAutoNegAdvertiseReg = ReadPhyRegister(Adapter,
                                             PHY_AUTONEG_ADVERTISEMENT,
                                             Adapter->PhyAddress);

    AutoNegHwSetting = (MiiAutoNegAdvertiseReg >> 5) & 0xF;

    Mii1000TCtrlReg = ReadPhyRegister(Adapter,
                                      PHY_1000T_CTRL_REG,
                                      Adapter->PhyAddress);

    AutoNegHwSetting |= ((Mii1000TCtrlReg & 0x0300) >> 4);

    MiiAutoNegAdvertiseReg = ((MiiAutoNegAdvertiseReg & 0x0C00) >> 10);

    Adapter->AutoNegAdvertised &= AUTONEG_ADVERTISE_SPEED_DEFAULT;

    if (Adapter->AutoNegAdvertised == 0)
        Adapter->AutoNegAdvertised = AUTONEG_ADVERTISE_SPEED_DEFAULT;

    AutoNegAdDefault = AUTONEG_ADVERTISE_SPEED_DEFAULT;

    if (Adapter->AutoNeg &&
        (Adapter->AutoNegAdvertised == AutoNegHwSetting) &&
        (Adapter->FlowControl == MiiAutoNegAdvertiseReg)) {
        DEBUGOUT("No overrides - Reading MII Status Reg..\n");

        MiiStatusReg = ReadPhyRegister(Adapter,
                                       PHY_MII_STATUS_REG,
                                       Adapter->PhyAddress);

        MiiStatusReg = ReadPhyRegister(Adapter,
                                       PHY_MII_STATUS_REG,
                                       Adapter->PhyAddress);

        DEBUGOUT1("MII Status Reg contents = %x\n", MiiStatusReg);

        if (MiiStatusReg & MII_SR_LINK_STATUS) {
            Data = ReadPhyRegister(Adapter,
                                   PXN_PHY_SPEC_STAT_REG,
                                   Adapter->PhyAddress);
            DEBUGOUT1("Paxson Phy Specific Status Reg contents = %x\n",
                      Data);

            ConfigureMacToPhySettings(Adapter, Data);

            ConfigFlowControlAfterLinkUp(Adapter);
            return (TRUE);
        }
    }

    if (Adapter->AutoNeg) {
        DEBUGOUT
            ("Livengood - Reconfiguring auto-neg advertisement params\n");
        RestartAutoNeg = PhySetupAutoNegAdvertisement(Adapter);
    } else {
        DEBUGOUT("Livengood - Forcing speed and duplex\n");
        PhyForceSpeedAndDuplex(Adapter);
    }

    if (RestartAutoNeg) {
        DEBUGOUT("Restarting Auto-Neg\n");

        MiiCtrlReg = ReadPhyRegister(Adapter,
                                     PHY_MII_CTRL_REG,
                                     Adapter->PhyAddress);

        MiiCtrlReg |= (MII_CR_AUTO_NEG_EN | MII_CR_RESTART_AUTO_NEG);

        WritePhyRegister(Adapter,
                         PHY_MII_CTRL_REG,
                         Adapter->PhyAddress, MiiCtrlReg);

        if (Adapter->WaitAutoNegComplete) {

            DEBUGOUT("Waiting for Auto-Neg to complete.\n");
            MiiStatusReg = 0;

            for (i = PHY_AUTO_NEG_TIME; i > 0; i--) {

                MiiStatusReg = ReadPhyRegister(Adapter,
                                               PHY_MII_STATUS_REG,
                                               Adapter->PhyAddress);

                MiiStatusReg = ReadPhyRegister(Adapter,
                                               PHY_MII_STATUS_REG,
                                               Adapter->PhyAddress);

                if (MiiStatusReg & MII_SR_AUTONEG_COMPLETE)
                    break;

                DelayInMilliseconds(100);
            }
        }
    }

    MiiStatusReg = ReadPhyRegister(Adapter,
                                   PHY_MII_STATUS_REG,
                                   Adapter->PhyAddress);

    MiiStatusReg = ReadPhyRegister(Adapter,
                                   PHY_MII_STATUS_REG,
                                   Adapter->PhyAddress);

    DEBUGOUT1("Checking for link status - MII Status Reg contents = %x\n",
              MiiStatusReg);

    for (i = 0; i < 10; i++) {
        if (MiiStatusReg & MII_SR_LINK_STATUS) {
            break;
        }
        DelayInMicroseconds(10);
        DEBUGOUT(".");

        MiiStatusReg = ReadPhyRegister(Adapter,
                                       PHY_MII_STATUS_REG,
                                       Adapter->PhyAddress);

        MiiStatusReg = ReadPhyRegister(Adapter,
                                       PHY_MII_STATUS_REG,
                                       Adapter->PhyAddress);
    }

    if (MiiStatusReg & MII_SR_LINK_STATUS) {

        Data = ReadPhyRegister(Adapter,
                               PXN_PHY_SPEC_STAT_REG, Adapter->PhyAddress);

        DEBUGOUT1("Paxson Phy Specific Status Reg contents = %x\n", Data);

        ConfigureMacToPhySettings(Adapter, Data);
        DEBUGOUT("Valid link established!!!\n");

        ConfigFlowControlAfterLinkUp(Adapter);
    } else {
        DEBUGOUT("Unable to establish link!!!\n");
    }

    return (TRUE);
}

STATIC BOOLEAN PhySetupAutoNegAdvertisement(IN PADAPTER_STRUCT Adapter)
 {
    UINT16 MiiAutoNegAdvertiseReg, Mii1000TCtrlReg;

    DEBUGFUNC("PhySetupAutoNegAdvertisement")

        MiiAutoNegAdvertiseReg = ReadPhyRegister(Adapter,
                                                 PHY_AUTONEG_ADVERTISEMENT,
                                                 Adapter->PhyAddress);

    Mii1000TCtrlReg = ReadPhyRegister(Adapter,
                                      PHY_1000T_CTRL_REG,
                                      Adapter->PhyAddress);

    MiiAutoNegAdvertiseReg &= ~REG4_SPEED_MASK;
    Mii1000TCtrlReg &= ~REG9_SPEED_MASK;

    DEBUGOUT1("AutoNegAdvertised %x\n", Adapter->AutoNegAdvertised);

    if (Adapter->AutoNegAdvertised & ADVERTISE_10_HALF) {
        DEBUGOUT("Advertise 10mb Half duplex\n");
        MiiAutoNegAdvertiseReg |= NWAY_AR_10T_HD_CAPS;
    }

    if (Adapter->AutoNegAdvertised & ADVERTISE_10_FULL) {
        DEBUGOUT("Advertise 10mb Full duplex\n");
        MiiAutoNegAdvertiseReg |= NWAY_AR_10T_FD_CAPS;
    }

    if (Adapter->AutoNegAdvertised & ADVERTISE_100_HALF) {
        DEBUGOUT("Advertise 100mb Half duplex\n");
        MiiAutoNegAdvertiseReg |= NWAY_AR_100TX_HD_CAPS;
    }

    if (Adapter->AutoNegAdvertised & ADVERTISE_100_FULL) {
        DEBUGOUT("Advertise 100mb Full duplex\n");
        MiiAutoNegAdvertiseReg |= NWAY_AR_100TX_FD_CAPS;
    }

    if (Adapter->AutoNegAdvertised & ADVERTISE_1000_HALF) {
        DEBUGOUT
            ("Advertise 1000mb Half duplex requested, request denied!\n");
    }

    if (Adapter->AutoNegAdvertised & ADVERTISE_1000_FULL) {
        DEBUGOUT("Advertise 1000mb Full duplex\n");
        Mii1000TCtrlReg |= CR_1000T_FD_CAPS;
    }

    switch (Adapter->FlowControl) {
    case FLOW_CONTROL_NONE:

        MiiAutoNegAdvertiseReg &= ~(NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);

        break;

    case FLOW_CONTROL_RECEIVE_PAUSE:

        MiiAutoNegAdvertiseReg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);

        break;

    case FLOW_CONTROL_TRANSMIT_PAUSE:

        MiiAutoNegAdvertiseReg |= NWAY_AR_ASM_DIR;
        MiiAutoNegAdvertiseReg &= ~NWAY_AR_PAUSE;

        break;

    case FLOW_CONTROL_FULL:

        MiiAutoNegAdvertiseReg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);

        break;

    default:

        DEBUGOUT("Flow control param set incorrectly\n");
        ASSERT(0);
        break;
    }

    WritePhyRegister(Adapter,
                     PHY_AUTONEG_ADVERTISEMENT,
                     Adapter->PhyAddress, MiiAutoNegAdvertiseReg);

    DEBUGOUT1("Auto-Neg Advertising %x\n", MiiAutoNegAdvertiseReg);

    WritePhyRegister(Adapter,
                     PHY_1000T_CTRL_REG,
                     Adapter->PhyAddress, Mii1000TCtrlReg);
    return (TRUE);
}

STATIC VOID PhyForceSpeedAndDuplex(IN PADAPTER_STRUCT Adapter)
 {
    UINT16 MiiCtrlReg;
    UINT16 MiiStatusReg;
    UINT16 PhyData;
    UINT16 i;
    UINT32 TctlReg;
    UINT32 DeviceCtrlReg;

    DEBUGFUNC("PhyForceSpeedAndDuplex")

        Adapter->FlowControl = FLOW_CONTROL_NONE;

    DEBUGOUT1("Adapter->FlowControl = %d\n", Adapter->FlowControl);

    DeviceCtrlReg = E1000_READ_REG(Ctrl);

    DeviceCtrlReg |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
    DeviceCtrlReg &= ~(DEVICE_SPEED_MASK);

    DeviceCtrlReg &= ~E1000_CTRL_ASDE;

    MiiCtrlReg = ReadPhyRegister(Adapter,
                                 PHY_MII_CTRL_REG, Adapter->PhyAddress);

    MiiCtrlReg &= ~MII_CR_AUTO_NEG_EN;

    if (Adapter->ForcedSpeedDuplex == FULL_100 ||
        Adapter->ForcedSpeedDuplex == FULL_10) {

        DeviceCtrlReg |= E1000_CTRL_FD;
        MiiCtrlReg |= MII_CR_FULL_DUPLEX;

        DEBUGOUT("Full Duplex\n");
    } else {

        DeviceCtrlReg &= ~E1000_CTRL_FD;
        MiiCtrlReg &= ~MII_CR_FULL_DUPLEX;

        DEBUGOUT("Half Duplex\n");
    }

    if (Adapter->ForcedSpeedDuplex == FULL_100 ||
        Adapter->ForcedSpeedDuplex == HALF_100) {

        DeviceCtrlReg |= E1000_CTRL_SPD_100;
        MiiCtrlReg |= MII_CR_SPEED_100;
        MiiCtrlReg &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_10);

        DEBUGOUT("Forcing 100mb ");
    } else {

        DeviceCtrlReg &= ~(E1000_CTRL_SPD_1000 | E1000_CTRL_SPD_100);
        MiiCtrlReg |= MII_CR_SPEED_10;
        MiiCtrlReg &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_100);

        DEBUGOUT("Forcing 10mb ");
    }

    TctlReg = E1000_READ_REG(Tctl);
    DEBUGOUT1("TctlReg = %x\n", TctlReg);

    if (!(MiiCtrlReg & MII_CR_FULL_DUPLEX)) {

        TctlReg &= ~E1000_TCTL_COLD;
        TctlReg |= (E1000_HDX_COLLISION_DISTANCE << E1000_COLD_SHIFT);
    } else {

        TctlReg &= ~E1000_TCTL_COLD;
        TctlReg |= (E1000_FDX_COLLISION_DISTANCE << E1000_COLD_SHIFT);
    }

    E1000_WRITE_REG(Tctl, TctlReg);

    E1000_WRITE_REG(Ctrl, DeviceCtrlReg);

    PhyData = ReadPhyRegister(Adapter,
                              PXN_PHY_SPEC_CTRL_REG, Adapter->PhyAddress);

    PhyData &= ~PXN_PSCR_AUTO_X_MODE;

    WritePhyRegister(Adapter,
                     PXN_PHY_SPEC_CTRL_REG, Adapter->PhyAddress, PhyData);

    DEBUGOUT1("Paxson PSCR: %x \n", PhyData);

    MiiCtrlReg |= MII_CR_RESET;

    WritePhyRegister(Adapter,
                     PHY_MII_CTRL_REG, Adapter->PhyAddress, MiiCtrlReg);

    if (Adapter->WaitAutoNegComplete) {

        DEBUGOUT("Waiting for forced speed/duplex link.\n");
        MiiStatusReg = 0;

#define PHY_WAIT_FOR_FORCED_TIME    20

        for (i = 20; i > 0; i--) {

            MiiStatusReg = ReadPhyRegister(Adapter,
                                           PHY_MII_STATUS_REG,
                                           Adapter->PhyAddress);

            MiiStatusReg = ReadPhyRegister(Adapter,
                                           PHY_MII_STATUS_REG,
                                           Adapter->PhyAddress);

            if (MiiStatusReg & MII_SR_LINK_STATUS) {
                break;
            }
            DelayInMilliseconds(100);
        }

        if (i == 0) {

            PxnPhyResetDsp(Adapter);
        }

        for (i = 20; i > 0; i--) {
            if (MiiStatusReg & MII_SR_LINK_STATUS) {
                break;
            }

            DelayInMilliseconds(100);

            MiiStatusReg = ReadPhyRegister(Adapter,
                                           PHY_MII_STATUS_REG,
                                           Adapter->PhyAddress);

            MiiStatusReg = ReadPhyRegister(Adapter,
                                           PHY_MII_STATUS_REG,
                                           Adapter->PhyAddress);

        }
    }

    PhyData = ReadPhyRegister(Adapter,
                              PXN_EXT_PHY_SPEC_CTRL_REG,
                              Adapter->PhyAddress);

    PhyData |= PXN_EPSCR_TX_CLK_25;

    WritePhyRegister(Adapter,
                     PXN_EXT_PHY_SPEC_CTRL_REG,
                     Adapter->PhyAddress, PhyData);

    PhyData = ReadPhyRegister(Adapter,
                              PXN_PHY_SPEC_CTRL_REG, Adapter->PhyAddress);

    PhyData |= PXN_PSCR_ASSERT_CRS_ON_TX;

    WritePhyRegister(Adapter,
                     PXN_PHY_SPEC_CTRL_REG, Adapter->PhyAddress, PhyData);
    DEBUGOUT1("After force, Paxson Phy Specific Ctrl Reg = %4x\r\n",
              PhyData);

    PxnPhyResetDsp(Adapter);

    return;
}

VOID
ASDEConfigureMacToPhySettings(IN PADAPTER_STRUCT Adapter,
                              IN UINT16 MiiRegisterData)
 {
    UINT32 DeviceCtrlReg, TctlReg;
    UINT16 MiiCtrlReg;

    DEBUGFUNC("ConfigureMacToPhySettings")

        MiiCtrlReg = ReadPhyRegister(Adapter,
                                     PHY_MII_CTRL_REG,
                                     Adapter->PhyAddress);

    DeviceCtrlReg = E1000_READ_REG(Ctrl);

    DEBUGOUT1("MII Register Data = %x\r\n", MiiRegisterData);

    DeviceCtrlReg &= ~E1000_CTRL_ILOS;

    TctlReg = E1000_READ_REG(Tctl);
    DEBUGOUT1("TctlReg = %x\n", TctlReg);

    if (!(MiiCtrlReg & MII_CR_FULL_DUPLEX)) {

        if (MiiCtrlReg & MII_CR_SPEED_1000) {
            TctlReg &= ~E1000_TCTL_COLD;
            TctlReg |=
                (E1000_GB_HDX_COLLISION_DISTANCE << E1000_COLD_SHIFT);

            TctlReg |= E1000_TCTL_PBE;

        } else {
            TctlReg &= ~E1000_TCTL_COLD;
            TctlReg |= (E1000_HDX_COLLISION_DISTANCE << E1000_COLD_SHIFT);
        }
    } else {

        TctlReg &= ~E1000_TCTL_COLD;
        TctlReg |= (E1000_FDX_COLLISION_DISTANCE << E1000_COLD_SHIFT);
    }

    E1000_WRITE_REG(Tctl, TctlReg);

    E1000_WRITE_REG(Ctrl, DeviceCtrlReg);

    return;
}

VOID
ConfigureMacToPhySettings(IN PADAPTER_STRUCT Adapter,
                          IN UINT16 MiiRegisterData)
 {
    UINT32 DeviceCtrlReg, TctlReg;

    DEBUGFUNC("ConfigureMacToPhySettings")

        TctlReg = E1000_READ_REG(Tctl);
    DEBUGOUT1("TctlReg = %x\n", TctlReg);

    DeviceCtrlReg = E1000_READ_REG(Ctrl);

    DeviceCtrlReg |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
    DeviceCtrlReg &= ~(DEVICE_SPEED_MASK);

    DEBUGOUT1("MII Register Data = %x\r\n", MiiRegisterData);

    DeviceCtrlReg &= ~E1000_CTRL_ILOS;

    if (MiiRegisterData & PXN_PSSR_DPLX) {
        DeviceCtrlReg |= E1000_CTRL_FD;

        TctlReg &= ~E1000_TCTL_COLD;
        TctlReg |= (E1000_FDX_COLLISION_DISTANCE << E1000_COLD_SHIFT);
    } else {
        DeviceCtrlReg &= ~E1000_CTRL_FD;

        if ((MiiRegisterData & PXN_PSSR_SPEED) == PXN_PSSR_1000MBS) {
            TctlReg &= ~E1000_TCTL_COLD;
            TctlReg |=
                (E1000_GB_HDX_COLLISION_DISTANCE << E1000_COLD_SHIFT);

            TctlReg |= E1000_TCTL_PBE;

        } else {
            TctlReg &= ~E1000_TCTL_COLD;
            TctlReg |= (E1000_HDX_COLLISION_DISTANCE << E1000_COLD_SHIFT);
        }
    }

    if ((MiiRegisterData & PXN_PSSR_SPEED) == PXN_PSSR_1000MBS)
        DeviceCtrlReg |= E1000_CTRL_SPD_1000;
    else if ((MiiRegisterData & PXN_PSSR_SPEED) == PXN_PSSR_100MBS)
        DeviceCtrlReg |= E1000_CTRL_SPD_100;
    else
        DeviceCtrlReg &= ~(E1000_CTRL_SPD_1000 | E1000_CTRL_SPD_100);

    E1000_WRITE_REG(Tctl, TctlReg);

    E1000_WRITE_REG(Ctrl, DeviceCtrlReg);

    return;
}

VOID DisplayMiiContents(IN PADAPTER_STRUCT Adapter, IN UINT8 PhyAddress)
 {
    UINT16 Data, PhyIDHi, PhyIDLo;
    UINT32 PhyID;

    DEBUGFUNC("DisplayMiiContents")

        DEBUGOUT1("Adapter Base Address = %x\n",
                  Adapter->HardwareVirtualAddress);

    Data = ReadPhyRegister(Adapter, PHY_MII_CTRL_REG, PhyAddress);

    DEBUGOUT1("MII Ctrl Reg contents = %x\n", Data);

    Data = ReadPhyRegister(Adapter, PHY_MII_STATUS_REG, PhyAddress);

    Data = ReadPhyRegister(Adapter, PHY_MII_STATUS_REG, PhyAddress);

    DEBUGOUT1("MII Status Reg contents = %x\n", Data);

    PhyIDHi = ReadPhyRegister(Adapter, PHY_PHY_ID_REG1, PhyAddress);

    DelayInMicroseconds(2);

    PhyIDLo = ReadPhyRegister(Adapter, PHY_PHY_ID_REG2, PhyAddress);

    PhyID = (PhyIDLo | (PhyIDHi << 16)) & PHY_REVISION_MASK;

    DEBUGOUT1("Phy ID = %x \n", PhyID);

    Data = ReadPhyRegister(Adapter, PHY_AUTONEG_ADVERTISEMENT, PhyAddress);

    DEBUGOUT1("Reg 4 contents = %x\n", Data);

    Data = ReadPhyRegister(Adapter, PHY_AUTONEG_LP_BPA, PhyAddress);

    DEBUGOUT1("Reg 5 contents = %x\n", Data);

    Data = ReadPhyRegister(Adapter, PHY_AUTONEG_EXPANSION_REG, PhyAddress);

    DEBUGOUT1("Reg 6 contents = %x\n", Data);

    Data = ReadPhyRegister(Adapter, PHY_AUTONEG_NEXT_PAGE_TX, PhyAddress);

    DEBUGOUT1("Reg 7 contents = %x\n", Data);

    Data = ReadPhyRegister(Adapter,
                           PHY_AUTONEG_LP_RX_NEXT_PAGE, PhyAddress);

    DEBUGOUT1("Reg 8 contents = %x\n", Data);

    Data = ReadPhyRegister(Adapter, PHY_1000T_CTRL_REG, PhyAddress);

    DEBUGOUT1("Reg 9 contents = %x\n", Data);

    Data = ReadPhyRegister(Adapter, PHY_1000T_STATUS_REG, PhyAddress);

    DEBUGOUT1("Reg A contents = %x\n", Data);

    Data = ReadPhyRegister(Adapter, PHY_IEEE_EXT_STATUS_REG, PhyAddress);

    DEBUGOUT1("Reg F contents = %x\n", Data);

    Data = ReadPhyRegister(Adapter, PXN_PHY_SPEC_CTRL_REG, PhyAddress);

    DEBUGOUT1("Paxson Specific Control Reg (0x10) = %x\n", Data);

    Data = ReadPhyRegister(Adapter, PXN_PHY_SPEC_STAT_REG, PhyAddress);

    DEBUGOUT1("Paxson Specific Status Reg (0x11) = %x\n", Data);

    Data = ReadPhyRegister(Adapter, PXN_INT_ENABLE_REG, PhyAddress);

    DEBUGOUT1("Paxson Interrupt Enable Reg (0x12) = %x\n", Data);

    Data = ReadPhyRegister(Adapter, PXN_INT_STATUS_REG, PhyAddress);

    DEBUGOUT1("Paxson Interrupt Status Reg (0x13) = %x\n", Data);

    Data = ReadPhyRegister(Adapter, PXN_EXT_PHY_SPEC_CTRL_REG, PhyAddress);

    DEBUGOUT1("Paxson Ext. Phy Specific Control (0x14) = %x\n", Data);

    Data = ReadPhyRegister(Adapter, PXN_RX_ERROR_COUNTER, PhyAddress);

    DEBUGOUT1("Paxson Receive Error Counter (0x15) = %x\n", Data);

    Data = ReadPhyRegister(Adapter, PXN_LED_CTRL_REG, PhyAddress);

    DEBUGOUT1("Paxson LED control reg (0x18) = %x\n", Data);
}

UINT32 AutoDetectGigabitPhy(IN PADAPTER_STRUCT Adapter)
{
    UINT32 PhyAddress = 0;
    UINT32 PhyIDHi;
    UINT16 PhyIDLo;
    BOOLEAN GotOne = FALSE;

    DEBUGFUNC("AutoDetectGigabitPhy")

        while ((!GotOne) && (PhyAddress <= MAX_PHY_REG_ADDRESS)) {

        PhyIDHi = ReadPhyRegister(Adapter, PHY_PHY_ID_REG1, PhyAddress);

        DelayInMicroseconds(2);

        PhyIDLo = ReadPhyRegister(Adapter, PHY_PHY_ID_REG2, PhyAddress);

        Adapter->PhyId = (PhyIDLo | (PhyIDHi << 16)) & PHY_REVISION_MASK;

        if (Adapter->PhyId == PAXSON_PHY_88E1000 ||
            Adapter->PhyId == PAXSON_PHY_88E1000S) {
            DEBUGOUT2("PhyId 0x%x detected at address 0x%x\n",
                      Adapter->PhyId, PhyAddress);

            GotOne = TRUE;
        } else {
            PhyAddress++;
        }

    }

    if (PhyAddress > MAX_PHY_REG_ADDRESS) {
        DEBUGOUT("Could not auto-detect Phy!\n");
    }

    return (PhyAddress);
}

VOID PxnPhyResetDsp(IN PADAPTER_STRUCT Adapter)
{
    WritePhyRegister(Adapter, 29, Adapter->PhyAddress, 0x1d);
    WritePhyRegister(Adapter, 30, Adapter->PhyAddress, 0xc1);
    WritePhyRegister(Adapter, 30, Adapter->PhyAddress, 0x00);
}
