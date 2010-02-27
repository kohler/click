/* -*- c-basic-offset: 4 -*-
 *
 * in_cksum.c -- Internet checksum
 * parts borrowed, with bug fixes, from one of the BSDs
 */

#include <click/config.h>
#include <clicknet/ip.h>
#if CLICK_LINUXMODULE
# include <linux/string.h>
#elif CLICK_BSDMODULE
# include <sys/param.h>
# include <sys/proc.h>
# include <sys/systm.h>
#else
# include <string.h>
#endif

#if !CLICK_LINUXMODULE
uint16_t
click_in_cksum(const unsigned char *addr, int len)
{
    int nleft = len;
    const uint16_t *w = (const uint16_t *)addr;
    uint32_t sum = 0;
    uint16_t answer = 0;

    /*
     * Our algorithm is simple, using a 32 bit accumulator (sum), we add
     * sequential 16 bit words to it, and at the end, fold back all the
     * carry bits from the top 16 bits into the lower 16 bits.
     */
    while (nleft > 1)  {
	sum += *w++;
	nleft -= 2;
    }

    /* mop up an odd byte, if necessary */
    if (nleft == 1) {
	*(unsigned char *)(&answer) = *(const unsigned char *)w ;
	sum += answer;
    }

    /* add back carry outs from top 16 bits to low 16 bits */
    sum = (sum & 0xffff) + (sum >> 16);
    sum += (sum >> 16);
    /* guaranteed now that the lower 16 bits of sum are correct */

    answer = ~sum;              /* truncate to 16 bits */
    return answer;
}

uint16_t
click_in_cksum_pseudohdr_raw(uint32_t csum, uint32_t src, uint32_t dst, int proto, int packet_len)
{
    assert(csum <= 0xFFFF);
    csum = ~csum & 0xFFFF;
# ifdef __i386__
    // borrowed from Linux
    __asm__("\n\
	addl %1, %0\n\
	adcl %2, %0\n\
	adcl %3, %0\n\
	adcl $0, %0\n"
	    : "=r" (csum)
	    : "g" (src), "g" (dst), "g" ((htons(packet_len) << 16) + (proto << 8)), "0" (csum));
    __asm__("\n\
	addl %1, %0\n\
	adcl $0xffff, %0\n"
	    : "=r" (csum)
	    : "r" (csum << 16), "0" (csum & 0xFFFF0000));
    return (~csum) >> 16;
# else
    csum += (src & 0xffff) + (src >> 16);
    csum += (dst & 0xffff) + (dst >> 16);
    csum += htons(packet_len) + htons(proto);
    csum = (csum & 0xffff) + (csum >> 16);
    return ~(csum + (csum >> 16)) & 0xFFFF;
# endif
}
#endif

uint16_t
click_in_cksum_pseudohdr_hard(uint32_t csum, const struct click_ip *iph, int packet_len)
{
    const uint8_t *opt = (const uint8_t *)(iph + 1);
    const uint8_t *end_opt = ((const uint8_t *)iph) + (iph->ip_hl << 2);
    while (opt < end_opt) {
	/* check one-byte options */
	if (*opt == IPOPT_NOP) {
	    opt++;
	    continue;
	} else if (*opt == IPOPT_EOL)
	    break;

	/* check option length */
	if (opt + 1 >= end_opt || opt[1] < 2 || opt + opt[1] > end_opt)
	    break;

	/* grab correct final destination from source routing option */
	if ((*opt == IPOPT_SSRR || *opt == IPOPT_LSRR) && opt[1] >= 7) {
	    uint32_t daddr;
	    memcpy(&daddr, opt + opt[1] - 4, 4);
	    return click_in_cksum_pseudohdr_raw(csum, iph->ip_src.s_addr, daddr, iph->ip_p, packet_len);
	}

	opt += opt[1];
    }

    return click_in_cksum_pseudohdr_raw(csum, iph->ip_src.s_addr, iph->ip_dst.s_addr, iph->ip_p, packet_len);
}

void
click_update_zero_in_cksum_hard(uint16_t *csum, const unsigned char *x, int len)
{
    for (; len > 0; --len, ++x)
	if (*x)
	    return;
    // if we get here, all bytes were zero, so the checksum is ~0
    *csum = ~0;
}
