#ifndef GRID_HH
#define GRID_HH
CLICK_DECLS

// defining this will break lots of shit
#define SMALL_GRID_HEADERS

// REMINDER: UPDATE GRID_VERSION WITH EVERY MODIFICATION TO HEADERS

// packet data should be 4 byte aligned
#define ASSERT_ALIGNED(p) assert(((unsigned int)(p) % 4) == 0)

/* All multibyte values sent over the wire in network byte order,
   unless otherwise noted (e.g. IP addresses) */

static const double GRID_RAD_PER_DEG = 0.017453293; // from xcalc
static const double GRID_EARTH_RADIUS = 6378156; // metres XXX do i believe this?
static const double GRID_PI = 3.1415927;

// A geographical position.
// Suitable for including inside a packet.
struct grid_location {
public:
  // default: all zeroes
  grid_location() : _mslat(0), _mslon(0), _h(0) { };

  // lat, lon in degrees, height in metres
  grid_location(double lat, double lon, double h = 0) 
  { set(lat, lon, h); }

  // Latitude in degrees.
  double lat() const {
    return(toDeg(_mslat));
  }

  // Longitude in degrees.
  double lon() const {
    return(toDeg(_mslon));
  }

  // Height in metres.
  double h() const {
    return (double) _h / 1000.0;
  }

  // What is the distance between l1 and l2 in meters?
  // (definition in gridlocationinfo.cc)
  static double calc_range(const grid_location &l1, const grid_location &l2);    

  String s() const {
    char buf[255];
    snprintf(buf, 255, "%.5f,%.5f,%.3f", lat(), lon(), h());
    return String(buf);
  }

private:
  // Internally, we remember positions as lat/lon in milliseconds,
  // as 32-bit integers in network order.
  long _mslat;
  long _mslon;

  // heights as millimetres.  we don't really have a good height datum.
  long _h;

  //  long _pad; // XXX ???

  // Convert milliseconds to degrees.
  static double toDeg(long ms) {
    return(((long)ntohl(ms)) / (1000.0 * 60.0 * 60.0));
  }

  // Convert degrees to milliseconds.
  static long toMS(double d){
    assert(d >= -180.0 && d <= 180.0);
    return(htonl((long)(d * 1000 * 60 * 60)));
  }

  // Set the lat, lon in degrees, h in metres
  void set(double lat, double lon, double h) {
    _mslat = toMS(lat);
    _mslon = toMS(lon);
    _h = (long) (h * 1000);
  }

  static int sign(double x) { return (((x) < 0) ? -1 : 1); }

};


// regions for geocast
struct grid_region {
public:
  grid_region(const grid_location &c, unsigned long r) 
    : _center(c) { _radius = htonl(r); }

  grid_region() : _radius(0) { }

  const unsigned int radius() { return ntohl(_radius); }
  const struct grid_location center() { return _center; }

  bool contains(const grid_location &l) 
  { return grid_location::calc_range(l, _center) <= _radius; }

private:
  struct grid_location _center;
  unsigned int _radius; // stored in network byte order
};



struct grid_hdr {

// REMINDER: UPDATE GRID_VERSION WITH EVERY MODIFICATION TO HEADERS
  static const unsigned int GRID_VERSION = 0xfed3;

  unsigned int version;     // which version of the grid protocol we are using

  unsigned char hdr_len;    // sizeof(grid_hdr). Why do we need this? -- it was changing...

  unsigned char type;
  static const unsigned char GRID_HELLO       = 1;  // no additional info in packet beyond header
  static const unsigned char GRID_LR_HELLO    = 2;  // followed by grid_hello and grid_nbr_entries
  static const unsigned char GRID_NBR_ENCAP   = 3;  // followed by grid_nbr_encap
  static const unsigned char GRID_LOC_QUERY   = 4;  // followed by grid_loc_query
  static const unsigned char GRID_LOC_REPLY   = 5;  // followed by grid_nbr_encap 
  static const unsigned char GRID_ROUTE_PROBE = 6;  // followed by grid_nbr_encap and grid_route_probe
  static const unsigned char GRID_ROUTE_REPLY = 7;  // followed by grid_nbr_encap and grid_route_reply
  static const unsigned char GRID_GEOCAST     = 8;  // followed by grid_geocast
  static const unsigned char GRID_LINK_PROBE  = 9;  // followed by grid_link_probe

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

  static unsigned int get_pad_bytes(struct grid_hdr &gh) 
  { return (gh.pad1 << 24) | (gh.pad2 << 16) | (gh.pad3 << 8) | gh.pad4; }
  
  static void set_pad_bytes(struct grid_hdr &gh, unsigned int v) 
  { gh.pad1 = (v >> 24); gh.pad2 = (v >> 16) & 0xff; gh.pad3 = (v >> 8) & 0xff; gh.pad4 = v & 0xff; }
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

  unsigned int metric;
  bool metric_valid;

  /* ping-pong stats, valid only for 1-hop nbrs */
  /* 
   * in our route advertisement packet these stats reflect _our_
   * measurements of packets sent by this neighbor.  
   */
  int link_qual;
  int link_sig;
  struct timeval measurement_time;

  unsigned char num_rx;
  unsigned char num_expected;
  struct timeval last_bcast;

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
#ifndef SMALL_GRID_HEADERS
  struct grid_location dst_loc;
  unsigned short dst_loc_err;
  bool dst_loc_good;
#else
  unsigned char pad1, pad2, pad3;
#endif
  unsigned char hops_travelled;

#ifndef SMALL_GRID_HEADERS
  int link_qual;
  int link_sig;           
  struct timeval measurement_time;  

  unsigned char num_rx;
  unsigned char num_expected;
  struct timeval last_bcast;
#endif
};

struct grid_loc_query {

// REMINDER: UPDATE GRID_VERSION WITH EVERY MODIFICATION TO HEADERS

  grid_loc_query() { assert(sizeof(grid_loc_query) % 4 == 0); }
  unsigned int dst_ip;
  unsigned int seq_no;
};

struct grid_route_probe {
  unsigned int nonce;
  struct timeval send_time;
};

struct grid_route_reply {
  unsigned int nonce;
  unsigned int probe_dest;
  unsigned char reply_hop;
  struct timeval probe_send_time;
  unsigned int route_action;
  unsigned int data1;
  unsigned int data2;
};

struct grid_geocast {
  struct grid_region dst_region;
  unsigned short dst_loc_err;
  unsigned char hops_travelled;
  char pad;
  unsigned int seq_no;
  // no payload length field, use grid_hdr total_len field.
  // no loc_good flag; by definition, the destination location must be good.
};

struct grid_link_probe {
  unsigned int seq_no;
  unsigned int period;      // period of this node's probe broadcasts, in msecs
  unsigned int num_links;   // number of grid_link_info entries following
  unsigned int window;      // this node's linkstat window, in msecs
};

struct grid_link_entry {
  unsigned int ip;
  unsigned int period;      // period of node's probe broadcasts, in msecs
  struct timeval last;      // time of most recent broadcast received from node
  unsigned int num_rx;      // number of probe bcasts received from node during window
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
  case GRID_ROUTE_PROBE: return String("GRID_ROUTE_PROBE"); break;
  case GRID_ROUTE_REPLY: return String("GRID_ROUTE_REPLY"); break;
  case GRID_GEOCAST: return String("GRID_GEOCAST"); break;
  case GRID_LINK_PROBE: return String("GRID_LINK_PROBE"); break;
  default: return String("Unknown-type");
  }
}

CLICK_ENDDECLS
#endif
