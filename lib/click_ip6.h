#ifndef CLICK_IP6_H
#define CLICK_IP6_H
#include "click_ip.h"

/*
 * click_ip6.h -- our own definitions of IP6 headers
 * based on RFC 2460
 */

/* IPv6 address , same as from /usr/include/netinet/in.h  */
struct click_in6_addr {
  union {
    unsigned char       u6_addr8[16];
    unsigned short      u6_addr16[8];
    unsigned int        u6_addr32[4];
#if ULONG_MAX > 0xffffffff
    unsigned long long  u6_addr64[2];
#endif
  } in6_u;
#define s6_addr in6_u.u6_addr8
#define s6_addr16 in6_u.u6_addr16
#define s6_addr32 in6_u.u6_addr32
#define s6_addr64 in6_u.u6_addr64
};

struct click_ip6 {
#if __BYTE_ORDER == __LITTLE_ENDIAN
  unsigned char ip6_pri:4;		/* 0     priority */
  unsigned char ip6_v:4;		/*       version == 6 */
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
  unsigned char ip6_v:4;		/* 0     version == 6 */
  unsigned char ip6_pri:4;		/*       priority */
#endif
  unsigned char ip6_flow[3];		/* 1-3   flow label */
  unsigned short ip6_plen;		/* 4-5   payload length */
  unsigned char ip6_nxt;		/* 6     next header */
  unsigned char ip6_hlim;	     	/* 7     hop limit  */
  struct click_in6_addr ip6_src;	/* 8-23  source address */
  struct click_in6_addr ip6_dst;	/* 24-39 dest address */
};


unsigned short in6_fast_cksum(const struct click_in6_addr *saddr,
			      const struct click_in6_addr *daddr,
			      unsigned short len,
			      unsigned short proto,
			      unsigned short ori_csum,
			      const unsigned char *addr,
			      unsigned short len2);

unsigned short in6_cksum(struct click_in6_addr *saddr,
			 struct click_in6_addr *daddr,
			 unsigned short len,
			 unsigned short proto,
			 unsigned short ori_csum,
			 unsigned char *addr,
			 unsigned short len2);

#endif
