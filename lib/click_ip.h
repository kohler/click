#ifndef CLICK_IP_H
#define CLICK_IP_H
#include "glue.hh"

/*
 * click_ip.h -- our own definitions of IP headers
 * based on a file from one of the BSDs
 */

#define IPVERSION 4

#ifndef __BYTE_ORDER
#define __LITTLE_ENDIAN 1234
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif

struct click_ip {
#if __BYTE_ORDER == __LITTLE_ENDIAN
  unsigned char ip_hl:4;		/* 0     header length */
  unsigned char ip_v:4;			/*       & version */
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
  unsigned char ip_v:4;			/* 0     version */
  unsigned char ip_hl:4;		/*       & header length */
#endif
  unsigned char ip_tos;			/* 1     type of service */
  unsigned short ip_len;		/* 2-3   total length */
  unsigned short ip_id;			/* 4-5   identification */
  unsigned short ip_off;		/* 6-7   fragment offset field */
#define	IP_RF 0x8000			/* reserved fragment flag */
#define	IP_DF 0x4000			/* dont fragment flag */
#define	IP_MF 0x2000			/* more fragments flag */
#define	IP_OFFMASK 0x1fff		/* mask for fragmenting bits */
  unsigned char ip_ttl;			/* 8     time to live */
  unsigned char ip_p;			/* 9     protocol */
  unsigned short ip_sum;		/* 10-11 checksum */
  struct in_addr ip_src;		/* 12-15 source address */
  struct in_addr ip_dst;		/* 16-19 destination address */
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
unsigned short in_cksum(unsigned char *addr, int len);
#endif

#endif
