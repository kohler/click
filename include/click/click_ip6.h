/* -*- mode: c; c-basic-offset: 4 -*- */
#ifndef CLICK_IP6_H
#define CLICK_IP6_H
#include <click/click_ip.h>

/*
 * click_ip6.h -- our own definitions of IP6 headers
 * based on RFC 2460
 */

/* IPv6 address , same as from /usr/include/netinet/in.h  */
struct click_in6_addr {
    union {
	uint8_t		u6_addr8[16];
	uint16_t	u6_addr16[8];
	uint32_t	u6_addr32[4];
#ifdef HAVE_INT64_TYPES
	uint64_t	u6_addr64[2];
#endif
    } in6_u;
#define s6_addr in6_u.u6_addr8
#define s6_addr16 in6_u.u6_addr16
#define s6_addr32 in6_u.u6_addr32
#define s6_addr64 in6_u.u6_addr64
};

struct click_ip6 {
#if CLICK_BYTE_ORDER == CLICK_LITTLE_ENDIAN
    uint8_t ip6_pri : 4;		/* 0     priority */
    uint8_t ip6_v : 4;			/*       version == 6 */
#endif
#if CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN
    uint8_t ip6_v : 4;			/* 0     version == 6 */
    uint8_t ip6_pri : 4;		/*       priority */
#endif
    uint8_t ip6_flow[3];		/* 1-3   flow label */
    uint16_t ip6_plen;			/* 4-5   payload length */
    uint8_t ip6_nxt;			/* 6     next header */
    uint8_t ip6_hlim;	     		/* 7     hop limit  */
    struct click_in6_addr ip6_src;	/* 8-23  source address */
    struct click_in6_addr ip6_dst;	/* 24-39 dest address */
};

unsigned short 
in_ip4_cksum(const unsigned  saddr,
	     const unsigned  daddr,
	     unsigned short len,
	     unsigned char proto,
	     unsigned short ori_csum,
	     const unsigned char *addr,
	     unsigned short len2);


unsigned short in6_fast_cksum(const struct click_in6_addr *saddr,
			      const struct click_in6_addr *daddr,
			      unsigned short len,
			      unsigned char  proto,
			      unsigned short ori_csum,
			      const unsigned char *addr,
			      unsigned short len2);

unsigned short in6_cksum(struct click_in6_addr *saddr,
			 struct click_in6_addr *daddr,
			 unsigned short len,
			 unsigned char  proto,
			 unsigned short ori_csum,
			 unsigned char *addr,
			 unsigned short len2);

#endif
