#ifndef GRID_HH
#define GRID_HH

struct grid_hdr {
  unsigned char len;
  unsigned char type;
  unsigned int ip;
#define GRID_HELLO 1
#define GRID_NBR_ENCAP 2
};

#endif
