#ifndef GRID_HH
#define GRID_HH

// XXX when these settle, we can reorder, align and pack the fields etc...

// packet data should be 4 byte aligned
#define ASSERT_ALIGNED(p) assert(((unsigned int)(p) % 4) == 0)

/* All multibyte values sent over the wire in network byte order,
   unless otherwise noted (e.g. IP addresses) */

// A geographical position.
// Suitable for including inside a packet.
struct grid_location {

// REMINDER: UPDATE GRID_VERSION WITH EVERY MODIFICATION TO HEADERS

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

// REMINDER: UPDATE GRID_VERSION WITH EVERY MODIFICATION TO HEADERS
#define GRID_VERSION 0xfecf

  unsigned int version; // look, doug, a coherent variable name
                        // (should maybe be called nbr_vrs or something)

  unsigned char hdr_len;    // sizeof(grid_hdr). Why do we need this? -- it was changing...

  unsigned char type;
  static const unsigned char GRID_HELLO     = 1;  // no additional info in packet beyond header
  static const unsigned char GRID_LR_HELLO  = 2;  // followed by grid_hello and grid_nbr_entries
  static const unsigned char GRID_NBR_ENCAP = 3;  // followed by grid_nbr_encap
  static const unsigned char GRID_LOC_QUERY = 4;  // followed by grid_loc_query
  static const unsigned char GRID_LOC_REPLY = 5;  // followed by grid_nbr_encap header 

  unsigned char pad1, pad2;

  /*
   * Sender is who originally created this packet and injected it into
   * the network.  i.e. the following five fields (ip, ...,
   * loc_seq_no) must be filled in by any element that creates new
   * Grid packets.  Actually, as long as ip and tx_ip are both set to
   * the same address, FixSrcLoc will do all the neccessary filling
   * in.
   *
   * Transmitter is who last transmitted this packet to us.  When the
   * packet is first transmitted, the sender and transmitter are the
   * same.  Also, the transmitter information can be mapped to the
   * packet's MAC src address.  All the tx_* fields are filled in by
   * every node that handles a Grid packet.  Any Grid element that
   * handles a packet that will be sent back our
   * (e.g. FloodingLocQuerier, routing elements) should set the tx_ip
   * field.  The other location info is typically handled by the
   * FixSrcLoc element.  
   */


// REMINDER: UPDATE GRID_VERSION WITH EVERY MODIFICATION TO HEADERS

  unsigned int ip;          // Sender's IP address. 
  struct grid_location loc; // Sender's location, set by FixSrcLoc.
  unsigned short loc_err;   // Error radius of position, in metres.  
  bool loc_good;            // If false, don't believe loc  
  unsigned char pad3;       // assume bool is char aligned
  unsigned int loc_seq_no;  

  unsigned int tx_ip;       // Transmitter 
  struct grid_location tx_loc;
  unsigned short tx_loc_err;
  bool tx_loc_good;
  unsigned char pad4;
  unsigned int tx_loc_seq_no;

  /* XXX this location err and sequence number info needs to be
     incorporated into the grid_location class, which will enable us
     to avoid manually converting it between host and network order.
     okay, by the time i have finished writing this i could probably
     have done it... */

  unsigned short total_len; // Of the whole packet, starting at grid_hdr.
                            // Why do we need total_len? What about byte order? -- for cksum.  network order.
  unsigned short cksum;     // Over the whole packet, starting at grid_hdr.

  grid_hdr() : hdr_len(sizeof(grid_hdr)), cksum(0) 
  { total_len = htons(sizeof(grid_hdr)); assert(total_len % 4 == 0); }

  static String type_string(unsigned char type);
};

struct grid_nbr_entry {

// REMINDER: UPDATE GRID_VERSION WITH EVERY MODIFICATION TO HEADERS

  unsigned int ip; 
  unsigned int next_hop_ip;
  unsigned char num_hops; 
  /* 0 for num_hops indicate that this dest. is unreachable.  if so,
     loc fields are meaningless */
  struct grid_location loc;
  unsigned short loc_err;
  bool loc_good;
  bool is_gateway;
  unsigned int seq_no;
  union {
    unsigned int age; /* keep ``age'' for legacy code */
    unsigned int ttl;
  };

  grid_nbr_entry() : ip(0), next_hop_ip(0), num_hops(0), loc(0, 0), seq_no(0) 
  { assert(sizeof(grid_nbr_entry) % 4 == 0); }

  grid_nbr_entry(unsigned int _ip, unsigned int _nhip, unsigned char h, unsigned int s)
    : ip(_ip), next_hop_ip(_nhip), num_hops(h), loc(0, 0), seq_no(s)
    { assert(sizeof(grid_nbr_entry) % 4 == 0); }
};

struct grid_hello {

// REMINDER: UPDATE GRID_VERSION WITH EVERY MODIFICATION TO HEADERS

  unsigned int seq_no;
  union {
    unsigned int age; /* keep age field for legacy code */
    unsigned int ttl; // decreasing, approximately in milliseconds
  };

  bool is_gateway; // is the hello's transmitter also a gateway?

  unsigned char num_nbrs;
  unsigned char nbr_entry_sz;
  // for GRID_LR_HELLO packets, followed by num_nbrs grid_nbr_entry
  // structs.

  grid_hello() : num_nbrs(0), nbr_entry_sz(sizeof(grid_nbr_entry)) 
  { assert(sizeof(grid_hello) % 4 == 0); }

  static const unsigned int MIN_AGE_DECREMENT = 10;
  static const unsigned int MAX_AGE_DEFAULT = 30000; // ~ 30 secs
  // age is really ttl...add these new names for clarity
  static const unsigned int MIN_TTL_DECREMENT = MIN_AGE_DECREMENT;
  static const unsigned int MAX_TTL_DEFAULT = MAX_AGE_DEFAULT;
};

struct grid_nbr_encap {

// REMINDER: UPDATE GRID_VERSION WITH EVERY MODIFICATION TO HEADERS

  grid_nbr_encap() { assert(sizeof(grid_nbr_encap) % 4 == 0); }
  unsigned int dst_ip;
  struct grid_location dst_loc;
  unsigned short dst_loc_err;
  bool dst_loc_good;
  unsigned char hops_travelled;
};

struct grid_loc_query {

// REMINDER: UPDATE GRID_VERSION WITH EVERY MODIFICATION TO HEADERS

  grid_loc_query() { assert(sizeof(grid_loc_query) % 4 == 0); }
  unsigned int dst_ip;
  unsigned int seq_no;
};


inline String
grid_hdr::type_string(unsigned char type)
{
  switch (type) {
  case GRID_HELLO: return String("GRID_HELLO"); break;
  case GRID_LR_HELLO: return String("GRID_LR_HELLO"); break;
  case GRID_NBR_ENCAP: return String("GRID_NBR_ENCAP"); break;
  case GRID_LOC_REPLY: return String("GRID_LOC_REPLY"); break;
  case GRID_LOC_QUERY: return String("GRID_LOC_QUERY"); break;
  default: return String("Unknown-type");
  }
}


static const double GRID_RAD_PER_DEG = 0.017453293; // from xcalc
static const double GRID_EARTH_RADIUS = 6378156; // metres XXX do i believe this?
static const double GRID_PI = 3.1415927;

#endif
