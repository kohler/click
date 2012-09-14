#ifndef CLICK_COPYFLOWID_HH
#define CLICK_COPYFLOWID_HH
#include <click/element.hh>
#include <click/ipflowid.hh>
CLICK_DECLS

/*
 * =c
 * CopyFlowID()
 * =s udp
 * sets UDP/TCP flow ID
 * =d
 * remembers flow id from packets between input/output 0, tag the flow id onto
 * packets between input/output 1. only remembers the flow id from the first
 * packet after initialization or reset.
 */

class CopyFlowID : public Element {
private:
  static int reset_write_handler
    (const String &, Element *, void *, ErrorHandler *);

  void monitor(Packet *);
  Packet *set(Packet *);

  bool _start;
  IPFlowID _flow;

public:
  CopyFlowID() CLICK_COLD;
  ~CopyFlowID() CLICK_COLD;

  const char *class_name() const	{ return "CopyFlowID"; }
  const char *port_count() const	{ return "2/2"; }

  int initialize(ErrorHandler *) CLICK_COLD;
  int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;

  void push(int, Packet *);
  Packet *pull(int);
  void add_handlers() CLICK_COLD;

};

CLICK_ENDDECLS
#endif
