#ifndef BRIDGEMESSAGE_HH
#define BRIDGEMESSAGE_HH

#include "glue.hh"
#include "string.hh"

class BridgeMessage {
public:
  struct wire;

  BridgeMessage()			{ expire (); }
  BridgeMessage(wire* msg)		{ from_wire(msg); }


  void reset(u_int64_t bridge_id);
  void from_wire(wire* msg);
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
  int compare(wire* other) const;
  int compare(const BridgeMessage* other,
	      u_int64_t _bridge_id, u_int16_t _port_id) const;

  String s(String tag = "") const;

  struct wire {
  public:
    u_int8_t dst[6];		// 12
    u_int8_t src[6];

    u_int16_t length;		// 5
    u_int16_t sap;
    u_int8_t ctl;

    u_int8_t xxx_protocol[2];	// 35
    u_int8_t version;
    u_int8_t type;
    u_int8_t tc:1;
    u_int8_t reserved:6;
    u_int8_t tca:1;
    u_int8_t xxx_root[8];
    u_int8_t xxx_cost[4];
    u_int8_t xxx_bridge_id[8];
    u_int8_t xxx_port_id[2];
    u_int8_t xxx_message_age[2];
    u_int8_t xxx_max_age[2];
    u_int8_t xxx_hello_time[2];
    u_int8_t xxx_forward_delay[2];

    u_int8_t padding[8];	// 8

    String s(String tag = "");

    u_int16_t &protocol()	{ return (u_int16_t &)*&xxx_protocol[0]; }
    u_int64_t &root()		{ return (u_int64_t &)*&xxx_root[0]; }
    u_int32_t &cost()		{ return (u_int32_t &)*&xxx_cost[0]; }
    u_int64_t &bridge_id()	{ return (u_int64_t &)*&xxx_bridge_id[0]; }
    u_int16_t &port_id()	{ return (u_int16_t &)*&xxx_port_id[0]; }
    u_int16_t &message_age()	{ return (u_int16_t &)*&xxx_message_age[0]; }
    u_int16_t &max_age()	{ return (u_int16_t &)*&xxx_max_age[0]; }
    u_int16_t &hello_time()	{ return (u_int16_t &)*&xxx_hello_time[0]; }
    u_int16_t &forward_delay()	{ return (u_int16_t &)*&xxx_forward_delay[0]; }
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
