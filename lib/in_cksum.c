/* -*- c-basic-offset: 4 -*-
 *
 * in_cksum.c -- Internet checksum
 * borrowed, with bug fixes, from one of the BSDs
 */

#include <click/config.h>
#include <clicknet/ip.h>

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
click_in_cksum_pseudohdr(uint32_t csum, uint32_t src, uint32_t dst, int proto, int packet_len)
{
    assert(csum <= 0xFFFF);
    csum = ~csum & 0xFFFF;
#ifdef __i386__
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
#else
    csum += (src & 0xffff) + (src >> 16);
    csum += (dst & 0xffff) + (dst >> 16);
    csum += htons(packet_len) + htons(proto);
    csum = (csum & 0xffff) + (csum >> 16);
    return ~(csum + (csum >> 16)) & 0xFFFF;
#endif
}
