#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "bridgemessage.hh"
#include "glue.hh"
#include "confparse.hh"

String
BridgeMessage::s(String tag) const {
  char* buf = new char[256];
  String s;

  sprintf(buf, "%s %016s:%04hx: %2s  %x -> %016s  m/h/d: %hx/%hx/%hx",
	  tag.cc(),
	  cp_unparse_ulonglong(_bridge_id,16,false).cc(), _port_id,
	  _tc ? "TC" : "tc",
	  _cost, cp_unparse_ulonglong(_root,16,false).cc(),
	  _max_age, _hello_time, _forward_delay);
  s = buf;
  delete [] buf;
  return s;
}


#define COMPARE(a,b,c) if ((a) != (b)) return ((a) < (b)) ? (c) : -(c);

int
BridgeMessage::compare(const BridgeMessage* other) const {
  COMPARE(_root, other->_root, 4);
  COMPARE(_cost, other->_cost, 3);
  COMPARE(_bridge_id, other->_bridge_id, 2);
  COMPARE(_port_id, other->_port_id, 1);
  return 0;
}


int
BridgeMessage::compare(BridgeMessage::wire* other) const {
  COMPARE(_root, ntohq(other->root()), 4);
  COMPARE(_cost, ntohl(other->cost()), 3);
  COMPARE(_bridge_id, ntohq(other->bridge_id()), 2);
  COMPARE(_port_id, ntohs(other->port_id()), 1);
  return 0;
}

int
BridgeMessage::compare(const BridgeMessage* other,
		       u_int64_t _bridge_id, u_int16_t _port_id) const {
  COMPARE(_root, other->_root, 4);
  COMPARE(_cost, other->_cost, 3);
  COMPARE(_bridge_id, other->_bridge_id, 2);
  COMPARE(_port_id, other->_port_id, 1);
  return 0;
}

void
BridgeMessage::reset(u_int64_t bridge_id) {
  _root = _bridge_id = bridge_id;
  _cost = 0;
  _timestamp.tv_sec = ~(1 << 31); // Never expire
  _tc = false;
  _max_age = 20;
  _hello_time = 2;
  _forward_delay = 15;
}

/* If message's timestamp is older than cutoff, make the message as
   bad as possible. */
bool BridgeMessage::expire(const timeval* cutoff) {
  if (timercmp(&_timestamp, cutoff, >))
    return false;
  expire();
  return true;
}

/* If t is after the message's timestamp, make the message as bad as
   possible. */
void BridgeMessage::expire() {
  _root = _bridge_id = ~(u_int64_t)0; // Worst possible
  _cost = ~(u_int16_t)0;	// Worst possible
  _timestamp.tv_sec = ~(1 << 31); // Never expire
  _tc = false;
}

void
BridgeMessage::from_wire(BridgeMessage::wire* msg) {
  _root = ntohq(msg->root());
  _cost = ntohl(msg->cost());
  _bridge_id = ntohq(msg->bridge_id());
  _port_id = ntohs(msg->port_id());

  click_gettimeofday(&_timestamp);

  // How stale is this message?
  const int million = 1000000;
  int lateness = (ntohs(msg->message_age()) * million)/256;
  _timestamp.tv_sec -= lateness / million;
  _timestamp.tv_usec -= lateness % million;
  if (_timestamp.tv_usec < 0) {
    _timestamp.tv_sec--;
    _timestamp.tv_usec += million;
  }

  _tc = msg->tc;

  // Propagate Parameters
  _max_age = ntohs(msg->max_age()) / 256;
  _hello_time =  ntohs(msg->hello_time()) / 256;
  _forward_delay =  ntohs(msg->forward_delay()) / 256;
}

void
BridgeMessage::to_wire(BridgeMessage::wire* msg) const {
  prep_msg(msg);
  msg->length = htons(38);	// Data + 3 (for sap and ctl, I guess)
  msg->type = 0;		// CONFIRM
  msg->tca = 0;
  msg->reserved = 0;
  msg->tc = _tc;
  msg->root() = htonq(_root);
  msg->cost() = htonl(_cost);
  // Actually, these two will be overwritten
  msg->bridge_id() = htonq(_bridge_id);
  msg->port_id() = htons(_port_id);
  // How stale is this message?
  const int million = 1000000;
  if (_timestamp.tv_sec == ~(1<<31)) { // Special "do not expire" value
    msg->message_age() = htons(0);
  } else {
    timeval t;
    click_gettimeofday(&t);
    t.tv_sec -= _timestamp.tv_sec;
    t.tv_usec -= _timestamp.tv_usec;
    msg->message_age() = htons((t.tv_usec * 256)/million);
    msg->message_age() += htons(t.tv_sec * 256);
  }

  // Propagate Parameters
  msg->max_age() = htons(256 * _max_age);
  msg->hello_time() =  htons(256 * _hello_time);
  msg->forward_delay() =  htons(256 * _forward_delay);
}


String
BridgeMessage::wire::s(String tag) {
  char* buf = new char[256];
  String s;

  /*
  if (protocol || version)
    click_chatter("PROTOCOL: %hx   VERSION: %hx",
		htons(protocol), htons(version));
  */

  if (type == 128)
    sprintf(buf, "%s TCM", tag.cc());
  else
    sprintf(buf, "%s %3s %016s:%04hx: %3s %2s  %08x -> %016s  "
	    "a/m/h/d: %hx/%hx/%hx/%hx",
	    tag.cc(),
	    type ? "???" : "CFG",
	    cp_unparse_ulonglong(ntohq(bridge_id()),16,false).cc(),
	    ntohs(port_id()),
	    tca ? "TCA":"tca", tc ? "TC" : "tc",
	    ntohl(cost()), cp_unparse_ulonglong(ntohq(root()),16,false).cc(),
	    ntohs(message_age()), ntohs(max_age()),
	    ntohs(hello_time()), ntohs(forward_delay()));
  s = buf;
  delete [] buf;
  return s;
}

void BridgeMessage::prep_msg(BridgeMessage::wire* msg) {
  memset(msg, 0, sizeof(*msg));	// REMOVE, HELPFUL FOR DEBUGGING
  memcpy(msg->dst, _all_bridges, 6);
  msg->sap = 0x4242;		// Bridge Messaging Protocol
  msg->ctl = 3;			// "Unnumbered information"
  msg->protocol() = 0;
  msg->version = 0;
}

void BridgeMessage::fill_tcm(BridgeMessage::wire* msg) {
  prep_msg(msg);
  msg->length = htons(7);
  msg->type = 128;
}

u_int8_t BridgeMessage::_all_bridges[6] = {
  0x01, 0x80, 0xc2, 0x00, 0x00, 0x00
};

ELEMENT_PROVIDES(EtherSwitchBridgeMessage)
