/* -*- mode: c; c-basic-offset: 4 -*- */
#ifndef CLICK_RFC1483_H
#define CLICK_RFC1483_H

/*
 * <click/rfc1483.h>
 */

struct click_rfc1483 {
    uint8_t	snap[6];
    uint16_t	ether_type;
} __attribute__ ((packed));

#define RFC1483_SNAP_EXPECTED		"\xAA\xAA\x03\x00\x00\x00"
#define RFC1483_SNAP_EXPECTED_LEN	6

#endif
