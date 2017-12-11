/*
 * Copyright (c) 1996, 2003 VIA Networking Technologies, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * File: card.c
 * Purpose: Provide functions to setup NIC operation mode
 * Functions:
 *      s_vSafeResetTx - Rest Tx
 *      CARDvSetRSPINF - Set RSPINF
 *      vUpdateIFS - Update slotTime,SIFS,DIFS, and EIFS
 *      CARDvUpdateBasicTopRate - Update BasicTopRate
 *      CARDbAddBasicRate - Add to BasicRateSet
 *      CARDbSetBasicRate - Set Basic Tx Rate
 *      CARDbIsOFDMinBasicRate - Check if any OFDM rate is in BasicRateSet
 *      CARDvSetLoopbackMode - Set Loopback mode
 *      CARDbSoftwareReset - Sortware reset NIC
 *      CARDqGetTSFOffset - Caculate TSFOffset
 *      CARDbGetCurrentTSF - Read Current NIC TSF counter
 *      CARDqGetNextTBTT - Caculate Next Beacon TSF counter
 *      CARDvSetFirstNextTBTT - Set NIC Beacon time
 *      CARDvUpdateNextTBTT - Sync. NIC Beacon time
 *      CARDbRadioPowerOff - Turn Off NIC Radio Power
 *      CARDbRadioPowerOn - Turn On NIC Radio Power
 *      CARDbSetWEPMode - Set NIC Wep mode
 *      CARDbSetTxPower - Set NIC tx power
 *
 * Revision History:
 *      06-10-2003 Bryan YC Fan:  Re-write codes to support VT3253 spec.
 *      08-26-2003 Kyle Hsu:      Modify the defination type of dwIoBase.
 *      09-01-2003 Bryan YC Fan:  Add vUpdateIFS().
 *
 */

#include "tmacro.h"
#include "card.h"
#include "baseband.h"
#include "mac.h"
#include "desc.h"
#include "rf.h"
#include "vntwifi.h"
#include "power.h"
#include "key.h"
#include "rc4.h"
#include "country.h"
#include "channel.h"

/*---------------------  Static Definitions -------------------------*/

//static int          msglevel                =MSG_LEVEL_DEBUG;
static int          msglevel                =MSG_LEVEL_INFO;

#define C_SIFS_A        16      // micro sec.
#define C_SIFS_BG       10

#define C_EIFS          80      // micro sec.


#define C_SLOT_SHORT    9       // micro sec.
#define C_SLOT_LONG     20

#define C_CWMIN_A       15      // slot time
#define C_CWMIN_B       31

#define C_CWMAX         1023    // slot time

#define WAIT_BEACON_TX_DOWN_TMO         3    // Times

                                                              //1M,   2M,   5M,  11M,  18M,  24M,  36M,  54M
static BYTE abyDefaultSuppRatesG[] = {WLAN_EID_SUPP_RATES, 8, 0x02, 0x04, 0x0B, 0x16, 0x24, 0x30, 0x48, 0x6C};
                                                                    //6M,   9M,  12M,  48M
static BYTE abyDefaultExtSuppRatesG[] = {WLAN_EID_EXTSUPP_RATES, 4, 0x0C, 0x12, 0x18, 0x60};
                                                              //6M,   9M,  12M,  18M,  24M,  36M,  48M,  54M
static BYTE abyDefaultSuppRatesA[] = {WLAN_EID_SUPP_RATES, 8, 0x0C, 0x12, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6C};
                                                              //1M,   2M,   5M,  11M,
static BYTE abyDefaultSuppRatesB[] = {WLAN_EID_SUPP_RATES, 4, 0x02, 0x04, 0x0B, 0x16};


/*---------------------  Static Variables  --------------------------*/


const WORD cwRXBCNTSFOff[MAX_RATE] =
{17, 17, 17, 17, 34, 23, 17, 11, 8, 5, 4, 3};


/*---------------------  Static Functions  --------------------------*/

static
void
s_vCaculateOFDMRParameter(
    BYTE byRate,
    CARD_PHY_TYPE ePHYType,
    unsigned char *pbyTxRate,
    unsigned char *pbyRsvTime
    );


/*---------------------  Export Functions  --------------------------*/

/*
 * Description: Caculate TxRate and RsvTime fields for RSPINF in OFDM mode.
 *
 * Parameters:
 *  In:
 *      wRate           - Tx Rate
 *      byPktType       - Tx Packet type
 *  Out:
 *      pbyTxRate       - pointer to RSPINF TxRate field
 *      pbyRsvTime      - pointer to RSPINF RsvTime field
 *
 * Return Value: none
 *
 */
static
void
s_vCaculateOFDMRParameter (
    BYTE byRate,
    CARD_PHY_TYPE ePHYType,
    unsigned char *pbyTxRate,
    unsigned char *pbyRsvTime
    )
{
    switch (byRate) {
    case RATE_6M :
        if (ePHYType == PHY_TYPE_11A) {//5GHZ
            *pbyTxRate = 0x9B;
            *pbyRsvTime = 44;
        }
        else {
            *pbyTxRate = 0x8B;
            *pbyRsvTime = 50;
        }
        break;

    case RATE_9M :
        if (ePHYType == PHY_TYPE_11A) {//5GHZ
            *pbyTxRate = 0x9F;
            *pbyRsvTime = 36;
        }
        else {
            *pbyTxRate = 0x8F;
            *pbyRsvTime = 42;
        }
        break;

   case RATE_12M :
        if (ePHYType == PHY_TYPE_11A) {//5GHZ
            *pbyTxRate = 0x9A;
            *pbyRsvTime = 32;
        }
        else {
            *pbyTxRate = 0x8A;
            *pbyRsvTime = 38;
        }
        break;

   case RATE_18M :
        if (ePHYType == PHY_TYPE_11A) {//5GHZ
            *pbyTxRate = 0x9E;
            *pbyRsvTime = 28;
        }
        else {
            *pbyTxRate = 0x8E;
            *pbyRsvTime = 34;
        }
        break;

    case RATE_36M :
        if (ePHYType == PHY_TYPE_11A) {//5GHZ
            *pbyTxRate = 0x9D;
            *pbyRsvTime = 24;
        }
        else {
            *pbyTxRate = 0x8D;
            *pbyRsvTime = 30;
        }
        break;

    case RATE_48M :
        if (ePHYType == PHY_TYPE_11A) {//5GHZ
            *pbyTxRate = 0x98;
            *pbyRsvTime = 24;
        }
        else {
            *pbyTxRate = 0x88;
            *pbyRsvTime = 30;
        }
        break;

    case RATE_54M :
        if (ePHYType == PHY_TYPE_11A) {//5GHZ
            *pbyTxRate = 0x9C;
            *pbyRsvTime = 24;
        }
        else {
            *pbyTxRate = 0x8C;
            *pbyRsvTime = 30;
        }
        break;

    case RATE_24M :
    default :
        if (ePHYType == PHY_TYPE_11A) {//5GHZ
            *pbyTxRate = 0x99;
            *pbyRsvTime = 28;
        }
        else {
            *pbyTxRate = 0x89;
            *pbyRsvTime = 34;
        }
        break;
    }
}



/*
 * Description: Set RSPINF
 *
 * Parameters:
 *  In:
 *      pDevice             - The adapter to be set
 *  Out:
 *      none
 *
 * Return Value: None.
 *
 */
static
void
s_vSetRSPINF (PSDevice pDevice, CARD_PHY_TYPE ePHYType, void *pvSupportRateIEs, void *pvExtSupportRateIEs)
{
    BYTE  byServ = 0, bySignal = 0; // For CCK
    WORD  wLen = 0;
    BYTE  byTxRate = 0, byRsvTime = 0;    // For OFDM

    //Set to Page1
    MACvSelectPage1(pDevice->PortOffset);

    //RSPINF_b_1
    BBvCaculateParameter(pDevice,
                         14,
                         VNTWIFIbyGetACKTxRate(RATE_1M, pvSupportRateIEs, pvExtSupportRateIEs),
                         PK_TYPE_11B,
                         &wLen,
                         &byServ,
                         &bySignal
    );

    VNSvOutPortD(pDevice->PortOffset + MAC_REG_RSPINF_B_1, MAKEDWORD(wLen,MAKEWORD(bySignal,byServ)));
    ///RSPINF_b_2
    BBvCaculateParameter(pDevice,
                         14,
                         VNTWIFIbyGetACKTxRate(RATE_2M, pvSupportRateIEs, pvExtSupportRateIEs),
                         PK_TYPE_11B,
                         &wLen,
                         &byServ,
                         &bySignal
    );

    VNSvOutPortD(pDevice->PortOffset + MAC_REG_RSPINF_B_2, MAKEDWORD(wLen,MAKEWORD(bySignal,byServ)));
    //RSPINF_b_5
    BBvCaculateParameter(pDevice,
                         14,
                         VNTWIFIbyGetACKTxRate(RATE_5M, pvSupportRateIEs, pvExtSupportRateIEs),
                         PK_TYPE_11B,
                         &wLen,
                         &byServ,
                         &bySignal
    );

    VNSvOutPortD(pDevice->PortOffset + MAC_REG_RSPINF_B_5, MAKEDWORD(wLen,MAKEWORD(bySignal,byServ)));
    //RSPINF_b_11
    BBvCaculateParameter(pDevice,
                         14,
                         VNTWIFIbyGetACKTxRate(RATE_11M, pvSupportRateIEs, pvExtSupportRateIEs),
                         PK_TYPE_11B,
                         &wLen,
                         &byServ,
                         &bySignal
    );

    VNSvOutPortD(pDevice->PortOffset + MAC_REG_RSPINF_B_11, MAKEDWORD(wLen,MAKEWORD(bySignal,byServ)));
    //RSPINF_a_6
    s_vCaculateOFDMRParameter(RATE_6M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_6, MAKEWORD(byTxRate,byRsvTime));
    //RSPINF_a_9
    s_vCaculateOFDMRParameter(RATE_9M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_9, MAKEWORD(byTxRate,byRsvTime));
    //RSPINF_a_12
    s_vCaculateOFDMRParameter(RATE_12M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_12, MAKEWORD(byTxRate,byRsvTime));
    //RSPINF_a_18
    s_vCaculateOFDMRParameter(RATE_18M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_18, MAKEWORD(byTxRate,byRsvTime));
    //RSPINF_a_24
    s_vCaculateOFDMRParameter(RATE_24M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_24, MAKEWORD(byTxRate,byRsvTime));
    //RSPINF_a_36
    s_vCaculateOFDMRParameter(
                              VNTWIFIbyGetACKTxRate(RATE_36M, pvSupportRateIEs, pvExtSupportRateIEs),
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_36, MAKEWORD(byTxRate,byRsvTime));
    //RSPINF_a_48
    s_vCaculateOFDMRParameter(
                              VNTWIFIbyGetACKTxRate(RATE_48M, pvSupportRateIEs, pvExtSupportRateIEs),
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_48, MAKEWORD(byTxRate,byRsvTime));
    //RSPINF_a_54
    s_vCaculateOFDMRParameter(
                              VNTWIFIbyGetACKTxRate(RATE_54M, pvSupportRateIEs, pvExtSupportRateIEs),
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_54, MAKEWORD(byTxRate,byRsvTime));
    //RSPINF_a_72
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_72, MAKEWORD(byTxRate,byRsvTime));
    //Set to Page0
    MACvSelectPage0(pDevice->PortOffset);
}

/*---------------------  Export Functions  --------------------------*/

/*
 * Description: Card Send packet function
 *
 * Parameters:
 *  In:
 *      pDeviceHandler      - The adapter to be set
 *      pPacket             - Packet buffer pointer
 *      ePktType            - Packet type
 *      uLength             - Packet length
 *  Out:
 *      none
 *
 * Return Value: TRUE if succeeded; FALSE if failed.
 *
 */
/*
BOOL CARDbSendPacket (void *pDeviceHandler, void *pPacket, CARD_PKT_TYPE ePktType, unsigned int uLength)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    if (ePktType == PKT_TYPE_802_11_MNG) {
        return TXbTD0Send(pDevice, pPacket, uLength);
    } else if (ePktType == PKT_TYPE_802_11_BCN) {
        return TXbBeaconSend(pDevice, pPacket, uLength);
    } if (ePktType == PKT_TYPE_802_11_DATA) {
        return TXbTD1Send(pDevice, pPacket, uLength);
    }

    return (TRUE);
}
*/


/*
 * Description: Get Card short preamble option value
 *
 * Parameters:
 *  In:
 *      pDevice             - The adapter to be set
 *  Out:
 *      none
 *
 * Return Value: TRUE if short preamble; otherwise FALSE
 *
 */
BOOL CARDbIsShortPreamble (void *pDeviceHandler)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    if (pDevice->byPreambleType == 0) {
        return(FALSE);
    }
    return(TRUE);
}

/*
 * Description: Get Card short slot time option value
 *
 * Parameters:
 *  In:
 *      pDevice             - The adapter to be set
 *  Out:
 *      none
 *
 * Return Value: TRUE if short slot time; otherwise FALSE
 *
 */
BOOL CARDbIsShorSlotTime (void *pDeviceHandler)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    return(pDevice->bShortSlotTime);
}


/*
 * Description: Update IFS
 *
 * Parameters:
 *  In:
 *      pDevice             - The adapter to be set
 *  Out:
 *      none
 *
 * Return Value: None.
 *
 */
BOOL CARDbSetPhyParameter (void *pDeviceHandler, CARD_PHY_TYPE ePHYType, WORD wCapInfo, BYTE byERPField, void *pvSupportRateIEs, void *pvExtSupportRateIEs)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    BYTE        byCWMaxMin = 0;
    BYTE        bySlot = 0;
    BYTE        bySIFS = 0;
    BYTE        byDIFS = 0;
    BYTE        byData;
//    PWLAN_IE_SUPP_RATES pRates = NULL;
    PWLAN_IE_SUPP_RATES pSupportRates = (PWLAN_IE_SUPP_RATES) pvSupportRateIEs;
    PWLAN_IE_SUPP_RATES pExtSupportRates = (PWLAN_IE_SUPP_RATES) pvExtSupportRateIEs;


    //Set SIFS, DIFS, EIFS, SlotTime, CwMin
    if (ePHYType == PHY_TYPE_11A) {
        if (pSupportRates == NULL) {
            pSupportRates = (PWLAN_IE_SUPP_RATES) abyDefaultSuppRatesA;
        }
        if (pDevice->byRFType == RF_AIROHA7230) {
            // AL7230 use single PAPE and connect to PAPE_2.4G
            MACvSetBBType(pDevice->PortOffset, BB_TYPE_11G);
            pDevice->abyBBVGA[0] = 0x20;
            pDevice->abyBBVGA[2] = 0x10;
            pDevice->abyBBVGA[3] = 0x10;
            BBbReadEmbeded(pDevice->PortOffset, 0xE7, &byData);
            if (byData == 0x1C) {
                BBbWriteEmbeded(pDevice->PortOffset, 0xE7, pDevice->abyBBVGA[0]);
            }
        } else if (pDevice->byRFType == RF_UW2452) {
            MACvSetBBType(pDevice->PortOffset, BB_TYPE_11A);
            pDevice->abyBBVGA[0] = 0x18;
            BBbReadEmbeded(pDevice->PortOffset, 0xE7, &byData);
            if (byData == 0x14) {
                BBbWriteEmbeded(pDevice->PortOffset, 0xE7, pDevice->abyBBVGA[0]);
                BBbWriteEmbeded(pDevice->PortOffset, 0xE1, 0x57);
            }
        } else {
            MACvSetBBType(pDevice->PortOffset, BB_TYPE_11A);
        }
        BBbWriteEmbeded(pDevice->PortOffset, 0x88, 0x03);
        bySlot = C_SLOT_SHORT;
        bySIFS = C_SIFS_A;
        byDIFS = C_SIFS_A + 2*C_SLOT_SHORT;
        byCWMaxMin = 0xA4;
    } else if (ePHYType == PHY_TYPE_11B) {
        if (pSupportRates == NULL) {
            pSupportRates = (PWLAN_IE_SUPP_RATES) abyDefaultSuppRatesB;
        }
        MACvSetBBType(pDevice->PortOffset, BB_TYPE_11B);
        if (pDevice->byRFType == RF_AIROHA7230) {
            pDevice->abyBBVGA[0] = 0x1C;
            pDevice->abyBBVGA[2] = 0x00;
            pDevice->abyBBVGA[3] = 0x00;
            BBbReadEmbeded(pDevice->PortOffset, 0xE7, &byData);
            if (byData == 0x20) {
                BBbWriteEmbeded(pDevice->PortOffset, 0xE7, pDevice->abyBBVGA[0]);
            }
        } else if (pDevice->byRFType == RF_UW2452) {
            pDevice->abyBBVGA[0] = 0x14;
            BBbReadEmbeded(pDevice->PortOffset, 0xE7, &byData);
            if (byData == 0x18) {
                BBbWriteEmbeded(pDevice->PortOffset, 0xE7, pDevice->abyBBVGA[0]);
                BBbWriteEmbeded(pDevice->PortOffset, 0xE1, 0xD3);
            }
        }
        BBbWriteEmbeded(pDevice->PortOffset, 0x88, 0x02);
        bySlot = C_SLOT_LONG;
        bySIFS = C_SIFS_BG;
        byDIFS = C_SIFS_BG + 2*C_SLOT_LONG;
        byCWMaxMin = 0xA5;
    } else {// PK_TYPE_11GA & PK_TYPE_11GB
        if (pSupportRates == NULL) {
            pSupportRates = (PWLAN_IE_SUPP_RATES) abyDefaultSuppRatesG;
            pExtSupportRates = (PWLAN_IE_SUPP_RATES) abyDefaultExtSuppRatesG;
        }
        MACvSetBBType(pDevice->PortOffset, BB_TYPE_11G);
        if (pDevice->byRFType == RF_AIROHA7230) {
            pDevice->abyBBVGA[0] = 0x1C;
            pDevice->abyBBVGA[2] = 0x00;
            pDevice->abyBBVGA[3] = 0x00;
            BBbReadEmbeded(pDevice->PortOffset, 0xE7, &byData);
            if (byData == 0x20) {
                BBbWriteEmbeded(pDevice->PortOffset, 0xE7, pDevice->abyBBVGA[0]);
            }
        } else if (pDevice->byRFType == RF_UW2452) {
            pDevice->abyBBVGA[0] = 0x14;
            BBbReadEmbeded(pDevice->PortOffset, 0xE7, &byData);
            if (byData == 0x18) {
                BBbWriteEmbeded(pDevice->PortOffset, 0xE7, pDevice->abyBBVGA[0]);
                BBbWriteEmbeded(pDevice->PortOffset, 0xE1, 0xD3);
            }
        }
        BBbWriteEmbeded(pDevice->PortOffset, 0x88, 0x08);
        bySIFS = C_SIFS_BG;
        if(VNTWIFIbIsShortSlotTime(wCapInfo)) {
            bySlot = C_SLOT_SHORT;
            byDIFS = C_SIFS_BG + 2*C_SLOT_SHORT;
        } else {
            bySlot = C_SLOT_LONG;
            byDIFS = C_SIFS_BG + 2*C_SLOT_LONG;
	    }
        if (VNTWIFIbyGetMaxSupportRate(pSupportRates, pExtSupportRates) > RATE_11M) {
            byCWMaxMin = 0xA4;
        } else {
            byCWMaxMin = 0xA5;
        }
        if (pDevice->bProtectMode != VNTWIFIbIsProtectMode(byERPField)) {
            pDevice->bProtectMode = VNTWIFIbIsProtectMode(byERPField);
            if (pDevice->bProtectMode) {
                MACvEnableProtectMD(pDevice->PortOffset);
            } else {
                MACvDisableProtectMD(pDevice->PortOffset);
            }
        }
        if (pDevice->bBarkerPreambleMd != VNTWIFIbIsBarkerMode(byERPField)) {
            pDevice->bBarkerPreambleMd = VNTWIFIbIsBarkerMode(byERPField);
            if (pDevice->bBarkerPreambleMd) {
                MACvEnableBarkerPreambleMd(pDevice->PortOffset);
            } else {
                MACvDisableBarkerPreambleMd(pDevice->PortOffset);
            }
        }
    }

    if (pDevice->byRFType == RF_RFMD2959) {
        // bcs TX_PE will reserve 3 us
        // hardware's processing time here is 2 us.
        bySIFS -= 3;
        byDIFS -= 3;
    //{{ RobertYu: 20041202
    //// TX_PE will reserve 3 us for MAX2829 A mode only, it is for better TX throughput
    //// MAC will need 2 us to process, so the SIFS, DIFS can be shorter by 2 us.
    }

    if (pDevice->bySIFS != bySIFS) {
        pDevice->bySIFS = bySIFS;
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_SIFS, pDevice->bySIFS);
    }
    if (pDevice->byDIFS != byDIFS) {
        pDevice->byDIFS = byDIFS;
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_DIFS, pDevice->byDIFS);
    }
    if (pDevice->byEIFS != C_EIFS) {
        pDevice->byEIFS = C_EIFS;
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_EIFS, pDevice->byEIFS);
    }
    if (pDevice->bySlot != bySlot) {
        pDevice->bySlot = bySlot;
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_SLOT, pDevice->bySlot);
        if (pDevice->bySlot == C_SLOT_SHORT) {
            pDevice->bShortSlotTime = TRUE;
        } else {
            pDevice->bShortSlotTime = FALSE;
        }
        BBvSetShortSlotTime(pDevice);
    }
    if (pDevice->byCWMaxMin != byCWMaxMin) {
        pDevice->byCWMaxMin = byCWMaxMin;
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_CWMAXMIN0, pDevice->byCWMaxMin);
    }
    if (VNTWIFIbIsShortPreamble(wCapInfo)) {
        pDevice->byPreambleType = pDevice->byShortPreamble;
    } else {
        pDevice->byPreambleType = 0;
    }
    s_vSetRSPINF(pDevice, ePHYType, pSupportRates, pExtSupportRates);
    pDevice->eCurrentPHYType = ePHYType;
    // set for NDIS OID_802_11SUPPORTED_RATES
    return (TRUE);
}

/*
 * Description: Sync. TSF counter to BSS
 *              Get TSF offset and write to HW
 *
 * Parameters:
 *  In:
 *      pDevice         - The adapter to be sync.
 *      byRxRate        - data rate of receive beacon
 *      qwBSSTimestamp  - Rx BCN's TSF
 *      qwLocalTSF      - Local TSF
 *  Out:
 *      none
 *
 * Return Value: none
 *
 */
BOOL CARDbUpdateTSF (void *pDeviceHandler, BYTE byRxRate, QWORD qwBSSTimestamp, QWORD qwLocalTSF)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    QWORD       qwTSFOffset;

    HIDWORD(qwTSFOffset) = 0;
    LODWORD(qwTSFOffset) = 0;

    if ((HIDWORD(qwBSSTimestamp) != HIDWORD(qwLocalTSF)) ||
        (LODWORD(qwBSSTimestamp) != LODWORD(qwLocalTSF))) {
        qwTSFOffset = CARDqGetTSFOffset(byRxRate, qwBSSTimestamp, qwLocalTSF);
        // adjust TSF
        // HW's TSF add TSF Offset reg
        VNSvOutPortD(pDevice->PortOffset + MAC_REG_TSFOFST, LODWORD(qwTSFOffset));
        VNSvOutPortD(pDevice->PortOffset + MAC_REG_TSFOFST + 4, HIDWORD(qwTSFOffset));
        MACvRegBitsOn(pDevice->PortOffset, MAC_REG_TFTCTL, TFTCTL_TSFSYNCEN);
    }
    return(TRUE);
}


/*
 * Description: Set NIC TSF counter for first Beacon time
 *              Get NEXTTBTT from adjusted TSF and Beacon Interval
 *
 * Parameters:
 *  In:
 *      pDevice         - The adapter to be set.
 *      wBeaconInterval - Beacon Interval
 *  Out:
 *      none
 *
 * Return Value: TRUE if succeed; otherwise FALSE
 *
 */
BOOL CARDbSetBeaconPeriod (void *pDeviceHandler, WORD wBeaconInterval)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    unsigned int uBeaconInterval = 0;
    unsigned int uLowNextTBTT = 0;
    unsigned int uHighRemain = 0;
    unsigned int uLowRemain = 0;
    QWORD       qwNextTBTT;

    HIDWORD(qwNextTBTT) = 0;
    LODWORD(qwNextTBTT) = 0;
    CARDbGetCurrentTSF(pDevice->PortOffset, &qwNextTBTT); //Get Local TSF counter
    uBeaconInterval = wBeaconInterval * 1024;
    // Next TBTT = ((local_current_TSF / beacon_interval) + 1 ) * beacon_interval
    uLowNextTBTT = (LODWORD(qwNextTBTT) >> 10) << 10;
    uLowRemain = (uLowNextTBTT) % uBeaconInterval;
    // high dword (mod) bcn
    uHighRemain = (((0xffffffff % uBeaconInterval) + 1) * HIDWORD(qwNextTBTT))
                  % uBeaconInterval;
    uLowRemain = (uHighRemain + uLowRemain) % uBeaconInterval;
    uLowRemain = uBeaconInterval - uLowRemain;

    // check if carry when add one beacon interval
    if ((~uLowNextTBTT) < uLowRemain) {
        HIDWORD(qwNextTBTT) ++ ;
    }
    LODWORD(qwNextTBTT) = uLowNextTBTT + uLowRemain;

    // set HW beacon interval
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_BI, wBeaconInterval);
    pDevice->wBeaconInterval = wBeaconInterval;
    // Set NextTBTT
    VNSvOutPortD(pDevice->PortOffset + MAC_REG_NEXTTBTT, LODWORD(qwNextTBTT));
    VNSvOutPortD(pDevice->PortOffset + MAC_REG_NEXTTBTT + 4, HIDWORD(qwNextTBTT));
    MACvRegBitsOn(pDevice->PortOffset, MAC_REG_TFTCTL, TFTCTL_TBTTSYNCEN);

    return(TRUE);
}



/*
 * Description: Card Stop Hardware Tx
 *
 * Parameters:
 *  In:
 *      pDeviceHandler      - The adapter to be set
 *      ePktType            - Packet type to stop
 *  Out:
 *      none
 *
 * Return Value: TRUE if all data packet complete; otherwise FALSE.
 *
 */
BOOL CARDbStopTxPacket (void *pDeviceHandler, CARD_PKT_TYPE ePktType)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;


    if (ePktType == PKT_TYPE_802_11_ALL) {
        pDevice->bStopBeacon = TRUE;
        pDevice->bStopTx0Pkt = TRUE;
        pDevice->bStopDataPkt = TRUE;
    } else if (ePktType == PKT_TYPE_802_11_BCN) {
        pDevice->bStopBeacon = TRUE;
    } else if (ePktType == PKT_TYPE_802_11_MNG) {
        pDevice->bStopTx0Pkt = TRUE;
    } else if (ePktType == PKT_TYPE_802_11_DATA) {
        pDevice->bStopDataPkt = TRUE;
    }

    if (pDevice->bStopBeacon == TRUE) {
        if (pDevice->bIsBeaconBufReadySet == TRUE) {
            if (pDevice->cbBeaconBufReadySetCnt < WAIT_BEACON_TX_DOWN_TMO) {
                pDevice->cbBeaconBufReadySetCnt ++;
                return(FALSE);
            }
        }
        pDevice->bIsBeaconBufReadySet = FALSE;
        pDevice->cbBeaconBufReadySetCnt = 0;
        MACvRegBitsOff(pDevice->PortOffset, MAC_REG_TCR, TCR_AUTOBCNTX);
    }
    // wait all TD0 complete
    if (pDevice->bStopTx0Pkt == TRUE) {
         if (pDevice->iTDUsed[TYPE_TXDMA0] != 0){
            return(FALSE);
        }
    }
    // wait all Data TD complete
    if (pDevice->bStopDataPkt == TRUE) {
        if (pDevice->iTDUsed[TYPE_AC0DMA] != 0){
            return(FALSE);
        }
    }

    return(TRUE);
}


/*
 * Description: Card Start Hardware Tx
 *
 * Parameters:
 *  In:
 *      pDeviceHandler      - The adapter to be set
 *      ePktType            - Packet type to start
 *  Out:
 *      none
 *
 * Return Value: TRUE if success; FALSE if failed.
 *
 */
BOOL CARDbStartTxPacket (void *pDeviceHandler, CARD_PKT_TYPE ePktType)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;


    if (ePktType == PKT_TYPE_802_11_ALL) {
        pDevice->bStopBeacon = FALSE;
        pDevice->bStopTx0Pkt = FALSE;
        pDevice->bStopDataPkt = FALSE;
    } else if (ePktType == PKT_TYPE_802_11_BCN) {
        pDevice->bStopBeacon = FALSE;
    } else if (ePktType == PKT_TYPE_802_11_MNG) {
        pDevice->bStopTx0Pkt = FALSE;
    } else if (ePktType == PKT_TYPE_802_11_DATA) {
        pDevice->bStopDataPkt = FALSE;
    }

    if ((pDevice->bStopBeacon == FALSE) &&
        (pDevice->bBeaconBufReady == TRUE) &&
        (pDevice->eOPMode == OP_MODE_ADHOC)) {
        MACvRegBitsOn(pDevice->PortOffset, MAC_REG_TCR, TCR_AUTOBCNTX);
    }

    return(TRUE);
}



/*
 * Description: Card Set BSSID value
 *
 * Parameters:
 *  In:
 *      pDeviceHandler      - The adapter to be set
 *      pbyBSSID            - pointer to BSSID field
 *      bAdhoc              - flag to indicate IBSS
 *  Out:
 *      none
 *
 * Return Value: TRUE if success; FALSE if failed.
 *
 */
BOOL CARDbSetBSSID(void *pDeviceHandler, unsigned char *pbyBSSID, CARD_OP_MODE eOPMode)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;

    MACvWriteBSSIDAddress(pDevice->PortOffset, pbyBSSID);
    memcpy(pDevice->abyBSSID, pbyBSSID, WLAN_BSSID_LEN);
    if (eOPMode == OP_MODE_ADHOC) {
        MACvRegBitsOn(pDevice->PortOffset, MAC_REG_HOSTCR, HOSTCR_ADHOC);
    } else {
        MACvRegBitsOff(pDevice->PortOffset, MAC_REG_HOSTCR, HOSTCR_ADHOC);
    }
    if (eOPMode == OP_MODE_AP) {
        MACvRegBitsOn(pDevice->PortOffset, MAC_REG_HOSTCR, HOSTCR_AP);
    } else {
        MACvRegBitsOff(pDevice->PortOffset, MAC_REG_HOSTCR, HOSTCR_AP);
    }
    if (eOPMode == OP_MODE_UNKNOWN) {
        MACvRegBitsOff(pDevice->PortOffset, MAC_REG_RCR, RCR_BSSID);
        pDevice->bBSSIDFilter = FALSE;
        pDevice->byRxMode &= ~RCR_BSSID;
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "wcmd: rx_mode = %x\n", pDevice->byRxMode );
    } else {
        if (is_zero_ether_addr(pDevice->abyBSSID) == FALSE) {
            MACvRegBitsOn(pDevice->PortOffset, MAC_REG_RCR, RCR_BSSID);
            pDevice->bBSSIDFilter = TRUE;
            pDevice->byRxMode |= RCR_BSSID;
	    }
	    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "wmgr: rx_mode = %x\n", pDevice->byRxMode );
    }
    // Adopt BSS state in Adapter Device Object
    pDevice->eOPMode = eOPMode;
    return(TRUE);
}


/*
 * Description: Card indicate status
 *
 * Parameters:
 *  In:
 *      pDeviceHandler      - The adapter to be set
 *      eStatus             - Status
 *  Out:
 *      none
 *
 * Return Value: TRUE if success; FALSE if failed.
 *
 */




/*
 * Description: Save Assoc info. contain in assoc. response frame
 *
 * Parameters:
 *  In:
 *      pDevice             - The adapter to be set
 *      wCapabilityInfo     - Capability information
 *      wStatus             - Status code
 *      wAID                - Assoc. ID
 *      uLen                - Length of IEs
 *      pbyIEs              - pointer to IEs
 *  Out:
 *      none
 *
 * Return Value: TRUE if succeed; otherwise FALSE
 *
 */
BOOL CARDbSetTxDataRate(
    void *pDeviceHandler,
    WORD    wDataRate
    )
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;

    pDevice->wCurrentRate = wDataRate;
    return(TRUE);
}

/*+
 *
 * Routine Description:
 *      Consider to power down when no more packets to tx or rx.
 *
 * Parameters:
 *  In:
 *      pDevice             - The adapter to be set
 *  Out:
 *      none
 *
 * Return Value: TRUE if power down success; otherwise FALSE
 *
-*/
BOOL
CARDbPowerDown(
    void *pDeviceHandler
    )
{
    PSDevice        pDevice = (PSDevice)pDeviceHandler;
    unsigned int uIdx;

    // check if already in Doze mode
    if (MACbIsRegBitsOn(pDevice->PortOffset, MAC_REG_PSCTL, PSCTL_PS))
        return TRUE;

    // Froce PSEN on
    MACvRegBitsOn(pDevice->PortOffset, MAC_REG_PSCTL, PSCTL_PSEN);

    // check if all TD are empty,

    for (uIdx = 0; uIdx < TYPE_MAXTD; uIdx ++) {
        if (pDevice->iTDUsed[uIdx] != 0)
            return FALSE;
    }

    MACvRegBitsOn(pDevice->PortOffset, MAC_REG_PSCTL, PSCTL_GO2DOZE);
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Go to Doze ZZZZZZZZZZZZZZZ\n");
    return TRUE;
}

/*
 * Description: Turn off Radio power
 *
 * Parameters:
 *  In:
 *      pDevice         - The adapter to be turned off
 *  Out:
 *      none
 *
 * Return Value: TRUE if success; otherwise FALSE
 *
 */
BOOL CARDbRadioPowerOff (void *pDeviceHandler)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    BOOL        bResult = TRUE;

    if (pDevice->bRadioOff == TRUE)
        return TRUE;


    switch (pDevice->byRFType) {

        case RF_RFMD2959:
            MACvWordRegBitsOff(pDevice->PortOffset, MAC_REG_SOFTPWRCTL, SOFTPWRCTL_TXPEINV);
            MACvWordRegBitsOn(pDevice->PortOffset, MAC_REG_SOFTPWRCTL, SOFTPWRCTL_SWPE1);
            break;

        case RF_AIROHA:
        case RF_AL2230S:
        case RF_AIROHA7230: //RobertYu:20050104
            MACvWordRegBitsOff(pDevice->PortOffset, MAC_REG_SOFTPWRCTL, SOFTPWRCTL_SWPE2);
            MACvWordRegBitsOff(pDevice->PortOffset, MAC_REG_SOFTPWRCTL, SOFTPWRCTL_SWPE3);
            break;

    }

    MACvRegBitsOff(pDevice->PortOffset, MAC_REG_HOSTCR, HOSTCR_RXON);

    BBvSetDeepSleep(pDevice->PortOffset, pDevice->byLocalID);

    pDevice->bRadioOff = TRUE;
     //2007-0409-03,<Add> by chester
printk("chester power off\n");
MACvRegBitsOn(pDevice->PortOffset, MAC_REG_GPIOCTL0, LED_ACTSET);  //LED issue
    return bResult;
}


/*
 * Description: Turn on Radio power
 *
 * Parameters:
 *  In:
 *      pDevice         - The adapter to be turned on
 *  Out:
 *      none
 *
 * Return Value: TRUE if success; otherwise FALSE
 *
 */
BOOL CARDbRadioPowerOn (void *pDeviceHandler)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    BOOL        bResult = TRUE;
printk("chester power on\n");
    if (pDevice->bRadioControlOff == TRUE){
if (pDevice->bHWRadioOff == TRUE) printk("chester bHWRadioOff\n");
if (pDevice->bRadioControlOff == TRUE) printk("chester bRadioControlOff\n");
        return FALSE;}

    if (pDevice->bRadioOff == FALSE)
       {
printk("chester pbRadioOff\n");
return TRUE;}

    BBvExitDeepSleep(pDevice->PortOffset, pDevice->byLocalID);

    MACvRegBitsOn(pDevice->PortOffset, MAC_REG_HOSTCR, HOSTCR_RXON);

    switch (pDevice->byRFType) {

        case RF_RFMD2959:
            MACvWordRegBitsOn(pDevice->PortOffset, MAC_REG_SOFTPWRCTL, SOFTPWRCTL_TXPEINV);
            MACvWordRegBitsOff(pDevice->PortOffset, MAC_REG_SOFTPWRCTL, SOFTPWRCTL_SWPE1);
            break;

        case RF_AIROHA:
        case RF_AL2230S:
        case RF_AIROHA7230: //RobertYu:20050104
            MACvWordRegBitsOn(pDevice->PortOffset, MAC_REG_SOFTPWRCTL, (SOFTPWRCTL_SWPE2 |
                                                                        SOFTPWRCTL_SWPE3));
            break;

    }

    pDevice->bRadioOff = FALSE;
//  2007-0409-03,<Add> by chester
printk("chester power on\n");
MACvRegBitsOff(pDevice->PortOffset, MAC_REG_GPIOCTL0, LED_ACTSET); //LED issue
    return bResult;
}



BOOL CARDbRemoveKey (void *pDeviceHandler, unsigned char *pbyBSSID)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;

    KeybRemoveAllKey(&(pDevice->sKey), pbyBSSID, pDevice->PortOffset);
    return (TRUE);
}


/*
 *
 * Description:
 *    Add BSSID in PMKID Candidate list.
 *
 * Parameters:
 *  In:
 *      hDeviceContext - device structure point
 *      pbyBSSID - BSSID address for adding
 *      wRSNCap - BSS's RSN capability
 *  Out:
 *      none
 *
 * Return Value: none.
 *
-*/
BOOL
CARDbAdd_PMKID_Candidate (
    void *pDeviceHandler,
    unsigned char *pbyBSSID,
    BOOL             bRSNCapExist,
    WORD             wRSNCap
    )
{
    PSDevice            pDevice = (PSDevice) pDeviceHandler;
    PPMKID_CANDIDATE    pCandidateList;
    unsigned int ii = 0;

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"bAdd_PMKID_Candidate START: (%d)\n", (int)pDevice->gsPMKIDCandidate.NumCandidates);

    if (pDevice->gsPMKIDCandidate.NumCandidates >= MAX_PMKIDLIST) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"vFlush_PMKID_Candidate: 3\n");
        memset(&pDevice->gsPMKIDCandidate, 0, sizeof(SPMKIDCandidateEvent));
    }

    for (ii = 0; ii < 6; ii++) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"%02X ", *(pbyBSSID + ii));
    }
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"\n");


    // Update Old Candidate
    for (ii = 0; ii < pDevice->gsPMKIDCandidate.NumCandidates; ii++) {
        pCandidateList = &pDevice->gsPMKIDCandidate.CandidateList[ii];
        if ( !memcmp(pCandidateList->BSSID, pbyBSSID, ETH_ALEN)) {
            if ((bRSNCapExist == TRUE) && (wRSNCap & BIT0)) {
                pCandidateList->Flags |= NDIS_802_11_PMKID_CANDIDATE_PREAUTH_ENABLED;
            } else {
                pCandidateList->Flags &= ~(NDIS_802_11_PMKID_CANDIDATE_PREAUTH_ENABLED);
            }
            return TRUE;
        }
    }

    // New Candidate
    pCandidateList = &pDevice->gsPMKIDCandidate.CandidateList[pDevice->gsPMKIDCandidate.NumCandidates];
    if ((bRSNCapExist == TRUE) && (wRSNCap & BIT0)) {
        pCandidateList->Flags |= NDIS_802_11_PMKID_CANDIDATE_PREAUTH_ENABLED;
    } else {
        pCandidateList->Flags &= ~(NDIS_802_11_PMKID_CANDIDATE_PREAUTH_ENABLED);
    }
    memcpy(pCandidateList->BSSID, pbyBSSID, ETH_ALEN);
    pDevice->gsPMKIDCandidate.NumCandidates++;
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"NumCandidates:%d\n", (int)pDevice->gsPMKIDCandidate.NumCandidates);
    return TRUE;
}

void *
CARDpGetCurrentAddress (
    void *pDeviceHandler
    )
{
    PSDevice            pDevice = (PSDevice) pDeviceHandler;

    return (pDevice->abyCurrentNetAddr);
}

/*
 *
 * Description:
 *    Start Spectrum Measure defined in 802.11h
 *
 * Parameters:
 *  In:
 *      hDeviceContext - device structure point
 *  Out:
 *      none
 *
 * Return Value: none.
 *
-*/
BOOL
CARDbStartMeasure (
    void *pDeviceHandler,
    void *pvMeasureEIDs,
    unsigned int uNumOfMeasureEIDs
    )
{
    PSDevice                pDevice = (PSDevice) pDeviceHandler;
    PWLAN_IE_MEASURE_REQ    pEID = (PWLAN_IE_MEASURE_REQ) pvMeasureEIDs;
    QWORD                   qwCurrTSF;
    QWORD                   qwStartTSF;
    BOOL                    bExpired = TRUE;
    WORD                    wDuration = 0;

    if ((pEID == NULL) ||
        (uNumOfMeasureEIDs == 0)) {
        return (TRUE);
    }
    CARDbGetCurrentTSF(pDevice->PortOffset, &qwCurrTSF);
    if (pDevice->bMeasureInProgress == TRUE) {
        pDevice->bMeasureInProgress = FALSE;
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_RCR, pDevice->byOrgRCR);
        MACvSelectPage1(pDevice->PortOffset);
        VNSvOutPortD(pDevice->PortOffset + MAC_REG_MAR0, pDevice->dwOrgMAR0);
        VNSvOutPortD(pDevice->PortOffset + MAC_REG_MAR4, pDevice->dwOrgMAR4);
        // clear measure control
        MACvRegBitsOff(pDevice->PortOffset, MAC_REG_MSRCTL, MSRCTL_EN);
        MACvSelectPage0(pDevice->PortOffset);
        set_channel(pDevice, pDevice->byOrgChannel);
        MACvSelectPage1(pDevice->PortOffset);
        MACvRegBitsOn(pDevice->PortOffset, MAC_REG_MSRCTL+1, MSRCTL1_TXPAUSE);
        MACvSelectPage0(pDevice->PortOffset);
    }
    pDevice->uNumOfMeasureEIDs = uNumOfMeasureEIDs;

    do {
        pDevice->pCurrMeasureEID = pEID;
        pEID++;
        pDevice->uNumOfMeasureEIDs--;

        if (pDevice->byLocalID > REV_ID_VT3253_B1) {
            HIDWORD(qwStartTSF) = HIDWORD(*((PQWORD) (pDevice->pCurrMeasureEID->sReq.abyStartTime)));
            LODWORD(qwStartTSF) = LODWORD(*((PQWORD) (pDevice->pCurrMeasureEID->sReq.abyStartTime)));
            wDuration = *((unsigned short *) (pDevice->pCurrMeasureEID->sReq.abyDuration));
            wDuration += 1; // 1 TU for channel switching

            if ((LODWORD(qwStartTSF) == 0) && (HIDWORD(qwStartTSF) == 0)) {
                // start imediately by setting start TSF == current TSF + 2 TU
                LODWORD(qwStartTSF) = LODWORD(qwCurrTSF) + 2048;
                HIDWORD(qwStartTSF) = HIDWORD(qwCurrTSF);
                if (LODWORD(qwCurrTSF) > LODWORD(qwStartTSF)) {
                    HIDWORD(qwStartTSF)++;
                }
                bExpired = FALSE;
                break;
            } else {
                // start at setting start TSF - 1TU(for channel switching)
                if (LODWORD(qwStartTSF) < 1024) {
                    HIDWORD(qwStartTSF)--;
                }
                LODWORD(qwStartTSF) -= 1024;
            }

            if ((HIDWORD(qwCurrTSF) < HIDWORD(qwStartTSF)) ||
                ((HIDWORD(qwCurrTSF) == HIDWORD(qwStartTSF)) &&
                (LODWORD(qwCurrTSF) < LODWORD(qwStartTSF)))
                ) {
                bExpired = FALSE;
                break;
            }
            VNTWIFIbMeasureReport(  pDevice->pMgmt,
                                    FALSE,
                                    pDevice->pCurrMeasureEID,
                                    MEASURE_MODE_LATE,
                                    pDevice->byBasicMap,
                                    pDevice->byCCAFraction,
                                    pDevice->abyRPIs
                                    );
        } else {
            // hardware do not support measure
            VNTWIFIbMeasureReport(  pDevice->pMgmt,
                                    FALSE,
                                    pDevice->pCurrMeasureEID,
                                    MEASURE_MODE_INCAPABLE,
                                    pDevice->byBasicMap,
                                    pDevice->byCCAFraction,
                                    pDevice->abyRPIs
                                    );
        }
    } while (pDevice->uNumOfMeasureEIDs != 0);

    if (bExpired == FALSE) {
        MACvSelectPage1(pDevice->PortOffset);
        VNSvOutPortD(pDevice->PortOffset + MAC_REG_MSRSTART, LODWORD(qwStartTSF));
        VNSvOutPortD(pDevice->PortOffset + MAC_REG_MSRSTART + 4, HIDWORD(qwStartTSF));
        VNSvOutPortW(pDevice->PortOffset + MAC_REG_MSRDURATION, wDuration);
        MACvRegBitsOn(pDevice->PortOffset, MAC_REG_MSRCTL, MSRCTL_EN);
        MACvSelectPage0(pDevice->PortOffset);
    } else {
        // all measure start time expired we should complete action
        VNTWIFIbMeasureReport(  pDevice->pMgmt,
                                TRUE,
                                NULL,
                                0,
                                pDevice->byBasicMap,
                                pDevice->byCCAFraction,
                                pDevice->abyRPIs
                                );
    }
    return (TRUE);
}


/*
 *
 * Description:
 *    Do Channel Switch defined in 802.11h
 *
 * Parameters:
 *  In:
 *      hDeviceContext - device structure point
 *  Out:
 *      none
 *
 * Return Value: none.
 *
-*/
BOOL
CARDbChannelSwitch (
    void *pDeviceHandler,
    BYTE             byMode,
    BYTE             byNewChannel,
    BYTE             byCount
    )
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    BOOL        bResult = TRUE;

    if (byCount == 0) {
        bResult = set_channel(pDevice, byNewChannel);
        VNTWIFIbChannelSwitch(pDevice->pMgmt, byNewChannel);
        MACvSelectPage1(pDevice->PortOffset);
        MACvRegBitsOn(pDevice->PortOffset, MAC_REG_MSRCTL+1, MSRCTL1_TXPAUSE);
        MACvSelectPage0(pDevice->PortOffset);
        return(bResult);
    }
    pDevice->byChannelSwitchCount = byCount;
    pDevice->byNewChannel = byNewChannel;
    pDevice->bChannelSwitch = TRUE;
    if (byMode == 1) {
        bResult=CARDbStopTxPacket(pDevice, PKT_TYPE_802_11_ALL);
    }
    return (bResult);
}


/*
 *
 * Description:
 *    Handle Quiet EID defined in 802.11h
 *
 * Parameters:
 *  In:
 *      hDeviceContext - device structure point
 *  Out:
 *      none
 *
 * Return Value: none.
 *
-*/
BOOL
CARDbSetQuiet (
    void *pDeviceHandler,
    BOOL             bResetQuiet,
    BYTE             byQuietCount,
    BYTE             byQuietPeriod,
    WORD             wQuietDuration,
    WORD             wQuietOffset
    )
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    unsigned int ii = 0;

    if (bResetQuiet == TRUE) {
        MACvRegBitsOff(pDevice->PortOffset, MAC_REG_MSRCTL, (MSRCTL_QUIETTXCHK | MSRCTL_QUIETEN));
        for(ii=0;ii<MAX_QUIET_COUNT;ii++) {
            pDevice->sQuiet[ii].bEnable = FALSE;
        }
        pDevice->uQuietEnqueue = 0;
        pDevice->bEnableFirstQuiet = FALSE;
        pDevice->bQuietEnable = FALSE;
        pDevice->byQuietStartCount = byQuietCount;
    }
    if (pDevice->sQuiet[pDevice->uQuietEnqueue].bEnable == FALSE) {
        pDevice->sQuiet[pDevice->uQuietEnqueue].bEnable = TRUE;
        pDevice->sQuiet[pDevice->uQuietEnqueue].byPeriod = byQuietPeriod;
        pDevice->sQuiet[pDevice->uQuietEnqueue].wDuration = wQuietDuration;
        pDevice->sQuiet[pDevice->uQuietEnqueue].dwStartTime = (DWORD) byQuietCount;
        pDevice->sQuiet[pDevice->uQuietEnqueue].dwStartTime *= pDevice->wBeaconInterval;
        pDevice->sQuiet[pDevice->uQuietEnqueue].dwStartTime += wQuietOffset;
        pDevice->uQuietEnqueue++;
        pDevice->uQuietEnqueue %= MAX_QUIET_COUNT;
        if (pDevice->byQuietStartCount < byQuietCount) {
            pDevice->byQuietStartCount = byQuietCount;
        }
    } else {
        // we can not handle Quiet EID more
    }
    return (TRUE);
}


/*
 *
 * Description:
 *    Do Quiet, It will called by either ISR (after start) or VNTWIFI (before start) so do not need SPINLOCK
 *
 * Parameters:
 *  In:
 *      hDeviceContext - device structure point
 *  Out:
 *      none
 *
 * Return Value: none.
 *
-*/
BOOL
CARDbStartQuiet (
    void *pDeviceHandler
    )
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    unsigned int ii = 0;
    DWORD       dwStartTime = 0xFFFFFFFF;
    unsigned int uCurrentQuietIndex = 0;
    DWORD       dwNextTime = 0;
    DWORD       dwGap = 0;
    DWORD       dwDuration = 0;

    for(ii=0;ii<MAX_QUIET_COUNT;ii++) {
        if ((pDevice->sQuiet[ii].bEnable == TRUE) &&
            (dwStartTime > pDevice->sQuiet[ii].dwStartTime)) {
            dwStartTime = pDevice->sQuiet[ii].dwStartTime;
            uCurrentQuietIndex = ii;
        }
    }
    if (dwStartTime == 0xFFFFFFFF) {
        // no more quiet
        pDevice->bQuietEnable = FALSE;
        MACvRegBitsOff(pDevice->PortOffset, MAC_REG_MSRCTL, (MSRCTL_QUIETTXCHK | MSRCTL_QUIETEN));
    } else {
        if (pDevice->bQuietEnable == FALSE) {
            // first quiet
            pDevice->byQuietStartCount--;
            dwNextTime = pDevice->sQuiet[uCurrentQuietIndex].dwStartTime;
            dwNextTime %= pDevice->wBeaconInterval;
            MACvSelectPage1(pDevice->PortOffset);
            VNSvOutPortW(pDevice->PortOffset + MAC_REG_QUIETINIT, (WORD) dwNextTime);
            VNSvOutPortW(pDevice->PortOffset + MAC_REG_QUIETDUR, (WORD) pDevice->sQuiet[uCurrentQuietIndex].wDuration);
            if (pDevice->byQuietStartCount == 0) {
                pDevice->bEnableFirstQuiet = FALSE;
                MACvRegBitsOn(pDevice->PortOffset, MAC_REG_MSRCTL, (MSRCTL_QUIETTXCHK | MSRCTL_QUIETEN));
            } else {
                pDevice->bEnableFirstQuiet = TRUE;
            }
            MACvSelectPage0(pDevice->PortOffset);
        } else {
            if (pDevice->dwCurrentQuietEndTime > pDevice->sQuiet[uCurrentQuietIndex].dwStartTime) {
                // overlap with previous Quiet
                dwGap =  pDevice->dwCurrentQuietEndTime - pDevice->sQuiet[uCurrentQuietIndex].dwStartTime;
                if (dwGap >= pDevice->sQuiet[uCurrentQuietIndex].wDuration) {
                    // return FALSE to indicate next quiet expired, should call this function again
                    return (FALSE);
                }
                dwDuration = pDevice->sQuiet[uCurrentQuietIndex].wDuration - dwGap;
                dwGap = 0;
            } else {
                dwGap = pDevice->sQuiet[uCurrentQuietIndex].dwStartTime - pDevice->dwCurrentQuietEndTime;
                dwDuration = pDevice->sQuiet[uCurrentQuietIndex].wDuration;
            }
            // set GAP and Next duration
            MACvSelectPage1(pDevice->PortOffset);
            VNSvOutPortW(pDevice->PortOffset + MAC_REG_QUIETGAP, (WORD) dwGap);
            VNSvOutPortW(pDevice->PortOffset + MAC_REG_QUIETDUR, (WORD) dwDuration);
            MACvRegBitsOn(pDevice->PortOffset, MAC_REG_MSRCTL, MSRCTL_QUIETRPT);
            MACvSelectPage0(pDevice->PortOffset);
        }
        pDevice->bQuietEnable = TRUE;
        pDevice->dwCurrentQuietEndTime = pDevice->sQuiet[uCurrentQuietIndex].dwStartTime;
        pDevice->dwCurrentQuietEndTime += pDevice->sQuiet[uCurrentQuietIndex].wDuration;
        if (pDevice->sQuiet[uCurrentQuietIndex].byPeriod == 0) {
            // not period disable current quiet element
            pDevice->sQuiet[uCurrentQuietIndex].bEnable = FALSE;
        } else {
            // set next period start time
            dwNextTime = (DWORD) pDevice->sQuiet[uCurrentQuietIndex].byPeriod;
            dwNextTime *= pDevice->wBeaconInterval;
            pDevice->sQuiet[uCurrentQuietIndex].dwStartTime = dwNextTime;
        }
        if (pDevice->dwCurrentQuietEndTime > 0x80010000) {
            // decreament all time to avoid wrap around
            for(ii=0;ii<MAX_QUIET_COUNT;ii++) {
                if (pDevice->sQuiet[ii].bEnable == TRUE) {
                    pDevice->sQuiet[ii].dwStartTime -= 0x80000000;
                }
            }
            pDevice->dwCurrentQuietEndTime -= 0x80000000;
        }
    }
    return (TRUE);
}

/*
 *
 * Description:
 *    Set Local Power Constraint
 *
 * Parameters:
 *  In:
 *      hDeviceContext - device structure point
 *  Out:
 *      none
 *
 * Return Value: none.
 *
-*/
void
CARDvSetPowerConstraint (
    void *pDeviceHandler,
    BYTE             byChannel,
    char byPower
    )
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;

    if (byChannel > CB_MAX_CHANNEL_24G) {
        if (pDevice->bCountryInfo5G == TRUE) {
            pDevice->abyLocalPwr[byChannel] = pDevice->abyRegPwr[byChannel] - byPower;
        }
    } else {
        if (pDevice->bCountryInfo24G == TRUE) {
            pDevice->abyLocalPwr[byChannel] = pDevice->abyRegPwr[byChannel] - byPower;
        }
    }
}


/*
 *
 * Description:
 *    Set Local Power Constraint
 *
 * Parameters:
 *  In:
 *      hDeviceContext - device structure point
 *  Out:
 *      none
 *
 * Return Value: none.
 *
-*/
void
CARDvGetPowerCapability (
    void *pDeviceHandler,
    unsigned char *pbyMinPower,
    unsigned char *pbyMaxPower
    )
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    BYTE        byDec = 0;

    *pbyMaxPower = pDevice->abyOFDMDefaultPwr[pDevice->byCurrentCh];
    byDec = pDevice->abyOFDMPwrTbl[pDevice->byCurrentCh];
    if (pDevice->byRFType == RF_UW2452) {
        byDec *= 3;
        byDec >>= 1;
    } else {
        byDec <<= 1;
    }
    *pbyMinPower = pDevice->abyOFDMDefaultPwr[pDevice->byCurrentCh] - byDec;
}

/*
 *
 * Description:
 *    Get Current Tx Power
 *
 * Parameters:
 *  In:
 *      hDeviceContext - device structure point
 *  Out:
 *      none
 *
 * Return Value: none.
 *
 */
char
CARDbyGetTransmitPower (
    void *pDeviceHandler
    )
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;

    return (pDevice->byCurPwrdBm);
}

//xxx
void
CARDvSafeResetTx (
    void *pDeviceHandler
    )
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    unsigned int uu;
    PSTxDesc    pCurrTD;

    // initialize TD index
    pDevice->apTailTD[0] = pDevice->apCurrTD[0] = &(pDevice->apTD0Rings[0]);
    pDevice->apTailTD[1] = pDevice->apCurrTD[1] = &(pDevice->apTD1Rings[0]);

    for (uu = 0; uu < TYPE_MAXTD; uu ++)
        pDevice->iTDUsed[uu] = 0;

    for (uu = 0; uu < pDevice->sOpts.nTxDescs[0]; uu++) {
        pCurrTD = &(pDevice->apTD0Rings[uu]);
        pCurrTD->m_td0TD0.f1Owner = OWNED_BY_HOST;
        // init all Tx Packet pointer to NULL
    }
    for (uu = 0; uu < pDevice->sOpts.nTxDescs[1]; uu++) {
        pCurrTD = &(pDevice->apTD1Rings[uu]);
        pCurrTD->m_td0TD0.f1Owner = OWNED_BY_HOST;
        // init all Tx Packet pointer to NULL
    }

    // set MAC TD pointer
    MACvSetCurrTXDescAddr(TYPE_TXDMA0, pDevice->PortOffset,
                        (pDevice->td0_pool_dma));

    MACvSetCurrTXDescAddr(TYPE_AC0DMA, pDevice->PortOffset,
                        (pDevice->td1_pool_dma));

    // set MAC Beacon TX pointer
    MACvSetCurrBCNTxDescAddr(pDevice->PortOffset,
                        (pDevice->tx_beacon_dma));

}



/*+
 *
 * Description:
 *      Reset Rx
 *
 * Parameters:
 *  In:
 *      pDevice     - Pointer to the adapter
 *  Out:
 *      none
 *
 * Return Value: none
 *
-*/
void
CARDvSafeResetRx (
    void *pDeviceHandler
    )
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    unsigned int uu;
    PSRxDesc    pDesc;



    // initialize RD index
    pDevice->pCurrRD[0]=&(pDevice->aRD0Ring[0]);
    pDevice->pCurrRD[1]=&(pDevice->aRD1Ring[0]);

    // init state, all RD is chip's
    for (uu = 0; uu < pDevice->sOpts.nRxDescs0; uu++) {
        pDesc =&(pDevice->aRD0Ring[uu]);
        pDesc->m_rd0RD0.wResCount = (WORD)(pDevice->rx_buf_sz);
        pDesc->m_rd0RD0.f1Owner=OWNED_BY_NIC;
        pDesc->m_rd1RD1.wReqCount = (WORD)(pDevice->rx_buf_sz);
    }

    // init state, all RD is chip's
    for (uu = 0; uu < pDevice->sOpts.nRxDescs1; uu++) {
        pDesc =&(pDevice->aRD1Ring[uu]);
        pDesc->m_rd0RD0.wResCount = (WORD)(pDevice->rx_buf_sz);
        pDesc->m_rd0RD0.f1Owner=OWNED_BY_NIC;
        pDesc->m_rd1RD1.wReqCount = (WORD)(pDevice->rx_buf_sz);
    }

    pDevice->cbDFCB = CB_MAX_RX_FRAG;
    pDevice->cbFreeDFCB = pDevice->cbDFCB;

    // set perPkt mode
    MACvRx0PerPktMode(pDevice->PortOffset);
    MACvRx1PerPktMode(pDevice->PortOffset);
    // set MAC RD pointer
    MACvSetCurrRx0DescAddr(pDevice->PortOffset,
                            pDevice->rd0_pool_dma);

    MACvSetCurrRx1DescAddr(pDevice->PortOffset,
                            pDevice->rd1_pool_dma);
}




/*
 * Description: Get response Control frame rate in CCK mode
 *
 * Parameters:
 *  In:
 *      pDevice             - The adapter to be set
 *      wRateIdx            - Receiving data rate
 *  Out:
 *      none
 *
 * Return Value: response Control frame rate
 *
 */
WORD CARDwGetCCKControlRate(void *pDeviceHandler, WORD wRateIdx)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    unsigned int ui = (unsigned int) wRateIdx;

    while (ui > RATE_1M) {
        if (pDevice->wBasicRate & ((WORD)1 << ui)) {
            return (WORD)ui;
        }
        ui --;
    }
    return (WORD)RATE_1M;
}

/*
 * Description: Get response Control frame rate in OFDM mode
 *
 * Parameters:
 *  In:
 *      pDevice             - The adapter to be set
 *      wRateIdx            - Receiving data rate
 *  Out:
 *      none
 *
 * Return Value: response Control frame rate
 *
 */
WORD CARDwGetOFDMControlRate (void *pDeviceHandler, WORD wRateIdx)
{
    PSDevice pDevice = (PSDevice) pDeviceHandler;
    unsigned int ui = (unsigned int) wRateIdx;

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"BASIC RATE: %X\n", pDevice->wBasicRate);

    if (!CARDbIsOFDMinBasicRate((void *)pDevice)) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"CARDwGetOFDMControlRate:(NO OFDM) %d\n", wRateIdx);
        if (wRateIdx > RATE_24M)
            wRateIdx = RATE_24M;
        return wRateIdx;
    }
    while (ui > RATE_11M) {
        if (pDevice->wBasicRate & ((WORD)1 << ui)) {
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"CARDwGetOFDMControlRate : %d\n", ui);
            return (WORD)ui;
        }
        ui --;
    }
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"CARDwGetOFDMControlRate: 6M\n");
    return (WORD)RATE_24M;
}


/*
 * Description: Set RSPINF
 *
 * Parameters:
 *  In:
 *      pDevice             - The adapter to be set
 *  Out:
 *      none
 *
 * Return Value: None.
 *
 */
void CARDvSetRSPINF (void *pDeviceHandler, CARD_PHY_TYPE ePHYType)
{
    PSDevice pDevice = (PSDevice) pDeviceHandler;
    BYTE  byServ = 0x00, bySignal = 0x00; //For CCK
    WORD  wLen = 0x0000;
    BYTE  byTxRate, byRsvTime;             //For OFDM

    //Set to Page1
    MACvSelectPage1(pDevice->PortOffset);

    //RSPINF_b_1
    BBvCaculateParameter(pDevice,
                         14,
                         CARDwGetCCKControlRate((void *)pDevice, RATE_1M),
                         PK_TYPE_11B,
                         &wLen,
                         &byServ,
                         &bySignal
    );

    VNSvOutPortD(pDevice->PortOffset + MAC_REG_RSPINF_B_1, MAKEDWORD(wLen,MAKEWORD(bySignal,byServ)));
    ///RSPINF_b_2
    BBvCaculateParameter(pDevice,
                         14,
                         CARDwGetCCKControlRate((void *)pDevice, RATE_2M),
                         PK_TYPE_11B,
                         &wLen,
                         &byServ,
                         &bySignal
    );

    VNSvOutPortD(pDevice->PortOffset + MAC_REG_RSPINF_B_2, MAKEDWORD(wLen,MAKEWORD(bySignal,byServ)));
    //RSPINF_b_5
    BBvCaculateParameter(pDevice,
                         14,
                         CARDwGetCCKControlRate((void *)pDevice, RATE_5M),
                         PK_TYPE_11B,
                         &wLen,
                         &byServ,
                         &bySignal
    );

    VNSvOutPortD(pDevice->PortOffset + MAC_REG_RSPINF_B_5, MAKEDWORD(wLen,MAKEWORD(bySignal,byServ)));
    //RSPINF_b_11
    BBvCaculateParameter(pDevice,
                         14,
                         CARDwGetCCKControlRate((void *)pDevice, RATE_11M),
                         PK_TYPE_11B,
                         &wLen,
                         &byServ,
                         &bySignal
    );

    VNSvOutPortD(pDevice->PortOffset + MAC_REG_RSPINF_B_11, MAKEDWORD(wLen,MAKEWORD(bySignal,byServ)));
    //RSPINF_a_6
    s_vCaculateOFDMRParameter(RATE_6M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_6, MAKEWORD(byTxRate,byRsvTime));
    //RSPINF_a_9
    s_vCaculateOFDMRParameter(RATE_9M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_9, MAKEWORD(byTxRate,byRsvTime));
    //RSPINF_a_12
    s_vCaculateOFDMRParameter(RATE_12M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_12, MAKEWORD(byTxRate,byRsvTime));
    //RSPINF_a_18
    s_vCaculateOFDMRParameter(RATE_18M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
   VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_18, MAKEWORD(byTxRate,byRsvTime));
    //RSPINF_a_24
    s_vCaculateOFDMRParameter(RATE_24M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_24, MAKEWORD(byTxRate,byRsvTime));
    //RSPINF_a_36
    s_vCaculateOFDMRParameter(CARDwGetOFDMControlRate((void *)pDevice, RATE_36M),
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_36, MAKEWORD(byTxRate,byRsvTime));
    //RSPINF_a_48
    s_vCaculateOFDMRParameter(CARDwGetOFDMControlRate((void *)pDevice, RATE_48M),
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_48, MAKEWORD(byTxRate,byRsvTime));
    //RSPINF_a_54
    s_vCaculateOFDMRParameter(CARDwGetOFDMControlRate((void *)pDevice, RATE_54M),
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_54, MAKEWORD(byTxRate,byRsvTime));

    //RSPINF_a_72
    s_vCaculateOFDMRParameter(CARDwGetOFDMControlRate((void *)pDevice, RATE_54M),
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_72, MAKEWORD(byTxRate,byRsvTime));
    //Set to Page0
    MACvSelectPage0(pDevice->PortOffset);
}

/*
 * Description: Update IFS
 *
 * Parameters:
 *  In:
 *      pDevice             - The adapter to be set
 *  Out:
 *      none
 *
 * Return Value: None.
 *
 */
void vUpdateIFS (void *pDeviceHandler)
{
    //Set SIFS, DIFS, EIFS, SlotTime, CwMin
    PSDevice pDevice = (PSDevice) pDeviceHandler;

    BYTE byMaxMin = 0;
    if (pDevice->byPacketType==PK_TYPE_11A) {//0000 0000 0000 0000,11a
        pDevice->uSlot = C_SLOT_SHORT;
        pDevice->uSIFS = C_SIFS_A;
        pDevice->uDIFS = C_SIFS_A + 2*C_SLOT_SHORT;
        pDevice->uCwMin = C_CWMIN_A;
        byMaxMin = 4;
    }
    else if (pDevice->byPacketType==PK_TYPE_11B) {//0000 0001 0000 0000,11b
        pDevice->uSlot = C_SLOT_LONG;
        pDevice->uSIFS = C_SIFS_BG;
        pDevice->uDIFS = C_SIFS_BG + 2*C_SLOT_LONG;
	    pDevice->uCwMin = C_CWMIN_B;
        byMaxMin = 5;
    }
    else { // PK_TYPE_11GA & PK_TYPE_11GB
        pDevice->uSIFS = C_SIFS_BG;
        if (pDevice->bShortSlotTime) {
            pDevice->uSlot = C_SLOT_SHORT;
        } else {
	        pDevice->uSlot = C_SLOT_LONG;
	    }
	    pDevice->uDIFS = C_SIFS_BG + 2*pDevice->uSlot;
        if (pDevice->wBasicRate & 0x0150) { //0000 0001 0101 0000,24M,12M,6M
            pDevice->uCwMin = C_CWMIN_A;
            byMaxMin = 4;
        }
        else {
            pDevice->uCwMin = C_CWMIN_B;
            byMaxMin = 5;
        }
    }

    pDevice->uCwMax = C_CWMAX;
    pDevice->uEIFS = C_EIFS;
    if (pDevice->byRFType == RF_RFMD2959) {
        // bcs TX_PE will reserve 3 us
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_SIFS, (BYTE)(pDevice->uSIFS - 3));
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_DIFS, (BYTE)(pDevice->uDIFS - 3));
    } else {
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_SIFS, (BYTE)pDevice->uSIFS);
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_DIFS, (BYTE)pDevice->uDIFS);
    }
    VNSvOutPortB(pDevice->PortOffset + MAC_REG_EIFS, (BYTE)pDevice->uEIFS);
    VNSvOutPortB(pDevice->PortOffset + MAC_REG_SLOT, (BYTE)pDevice->uSlot);
    byMaxMin |= 0xA0;//1010 1111,C_CWMAX = 1023
    VNSvOutPortB(pDevice->PortOffset + MAC_REG_CWMAXMIN0, (BYTE)byMaxMin);
}

void CARDvUpdateBasicTopRate (void *pDeviceHandler)
{
    PSDevice pDevice = (PSDevice) pDeviceHandler;
    BYTE byTopOFDM = RATE_24M, byTopCCK = RATE_1M;
    BYTE ii;

     //Determines the highest basic rate.
     for (ii = RATE_54M; ii >= RATE_6M; ii --) {
         if ( (pDevice->wBasicRate) & ((WORD)(1<<ii)) ) {
             byTopOFDM = ii;
             break;
         }
     }
     pDevice->byTopOFDMBasicRate = byTopOFDM;

     for (ii = RATE_11M;; ii --) {
         if ( (pDevice->wBasicRate) & ((WORD)(1<<ii)) ) {
             byTopCCK = ii;
             break;
         }
         if (ii == RATE_1M)
            break;
     }
     pDevice->byTopCCKBasicRate = byTopCCK;
}


/*
 * Description: Set NIC Tx Basic Rate
 *
 * Parameters:
 *  In:
 *      pDevice         - The adapter to be set
 *      wBasicRate      - Basic Rate to be set
 *  Out:
 *      none
 *
 * Return Value: TRUE if succeeded; FALSE if failed.
 *
 */
BOOL CARDbAddBasicRate (void *pDeviceHandler, WORD wRateIdx)
{
    PSDevice pDevice = (PSDevice) pDeviceHandler;
    WORD wRate = (WORD)(1<<wRateIdx);

    pDevice->wBasicRate |= wRate;

    //Determines the highest basic rate.
    CARDvUpdateBasicTopRate((void *)pDevice);

    return(TRUE);
}

BOOL CARDbIsOFDMinBasicRate (void *pDeviceHandler)
{
    PSDevice pDevice = (PSDevice) pDeviceHandler;
    int ii;

    for (ii = RATE_54M; ii >= RATE_6M; ii --) {
        if ((pDevice->wBasicRate) & ((WORD)(1<<ii)))
            return TRUE;
    }
    return FALSE;
}

BYTE CARDbyGetPktType (void *pDeviceHandler)
{
    PSDevice pDevice = (PSDevice) pDeviceHandler;

    if (pDevice->byBBType == BB_TYPE_11A || pDevice->byBBType == BB_TYPE_11B) {
        return (BYTE)pDevice->byBBType;
    }
    else if (CARDbIsOFDMinBasicRate((void *)pDevice)) {
        return PK_TYPE_11GA;
    }
    else {
    	return PK_TYPE_11GB;
    }
}

/*
 * Description: Set NIC Loopback mode
 *
 * Parameters:
 *  In:
 *      pDevice         - The adapter to be set
 *      wLoopbackMode   - Loopback mode to be set
 *  Out:
 *      none
 *
 * Return Value: none
 *
 */
void CARDvSetLoopbackMode (DWORD_PTR dwIoBase, WORD wLoopbackMode)
{
    switch(wLoopbackMode) {
    case CARD_LB_NONE:
    case CARD_LB_MAC:
    case CARD_LB_PHY:
        break;
    default:
        ASSERT(FALSE);
        break;
    }
    // set MAC loopback
    MACvSetLoopbackMode(dwIoBase, LOBYTE(wLoopbackMode));
    // set Baseband loopback
}


/*
 * Description: Software Reset NIC
 *
 * Parameters:
 *  In:
 *      pDevice         - The adapter to be reset
 *  Out:
 *      none
 *
 * Return Value: none
 *
 */
BOOL CARDbSoftwareReset (void *pDeviceHandler)
{
    PSDevice pDevice = (PSDevice) pDeviceHandler;

    // reset MAC
    if (!MACbSafeSoftwareReset(pDevice->PortOffset))
        return FALSE;

    return TRUE;
}


/*
 * Description: Caculate TSF offset of two TSF input
 *              Get TSF Offset from RxBCN's TSF and local TSF
 *
 * Parameters:
 *  In:
 *      pDevice         - The adapter to be sync.
 *      qwTSF1          - Rx BCN's TSF
 *      qwTSF2          - Local TSF
 *  Out:
 *      none
 *
 * Return Value: TSF Offset value
 *
 */
QWORD CARDqGetTSFOffset (BYTE byRxRate, QWORD qwTSF1, QWORD qwTSF2)
{
    QWORD   qwTSFOffset;
    WORD    wRxBcnTSFOffst= 0;;

    HIDWORD(qwTSFOffset) = 0;
    LODWORD(qwTSFOffset) = 0;
    wRxBcnTSFOffst = cwRXBCNTSFOff[byRxRate%MAX_RATE];
    (qwTSF2).u.dwLowDword += (DWORD)(wRxBcnTSFOffst);
    if ((qwTSF2).u.dwLowDword < (DWORD)(wRxBcnTSFOffst)) {
        (qwTSF2).u.dwHighDword++;
    }
    LODWORD(qwTSFOffset) = LODWORD(qwTSF1) - LODWORD(qwTSF2);
    if (LODWORD(qwTSF1) < LODWORD(qwTSF2)) {
        // if borrow needed
        HIDWORD(qwTSFOffset) = HIDWORD(qwTSF1) - HIDWORD(qwTSF2) - 1 ;
    }
    else {
        HIDWORD(qwTSFOffset) = HIDWORD(qwTSF1) - HIDWORD(qwTSF2);
    };
    return (qwTSFOffset);
}


/*
 * Description: Read NIC TSF counter
 *              Get local TSF counter
 *
 * Parameters:
 *  In:
 *      pDevice         - The adapter to be read
 *  Out:
 *      qwCurrTSF       - Current TSF counter
 *
 * Return Value: TRUE if success; otherwise FALSE
 *
 */
BOOL CARDbGetCurrentTSF (DWORD_PTR dwIoBase, PQWORD pqwCurrTSF)
{
    WORD    ww;
    BYTE    byData;

    MACvRegBitsOn(dwIoBase, MAC_REG_TFTCTL, TFTCTL_TSFCNTRRD);
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        VNSvInPortB(dwIoBase + MAC_REG_TFTCTL, &byData);
        if ( !(byData & TFTCTL_TSFCNTRRD))
            break;
    }
    if (ww == W_MAX_TIMEOUT)
        return(FALSE);
    VNSvInPortD(dwIoBase + MAC_REG_TSFCNTR, &LODWORD(*pqwCurrTSF));
    VNSvInPortD(dwIoBase + MAC_REG_TSFCNTR + 4, &HIDWORD(*pqwCurrTSF));

    return(TRUE);
}


/*
 * Description: Read NIC TSF counter
 *              Get NEXTTBTT from adjusted TSF and Beacon Interval
 *
 * Parameters:
 *  In:
 *      qwTSF           - Current TSF counter
 *      wbeaconInterval - Beacon Interval
 *  Out:
 *      qwCurrTSF       - Current TSF counter
 *
 * Return Value: TSF value of next Beacon
 *
 */
QWORD CARDqGetNextTBTT (QWORD qwTSF, WORD wBeaconInterval)
{

    unsigned int uLowNextTBTT;
    unsigned int uHighRemain, uLowRemain;
    unsigned int uBeaconInterval;

    uBeaconInterval = wBeaconInterval * 1024;
    // Next TBTT = ((local_current_TSF / beacon_interval) + 1 ) * beacon_interval
    uLowNextTBTT = (LODWORD(qwTSF) >> 10) << 10;
    // low dword (mod) bcn
    uLowRemain = (uLowNextTBTT) % uBeaconInterval;
//    uHighRemain = ((0x80000000 % uBeaconInterval)* 2 * HIDWORD(qwTSF))
//                  % uBeaconInterval;
    // high dword (mod) bcn
    uHighRemain = (((0xffffffff % uBeaconInterval) + 1) * HIDWORD(qwTSF))
                  % uBeaconInterval;
    uLowRemain = (uHighRemain + uLowRemain) % uBeaconInterval;
    uLowRemain = uBeaconInterval - uLowRemain;

    // check if carry when add one beacon interval
    if ((~uLowNextTBTT) < uLowRemain)
        HIDWORD(qwTSF) ++ ;

    LODWORD(qwTSF) = uLowNextTBTT + uLowRemain;

    return (qwTSF);
}


/*
 * Description: Set NIC TSF counter for first Beacon time
 *              Get NEXTTBTT from adjusted TSF and Beacon Interval
 *
 * Parameters:
 *  In:
 *      dwIoBase        - IO Base
 *      wBeaconInterval - Beacon Interval
 *  Out:
 *      none
 *
 * Return Value: none
 *
 */
void CARDvSetFirstNextTBTT (DWORD_PTR dwIoBase, WORD wBeaconInterval)
{

    QWORD   qwNextTBTT;

    HIDWORD(qwNextTBTT) = 0;
    LODWORD(qwNextTBTT) = 0;
    CARDbGetCurrentTSF(dwIoBase, &qwNextTBTT); //Get Local TSF counter
    qwNextTBTT = CARDqGetNextTBTT(qwNextTBTT, wBeaconInterval);
    // Set NextTBTT
    VNSvOutPortD(dwIoBase + MAC_REG_NEXTTBTT, LODWORD(qwNextTBTT));
    VNSvOutPortD(dwIoBase + MAC_REG_NEXTTBTT + 4, HIDWORD(qwNextTBTT));
    MACvRegBitsOn(dwIoBase, MAC_REG_TFTCTL, TFTCTL_TBTTSYNCEN);
    //DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Card:First Next TBTT[%8xh:%8xh] \n", HIDWORD(qwNextTBTT), LODWORD(qwNextTBTT));
    return;
}


/*
 * Description: Sync NIC TSF counter for Beacon time
 *              Get NEXTTBTT and write to HW
 *
 * Parameters:
 *  In:
 *      pDevice         - The adapter to be set
 *      qwTSF           - Current TSF counter
 *      wBeaconInterval - Beacon Interval
 *  Out:
 *      none
 *
 * Return Value: none
 *
 */
void CARDvUpdateNextTBTT (DWORD_PTR dwIoBase, QWORD qwTSF, WORD wBeaconInterval)
{

    qwTSF = CARDqGetNextTBTT(qwTSF, wBeaconInterval);
    // Set NextTBTT
    VNSvOutPortD(dwIoBase + MAC_REG_NEXTTBTT, LODWORD(qwTSF));
    VNSvOutPortD(dwIoBase + MAC_REG_NEXTTBTT + 4, HIDWORD(qwTSF));
    MACvRegBitsOn(dwIoBase, MAC_REG_TFTCTL, TFTCTL_TBTTSYNCEN);
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Card:Update Next TBTT[%8xh:%8xh] \n",
		    (unsigned int) HIDWORD(qwTSF), (unsigned int) LODWORD(qwTSF));

    return;
}







