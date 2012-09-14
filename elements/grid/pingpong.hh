#ifndef PINGPONGHH
#define PINGPONGHH

/*
 * =c
 * PingPong(LINKSTAT)
 * =s Grid
 * =d
 *
 * Expects Grid packets as input.  Places ping-pong link stats,
 * acquired from LINKSTAT, a LinkStat element, into outgoing unicast packets.  On the
 * other side of the link a LinkTracker element will aggregate these
 * statistics as neccessary.
 *
 * =a
 * AiroInfo, LinkStat, LinkTracker */

#include <click/bighashmap.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

class LinkStat;

class PingPong : public Element {

  LinkStat *_ls;

public:

  PingPong() CLICK_COLD;
  ~PingPong() CLICK_COLD;

  const char *class_name() const		{ return "PingPong"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return "a/a"; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);
};

CLICK_ENDDECLS
#endif
