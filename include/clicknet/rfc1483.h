/* -*- mode: c; c-basic-offset: 4 -*- */
#ifndef CLICKNET_RFC1483_H
#define CLICKNET_RFC1483_H

/*
 * <clicknet/rfc1483.h>
 */

struct click_rfc1483 {
    uint8_t	dsap;
    uint8_t	ssap;
    uint8_t	ui;
    uint8_t	orgcode[3];
    uint16_t	ether_type;
};

#define RFC1483_SNAP_IP_EXPECTED	"\xAA\xAA\x03\x00\x00\x00"
#define RFC1483_SNAP_IP_EXPECTED_LEN	6

#endif
