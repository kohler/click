#ifndef CLICK_IP6_H
#define CLICK_IP6_H
#include "glue.hh"
#include "ip6address.hh"

/*
 * click_ip6.h -- our own definitions of IP6 headers
 * based on a file from one of the BSDs
 */

#define IPVERSION 6

#ifndef __BYTE_ORDER
#define __LITTLE_ENDIAN 1234
#define __BYTE_ORDER __LITTLE_ENDIAN
//#define __BYTE_ORDER __BIG_ENDIAN
#endif

struct click_ip6 {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    unsigned char ip6_pri:4;			/* 0   priority */
    unsigned char ip6_v:4;			/*     version */
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
    unsigned char ip6_v:4;			/* 0   version */
    unsigned char ip6_pri:4;			/*     priority */
#endif
    uint8_t ip6_flow[3];             /* 1-3   flow label -follow the example from ip6.h*/
    unsigned short ip6_plen;         /* 4-5   payload length */
    unsigned char ip6_nxt;           /* 6     next header */
    unsigned char ip6_hlim;          /* 7     hop limit  */
    IP6Address ip6_src;              /* 8-23  source address 16 bytes */
    IP6Address ip6_dst;              /* 24-39 dest address - 16 bytes */
};

/* ip_protocol */
#define IP_PROTO_ICMP		1
#define IP_PROTO_IGMP		2
#define IP_PROTO_GGP		3
#define IP_PROTO_IPIP		4
#define IP_PROTO_ST		5
#define IP_PROTO_TCP		6
#define IP_PROTO_UCL		7
#define IP_PROTO_EGP		8
#define IP_PROTO_IGP		9
#define IP_PROTO_BBN		10
#define IP_PROTO_NVPII		11
#define IP_PROTO_PUP		12
#define IP_PROTO_ARGUS		13
#define IP_PROTO_EMCON		14
#define IP_PROTO_XNET		15
#define IP_PROTO_CHAOS		16
#define IP_PROTO_UDP		17
#define IP_PROTO_MUX		18
#define IP_PROTO_DCN		19
#define IP_PROTO_HMP		20
#define IP_PROTO_PRM		21
#define IP_PROTO_XNS		22
#define IP_PROTO_TRUNK1		23
#define IP_PROTO_TRUNK2		24
#define IP_PROTO_LEAF1		25
#define IP_PROTO_LEAF2		26
#define IP_PROTO_RDP		27
#define IP_PROTO_IRTP		28
#define IP_PROTO_ISOTP4		29
#define IP_PROTO_NETBLT		30
#define IP_PROTO_MFENSP		31
#define IP_PROTO_MERIT		32
#define IP_PROTO_SEP		33
#define IP_PROTO_ICMP6          58
#define IP_PROTO_CFTP		62
#define IP_PROTO_SATNET		64
#define IP_PROTO_MITSUBNET	65
#define IP_PROTO_RVD		66
#define IP_PROTO_IPPC		67
#define IP_PROTO_SATMON		69
#define IP_PROTO_IPCV		71
#define IP_PROTO_BRSATMON	76
#define IP_PROTO_WBMON		78
#define IP_PROTO_WBEXPAK	79

#define	IPOPT_EOL		0		/* end of option list */
#define	IPOPT_NOP		1		/* no operation */
#define IPOPT_RR        7       /* record packet route */
#define IPOPT_TS        68      /* timestamp */
#define IPOPT_SECURITY      130     /* provide s,c,h,tcc */
#define IPOPT_LSRR      131     /* loose source route */
#define IPOPT_SATID     136     /* satnet id */
#define IPOPT_SSRR      137     /* strict source route */
#define IPOPT_RA        148     /* router alert */

#ifdef CLICK_LINUXMODULE
# define new xxx_new
# include <net/checksum.h>
# undef new
# define in_cksum(addr, len)	ip_compute_csum(addr, len)
#else
# ifdef __cplusplus
extern "C"
# endif
unsigned short in_cksum(const unsigned char *addr, int len);
#endif

#endif
