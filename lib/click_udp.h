#ifndef CLICK_UDP_H
#define CLICK_UDP_H

/*
 * click_udp.h -- our own definition of the UDP header
 * based on a file from one of the BSDs
 */

struct udphdr {
  unsigned short uh_sport;
  unsigned short uh_dport;
  unsigned short uh_ulen;
  unsigned short uh_sum;
};

#endif
