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

#define IN
#define OUT

VOID AdapterStop(PADAPTER_STRUCT Adapter);

BOOLEAN InitializeHardware(PADAPTER_STRUCT Adapter);

VOID InitRxAddresses(PADAPTER_STRUCT Adapter);

BOOLEAN SetupFlowControlAndLink(PADAPTER_STRUCT Adapter);

BOOLEAN SetupPcsLink(PADAPTER_STRUCT Adapter, UINT32 DeviceControlReg);

VOID ConfigFlowControlAfterLinkUp(PADAPTER_STRUCT Adapter);

VOID ForceMacFlowControlSetting(PADAPTER_STRUCT Adapter);

VOID CheckForLink(PADAPTER_STRUCT Adapter);

VOID ClearHwStatsCounters(PADAPTER_STRUCT Adapter);

VOID
GetSpeedAndDuplex(PADAPTER_STRUCT Adapter, PUINT16 Speed, PUINT16 Duplex);

UINT16 ReadEepromWord(PADAPTER_STRUCT Adapter, UINT16 Reg);

STATIC
    VOID ShiftOutBits(PADAPTER_STRUCT Adapter, UINT16 Data, UINT16 Count);

STATIC VOID RaiseClock(PADAPTER_STRUCT Adapter, UINT32 * EecdRegValue);

STATIC VOID LowerClock(PADAPTER_STRUCT Adapter, UINT32 * EecdRegValue);

STATIC USHORT ShiftInBits(PADAPTER_STRUCT Adapter);

STATIC VOID EepromCleanup(PADAPTER_STRUCT Adapter);

BOOLEAN ValidateEepromChecksum(PADAPTER_STRUCT Adapter);

VOID UpdateEepromChecksum(PADAPTER_STRUCT Adapter);

BOOLEAN WriteEepromWord(PADAPTER_STRUCT Adapter, USHORT reg, USHORT data);

STATIC UINT16 WaitEepromCommandDone(PADAPTER_STRUCT Adapter);

STATIC VOID StandBy(PADAPTER_STRUCT Adapter);

BOOLEAN ReadPartNumber(PADAPTER_STRUCT Adapter, PUINT PartNumber);

VOID IdLedOn(PADAPTER_STRUCT Adapter);

VOID IdLedOff(PADAPTER_STRUCT Adapter);

VOID AdapterStop(PADAPTER_STRUCT Adapter)
{
    UINT32 CtrlContents;

    UINT32 IcrContents;

    UINT16 PciCommandWord;

    DEBUGFUNC("AdapterStop")

        if (Adapter->AdapterStopped) {
        DEBUGOUT("Exiting because the adapter is already stopped!!!\n");
        return;
    }

    Adapter->AdapterStopped = TRUE;

    if (Adapter->MacType == MAC_WISEMAN_2_0) {
        if (Adapter->PciCommandWord & CMD_MEM_WRT_INVALIDATE) {
            DEBUGOUT("Disabling MWI on rev 2.0 Wiseman silicon\n");

            PciCommandWord =
                Adapter->PciCommandWord & ~CMD_MEM_WRT_INVALIDATE;

            WritePciConfigWord(PCI_COMMAND_REGISTER, &PciCommandWord);
        }
    }

    DEBUGOUT("Masking off all interrupts\n");
    E1000_WRITE_REG(Imc, 0xffffffff);

    E1000_WRITE_REG(Rctl, 0);
    E1000_WRITE_REG(Tctl, 0);

    DelayInMilliseconds(10);

    DEBUGOUT("Issuing a global reset to MAC\n");
    E1000_WRITE_REG(Ctrl, E1000_CTRL_RST);

    DelayInMilliseconds(10);

    DEBUGOUT("Masking off all interrupts\n");
    E1000_WRITE_REG(Imc, 0xffffffff);

    IcrContents = E1000_READ_REG(Icr);

    if (Adapter->MacType == MAC_WISEMAN_2_0) {
        if (Adapter->PciCommandWord & CMD_MEM_WRT_INVALIDATE) {
            WritePciConfigWord(PCI_COMMAND_REGISTER,
                               &Adapter->PciCommandWord);
        }
    }
}

BOOLEAN InitializeHardware(PADAPTER_STRUCT Adapter)
 {
    UINT i;
    UINT16 PciCommandWord;
    BOOLEAN Status;
    UINT32 DeviceStatusReg;

    DEBUGFUNC("InitializeHardware");

    if (Adapter->MacType >= MAC_LIVENGOOD) {
        DeviceStatusReg = E1000_READ_REG(Status);
        if (DeviceStatusReg & E1000_STATUS_TBIMODE) {
            Adapter->MediaType = MEDIA_TYPE_FIBER;
        } else {

            Adapter->MediaType = MEDIA_TYPE_COPPER;

        }
    } else {

        Adapter->MediaType = MEDIA_TYPE_FIBER;
    }

    DEBUGOUT("Initializing the IEEE VLAN\n");
    E1000_WRITE_REG(Vet, 0);

    for (i = 0; i < E1000_VLAN_FILTER_TBL_SIZE; i++) {
        E1000_WRITE_REG(Vfta[i], 0);
    }

    if (Adapter->MacType == MAC_WISEMAN_2_0) {
        if (Adapter->PciCommandWord & CMD_MEM_WRT_INVALIDATE) {
            DEBUGOUT("Disabling MWI on rev 2.0 Wiseman silicon\n");

            PciCommandWord =
                Adapter->PciCommandWord & ~CMD_MEM_WRT_INVALIDATE;

            WritePciConfigWord(PCI_COMMAND_REGISTER, &PciCommandWord);
        }

        E1000_WRITE_REG(Rctl, E1000_RCTL_RST);

        DelayInMilliseconds(5);
    }

    InitRxAddresses(Adapter);

    if (Adapter->MacType == MAC_WISEMAN_2_0) {
        E1000_WRITE_REG(Rctl, 0);

        DelayInMilliseconds(1);

        if (Adapter->PciCommandWord & CMD_MEM_WRT_INVALIDATE) {
            WritePciConfigWord(PCI_COMMAND_REGISTER,
                               &Adapter->PciCommandWord);
        }
    }

    DEBUGOUT("Zeroing the MTA\n");
    for (i = 0; i < E1000_MC_TBL_SIZE; i++) {
        E1000_WRITE_REG(Mta[i], 0);
    }

    Status = SetupFlowControlAndLink(Adapter);

    ClearHwStatsCounters(Adapter);

    return (Status);
}

VOID InitRxAddresses(PADAPTER_STRUCT Adapter)
 {
    UINT i;
    UINT32 HwLowAddress;
    UINT32 HwHighAddress;

    DEBUGFUNC("InitRxAddresses")

        DEBUGOUT("Programming IA into RAR[0]\n");
    HwLowAddress = (Adapter->CurrentNetAddress[0] |
                    (Adapter->CurrentNetAddress[1] << 8) |
                    (Adapter->CurrentNetAddress[2] << 16) |
                    (Adapter->CurrentNetAddress[3] << 24));

    HwHighAddress = (Adapter->CurrentNetAddress[4] |
                     (Adapter->CurrentNetAddress[5] << 8) | E1000_RAH_AV);

    E1000_WRITE_REG(Rar[0].Low, HwLowAddress);
    E1000_WRITE_REG(Rar[0].High, HwHighAddress);

    DEBUGOUT("Clearing RAR[1-15]\n");
    for (i = 1; i < E1000_RAR_ENTRIES; i++) {
        E1000_WRITE_REG(Rar[i].Low, 0);
        E1000_WRITE_REG(Rar[i].High, 0);
    }

    return;
}

BOOLEAN SetupFlowControlAndLink(PADAPTER_STRUCT Adapter)
 {
    UINT32 TempEepromWord;
    UINT32 DeviceControlReg;
    UINT32 DeviceStatusReg;
    UINT32 ExtDevControlReg;
    BOOLEAN Status = TRUE;

    DEBUGFUNC("SetupFlowControlAndLink")

        TempEepromWord = ReadEepromWord(Adapter, EEPROM_INIT_CONTROL1_REG);

    DeviceControlReg =
        (((TempEepromWord & EEPROM_WORD0A_SWDPIO) << SWDPIO_SHIFT) |
         ((TempEepromWord & EEPROM_WORD0A_ILOS) << ILOS_SHIFT));

    if (Adapter->DmaFairness)
        DeviceControlReg |= E1000_CTRL_PRIOR;

    TempEepromWord = ReadEepromWord(Adapter, EEPROM_INIT_CONTROL2_REG);

    if (Adapter->FlowControl > FLOW_CONTROL_FULL) {
        if ((TempEepromWord & EEPROM_WORD0F_PAUSE_MASK) == 0)
            Adapter->FlowControl = FLOW_CONTROL_NONE;
        else if ((TempEepromWord & EEPROM_WORD0F_PAUSE_MASK) ==
                 EEPROM_WORD0F_ASM_DIR) Adapter->FlowControl =
                FLOW_CONTROL_TRANSMIT_PAUSE;
        else
            Adapter->FlowControl = FLOW_CONTROL_FULL;
    }

    Adapter->OriginalFlowControl = Adapter->FlowControl;

    if (Adapter->MacType == MAC_WISEMAN_2_0)
        Adapter->FlowControl &= (~FLOW_CONTROL_TRANSMIT_PAUSE);

    if ((Adapter->MacType < MAC_LIVENGOOD)
        && (Adapter->ReportTxEarly == 1)) Adapter->FlowControl &=
            (~FLOW_CONTROL_RECEIVE_PAUSE);

    DEBUGOUT1("After fix-ups FlowControl is now = %x\n",
              Adapter->FlowControl);

    if (Adapter->MacType >= MAC_LIVENGOOD) {
        ExtDevControlReg = ((TempEepromWord & EEPROM_WORD0F_SWPDIO_EXT)
                            << SWDPIO__EXT_SHIFT);
        E1000_WRITE_REG(Exct, ExtDevControlReg);
    }

    if (Adapter->MacType >= MAC_LIVENGOOD) {
        if (Adapter->MediaType == MEDIA_TYPE_FIBER) {
            Status = SetupPcsLink(Adapter, DeviceControlReg);
        }

        else {

            Status = PhySetup(Adapter, DeviceControlReg);
        }

    } else {
        Status = SetupPcsLink(Adapter, DeviceControlReg);
    }

    DEBUGOUT
        ("Initializing the Flow Control address, type and timer regs\n");

    E1000_WRITE_REG(Fcal, FLOW_CONTROL_ADDRESS_LOW);
    E1000_WRITE_REG(Fcah, FLOW_CONTROL_ADDRESS_HIGH);
    E1000_WRITE_REG(Fct, FLOW_CONTROL_TYPE);
    E1000_WRITE_REG(Fcttv, FC_DEFAULT_TX_TIMER);

    if (!(Adapter->FlowControl & FLOW_CONTROL_TRANSMIT_PAUSE)) {
        E1000_WRITE_REG(Fcrtl, 0);
        E1000_WRITE_REG(Fcrth, 0);
    } else {

        E1000_WRITE_REG(Fcrtl, (FC_DEFAULT_LO_THRESH | E1000_FCRTL_XONE));
        E1000_WRITE_REG(Fcrth, FC_DEFAULT_HI_THRESH);
    }

    return (Status);
}

BOOLEAN SetupPcsLink(PADAPTER_STRUCT Adapter, UINT32 DeviceControlReg)
 {
    UINT32 i;
    UINT32 StatusContents;
    UINT32 TctlReg;
    UINT32 TransmitConfigWord;

    DEBUGFUNC("SetupPcsLink")

        TctlReg = E1000_READ_REG(Tctl);
    TctlReg |= (E1000_FDX_COLLISION_DISTANCE << E1000_COLD_SHIFT);
    E1000_WRITE_REG(Tctl, TctlReg);

    switch (Adapter->FlowControl) {
    case FLOW_CONTROL_NONE:

        TransmitConfigWord = (E1000_TXCW_ANE | E1000_TXCW_FD);

        break;

    case FLOW_CONTROL_RECEIVE_PAUSE:

        TransmitConfigWord =
            (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);

        break;

    case FLOW_CONTROL_TRANSMIT_PAUSE:

        TransmitConfigWord =
            (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_ASM_DIR);

        break;

    case FLOW_CONTROL_FULL:

        TransmitConfigWord =
            (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);

        break;

    default:

        DEBUGOUT("Flow control param set incorrectly\n");
        ASSERT(0);
        break;
    }

    DEBUGOUT("Auto-negotiation enabled\n");

    E1000_WRITE_REG(Txcw, TransmitConfigWord);
    E1000_WRITE_REG(Ctrl, DeviceControlReg);

    Adapter->TxcwRegValue = TransmitConfigWord;
    DelayInMilliseconds(1);

    if (!(E1000_READ_REG(Ctrl) & E1000_CTRL_SWDPIN1)) {

        DEBUGOUT("Looking for Link\n");
        for (i = 0; i < (LINK_UP_TIMEOUT / 10); i++) {
            DelayInMilliseconds(10);

            StatusContents = E1000_READ_REG(Status);
            if (StatusContents & E1000_STATUS_LU)
                break;
        }

        if (i == (LINK_UP_TIMEOUT / 10)) {

            DEBUGOUT("Never got a valid link from auto-neg!!!\n");

            Adapter->AutoNegFailed = 1;
            CheckForLink(Adapter);
            Adapter->AutoNegFailed = 0;
        } else {
            Adapter->AutoNegFailed = 0;
            DEBUGOUT("Valid Link Found\n");
        }
    } else {
        DEBUGOUT("No Signal Detected\n");
    }

    return (TRUE);
}

VOID ConfigFlowControlAfterLinkUp(PADAPTER_STRUCT Adapter)
 {
    UINT16 MiiStatusReg, MiiNWayAdvertiseReg, MiiNWayBasePgAbleReg;
    UINT16 Speed, Duplex;

    DEBUGFUNC("ConfigFlowControlAfterLinkUp")

        if (
            ((Adapter->MediaType == MEDIA_TYPE_FIBER)
             && (Adapter->AutoNegFailed))
            || ((Adapter->MediaType == MEDIA_TYPE_COPPER)
                && (!Adapter->AutoNeg))) {
        ForceMacFlowControlSetting(Adapter);
    }

    if ((Adapter->MediaType == MEDIA_TYPE_COPPER) && Adapter->AutoNeg) {

        MiiStatusReg = ReadPhyRegister(Adapter,
                                       PHY_MII_STATUS_REG,
                                       Adapter->PhyAddress);

        MiiStatusReg = ReadPhyRegister(Adapter,
                                       PHY_MII_STATUS_REG,
                                       Adapter->PhyAddress);

        if (MiiStatusReg & MII_SR_AUTONEG_COMPLETE) {

            MiiNWayAdvertiseReg = ReadPhyRegister(Adapter,
                                                  PHY_AUTONEG_ADVERTISEMENT,
                                                  Adapter->PhyAddress);

            MiiNWayBasePgAbleReg = ReadPhyRegister(Adapter,
                                                   PHY_AUTONEG_LP_BPA,
                                                   Adapter->PhyAddress);

            if ((MiiNWayAdvertiseReg & NWAY_AR_PAUSE) &&
                (MiiNWayBasePgAbleReg & NWAY_LPAR_PAUSE)) {

                if (Adapter->OriginalFlowControl == FLOW_CONTROL_FULL) {
                    Adapter->FlowControl = FLOW_CONTROL_FULL;
                    DEBUGOUT("Flow Control = FULL.\r\n");
                } else {
                    Adapter->FlowControl = FLOW_CONTROL_RECEIVE_PAUSE;
                    DEBUGOUT("Flow Control = RX PAUSE frames only.\r\n");
                }
            }

            else if (!(MiiNWayAdvertiseReg & NWAY_AR_PAUSE) &&
                     (MiiNWayAdvertiseReg & NWAY_AR_ASM_DIR) &&
                     (MiiNWayBasePgAbleReg & NWAY_LPAR_PAUSE) &&
                     (MiiNWayBasePgAbleReg & NWAY_LPAR_ASM_DIR)) {
                Adapter->FlowControl = FLOW_CONTROL_TRANSMIT_PAUSE;
                DEBUGOUT("Flow Control = TX PAUSE frames only.\r\n");
            }

            else if ((MiiNWayAdvertiseReg & NWAY_AR_PAUSE) &&
                     (MiiNWayAdvertiseReg & NWAY_AR_ASM_DIR) &&
                     !(MiiNWayBasePgAbleReg & NWAY_LPAR_PAUSE) &&
                     (MiiNWayBasePgAbleReg & NWAY_LPAR_ASM_DIR)) {
                Adapter->FlowControl = FLOW_CONTROL_RECEIVE_PAUSE;
                DEBUGOUT("Flow Control = RX PAUSE frames only.\r\n");
            }

            else if (Adapter->OriginalFlowControl == FLOW_CONTROL_NONE) {
                Adapter->FlowControl = FLOW_CONTROL_NONE;
                DEBUGOUT("Flow Control = NONE.\r\n");
            } else {
                Adapter->FlowControl = FLOW_CONTROL_RECEIVE_PAUSE;
                DEBUGOUT("Flow Control = RX PAUSE frames only.\r\n");
            }

            GetSpeedAndDuplex(Adapter, &Speed, &Duplex);

            if (Duplex == HALF_DUPLEX)
                Adapter->FlowControl = FLOW_CONTROL_NONE;

            ForceMacFlowControlSetting(Adapter);
        } else {
            DEBUGOUT("Copper PHY and Auto Neg has not completed.\r\n");
        }
    }

}

VOID ForceMacFlowControlSetting(PADAPTER_STRUCT Adapter)
 {
    UINT32 CtrlRegValue;

    DEBUGFUNC("ForceMacFlowControlSetting")

        CtrlRegValue = E1000_READ_REG(Ctrl);

    switch (Adapter->FlowControl) {
    case FLOW_CONTROL_NONE:

        CtrlRegValue &= (~(E1000_CTRL_TFCE | E1000_CTRL_RFCE));
        break;

    case FLOW_CONTROL_RECEIVE_PAUSE:

        CtrlRegValue &= (~E1000_CTRL_TFCE);
        CtrlRegValue |= E1000_CTRL_RFCE;
        break;

    case FLOW_CONTROL_TRANSMIT_PAUSE:

        CtrlRegValue &= (~E1000_CTRL_RFCE);
        CtrlRegValue |= E1000_CTRL_TFCE;
        break;

    case FLOW_CONTROL_FULL:

        CtrlRegValue |= (E1000_CTRL_TFCE | E1000_CTRL_RFCE);
        break;

    default:

        DEBUGOUT("Flow control param set incorrectly\n");
        ASSERT(0);

        break;
    }

    if (Adapter->MacType == MAC_WISEMAN_2_0)
        CtrlRegValue &= (~E1000_CTRL_TFCE);

    E1000_WRITE_REG(Ctrl, CtrlRegValue);
}

VOID CheckForLink(PADAPTER_STRUCT Adapter)
 {
    UINT32 RxcwRegValue;
    UINT32 CtrlRegValue;
    UINT32 StatusRegValue;
    UINT16 PhyData;

    DEBUGFUNC("CheckForLink")

        CtrlRegValue = E1000_READ_REG(Ctrl);

    StatusRegValue = E1000_READ_REG(Status);

    RxcwRegValue = E1000_READ_REG(Rxcw);

    if (Adapter->MediaType == MEDIA_TYPE_COPPER && Adapter->GetLinkStatus) {

        PhyData = ReadPhyRegister(Adapter,
                                  PHY_MII_STATUS_REG, Adapter->PhyAddress);

        PhyData = ReadPhyRegister(Adapter,
                                  PHY_MII_STATUS_REG, Adapter->PhyAddress);

        if (PhyData & MII_SR_LINK_STATUS)
            Adapter->GetLinkStatus = FALSE;
        else {
            DEBUGOUT("**** CFL - No link detected. ****\r\n");
            return;
        }

        if (!Adapter->AutoNeg)
            return;

        switch (Adapter->PhyId) {
        case PAXSON_PHY_88E1000:
        case PAXSON_PHY_88E1000S:

            PhyData = ReadPhyRegister(Adapter,
                                      PXN_PHY_SPEC_STAT_REG,
                                      Adapter->PhyAddress);

            DEBUGOUT1("CFL - Auto-Neg complete.  PhyData = %x\r\n",
                      PhyData);
            ConfigureMacToPhySettings(Adapter, PhyData);

            ConfigFlowControlAfterLinkUp(Adapter);
            break;

        default:
            DEBUGOUT("CFL - Invalid PHY detected.\r\n");

        }
    }

    else

         if ((Adapter->MediaType == MEDIA_TYPE_FIBER) &&
             (!(StatusRegValue & E1000_STATUS_LU)) &&
             (!(CtrlRegValue & E1000_CTRL_SWDPIN1)) &&
             (!(RxcwRegValue & E1000_RXCW_C))) {
        if (Adapter->AutoNegFailed == 0) {
            Adapter->AutoNegFailed = 1;
            return;
        }

        DEBUGOUT("NOT RXing /C/, disable AutoNeg and force link.\r\n");

        E1000_WRITE_REG(Txcw, (Adapter->TxcwRegValue & ~E1000_TXCW_ANE));

        CtrlRegValue = E1000_READ_REG(Ctrl);
        CtrlRegValue |= (E1000_CTRL_SLU | E1000_CTRL_FD);
        E1000_WRITE_REG(Ctrl, CtrlRegValue);

        ConfigFlowControlAfterLinkUp(Adapter);

    }
        else if ((Adapter->MediaType == MEDIA_TYPE_FIBER) &&
                 (CtrlRegValue & E1000_CTRL_SLU) &&
                 (RxcwRegValue & E1000_RXCW_C)) {

        DEBUGOUT("RXing /C/, enable AutoNeg and stop forcing link.\r\n");

        E1000_WRITE_REG(Txcw, Adapter->TxcwRegValue);

        E1000_WRITE_REG(Ctrl, (CtrlRegValue & ~E1000_CTRL_SLU));
    }

    return;
}

VOID ClearHwStatsCounters(PADAPTER_STRUCT Adapter)
 {
    volatile UINT32 RegisterContents;

    DEBUGFUNC("ClearHwStatsCounters")

        if (Adapter->AdapterStopped) {
        DEBUGOUT("Exiting because the adapter is stopped!!!\n");
        return;
    }

    RegisterContents = E1000_READ_REG(Crcerrs);
    RegisterContents = E1000_READ_REG(Symerrs);
    RegisterContents = E1000_READ_REG(Mpc);
    RegisterContents = E1000_READ_REG(Scc);
    RegisterContents = E1000_READ_REG(Ecol);
    RegisterContents = E1000_READ_REG(Mcc);
    RegisterContents = E1000_READ_REG(Latecol);
    RegisterContents = E1000_READ_REG(Colc);
    RegisterContents = E1000_READ_REG(Dc);
    RegisterContents = E1000_READ_REG(Sec);
    RegisterContents = E1000_READ_REG(Rlec);
    RegisterContents = E1000_READ_REG(Xonrxc);
    RegisterContents = E1000_READ_REG(Xontxc);
    RegisterContents = E1000_READ_REG(Xoffrxc);
    RegisterContents = E1000_READ_REG(Xofftxc);
    RegisterContents = E1000_READ_REG(Fcruc);
    RegisterContents = E1000_READ_REG(Prc64);
    RegisterContents = E1000_READ_REG(Prc127);
    RegisterContents = E1000_READ_REG(Prc255);
    RegisterContents = E1000_READ_REG(Prc511);
    RegisterContents = E1000_READ_REG(Prc1023);
    RegisterContents = E1000_READ_REG(Prc1522);
    RegisterContents = E1000_READ_REG(Gprc);
    RegisterContents = E1000_READ_REG(Bprc);
    RegisterContents = E1000_READ_REG(Mprc);
    RegisterContents = E1000_READ_REG(Gptc);
    RegisterContents = E1000_READ_REG(Gorl);
    RegisterContents = E1000_READ_REG(Gorh);
    RegisterContents = E1000_READ_REG(Gotl);
    RegisterContents = E1000_READ_REG(Goth);
    RegisterContents = E1000_READ_REG(Rnbc);
    RegisterContents = E1000_READ_REG(Ruc);
    RegisterContents = E1000_READ_REG(Rfc);
    RegisterContents = E1000_READ_REG(Roc);
    RegisterContents = E1000_READ_REG(Rjc);
    RegisterContents = E1000_READ_REG(Torl);
    RegisterContents = E1000_READ_REG(Torh);
    RegisterContents = E1000_READ_REG(Totl);
    RegisterContents = E1000_READ_REG(Toth);
    RegisterContents = E1000_READ_REG(Tpr);
    RegisterContents = E1000_READ_REG(Tpt);
    RegisterContents = E1000_READ_REG(Ptc64);
    RegisterContents = E1000_READ_REG(Ptc127);
    RegisterContents = E1000_READ_REG(Ptc255);
    RegisterContents = E1000_READ_REG(Ptc511);
    RegisterContents = E1000_READ_REG(Ptc1023);
    RegisterContents = E1000_READ_REG(Ptc1522);
    RegisterContents = E1000_READ_REG(Mptc);
    RegisterContents = E1000_READ_REG(Bptc);

    if (Adapter->MacType < MAC_LIVENGOOD)
        return;

    RegisterContents = E1000_READ_REG(Algnerrc);
    RegisterContents = E1000_READ_REG(Rxerrc);
    RegisterContents = E1000_READ_REG(Tuc);
    RegisterContents = E1000_READ_REG(Tncrs);
    RegisterContents = E1000_READ_REG(Cexterr);
    RegisterContents = E1000_READ_REG(Rutec);

    RegisterContents = E1000_READ_REG(Tsctc);
    RegisterContents = E1000_READ_REG(Tsctfc);

}

VOID
GetSpeedAndDuplex(PADAPTER_STRUCT Adapter, PUINT16 Speed, PUINT16 Duplex)
 {
    UINT32 DeviceStatusReg;
    UINT16 PhyData;

    DEBUGFUNC("GetSpeedAndDuplex")

        if (Adapter->AdapterStopped) {
        *Speed = 0;
        *Duplex = 0;
        return;
    }

    if (Adapter->MacType >= MAC_LIVENGOOD) {
        DEBUGOUT("Livengood MAC\n");
        DeviceStatusReg = E1000_READ_REG(Status);
        if (DeviceStatusReg & E1000_STATUS_SPEED_1000) {
            *Speed = SPEED_1000;
            DEBUGOUT("   1000 Mbs\n");
        } else if (DeviceStatusReg & E1000_STATUS_SPEED_100) {
            *Speed = SPEED_100;
            DEBUGOUT("   100 Mbs\n");
        } else {
            *Speed = SPEED_10;
            DEBUGOUT("   10 Mbs\n");
        }

        if (DeviceStatusReg & E1000_STATUS_FD) {
            *Duplex = FULL_DUPLEX;
            DEBUGOUT("   Full Duplex\r\n");
        } else {
            *Duplex = HALF_DUPLEX;
            DEBUGOUT("   Half Duplex\r\n");
        }
    } else {
        DEBUGOUT("Wiseman MAC - 1000 Mbs, Full Duplex\r\n");
        *Speed = SPEED_1000;
        *Duplex = FULL_DUPLEX;
    }

    return;
}

UINT16 ReadEepromWord(PADAPTER_STRUCT Adapter, UINT16 Reg)
 {
    UINT16 Data;

    ASSERT(Reg < EEPROM_WORD_SIZE);

    E1000_WRITE_REG(Eecd, E1000_EECS);

    ShiftOutBits(Adapter, EEPROM_READ_OPCODE, 3);
    ShiftOutBits(Adapter, Reg, 6);

    Data = ShiftInBits(Adapter);

    EepromCleanup(Adapter);
    return (Data);
}

STATIC
    VOID ShiftOutBits(PADAPTER_STRUCT Adapter, UINT16 Data, UINT16 Count)
 {
    UINT32 EecdRegValue;
    UINT32 Mask;

    Mask = 0x01 << (Count - 1);

    EecdRegValue = E1000_READ_REG(Eecd);

    EecdRegValue &= ~(E1000_EEDO | E1000_EEDI);

    do {

        EecdRegValue &= ~E1000_EEDI;

        if (Data & Mask)
            EecdRegValue |= E1000_EEDI;

        E1000_WRITE_REG(Eecd, EecdRegValue);

        DelayInMicroseconds(50);

        RaiseClock(Adapter, &EecdRegValue);
        LowerClock(Adapter, &EecdRegValue);

        Mask = Mask >> 1;

    } while (Mask);

    EecdRegValue &= ~E1000_EEDI;

    E1000_WRITE_REG(Eecd, EecdRegValue);
}

STATIC VOID RaiseClock(PADAPTER_STRUCT Adapter, UINT32 * EecdRegValue)
 {

    *EecdRegValue = *EecdRegValue | E1000_EESK;

    E1000_WRITE_REG(Eecd, *EecdRegValue);

    DelayInMicroseconds(50);
}

STATIC VOID LowerClock(PADAPTER_STRUCT Adapter, UINT32 * EecdRegValue)
 {

    *EecdRegValue = *EecdRegValue & ~E1000_EESK;

    E1000_WRITE_REG(Eecd, *EecdRegValue);

    DelayInMicroseconds(50);
}

STATIC UINT16 ShiftInBits(PADAPTER_STRUCT Adapter)
 {
    UINT32 EecdRegValue;
    UINT i;
    UINT16 Data;

    EecdRegValue = E1000_READ_REG(Eecd);

    EecdRegValue &= ~(E1000_EEDO | E1000_EEDI);
    Data = 0;

    for (i = 0; i < 16; i++) {
        Data = Data << 1;
        RaiseClock(Adapter, &EecdRegValue);

        EecdRegValue = E1000_READ_REG(Eecd);

        EecdRegValue &= ~(E1000_EEDI);
        if (EecdRegValue & E1000_EEDO)
            Data |= 1;

        LowerClock(Adapter, &EecdRegValue);
    }

    return (Data);
}

STATIC VOID EepromCleanup(PADAPTER_STRUCT Adapter)
 {
    UINT32 EecdRegValue;

    EecdRegValue = E1000_READ_REG(Eecd);

    EecdRegValue &= ~(E1000_EECS | E1000_EEDI);

    E1000_WRITE_REG(Eecd, EecdRegValue);

    RaiseClock(Adapter, &EecdRegValue);
    LowerClock(Adapter, &EecdRegValue);
}

BOOLEAN ValidateEepromChecksum(PADAPTER_STRUCT Adapter)
 {
    UINT16 Checksum = 0;
    UINT16 Iteration;

    for (Iteration = 0; Iteration < (EEPROM_CHECKSUM_REG + 1); Iteration++)
        Checksum += ReadEepromWord(Adapter, Iteration);

    if (Checksum == (UINT16) EEPROM_SUM)
        return (TRUE);
    else
        return (FALSE);
}

VOID UpdateEepromChecksum(PADAPTER_STRUCT Adapter)
 {
    UINT16 Checksum = 0;
    UINT16 Iteration;

    for (Iteration = 0; Iteration < EEPROM_CHECKSUM_REG; Iteration++)
        Checksum += ReadEepromWord(Adapter, Iteration);

    Checksum = (UINT16) EEPROM_SUM - Checksum;

    WriteEepromWord(Adapter, EEPROM_CHECKSUM_REG, Checksum);
}

BOOLEAN WriteEepromWord(PADAPTER_STRUCT Adapter, UINT16 Reg, UINT16 Data)
 {

    E1000_WRITE_REG(Eecd, E1000_EECS);

    ShiftOutBits(Adapter, EEPROM_EWEN_OPCODE, 5);
    ShiftOutBits(Adapter, Reg, 4);

    StandBy(Adapter);

    ShiftOutBits(Adapter, EEPROM_ERASE_OPCODE, 3);
    ShiftOutBits(Adapter, Reg, 6);

    if (!WaitEepromCommandDone(Adapter))
        return (FALSE);

    ShiftOutBits(Adapter, EEPROM_WRITE_OPCODE, 3);
    ShiftOutBits(Adapter, Reg, 6);
    ShiftOutBits(Adapter, Data, 16);

    if (!WaitEepromCommandDone(Adapter))
        return (FALSE);

    ShiftOutBits(Adapter, EEPROM_EWDS_OPCODE, 5);
    ShiftOutBits(Adapter, Reg, 4);

    EepromCleanup(Adapter);

    return (TRUE);
}

STATIC UINT16 WaitEepromCommandDone(PADAPTER_STRUCT Adapter)
 {
    UINT32 EecdRegValue;
    UINT i;

    StandBy(Adapter);

    for (i = 0; i < 200; i++) {
        EecdRegValue = E1000_READ_REG(Eecd);

        if (EecdRegValue & E1000_EEDO)
            return (TRUE);

        DelayInMicroseconds(5);
    }

    return (FALSE);
}

STATIC VOID StandBy(PADAPTER_STRUCT Adapter)
 {
    UINT32 EecdRegValue;

    EecdRegValue = E1000_READ_REG(Eecd);

    EecdRegValue &= ~(E1000_EECS | E1000_EESK);

    E1000_WRITE_REG(Eecd, EecdRegValue);

    DelayInMicroseconds(5);

    EecdRegValue |= E1000_EECS;

    E1000_WRITE_REG(Eecd, EecdRegValue);
}

BOOLEAN ReadPartNumber(PADAPTER_STRUCT Adapter, PUINT PartNumber)
{
    UINT16 EepromWordValue;

    DEBUGFUNC("ReadPartNumber")

        if (Adapter->AdapterStopped) {
        *PartNumber = 0;
        return (FALSE);
    }

    EepromWordValue = ReadEepromWord(Adapter,
                                     (UINT16) (EEPROM_PBA_BYTE_1));

    DEBUGOUT("Read first part number word\n");

    *PartNumber = (UINT32) EepromWordValue;
    *PartNumber = *PartNumber << 16;

    EepromWordValue = ReadEepromWord(Adapter,
                                     (UINT16) (EEPROM_PBA_BYTE_1 + 1));

    DEBUGOUT("Read second part number word\n");

    *PartNumber |= EepromWordValue;

    return (TRUE);

}

VOID IdLedOn(PADAPTER_STRUCT Adapter)
{
    UINT32 CtrlRegValue;

    if (Adapter->AdapterStopped) {
        return;
    }

    CtrlRegValue = E1000_READ_REG(Ctrl);

    CtrlRegValue |= E1000_CTRL_SWDPIO0 | E1000_CTRL_SWDPIN0;
    E1000_WRITE_REG(Ctrl, CtrlRegValue);

}

VOID IdLedOff(PADAPTER_STRUCT Adapter)
{
    UINT32 CtrlRegValue;

    if (Adapter->AdapterStopped) {
        return;
    }

    CtrlRegValue = E1000_READ_REG(Ctrl);

    CtrlRegValue |= E1000_CTRL_SWDPIO0;

    CtrlRegValue &= ~E1000_CTRL_SWDPIN0;

    E1000_WRITE_REG(Ctrl, CtrlRegValue);
}
