/* -*- mode: c; c-basic-offset: 4 -*- */
#ifndef CLICK_FDDI_H
#define CLICK_FDDI_H

/*
 * <click/fddi.h> -- our own definitions of FDDI headers
 * based on a file from Linux
 */

struct click_fddi {
    uint8_t	fc;
    uint8_t	daddr[6];
    uint8_t	saddr[6];
} __attribute__ ((packed));

struct click_fddi_8022_1 {
    struct click_fddi h;
    uint8_t	dsap;
    uint8_t	ssap;
    uint8_t	ctrl;
} __attribute__ ((packed));

struct click_fddi_8022_2 {
    struct click_fddi h;
    uint8_t	dsap;
    uint8_t	ssap;
    uint8_t	ctrl1;
    uint8_t	ctrl2;
} __attribute__ ((packed));

struct click_fddi_snap {
    struct click_fddi h;
    uint8_t	dsap;
    uint8_t	ssap;
    uint8_t	ctrl;
    uint8_t	oui[3];
    uint16_t	ether_type;
} __attribute__ ((packed));

#define	FDDI_FC_LLC_ASYNC	0x50
#define	FDDI_FC_LLCMASK 	0xF0    	/* length/class/format bits */

#define FDDI_SNAP_EXPECTED	"\xAA\xAA\x03\x00\x00\x00"
#define FDDI_SNAP_EXPECTED_LEN	6

#endif
