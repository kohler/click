/* -*- mode: c; c-basic-offset: 4 -*- */
#ifndef CLICKNET_IP_H
#define CLICKNET_IP_H
/* get struct in_addr */
#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#if CLICK_LINUXMODULE
# include <net/checksum.h>
# include <linux/in.h>
#else
# include <sys/types.h>
# include <netinet/in.h>
#endif

/*
 * <clicknet/ip.h> -- IP header definitions, based on one of the BSDs.
 *
 * Relevant RFCs include:
 *   RFC791	Internet Protocol
 *   RFC3168	The Addition of Explicit Congestion Notification (ECN) to IP
 */

#define IP_ADDR_LEN 4

struct click_ip {
#if CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN
    unsigned	ip_v : 4;		/* 0     version == 4		     */
    unsigned	ip_hl : 4;		/*       header length		     */
#elif CLICK_BYTE_ORDER == CLICK_LITTLE_ENDIAN
    unsigned	ip_hl : 4;		/* 0     header length		     */
    unsigned	ip_v : 4;		/*       version == 4		     */
#else
#   error "unknown byte order"
#endif
    uint8_t	ip_tos;			/* 1     type of service	     */
#define IP_DSCPMASK	0xFC		/*         diffserv code point	     */
#define IP_ECNMASK	0x03		/*	   ECN code point	     */
#define   IP_ECN_NOT_ECT  0x00		/*         not ECN capable transport */
#define   IP_ECN_ECT1	  0x01		/*         ECN capable transport     */
#define   IP_ECN_ECT2	  0x02		/*         ECN capable transport     */
#define   IP_ECN_CE	  0x03		/*         ECN congestion exp'd	     */
    uint16_t	ip_len;			/* 2-3   total length		     */
    uint16_t	ip_id;			/* 4-5   identification		     */
    uint16_t	ip_off;			/* 6-7   fragment offset field	     */
#define	IP_RF		0x8000		/*         reserved fragment flag    */
#define	IP_DF		0x4000		/*         don't fragment flag	     */
#define	IP_MF		0x2000		/*         more fragments flag	     */
#define	IP_OFFMASK	0X1FFF		/*         mask for fragmenting bits */
    uint8_t	ip_ttl;			/* 8     time to live		     */
    uint8_t	ip_p;			/* 9     protocol		     */
    uint16_t	ip_sum;			/* 10-11 checksum		     */
    struct in_addr ip_src;		/* 12-15 source address		     */
    struct in_addr ip_dst;		/* 16-19 destination address	     */
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
#define IP_PROTO_DCCP		33
#define IP_PROTO_RSVP		46
#define IP_PROTO_GRE		47
#define IP_PROTO_ICMP6		58
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
#define IP_PROTO_SCTP		132
#define IP_PROTO_UDPLITE	136

#define IP_PROTO_NONE		257
#define IP_PROTO_TRANSP		258
#define IP_PROTO_TCP_OR_UDP	256
#define IP_PROTO_PAYLOAD	259

#define	IPOPT_EOL		0	/* end of option list */
#define	IPOPT_NOP		1	/* no operation */
#define IPOPT_RR		7	/* record packet route */
#define IPOPT_TS		68	/* timestamp */
#define IPOPT_SECURITY		130	/* provide s,c,h,tcc */
#define IPOPT_LSRR		131	/* loose source route */
#define IPOPT_SATID		136	/* satnet id */
#define IPOPT_SSRR		137	/* strict source route */
#define IPOPT_RA		148	/* router alert */

#define IP_ISFRAG(iph)	  (((iph)->ip_off & htons(IP_MF | IP_OFFMASK)) != 0)
#define IP_FIRSTFRAG(iph) (((iph)->ip_off & htons(IP_OFFMASK)) == 0)


/* checksum functions */

#if !CLICK_LINUXMODULE
/** @brief Calculate an Internet checksum over a data range.
 * @param x data to checksum
 * @param len number of bytes to checksum
 *
 * @a x must be two-byte aligned. */
uint16_t click_in_cksum(const unsigned char *x, int len);
uint16_t click_in_cksum_pseudohdr_raw(uint32_t csum, uint32_t src, uint32_t dst, int proto, int packet_len);
#else
# define click_in_cksum(addr, len) \
		ip_compute_csum((unsigned char *)(addr), (len))
# define click_in_cksum_pseudohdr_raw(csum, src, dst, proto, transport_len) \
		csum_tcpudp_magic((src), (dst), (transport_len), (proto), ~(csum) & 0xFFFF)
#endif
uint16_t click_in_cksum_pseudohdr_hard(uint32_t csum, const struct click_ip *iph, int packet_len);
void click_update_zero_in_cksum_hard(uint16_t *csum, const unsigned char *addr, int len);

/** @brief Adjust an Internet checksum according to a pseudoheader.
 * @param data_csum initial checksum (may be a 16-bit checksum)
 * @param iph IP header from which to extract pseudoheader information
 * @param transport_len length of transport header
 *
 * This function correctly handles Internet headers that contain source
 * routing options.  The final destination from the source routing option is
 * used to compute the pseudoheader checksum.  */
static inline uint16_t
click_in_cksum_pseudohdr(uint32_t data_csum, const struct click_ip *iph,
			 int transport_len)
{
    if (iph->ip_hl == 5)
	return click_in_cksum_pseudohdr_raw(data_csum, iph->ip_src.s_addr, iph->ip_dst.s_addr, iph->ip_p, transport_len);
    else
	return click_in_cksum_pseudohdr_hard(data_csum, iph, transport_len);
}

/** @brief Incrementally adjust an Internet checksum.
 * @param[in, out] csum points to checksum
 * @param old_hw old halfword
 * @param new_hw new halfword
 *
 * The checksum stored in *@a csum is updated to account for a change of @a
 * old_hw to @a new_hw.
 *
 * Because of the vagaries of one's-complement arithmetic, this function will
 * never produce a new checksum of ~+0 = 0xFFFF.  This is usually OK, since
 * IP, TCP, and UDP checksums will never have this value.  (Only all-zero data
 * can checksum to ~+0, but IP, TCP, and UDP checksums always cover at least
 * one non-zero byte.)  If you are checksumming data that is potentially all
 * zero, then call click_update_zero_in_cksum() after calling
 * click_update_in_cksum(). */
static inline void
click_update_in_cksum(uint16_t *csum, uint16_t old_hw, uint16_t new_hw)
{
    /* incrementally update IP checksum according to RFC1624:
       new_sum = ~(~old_sum + ~old_halfword + new_halfword) */
    uint32_t sum = (~*csum & 0xFFFF) + (~old_hw & 0xFFFF) + new_hw;
    sum = (sum & 0xFFFF) + (sum >> 16);
    *csum = ~(sum + (sum >> 16));
}

/** @brief Potentially fix a zero-valued Internet checksum.
 * @param[in, out] csum points to checksum
 * @param x data to checksum
 * @param len number of bytes to checksum
 *
 * If all the data bytes in [x, x+len) are zero, then its checksum should be
 * ~+0 = 0xFFFF, but click_update_in_cksum may have set the checksum to ~-0 =
 * 0x0000.  This function checks for such a case and sets the checksum
 * correctly. */
static inline void
click_update_zero_in_cksum(uint16_t *csum, const unsigned char *x, int len)
{
    if (*csum == 0)
	click_update_zero_in_cksum_hard(csum, x, len);
}

CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>
#endif
