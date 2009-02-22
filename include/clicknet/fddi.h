/* -*- mode: c; c-basic-offset: 4 -*- */
#ifndef CLICKNET_FDDI_H
#define CLICKNET_FDDI_H

/*
 * <clicknet/fddi.h> -- our own definitions of FDDI headers
 * based on a file from Linux
 */

struct click_fddi {
    uint8_t	fc;
    uint8_t	daddr[6];
    uint8_t	saddr[6];
} CLICK_SIZE_PACKED_ATTRIBUTE;

struct click_fddi_8022_1 {
    struct click_fddi h;
    uint8_t	dsap;
    uint8_t	ssap;
    uint8_t	ctrl;
} CLICK_SIZE_PACKED_ATTRIBUTE;

struct click_fddi_8022_2 {
    struct click_fddi h;
    uint8_t	dsap;
    uint8_t	ssap;
    uint8_t	ctrl1;
    uint8_t	ctrl2;
} CLICK_SIZE_PACKED_ATTRIBUTE;

struct click_fddi_snap {
    struct click_fddi h;
    uint8_t	dsap;
    uint8_t	ssap;
    uint8_t	ctrl;
    uint8_t	oui[3];
    uint16_t	ether_type;
} CLICK_SIZE_PACKED_ATTRIBUTE;

#define	FDDI_FC_LLC_ASYNC	0x50
#define	FDDI_FC_LLCMASK 	0xF0    	/* length/class/format bits */

#endif
