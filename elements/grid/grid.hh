#ifndef GRID_HH
#define GRID_HH

// XXX when these settle, we can reorder, align and pack the fields etc...

/* All multibyte values sent over the wire in network byte order,
   unless otherwise noted (e.g. IP addresses) */

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
    return(((long)ntohl(ms)) / (1000.0 * 60.0 * 60.0));
  }

  // Convert degrees to milliseconds.
  static long toMS(double d){
    assert(d >= -180.0 && d <= 180.0);
    return(htonl((long)(d * 1000 * 60 * 60)));
  }

  // Latitude in degrees.
  double lat() const {
    return(toDeg(_mslat));
  }

  // Longitude in degrees.
  double lon() const {
    return(toDeg(_mslon));
  }

  // Set the lat and lon, in degrees.
  void set(double lat, double lon) {
    _mslat = toMS(lat);
    _mslon = toMS(lon);
  }

  String s() const {
    char buf[255];
    snprintf(buf, 255, "%f %f", lat(), lon());
    return String(buf);
  }
};

struct grid_hdr {
  unsigned char hdr_len;    // sizeof(grid_hdr). Why do we need this? -- it was changing...

  unsigned char type;
  static const int GRID_HELLO     = 1;  // no additional info in packet beyond header
  static const int GRID_LR_HELLO  = 2;  // followed by grid_hello and grid_nbr_entries
  static const int GRID_NBR_ENCAP = 3;  // followed by grid_nbr_encap
  static const int GRID_FLOOD_LOC_QUERY = 4; // followed by grid_flood_loc_query
  static const int GRID_FLOOD_LOC_REPLY = 5; // followed by grid_nbr_encap header 

  unsigned int ip;          // Sender's IP address.
  struct grid_location loc; // Sender's location, set by FixSrcLoc.
  int loc_err;              // Error radius of position, in metres.  if negative, don't believe loc
  static const int BAD_LOC = -1;
  unsigned int loc_seq_no;  
  unsigned short total_len; // Of the whole packet, starting at grid_hdr.
                            // Why do we need total_len? What about byte order? -- for cksum.  network order.
  unsigned short cksum;     // Over the whole packet, starting at grid_hdr.

  grid_hdr() : hdr_len(sizeof(grid_hdr)), cksum(0) 
  { total_len = htons(sizeof(grid_hdr)); }

  static String type_string(int type);
};

struct grid_nbr_entry {
  unsigned int ip; 
  unsigned int next_hop_ip;
  unsigned char num_hops; 
  /* 0 for num_hops indicate that this dest. is unreachable.  if so,
     loc fields are meaningless */
  struct grid_location loc;
  int loc_err;
  unsigned int seq_no;
  unsigned int age; 

  grid_nbr_entry() : ip(0), next_hop_ip(0), num_hops(0), loc(0, 0), seq_no(0) { }

  grid_nbr_entry(unsigned int _ip, unsigned int _nhip, unsigned char h, unsigned int s)
    : ip(_ip), next_hop_ip(_nhip), num_hops(h), loc(0, 0), seq_no(s) { } 
};

struct grid_hello {
  unsigned int seq_no;
  unsigned int age; // decreasing, approximately in milliseconds
  unsigned char num_nbrs;
  unsigned char nbr_entry_sz;
  // for GRID_LR_HELLO packets, followed by num_nbrs grid_nbr_entry
  // structs.

  grid_hello() : num_nbrs(0), nbr_entry_sz(sizeof(grid_nbr_entry)) { }

  static const unsigned int MIN_AGE_DECREMENT = 10;
  static const unsigned int MAX_AGE_DEFAULT = 30000; // ~ 30 secs
};

struct grid_nbr_encap {
  unsigned int dst_ip;
  struct grid_location dst_loc;
  int dst_loc_err;
  unsigned char hops_travelled;
};

struct grid_flood_loc_query {
  unsigned int src_ip; // because gh->ip gets rewritten by every sender
  unsigned int dst_ip;
  unsigned int seq_no;
};


inline String
grid_hdr::type_string(int type)
{
  switch (type) {
  case GRID_HELLO: return String("GRID_HELLO"); break;
  case GRID_LR_HELLO: return String("GRID_LR_HELLO"); break;
  case GRID_NBR_ENCAP: return String("GRID_NBR_ENCAP"); break;
  default: return String("Unknown");
  }
}


static const double GRID_RAD_PER_DEG = 0.017453293; // from xcalc
static const double GRID_EARTH_RADIUS = 6378156; // metres XXX do i believe this?
static const double GRID_PI = 3.1415927;

#endif
