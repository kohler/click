#ifndef GRID_HH
#define GRID_HH
#include <click/glue.hh>
#include <click/string.hh>
CLICK_DECLS

// defining this will break lots of shit
#define SMALL_GRID_HEADERS
#define SMALL_GRID_PROBES

// REMINDER: UPDATE GRID_VERSION WITH EVERY MODIFICATION TO HEADERS

// packet data should be 4 byte aligned
#define ASSERT_4ALIGNED(p) assert(((uintptr_t) (p) % 4) == 0)

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

#ifdef CLICK_USERLEVEL
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
    return (double) ntohl(_h) / 1000.0;
  }

  // What is the distance between l1 and l2 in meters?
  // (definition in gridlocationinfo.cc)
  static double calc_range(const grid_location &l1, const grid_location &l2);

  String s() const {
    char buf[255];
    snprintf(buf, 255, "%.5f,%.5f,%.3f", lat(), lon(), h());
    return String(buf);
  }
#else // CLICK_USERLEVEL
  String s() const {
    char buf[255];
    snprintf(buf, 255, "%ld,%ld,%ld", lat_ms(), lon_ms(), h_mm());
    return String(buf);
  }
#endif

    // lat, lon in milliseconds, height in millimeters
    grid_location(int32_t lat, int32_t lon, int32_t h = 0)
    { set(lat, lon, h); }

    int32_t lat_ms() const { return ntohl(_mslat); }
    int32_t lon_ms() const { return ntohl(_mslon); }
    int32_t h_mm()   const { return ntohl(_h);   }

private:
    // Internally, we remember positions as lat/lon in milliseconds,
    // as 32-bit integers in network order.
    int32_t _mslat;
    int32_t _mslon;

    // heights as millimetres.  we don't really have a good height
    // datum.  network byte order.
    int32_t _h;

#ifdef CLICK_USERLEVEL
  // Convert network byte order milliseconds to host byte order degrees
  static double toDeg(int32_t ms) {
    return(((int32_t)ntohl(ms)) / (1000.0 * 60.0 * 60.0));
  }

  // Convert host byte order degrees to network byte order milliseconds
  static long toMS(double d) {
    assert(d >= -180.0 && d <= 180.0);
    return(htonl((long)(d * 1000 * 60 * 60)));
  }

  // Set the lat, lon in degrees, h in metres
  void set(double lat, double lon, double h) {
    _mslat = toMS(lat);
    _mslon = toMS(lon);
    _h = htonl((long) (h * 1000));
  }

  static int sign(double x) { return (((x) < 0) ? -1 : 1); }
#endif

  void set(int32_t lat, int32_t lon, int32_t h) {
    _mslat = htonl(lat);
    _mslon = htonl(lon);
    _h = htonl(h);
  }
};

// regions for geocast
struct grid_region {
public:
  grid_region(const grid_location &c, unsigned long r)
    : _center(c) { _radius = htonl(r); }

  grid_region() : _radius(0) { }

  unsigned int radius() const { return ntohl(_radius); }
  const grid_location &center() const { return _center; }

#ifdef CLICK_USERLEVEL
  bool contains(const grid_location &l)
  { return grid_location::calc_range(l, _center) <= _radius; }
#endif

private:
  struct grid_location _center;
  uint32_t _radius; // stored in network byte order
};



struct grid_hdr {

    // REMINDER: UPDATE GRID_VERSION WITH EVERY MODIFICATION TO HEADERS
    enum {
	GRID_VERSION = 0xFED4
    };

    uint32_t version;		// which version of the grid protocol we are using

    uint8_t hdr_len;		// sizeof(grid_hdr). Why do we need this? -- it was changing...

    uint8_t type;
    enum Type {
	GRID_HELLO = 1,		// no additional info in packet beyond header
	GRID_LR_HELLO = 2,	// followed by grid_hello and grid_nbr_entries
	GRID_NBR_ENCAP = 3,	// followed by grid_nbr_encap
	GRID_LOC_QUERY = 4,	// followed by grid_loc_query
	GRID_LOC_REPLY = 5,	// followed by grid_nbr_encap
	GRID_ROUTE_PROBE = 6,	// followed by grid_nbr_encap and grid_route_probe
	GRID_ROUTE_REPLY = 7,	// followed by grid_nbr_encap and grid_route_reply
	GRID_GEOCAST = 8,	// followed by grid_geocast
	GRID_LINK_PROBE = 9	// followed by grid_link_probe
    };

    uint8_t pad1, pad2;

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

    uint32_t ip;			// Sender's IP address.
    struct grid_location loc;		// Sender's location, set by FixSrcLoc.
    uint16_t loc_err;			// Error radius of position, in metres.
    uint8_t /*bool*/ loc_good;		// If false, don't believe loc
    uint8_t pad3;			// assume bool is char aligned
    uint32_t loc_seq_no;

    uint32_t tx_ip;			// Transmitter
    struct grid_location tx_loc;
    uint16_t tx_loc_err;
    uint8_t /*bool*/ tx_loc_good;
    uint8_t pad4;
    uint32_t tx_loc_seq_no;

  /* XXX this location err and sequence number info needs to be
     incorporated into the grid_location class, which will enable us
     to avoid manually converting it between host and network order.
     okay, by the time i have finished writing this i could probably
     have done it... */

    uint16_t total_len;		// Of the whole packet, starting at grid_hdr.
                            // Why do we need total_len? What about byte order? -- for cksum.  network order.
    uint16_t cksum;     // Over the whole packet, starting at grid_hdr.

  grid_hdr() : hdr_len(sizeof(grid_hdr)), cksum(0)
  { total_len = htons(sizeof(grid_hdr)); assert(total_len % 4 == 0); }

    static String type_string(uint8_t type);

    static uint32_t get_pad_bytes(struct grid_hdr &gh)
    { return (gh.pad1 << 24) | (gh.pad2 << 16) | (gh.pad3 << 8) | gh.pad4; }

    static void set_pad_bytes(struct grid_hdr &gh, uint32_t v)
    { gh.pad1 = (v >> 24); gh.pad2 = (v >> 16) & 0xff; gh.pad3 = (v >> 8) & 0xff; gh.pad4 = v & 0xff; }
};

struct grid_nbr_entry {

// REMINDER: UPDATE GRID_VERSION WITH EVERY MODIFICATION TO HEADERS

    uint32_t ip;
    uint32_t next_hop_ip;
    uint8_t num_hops;
  /* 0 for num_hops indicate that this dest. is unreachable.  if so,
     loc fields are meaningless */

    struct grid_location loc;
    uint16_t loc_err;
    uint8_t /* bool */ loc_good;
    uint8_t /* bool */ is_gateway;
    uint32_t seq_no;
    union {
	uint32_t age; /* keep ``age'' for legacy code */
	uint32_t ttl;
    };

    uint32_t metric;
    uint8_t /* bool */ metric_valid;

#ifndef SMALL_GRID_HEADERS
  /* ping-pong stats, valid only for 1-hop nbrs */
  /*
   * in our route advertisement packet these stats reflect _our_
   * measurements of packets sent by this neighbor.
   */
    int32_t link_qual;
    int32_t link_sig;
    struct timeval measurement_time;

    uint8_t num_rx;
    uint8_t num_expected;
    struct timeval last_bcast;
#endif

    grid_nbr_entry() : ip(0), next_hop_ip(0), num_hops(0), loc(0, 0, 0), seq_no(0)
    { assert(sizeof(grid_nbr_entry) % 4 == 0); }

    grid_nbr_entry(uint32_t _ip, uint32_t _nhip, uint8_t h, uint32_t s)
	: ip(_ip), next_hop_ip(_nhip), num_hops(h), loc(0, 0, 0), seq_no(s)
    { assert(sizeof(grid_nbr_entry) % 4 == 0); }
};

struct grid_hello {

// REMINDER: UPDATE GRID_VERSION WITH EVERY MODIFICATION TO HEADERS

    uint32_t seq_no;
    union {
	uint32_t age; /* keep age field for legacy code */
	uint32_t ttl; // decreasing, approximately in milliseconds
    };

    uint8_t /*bool*/ is_gateway; // is the hello's transmitter also a gateway?

    uint8_t num_nbrs;
    uint8_t nbr_entry_sz;
  // for GRID_LR_HELLO packets, followed by num_nbrs grid_nbr_entry
  // structs.

  grid_hello() : num_nbrs(0), nbr_entry_sz(sizeof(grid_nbr_entry))
  { assert(sizeof(grid_hello) % 4 == 0); }

    enum {
	MIN_AGE_DECREMENT = 10,
	MAX_AGE_DEFAULT = 300000, // ~ 300 secs
	// age is really ttl...add these new names for clarity
	MIN_TTL_DECREMENT = MIN_AGE_DECREMENT,
	MAX_TTL_DEFAULT = MAX_AGE_DEFAULT
    };
};

struct grid_nbr_encap {

// REMINDER: UPDATE GRID_VERSION WITH EVERY MODIFICATION TO HEADERS

  grid_nbr_encap() { assert(sizeof(grid_nbr_encap) % 4 == 0); }
  uint32_t dst_ip;
#ifndef SMALL_GRID_HEADERS
  struct grid_location dst_loc;
  uint16_t dst_loc_err;
  uint8_t /* bool */ dst_loc_good;
#else
  uint8_t pad1, pad2, pad3;
#endif
  uint8_t hops_travelled;

#ifndef SMALL_GRID_HEADERS
    int32_t link_qual;
    int32_t link_sig;
    struct timeval measurement_time;

    uint8_t num_rx;
    uint8_t num_expected;
    struct timeval last_bcast;
#endif
};

struct grid_loc_query {

// REMINDER: UPDATE GRID_VERSION WITH EVERY MODIFICATION TO HEADERS

    grid_loc_query() { assert(sizeof(grid_loc_query) % 4 == 0); }
    uint32_t dst_ip;
    uint32_t seq_no;
};

struct grid_route_probe {
    uint32_t nonce;
    struct timeval send_time;
};

struct grid_route_reply {
    uint32_t nonce;
    uint32_t probe_dest;
    uint8_t reply_hop;
    struct timeval probe_send_time;
    uint32_t route_action;
    uint32_t data1;
    uint32_t data2;
};

struct grid_geocast {
    struct grid_region dst_region;
    uint16_t dst_loc_err;
    uint8_t hops_travelled;
    uint8_t pad;
    uint32_t seq_no;
    // no payload length field, use grid_hdr total_len field.
    // no loc_good flag; by definition, the destination location must be good.
};

struct grid_link_probe {
  uint32_t seq_no;
  uint32_t period;      // period of this node's probe broadcasts, in msecs
  uint32_t num_links;   // number of grid_link_entry entries following
  uint32_t tau;         // this node's loss-rate averaging period, in msecs
};

struct grid_link_entry {
#ifdef SMALL_GRID_PROBES
  uint8_t ip;            // last byte of IP address
  uint8_t num_rx;
#else
  uint32_t ip;
  uint32_t period;         // period of node's probe broadcasts, in msecs
  struct timeval last_rx_time; // time of most recent probe received from node
  uint32_t last_seq_no;    // seqno of most recent probe received from this host
  uint32_t num_rx;         // number of probe bcasts received from node during last tau msecs
#endif
};

inline String
grid_hdr::type_string(uint8_t type)
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
  default:
    {
      char buf[100];
      snprintf(buf, sizeof(buf), "Unknown-type 0x%02x", (unsigned) type);
      return String(buf);
    }
  }
}

CLICK_ENDDECLS
#endif
