/* -*- mode: c; c-basic-offset: 4 -*- */
#ifndef CLICKNET_RFC1483_H
#define CLICKNET_RFC1483_H

/*
 * <clicknet/rfc1483.h>
 */

struct click_rfc1483 {
    uint8_t	snap[6];
    uint16_t	ether_type;
};

#define RFC1483_SNAP_EXPECTED		"\xAA\xAA\x03\x00\x00\x00"
#define RFC1483_SNAP_EXPECTED_LEN	6

#endif
