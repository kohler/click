#ifndef CLICK_UDP_H
#define CLICK_UDP_H

struct udphdr {
  unsigned short uh_sport;
  unsigned short uh_dport;
  unsigned short uh_ulen;
  unsigned short uh_sum;
};

#endif
