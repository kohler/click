#ifndef GRID_HH
#define GRID_HH

// XXX when these settle, we can reorder, align and pack the fields etc...

// lat/lon in decimal degrees. +: north/east, -: south/west
struct grid_location {
  float lat; 
  float lon; 
  // XXX clearly needs to be better than this!
  grid_location() : lat(0), lon(0) { };
  grid_location(float _lat, float _lon) : lat(_lat), lon(_lon) { }

  void ntohloc() {
    assert(sizeof(float) == sizeof(unsigned long));
    long t1 = ntohl(*(unsigned long *) &lat);
    long t2 = ntohl(*(unsigned long *) &lon);
    lat = *(float *) &t1;
    lon = *(float *) &t2;
  }

  void htonloc() {
    assert(sizeof(float) == sizeof(unsigned long));
    long t1 = htonl(*(unsigned long *) &lat);
    long t2 = htonl(*(unsigned long *) &lon);
    lat = *(float *) &t1;
    lon = *(float *) &t2;
  }
};

struct grid_hdr {
  unsigned char hdr_len; // bytes
  unsigned char type;
#define GRID_HELLO     1
#define GRID_NBR_ENCAP 2
  unsigned int ip;
  struct grid_location loc;
  unsigned short total_len; // bytes
  unsigned short cksum; // over whole packet

  grid_hdr()
    : hdr_len(sizeof(grid_hdr)), total_len(sizeof(grid_hdr)), cksum(0) { }
};

struct grid_nbr_entry {
  unsigned int ip; 
  unsigned int next_hop_ip;
  unsigned char num_hops; // what does 0 indicate? XXX
  grid_location loc;

  grid_nbr_entry() : ip(0), next_hop_ip(0), num_hops(0), loc(0, 0) { }

  grid_nbr_entry(unsigned int _ip, unsigned int _nhip, unsigned char h)
    : ip(_ip), next_hop_ip(_nhip), num_hops(h), loc(0, 0) { } 
};

struct grid_hello {
  unsigned char num_nbrs;
  unsigned char nbr_entry_sz;
  // followed by num_nbrs grid_nbr structs

  grid_hello() : num_nbrs(0), nbr_entry_sz(sizeof(grid_nbr_entry)) { }
};

struct grid_nbr_encap {
  unsigned int dst_ip;
  unsigned char hops_travelled;
};

#endif
