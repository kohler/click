/*
 * in_cksum.c -- Internet checksum
 * borrowed, with bug fixes, from one of the BSDs
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "click_ip.h"

unsigned short
in_cksum(const unsigned char *addr, int len)
{
  int nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned sum = 0;
  unsigned short answer = 0;
    
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
