/*
 * bridgemessage.{cc,hh} -- parse IEEE 802.1d Ethernet bridge messages
 * John Jannotti
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "bridgemessage.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
CLICK_DECLS

String
BridgeMessage::s(String tag) const {
  // static assertion on message size
  static_assert(sizeof(BridgeMessage::wire) == 60, "BridgeMessages must be 60 bytes on the wire.");

  char* buf = new char[256];
  String s;

  sprintf(buf, "%s %16s:%04x: %2s  %x -> %16s  m/h/d: %x/%x/%x",
	  tag.c_str(),
	  String::make_numeric(static_cast<String::uint_large_t>(_bridge_id), 16, false).c_str(),
	  _port_id,
	  _tc ? "TC" : "tc",
	  _cost, String::make_numeric(static_cast<String::uint_large_t>(_root), 16, false).c_str(),
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
BridgeMessage::compare(const BridgeMessage::wire* other) const {
  COMPARE(_root, ntohq(other->root), 4);
  COMPARE(_cost, ntohl(other->cost), 3);
  COMPARE(_bridge_id, ntohq(other->bridge_id), 2);
  COMPARE(_port_id, ntohs(other->port_id), 1);
  return 0;
}

int
BridgeMessage::compare(const BridgeMessage* other,
		       uint64_t _bridge_id, uint16_t _port_id) const {
  COMPARE(_root, other->_root, 4);
  COMPARE(_cost, other->_cost, 3);
  COMPARE(_bridge_id, other->_bridge_id, 2);
  COMPARE(_port_id, other->_port_id, 1);
  return 0;
}

void
BridgeMessage::reset(uint64_t bridge_id) {
  _root = _bridge_id = bridge_id;
  _cost = 0;
  _timestamp = Timestamp::make_sec(Timestamp::max_seconds); // Never expire
  _tc = false;
  _max_age = 20;
  _hello_time = 2;
  _forward_delay = 15;
}

/* If message's timestamp is older than cutoff, make the message as
   bad as possible. */
bool BridgeMessage::expire(const Timestamp& cutoff) {
  if (_timestamp > cutoff)
    return false;
  expire();
  return true;
}

/* If t is after the message's timestamp, make the message as bad as
   possible. */
void BridgeMessage::expire() {
  _root = _bridge_id = ~(uint64_t)0; // Worst possible
  _cost = ~(uint16_t)0;	// Worst possible
  _timestamp = Timestamp::make_sec(Timestamp::max_seconds); // Never expire
  _tc = false;
}

void
BridgeMessage::from_wire(const BridgeMessage::wire* msg) {
  _root = ntohq(msg->root);
  _cost = ntohl(msg->cost);
  _bridge_id = ntohq(msg->bridge_id);
  _port_id = ntohs(msg->port_id);

  _timestamp = Timestamp::now();

  // How stale is this message?
  int lateness = (ntohs(msg->message_age) * 1000000)/256;
  _timestamp -= Timestamp::make_usec(lateness);
  _tc = msg->tc;

  // Propagate Parameters
  _max_age = ntohs(msg->max_age) / 256;
  _hello_time =  ntohs(msg->hello_time) / 256;
  _forward_delay =  ntohs(msg->forward_delay) / 256;
}

void
BridgeMessage::to_wire(BridgeMessage::wire* msg) const {
  prep_msg(msg);
  msg->length = htons(38);	// Data + 3 (for sap and ctl, I guess)
  msg->type = 0;		// CONFIRM
  msg->tca = 0;
  msg->reserved = 0;
  msg->tc = _tc;
  msg->root = htonq(_root);
  msg->cost = htonl(_cost);
  // Actually, these two will be overwritten
  msg->bridge_id = htonq(_bridge_id);
  msg->port_id = htons(_port_id);
  // How stale is this message?
  if (_timestamp.sec() == Timestamp::max_seconds) { // Special "do not expire" value
    msg->message_age = htons(0);
  } else {
    Timestamp t = Timestamp::now() - _timestamp;
    msg->message_age = htons((t.usec() * 256)/1000000);
    msg->message_age += htons(t.sec() * 256);
  }

  // Propagate Parameters
  msg->max_age = htons(256 * _max_age);
  msg->hello_time =  htons(256 * _hello_time);
  msg->forward_delay =  htons(256 * _forward_delay);
}


String
BridgeMessage::wire::s(String tag) const {
  char* buf = new char[256];
  String s;

  /*
  if (protocol || version)
    click_chatter("PROTOCOL: %hx   VERSION: %hx",
		htons(protocol), htons(version));
  */

  if (type == 128)
    sprintf(buf, "%s TCM", tag.c_str());
  else
    sprintf(buf, "%s %3s %16s:%04hx: %3s %2s  %08x -> %16s  "
	    "a/m/h/d: %hx/%hx/%hx/%hx",
	    tag.c_str(),
	    type ? "???" : "CFG",
	    String::make_numeric(static_cast<String::uint_large_t>(ntohq(bridge_id)), 16, false).c_str(),
	    ntohs(port_id),
	    tca ? "TCA":"tca", tc ? "TC" : "tc",
	    ntohl(cost), String::make_numeric(static_cast<String::uint_large_t>(ntohq(root)), 16, false).c_str(),
	    ntohs(message_age), ntohs(max_age),
	    ntohs(hello_time), ntohs(forward_delay));
  s = buf;
  delete [] buf;
  return s;
}

void BridgeMessage::prep_msg(BridgeMessage::wire* msg) {
  memset(msg, 0, sizeof(*msg));	// REMOVE, HELPFUL FOR DEBUGGING
  memcpy(msg->dst, _all_bridges, 6);
  msg->sap = 0x4242;		// Bridge Messaging Protocol
  msg->ctl = 3;			// "Unnumbered information"
  msg->protocol = 0;
  msg->version = 0;
}

void BridgeMessage::fill_tcm(BridgeMessage::wire* msg) {
  prep_msg(msg);
  msg->length = htons(7);
  msg->type = 128;
}

uint8_t BridgeMessage::_all_bridges[6] = {
  0x01, 0x80, 0xc2, 0x00, 0x00, 0x00
};

CLICK_ENDDECLS
ELEMENT_REQUIRES(int64)
ELEMENT_PROVIDES(EtherSwitchBridgeMessage)
