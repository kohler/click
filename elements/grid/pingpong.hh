#ifndef PINGPONGHH
#define PINGPONGHH

/*
 * =c
 * PingPong(LinkStat)
 * =s Grid
 * =d
 *
 * Expects Grid packets as input.  Places ping-pong link stats,
 * acquired from LinkStat, into outgoing unicast packets.  On the
 * other side of the link a LinkTracker element will aggregate these
 * statistics as neccessary.
 *
 * =a
 * AiroInfo, LinkStat, LinkTracker */

#include <click/bighashmap.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/ipaddress.hh>
#include "linkstat.hh"
CLICK_DECLS

class PingPong : public Element {

  LinkStat *_ls;

public:
  
  PingPong();
  ~PingPong();
  
  const char *class_name() const		{ return "PingPong"; }
  const char *processing() const		{ return "a/a"; }
  
  PingPong *clone() const;

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);
};

CLICK_ENDDECLS
#endif
