#ifndef GRID_HH
#define GRID_HH

// XXX when these settle, we can reorder, align and pack the fields etc...

// A geographical position.
// Suitable for including inside a packet.
struct grid_location {
  // Internally, we remember positions as lat/lon in milliseconds,
  // as 32-bit integers in network order.
  long _mslat;
  long _mslon;

  grid_location() : _mslat(0), _mslon(0) { };
  grid_location(float lat, float lon) {
    set(lat, lon);
  }    

  // Convert milliseconds to degrees.
  static double toDeg(long ms) {
    return(ntohl(ms) / (1000.0 * 60.0 * 60.0));
  }

  // Convert degrees to milliseconds.
  static long toMS(double d){
    assert(d >= -180.0 && d <= 180.0);
    return(htonl((long)(d * 1000 * 60 * 60)));
  }

  // Latitude in degrees.
  double lat() {
    return(toDeg(_mslat));
  }

  // Longitude in degrees.
  double lon() {
    return(toDeg(_mslon));
  }

  // Set the lat and lon, in degrees.
  void set(double lat, double lon) {
    _mslat = toMS(lat);
    _mslon = toMS(lon);
  }
};

struct grid_hdr {
  unsigned char hdr_len;    // sizeof(grid_hdr)
  unsigned char type;
#define GRID_HELLO     1
#define GRID_NBR_ENCAP 2
  unsigned int ip;          // Sender's IP address.
  struct grid_location loc; // Sender's location, set by FixSrcLoc.
  unsigned short total_len; // Of the whole packet, starting at grid_hdr.
  unsigned short cksum;     // Over the whole packet, starting at grid_hdr.

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
