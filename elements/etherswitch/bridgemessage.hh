#ifndef BRIDGEMESSAGE_HH
#define BRIDGEMESSAGE_HH

#include <click/glue.hh>
#include <click/string.hh>
#include <click/integers.hh>

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

#ifdef __GNUC__
# define CLICK_PACKED(x) x __attribute__((packed))
#else
# error "GNU C's __attribute__((packed)) extension required"
#endif
  
  struct wire {
  public:
    u_int8_t dst[6];		// 12
    u_int8_t src[6];

    u_int16_t length;		// 5
    u_int16_t sap;
    u_int8_t ctl;


    CLICK_PACKED(u_int16_t protocol);	// 35
    u_int8_t version;
    u_int8_t type;
    u_int8_t tc:1;
    u_int8_t reserved:6;
    u_int8_t tca:1;
    CLICK_PACKED(u_int64_t root);
    CLICK_PACKED(u_int32_t cost);
    CLICK_PACKED(u_int64_t bridge_id);
    CLICK_PACKED(u_int16_t port_id);
    CLICK_PACKED(u_int16_t message_age);
    CLICK_PACKED(u_int16_t max_age);
    CLICK_PACKED(u_int16_t hello_time);
    CLICK_PACKED(u_int16_t forward_delay);

    u_int8_t padding[8];	// 8

    String s(String tag = "") const;
  };

#undef packed

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

#endif
