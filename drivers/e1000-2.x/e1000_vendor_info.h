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

/* ====================================================================== */
/*                              vendor_info                               */
/* ====================================================================== */

/* 
 * vendor_info_array
 *
 * This array contains the list of Subsystem IDs on which the driver
 * should load.
 *
 * The format of each entry of the array is as follows:
 * { SUBSYSTEM_VENDOR, SUBSYSTEM_DEV, BRANDING_STRING }
 *
 * If there is a CATCHALL in the SUBSYSTEM_DEV field, the driver will
 * load on all subsystem device IDs of that vendor.
 * If there is a CATCHALL in the SUBSYSTEM_VENDOR field, the driver will
 * load on all cards, irrespective of the vendor.
 *
 * The last entry of the array must be:
 * { 0, 0, "" }
 */

#ifndef CATCHALL
#define CATCHALL 0xffff
#endif

#ifndef _VENDOR_INFO_T
#define _VENDOR_INFO_T
typedef struct _e1000_vendor_info_t
{
    uint16_t dev;
    uint16_t sub_ven;
    uint16_t sub_dev;
    char *idstr;                      /* String to be printed for these IDs */
}
e1000_vendor_info_t;
#endif /* _VENDOR_INFO_T */

e1000_vendor_info_t e1000_vendor_info_array[] = {
    {0x1000, 0x8086, 0x1000, "Intel(R) PRO/1000 Gigabit Server Adapter"},
    {0x1001, 0x8086, 0x1003, "Intel(R) PRO/1000 F Server Adapter"},
    {0x1004, 0x8086, 0x1004, "Intel(R) PRO/1000 T Server Adapter"},
    {0x1000, 0x1014, 0x0119, "IBM Netfinity Gigabit Ethernet SX Adapter"},
    {0x1001, 0x1014, 0x01EA, "IBM Gigabit Ethernet SX Server Adapter"},
    {0x1004, 0x1014, 0x10F2, "IBM Gigabit Ethernet Server Adapter"},
    {0x1000, CATCHALL, CATCHALL, "Intel 82542-based Gigabit Server Adapter"},
    {0x1001, CATCHALL, CATCHALL, "Intel 82543GC-based F Gigabit Adapter"},
    {0x1004, CATCHALL, CATCHALL, "Intel 82543GC-based T Gigabit Adapter"},
    {0x0, 0x0, 0x0, ""}                      /* This has to be the last entry */
};

