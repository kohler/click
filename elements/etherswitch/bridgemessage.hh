#ifndef BRIDGEMESSAGE_HH
#define BRIDGEMESSAGE_HH

#include "glue.hh"
#include "string.hh"

class BridgeMessage {
public:
  struct wire;

  BridgeMessage()			{ expire (); }
  BridgeMessage(const wire* msg)	{ from_wire(msg); }


  void reset(u_int64_t bridge_id);
  void from_wire(const wire* msg);
  void to_wire(wire* msg) const;
  static void fill_tcm(wire* msg);

  
  bool expire(const timeval* cutoff);	// Possibly expire, based on timestamp
  void expire();		// Set fields to worst values


  // Both compare()s return positive if 'this' is better than 'other',
  // 0 if equal, negative if worse.  The value returned depend on
  // which test determined the "winner".  The last version allows the
  // caller to override the port and bridge_id in 'this'.  (To ask the
  // question, "If *I* sent this message on a certain port, how would
  // it compare?"
  int compare(const BridgeMessage* other) const;
  int compare(const wire* other) const;
  int compare(const BridgeMessage* other,
	      u_int64_t _bridge_id, u_int16_t _port_id) const;

  String s(String tag = "") const;

#define PACKED __attribute__((packed))
  struct wire {
  public:
    u_int8_t dst[6] PACKED;	// 12
    u_int8_t src[6] PACKED;

    u_int16_t length PACKED;	// 5
    u_int16_t sap PACKED;
    u_int8_t ctl PACKED;


    u_int16_t protocol PACKED;	// 35
    u_int8_t version PACKED;
    u_int8_t type PACKED;
    u_int8_t tc:1 PACKED;
    u_int8_t reserved:6 PACKED;
    u_int8_t tca:1 PACKED;
    u_int64_t root PACKED;
    u_int32_t cost PACKED;
    u_int64_t bridge_id PACKED;
    u_int16_t port_id PACKED;
    u_int16_t message_age PACKED;
    u_int16_t max_age PACKED;
    u_int16_t hello_time PACKED;
    u_int16_t forward_delay PACKED;

    u_int8_t padding[8];	// 8

    //    u_int32_t fcs PACKED;	// we never actually see this

    String s(String tag = "") const;
  };

  // Parameters that get propagated
  u_int32_t _max_age;		// in seconds
  u_int32_t _hello_time;	// in seconds
  u_int32_t _forward_delay;	// in seconds

private:
  u_int64_t _root;
  u_int64_t _bridge_id;
public:  u_int32_t _cost; private: // Put in an incrementer JJJ
  u_int16_t _port_id;

  bool _tc;

  timeval _timestamp; // When the message should be considered to have
		      // been created, used for expiration.

  static void prep_msg(wire* msg);
  static u_int8_t _all_bridges[6];
};

inline u_int64_t htonq(u_int64_t x) {
  u_int32_t hi = x >> 32;
  u_int32_t lo = x & 0xffffffff;
  return (((u_int64_t)htonl(lo)) << 32) | htonl(hi);
}

inline u_int64_t ntohq(u_int64_t x) {
  u_int32_t hi = x >> 32;
  u_int32_t lo = x & 0xffffffff;
  return (((u_int64_t)ntohl(lo)) << 32) | ntohl(hi);
}

#endif
