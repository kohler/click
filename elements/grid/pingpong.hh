#ifndef PINGPONGHH
#define PINGPONGHH

/*
 * =c
 * PingPong(LinkStat)
 * =s Grid
 * =d
 *
 * Expects Grid packets as input.  Places ping-pong link stats,
 * acquired from LinkStat, into outgoing unicast packets.
 *
 * =a
 * AiroInfo, LinkStat, LinkTracker */

#include <click/bighashmap.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/ipaddress.hh>
#include "linkstat.hh"


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

#endif
