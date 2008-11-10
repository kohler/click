#ifndef CLICK_BRIDGEMESSAGE_HH
#define CLICK_BRIDGEMESSAGE_HH
#include <click/glue.hh>
#include <click/string.hh>
#include <click/integers.hh>
#include <click/timestamp.hh>
CLICK_DECLS

class BridgeMessage {
public:
  struct wire;

  BridgeMessage()			{ expire (); }
  BridgeMessage(const wire* msg)	{ from_wire(msg); }


  void reset(uint64_t bridge_id);
  void from_wire(const wire* msg);
  void to_wire(wire* msg) const;
  static void fill_tcm(wire* msg);


  bool expire(const Timestamp& cutoff);	// Possibly expire, based on timestamp
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
	      uint64_t _bridge_id, uint16_t _port_id) const;

  String s(String tag = "") const;

  CLICK_PACKED_STRUCTURE(
  struct wire {,
  public:
    uint8_t dst[6];		// 0-5
    uint8_t src[6];		// 6-11

    uint16_t length;		// 12-13
    uint16_t sap;		// 14-15
    uint8_t ctl;		// 16

    uint16_t protocol;		// 17-18
    uint8_t version;		// 19
    uint8_t type;		// 20
    unsigned tc:1;		// 21
    unsigned reserved:6;
    unsigned tca:1;
    uint64_t root;		// 22-29
    uint32_t cost;		// 30-33
    uint64_t bridge_id;		// 34-41
    uint16_t port_id;		// 42-43
    uint16_t message_age;	// 44-45
    uint16_t max_age;		// 46-47
    uint16_t hello_time;	// 48-49
    uint16_t forward_delay;	// 50-51

    uint8_t padding[8];		// 52-59

    String s(String tag = "") const;
  });

  // Parameters that get propagated
  uint32_t _max_age;		// in seconds
  uint32_t _hello_time;	// in seconds
  uint32_t _forward_delay;	// in seconds

private:
  uint64_t _root;
  uint64_t _bridge_id;
public:  uint32_t _cost; private: // Put in an incrementer JJJ
  uint16_t _port_id;

  bool _tc;

  Timestamp _timestamp; // When the message should be considered to have
		      // been created, used for expiration.

  static void prep_msg(wire* msg);
  static uint8_t _all_bridges[6];
};

CLICK_ENDDECLS
#endif
