#ifndef GRID_HH
#define GRID_HH

// lat/lon in decimal degrees. +: north/east, -: south/west
struct grid_location {
  float lat; 
  float lon; 
  // XXX clearly needs to be better than this!
  grid_location() : lat(0), lon(0) { };
  grid_location(float _lat, float _lon) : lat(_lat), lon(_lon) { }
};

struct grid_hdr {
  unsigned char len;
  unsigned char type;
#define GRID_HELLO 1
#define GRID_NBR_ENCAP 2
  unsigned int ip;
  struct grid_location loc;
};

#endif
